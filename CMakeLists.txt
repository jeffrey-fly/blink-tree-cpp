cmake_minimum_required(VERSION 3.16)
project(blink_tree CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# ThreadSanitizer option, enable with -DTSAN=ON
option(TSAN "Enable ThreadSanitizer" OFF)

# AddressSanitizer option, enable with -DASAN=ON
option(ASAN "Enable AddressSanitizer" OFF)

if(TSAN AND ASAN)
  message(FATAL_ERROR "TSAN and ASAN cannot be enabled at the same time")
endif()

add_library(blink_tree_lib
  src/blink_tree.cpp
)

target_include_directories(blink_tree_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(blink_tree_lib PUBLIC
  $<$<BOOL:${TSAN}>:-fsanitize=thread>
  $<$<BOOL:${ASAN}>:-fsanitize=address>
)

target_link_options(blink_tree_lib PUBLIC
  $<$<BOOL:${TSAN}>:-fsanitize=thread>
  $<$<BOOL:${ASAN}>:-fsanitize=address>
)

add_executable(blink_tree_test
  test/main.cpp
)

target_link_libraries(blink_tree_test
  PRIVATE
    blink_tree_lib
)

target_compile_definitions(blink_tree_lib PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)