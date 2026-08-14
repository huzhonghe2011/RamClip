# RamClip

RamClip 是一个轻量级的 Windows **内存剪贴板工具**。

它可以把多个剪贴板内容暂存在内存槽位中，并通过全局快捷键快速添加、切换、复制、粘贴和删除。

支持文字、图片、文件等常见 Windows 剪贴板内容，并提供简单的悬浮预览界面。

> 仓库名称：RamClipboard
> 程序名称：RamClip

## 下载与使用

请前往仓库的 **Releases** 页面下载最新版本。

### 普通用户：推荐 Portable ZIP

大多数用户应该下载 portable ZIP，而不是单独的 EXE。

| 文件                               | 架构    | 推荐用途                             |
| -------------------------------- | ----- | -------------------------------- |
| `RamClip-win-x64-portable.zip`   | x64   | **普通 Intel / AMD Windows 电脑，推荐** |
| `RamClip-win-arm64-portable.zip` | ARM64 | **Windows ARM64 设备，推荐**          |
| `RamClip-win-x64.exe`            | x64   | 已有对应运行环境的高级用户 / 测试               |
| `RamClip-win-arm64.exe`          | ARM64 | 已有对应运行环境的高级用户 / 测试               |

不知道自己的电脑是哪种架构时，绝大多数 Intel / AMD Windows 电脑请选择 **x64**。

### Portable ZIP 怎么使用？

1. 下载对应架构的 portable ZIP。
2. 将 ZIP **完整解压**到任意文件夹。
3. 不要单独移动其中的 `RamClip.exe`，请保留随程序提供的 DLL 文件。
4. 双击 `RamClip.exe`。

不需要安装。

Portable ZIP 已经包含 RamClip 所需的 LLVM-MinGW 运行库文件，普通用户无需另外安装 LLVM-MinGW、Visual Studio 或其他 C++ 开发环境。

> 如果 Releases 中同时存在 `.exe` 和 `-portable.zip`，普通用户请选择 **`-portable.zip`**。

首次运行时，Windows SmartScreen 可能提示未知发布者。当前发布文件没有使用商业代码签名证书，请确认文件来自本项目官方 Releases 后再决定是否运行。

## 主要功能

* 多剪贴板内存槽位
* 支持文字、图片和文件
* 支持剪贴板内容预览
* 全局快捷键操作
* 支持直接粘贴指定槽位
* 支持连续触发全局粘贴
* 粘贴后恢复原系统剪贴板
* 支持高 DPI / 多显示器环境
* 使用 Windows 原生 Win32 API
* Direct2D / DirectWrite 界面
* 支持 Windows x64 与 ARM64
* 无需安装，portable ZIP 解压即可使用

## 快捷键

| 快捷键                 | 功能                 |
| ------------------- | ------------------ |
| ``Alt + ` ``        | 打开 / 关闭 RamClip 界面 |
| `Alt + C`           | 将当前选中的内容添加为槽位 1    |
| `Alt + V`           | 全局粘贴槽位 1           |
| `Alt + Z`           | 删除槽位 1             |
| `Alt + 1`           | 切换当前槽位             |
| `Alt + 2`           | 将当前系统剪贴板添加为槽位 1    |
| `Alt + 3`           | 将当前槽位复制到系统剪贴板      |
| `Alt + 4`           | 删除当前槽位             |
| `Alt + 5`           | 全局粘贴当前槽位           |
| `Ctrl + Alt + 1`    | 全局粘贴当前槽位并删除        |
| ``Ctrl + Alt + ` `` | 退出 RamClip         |

`Alt + V`、`Alt + 5` 和 `Ctrl + Alt + 1` 可以直接作为全局粘贴快捷键使用，并支持连续触发。

如果某个快捷键已经被其他程序注册，RamClip 可能无法注册该快捷键。

## RamClip 如何处理系统剪贴板？

RamClip 的槽位内容保存在程序内存中。

使用粘贴功能时，RamClip 会：

1. 备份当前 Windows 系统剪贴板。
2. 临时将指定槽位写入系统剪贴板。
3. 向当前目标窗口发送粘贴操作。
4. 在粘贴完成后恢复之前的系统剪贴板。

连续粘贴时，RamClip 会尽量复用原始剪贴板备份。

如果恢复前检测到系统剪贴板已经被其他程序或用户修改，RamClip 会避免用旧备份覆盖较新的剪贴板内容。

## 关于数据

RamClip **不会持久化保存槽位内容**。

退出程序后：

* 文字槽位不会保存
* 图片槽位不会保存
* 文件列表槽位不会保存

所有槽位数据随程序退出而释放。

对于较大的图片、文件列表或其他剪贴板数据，RamClip 会占用相应的内存空间。

因此 RamClip 更适合作为一个**临时、多槽位的内存剪贴板**，而不是长期剪贴板历史数据库。

## 系统要求

* Windows 11
* x64 或 ARM64 处理器

RamClip 使用 Windows 原生 Win32 API、Direct2D 和 DirectWrite。

## Portable 与单独 EXE 的区别

### Portable ZIP

适合普通用户。

例如：

```text
RamClip-win-x64-portable.zip
```

解压后通常包含：

```text
RamClip.exe
所需的 LLVM-MinGW 运行库 DLL
```

这些文件需要放在一起使用。

### 单独 EXE

例如：

```text
RamClip-win-x64.exe
```

这是依赖外部运行环境的构建。

它主要提供给已经拥有对应 LLVM-MinGW / C++ 运行环境的开发者、高级用户或测试场景。

**普通用户不要因为 EXE 看起来更简单就优先下载它。**

请使用 portable ZIP。

## 文件校验

Release 同时提供：

```text
SHA256SUMS.txt
```

其中包含发布文件对应的 SHA-256，可用于验证下载文件是否完整。

## 从源码编译

RamClip 使用：

* C++20
* LLVM-MinGW
* PowerShell 构建脚本

确保 LLVM-MinGW 编译器已经加入 `PATH`。

### 编译全部架构

```powershell
.\build.ps1 -Arch all
```

### 仅编译 x64

```powershell
.\build.ps1 -Arch x64
```

### 仅编译 ARM64

```powershell
.\build.ps1 -Arch arm64
```

构建输出位于：

```text
dist/
```

构建脚本会生成对应架构的普通 EXE、portable ZIP 和 `SHA256SUMS.txt`。

GitHub Actions 也会自动构建 Windows x64 与 ARM64 产物。

## 技术实现

RamClip 主要使用：

* Win32 API
* Windows Clipboard API
* Direct2D
* DirectWrite
* GDI
* C++20

程序主体保持为原生 Windows C++ 实现，不依赖 Electron、Qt 等大型 GUI 框架。

## 开发说明

RamClip 的项目构思、功能设计与测试由作者主导，开发过程中使用 AI 协助生成和迭代部分代码。

## License

本项目使用 MIT License。

详见 `LICENSE`。
