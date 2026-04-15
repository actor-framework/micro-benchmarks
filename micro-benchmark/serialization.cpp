#include "main.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/make_message.hpp"
#include "caf/message.hpp"

#include <random>
#include <tuple>

#if CAF_VERSION < 1700
using container_type = std::vector<char>;
#else
using container_type = caf::binary_serializer::container_type;
#endif

#if CAF_VERSION >= 1800
template <class Inspector, class T>
void apply(Inspector& inspector, T& x) {
  std::ignore = inspector.apply(x);
}
#else
template <class Inspector, class T>
void apply(Inspector& inspector, T& x) {
  std::ignore = inspector(x);
}
#endif

using namespace caf;

class serialization : public base_fixture {
public:
  caf_context_ptr context;

  node_id hash_node_id_val;

  container_type hash_node_id_val_binary;

  node_id uri_node_id_val;

  container_type uri_node_id_val_binary;

  foo foo_val = foo{10, 20};

  container_type foo_val_binary;

  bar bar_val = bar{foo{1, 2}, "three"};

  container_type bar_val_binary;

  message msg_2int = make_message(1, 2);

  container_type msg_2int_binary;

  container_type buf;

  serialization() {
    using host_id_type = std::array<uint8_t, 20>;
    std::minstd_rand rng{0xCAF};
    std::uniform_int_distribution<> u8{0, std::numeric_limits<uint8_t>::max()};
    host_id_type id;
    for (auto& val : id)
      val = static_cast<uint8_t>(u8(rng));
    hash_node_id_val = make_node_id(rng(), id);
    auto u = make_uri("caf:40dd4919-6585-40c3-b853-712edce5de34");
    assert(u);
    uri_node_id_val = make_node_id(*u);
  }

  void SetUp(const benchmark::State&) override {
    buf.reserve(1024);
    context = make_caf_context();
    store(hash_node_id_val, hash_node_id_val_binary);
    store(uri_node_id_val, uri_node_id_val_binary);
    store(foo_val, foo_val_binary);
    store(bar_val, bar_val_binary);
    store(msg_2int, msg_2int_binary);
  }

  void TearDown(const ::benchmark::State&) override {
    hash_node_id_val_binary.clear();
    uri_node_id_val_binary.clear();
    foo_val_binary.clear();
    bar_val_binary.clear();
    msg_2int_binary.clear();
    context.reset();
  }

private:
  template <class T>
  void store(T x, container_type& storage) {
    binary_serializer sink{context->sys, storage};
    apply(sink, x);
  }
};

BENCHMARK_F(serialization, save_hash_node_id_binary)(benchmark::State& state) {
  for (auto _ : state) {
    buf.clear();
    binary_serializer sink{context->sys, buf};
    apply(sink, hash_node_id_val);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(serialization, save_uri_node_id_binary)(benchmark::State& state) {
  for (auto _ : state) {
    buf.clear();
    binary_serializer sink{context->sys, buf};
    apply(sink, uri_node_id_val);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(serialization, save_foo_binary)(benchmark::State& state) {
  for (auto _ : state) {
    buf.clear();
    binary_serializer sink{context->sys, buf};
    apply(sink, foo_val);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(serialization, save_bar_binary)(benchmark::State& state) {
  for (auto _ : state) {
    buf.clear();
    binary_serializer sink{context->sys, buf};
    apply(sink, bar_val);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(serialization, save_msg_2int_binary)(benchmark::State& state) {
  for (auto _ : state) {
    buf.clear();
    binary_serializer sink{context->sys, buf};
    apply(sink, msg_2int);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(serialization, load_hash_node_id_binary)(benchmark::State& state) {
  for (auto _ : state) {
    node_id result;
    binary_deserializer source{context->sys, hash_node_id_val_binary};
    apply(source, result);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_F(serialization, load_uri_node_id_binary)(benchmark::State& state) {
  for (auto _ : state) {
    node_id result;
    binary_deserializer source{context->sys, uri_node_id_val_binary};
    apply(source, result);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_F(serialization, load_foo_binary)(benchmark::State& state) {
  for (auto _ : state) {
    foo result;
    binary_deserializer source{context->sys, foo_val_binary};
    apply(source, result);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_F(serialization, load_bar_binary)(benchmark::State& state) {
  for (auto _ : state) {
    bar result;
    binary_deserializer source{context->sys, bar_val_binary};
    apply(source, result);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_F(serialization, load_msg_2int_binary)(benchmark::State& state) {
  for (auto _ : state) {
    message result;
    binary_deserializer source{context->sys, msg_2int_binary};
    apply(source, result);
    benchmark::DoNotOptimize(result);
  }
}
