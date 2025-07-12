cmake_minimum_required(VERSION 3.22)
project(http_server)
find_package(OpenSSL REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(http_server main.cpp
        third_party/httplib.h
        third_party/json.hpp)
# After creating your executable
target_link_libraries(http_server
        OpenSSL::SSL
        OpenSSL::Crypto
)
