cmake_minimum_required (VERSION 2.6)
set (CMAKE_CXX_STANDARD 14)

project (CCTS)

add_executable(DEMO
	src/demo.cpp
)

add_executable(MV2PL
	src/MV2PL.cpp
)

add_executable(MVTO
	src/MVTO.cpp
)

target_link_libraries (MV2PL pthread)
target_link_libraries (MVTO pthread)

include_directories("./include")
include_directories($ENV{BOOST_ROOT})
