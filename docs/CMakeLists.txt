execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile )

add_custom_target(
  ee_plugin_doxygen
  COMMAND doxygen Doxyfile
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating Doxygen documentation" VERBATIM
)

set_property(TARGET ee_plugin_doxygen PROPERTY EXCLUDE_FROM_ALL TRUE)