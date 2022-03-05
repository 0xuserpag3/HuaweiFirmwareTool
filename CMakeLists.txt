cmake_minimum_required(VERSION 3.12)

project(hw_fmw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hw_fmw hw_fmw.cpp)
add_executable(hw_sign hw_sign.cpp)
add_executable(hw_verify hw_verify.cpp)

add_library(util STATIC util_hw.cpp util_rsa.cpp util.cpp)

find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
  target_link_libraries(util OpenSSL::Crypto)
endif(OPENSSL_FOUND)

find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
  target_link_libraries(util ZLIB::ZLIB)
endif(ZLIB_FOUND)

# https://en.cppreference.com/w/cpp/filesystem
# Using this library may require additional compiler/linker options. GNU implementation prior to 9.1 requires linking with -lstdc++fs and LLVM implementation prior to LLVM 9.0 requires linking with -lc++fs. 
target_link_libraries(util stdc++fs)

target_link_libraries(hw_fmw PRIVATE util)
target_link_libraries(hw_sign PRIVATE util)
target_link_libraries(hw_verify PRIVATE util)

