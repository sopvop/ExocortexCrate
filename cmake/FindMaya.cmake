
if(APPLE)
    find_path(MAYA_BASE_DIR
            ../../devkit/include/maya/MFn.h
        HINTS
            ${MAYA_LOCATION}
            $ENV{MAYA_LOCATION}
            "/Applications/Autodesk/maya2014/Maya.app/Contents"
            "/Applications/Autodesk/maya2013.5/Maya.app/Contents"
            "/Applications/Autodesk/maya2013/Maya.app/Contents"
            "/Applications/Autodesk/maya2012.17/Maya.app/Contents"
            "/Applications/Autodesk/maya2012/Maya.app/Contents"
            "/Applications/Autodesk/maya2011/Maya.app/Contents"
            "/Applications/Autodesk/maya2010/Maya.app/Contents"
    )
    find_path(MAYA_LIBRARY_DIR libOpenMaya.dylib
        HINTS
            ${MAYA_LOCATION}
            $ENV{MAYA_LOCATION}
            ${MAYA_BASE_DIR}
        PATH_SUFFIXES
            Maya.app/contents/MacOS/
        DOC
            "Maya's libraries path"
    )
endif(APPLE)

if(UNIX)
    find_path(MAYA_BASE_DIR
            include/maya/MFn.h
        HINTS
            ${MAYA_LOCATION}
            $ENV{MAYA_LOCATION}
            "/var/empty/autodesk/maya2013-x64"
            "/var/empty/autodesk/maya2012.17-x64"
            "/var/empty/autodesk/maya2012-x64"
            "/var/empty/autodesk/maya2011-x64"
            "/var/empty/autodesk/maya2010-x64"
    )
    find_path(MAYA_LIBRARY_DIR
            libOpenMaya.so
        HINTS
            ${MAYA_LOCATION}
            $ENV{MAYA_LOCATION}
            ${MAYA_BASE_DIR}
        PATH_SUFFIXES
            lib/
        DOC
            "Maya's libraries path"
    )
endif(UNIX)

if(WIN32)
    find_path(MAYA_BASE_DIR
            include/maya/MFn.h
        HINTS
            ${MAYA_LOCATION}
            $ENV{MAYA_LOCATION}
            "C:/Program Files/Autodesk/Maya2013.5-x64"
            "C:/Program Files/Autodesk/Maya2013.5"
            "C:/Program Files (x86)/Autodesk/Maya2013.5"
            "C:/Autodesk/maya-2013.5x64"
            "C:/Program Files/Autodesk/Maya2013-x64"
            "C:/Program Files/Autodesk/Maya2013"
            "C:/Program Files (x86)/Autodesk/Maya2013"
            "C:/Autodesk/maya-2013x64"
            "C:/Program Files/Autodesk/Maya2012-x64"
            "C:/Program Files/Autodesk/Maya2012"
            "C:/Program Files (x86)/Autodesk/Maya2012"
            "C:/Autodesk/maya-2012x64"
            "C:/Program Files/Autodesk/Maya2011-x64"
            "C:/Program Files/Autodesk/Maya2011"
            "C:/Program Files (x86)/Autodesk/Maya2011"
            "C:/Autodesk/maya-2011x64"
            "C:/Program Files/Autodesk/Maya2010-x64"
            "C:/Program Files/Autodesk/Maya2010"
            "C:/Program Files (x86)/Autodesk/Maya2010"
            "C:/Autodesk/maya-2010x64"
    )
    find_path(MAYA_LIBRARY_DIR
            OpenMaya.lib
        HINTS
            ${MAYA_LOCATION}
            $ENV{MAYA_LOCATION}
            ${MAYA_BASE_DIR}
        PATH_SUFFIXES
            lib/
        DOC
            "Maya's libraries path"
    )
endif(WIN32)

find_path(MAYA_INCLUDE_DIR
        maya/MFn.h
    HINTS
        ${MAYA_LOCATION}
        $ENV{MAYA_LOCATION}
        ${MAYA_BASE_DIR}
    PATH_SUFFIXES
        ../../devkit/include/
        include/
    DOC
        "Maya's devkit headers path"
)

find_path(MAYA_LIBRARY_DIR
        OpenMaya
    HINTS
        ${MAYA_LOCATION}
        $ENV{MAYA_LOCATION}
        ${MAYA_BASE_DIR}
    PATH_SUFFIXES
        ../../devkit/include/
        include/
    DOC
        "Maya's devkit headers path"
)

list(APPEND MAYA_INCLUDE_DIRS ${MAYA_INCLUDE_DIR})

#find_path(MAYA_DEVKIT_INC_DIR
#       GL/glext.h
#    HINTS
#        ${MAYA_LOCATION}
#        $ENV{MAYA_LOCATION}
#        ${MAYA_BASE_DIR}
#    PATH_SUFFIXES
#        /devkit/plug-ins/
#    DOC
#        "Maya's devkit headers path"
#)

#list(APPEND MAYA_INCLUDE_DIRS ${MAYA_DEVKIT_INC_DIR})

