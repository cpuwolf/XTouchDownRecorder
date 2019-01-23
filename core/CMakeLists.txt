# Necessary CMake
project(xtouchdownrecorder CXX)
cmake_minimum_required(VERSION 2.8)



# Include the SDK. Adjust this path to wherever you have the SDK repository
add_subdirectory(../ bin/)

set(CURL_STATICLIB ON CACHE BOOL "libcurl static build" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "libcurl exe build" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "libcurl test build" FORCE)
IF(WIN32)
set(CMAKE_USE_WINSSL ON CACHE BOOL "libcurl winssl" FORCE)
ENDIF()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(../curl-7.58.0/ curl/)


add_definitions(-DCURL_STATICLIB)
set(LIBCURL_DIR ${PROJECT_SOURCE_DIR}/../curl-7.58.0)
#set(CURL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include)
#set(CURL_LIBRARY ${CMAKE_SOURCE_DIR}/lib)

find_path(LIBCURL_INCLUDE_DIR curl/curl.h HINTS ${LIBCURL_DIR}/include)
include_directories(${LIBCURL_INCLUDE_DIR})
# Set up the plugin
add_xplane_plugin(xtouchdownrecorder xtouchdownrecorder.cpp lightworker.cpp lightworker.h)

add_dependencies(${PROJECT_NAME} libcurl)

#find_package(CURL REQUIRED)
#if (CURL_FOUND)
	include_directories(${CURL_INCLUDE_DIRS})
	target_link_libraries(xtouchdownrecorder PUBLIC libcurl)
	add_definitions(-DCURL_STATICLIB)
#endif (CURL_FOUND)




