cmake_minimum_required (VERSION 2.6)
set (CMAKE_CXX_STANDARD 14)

project (CCTS)

add_executable(DEMO
	src/demo.cpp
)

target_link_libraries (DEMO pthread)

include_directories("./include")
