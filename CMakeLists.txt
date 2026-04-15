cmake_minimum_required(VERSION 3.16)
project(SputnikAuthV2)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(SputnikAuthV2 ${SOURCES})

find_package(CURL REQUIRED)

target_link_libraries(SputnikAuthV2 PRIVATE CURL::libcurl)

file(GLOB DLLS
    "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-mingw-dynamic/bin/*.dll"
)

add_custom_command(TARGET SputnikAuthV2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLLS}
    $<TARGET_FILE_DIR:SputnikAuthV2>
)