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
void reset(Ts&... xs) {
  ((xs = Ts{}), ...);
}

class or_else : public base_fixture {
public:
  // -- messages that we create with make_message ------------------------------

  message native_two_ints;
  message native_two_doubles;
  message native_two_strings;
  message native_one_foo;
  message native_one_bar;

  // -- our behavior and index for the last invoked handler --------------------

  size_t invoked = 0;

  behavior bhvr;

  // -- setup and teardown -----------------------------------------------------

  void SetUp(const benchmark::State&) override {
    using std::string;
    native_two_ints = make_message(1, 2);
    native_two_doubles = make_message(1.0, 2.0);
    native_two_strings = make_message("hi", "there");
    native_one_foo = make_message(foo{1, 2});
    native_one_bar = make_message(bar{foo{1, 2}});
    bhvr = message_handler{[this](int) { invoked = 1; }}
             .or_else([this](int, int) { invoked = 2; })
             .or_else([this](double) { invoked = 3; })
             .or_else([this](double, double) { invoked = 4; })
             .or_else([this](const string&) { invoked = 5; })
             .or_else([this](const string&, const string&) { invoked = 6; })
             .or_else([this](const foo&) { invoked = 7; })
             .or_else([this](const bar&) { invoked = 8; });
  }

  void TearDown(const ::benchmark::State&) override {
    reset(native_two_ints, native_two_doubles, native_two_strings,
          native_one_foo, native_one_bar, bhvr);
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

BENCHMARK_F(or_else, make_message)(benchmark::State& state) {
  for (auto _ : state) {
    if (!match(state, native_two_ints, 2)
        || !match(state, native_two_doubles, 4)
        || !match(state, native_two_strings, 6)
        || !match(state, native_one_foo, 7) || !match(state, native_one_bar, 8))
      break;
  }
}
