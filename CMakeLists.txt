cmake_minimum_required(VERSION 3.14)
project(apiserver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(Threads REQUIRED)

add_executable(apiserver main.cpp)
target_include_directories(apiserver PRIVATE include)
target_link_libraries(apiserver PRIVATE Threads::Threads)

# OpenSSL optional — fallback ke hash internal kalau tidak ada
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  message(STATUS "OpenSSL found — using SHA256 from libcrypto")
  target_compile_definitions(apiserver PRIVATE HAVE_OPENSSL=1)
  target_link_libraries(apiserver PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
  message(STATUS "OpenSSL NOT found — using built-in fallback hash")
endif()
