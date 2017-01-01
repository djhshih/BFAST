cmake_minimum_required(VERSION 3.7)
project(bfast C)

set(TARGET_VERSION_MAJOR 0)
set(TARGET_VERSION_MINOR 7)
set(TARGET_VERSION_PATCH 0b)
set(PROJECT_VERSION
	"${TARGET_VERSION_MAJOR}.${TARGET_VERSION_MINOR}.${TARGET_VERSION_PATCH}"
)

configure_file ("config.h.cmake.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})

find_package(BZIP2 REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(${BZIP2_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})

add_subdirectory(bfast) 
add_subdirectory(scripts)
add_subdirectory(butil)

