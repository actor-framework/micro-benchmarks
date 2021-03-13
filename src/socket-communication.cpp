#include "barrier.hpp"
#include "main.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/message.hpp"
#include "caf/net/length_prefix_framing.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"

#include <cstdint>
#include <numeric>
#include <vector>

#ifdef CAF_POSIX
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

#define APPLY_OR_DIE(what)                                                     \
  if (!sink.apply(what))                                                       \
    CAF_CRITICAL("failed to apply data to the sink!");

using namespace caf;
using namespace std::literals;

template <class Container>
ptrdiff_t ssize(const Container& container) {
  return static_cast<ptrdiff_t>(container.size());
}

constexpr string_view ping_text
  = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

constexpr string_view pong_text
  = "In sapien diam, porttitor sed pretium quis, varius at orci.";

class socket_fixture : public base_fixture {
public:
  socket_fixture() : start(3), stop(3) {
    // nop
  }

  void SetUp(const benchmark::State&) override {
    fin = false;
    std::tie(ping_sock, pong_sock) = *net::make_stream_socket_pair();
    // Note: for the length-prefix framing, we need a 32-bit size header.
    {
      binary_serializer sink{nullptr, ping_out};
      sink.skip(sizeof(uint32_t));
      APPLY_OR_DIE(ping_text);
      sink.seek(0);
      APPLY_OR_DIE(static_cast<uint32_t>(ping_out.size() - sizeof(uint32_t)));
      pong_in.resize(ping_out.size());
    }
    {
      binary_serializer sink{nullptr, pong_out};
      sink.skip(sizeof(uint32_t));
      APPLY_OR_DIE(pong_text);
      sink.seek(0);
      APPLY_OR_DIE(static_cast<uint32_t>(pong_out.size() - sizeof(uint32_t)));
      ping_in.resize(pong_out.size());
    }
  }

  void TearDown(const benchmark::State&) override {
    fin = true;
    start.arrive_and_wait();
    sender.join();
    receiver.join();
    close(ping_sock);
    close(pong_sock);
  }

  net::stream_socket handle() const noexcept {
    return ping_sock;
  }

  template <class F>
  auto loop(F fun) {
    return [this, f{std::move(fun)}] {
      for (;;) {
        start.arrive_and_wait();
        if (fin.load())
          return;
        f();
        stop.arrive_and_wait();
      }
    };
  }

  net::stream_socket ping_sock;
  net::stream_socket pong_sock;
  byte_buffer ping_out;
  byte_buffer ping_in;
  byte_buffer pong_out;
  byte_buffer pong_in;

  std::atomic<bool> fin;
  barrier start;
  barrier stop;
  std::thread sender;
  std::thread receiver;
};

#ifdef CAF_POSIX
auto posix_write(net::stream_socket fd, const byte_buffer& buf) {
  return ::send(fd.id, buf.data(), buf.size(), 0);
}

auto posix_read(net::stream_socket fd, byte_buffer& buf) {
  return ::recv(fd.id, buf.data(), buf.size(), 0);
}

class posix_communication : public socket_fixture {
public:
  void SetUp(const benchmark::State& state) override {
    socket_fixture::SetUp(state);
    sender = std::thread{loop([this] {
      if (posix_write(ping_sock, ping_out) != ssize(ping_out))
        CAF_CRITICAL("failed to write buffer");
      if (posix_read(ping_sock, ping_in) != ssize(ping_in))
        CAF_CRITICAL("failed to read buffer");
    })};
    receiver = std::thread{loop([this] {
      if (posix_read(pong_sock, pong_in) != ssize(pong_in))
        CAF_CRITICAL("failed to read buffer");
      if (posix_write(pong_sock, pong_out) != ssize(pong_out))
        CAF_CRITICAL("failed to write buffer");
    })};
  }
};

BENCHMARK_F(posix_communication, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}
#endif

class stream_socket_communication : public socket_fixture {
public:
  void SetUp(const benchmark::State& state) override {
    socket_fixture::SetUp(state);
    sender = std::thread{loop([this] {
      if (net::write(ping_sock, ping_out) != ssize(ping_out))
        CAF_CRITICAL("failed to write buffer");
      if (net::read(ping_sock, ping_in) != ssize(ping_in))
        CAF_CRITICAL("failed to read buffer");
    })};
    receiver = std::thread{loop([this] {
      if (net::read(pong_sock, pong_in) != ssize(pong_in))
        CAF_CRITICAL("failed to read buffer");
      if (net::write(pong_sock, pong_out) != ssize(pong_out))
        CAF_CRITICAL("failed to write buffer");
    })};
  }
};

BENCHMARK_F(stream_socket_communication, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}

enum class app_state {
  reading,
  writing,
  done,
};

class pong_stream_application {
public:
  using input_tag = tag::stream_oriented;

  pong_stream_application(byte_buffer* in, byte_buffer* out)
    : state(app_state::done), in(in), out(out) {
    // nop
  }

  template <class ParentPtr>
  error init(net::socket_manager*, ParentPtr parent, const settings&) {
    parent->configure_read(net::receive_policy::exactly(in->size()));
    return none;
  }

  template <class ParentPtr>
  bool prepare_send(ParentPtr) {
    return true;
  }

  template <class ParentPtr>
  bool done_sending(ParentPtr) {
    if (state != app_state::writing)
      CAF_CRITICAL("done_sending called but app is not writing!");
    state = app_state::done;
    return true;
  }

