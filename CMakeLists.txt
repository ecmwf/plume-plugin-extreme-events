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
ecbuild_find_package( NAME plume  VERSION  0.0.1  REQUIRED )

ecbuild_add_option(FEATURE EE_PLUGIN_SINGLE_PRECISION DESCRIPTION "Single precision Atlas fields" DEFAULT OFF)

if( HAVE_EE_PLUGIN_SINGLE_PRECISION ) 
  list(APPEND PLUGINS_DEFINITIONS WITH_EE_PLUGIN_SINGLE_PRECISION )
endif()

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

