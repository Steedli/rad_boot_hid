# nRF54H20 无线核心自定义引导程序与HID鼠标应用

## 概述

本项目展示了nRF54H20双核SoC的自定义引导程序实现，利用CPURAD（无线核心）同时运行自定义引导程序（`cpurad_boot`）和USB HID鼠标应用（`hid_mouse`）。CPUAPP（应用核心）运行一个最小的空应用程序，无需引导程序。

## 架构

### 双核配置

nRF54H20具有两个核心：
- **CPUAPP（应用核心）**：运行`empty_app_core` - 一个最小的占位应用程序，无引导程序
- **CPURAD（无线核心）**：运行自定义引导程序（`cpurad_boot`）和应用程序（`hid_mouse`）

### 项目组成

```
rad_boot_hid/
├── dts_common/
│   ├── memlayout.dtsi           # MRAM分区定义
│   ├── nrf54h20_cpuapp.dtsi     # CPUAPP设备树配置
│   └── nrf54h20_cpurad.dtsi     # CPURAD设备树配置
├── cpurad_boot/                 # CPURAD自定义引导程序
│   ├── src/
│   │   ├── main.c               # 引导程序主逻辑
│   │   ├── arm_cleanup.c        # ARM外设清理
│   │   └── nrf_cleanup.c        # Nordic外设清理
│   └── app.overlay              # 引导程序设备树覆盖
├── hid_mouse/                   # HID鼠标应用程序
│   ├── src/
│   │   └── main.c               # 应用程序主逻辑
│   ├── app.overlay              # 应用程序设备树覆盖
│   └── sysbuild/
│       └── empty_app_core/      # CPUAPP空应用程序
└── README_CN.md                 # 本文件
```

## MRAM分区布局

`memlayout.dtsi`文件定义了nRF54H20的完整MRAM（磁阻式随机存取存储器）分区结构。这是**关键配置文件**，用于在两个核心之间分配内存资源。

### 完整内存映射

| 分区名称 | 地址 | 大小 | 核心 | 用途 |
|---------|------|------|------|------|
| `cpuapp_slot0_partition` | 0x30000 | 64KB | CPUAPP | 空应用程序 |
| `cpuapp_slot1_partition` | 0x1DC000 | 4KB | CPUAPP | 系统兼容性占位符 |
| `cpurad_slot0_partition` | 0x40000 | 128KB | CPURAD | **自定义引导程序** |
| `cpurad_app_partition` | 0x60000 | 748KB | CPURAD | **主应用程序** |
| `cpurad_app2_partition` | 0x11B000 | 748KB | CPURAD | **辅助应用程序** |
| `storage_partition` | 0x1D0000 | 40KB | 共享 | 存储区域 |
| `periphconf_partition` | 0x1DA000 | 8KB | 系统 | 外设配置 |
| `secure_storage_partition` | 0x1FD000 | 12KB | 共享 | 安全密钥和ITS |

### 关键分区说明

#### ⚠️ 重要：请勿重命名这些标签

以下分区标签是**系统定义的**，**绝对不能重命名**。更改这些标签将导致系统初始化失败：

##### 1. `cpuapp_slot0_partition`（CPUAPP核心）
- **用途**：CPUAPP的主代码分区
- **大小**：64KB @ 0x30000
- **内容**：空应用程序核心（`empty_app_core`）
- **系统要求**：Zephyr RTOS和Nordic SDK期望使用此标签来定位CPUAPP应用程序代码
- **警告**：重命名此分区将导致CPUAPP核心无法启动

##### 2. `cpurad_slot0_partition`（CPURAD核心 - 引导程序）
- **用途**：CPURAD的自定义引导程序分区
- **大小**：128KB @ 0x40000
- **内容**：带USB支持的自定义引导程序（`cpurad_boot`）
- **系统要求**：系统启动序列使用此标签来识别CPURAD引导程序位置
- **警告**：重命名将破坏CPURAD启动链

##### 3. `cpurad_app_partition`（CPURAD核心 - 应用程序）
- **用途**：CPURAD的主应用程序分区
- **大小**：748KB @ 0x60000
- **内容**：HID鼠标应用程序或用户固件
- **引导程序引用**：引导程序在初始化后跳转到此分区
- **代码引用**：`cpurad_boot/src/main.c`引用此标签：
  ```c
  #define APP_PARTITION_NODE  DT_NODELABEL(cpurad_app_partition)
  ```
- **警告**：重命名需要更新引导程序源代码，可能破坏DFU功能

