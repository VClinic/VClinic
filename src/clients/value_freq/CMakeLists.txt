# cmake_minimum_required(VERSION 2.6)

# include(../../make/policies.cmake NO_POLICY_SCOPE)

# if (UNIX)
#   add_compile_options(-Wno-deprecated)
#   add_compile_options(-Wno-unused-result)
#   add_compile_options(-Wno-unused-variable)
#   add_compile_options(-Wno-unused-local-typedefs)
#   add_compile_options(-Wno-unused-function)
#   add_compile_options(-Werror=sign-compare)
#   add_compile_options(-Werror=narrowing)
#   add_compile_options(-std=c++11)

#   if (DEBUG)
#     add_compile_options(-g3)
#   endif (DEBUG)
# endif (UNIX)

# # add third-party libraries

# add_library(value_freq SHARED
# value_freq.cpp
#   )

# include_directories(../../vprofile)

# configure_DynamoRIO_client(value_freq)
# # use_DynamoRIO_extension(value_freq drsym)
# use_DynamoRIO_extension(value_freq vprofile)
# # use_DynamoRIO_extension(value_freq droption)
# place_shared_lib_in_lib_dir(value_freq)

# add_dependencies(value_freq api_headers)

# # Provide a hint for how to use the client
# if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
#   add_custom_command(TARGET value_freq
#     POST_BUILD
#     COMMAND ${CMAKE_COMMAND}
#     ARGS -E echo "Usage: pass to drconfig or drrun: -t value_freq"
#     VERBATIM)
# endif ()

# install_target(value_freq ${INSTALL_CLIENTS_LIB})

# set(INSTALL_CCTTEST_CONFIG ${INSTALL_CLIENTS_BASE})

# function (write_config_file dst bindir libdir)
#   file(WRITE  ${dst} "# value_freq tool config file\n")
#   file(APPEND ${dst} "CLIENT_REL=${libdir}/${LIB_PFX}value_freq${LIB_EXT}\n")
# endfunction ()

# if (X64)
#   set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/value_freq.drrun64)
#   set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/value_freq.drrun64)
# else (X64)
#   set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/value_freq.drrun32)
#   set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/value_freq.drrun32)
# endif (X64)

# set(BUILD_CLIENTS_BIN clients/${INSTALL_BIN})
# set(BUILD_CLIENTS_LIB clients/${INSTALL_LIB})

# write_config_file(${CONFIG_INSTALL} ${INSTALL_CLIENTS_BIN} ${INSTALL_CLIENTS_LIB})
# write_config_file(${CONFIG_BUILD} ${BUILD_CLIENTS_BIN} ${BUILD_CLIENTS_LIB})

# DR_install(FILES "${CONFIG_INSTALL}" DESTINATION ${INSTALL_CCTTEST_CONFIG})
# register_tool_file("value_freq")

# ##################################################
# # Documentation

