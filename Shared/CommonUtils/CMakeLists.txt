cmake_minimum_required (VERSION 2.6)

project ( CommonUtils )

file(GLOB Sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB Includes ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library( ${PROJECT_NAME} STATIC ${Sources} )

target_link_libraries( ${PROJECT_NAME}
   ${ALL_ALEMBIC_LIBS}
   )

SET( DocSource_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Doc )
SET( DocInstall_DIR ${Exocortex_INSTALL_BASE_DIR}/Doc )

install(DIRECTORY ${DocSource_DIR} DESTINATION ${DocInstall_DIR} )
