cmake_minimum_required(VERSION 4.2)
project(CitrusShell C)

set(CMAKE_C_STANDARD 11)

add_executable(CitrusShell main.c
        parser.h
        parser.c
        completion.c
        completion.h)

# --- Readline ---
# macOS ships a BSD libedit under the readline.h name, not real GNU readline,
# so linking just "-lreadline" resolves to nothing (or the wrong lib) unless
# we point CMake at another copy explicitly.
if(APPLE)
    execute_process(
            COMMAND brew --prefix readline
            OUTPUT_VARIABLE READLINE_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    if(READLINE_PREFIX)
        set(READLINE_INCLUDE_DIR "${READLINE_PREFIX}/include")
        set(READLINE_LIBRARY "${READLINE_PREFIX}/lib/libreadline.dylib")
    endif()
endif()

find_path(READLINE_INCLUDE_DIR readline/readline.h)
find_library(READLINE_LIBRARY readline)

if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARY)
    message(FATAL_ERROR "GNU readline not found. If on macOS run: brew install readline")
endif()

target_include_directories(CitrusShell PRIVATE ${READLINE_INCLUDE_DIR})
target_link_libraries(CitrusShell ${READLINE_LIBRARY})