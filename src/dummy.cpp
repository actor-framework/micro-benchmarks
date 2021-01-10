#include "main.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/message.hpp"

#include "caf/net/endpoint_manager.hpp"

#include <cstdint>


BENCHMARK_F(base_fixture, spawn_and_await)(benchmark::State& state) {
  for (auto _ : state) {
    // nop
  }
}
