# 
#  (C) Copyright 2023- ECMWF.
# 
#  This software is licensed under the terms of the Apache Licence Version 2.0
#  which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
# 
#  In applying this licence, ECMWF does not waive the privileges and immunities
#  granted to it by virtue of its status as an intergovernmental organisation nor
#  does it submit to any jurisdiction.
# 

# #################### EE plugin ######################
set(EE_PLUGIN_FILES_H    
    notification.h
    healpix_utils.h
    ee_plugin.h
    ee_registry/ee_base.h
    ee_registry/ee_registry.h
    ee_registry/extreme_wind.h
    plugin_types.h
)

set(EE_PLUGIN_FILES_CC    
    notification.cc
    healpix_utils.cc
    ee_plugin.cc
    ee_plugin_registration.cc
    ee_registry/ee_registry.cc
    ee_registry/extreme_wind.cc
)

set(EE_PLUGIN_SOURCES
    ${EE_PLUGIN_FILES_H}
    ${EE_PLUGIN_FILES_CC}
)

ecbuild_add_library(
    TARGET extreme_event_plugin
    INSTALL_HEADERS LISTED
    HEADER_DESTINATION
        ${INSTALL_INCLUDE_DIR}/plume-plugin-extreme-events
    SOURCES
        ${EE_PLUGIN_SOURCES}
    PUBLIC_INCLUDES
       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
       $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    PRIVATE_INCLUDES
        "${MPI_INCLUDE_DIRS}"
    DEFINITIONS
        ${PLUGINS_DEFINITIONS}
    PUBLIC_LIBS
        atlas
        eckit
        plume
        plume_plugin
)