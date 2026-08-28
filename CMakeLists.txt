cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(snake-game LANGUAGES C)

set(SNAKE_SOURCES
    main.c
    colors.c
    colors.h
)

add_executable(snake ${SNAKE_SOURCES})
target_include_directories(snake PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(snake PRIVATE c_std_23)
target_compile_options(snake PRIVATE -Wall -Wextra)

if(WIN32)
    # PDCurses (wincon backend), built from source via FetchContent.
    include(FetchContent)
    FetchContent_Declare(
        pdcurses
        GIT_REPOSITORY https://github.com/wmcbrine/PDCurses.git
        GIT_TAG        3.9
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(pdcurses)

    # PDCurses ships its own subdir builds rather than a top-level CMake target.
    # Compile the wincon backend as a static lib here.
    file(GLOB PDCURSES_CORE   "${pdcurses_SOURCE_DIR}/pdcurses/*.c")
    file(GLOB PDCURSES_WINCON "${pdcurses_SOURCE_DIR}/wincon/*.c")
    add_library(pdcurses STATIC ${PDCURSES_CORE} ${PDCURSES_WINCON})
    target_include_directories(pdcurses PUBLIC "${pdcurses_SOURCE_DIR}")
    target_compile_definitions(pdcurses PUBLIC PDC_WIDE)

    # main.c uses `#include "PDCurses/curses.h"` on Windows.
    # Stage the header into a PDCurses/ subdir of the build tree so that
    # include path resolves without touching sources or relying on symlinks.
    file(COPY "${pdcurses_SOURCE_DIR}/curses.h"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/PDCurses")
    target_include_directories(snake PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

    target_link_libraries(snake PRIVATE pdcurses)
else()
    # Linux / macOS: link the system curses. On macOS, prefer Homebrew's
    # ncurses if present — the system one is typically outdated.
    if(APPLE)
        find_program(BREW_EXECUTABLE brew)
        if(BREW_EXECUTABLE)
            execute_process(
                COMMAND "${BREW_EXECUTABLE}" --prefix ncurses
                OUTPUT_VARIABLE BREW_NCURSES_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE BREW_NCURSES_RC)
            if(BREW_NCURSES_RC EQUAL 0 AND IS_DIRECTORY "${BREW_NCURSES_PREFIX}")
                list(PREPEND CMAKE_PREFIX_PATH "${BREW_NCURSES_PREFIX}")
                message(STATUS "Using Homebrew ncurses at ${BREW_NCURSES_PREFIX}")
            endif()
        endif()
    endif()

    set(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)
    target_include_directories(snake PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(snake PRIVATE ${CURSES_LIBRARIES})
endif()
