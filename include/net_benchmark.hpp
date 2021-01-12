#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte_span.hpp"
#include "caf/config.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/tag/stream_oriented.hpp"

#include <benchmark/benchmark.h>

// -- fixture base for initializing global meta objects ------------------------

struct net_context {
  caf::actor_system_config cfg;
  caf::actor_system sys;
  net_context();
};

using net_context_ptr = std::unique_ptr<net_context>;

inline auto make_net_context() {
  return std::make_unique<net_context>();
}

using base_fixture = benchmark::Fixture;

// -- dummy classes ------------------------------------------------------------

class dummy_application_base {
public:
  using input_tag = caf::tag::stream_oriented;

  dummy_application_base(std::shared_ptr<caf::byte_buffer> data)
    : data_{std::move(data)} {
    // nop
  }

  template <class ParentPtr>
  caf::error
  init(caf::net::socket_manager*, ParentPtr parent, const caf::settings&) {
    return caf::none;
  }

  template <class ParentPtr>
  bool done_sending(ParentPtr) {
    return true;
  }

  template <class ParentPtr>
  size_t consume(ParentPtr, caf::byte_span data, caf::byte_span) {
    return data.size();
  }

  template <class ParentPtr>
  void resolve(ParentPtr, caf::string_view, const caf::actor&) {
    // nop
  }

  static void handle_error(caf::sec) {
    // nop
  }

  template <class ParentPtr>
  static void abort(ParentPtr, const caf::error&) {
    // nop
  }

  template <class ParentPtr>
  static void abort_reason(ParentPtr, const caf::error&) {
    // nop
  }

  std::shared_ptr<caf::byte_buffer> data_;
};

class stream_application : public dummy_application_base {
public:
  stream_application(std::shared_ptr<caf::byte_buffer> buf)
    : dummy_application_base{std::move(buf)} {
    // nop
  }

  template <class ParentPtr>
  bool prepare_send(ParentPtr parent) {
    parent->begin_output();
    auto& buf = parent->output_buffer();
    buf.insert(buf.begin(), data_->begin(), data_->end());
    parent->end_output();
    return true;
  }
};

class message_application : public dummy_application_base {
public:
  message_application(std::shared_ptr<caf::byte_buffer> buf)
    : dummy_application_base{std::move(buf)} {
    // nop
  }

  template <class ParentPtr>
  bool prepare_send(ParentPtr parent) {
    parent->begin_message();
    auto& buf = parent->message_buffer();
    buf.insert(buf.begin(), data_->begin(), data_->end());
    parent->end_message();
    return true;
  }
};
