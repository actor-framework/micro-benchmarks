#include "main.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/message.hpp"

#include <cstdint>

using namespace caf;

class actors : public base_fixture {
public:
  caf_context_ptr context;

  void SetUp(const benchmark::State&) override {
    context = make_caf_context();
  }

  void TearDown(const ::benchmark::State&) override {
    context.reset();
  }
};

void dummy() {
  // nop
}

BENCHMARK_F(actors, spawn_and_await)(benchmark::State& state) {
  for (auto _ : state) {
    context->sys.spawn(dummy);
    context->sys.await_all_actors_done();
  }
}
