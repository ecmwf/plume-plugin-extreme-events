# 
#  (C) Copyright 2025- ECMWF.
# 
#  This software is licensed under the terms of the Apache Licence Version 2.0
#  which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
# 
#  In applying this licence, ECMWF does not waive the privileges and immunities
#  granted to it by virtue of its status as an intergovernmental organisation nor
#  does it submit to any jurisdiction.
# 

# ################# config headers ####################
ecbuild_generate_config_headers( DESTINATION ${INSTALL_INCLUDE_DIR}/plume-plugin-extreme-events )

configure_file( version.h.in   version.h   )
configure_file( git_sha1.h.in  git_sha1.h  )

install(FILES
			${CMAKE_CURRENT_BINARY_DIR}/version.h
			${CMAKE_CURRENT_BINARY_DIR}/git_sha1.h
		DESTINATION
			${INSTALL_INCLUDE_DIR}/plume-plugin-extreme-events
)

# #################### EE plugin ######################
set(EE_PLUGIN_FILES_H
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    ${CMAKE_CURRENT_BINARY_DIR}/git_sha1.h    
    notification.h
    healpix_utils.h
    ee_emulator_layer_writer.h
    ee_plugin.h
    ee_registry/ee_base.h
    ee_registry/ee_registry.h
    ee_registry/extreme_wind.h
    ee_registry/storm.h
    ee_registry/extreme_wave.h
    ee_registry/wind_drought.h
    ee_registry/ramp.h
    plugin_types.h
)

set(EE_PLUGIN_FILES_CC    
    notification.cc
    healpix_utils.cc
    ee_emulator_layer_writer.cc
    ee_plugin.cc
    ee_plugin_registration.cc
    ee_registry/ee_registry.cc
    ee_registry/extreme_wind.cc
    ee_registry/storm.cc
    ee_registry/extreme_wave.cc
    ee_registry/wind_drought.cc
    ee_registry/ramp.cc
)

set(EE_PLUGIN_SOURCES
    ${EE_PLUGIN_FILES_H}
    ${EE_PLUGIN_FILES_CC}
)
foreach(prec sp dp)
    if (HAVE_EE_PLUGIN_${prec})
        set(EE_PLUGIN_LIB_NAME "extreme_event_plugin_${prec}")
        set(EE_PLUGIN_DEFINITIONS ${PLUGINS_DEFINITIONS_${prec}})
        if (HAVE_EMULATOR_LAYER_OUTPUT)
            list(APPEND EE_PLUGIN_DEFINITIONS EE_PLUGIN_EMULATOR_LAYER_OUTPUT)
        endif()
        message(STATUS "Building plugin ${EE_PLUGIN_LIB_NAME} and definitions: ${EE_PLUGIN_DEFINITIONS}")

        ecbuild_add_library(
            TARGET ${EE_PLUGIN_LIB_NAME}
            INSTALL_HEADERS LISTED
            HEADER_DESTINATION
                ${INSTALL_INCLUDE_DIR}/plume-plugin-extreme-events-${prec}
            SOURCES
                ${EE_PLUGIN_SOURCES}
            PUBLIC_INCLUDES
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
                $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
            PRIVATE_INCLUDES
                "${MPI_INCLUDE_DIRS}"
            DEFINITIONS
                ${EE_PLUGIN_DEFINITIONS}
            PUBLIC_LIBS
                atlas
                eckit
                plume
                plume_plugin
        )
    endif()
endforeach()