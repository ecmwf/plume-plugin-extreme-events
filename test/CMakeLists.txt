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
    LIBS
        eckit
        plume_plugin
)