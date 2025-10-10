#!/bin/bash

# 遍历当前目录下所有以点开头的目录
for dir in .*/; do
    # 检查是否是目录
    if [ -d "$dir" ]; then
        # 获取去掉前面点的目录名
        new_dir_name="${dir:1}"
        # 重命名目录
        mv "$dir" "$new_dir_name"
    fi
done

echo "隐藏目录已取消隐藏并重命名。"
