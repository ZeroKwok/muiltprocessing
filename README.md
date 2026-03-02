# muiltprocessing

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Conan](https://img.shields.io/badge/conan-1.x-green.svg)](https://conan.io/)

一个基于 Boost::Process 和 ZeroMQ 的高性能 C++ 多进程并发框架。设计极简，仅需单个头文件即可使用。

## ✨ 特性

- 🚀 **轻量级**：只有一个hpp文件，即插即用
- ⚡ **高性能**：基于ZeroMQ的高效进程间通信
- 🛠 **易集成**：最小依赖，易于集成到现有项目
- 📦 **现代化**：支持C++17标准，使用CMake构建
- 🔧 **跨平台**：支持Linux、macOS和Windows

## 📋 依赖

- Boost.Process (>= 1.69)
- ZeroMQ (>= 4.3)
- 支持C++17的编译器

## 🔧 快速开始

### 编译安装

```bash
git clone https://github.com/ZeroKwok/muiltprocessing.git
cd muiltprocessing
conan install . --build=missing
cmake --preset="conan-default" -B build -S .  
cmake --build build --config Release
```

### 使用示例

- 管理者/主进程 [example\master.cpp](example\master.cpp)
- 工作者/子进程 [example\worker.py](example\worker.py)
