# Set up the imported targets for X-Plane plugin libraries:

IF(APPLE)
    SET(XPLANE_LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdk/Libraries/Mac/")
ELSEIF(WIN32)
    SET(XPLANE_LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdk/Libraries/Win/")
ENDIF()

SET(XPLANE_INCLUDES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CHeaders/" CACHE STRING PATH)

IF(APPLE OR WIN32)

    IF(APPLE)
        FIND_LIBRARY(XPLM_LIBRARY XPLM "${XPLANE_LIBRARY_PATH}")
	    FIND_LIBRARY(XPWIDGETS_LIBRARY XPWidgets "${XPLANE_LIBRARY_PATH}")
        add_library(xplm STATIC IMPORTED GLOBAL)
        add_library(xpwidgets STATIC IMPORTED GLOBAL)
        SET_PROPERTY(TARGET xplm PROPERTY IMPORTED_LOCATION "${XPLM_LIBRARY}")
        SET_PROPERTY(TARGET xpwidgets PROPERTY IMPORTED_LOCATION "${XPWIDGETS_LIBRARY}")
    ELSEIF(WIN32)
        FIND_LIBRARY(XPLM_LIBRARY XPLM_64 "${XPLANE_LIBRARY_PATH}")
	    FIND_LIBRARY(XPWIDGETS_LIBRARY XPWidgets_64 "${XPLANE_LIBRARY_PATH}")
        add_library(xplm SHARED IMPORTED GLOBAL)
        add_library(xpwidgets SHARED IMPORTED GLOBAL)
        SET_PROPERTY(TARGET xplm PROPERTY IMPORTED_IMPLIB "${XPLM_LIBRARY}")
        SET_PROPERTY(TARGET xpwidgets PROPERTY IMPORTED_IMPLIB "${XPWIDGETS_LIBRARY}")	
    ENDIF()
    find_package(OpenGL)
    SET(XPLM_LIBRARY "${XPLM_LIBRARY}" PARENT_SCOPE)
    SET(XPWIDGETS_LIBRARY "${XPWIDGETS_LIBRARY}" PARENT_SCOPE)
ENDIF()

IF(NOT UNIX)
# For some reason this doesn't work. It's not worth wasting more time on fixing it, so
# I'll manually include the directories below
SET_PROPERTY(TARGET xplm APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CHeaders/XPLM" "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CHeaders/Wrappers")
SET_PROPERTY(TARGET xpwidgets APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CHeaders/XPLM" "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CHeaders/Widgets" "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CHeaders/Wrappers")
ENDIF()

FUNCTION(add_xplane_sdk_definitions library_name) 
    IF(APPLE)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DAPL=1 -DIBM=0 -DLIN=0)
    ELSEIF(UNIX)
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DAPL=0 -DIBM=0 -DLIN=1)
    ELSEIF(WIN32)
        TARGET_COMPILE_OPTIONS(${library_name} PRIVATE "/MD$<$<CONFIG:Debug>:d>")
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DAPL=0 -DIBM=1 -DLIN=0)
    ENDIF()

    TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -DXPLM200=1 -DXPLM210=1)
    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC "${XPLANE_INCLUDES_PATH}/XPLM" "${XPLANE_INCLUDES_PATH}/Widgets" "${XPLANE_INCLUDES_PATH}/Wrappers")
ENDFUNCTION(add_xplane_sdk_definitions)

# Set up an X-Plane plugin
FUNCTION(add_xplane_plugin library_name ...)
    SET(FILES ${ARGV})
    LIST(REMOVE_AT FILES 0)
    MESSAGE(STATUS "Making X-Plane plugin called ${library_name}")

    IF(APPLE)
        ADD_EXECUTABLE(${library_name} ${FILES})
        SET_TARGET_PROPERTIES(${library_name} PROPERTIES OUTPUT_NAME "mac.xpl")
    ELSEIF(UNIX)
        ADD_LIBRARY(${library_name} SHARED ${FILES})
        SET_TARGET_PROPERTIES(${library_name} PROPERTIES OUTPUT_NAME "lin.xpl")
    ELSEIF(WIN32)
        ADD_LIBRARY(${library_name} MODULE ${FILES})
        TARGET_COMPILE_DEFINITIONS(${library_name} PUBLIC -D_CRT_SECURE_NO_WARNINGS /bigobj)
        SET_TARGET_PROPERTIES(${library_name} PROPERTIES OUTPUT_NAME "win.xpl")
    ENDIF()
    add_xplane_sdk_definitions(${library_name})

    #link flags
    IF(APPLE)
        SET_PROPERTY(TARGET ${library_name} APPEND_STRING PROPERTY LINK_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fPIC -fvisibility=hidden -bundle")
    ELSEIF(UNIX)
        SET_PROPERTY(TARGET ${library_name} APPEND_STRING PROPERTY LINK_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -rdynamic -undefined_warning -fPIC -fvisibility=hidden")
    ENDIF()

	TARGET_LINK_LIBRARIES(${library_name} PUBLIC ${XPLM_LIBRARY} ${XPWIDGETS_LIBRARY} ${OPENGL_gl_LIBRARY} ${GLUT_LIBRARIES} )
    TARGET_INCLUDE_DIRECTORIES(${library_name} PUBLIC "${XPLANE_INCLUDES_PATH}/XPLM" "${XPLANE_INCLUDES_PATH}/Widgets" "${XPLANE_INCLUDES_PATH}/Wrappers")
ENDFUNCTION(add_xplane_plugin)
