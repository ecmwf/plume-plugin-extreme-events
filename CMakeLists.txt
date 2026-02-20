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

cmake_minimum_required( VERSION 3.16 FATAL_ERROR )

find_package( ecbuild 3.5 REQUIRED HINTS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ecbuild)

project( plume_plugin_extreme_events LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

ecbuild_find_package( NAME eckit  VERSION  1.28.5   REQUIRED )
ecbuild_find_package( NAME atlas  VERSION  0.41 REQUIRED )
ecbuild_find_package( NAME plume  VERSION  0.5.0  REQUIRED )

# The plugin can be built in Single Precision (SP) and/or Double Precision (DP)
ecbuild_add_option( FEATURE SINGLE_PRECISION
                    DESCRIPTION "Compile plugin in single precision"
                    DEFAULT OFF )

ecbuild_add_option( FEATURE PLUME_PLUGINS_SINGLE_PRECISION
                    DESCRIPTION
                    "Compile plugin for single precision fields"
                    DEFAULT OFF)

ecbuild_add_option( FEATURE PLUME_PLUGINS_DOUBLE_PRECISION
                    DESCRIPTION
                    "Compile plugin for double precision fields"
                    DEFAULT ON)

set( HAVE_EE_PLUGIN_sp 0)
set( HAVE_EE_PLUGIN_dp 0)

if (HAVE_SINGLE_PRECISION OR HAVE_PLUME_PLUGINS_SINGLE_PRECISION)
  set( PLUGINS_DEFINITIONS_sp WITH_EE_PLUGIN_SINGLE_PRECISION )
  set( HAVE_EE_PLUGIN_sp 1)
endif()

if (HAVE_PLUME_PLUGINS_DOUBLE_PRECISION)
  set( PLUGINS_DEFINITIONS_dp WITH_EE_PLUGIN_DOUBLE_PRECISION )
  set( HAVE_EE_PLUGIN_dp 1)
endif()

# Tests using the Plume NWP Emulator are optional
ecbuild_add_option( FEATURE NWP_EMULATOR_TESTS
                    DESCRIPTION "Add plugin tests using the Plume NWP emulator"
                    DEFAULT OFF )

## Plugins
add_subdirectory(src)

## Scripts
add_subdirectory(scripts)

## Documentation
add_subdirectory(docs)

## Test
add_subdirectory(tests)

# finalize
ecbuild_install_project( NAME ${PROJECT_NAME} )
ecbuild_print_summary()

