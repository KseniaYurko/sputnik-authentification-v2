cmake_minimum_required(VERSION 3.16)
project(SputnikAuthV2)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.cc
)

add_executable(SputnikAuthV2 ${SOURCES})

find_package(CURL REQUIRED)
find_package(OpenMP)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/ProjectConfig.h.in
    ${CMAKE_BINARY_DIR}/generated/ProjectConfig.h
    @ONLY
)

target_include_directories(SputnikAuthV2 PRIVATE
    src
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(SputnikAuthV2 PRIVATE CURL::libcurl)

if(OpenMP_CXX_FOUND)
    target_link_libraries(SputnikAuthV2 PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(SputnikAuthV2 PRIVATE SPUTNIK_HAS_OPENMP=1)
endif()

file(GLOB DLLS
    "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-mingw-dynamic/bin/*.dll"
)

add_custom_command(TARGET SputnikAuthV2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLLS}
    $<TARGET_FILE_DIR:SputnikAuthV2>
)