  template <class ParentPtr>
  ptrdiff_t consume(ParentPtr parent, byte_span data, byte_span) {
    if (data.size() != in->size())
      CAF_CRITICAL("unexpected data");
    if (state != app_state::reading)
      CAF_CRITICAL("consume called but app is not reading!");
    parent->begin_output();
    auto& buf = parent->output_buffer();
    buf.insert(buf.end(), out->begin(), out->end());
    parent->end_output();
    state = app_state::writing;
    return static_cast<ptrdiff_t>(data.size());
  }

  template <class ParentPtr>
  void abort(ParentPtr, const error&) {
    CAF_CRITICAL("abort called");
  }

  app_state state;
  byte_buffer* in;
  byte_buffer* out;
};

class stream_transport_communication : public socket_fixture {
public:
  void SetUp(const benchmark::State& state) override {
    socket_fixture::SetUp(state);
    if (auto err = net::nonblocking(pong_sock, true))
      CAF_CRITICAL("nonblocking(pong_sock) failed");
    sender = std::thread{loop([this] {
      if (net::write(ping_sock, ping_out) != ssize(ping_out))
        CAF_CRITICAL("failed to write buffer");
      if (net::read(ping_sock, ping_in) != ssize(ping_in))
        CAF_CRITICAL("failed to read buffer");
    })};
    receiver = std::thread{[this] {
      using app = pong_stream_application;
      auto mpx = std::make_unique<net::multiplexer>(nullptr);
      if (auto err = mpx->init())
        CAF_CRITICAL("mpx->init failed");
      mpx->set_thread_id();
      auto mgr = net::make_socket_manager<app, net::stream_transport>(
        pong_sock, mpx.get(), &pong_in, &pong_out);
      settings cfg;
      if (auto err = mgr->init(cfg)) {
        auto what = "mgr->init failed: "s;
        what += to_string(err);
        CAF_CRITICAL(what.c_str());
      }
      auto f = loop([this, &mgr, &mpx] {
        auto& state = mgr->protocol().upper_layer().state;
        state = app_state::reading;
        while (state != app_state::done) {
          mpx->poll_once(true);
        }
      });
      f();
    }};
  }
};

BENCHMARK_F(stream_transport_communication, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}

class pong_msg_application {
public:
  using input_tag = tag::message_oriented;

  pong_msg_application(byte_buffer* in, byte_buffer* out)
    : state(app_state::done), in(in), out(out) {
    // nop
  }

  template <class ParentPtr>
  error init(net::socket_manager*, ParentPtr, const settings&) {
    return none;
  }

  template <class ParentPtr>
  bool prepare_send(ParentPtr) {
    return true;
  }

  template <class ParentPtr>
  bool done_sending(ParentPtr) {
    if (state != app_state::writing)
      CAF_CRITICAL("done_sending called but app is not writing!");
    state = app_state::done;
    return true;
  }

  template <class ParentPtr>
  ptrdiff_t consume(ParentPtr parent, byte_span msg) {
    if (msg.size() != in->size() - sizeof(uint32_t)) {
      std::string msg = "unexpected data: expected ";
      msg += std::to_string(in->size() - sizeof(uint32_t));
      msg += " bytes, got ";
      msg += std::to_string(msg.size());
      CAF_CRITICAL(msg.c_str());
    }
    if (state != app_state::reading)
      CAF_CRITICAL("consume called but app is not reading!");
    parent->begin_message();
    auto& buf = parent->message_buffer();
    buf.insert(buf.end(), out->begin() + sizeof(uint32_t), out->end());
    if (!parent->end_message())
      CAF_CRITICAL("end_message failed");
    state = app_state::writing;
    return static_cast<ptrdiff_t>(msg.size());
  }

  template <class ParentPtr>
  void abort(ParentPtr, const error&) {
    CAF_CRITICAL("abort called");
  }

  app_state state;
  byte_buffer* in;
  byte_buffer* out;
};

class msg_transport_communication : public socket_fixture {
public:
  void SetUp(const benchmark::State& state) override {
    socket_fixture::SetUp(state);
    if (auto err = net::nonblocking(pong_sock, true))
      CAF_CRITICAL("nonblocking(pong_sock) failed");
    sender = std::thread{loop([this] {
      if (net::write(ping_sock, ping_out) != ssize(ping_out))
        CAF_CRITICAL("failed to write buffer");
      if (net::read(ping_sock, ping_in) != ssize(ping_in))
        CAF_CRITICAL("failed to read buffer");
    })};
    receiver = std::thread{[this] {
      using app = pong_msg_application;
      auto mpx = std::make_unique<net::multiplexer>(nullptr);
      if (auto err = mpx->init())
        CAF_CRITICAL("mpx->init failed");
      mpx->set_thread_id();
      auto mgr = net::make_socket_manager<app, net::length_prefix_framing,
                                          net::stream_transport>(
        pong_sock, mpx.get(), &pong_in, &pong_out);
      settings cfg;
      if (auto err = mgr->init(cfg)) {
        auto what = "mgr->init failed: "s;
        what += to_string(err);
        CAF_CRITICAL(what.c_str());
      }
      auto f = loop([this, &mgr, &mpx] {
        auto& state = mgr->protocol().upper_layer().upper_layer().state;
        state = app_state::reading;
        while (state != app_state::done) {
          mpx->poll_once(true);
        }
      });
      f();
    }};
  }
};

BENCHMARK_F(msg_transport_communication, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}

