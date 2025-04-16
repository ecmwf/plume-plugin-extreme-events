set(EE_PLUGIN_TEST_FILES_H    
    ../src/notification.h
    ../src/healpix_utils.h
    ../src/ee_plugin.h
    ../src/ee_registry/ee_base.h
    ../src/ee_registry/ee_registry.h
    ../src/ee_registry/extreme_wind.h
    ../src/plugin_types.h
)


# ee_plugin_registration.cc module is excluded in the compilation of the tests
# because eckit plugin deregistration causes a segmentation fault
# see Jira issue ECKIT-520 for more details
set(EE_PLUGIN_TEST_FILES_CC    
    ../src/notification.cc
    ../src/healpix_utils.cc
    ../src/ee_plugin.cc
    ../src/ee_registry/ee_registry.cc
    ../src/ee_registry/extreme_wind.cc
)

set(EE_PLUGIN_TEST_SOURCES
    ${EE_PLUGIN_TEST_FILES_H}
    ${EE_PLUGIN_TEST_FILES_CC}
)

ecbuild_add_test (
    TARGET ee_plugin_test_core
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

ecbuild_add_test(
    TARGET  ee_plugin_run_test
    COMMAND nwp_emulator_run.x
    ENVIRONMENT CLASS="test"
                TYPE="test"
                EXPVER="0001"
                DATE="00000000"
                TIME="0000"
                PLUME_PLUGIN_DEV=1
                DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{DYLD_LIBRARY_PATH}
                LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$ENV{LD_LIBRARY_PATH}
    ARGS --config-src=${CMAKE_CURRENT_SOURCE_DIR}/data/emulator_config.yml
         --plume-cfg=${CMAKE_CURRENT_SOURCE_DIR}/data/plume_config.yml
)