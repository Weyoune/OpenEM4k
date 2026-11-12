# SPDX-License-Identifier: GPL-3.0-only
# OpenEM4k - the high resolution user interface for OpenEM, built step by step. First step: the match scenes.
#   cmake -S . -B build
#   cmake --build build --config Release
#   ctest --test-dir build -C Release
cmake_minimum_required(VERSION 3.24)
project(OpenEM4k VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()
find_package(Threads REQUIRED)

# Match scenes: own vector graphics, no SDL
add_library(em4k_match STATIC
    src/match/player.cpp
    src/match/rink.cpp
    src/match/scene.cpp
    src/match/text.cpp
    src/match/tor_script.cpp)
target_include_directories(em4k_match PUBLIC src)
target_link_libraries(em4k_match PUBLIC Threads::Threads)
# not em4k_match.lib: the program em4k_match would write its own files of that name next to it
set_target_properties(em4k_match PROPERTIES OUTPUT_NAME em4k_match_core)

# SDL2 for the window: the pinned release fetched at configure time, or an installed one
option(EM4K_BUILD_APPS "Build the programs with a window (needs SDL2)" ON)
option(EM4K_USE_SYSTEM_SDL2 "Use an installed SDL2 instead of fetching the pinned release" OFF)
if(EM4K_BUILD_APPS)
    if(EM4K_USE_SYSTEM_SDL2)
        find_package(SDL2 REQUIRED CONFIG)
        set(EM4K_SDL_TARGET SDL2::SDL2)
    else()
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-2.32.10
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(SDL2)
        set(EM4K_SDL_TARGET SDL2::SDL2-static)
    endif()
    if(TARGET SDL2::SDL2main)
        list(PREPEND EM4K_SDL_TARGET SDL2::SDL2main)
    endif()
    add_executable(em4k_match_viewer src/app/match_viewer.cpp)
    set_target_properties(em4k_match_viewer PROPERTIES OUTPUT_NAME em4k_match)
    target_link_libraries(em4k_match_viewer PRIVATE em4k_match ${EM4K_SDL_TARGET})
endif()

enable_testing()
add_executable(em4k_match_tests tests/match_tests.cpp)
target_link_libraries(em4k_match_tests PRIVATE em4k_match)
add_test(NAME match COMMAND em4k_match_tests)
# With the original game at hand (never part of this repository) the test also plays every script.
set(EM4K_DATA "" CACHE PATH "Directory of an installed Eishockey Manager (with TORE/)")
if(EM4K_DATA AND IS_DIRECTORY "${EM4K_DATA}/TORE")
    add_test(NAME match_original COMMAND em4k_match_tests "${EM4K_DATA}")
endif()
