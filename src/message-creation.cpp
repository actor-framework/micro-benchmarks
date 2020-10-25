#include "main.hpp"

#include "caf/message.hpp"
#include "caf/message_builder.hpp"

#include <cstdint>

using namespace caf;

using message_creation = base_fixture;

// -- benchmarking of message creation -----------------------------------------

BENCHMARK_F(message_creation, make_message)(benchmark::State& state) {
  for (auto _ : state) {
    auto msg = make_message(size_t{0});
    benchmark::DoNotOptimize(msg);
  }
}

BENCHMARK_F(message_creation, message_builder)(benchmark::State& state) {
  for (auto _ : state) {
    message_builder mb;
    message msg = mb.append(size_t{0}).move_to_message();
    benchmark::DoNotOptimize(msg);
  }
}
