cmake_minimum_required(VERSION 3.15...4.0)

project(coinbase_feed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.83 REQUIRED)
find_package(pybind11 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)

add_executable(coinbase_feed src/coinbase_feed.cpp)

target_sources(coinbase_feed PRIVATE src/coinbase_feed.cpp)

target_link_libraries(coinbase_feed PRIVATE pybind11::embed jsoncpp ssl crypto)

install(TARGETS coinbase_feed DESTINATION bin)
