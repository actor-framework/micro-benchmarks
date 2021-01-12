#include "net_benchmark.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/message.hpp"
#include "caf/net/length_prefix_framing.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"

#include <cstdint>
#include <numeric>
#include <vector>

using namespace caf;

using stream_socket_pair = std::pair<net::stream_socket, net::stream_socket>;

class net_communication : public base_fixture {
public:
  net_communication()
    : send_buf{std::make_shared<byte_buffer>()},
      recv_buf{std::make_shared<byte_buffer>()},
      err{none},
      raw_transport{send_buf},
      lp_transport{send_buf} {
    // nop
  }

  void SetUp(const benchmark::State&) override {
    auto sockets = *caf::net::make_stream_socket_pair();
    send_socket = sockets.first;
    recv_socket = sockets.second;
    msg = make_message("abc", uint32_t{10}, 20.0);
    binary_serializer sink{nullptr, *send_buf};
    if (!sink.apply(msg)) {
      std::cout << "Could not serialize message: "
                << to_string(sink.get_error()) << std::endl;
    }
    recv_buf->resize(send_buf->size());
  }

  void TearDown(const ::benchmark::State&) override {
    close(send_socket);
    close(recv_socket);
  }

  void send(net::stream_socket sock, byte_span data) {
    while (!data.empty()) {
      auto res = net::write(sock, data);
      if (res > 0)
        data = data.subspan(res);
      else
        std::cout << "Something went wrong! "
                  << net::last_socket_error_as_string() << std::endl;
    }
  }

  void recv(net::stream_socket sock, byte_span buf) {
    while (!buf.empty()) {
      auto res = net::read(sock, buf);
      if (res > 0)
        buf = buf.subspan(res);
      else
        std::cout << "Something went wrong! "
                  << net::last_socket_error_as_string() << std::endl;
    }
  }

  net::stream_socket handle() const noexcept {
    return send_socket;
  }

  void abort_reason(error reason) const noexcept {
    // nop
  }

  template <class... Ts>
  const error& abort_reason_or(Ts&&...) {
    return err;
  }

  void register_reading() {
    // nop
  }

  void register_writing() {
    // nop
  }

  message msg;
  net::stream_socket send_socket;
  net::stream_socket recv_socket;
  std::shared_ptr<byte_buffer> send_buf;
  std::shared_ptr<byte_buffer> recv_buf;
  caf::error err;

  net::stream_transport<stream_application> raw_transport;
  net::stream_transport<net::length_prefix_framing<message_application>>
    lp_transport;
};

BENCHMARK_F(net_communication, raw_sockets)(benchmark::State& state) {
  for (auto _ : state) {
    send(send_socket, *send_buf);
    recv(recv_socket, *recv_buf);
  }
}

BENCHMARK_F(net_communication, stream_transport)(benchmark::State& state) {
  for (auto _ : state) {
    raw_transport.handle_write_event(this);
    recv(recv_socket, *recv_buf);
  }
}

BENCHMARK_F(net_communication, length_prefix_framing)
(benchmark::State& state) {
  for (auto _ : state) {
    lp_transport.handle_write_event(this);
    recv(recv_socket, *recv_buf);
  }
}
