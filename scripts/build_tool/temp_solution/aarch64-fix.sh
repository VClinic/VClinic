#!/bin/bash
CUR_DIR=$(cd "$(dirname "$0")";pwd)
cd ../../../dynamorio/
git apply $CUR_DIR/aarch64-fix-reg_get_size.patch
