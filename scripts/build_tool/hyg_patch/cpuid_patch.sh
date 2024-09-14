#!/bin/bash
CUR_DIR=$1
pushd $CUR_DIR/hyg_patch

OLD_FILE="proc.c"
TARGET_FILE="../../../dynamorio/core/arch/x86/proc.c"
PATCH_FILE="hyg.patch"

diff "$TARGET_FILE" "$OLD_FILE" > /dev/null
if [ $? -ne 0 ]; then
    patch --dry-run $TARGET_FILE < "$PATCH_FILE" > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "INFO: patch already applied. Skip patching."
    else
        echo "ERROR: inconsistent file detected for patching."
        exit 1
    fi
else
    # 文件一致，应用补丁
    patch $TARGET_FILE < "$PATCH_FILE"
    if [ $? -eq 0 ]; then
        echo "patch applied"
    else
        echo "ERROR: patch failed"
        exit 1
    fi
fi

popd
