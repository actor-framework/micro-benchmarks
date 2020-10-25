#include "main.hpp"

int main(int argc, char** argv) {
#if CAF_VERSION >= 1800
  caf::init_global_meta_objects<caf::id_block::microbench>();
  caf::core::init_global_meta_objects();
#endif
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  benchmark::RunSpecifiedBenchmarks();
}
