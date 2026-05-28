# Trash

`trash` 是一个命令行删除替代工具，用于把文件或目录移动到系统回收站，而不是直接永久删除。

当前实现支持：

- Windows：移动到 Windows 回收站。
- WSL：仅支持 `/mnt/<drive>/...` 下的 Windows 盘符路径，并移动到 Windows 回收站。
- macOS：保留原始 macOS 实现。

## 安装

使用 xmake 一键安装当前版本。

Windows：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1
```

macOS / WSL：

```sh
sh ./scripts/install.sh /usr/local
```

也可以手动构建：

```sh
xmake f -c
xmake
xmake install -o /usr/local
```

Windows 构建只推荐使用 xmake。`Makefile` 保留给 macOS 原始实现和 manpage 生成，避免和 Windows/MSVC 工具链混用。

## 使用

移动文件或目录到回收站：

```sh
trash file.txt dir/
```

处理以 `-` 开头的文件名时，使用 `--` 分隔选项和路径：

```sh
trash -- -file.txt
```

常用参数：

- `-v`：输出已移动的路径；配合 `-l` 时显示统计信息。
- `-l`：列出回收站内容。
- `-e` / `-s`：清空回收站，默认会弹出确认。
- `-y`：配合 `-e` / `-s` 跳过确认。
- `-F`：兼容原始 macOS 参数；Windows 和 WSL 下无额外效果。

WSL 版本只处理 `/mnt/<drive>/...` 路径，例如 `/mnt/c/temp/a.txt`。`/home/...` 这类 Linux 文件系统路径不会发送到 Windows 回收站，程序会直接拒绝，避免 Windows 对 UNC / WSL 路径处理不一致。

## 项目结构

```text
src/windows/      Windows 回收站实现
src/wsl/          WSL 到 Windows 回收站的桥接实现
src/macos/        macOS 原始实现
include/macos/    macOS 头文件和 Finder ScriptingBridge 定义
scripts/          安装脚本
docs/             manpage 源文件和备份文档
```

结构调整后提交时需要把移动后的新路径一起纳入索引，建议使用：

```sh
git add -A
```

这样 Git 会把根目录旧文件删除和 `src/`、`include/`、`scripts/`、`docs/` 下的新文件作为一次结构移动处理。

## 验证

Windows：

```powershell
xmake f -c
xmake -vD
```

WSL：

```sh
sh ./scripts/check-wsl-build.sh
```

WSL 脚本会优先使用 `xmake` 完整验证 Linux 目标；如果 WSL 环境没有安装 `xmake`，会退回到 `g++` 编译 `src/wsl/trash_wsl.cpp`，用于确认 WSL 源文件本身没有编译错误。



## The MIT License

Copyright (c) Ali Rantakari

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
