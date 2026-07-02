cmake_minimum_required(VERSION 3.16)
project(blink_tree CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug 构建默认开启
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# ThreadSanitizer 选项，-DTSAN=ON 时开启
option(TSAN "Enable ThreadSanitizer" OFF)
if(TSAN)
  add_compile_options(-fsanitize=thread -g)
  add_link_options(-fsanitize=thread)
endif()

# AddressSanitizer 选项，-DASAN=ON 时开启
option(ASAN "Enable AddressSanitizer" OFF)
if(ASAN)
  add_compile_options(-fsanitize=address -g)
  add_link_options(-fsanitize=address)
endif()

include_directories(include)

add_library(blink_tree_lib
  src/blink_tree.cpp
)

add_executable(blink_tree_test
  test/main.cpp
)

target_link_libraries(blink_tree_test
  blink_tree_lib
)