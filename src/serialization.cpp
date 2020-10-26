#include "main.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/make_message.hpp"
#include "caf/message.hpp"

#include <tuple>

#if CAF_VERSION < 1700
using container_type = std::vector<char>;
#else
using container_type = caf::binary_serializer::container_type;
#endif

#if CAF_VERSION >= 1800
template <class Inspector, class T>
void apply(Inspector& inspector, T& x) {
  std::ignore = inspector.apply_object(x);
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

  foo foo_val = foo{10, 20};

  container_type foo_val_binary;

  bar bar_val = bar{foo{1, 2}, "three"};

  container_type bar_val_binary;

  message msg_2int = make_message(1, 2);

  container_type msg_2int_binary;

  void SetUp(const benchmark::State&) override {
    context = make_caf_context();
    store(foo_val, foo_val_binary);
    store(bar_val, bar_val_binary);
    store(msg_2int, msg_2int_binary);
  }

  void TearDown(const ::benchmark::State&) override {
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

BENCHMARK_F(serialization, save_foo_binary)(benchmark::State& state) {
  for (auto _ : state) {
    container_type buf;
    buf.reserve(512);
    binary_serializer sink{context->sys, buf};
    apply(sink, foo_val);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(serialization, save_bar_binary)(benchmark::State& state) {
  for (auto _ : state) {
    container_type buf;
    buf.reserve(512);
    binary_serializer sink{context->sys, buf};
    apply(sink, bar_val);
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(serialization, save_msg_2int_binary)(benchmark::State& state) {
  for (auto _ : state) {
    container_type buf;
    buf.reserve(512);
    binary_serializer sink{context->sys, buf};
    apply(sink, msg_2int);
    benchmark::DoNotOptimize(buf);
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