##### 4. `cpurad_app2_partition`（CPURAD核心 - 辅助）
- **用途**：辅助应用程序分区（未来DFU支持）
- **大小**：748KB @ 0x11B000
- **内容**：预留用于固件更新或辅助应用程序
- **备注**：目前未使用，但为将来的双bank固件更新预留

##### 5. `cpuapp_slot1_partition`（CPUAPP核心）
- **用途**：系统兼容性占位符
- **大小**：4KB @ 0x1DC000
- **系统要求**：Nordic的SoC初始化代码需要此分区来检测多镜像配置
- **警告**：删除此分区可能导致`soc.c`中的编译错误

### 内存分配总结

- **CPUAPP总计**：68KB（64KB + 4KB占位符）
- **CPURAD总计**：1624KB（128KB引导程序 + 748KB应用 + 748KB辅助应用）
- **共享/系统**：60KB（存储 + 外设 + 安全）

## 引导程序中的USB外设管理

### 启用USB的引导程序设计

`cpurad_boot`引导程序启用了USB功能，这为通过USB实现设备固件更新（DFU）提供了基础。此设计允许：

1. **通过USB传输固件**：在引导程序模式下可以通过USB接收新固件
2. **HID通信**：引导程序可以与主机工具通信进行固件更新
3. **无缝过渡**：跳转到应用程序后USB功能继续工作

### 关键：应用程序跳转前的外设清理

**⚠️ 重要**：在引导程序将控制权转移给应用程序之前，**所有外设必须正确关闭并释放其中断**。这对于应用程序成功重新初始化这些外设至关重要。

#### 必需的清理步骤

引导程序在以下文件中实现全面的外设清理：

```c
// 文件：cpurad_boot/src/arm_cleanup.c
void arm_core_cleanup(void)
{
    // 禁用所有中断
    __set_PRIMASK(1);
    
    // 清除待处理中断
    for (int i = 0; i < 8; i++) {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    
    // 禁用并清除SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    // 清理缓存（如果存在）
    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();
}
```

```c
// 文件：cpurad_boot/src/nrf_cleanup.c
void nrf_cleanup(void)
{
    // 禁用USB
    if (usbd_initialized) {
        usbd_shutdown();
    }
    
    // 其他外设清理
    // - 重置GPIO配置
    // - 禁用定时器
    // - 清除DMA通道
    // - 等等
}
```

#### 外设清理检查清单

跳转到应用程序之前，引导程序**必须**清理：

| 外设 | 清理操作 | 原因 |
|-----|---------|------|
| **USB** | `usbd_shutdown()`，禁用USB控制器 | 防止中断冲突，允许应用程序重新初始化USB |
| **UART** | 禁用TX/RX，清除FIFO | 释放引脚和中断 |
| **SPI** | 禁用SPI，释放CS引脚 | 防止总线冲突 |
| **I2C** | 发送STOP条件，禁用I2C | 为应用程序释放总线 |
| **定时器** | 停止所有定时器，清除比较值 | 防止意外中断 |
| **DMA** | 中止所有通道，清除配置 | 释放DMA通道 |
| **GPIO** | 重置为默认配置 | 清理引脚状态 |
| **中断** | 清除NVIC待处理位，禁用所有中断 | 防止引导程序ISR在应用程序上下文中执行 |

### 示例：USB清理序列

```c
void jump_to_application(uint32_t app_addr)
{
    printk("准备跳转到应用程序...\n");
    
    // 步骤1：禁用Zephyr系统组件
    sys_clock_disable();
    
    // 步骤2：Nordic外设清理
    nrf_cleanup();  // 禁用USB、GPIO等
    
    // 步骤3：ARM核心清理
    arm_core_cleanup();  // 清除中断、缓存
    
    // 步骤4：加载应用程序向量表
    arm_vector_table_t *app_vector = (arm_vector_table_t *)app_addr;
    
    // 步骤5：更新堆栈指针并跳转
    __set_MSP(app_vector->msp);
    app_vector->reset_vector();
    
    // 永远不应返回
    while (1);
}
```

### 为什么这很重要

**没有适当清理**：
- 应用程序可能从引导程序外设接收虚假中断
- USB重新初始化可能因硬件状态冲突而失败
- DMA传输可能损坏内存
- 引脚配置可能导致电气冲突

**有适当清理**：
- 应用程序以干净的硬件状态启动
- 过渡后USB无缝工作
- 所有外设都可以可靠地重新初始化
- 可以通过USB进行DFU

## 构建项目

### 先决条件

- nRF Connect SDK v3.1.0或更高版本
- Zephyr SDK
- West构建工具

### 构建命令

```bash
# 导航到项目目录
cd D:\workspace\NCS\example\v3.1.0\rad_boot_hid\hid_mouse

# 使用sysbuild构建（构建所有组件）
west build -b nrf54h20dk/nrf54h20/cpurad --sysbuild

# 烧录到设备
west flash
```

