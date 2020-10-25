#include "caf/config.hpp"

#include <benchmark/benchmark.h>

// -- custom message types -----------------------------------------------------

struct foo {
  int32_t a;
  int32_t b;
};

inline bool operator==(const foo& lhs, const foo& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

struct bar {
  foo a;
  std::string b;
};

inline bool operator==(const bar& lhs, const bar& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

#if CAF_VERSION >= 1800

#include "caf/init_global_meta_objects.hpp"
#include "caf/type_id.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(microbench, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(microbench, (bar));
  CAF_ADD_TYPE_ID(microbench, (foo));

CAF_END_TYPE_ID_BLOCK(microbench)

template <typename Inspector>
bool inspect(Inspector& f, foo& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

template <typename Inspector>
bool inspect(Inspector& f, bar& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

#else // CAF_VERSION >= 1800

template <typename Inspector>
typename Inspector::result_type inspect(Inspector& f, foo& x) {
  return f(x.a, x.b);
}

template <typename Inspector>
typename Inspector::result_type inspect(Inspector& f, bar& x) {
  return f(x.a, x.b);
}

#endif // CAF_VERSION >= 1800

// -- fixture base for initializing global meta objects -

using base_fixture = benchmark::Fixture;
