set(ILMBASE_LIB_SUFFIXES "-2_2" "-2_1" "")

find_path(ILMBASE_DIR include/OpenEXR/half.h
  HINTS ${ILMBASE_LOCATION} $ENV{ILMBASE_LOCATION} )

find_path(ILMBASE_INCLUDE_DIR half.h
  HINTS ${ILMBASE_DIR}
  PATH_SUFFIXES include/OpenEXR)

get_filename_component(ILMBASE_INCLUDE_DIR "${ILMBASE_INCLUDE_DIR}/../" ABSOLUTE)

find_library(ILMBASE_Half Half
  HINTS ${ILMBASE_DIR}
  PATH_SUFFIXES lib)

foreach(lib Imath IexMath Iex IlmThread)
  foreach(suff ${ILMBASE_LIB_SUFFIXES})
    find_library(ILMBASE_${lib} ${lib}${suff}
      HINTS ${ILMBASE_DIR}
      PATH_SUFFIXES lib)
  endforeach()
endforeach()

foreach(lib Imath IexMath Half Iex IlmThread)
  list(APPEND ILMBASE_LIBRARIES ${ILMBASE_${lib}})
  add_library(IlmBase::${lib} UNKNOWN IMPORTED)
  set_target_properties(IlmBase::${lib} PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION ${ILMBASE_${lib}}
    INTERFACE_INCLUDE_DIRECTORIES ${ILMBASE_INCLUDE_DIR}
)
endforeach()

set(ILMBASE_INCLUDE_DIRS ${ILMBASE_INCLUDE_DIR})

set(ILMBASE_INCLUDE_DIRS ${ILMBASE_INCLUDE_DIR})

find_package_handle_standard_args(IlmBase
    REQUIRED_VARS
        ILMBASE_DIR
        ILMBASE_INCLUDE_DIRS
        ILMBASE_LIBRARIES
)
set(ILMBASE_LIBRARIES 
  IlmBase::IlmThread 
  IlmBase::IexMath 
  IlmBase::Imath 
  IlmBase::Iex
  IlmBase::Half
)
