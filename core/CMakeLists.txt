# Necessary CMake
project(xtouchdownrecorder C)
cmake_minimum_required(VERSION 2.8)



# Include the SDK. Adjust this path to wherever you have the SDK repository
add_subdirectory(../ bin/)

# Set up the plugin
add_xplane_plugin(xtouchdownrecorder xtouchdownrecorder.c)


find_package(CURL)
if (CURL_FOUND)
	include_directories(${CURL_INCLUDE_DIRS})
	target_link_libraries(xtouchdownrecorder PUBLIC ${CURL_LIBRARIES})
endif (CURL_FOUND)




