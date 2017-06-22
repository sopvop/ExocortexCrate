cmake_minimum_required(VERSION 3.6)

# Have Linux builds use the same _DEBUG / NDEBUG defines as Microsoft's C++ compiler.
if( UNIX )
        IF (CMAKE_BUILD_TYPE MATCHES "Debug" )
                ADD_DEFINITIONS(-D_DEBUG)
        ELSE()
                ADD_DEFINITIONS(-DNDEBUG)
        ENDIF()
ENDIF()

SET( PROJECT_NAME "ExocortexCrate" )

IF (UNIX AND NOT APPLE )
        ADD_DEFINITIONS(-DLINUX )
ENDIF()

IF( WIN32 )
        ADD_DEFINITIONS( -D_WIN64 -DWIN64 )
ENDIF()

set( Exocortex_INSTALL_BASE_DIR "${CMAKE_INSTALL_PREFIX}" )

project( ${PROJECT_NAME} )

# Configuration
SET( crate_MAJOR_VERSION "1" )
SET( crate_MINOR_VERSION "1" )
SET( crate_BUILD_VERSION "151")
SET( crate_VERSION "${crate_MAJOR_VERSION}.${crate_MINOR_VERSION}" )
SET( crate_FULLVERSION "${crate_MAJOR_VERSION}.${crate_MINOR_VERSION}.${crate_BUILD_VERSION}" )

add_definitions( "-Dcrate_MAJOR_VERSION=${crate_MAJOR_VERSION}" )
add_definitions( "-Dcrate_MINOR_VERSION=${crate_MINOR_VERSION}" )
add_definitions( "-Dcrate_BUILD_VERSION=${crate_BUILD_VERSION}" )

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(BOOST_COMPONENTS date_time program_options regex thread filesystem system unit_test_framework )
find_package( Boost COMPONENTS REQUIRED ${BOOST_COMPONENTS} )

find_package( Maya REQUIRED)
find_package( ZLIB REQUIRED)
find_package( IlmBase REQUIRED)
find_package( X11 REQUIRED )
find_package( OpenGL REQUIRED )
find_package( Alembic REQUIRED )
find_package( HDF5 REQUIRED COMPONENTS CXX HL C)

include_directories(${X11_INCLUDE_DIR})
include_directories(${ILMBASE_INCLUDE_DIRS} ${ILMBASE_INCLUDE_DIR}/OpenEXR)
include_directories( ${Boost_INCLUDE_DIRS} )
include_directories( ${HDF5_INCLUDE_DIRS} )

link_directories( ${Boost_LIBRARY_DIRS} )
link_directories( ${MAYA_LIBRARY_DIR} )

SET( BUILD_SHARED_LIBS OFF )

add_definitions( -Dcrate_ver=crate-${crate_FULLVERSION} )

SET( ALEMBIC_ILMBASE_LIBS ${ILMBASE_LIBRARIES} )
SET( ALEMBIC_ZLIB_LIBS ${ZLIB_LIBRARIES} )
SET( ALEMBIC_HDF5_LIBS ${HDF5_HL_LIBRARIES} ${HDF5_LIBRARIES} )

# Alembic
add_definitions( -DNOMINMAX )

SET( ALL_ALEMBIC_LIBS Alembic::Alembic ${ALEMBIC_HDF5_LIBS} ${ALEMBIC_ZLIB_LIBS}
                ${ALEMBIC_ILMBASE_LIBS} ${ZLIB_LIBRARIES} ${Boost_LIBRARIES} )

ADD_SUBDIRECTORY ( "${CMAKE_CURRENT_SOURCE_DIR}/Shared/CommonUtils" "${CMAKE_CURRENT_BINARY_DIR}/Shared/CommonUtils" )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/Shared/CommonUtils )

add_definitions( -D_WINSOCKAPI_ )
add_definitions( -D_WINSOCKAPI2_ )


# Python extension
IF(FALSE ) #EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Python/CMakeLists.txt")
        if( NOT MSVC10 AND NOT MSVC11 )
                ADD_SUBDIRECTORY ( "${CMAKE_CURRENT_SOURCE_DIR}/Python/2.6.7" "${CMAKE_CURRENT_BINARY_DIR}/ExocortexAlembicPython/2.6.7" )
                if( WIN32 )
                        ADD_SUBDIRECTORY ( "${CMAKE_CURRENT_SOURCE_DIR}/Python/2.7.2" "${CMAKE_CURRENT_BINARY_DIR}/ExocortexAlembicPython/2.7.2" )
                else()
                        ADD_SUBDIRECTORY ( "${CMAKE_CURRENT_SOURCE_DIR}/Python/2.5.2" "${CMAKE_CURRENT_BINARY_DIR}/ExocortexAlembicPython/2.5.2" )
                endif()
        endif()
ENDIF()


# maya plugin
#TODO fix version thing
set (Maya_VERSION 2016)
add_subdirectory ( "${CMAKE_CURRENT_SOURCE_DIR}/Maya" )
