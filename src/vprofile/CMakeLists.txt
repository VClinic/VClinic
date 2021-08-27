# **********************************************************
# Copyright (c) 2020-2021 Xuhpclab. All rights reserved.
# Licensed under the MIT License.
# See LICENSE file for more information.
#  **********************************************************

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

add_library(vprofile_static STATIC ${srcs_static})
configure_extension(vprofile_static ON)
use_DynamoRIO_extension(vprofile_static drcontainers)
use_DynamoRIO_extension(vprofile_static drmgr_static)
use_DynamoRIO_extension(vprofile_static drutil_static)
use_DynamoRIO_extension(vprofile_static drsyms_static)
use_DynamoRIO_extension(vprofile_static drwrap_static)
use_DynamoRIO_extension(vprofile_static drreg_static)
use_DynamoRIO_extension(vprofile_static drcctlib_static)

install_ext_header(vprofile.h)