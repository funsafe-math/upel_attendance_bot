cmake_minimum_required(VERSION 3.9)

project(upel_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 0817715923c9705e68994eb52ef9df3f6845beba) # The commit hash for 1.10.x. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG  tags/v3.1.6
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(simdjson)

find_package(fmt)
find_package(spdlog REQUIRED)

#include(CheckIPOSupported)
#check_ipo_supported(RESULT supported OUTPUT error)

add_executable(upel_bot src/main.cpp src/state.hpp src/state.cpp)
target_link_libraries(upel_bot PRIVATE cpr::cpr fmt::fmt spdlog::spdlog simdjson)
#target_compile_options(upel_bot PRIVATE -Wall -Wextra -Wpedantic -Weffc++)
target_compile_options(upel_bot PRIVATE -Wall -Wextra -Wpedantic)
#set_property(TARGET upel_bot PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

#install(TARGETS upel_bot
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
