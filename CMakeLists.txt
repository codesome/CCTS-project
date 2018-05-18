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

add_executable(PriorityLocking
	src/PriorityLocking.cpp
)

add_executable(2PL
	src/2PL.cpp
)


target_link_libraries (MV2PL pthread)
target_link_libraries (MVTO pthread)
target_link_libraries (PriorityLocking pthread)
target_link_libraries (2PL pthread)

include_directories("./include")
include_directories($ENV{BOOST_ROOT})
