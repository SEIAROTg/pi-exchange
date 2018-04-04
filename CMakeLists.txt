cmake_minimum_required(VERSION 3.8)

project(pi-exchange CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DPIEX_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g ${CMAKE_CXX_FLAGS_RELEASE}")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

find_package(Boost)

include_directories(${CMAKE_SOURCE_DIR} ${Boost_INCLUDE_DIRS})

add_executable(exchange src/main.cpp)

add_executable(benchmark EXCLUDE_FROM_ALL benchmark/main.cpp)

add_executable(tests EXCLUDE_FROM_ALL tests/order.cpp tests/order-book.cpp tests/exchange.cpp tests/packets.cpp tests/socket.cpp tests/server.cpp)
target_link_libraries(tests gtest_main gmock)

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
