cmake_minimum_required(VERSION 3.18)

set(PROJECT_TARGET KnurDash)

project(${PROJECT_TARGET} VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

find_path(pigpio_INCLUDE_DIR 
	NAMES pigpio.h pigpiod_if.h pigpiod_if2.h
	HINTS /usr/include)

find_library(pigpio_LIBRARY 
	NAMES libpigpio.so
	HINTS /usr/lib)
find_library(pigpiod_if_LIBRARY 
	NAMES libpigpiod_if.so
	HINTS /usr/lib)
find_library(pigpiod_if2_LIBRARY 
	NAMES libpigpiod_if2.so
	HINTS /usr/lib)

set(pigpio_INCLUDE_DIRS ${pigpio_INCLUDE_DIR})
set(pigpio_INCLUDES     ${pigpio_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(pigpio 
    DEFAULT_MSG 
    pigpio_INCLUDE_DIR pigpio_LIBRARY pigpiod_if_LIBRARY pigpiod_if2_LIBRARY)

add_executable(${PROJECT_TARGET} main.c)
configure_file(${PROJECT_TARGET}.ui ${PROJECT_TARGET}.ui)
configure_file(${PROJECT_TARGET}.css ${PROJECT_TARGET}.css)

target_include_directories(${PROJECT_TARGET} PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories(${PROJECT_TARGET} PRIVATE ${GTK3_LIBRARY_DIRS})
target_compile_options(${PROJECT_TARGET} PRIVATE ${GTK3_CFLAGS_OTHER})
target_link_libraries(${PROJECT_TARGET} PRIVATE ${GTK3_LIBRARIES})