if ( NOT WINDOWS )
  if ( NOT DARWIN )
    set ( MAYA_EXTENSION ".so" )
    set(MAYA_COMPILE_FLAGS
      "-DBits64_ -m64 -DUNIX -D_BOOL -DLINUX -DFUNCPROTO -D_GNU_SOURCE -DLINUX_64 -fPIC -fno-strict-aliasing -DREQUIRE_IOSTREAM -Wno-multichar -Wno-comment -Wno-sign-compare -funsigned-char -pthread -Wno-deprecated -Wno-reorder -fno-gnu-keywords")
    set(MAYA_LINK_FLAGS "${MAYA_COMPILE_FLAGS} -Wl,-Bsymbolic")
  else()
    set ( MAYA_EXTENSION ".bundle" )
    set( MAYA_COMPILE_FLAGS
      "-DAW_NEW_IOSTREAMS -DCC_GNU_ -DOSMac_ -DOSMacOSX_ -DOSMac_MachO_ -DREQUIRE_IOSTREAM -fno-gnu-keywords -D_LANGUAGE_C_PLUS_PLUS -include ${MAYA_INCLUDE_PATH}/maya/OpenMayaMac.h" )

    set(MAYA_LINK_FLAGS
      "-fno-gnu-keywords -framework System  -framework SystemConfiguration -framework CoreServices -framework Carbon -framework Cocoa -framework ApplicationServices -framework Quicktime -framework IOKit -bundle -fPIC -L${ALEMBIC_MAYA_LIB_ROOT} -Wl,-executable_path,${ALEMBIC_MAYA_LIB_ROOT}" )
  endif()
else()
  set( MAYA_EXTENSION ".mll" )
  set( MAYA_COMPILE_FLAGS "/MD /D \"NT_PLUGIN\" /D \"REQUIRE_IOSTREAM\" /D \"_BOOL\"" )
  set( MAYA_LINK_FLAGS "")
endif()

string(REPLACE " " ";" MAYA_COMPILE_FLAGS_LIST ${MAYA_COMPILE_FLAGS})
string(REPLACE " " ";" MAYA_LINK_FLAGS_LIST ${MAYA_LINK_FLAGS})

foreach(MAYA_LIB
    OpenMaya
    OpenMayaAnim
    OpenMayaFX
    OpenMayaRender
    OpenMayaUI
    Image
    Foundation
    IMFbase
    tbb
    cg
    cgGL)

    find_library(MAYA_${MAYA_LIB}_LIBRARY
            ${MAYA_LIB}
        HINTS
            ${MAYA_LOCATION}
            $ENV{MAYA_LOCATION}
            ${MAYA_BASE_DIR}
        PATH_SUFFIXES
            MacOS/
            lib/
        DOC
            "Maya's ${MAYA_LIB} library path"
    )

    if (MAYA_${MAYA_LIB}_LIBRARY)
      add_library(Maya::${MAYA_LIB} UNKNOWN IMPORTED)
      set_target_properties(
          Maya::${MAYA_LIB} 
	PROPERTIES
	  IMPORTED_LINK_INTERFACE_LANGUAGES CXX
          IMPORTED_LOCATION ${MAYA_${MAYA_LIB}_LIBRARY}
          INTERFACE_INCLUDE_DIRECTORIES ${MAYA_INCLUDE_DIR}
          INTERFACE_COMPILE_OPTIONS "${MAYA_COMPILE_FLAGS_LIST}"
          INTERFACE_LINK_FLAGS "${MAYA_LINK_FLAGS_LIST}"
      )
      list(APPEND MAYA_LIBRARIES Maya::${MAYA_LIB})
    endif()
endforeach(MAYA_LIB)

find_program(MAYA_EXECUTABLE
        maya
        maya2016
        maya2018
    HINTS
        ${MAYA_LOCATION}
        $ENV{MAYA_LOCATION}
        ${MAYA_BASE_DIR}
    PATH_SUFFIXES
        MacOS/
        bin/
    DOC
        "Maya's executable path"
)

if(MAYA_INCLUDE_DIRS AND EXISTS "${MAYA_INCLUDE_DIR}/maya/MTypes.h")

    # Tease the MAYA_API_VERSION numbers from the lib headers
    file(STRINGS ${MAYA_INCLUDE_DIR}/maya/MTypes.h TMP REGEX "#define MAYA_API_VERSION.*$")
    string(REGEX MATCHALL "[0-9]+" MAYA_API_VERSION ${TMP})
endif()


# handle the QUIETLY and REQUIRED arguments and set MAYA_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Maya
    REQUIRED_VARS
        MAYA_EXECUTABLE
        MAYA_INCLUDE_DIRS
        MAYA_LIBRARY_DIR
        MAYA_LIBRARIES
        MAYA_COMPILE_FLAGS
        MAYA_LINK_FLAGS
        MAYA_EXTENSION
    VERSION_VAR
        MAYA_API_VERSION
)
