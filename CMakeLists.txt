cmake_minimum_required(VERSION 3.21)
project(binprotect CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS src/*.cpp src/*.hpp src/*.h)
file(GLOB_RECURSE external_files CONFIGURE_DEPENDS ext/*.cpp ext/*.hpp ext/*.h)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_files} ${external_files})

add_executable(binprotect ${source_files} ${external_files})

find_package(spdlog CONFIG REQUIRED)
find_package(Zydis CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)

target_include_directories(binprotect PRIVATE ext)
target_link_libraries(binprotect PRIVATE spdlog::spdlog Zydis::Zydis argparse::argparse)

if (MSVC)
    target_compile_options(binprotect PRIVATE /utf-8)
endif()
