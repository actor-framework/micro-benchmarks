#include "main.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/message.hpp"

#if CAF_VERSION >= 1900
#  include "caf/async/spsc_buffer.hpp"
#  include "caf/scheduled_actor/flow.hpp"
#  include "caf/stream.hpp"
#else
#  include "caf/attach_stream_sink.hpp"
#  include "caf/attach_stream_source.hpp"
#endif

#include <caf/detail/double_ended_queue.hpp>

#include <cstdint>

using namespace caf;
using namespace std::literals;

class actors : public base_fixture {
public:
  caf_context_ptr context;

  int some_int = 42;

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

#if CAF_VERSION >= 1900

using namespace caf::async;

BENCHMARK_DEFINE_F(actors, int_queue)(benchmark::State& state) {
  auto num_items = state.range(0);
  using actor_t = event_based_actor;
  for (auto _ : state) {
    auto& sys = context->sys;
    auto queue = std::make_shared<detail::double_ended_queue<int>>();
    sys.spawn([this, queue, num_items](actor_t* src) {
      for (int i = 0; i < num_items; ++i)
        queue->append(&some_int);
    });
    sys.spawn<detached>([queue, num_items](actor_t* snk) {
      for (int i = 0; i < num_items; ++i)
        auto ptr = queue->take_head();
    });
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(actors, int_queue)->Arg(1'000)->Arg(10'000)->Arg(100'000);

BENCHMARK_DEFINE_F(actors, int_flow)(benchmark::State& state) {
  auto num_items = static_cast<size_t>(state.range(0));
  using actor_t = event_based_actor;
  for (auto _ : state) {
    auto& sys = context->sys;
    auto [rd, wr] = async::make_spsc_buffer_resource<int>();
    sys.spawn([wr{wr}, num_items](actor_t* src) {
      src->make_observable().repeat(42).take(num_items).subscribe(wr);
    });
    sys.spawn([rd{rd}](actor_t* snk) {
      snk->make_observable().from_resource(rd).for_each([](int) {});
    });
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(actors, int_flow)->Arg(1'000)->Arg(10'000)->Arg(100'000);

void source(event_based_actor* self, size_t num_items, actor snk) {
  auto stream_hdl = self
                      ->make_observable() //
                      .iota(0)
                      .take(num_items)
                      .compose(self->to_stream("benchmark", 5ms, 50));
  self->send(snk, stream_hdl);
}

behavior sink(event_based_actor* self) {
  return {
    [=](stream input) {
      self->observe_as<int>(input, 100, 50).for_each([](int) {});
    },
  };
}
BENCHMARK_DEFINE_F(actors, int_stream)(benchmark::State& state) {
  auto num_items = static_cast<size_t>(state.range(0));
  using actor_t = event_based_actor;
  for (auto _ : state) {
    auto& sys = context->sys;
    sys.spawn(source, num_items, sys.spawn(sink));
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(actors, int_stream)->Arg(1'000)->Arg(10'000)->Arg(100'000);

#else

void source(event_based_actor* self, size_t num_items, actor snk) {
  attach_stream_source(
    self, snk, //
    [](size_t& n) { n = 0; },
    [num_items](size_t& n, downstream<int>& out, size_t num) {
      if (n + num > num_items)
        num = num_items - n;
      n += num;
      for (size_t i = 0; i < num; ++i)
        out.push(42);
    },
    [num_items](const size_t& n) { return n == num_items; });
}

behavior sink(event_based_actor* self) {
  return {
    [=](stream<int> in) {
      self->unbecome();
      return attach_stream_sink(
        self, in, [](unit_t&) {}, [=](unit_t&, int) {}, [](unit_t&) {});
    },
  };
}

BENCHMARK_DEFINE_F(actors, int_stream)(benchmark::State& state) {
  auto num_items = static_cast<size_t>(state.range(0));
  using actor_t = event_based_actor;
  for (auto _ : state) {
    auto& sys = context->sys;
    sys.spawn(source, num_items, sys.spawn(sink));
    sys.await_all_actors_done();
  }
}

BENCHMARK_REGISTER_F(actors, int_stream)->Arg(1'000)->Arg(10'000)->Arg(100'000);

#endif