### 构建输出

构建过程生成三个镜像：
1. `empty_app_core` - CPUAPP最小应用程序
2. `cpurad_boot` - CPURAD自定义引导程序（带USB）
3. `hid_mouse` - CPURAD HID鼠标应用程序

## 设备树配置

### CPUAPP配置（`nrf54h20_cpuapp.dtsi`）

主要特性：
- 禁用未使用的外设（USB、UART、GPIO等）
- 删除VPR（向量处理器）节点（cpuflpr、cpuppr）
- 空应用程序的最小配置

### CPURAD配置（`nrf54h20_cpurad.dtsi`）

主要特性：
- 启用USB HS（高速）控制器
- 配置按钮和LED的GPIO
- 启用UART进行调试输出
- HID设备配置

## 引导程序操作

### 启动序列

1. **上电**：nRF54H20启动，两个核心初始化
2. **CPUAPP**：加载并运行`empty_app_core`（保持空闲）
3. **CPURAD**：加载并运行`cpurad_boot`引导程序
4. **USB初始化**：引导程序启用USB以实现潜在的DFU
5. **外设清理**：引导程序关闭所有外设
6. **应用程序跳转**：引导程序将控制权转移到`cpurad_app_partition`
7. **应用程序启动**：HID鼠标应用程序运行，USB可以重新初始化

### 跳转机制

```c
// 引导程序跳转到应用程序
#define APP_PARTITION_NODE  DT_NODELABEL(cpurad_app_partition)
#define TARGET_IMAGE_ADDRESS (DT_REG_ADDR(MRAM_NODE) + DT_REG_ADDR(APP_PARTITION_NODE))

static void jump_to_image(uint32_t image_addr)
{
    // 加载向量表
    arm_vector_table_t *app_vector = (arm_vector_table_t *)image_addr;
    
    // 清理外设（关键！）
    nrf_cleanup();
    arm_core_cleanup();
    
    // 更新MSP并跳转到复位向量
    __set_MSP(app_vector->msp);
    app_vector->reset_vector();
}
```

## 未来增强

### 通过USB的DFU
在引导程序中启用USB并实现适当的清理后：
- 在引导程序中实现USB DFU类
- 通过USB接收固件镜像
- 写入`cpurad_app2_partition`
- 交换分区并重启

### 双bank固件更新
- 使用`cpurad_app_partition`和`cpurad_app2_partition`作为A/B bank
- 实现bank交换逻辑
- 支持更新失败时的回滚

## 重要说明

### 分区标签命名约定

⚠️ **关键**：以下分区标签在Nordic SDK和Zephyr RTOS中是**硬编码的**：
- `cpuapp_slot0_partition` - CPUAPP启动代码所期望
- `cpuapp_slot1_partition` - 多镜像检测所需
- `cpurad_slot0_partition` - CPURAD启动代码所期望

**永远不要重命名这些标签**，除非您同时修改：
1. Nordic SDK SoC初始化代码（`zephyr/soc/nordic/nrf54h/soc.c`）
2. 引导程序源代码（`cpurad_boot/src/main.c`）
3. Zephyr设备树绑定期望

### 内存对齐

由于MRAM硬件要求，所有分区必须对齐到4KB边界。`memlayout.dtsi`中的分区地址和大小已经遵循此要求。

### 引导程序大小

引导程序分区（`cpurad_slot0_partition`）设置为128KB，这对于带USB支持的引导程序来说是足够的。如果添加大量功能，请监视构建输出以确保它适合此限制。

## 故障排除

### 编译错误

**错误**：`undefined node label 'cpurad_app_partition'`
- **解决方案**：验证`memlayout.dtsi`正确定义了此分区
- **检查**：确保标签与引导程序源代码中的完全匹配

**错误**：`region FLASH overflowed`
- **解决方案**：减少引导程序功能或增加分区大小
- **操作**：在`memlayout.dtsi`中调整大小，同时保持对齐

### 运行时问题

**跳转后USB不工作**
- **原因**：引导程序中外设未正确清理
- **解决方案**：检查并增强`nrf_cleanup()`函数

**应用程序无法启动**
- **原因**：跳转地址不正确或向量表损坏
- **解决方案**：验证应用程序已编程到`cpurad_app_partition`地址

## 许可证

版权所有 (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

## 参考资料

- [nRF54H20产品规格](https://www.nordicsemi.com/Products/nRF54H20)
- [nRF Connect SDK文档](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
- [Zephyr设备树指南](https://docs.zephyrproject.org/latest/build/dts/index.html)
