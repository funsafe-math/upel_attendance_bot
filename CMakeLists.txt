cmake_minimum_required(VERSION 3.5)

project(upel_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 0817715923c9705e68994eb52ef9df3f6845beba) # The commit hash for 1.10.x. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

find_package(fmt)

add_executable(upel_bot src/main.cpp)
target_link_libraries(upel_bot PRIVATE cpr::cpr fmt::fmt)

#install(TARGETS upel_bot
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
