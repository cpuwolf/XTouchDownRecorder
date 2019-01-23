# Necessary CMake
project(xtouchdownrecorder CXX)
cmake_minimum_required(VERSION 2.8)

set(CEF_ROOT "F:/works/cef_binary_3.3599.1858.g285dbb1_windows64_minimal")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)


ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
SET_CEF_TARGET_OUT_DIR()
include_directories(${_CEF_ROOT})

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Include the SDK. Adjust this path to wherever you have the SDK repository
add_subdirectory(../ bin/)

set(CURL_STATICLIB ON CACHE BOOL "libcurl static build" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "libcurl exe build" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "libcurl test build" FORCE)
set(NROFF OFF CACHE BOOL "libcurl nroff build" FORCE)
IF(WIN32)
set(CMAKE_USE_WINSSL ON CACHE BOOL "libcurl winssl" FORCE)
ENDIF()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(../curl-7.58.0/ curl/)


add_definitions(-DCURL_STATICLIB)
set(LIBCURL_DIR ${PROJECT_SOURCE_DIR}/../curl-7.58.0)
#set(CURL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include)
#set(CURL_LIBRARY ${CMAKE_SOURCE_DIR}/lib)
set (LIBS ${LIBS} ${CEF_LIBRARIES} )

find_path(LIBCURL_INCLUDE_DIR curl/curl.h HINTS ${LIBCURL_DIR}/include)
include_directories(${LIBCURL_INCLUDE_DIR} ${CEF_INCLUDE_DIRS})
# Set up the plugin
add_xplane_plugin(xtouchdownrecorder xtouchdownrecorder.cpp lightworker.cpp cef3.cpp)

add_dependencies(${PROJECT_NAME} libcurl libcef_dll_wrapper)

COPY_FILES(xtouchdownrecorder "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(xtouchdownrecorder "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

IF(WIN32)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()
ENDIF()


#if (CURL_FOUND)
	include_directories(${CURL_INCLUDE_DIRS})
	target_link_libraries(xtouchdownrecorder PUBLIC libcurl libcef_lib libcef_dll_wrapper)
	add_definitions(-DCURL_STATICLIB)
#endif (CURL_FOUND)

PRINT_CEF_CONFIG()


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
add_executable (xtouchdownrecorder_ui cef3_ui.cpp)
target_link_libraries (xtouchdownrecorder_ui libcef_lib libcef_dll_wrapper)




