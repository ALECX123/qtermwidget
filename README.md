[中文文档 | Chinese Version](./README_CN.md)
# QTermWidget5 \(Windows ConPTY Adaptation Edition\)

## Project Overview

Forked from the original QTermWidget 2\.4\.0, this version is **natively Windows\-only**, powered by Windows ConPTY for terminal emulation\.
It is built with **Qt5 \+ CMake**, with a minimum supported Qt version of 5\.12\.11\.

### Core Features

1. **Windows Exclusive Terminal Backend**

    - Implements real cmd/PowerShell terminals via the native `CreatePseudoConsole` ConPTY API\.

2. **Full\-Featured Terminal Emulation**

    - 256\-color \& true\-color rendering with customizable color schemes\.

    - Complete VT escape sequence parsing, with native support for cursors, text selection and auto line wrapping\.

    - Native Chinese \& Unicode rendering; optional utf8proc library for advanced character width calculation\.

3. **Built\-in UI Widgets**

    - Integrated search bar, copy/paste functions, font scaling, and full set of keyboard shortcuts\.

    - Fully customizable keyboard mapping layouts\.

4. **Standard CMake Build System**

    - Complete CMake build scripts requiring CMake 3\.18 or higher\.

    - Produces standard shared libraries with exported CMake config files for downstream integration\.

    - Includes a standalone test terminal binary `test_terminal`\.

5. **Minimal High\-Level API**
A single `QTermWidget` control for drop\-in integration\. Launch terminal processes with one line of code and capture raw terminal output via Qt signals\.

## Dependencies

### Mandatory Global Dependencies

- CMake ≥ 3\.18\.0

- Qt5 Widgets ≥ 5\.12\.11

- Qt5 LinguistTools \(for translation files\)

### Windows System Requirements

Relies on system\-native ConPTY APIs\. Compatible with **Windows 10 1903 and all Windows 11 releases**; no extra low\-level system libraries required\.

### Optional Third\-Party Dependency

- `utf8proc`: Optimizes width calculation for complex Unicode characters\.

## Build \& Compilation Guide

### 1\. Clone Source Code

```bash
git clone https://github.com/xxx/qtermwidget.git
cd qtermwidget
```

### 2\. Create Isolated Build Directory

```bash
mkdir build && cd build
```

### 3\. CMake Configuration \(Windows MSVC Only\)

```bash
cmake .. -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_PREFIX_PATH="Path/to/Qt5.12.11/lib/cmake" ^
    -DBUILD_TEST_TERMINAL=ON
```

#### Optional Compilation Flags

```cmake
-DBUILD_EXAMPLE=ON        # Build official demo programs
-DUSE_UTF8PROC=ON         # Enable utf8proc for Unicode character width optimization
-DUPDATE_TRANSLATIONS=ON  # Regenerate & update TS translation files
```

### 4\. Compile Binaries

```bash
cmake --build . --config RelWithDebInfo
```

### 5\. System Installation

```bash
cmake --install .
# Uninstall command
cmake --build . --target uninstall
```

## Low\-Level Windows Implementation Details

The upstream open\-source QTermWidget is designed exclusively for Unix\-like systems\. This project fully rewrites the underlying backend and retains only a Windows ConPTY adaptation layer:

1. Pure Windows implementation of `KPtyDevice`

    - `initConPTY()` creates a pseudo\-terminal paired with bidirectional anonymous read/write pipes\.

    - Asynchronous terminal output reading using overlapped I/O in background QtConcurrent threads\.

    - Thread\-safe process start/stop logic; all kernel handles are automatically released to eliminate handle/memory leaks\.

2. Unified upper\-layer wrappers: `KPtyProcess` / `Pty`\. Business logic code requires no modification to handle underlying pipe mechanics\.

3. Default supported shells: `cmd.exe` and `powershell.exe`\.

4. Dynamic terminal resizing via the `ResizePseudoConsole` API\.

5. Pipes enable `ENABLE_VIRTUAL_TERMINAL_PROCESSING` to fully parse VT color and cursor control escape sequences\.

## Project Directory Structure

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

## Build Output Artifacts

1. Core library: Windows shared library `qtermwidget5.dll`

2. Executable binary: `test_terminal` \(standalone terminal test utility\)

3. CMake export config: `qtermwidget5-config.cmake` for direct linking in external projects

4. Resource assets: Color schemes, keyboard layouts and translation files installed automatically to system data directories

## License

Inherits the open\-source license of original QTermWidget: **LGPL\-2\.1 / GPL\-2\.0**\. Permits private secondary development and commercial use\.

## Compatibility Notes

- Operating System: Windows only \(Windows 10 1903, Windows 11 and newer\)\. **Linux / macOS are unsupported**\.

- Qt Framework: Qt5 only \(5\.12 \~ 5\.15\)\. The current branch is incompatible with Qt6\.

- CMake: Minimum required version 3\.18\.0\.
