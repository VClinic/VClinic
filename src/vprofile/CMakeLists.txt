cmake_minimum_required(VERSION 2.6)

include(../../make/policies.cmake NO_POLICY_SCOPE)

if (UNIX)
  add_compile_options(-std=c++11)
  if (DEBUG)
    add_compile_options(-g)
  endif (DEBUG)
endif (UNIX)

set(DynamoRIO_USE_LIBC OFF)

set(srcs
  vprofile.cpp
  common/log.cpp
  vtracer/vtracer.cpp
  )
set(srcs_static ${srcs})

include_directories("${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc/include")

# avoid conflict with existing extensions
set(PREFERRED_BASE 0x7A000000)

add_library(vprofile SHARED ${srcs})
configure_extension(vprofile OFF)
use_DynamoRIO_extension(vprofile drcontainers)
use_DynamoRIO_extension(vprofile drmgr)
use_DynamoRIO_extension(vprofile drutil)
use_DynamoRIO_extension(vprofile drsyms)
use_DynamoRIO_extension(vprofile drwrap)
use_DynamoRIO_extension(vprofile drreg)
use_DynamoRIO_extension(vprofile drcctlib)

# VPROFILE version with data address info logged
add_library(vprofile_with_addr SHARED ${srcs})
configure_extension(vprofile_with_addr OFF)
use_DynamoRIO_extension(vprofile_with_addr drcontainers)
use_DynamoRIO_extension(vprofile_with_addr drmgr)
use_DynamoRIO_extension(vprofile_with_addr drutil)
use_DynamoRIO_extension(vprofile_with_addr drsyms)
use_DynamoRIO_extension(vprofile_with_addr drwrap)
use_DynamoRIO_extension(vprofile_with_addr drreg)
use_DynamoRIO_extension(vprofile_with_addr drcctlib_with_addr)

add_library(vprofile_static STATIC ${srcs_static})
configure_extension(vprofile_static ON)
use_DynamoRIO_extension(vprofile_static drcontainers)
use_DynamoRIO_extension(vprofile_static drmgr_static)
use_DynamoRIO_extension(vprofile_static drutil_static)
use_DynamoRIO_extension(vprofile_static drsyms_static)
use_DynamoRIO_extension(vprofile_static drwrap_static)
use_DynamoRIO_extension(vprofile_static drreg_static)
use_DynamoRIO_extension(vprofile_static drcctlib_static)

# DRCCTLib version with data address info logged
add_library(vprofile_with_addr_static STATIC ${srcs_static})
configure_extension(vprofile_with_addr_static ON)
use_DynamoRIO_extension(vprofile_with_addr_static drcontainers)
use_DynamoRIO_extension(vprofile_with_addr_static drmgr_static)
use_DynamoRIO_extension(vprofile_with_addr_static drutil_static)
use_DynamoRIO_extension(vprofile_with_addr_static drsyms_static)
use_DynamoRIO_extension(vprofile_with_addr_static drwrap_static)
use_DynamoRIO_extension(vprofile_with_addr_static drreg_static)
use_DynamoRIO_extension(vprofile_with_addr_static drcctlib_with_addr_static)

install_ext_header(vprofile.h)
install_ext_header(vprofile_filter_func_list.h)
install_ext_header(vtracer/vtracer.h)
install_ext_header(vtracer/vtracer_define.h)
install_ext_header(common/log.h)
install_ext_header(shadow_memory_lock.h)