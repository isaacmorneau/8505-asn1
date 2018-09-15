cmake_minimum_required (VERSION 3.2)
project (8505-asn1)

set (8505-asn1_VERSION_MAJOR 1)
set (8505-asn1_VERSION_MINOR 0)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -fsanitze=address")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -s -fno-ident  -march=native -flto -DNDEBUG")

add_definitions(-D_POSIX_C_SOURCE=200809L)
add_definitions(-D_DEFAULT_SOURCE)

file(GLOB_RECURSE MAIN
    "src/**.c"
    )

list(REMOVE_ITEM MAIN ${CMAKE_CURRENT_SOURCE_DIR}/src/client.c)
list(REMOVE_ITEM MAIN ${CMAKE_CURRENT_SOURCE_DIR}/src/server.c)


add_executable(8505-asn1-server "src/server.c" ${MAIN})
add_executable(8505-asn1-client "src/client.c" ${MAIN})

target_include_directories(8505-asn1-server PUBLIC "src")
target_include_directories(8505-asn1-client PUBLIC "src")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(8505-asn1-server rt Threads::Threads)
target_link_libraries(8505-asn1-client rt Threads::Threads)
