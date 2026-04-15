#include "data.hpp"
#include "main.hpp"

#include <caf/json_reader.hpp>

#include <json/json.h>
#include <nlohmann/json.hpp>
#include <rapidjson/document.h>

#include <benchmark/benchmark.h>

#include <fstream>

class json_bench : public base_fixture {
public:
  std::string input;

  void SetUp(const benchmark::State&) override {
    std::ifstream t{twitter_json_file};
    if (!t) {
      fprintf(stderr, "failed to load %s\n", twitter_json_file);
      abort();
    }
    std::stringstream buffer;
    buffer << t.rdbuf();
    input = buffer.str();
  }
};

BENCHMARK_F(json_bench, caf_parse)(benchmark::State& state) {
  for (auto _ : state) {
    caf::json_reader reader;
    reader.load(input);
    benchmark::DoNotOptimize(reader);
  }
}

BENCHMARK_F(json_bench, rapidjson_parse)(benchmark::State& state) {
  for (auto _ : state) {
    rapidjson::Document document;
    document.Parse(input.c_str());
    benchmark::DoNotOptimize(document);
  }
}

BENCHMARK_F(json_bench, nlohmann_parse)(benchmark::State& state) {
  for (auto _ : state) {
    auto data = nlohmann::json::parse(input);
    benchmark::DoNotOptimize(data);
  }
}

BENCHMARK_F(json_bench, json_cpp_parse)(benchmark::State& state) {
  for (auto _ : state) {
    Json::Reader reader;
    Json::Value obj;
    reader.parse(input, obj, false);
    benchmark::DoNotOptimize(obj);
  }
}
