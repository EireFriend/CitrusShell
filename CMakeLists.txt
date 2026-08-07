cmake_minimum_required(VERSION 4.2)
project(CitrusShell C)

set(CMAKE_C_STANDARD 11)

add_executable(CitrusShell main.c
        parser.h
        parser.c)
