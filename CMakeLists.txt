cmake_minimum_required(VERSION 3.13.5 FATAL_ERROR)
project(CAF_MICROBENCH CXX)

set(CMAKE_CXX_STANDARD 17)

# -- includes ------------------------------------------------------------------

include(FetchContent)              # For bundling CAF with the incubator

# -- override CMake defaults for internal cache entries ------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON
    CACHE INTERNAL "Write JSON compile commands database")

# -- general options -----------------------------------------------------------

option(BUILD_SHARED_LIBS "Build shared library targets" ON)

# -- options with non-boolean values -------------------------------------------

set(SANITIZERS "" CACHE STRING
    "Comma separated sanitizers, e.g., 'address,undefined'")

set(CAF_TAG "master" CACHE STRING "CAF tag or branch name")

# -- macOS-specific options ----------------------------------------------------

if(APPLE)
  set(CMAKE_MACOSX_RPATH ON CACHE INTERNAL "Use rpaths on macOS and iOS")
endif()

# -- sanity checks -------------------------------------------------------------

if(MSVC AND SANITIZERS)
  message(FATAL_ERROR "Sanitizer builds are currently not supported on MSVC")
endif()

# -- adjust defaults: build release if not being told otherwise ----------------

if(NOT GENERATOR_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
  message(STATUS "Set build type to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

# -- get dependencies ----------------------------------------------------------

find_package(benchmark REQUIRED)

if (CAF_ROOT)
  find_package(CAF COMPONENTS core io REQUIRED)
else()
  message(STATUS "Fetch CAF ${CAF_TAG}")
  FetchContent_Declare(
    actor_framework
    GIT_REPOSITORY https://github.com/actor-framework/actor-framework.git
    GIT_TAG        ${CAF_TAG}
  )
  FetchContent_Populate(actor_framework)
  string(REGEX MATCH "^[0-9]+\.[0-9]+\.[0-9]+$" TAG_IS_VERSION "${CAF_TAG}")
  if(NOT TAG_IS_VERSION OR NOT CAF_TAG VERSION_LESS 0.18.0)
    # CAF >= 0.18 setup
    foreach(varname CAF_ENABLE_EXAMPLES CAF_ENABLE_IO_MODULE CAF_ENABLE_TESTING
                    CAF_ENABLE_TOOLS CAF_ENABLE_OPENSSL_MODULE)
      set(${varname} OFF CACHE INTERNAL "")
    endforeach()
    set(CAF_SANITIZERS "${SANITIZERS}" CACHE INTERNAL "")
    add_subdirectory(${actor_framework_SOURCE_DIR} ${actor_framework_BINARY_DIR})
  else()
    # CAF < 0.18 setup
    foreach(varname CAF_NO_COMPILER_CHECK CAF_NO_AUTO_LIBCPP CAF_NO_EXAMPLES
                    CAF_NO_UNIT_TESTS CAF_NO_OPENSSL CAF_NO_OPENCL CAF_NO_TOOLS
                    CAF_NO_PYTHON CAF_NO_SUMMARY)
      set(${varname} ON CACHE INTERNAL "")
    endforeach()
    if(NOT BUILD_SHARED_LIBS)
      set(CAF_BUILD_STATIC_ONLY ON CACHE INTERNAL "")
    endif()
    add_subdirectory(${actor_framework_SOURCE_DIR} ${actor_framework_BINARY_DIR})
    if(BUILD_SHARED_LIBS)
      add_library(CAF::core ALIAS libcaf_core_shared)
      add_library(CAF::io ALIAS libcaf_io_shared)
      target_include_directories(libcaf_core_shared INTERFACE
                                 "${actor_framework_SOURCE_DIR}/libcaf_core")
    else()
      add_library(CAF::core ALIAS libcaf_core_static)
      add_library(CAF::io ALIAS libcaf_io_static)
    endif()
  endif()
endif()

# -- add executables -----------------------------------------------------------

add_executable(micro-benchmark
  "src/actors.cpp"
  "src/main.cpp"
  "src/message-creation.cpp"
  "src/or_else.cpp"
  "src/pattern-matching.cpp"
  "src/serialization.cpp"
)

target_include_directories(micro-benchmark PRIVATE "include")

target_link_libraries(micro-benchmark PRIVATE benchmark::benchmark_main CAF::core)
