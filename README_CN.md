# rad_boot_hid - nRF54H20 双核自定义引导加载器项目

## 项目概述

本项目展示了 nRF54H20 双核 SoC 的自定义引导加载器实现，将 MRAM 分配最大化给无线核心(CPURAD)，同时最小化应用核心(CPUAPP)的使用。

### 核心特性

- **双核架构**: 充分利用 CPUAPP 和 CPURAD 双核，优化内存分配
- **自定义引导加载器**: `cpurad_boot` - 运行在 CPURAD 核心的 USB 使能引导加载器
- **USB HID 应用**: `hid_mouse` - HID 鼠标演示应用程序
- **优化内存布局**: CPUAPP 仅使用 64KB，1624KB 分配给 CPURAD(引导加载器 + 应用程序)
- **支持 DFU**: 引导加载器中的 USB 支持使得通过 USB 接口进行固件更新成为可能

## 项目结构

```
rad_boot_hid/
├── cpurad_boot/              # CPURAD 核心的自定义引导加载器
│   ├── src/
│   │   ├── main.c            # 引导加载器主逻辑
│   │   ├── arm_cleanup.c     # ARM 外设清理
│   │   └── nrf_cleanup.c     # nRF 外设清理
│   ├── include/
│   │   ├── arm_cleanup.h
│   │   └── nrf_cleanup.h
│   ├── app.overlay           # CPURAD 设备树覆盖文件
│   ├── prj.conf              # 引导加载器配置
│   └── CMakeLists.txt
│
├── hid_mouse/                # CPURAD 的 HID 鼠标应用程序
│   ├── src/
│   │   └── main.c            # HID 鼠标应用逻辑
│   ├── app.overlay           # 应用设备树覆盖文件
│   ├── prj.conf              # 应用配置
│   ├── sysbuild.conf         # Sysbuild 配置
│   └── CMakeLists.txt
│
├── sysbuild/
│   └── empty_app_core/       # CPUAPP 核心的最小化应用
│       ├── src/
│       │   └── main.c        # 空主循环
│       ├── app.overlay       # CPUAPP 设备树覆盖文件
│       └── CMakeLists.txt
│
└── dts_common/
    ├── memlayout.dtsi        # **关键: MRAM 分区定义文件**
    ├── nrf54h20_cpuapp.dtsi  # CPUAPP 设备树配置
    └── nrf54h20_cpurad.dtsi  # CPURAD 设备树配置
```

## 双核架构

### nRF54H20 核心分配

nRF54H20 是一款双核 SoC，核心分配如下:

- **CPUAPP (应用核心 - Cortex-M33)**
  - 运行: `empty_app_core` - 一个最小化占位应用程序
  - 内存: 64KB MRAM (cpuapp_slot0_partition)
  - 用途: 系统兼容性(无需引导加载器)

- **CPURAD (无线核心 - Cortex-M33 带无线功能)**
  - 运行: `cpurad_boot` (引导加载器) → `hid_mouse` (应用程序)
  - 内存: 总共 1624KB MRAM
    - 128KB 用于引导加载器 (cpurad_slot0_partition)
    - 748KB 用于主应用程序 (cpurad_app_partition)
    - 748KB 用于副应用程序 (cpurad_app2_partition)
  - 用途: 主应用程序执行和无线功能

## MRAM 分区布局

### 关键分区文件: `memlayout.dtsi`

`dts_common/memlayout.dtsi` 文件定义了整个 MRAM (2MB) 的分区结构。这是项目中**最关键的配置文件**。

### 分区表

| 分区名称 | 标签 | 起始地址 | 大小 | 用途 |
|---------|------|---------|------|------|
| CPUAPP Slot 0 | `cpuapp_slot0_partition` | 0x30000 | 64KB | CPUAPP 空应用程序 |
| CPURAD Slot 0 | `cpurad_slot0_partition` | 0x40000 | 128KB | **CPURAD 引导加载器 (rad_boot)** |
| CPURAD App | `cpurad_app_partition` | 0x60000 | 748KB | **主 CPURAD 应用程序** |
| CPURAD App2 | `cpurad_app2_partition` | 0x11B000 | 748KB | 副 CPURAD 应用程序 |
| Storage | `storage_partition` | 0x1D0000 | 40KB | 非易失性存储 |
| Peripheral Config | `periphconf_partition` | 0x1DA000 | 8KB | 外设配置 |
| CPUAPP Slot 1 | `cpuapp_slot1_partition` | 0x1DC000 | 4KB | 系统兼容性 |
| Secure Storage | `secure_storage_partition` | 0x1FD000 | 12KB | 加密和ITS存储 |

### 关键分区标签 - 请勿重命名！

