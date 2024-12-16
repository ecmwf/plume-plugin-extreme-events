# TODO: separate the NWP emulator in Plume and use its own var for precision
ecbuild_add_library(
  TARGET nwp_ee_emulator
  INSTALL_HEADERS LISTED
  SOURCES
    nwp_utils.h
    nwp_emulator.h
    nwp_utils.cc
    nwp_emulator.cc
  PUBLIC_INCLUDES
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/nwp_emulator>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/nwp_emulator>
  DEFINITIONS
    ${PLUGINS_DEFINITIONS} 
  PUBLIC_LIBS
    extreme_event_plugin
)