#!/bin/bash
# 将当前目录下所有文件夹隐藏（重命名为 .文件夹名）

for dir in */; do
    # 去掉末尾的斜杠
    dirname="${dir%/}"
    # 检查是否已隐藏（以.开头）
    if [[ "$dirname" != .* ]]; then
        mv "$dirname" ".${dirname}"
        echo "已隐藏: $dirname -> .${dirname}"
    fi
done

echo "✅ 所有文件夹已隐藏。"
