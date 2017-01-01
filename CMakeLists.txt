cmake_minimum_required(VERSION 3.6)
project(bfast C)

include(CheckSymbolExists)

set(TARGET_VERSION_MAJOR 0)
set(TARGET_VERSION_MINOR 7)
set(TARGET_VERSION_PATCH 0b)
set(PROJECT_VERSION
	"${TARGET_VERSION_MAJOR}.${TARGET_VERSION_MINOR}.${TARGET_VERSION_PATCH}"
)

check_symbol_exists(strtok_r "string.h" HAVE_STRTOK_R)

configure_file ("config.h.cmake.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})

find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads)

include_directories(${BZIP2_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})

set(STANDARD_LIBRARIES "m ${CMAKE_THREAD_LIBS_INIT}")

add_subdirectory(bfast) 
add_subdirectory(scripts)
add_subdirectory(butil)

