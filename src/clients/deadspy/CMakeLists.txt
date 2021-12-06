cmake_minimum_required(VERSION 2.6)

include(../../make/policies.cmake NO_POLICY_SCOPE)

if (UNIX)
  add_compile_options(-Wno-deprecated)
  add_compile_options(-Wno-unused-result)
  add_compile_options(-Wno-unused-variable)
  add_compile_options(-Wno-unused-local-typedefs)
  add_compile_options(-Wno-unused-function)
  add_compile_options(-Werror=sign-compare)
  add_compile_options(-Werror=narrowing)
  add_compile_options(-std=c++11)

  add_compile_options(-march=native)
  add_compile_options(-O3)

  if (DEBUG)
    add_compile_options(-g3)
  endif (DEBUG)
endif (UNIX)

# add third-party libraries

add_library(deadspy SHARED
deadspy.cpp
  )

configure_DynamoRIO_client(deadspy)
use_DynamoRIO_extension(deadspy drcctlib)
use_DynamoRIO_extension(deadspy droption)
use_DynamoRIO_extension(deadspy vprofile) 
# use_DynamoRIO_extension(deadspy drx) 
place_shared_lib_in_lib_dir(deadspy)

add_dependencies(deadspy api_headers)

# Provide a hint for how to use the client
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET deadspy
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drconfig or drrun: -t deadspy"
    VERBATIM)
endif ()

install_target(deadspy ${INSTALL_CLIENTS_LIB})

set(INSTALL_CCTTEST_CONFIG ${INSTALL_CLIENTS_BASE})

function (write_config_file dst bindir libdir)
  file(WRITE  ${dst} "# deadspy tool config file\n")
  file(APPEND ${dst} "CLIENT_REL=${libdir}/${LIB_PFX}deadspy${LIB_EXT}\n")
endfunction ()

if (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/deadspy.drrun64)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/deadspy.drrun64)
else (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/deadspy.drrun32)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/deadspy.drrun32)
endif (X64)

set(BUILD_CLIENTS_BIN clients/${INSTALL_BIN})
set(BUILD_CLIENTS_LIB clients/${INSTALL_LIB})

write_config_file(${CONFIG_INSTALL} ${INSTALL_CLIENTS_BIN} ${INSTALL_CLIENTS_LIB})
write_config_file(${CONFIG_BUILD} ${BUILD_CLIENTS_BIN} ${BUILD_CLIENTS_LIB})

DR_install(FILES "${CONFIG_INSTALL}" DESTINATION ${INSTALL_CCTTEST_CONFIG})
register_tool_file("deadspy")

##################################################
# Documentation

