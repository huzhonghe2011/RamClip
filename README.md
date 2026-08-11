# RamClip

RamClip 是一个轻量级的 Windows 内存剪贴板工具，可以把多个剪贴板内容暂存在内存槽位中，并通过全局快捷键快速复制、切换、粘贴和删除。

支持文字、图片、文件等常见 Windows 剪贴板格式，并提供简单的悬浮预览界面。

仓库名称：RamClipboard

## 功能

* 多剪贴板槽位
* 支持文字、图片和文件
* 剪贴板内容保存在内存中
* 支持全局快捷键
* 支持直接粘贴指定槽位
* 粘贴后恢复原系统剪贴板
* 支持 Windows DPI 缩放
* Direct2D / DirectWrite 界面
* 支持 Windows x64 与 ARM64
* 单文件 Standalone 版本无需安装额外 C++ 运行环境

## 快捷键

| 快捷键                | 功能              |
| ------------------ | --------------- |
| ``Alt + ` ``        | 打开 / 关闭界面       |
| `Alt + C `          | 将当前选中的内容添加为槽位 1 |
| `Alt + V `          | 粘贴槽位 1          |
| `Alt + Z `          | 删除槽位 1          |
| `Alt + 1 `          | 切换当前槽位          |
| `Alt + 2 `          | 将当前系统剪贴板添加为槽位 1 |
| `Alt + 3 `          | 将当前槽位复制到系统剪贴板   |
| `Alt + 4 `          | 删除当前槽位          |
| `Alt + 5 `          | 粘贴当前槽位          |
| `Ctrl + Alt + 1 `   | 粘贴当前槽位并删除       |
| ``Ctrl + Alt + ` `` | 退出程序            |

## 下载

请在 GitHub 仓库右侧的 **Releases** 页面下载最新版本。

提供以下 Windows 版本：

| 文件                                        | 架构    | 说明       |
| ----------------------------------------- | ----- | -------- |
| `RamClip-v2.3.2-win-x64.exe`              | x64   | 动态运行库版本  |
| `RamClip-v2.3.2-win-x64-standalone.exe`   | x64   | 自包含版本，推荐 |
| `RamClip-v2.3.2-win-arm64.exe`            | ARM64 | 动态运行库版本  |
| `RamClip-v2.3.2-win-arm64-standalone.exe` | ARM64 | 自包含版本，推荐 |

普通用户建议下载带有 `standalone` 的版本。

## 系统要求

* Windows 11
* x64 或 ARM64 处理器

RamClip 使用 Windows 原生 Win32 API、Direct2D 和 DirectWrite。

## 从源码编译

编译器：

* LLVM-MinGW
* C++20

### ARM64

```bat
aarch64-w64-mingw32-clang++.exe -std=c++20 -O2 -s -municode -mwindows RamClip.cpp -o RamClip.exe -ld2d1 -ldwrite -lgdi32 -luser32 -lshell32
```

### ARM64 Standalone

```bat
aarch64-w64-mingw32-clang++.exe -std=c++20 -O2 -s -static -municode -mwindows RamClip.cpp -o RamClip.exe -ld2d1 -ldwrite -lgdi32 -luser32 -lshell32
```

### x64

```bat
x86_64-w64-mingw32-clang++.exe -std=c++20 -O2 -s -municode -mwindows RamClip.cpp -o RamClip.exe -ld2d1 -ldwrite -lgdi32 -luser32 -lshell32
```

### x64 Standalone

```bat
x86_64-w64-mingw32-clang++.exe -std=c++20 -O2 -s -static -municode -mwindows RamClip.cpp -o RamClip.exe -ld2d1 -ldwrite -lgdi32 -luser32 -lshell32
```

## 关于数据

RamClip 的槽位数据保存在程序内存中。

退出 RamClip 后，槽位中的内容不会被持久化保存。

对于较大的图片、文件或其他剪贴板数据，程序会占用相应的内存空间。

## 开发说明

RamClip 的项目构思、功能设计与测试由作者主导，开发过程中使用 AI 协助生成和迭代部分代码。

## License

本项目使用 MIT License。

详见 `LICENSE` 文件。
