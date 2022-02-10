cmake_minimum_required(VERSION 3.10)

#set the project name
project(OSProject2122)

#includes
include_directories(../h/)

#sources
file(GLOB SOURCES "*.c" "../testers/p1test.c")

#add the executable
add_executable(OSProject2122 ${SOURCES})

