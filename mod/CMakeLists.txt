cmake_minimum_required(VERSION 3.20)
project(BroomMount LANGUAGES CXX RC)

# Broom Mount: an ASI plugin for Crimson Desert that makes the unused flying
# broom a mount of its own. The file it produces is BroomMount.asi, and a
# release is that file alone. What is known is in private/KNOWLEDGE.md.

if(NOT WIN32)
    message(FATAL_ERROR "Broom Mount targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # static CRT, no redist needed

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# MinHook is here only for its HDE64 length decoder, which farhook uses to
# measure the bytes it steals. No MinHook hook is ever created.
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(minhook)

# BroomMount.ini's own bytes, as a header, so a DMM install (which is the
# plugin alone) can have the documented ini written beside it on first run
# without a second copy of the text going out of step with the first.
# embed_ini.cmake reads and writes HEX on purpose: carrying the file through
# CMake as a string rewrites every LF as CRLF.
set(BM_INI_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/ini_default.h")
add_custom_command(
    OUTPUT "${BM_INI_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND ${CMAKE_COMMAND} "-DIN=${CMAKE_CURRENT_SOURCE_DIR}/BroomMount.ini"
                             "-DOUT=${BM_INI_HEADER}"
                             -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_ini.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/BroomMount.ini"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_ini.cmake"
    COMMENT "Embedding BroomMount.ini"
    VERBATIM
)

add_library(BroomMount SHARED
    "${BM_INI_HEADER}"
    src/dllmain.cpp
    src/core/mod.cpp
    src/core/log.cpp
    src/core/paths.cpp
    src/game/mem.cpp
    src/game/hash.cpp
    src/game/farhook.cpp
    src/game/gamefile.cpp
    src/game/broomy.cpp
    src/game/broomrows.cpp
    src/game/broomchart.cpp
    src/game/tablefile.cpp
    src/game/lz4.cpp
    src/game/grant.cpp
    src/game/analogspeed.cpp
    src/game/callgate.cpp
    src/game/equipfix.cpp
    src/resources.rc
)

# BroomMount.ini again, this time as the INIMETA resource (src/resources.rc);
# rebuild when the ini's ;@ directives or defaults change.
set_source_files_properties(src/resources.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/BroomMount.ini")

target_include_directories(BroomMount PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated" ${minhook_SOURCE_DIR}/src/hde)
target_link_libraries(BroomMount PRIVATE minhook)
target_compile_definitions(BroomMount PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(BroomMount PRIVATE /utf-8 /W3)

set_target_properties(BroomMount PROPERTIES PREFIX "" SUFFIX ".asi" OUTPUT_NAME "BroomMount")

if(MSVC)
    target_compile_options(BroomMount PRIVATE /guard:cf)
    target_link_options(BroomMount PRIVATE /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /guard:cf)
endif()

# The offline check of Broomy's builders, never part of a normal build:
# build.bat check IN OUT.
add_executable(buildcheck EXCLUDE_FROM_ALL
    tests/buildcheck.cpp
    src/game/broomrows.cpp
    src/game/tablefile.cpp
    src/game/lz4.cpp
    src/game/hash.cpp
)
target_include_directories(buildcheck PRIVATE src)
target_compile_definitions(buildcheck PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(buildcheck PRIVATE /utf-8 /W3)

# The offline check of Broomy's charts: build.bat chartcheck IN OUT.
add_executable(chartcheck EXCLUDE_FROM_ALL
    tests/chartcheck.cpp
    src/game/broomchart.cpp
)
target_include_directories(chartcheck PRIVATE src)
target_compile_definitions(chartcheck PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(chartcheck PRIVATE /utf-8 /W3)

add_custom_command(TARGET BroomMount POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:BroomMount>" "${CMAKE_SOURCE_DIR}/dist/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/BroomMount.ini" "${CMAKE_SOURCE_DIR}/dist/"
    COMMENT "Staging dist/"
    VERBATIM
)
