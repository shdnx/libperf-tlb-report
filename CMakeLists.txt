cmake_minimum_required(VERSION 2.6)
project(libperf_tlb_report VERSION 0.9 LANGUAGES C)

option(STANDALONE ON "Whether to register a constructor and destructor to automatically call perfevents_init() and perfevents_finalize()")
option(USE_GROUPS OFF "Whether to collect all performance events in a single group using PERF_FORMAT_GROUP. Experimental.")

configure_file(
  "${PROJECT_SOURCE_DIR}/buildconfig.h.in"
  "${PROJECT_BINARY_DIR}/include/build/config.h"
)

set(BIN "perf-tlb-report")

add_library(${BIN}
  src/libperf-tlb-report.c
)

target_include_directories(${BIN}
  PUBLIC
    "${PROJECT_BINARY_DIR}/include"
    "${PROJECT_SOURCE_DIR}"
)

target_compile_options(${BIN}
  "-Wall"
  "-Wextra"
  "-Wno-macro-redefined"
)

target_compile_features(${BIN} PUBLIC c_std_11)
