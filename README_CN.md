# QTermWidget5 \(Windows ConPTY 适配版\)

## 项目简介

基于原 QTermWidget 2\.4\.0，**仅原生支持 Windows 平台**，采用 Windows ConPTY 实现终端仿真；
基于 **Qt5 \+ CMake** 构建，最低 Qt 5\.12\.11。

### 核心特性

1. **Windows 专属终端支持**

    - 使用 `CreatePseudoConsole` ConPTY 实现真实 cmd/PowerShell 终端；

2. **完整终端仿真**

    - 256 色 / 真彩色、自定义配色方案；

    - 完整 VT 转义序列解析、光标、选区、自动换行；

    - 中文 / Unicode 渲染，可选 utf8proc 优化字符宽度；

3. **UI 内置组件**

    - 内置搜索栏、复制 / 粘贴、字体缩放、全套键盘快捷键；

    - 可自定义键盘映射布局；

4. **标准 CMake 构建体系**

    - CMake 3\.18\+ 完整构建脚本；

    - 生成标准共享库、CMake config 导出；

    - 内置独立测试终端程序 `test_terminal`；

5. **极简上层 API**
单控件 `QTermWidget` 开箱即用，一键启动终端程序，通过信号获取原始终端输出。

## 环境依赖

### 通用必备依赖

- CMake ≥ 3\.18\.0

- Qt5 Widgets ≥ 5\.12\.11

- Qt5 LinguistTools（翻译）

### Windows 系统要求

系统内置 ConPTY API，仅支持 **Win10 1903 版本及以上、所有 Win11 版本**，无需额外系统底层库

### 可选第三方依赖

- `utf8proc`：增强复杂 Unicode 字符宽度计算

## 编译构建指南

### 1\. 获取源码

```bash
git clone https://github.com/xxx/qtermwidget.git
cd qtermwidget
```

### 2\. 创建独立构建目录

```bash
mkdir build && cd build
```

### 3\. CMake 配置（仅 Windows MSVC）

```bash
cmake .. -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_PREFIX_PATH="Qt5.12.11 安装路径/lib/cmake" ^
    -DBUILD_TEST_TERMINAL=ON
```

#### 可选编译开关

```cmake
-DBUILD_EXAMPLE=ON        # 编译官方示例程序
-DUSE_UTF8PROC=ON         # 启用 utf8proc 字符宽度优化
-DUPDATE_TRANSLATIONS=ON  # 更新 ts 翻译文件
```

### 4\. 编译

```bash
cmake --build . --config RelWithDebInfo
```

### 5\. 系统安装

```bash
cmake --install .
# 卸载
cmake --build . --target uninstall
```

## Windows 平台底层实现说明

原版开源 QTermWidget 为 Unix 专用，本项目完全重构底层，仅保留 Windows ConPTY 适配层：

1. `KPtyDevice` 纯 Windows 实现

    - `initConPTY()` 创建伪终端 \+ 双向匿名读写管道；

    - QtConcurrent 后台线程使用重叠 IO 异步读取终端输出；

    - 线程安全启停，所有内核句柄自动释放，无内存 / 句柄泄漏；

2. `KPtyProcess` / `Pty` 上层封装统一，业务代码无需关心底层管道逻辑；

3. 默认终端程序：`cmd.exe` / `powershell.exe`；

4. 支持动态调整终端行列尺寸 `ResizePseudoConsole`；

5. 管道输出开启 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`，完整解析 VT 颜色、光标控制转义序列。

## 项目目录结构

```Plain
qtermwidget/
├── CMakeLists.txt          
├── README.md               
├── cmake/                  
├── lib/                    
│   ├── kptydevice.h/cpp    
│   ├── kptyprocess.h/cpp
│   ├── Pty.h/cpp           
│   ├── qtermwidget.h/cpp   
│   ├── Vt102Emulation.cpp  
│   ├── SearchBar.ui/h/cpp  
│   ├── color-schemes/      
│   └── kb-layouts/         
├── test/
│   └── main.cpp            
├── examples/cpp/           
└── translations/           
```

## 编译输出产物

1. 核心库：Windows `qtermwidget5.dll`

2. 可执行程序：`test_terminal` 测试终端工具

3. CMake 导出配置：`qtermwidget5-config.cmake`，第三方项目可直接链接

4. 资源文件：配色、键盘布局、翻译文件自动安装至系统数据目录

## 许可证

继承原 QTermWidget 开源协议：**LGPL\-2\.1 / GPL\-2\.0**，允许私有二次开发、商用。

## 兼容性说明

- 操作系统：仅 Windows（Win10 1903、Win11 及以上），**不支持 Linux /macOS**；

- Qt 版本：仅支持 Qt5（5\.12 \~ 5\.15），当前分支不兼容 Qt6；

- CMake：最低 3\.18\.0。