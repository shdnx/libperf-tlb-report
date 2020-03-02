cmake_minimum_required(VERSION 3.5)
project(libperf_tlb_report LANGUAGES C)

option(STANDALONE "Whether to register a constructor and destructor to automatically call perfevents_init() and perfevents_finalize()" ON)
option(USE_GROUPS "Whether to collect all performance events in a single group using PERF_FORMAT_GROUP. Experimental." OFF)

configure_file(
  "${PROJECT_SOURCE_DIR}/buildconfig.h.in"
  "${PROJECT_BINARY_DIR}/include/build/config.h"
  @ONLY
)

set(LIB "perf-tlb-report")

add_library(${LIB}
  libperf-tlb-report.c
)

target_include_directories(${LIB}
  PUBLIC
    "${PROJECT_BINARY_DIR}/include"
    "${PROJECT_SOURCE_DIR}"
)

target_compile_options(${LIB}
  PRIVATE
    "-Wall"
    "-Wextra"
    "-Wno-macro-redefined"
)

set_target_properties(${LIB}
  PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS ON
)
