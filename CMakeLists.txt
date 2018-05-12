cmake_minimum_required(VERSION 3.8)

project(pi-exchange CXX)
include(CheckCXXCompilerFlag)

# deciding target architecture: cortex-a53 / native / default
set(PIEX_ARCH_FLAGS "-march=armv8-a -mcpu=cortex-a53")
check_cxx_compiler_flag(${PIEX_ARCH_FLAGS} PIEX_ARCH_A53_SUPPORTED)
if (PIEX_ARCH_A53_SUPPORTED)
	message("-- Target architecture: cortex-a53")
else()
	set(PIEX_ARCH_FLAGS "-march=native")
	check_cxx_compiler_flag(${PIEX_ARCH_FLAGS} PIEX_ARCH_NATIVE_SUPPORTED)
	if (PIEX_ARCH_NATIVE_SUPPORTED)
		message("-- Target architecture: native")
	else()
		set(PIEX_ARCH_FLAGS "")
		message("-- Target architecture: default")
	endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DPIEX_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${PIEX_ARCH_FLAGS} -O3 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g ${CMAKE_CXX_FLAGS_RELEASE}")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

find_package(Boost)

include_directories(${CMAKE_SOURCE_DIR} ${Boost_INCLUDE_DIRS})
find_library(LIBATOMIC
	NAMES atomic atomic.so.1 libatomic.so.1
	HINTS
	/usr/lib64
	/usr/lib
	/lib64
	/lib
)
if (LIBATOMIC)
	link_libraries(atomic)
endif()

add_executable(exchange src/main.cpp)
target_link_libraries(exchange foonathan_memory)

add_executable(benchmark EXCLUDE_FROM_ALL benchmark/main.cpp)
target_link_libraries(benchmark m foonathan_memory)

add_executable(tests EXCLUDE_FROM_ALL tests/order.cpp tests/order-book.cpp tests/exchange.cpp tests/packets.cpp tests/socket.cpp tests/server.cpp)
target_link_libraries(tests gtest_main gmock foonathan_memory)

if (CMAKE_BUILD_TYPE MATCHES Debug)
	target_compile_options(tests PRIVATE --coverage)
	target_link_libraries(tests --coverage)
	add_custom_command(TARGET tests POST_BUILD COMMAND "${CMAKE_SOURCE_DIR}/tools/coverage-clean.sh")
endif()

configure_file(cmake/googletest.CMakeLists.txt googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
if(result)
	message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
if(result)
	message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

add_subdirectory(
	"${CMAKE_BINARY_DIR}/googletest-src"
	"${CMAKE_BINARY_DIR}/googletest-build"
	EXCLUDE_FROM_ALL
)

configure_file(cmake/memory.CMakeLists.txt memory-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/memory-download)
if(result)
	message(FATAL_ERROR "CMake step for memory failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/memory-download)
if(result)
	message(FATAL_ERROR "Build step for memory failed: ${result}")
endif()

add_subdirectory(
	"${CMAKE_BINARY_DIR}/memory-src"
	"${CMAKE_BINARY_DIR}/memory-build"
	EXCLUDE_FROM_ALL
)
