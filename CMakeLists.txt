cmake_minimum_required(VERSION 4.2)
project(CitrusShell C)

set(CMAKE_C_STANDARD 11)

add_executable(CitrusShell src/main.c
        src/parser.h
        src/parser.c
        src/completion.c
        src/completion.h
        src/jobs.h
        src/jobs.c
        src/terminal.c
        src/terminal.h
        src/user.c
        src/user.h)

# macOS ships a BSD libedit under the readline.h name, not real GNU readline,
# so linking just "-lreadline" resolves to nothing (or the wrong lib) unless it points CMake at another copy
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