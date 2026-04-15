#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"

#include <benchmark/benchmark.h>

#include <memory>

// -- utility functions --------------------------------------------------------

template <class Container>
ptrdiff_t ssize(const Container& container) {
  return static_cast<ptrdiff_t>(container.size());
}

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
#  if CAF_VERSION < 1900
  CAF_ADD_TYPE_ID(microbench, (caf::stream<int>) );
#  endif
  CAF_ADD_TYPE_ID(microbench, (foo));
  CAF_ADD_TYPE_ID(microbench, (std::vector<int>));

CAF_END_TYPE_ID_BLOCK(microbench)

template <typename Inspector>
bool inspect(Inspector& f, foo& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

template <typename Inspector>
bool inspect(Inspector& f, bar& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

#  define APPLY_OR_DIE(inspector, what)                                        \
    if (!inspector.apply(what))                                                \
      CAF_CRITICAL("failed to apply data to the inspector!");

#else // CAF_VERSION >= 1800

template <typename Inspector>
typename Inspector::result_type inspect(Inspector& f, foo& x) {
  return f(x.a, x.b);
}

template <typename Inspector>
typename Inspector::result_type inspect(Inspector& f, bar& x) {
  return f(x.a, x.b);
}

#  define APPLY_OR_DIE(inspector, what)                                        \
    if (auto err = inspector(what))                                            \
      CAF_CRITICAL("failed to apply data to the inspector!");

#endif // CAF_VERSION >= 1800

// -- fixture base for initializing global meta objects ------------------------

struct caf_context {
  caf::actor_system_config cfg;
  caf::actor_system sys;
  caf_context();
};

using caf_context_ptr = std::unique_ptr<caf_context>;

inline auto make_caf_context() {
  return std::make_unique<caf_context>();
}

using base_fixture = benchmark::Fixture;
