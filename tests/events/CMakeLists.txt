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
foreach(prec sp dp)
    if (HAVE_EE_PLUGIN_${prec})
        ecbuild_add_test (
            TARGET ee_plugin_test_storm_${prec}
            SOURCES
                ${EE_PLUGIN_TEST_SOURCES}
                test_storm.cc
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/../../src
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
            LIBS
                eckit
                plume_plugin
        )
        ecbuild_add_test (
            TARGET ee_plugin_test_extreme_wind_${prec}
            SOURCES
                ${EE_PLUGIN_TEST_SOURCES}
                test_extreme_wind.cc
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/../../src
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
            LIBS
                eckit
                plume_plugin
        )
        ecbuild_add_test (
            TARGET ee_plugin_test_extreme_wave_${prec}
            SOURCES
                ${EE_PLUGIN_TEST_SOURCES}
                test_extreme_wave.cc
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/../../src
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
            LIBS
                eckit
                plume_plugin
        )
        ecbuild_add_test (
            TARGET ee_plugin_test_wind_drought_${prec}
            SOURCES
                ${EE_PLUGIN_TEST_SOURCES}
                test_wind_drought.cc
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/../../src
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
            LIBS
                eckit
                plume_plugin
        )
        ecbuild_add_test (
            TARGET ee_plugin_test_ramp_${prec}
            SOURCES
                ${EE_PLUGIN_TEST_SOURCES}
                test_ramp.cc
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/../../src
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
            LIBS
                eckit
                plume_plugin
        )
    endif()
endforeach()
