
# Send a notification to aviso
ecbuild_add_executable( TARGET aviso_notification_test.x
  SOURCES
    notify_aviso.cc
  LIBS
    extreme_event_plugin
)

## Run emulator
ecbuild_add_executable( TARGET emulate_ee_detection.x
  SOURCES
    emulate_ee_detection.cc
  LIBS
    nwp_ee_emulator
)
ecbuild_configure_file(emulate_ee_detection.sh.in
                       ${CMAKE_BINARY_DIR}/bin/emulate_ee_detection.sh
)