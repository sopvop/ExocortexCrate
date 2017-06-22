
project ( MayaExocortexAlembic )

set ( COMPILE_NAME MayaExocortexAlembic )

add_definitions( -Dcrate_Maya_Version=${Maya_VERSION} )

include_directories( "." )

set(Sources
  AlembicArchiveStorage.cpp
  AlembicCamera.cpp
  alembic.cpp
  AlembicCurves.cpp
  AlembicFileNode.cpp
  AlembicHair.cpp
  AlembicImport.cpp
  AlembicLicensing.cpp
  AlembicObject.cpp
  AlembicPoints.cpp
  AlembicPolyMesh.cpp
  AlembicSubD.cpp
  AlembicTimeControl.cpp
  AlembicValidateNameCmd.cpp
  AlembicWriteJob.cpp
  AlembicXform.cpp
  AttributesReading.cpp
  AttributesWriter.cpp
  MayaUtility.cpp
  metadata.cpp
  sceneGraph.cpp
  stdafx.cpp
  utility.cpp
)

file(GLOB_RECURSE Scripts ${BASE_DIR}/MEL/*.*)
file(GLOB_RECURSE UI ${BASE_DIR}/UI/*.*)

SOURCE_GROUP("Source Files" FILES ${Sources})
SOURCE_GROUP("Header Files" FILES ${Includes})
SOURCE_GROUP("_MEL" FILES ${Scripts})
SOURCE_GROUP("_UI" FILES ${UI})


if( WIN32 )
    add_definitions( -DNOMINMAX )        # disable min/max macros from <windows.h>.
endif()

include_directories( ${MAYA_INCLUDE_DIRS} )

add_library( ${PROJECT_NAME} MODULE ${Sources} )


if (WIN32)
   add_definitions( -DNT_PLUGIN )
   add_definitions( -DREQUIRE_IOSTREAM )
   add_definitions( -D_BOOL )

   set( MAYA_EXTENSION ".mll" )
   set( MAYA_LINK_FLAGS "${MAYA_LINK_FLAGS} /export:initializePlugin /export:uninitializePlugin " )
elseif ( UNIX )
   set( MAYA_EXTENSION ".so" )
   set( MAYA_LINK_FLAGS "${MAYA_LINK_FLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/2016/MayaAlembic.versionScript")
endif()
set_target_properties(
    ${PROJECT_NAME}
  PROPERTIES #
    OUTPUT_NAME ${COMPILE_NAME}
    PREFIX ""  # There is no prefix even on UNIXes
    LINK_FLAGS ${MAYA_LINK_FLAGS}
    SUFFIX ${MAYA_EXTENSION}
)

target_link_libraries(
  ${PROJECT_NAME}

  CommonUtils
  ${MAYA_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${ALL_ALEMBIC_LIBS}
)

SET( Install_DIR ${Exocortex_INSTALL_BASE_DIR}/Maya${Maya_VERSION} )

install(
  TARGETS
    ${PROJECT_NAME}
  DESTINATION
    ${Install_DIR}/plug-ins
  COMPONENT
    maya_plugin

)

install(
  DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/MEL
  DESTINATION
    ${Install_DIR}/scripts
  COMPONENT
    maya_plugin
)

install(
  DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/UI
  DESTINATION
    ${Install_DIR}/ui
  COMPONENT
    maya_plugin
)

