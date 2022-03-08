cmake_minimum_required(VERSION 3.22)

PROJECT(tidbits)

find_package(Boost COMPONENTS program_options)
if (Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
endif()


add_executable(Blanch Blanch.cc)
target_link_libraries(Blanch Boost::program_options)

add_executable(BreakLinks BreakLinks.cc)

add_executable(TestStub TestStub.cc)

SET_TARGET_PROPERTIES(TestStub PROPERTIES
	COMPILE_FLAGS "-frepo -std=c++0x -pedantic -flto"
	LINK_FLAGS "-frepo -std=c++0x -pedantic -flto"
	)
