#include "main.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/message.hpp"

#include <cstdint>

using namespace caf;

struct context {
  actor_system_config cfg;
  actor_system sys;
  context() : sys(cfg) {
    // nop
  }
};

class actors : public base_fixture {
public:
  std::unique_ptr<context> ctx;

  void SetUp(const benchmark::State&) override {
    ctx.reset(new context);
  }

  void TearDown(const ::benchmark::State&) override {
    ctx.reset();
  }
};

void dummy() {
  // nop
}

BENCHMARK_F(actors, spawn_and_await)(benchmark::State& state) {
  for (auto _ : state) {
    ctx->sys.spawn(dummy);
    ctx->sys.await_all_actors_done();
  }
}
