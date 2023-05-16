cmake_minimum_required(VERSION 3.18)

set(PROJECT_TARGET KnurDash)

project(${PROJECT_TARGET} VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

add_executable(${PROJECT_TARGET} main.c)
configure_file(${PROJECT_TARGET}.ui ${PROJECT_TARGET}.ui)
configure_file(${PROJECT_TARGET}.css ${PROJECT_TARGET}.css)

target_include_directories(${PROJECT_TARGET} PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories(${PROJECT_TARGET} PRIVATE ${GTK3_LIBRARY_DIRS})
target_compile_options(${PROJECT_TARGET} PRIVATE ${GTK3_CFLAGS_OTHER})
target_link_libraries(${PROJECT_TARGET} PRIVATE ${GTK3_LIBRARIES})