⚠️ **警告**: 以下分区标签是**系统定义的，不可更改**:

1. **`cpuapp_slot0_partition`**
   - 用途: CPUAPP 核心的镜像槽
   - 要求方: Nordic SDK 的多镜像引导系统
   - 重命名后果: 构建失败或引导失败

2. **`cpurad_slot0_partition`**
   - 用途: CPURAD 引导加载器镜像槽
   - 要求方: Sysbuild 系统用于引导加载器识别
   - 重命名后果: 构建系统无法识别引导加载器

3. **`cpurad_app_partition`**
   - 用途: 引导加载器跳转到的主应用程序分区
   - 要求方: 引导加载器的 `main.c` - `APP_PARTITION_NODE` 宏
   - 重命名后果: 引导加载器无法定位应用程序，引导失败

这些标签被硬编码在:
- Nordic SDK 的 `soc.c` 初始化代码中
- Sysbuild CMake 脚本中
- 引导加载器应用逻辑中 (`cpurad_boot/src/main.c`)

**如果需要额外的应用程序分区**，请使用不同的名称，如 `cpurad_app2_partition` (已定义)。

## USB 外设管理

### 为什么外设清理至关重要

`cpurad_boot` 引导加载器使能了 USB 支持，以提供 DFU(设备固件更新)功能。当引导加载器跳转到 `hid_mouse` 应用程序时:

1. **问题**: USB 外设保持从引导加载器初始化的状态
2. **结果**: 应用程序可以重用 USB 而无需重新初始化
3. **要求**: 在应用程序接管之前清理外设状态

### 外设清理实现

引导加载器在 `cpurad_boot/src/` 中实现了全面的外设清理:

```c
// arm_cleanup.c - ARM Cortex-M33 外设清理
void arm_cleanup(void)
{
    // 禁用所有中断
    for (int i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
    }
    
    // 清除所有待处理的中断
    for (int i = 0; i < sizeof(NVIC->ICPR) / sizeof(NVIC->ICPR[0]); i++) {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
}

// nrf_cleanup.c - nRF54H20 外设清理
void nrf_cleanup(void)
{
    // 禁用 USB 控制器
    // 禁用 UART, SPI, I2C 等
    // 释放 GPIO 引脚
    // 清除 DMA 通道
}
```

### 支持的外设

在跳转到应用程序之前，引导加载器必须清理:

- ✅ **USB** (USBD/USBHS)
- ✅ **UART** (调试控制台)
- ✅ **GPIO** (按钮, LED)
- ⚠️ **SPI** (如果在引导加载器中使用)
- ⚠️ **I2C** (如果在引导加载器中使用)
- ⚠️ **Timer/RTC** (如果在引导加载器中使用)

**规则**: 在引导加载器中初始化的任何外设**必须**在跳转到应用程序之前正确关闭。

## 构建项目

### 先决条件

- Nordic Connect SDK v3.2.0 或更高版本
- West 构建工具
- NCS toolchain v3.2.0

### 构建命令

```bash
# 进入项目目录
cd ..\rad_boot_hid\hid_mouse

# 构建所有镜像 (sysbuild 将构建所有三个镜像)
west build -b nrf54h20dk/nrf54h20/cpurad

# 烧录到设备
west flash

# 清理构建
west build -t pristine
```

### 构建输出

Sysbuild 生成三个镜像:

1. `empty_app_core` → CPUAPP 核心
2. `cpurad_boot` → CPURAD 引导加载器
3. `hid_mouse` → CPURAD 应用程序

## 引导顺序

1. **系统复位** → nRF54H20 启动两个核心
2. **CPUAPP 核心** → 从 `cpuapp_slot0_partition` (0x30000) 加载 `empty_app_core`
3. **CPURAD 核心** → 从 `cpurad_slot0_partition` (0x40000) 加载 `cpurad_boot`
4. **引导加载器逻辑** → `cpurad_boot` 执行验证和清理
5. **跳转到应用程序** → 引导加载器跳转到 `cpurad_app_partition` (0x60000) 的 `hid_mouse`
6. **HID 鼠标运行** → USB HID 鼠标应用程序启动

## 未来增强

- [ ] 在引导加载器中实现通过 USB 的 DFU
- [ ] 使用 `cpurad_app2_partition` 实现双分区固件更新
- [ ] 带镜像签名的安全引导
- [ ] 回滚保护

## 许可证

SPDX-License-Identifier: Apache-2.0

## 参考资料

- [nRF54H20 产品规格](https://infocenter.nordicsemi.com/topic/struct_nrf54h20/struct/nrf54h20.html)
- [Zephyr RTOS 设备树指南](https://docs.zephyrproject.org/latest/build/dts/index.html)
- [Nordic Connect SDK 文档](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
