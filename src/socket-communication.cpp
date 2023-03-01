#include "barrier.hpp"
#include "main.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/message.hpp"
#include "caf/net/binary/lower_layer.hpp"
#include "caf/net/binary/upper_layer.hpp"
#include "caf/net/length_prefix_framing.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_oriented.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"

#include <cstdint>
#include <numeric>
#include <vector>

#ifdef CAF_POSIX
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

#if CAF_VERSION < 1900
using sv_t = caf::string_view;
#else
using sv_t = std::string_view;
#endif

using namespace caf;
using namespace std::literals;

namespace {

constexpr sv_t ping_text
  = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

constexpr sv_t pong_text
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
      APPLY_OR_DIE(sink, ping_text);
      sink.seek(0);
      APPLY_OR_DIE(sink,
                   static_cast<uint32_t>(ping_out.size() - sizeof(uint32_t)));
      pong_in.resize(ping_out.size());
    }
    {
      binary_serializer sink{nullptr, pong_out};
      sink.skip(sizeof(uint32_t));
      APPLY_OR_DIE(sink, pong_text);
      sink.seek(0);
      APPLY_OR_DIE(sink,
                   static_cast<uint32_t>(pong_out.size() - sizeof(uint32_t)));
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

} // namespace

// -- baseline: direct access to the POSIX socket API --------------------------

#ifdef CAF_POSIX

namespace {

auto posix_write(net::stream_socket fd, const byte_buffer& buf) {
  return ::send(fd.id, buf.data(), buf.size(), 0);
}

auto posix_read(net::stream_socket fd, byte_buffer& buf) {
  return ::recv(fd.id, buf.data(), buf.size(), 0);
}

class socket_communication_posix : public socket_fixture {
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

} // namespace

BENCHMARK_F(socket_communication_posix, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}

#endif // CAF_POSIX

// -- raw read and write on sockets using the CAF API --------------------------

namespace {

class socket_communication_raw : public socket_fixture {
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

} // namespace

BENCHMARK_F(socket_communication_raw, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}

// -- reading via stream_transport ---------------------------------------------

namespace {

enum class app_state {
  reading,
  writing,
  done,
};

class pong_stream_application : public net::stream_oriented::upper_layer {
public:
  pong_stream_application(byte_buffer* in, byte_buffer* out)
    : state_(app_state::done), in_(in), out_(out) {
    // nop
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    if (state_ != app_state::writing)
      CAF_CRITICAL("done_sending called but app is not writing!");
    state_ = app_state::done;
    return true;
  }

  void abort(const error&) override {
    CAF_CRITICAL("abort called");
  }

  error start(net::stream_oriented::lower_layer* down,
              const settings&) override {
    down->configure_read(net::receive_policy::exactly(in_->size()));
    down_ = down;
    return none;
  }

  ptrdiff_t consume(byte_span data, byte_span) override {
    if (data.size() != in_->size())
      CAF_CRITICAL("unexpected data");
    if (state_ != app_state::reading)
      CAF_CRITICAL("consume called but app is not reading!");
    down_->begin_output();
    auto& buf = down_->output_buffer();
    buf.insert(buf.end(), out_->begin(), out_->end());
    down_->end_output();
    state_ = app_state::writing;
    return static_cast<ptrdiff_t>(data.size());
  }

  void state(app_state value) {
    state_ = value;
  }

  app_state state() {
    return state_;
  }

private:
  net::stream_oriented::lower_layer* down_ = nullptr;
  app_state state_;
  byte_buffer* in_;
  byte_buffer* out_;
};

class socket_communication_stream_transport : public socket_fixture {
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
      using app_t = pong_stream_application;
      auto mpx = net::multiplexer::make(nullptr);
      mpx->set_thread_id();
      mpx->apply_updates();
      if (auto err = mpx->init())
        CAF_CRITICAL("mpx->init failed");
      auto app = std::make_unique<app_t>(&pong_in, &pong_out);
      auto app_ptr = app.get();
      auto transport = net::stream_transport::make(pong_sock, std::move(app));
      auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
      settings cfg;
      if (auto err = mgr->start(cfg)) {
        auto what = "mpx->init failed: " + to_string(err);
        CAF_CRITICAL(what.c_str());
      }
      mpx->apply_updates();
      auto f = loop([this, app_ptr, &mpx] {
        app_ptr->state(app_state::reading);
        while (app_ptr->state() != app_state::done) {
          mpx->poll_once(true);
        }
      });
      f();
    }};
  }
};

} // namespace

BENCHMARK_F(socket_communication_stream_transport, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}

// -- reading via length_prefix_framing (lpf) ----------------------------------

namespace {

class pong_msg_application : public net::binary::upper_layer {
public:
  pong_msg_application(byte_buffer* in, byte_buffer* out)
    : state_(app_state::done), in_(in), out_(out) {
    // nop
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    if (state_ != app_state::writing)
      CAF_CRITICAL("done_sending called but app is not writing!");
    state_ = app_state::done;
    return true;
  }

  error start(net::binary::lower_layer* down, const settings&) override {
    down->request_messages();
    down_ = down;
    return none;
  }

  ptrdiff_t consume(byte_span msg) override {
    if (msg.size() != in_->size() - sizeof(uint32_t)) {
      std::string msg = "unexpected data: expected ";
      msg += std::to_string(in_->size() - sizeof(uint32_t));
      msg += " bytes, got ";
      msg += std::to_string(msg.size());
      CAF_CRITICAL(msg.c_str());
    }
    if (state_ != app_state::reading)
      CAF_CRITICAL("consume called but app is not reading!");
    down_->begin_message();
    auto& buf = down_->message_buffer();
    buf.insert(buf.end(), out_->begin() + sizeof(uint32_t), out_->end());
    if (!down_->end_message())
      CAF_CRITICAL("end_message failed");
    state_ = app_state::writing;
    return static_cast<ptrdiff_t>(msg.size());
  }

  void abort(const error&) override {
    CAF_CRITICAL("abort called");
  }

  void state(app_state value) {
    state_ = value;
  }

  app_state state() {
    return state_;
  }

private:
  net::binary::lower_layer* down_ = nullptr;
  app_state state_;
  byte_buffer* in_;
  byte_buffer* out_;
};

class socket_communication_lpf : public socket_fixture {
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
      using app_t = pong_msg_application;
      auto mpx = net::multiplexer::make(nullptr);
      mpx->set_thread_id();
      mpx->apply_updates();
      if (auto err = mpx->init())
        CAF_CRITICAL("mpx->init failed");
      auto app = std::make_unique<app_t>(&pong_in, &pong_out);
      auto app_ptr = app.get();
      auto framing = net::length_prefix_framing::make(std::move(app));
      auto transport = net::stream_transport::make(pong_sock,
                                                   std::move(framing));
      auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
      settings cfg;
      if (auto err = mgr->start(cfg)) {
        auto what = "mgr->init failed: "s;
        what += to_string(err);
        CAF_CRITICAL(what.c_str());
      }
      mpx->apply_updates();
      auto f = loop([this, app_ptr, &mpx] {
        app_ptr->state(app_state::reading);
        while (app_ptr->state() != app_state::done) {
          mpx->poll_once(true);
        }
      });
      f();
    }};
  }
};

} // namespace

BENCHMARK_F(socket_communication_lpf, ping_pong)(benchmark::State& state) {
  for (auto _ : state) {
    start.arrive_and_wait();
    stop.arrive_and_wait();
  }
}
