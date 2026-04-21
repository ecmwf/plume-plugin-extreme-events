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
set(EE_PLUGIN_TEST_FILES_H
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/notification.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/healpix_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/ee_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/ee_registry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/extreme_wind.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/storm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/extreme_wave.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/wind_drought.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/ramp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugin_types.h
)


# ee_plugin_registration.cc module is excluded in the compilation of the tests
# because eckit plugin deregistration causes a segmentation fault
# see Jira issue ECKIT-520 for more details
set(EE_PLUGIN_TEST_FILES_CC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/notification.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/healpix_utils.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_plugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/ee_registry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/extreme_wind.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/storm.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/extreme_wave.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/wind_drought.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ee_registry/ramp.cc
)

set(EE_PLUGIN_TEST_SOURCES
    ${EE_PLUGIN_TEST_FILES_H}
    ${EE_PLUGIN_TEST_FILES_CC}
)

foreach(prec sp dp)
    if (HAVE_EE_PLUGIN_${prec})
        ecbuild_add_test (
            TARGET ee_plugin_test_core_${prec}
            SOURCES
                ${EE_PLUGIN_TEST_SOURCES}
                test_ee_plugin.cc
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/../src
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
            LIBS
                eckit
                plume_plugin
        )

        ecbuild_add_test (
            TARGET ee_plugin_test_healpix_${prec}
            SOURCES
                ../src/healpix_utils.h
                ../src/healpix_utils.cc
                test_healpix_utils.cc
            INCLUDES
                ${CMAKE_CURRENT_SOURCE_DIR}/../src
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
            LIBS
                eckit
                atlas
        )
    endif()
endforeach()

add_subdirectory(events)

foreach(prec sp dp)
    if (HAVE_EE_PLUGIN_${prec} AND HAVE_NWP_EMULATOR_TESTS)
        set(EE_PLUGIN_RUN_TEST_SCRIPT_${prec}
            ${CMAKE_BINARY_DIR}/bin/check_emulator_notifications_${prec}.sh
        )
        ecbuild_configure_file(check_emulator_notifications.sh.in
                               ${EE_PLUGIN_RUN_TEST_SCRIPT_${prec}} @ONLY)
        ecbuild_add_test(
            TYPE SCRIPT
            TARGET  ee_plugin_run_test_${prec}
            COMMAND ${EE_PLUGIN_RUN_TEST_SCRIPT_${prec}}
            ENVIRONMENT PLUME_PLUGIN_DEV=1
                        NWP_EMULATOR_RUN_BIN=$<TARGET_FILE:nwp_emulator_run_${prec}.x>
                        DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{DYLD_LIBRARY_PATH}
                        LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}
            ARGS ${CMAKE_CURRENT_SOURCE_DIR}/data/emulator_config.yml
                ${CMAKE_CURRENT_SOURCE_DIR}/data/plume_config_${prec}.yml
                "No wind"
                "Extremely strong wind"
                "Storm ("
                "Storm conditions"
                "Unsafe offshore wind-farm operations"
                "Wind drought"
                "Ramp"
        )
    endif()
endforeach()