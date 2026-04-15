#include "main.hpp"

#include "caf/behavior.hpp"
#include "caf/config_value.hpp"
#include "caf/message.hpp"
#include "caf/message_builder.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>

using namespace caf;

template <class... Ts>
message make_dynamic_message(Ts&&... xs) {
  message_builder mb;
  return mb.append_all(std::forward<Ts>(xs)...).to_message();
}

template <class... Ts>
void reset(Ts&... xs) {
  ((xs = Ts{}), ...);
}

class pattern_matching : public base_fixture {
public:
  // -- messages that we create with make_message ------------------------------

  message native_two_ints;
  message native_two_doubles;
  message native_two_strings;
  message native_one_foo;
  message native_one_bar;

  // -- messages that we create with a message_builder -------------------------

  message dynamic_two_ints;
  message dynamic_two_doubles;
  message dynamic_two_strings;
  message dynamic_one_foo;
  message dynamic_one_bar;

  // -- our behavior and index for the last invoked handler --------------------

  size_t invoked = 0;

  behavior bhvr;

  // -- setup and teardown -----------------------------------------------------

  void SetUp(const benchmark::State&) override {
    native_two_ints = make_message(1, 2);
    native_two_doubles = make_message(1.0, 2.0);
    native_two_strings = make_message("hi", "there");
    native_one_foo = make_message(foo{1, 2});
    native_one_bar = make_message(bar{foo{1, 2}});
    dynamic_two_ints = make_dynamic_message(1, 2);
    dynamic_two_doubles = make_dynamic_message(1.0, 2.0);
    dynamic_two_strings = make_dynamic_message("hi", "there");
    dynamic_one_foo = make_dynamic_message(foo{1, 2});
    dynamic_one_bar = make_dynamic_message(bar{foo{1, 2}});
    bhvr = behavior{
      [this](int) { invoked = 1; },
      [this](int, int) { invoked = 2; },
      [this](double) { invoked = 3; },
      [this](double, double) { invoked = 4; },
      [this](const std::string&) { invoked = 5; },
      [this](const std::string&, const std::string&) { invoked = 6; },
      [this](const foo&) { invoked = 7; },
      [this](const bar&) { invoked = 8; },
    };
  }

  void TearDown(const ::benchmark::State&) override {
    reset(native_two_ints, native_two_doubles, native_two_strings,
          native_one_foo, native_one_bar, dynamic_two_ints, dynamic_two_doubles,
          dynamic_two_strings, dynamic_one_foo, dynamic_one_bar, bhvr);
  }

  // -- utility for dispatching our messages -----------------------------------

  bool match(benchmark::State& state, message& msg,
             size_t expected_handler_id) {
    invoked = 0;
    bhvr(msg);
    if (invoked != expected_handler_id) {
      state.SkipWithError("Wrong handler called!");
      return false;
    }
    return true;
  }
};

BENCHMARK_F(pattern_matching, make_message)(benchmark::State& state) {
  for (auto _ : state) {
    if (!match(state, native_two_ints, 2)
        || !match(state, native_two_doubles, 4)
        || !match(state, native_two_strings, 6)
        || !match(state, native_one_foo, 7) || !match(state, native_one_bar, 8))
      break;
  }
}

BENCHMARK_F(pattern_matching, message_builder)(benchmark::State& state) {
  for (auto _ : state) {
    if (!match(state, dynamic_two_ints, 2)
        || !match(state, dynamic_two_doubles, 4)
        || !match(state, dynamic_two_strings, 6)
        || !match(state, dynamic_one_foo, 7)
        || !match(state, dynamic_one_bar, 8))
      break;
  }
}
