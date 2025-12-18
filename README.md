# rad_boot_hid - nRF54H20 Dual-Core Custom Bootloader Project

## Overview

This project demonstrates a custom bootloader implementation for the nRF54H20 dual-core SoC, maximizing MRAM allocation to the radio core (CPURAD) while minimizing application core (CPUAPP) usage.

### Key Features

- **Dual-Core Architecture**: Leverages both CPUAPP and CPURAD cores with optimized memory distribution
- **Custom Bootloader**: `cpurad_boot` - A USB-enabled bootloader running on CPURAD core
- **USB HID Application**: `hid_mouse` - HID mouse demonstration application
- **Optimized Memory Layout**: CPUAPP uses only 64KB, with 1624KB allocated to CPURAD (bootloader + applications)
- **DFU Ready**: USB support in bootloader enables firmware updates via USB interface

## Project Structure

```
rad_boot_hid/
├── cpurad_boot/              # Custom bootloader for CPURAD core
│   ├── src/
│   │   ├── main.c            # Bootloader main logic
│   │   ├── arm_cleanup.c     # ARM peripheral cleanup
│   │   └── nrf_cleanup.c     # nRF peripheral cleanup
│   ├── include/
│   │   ├── arm_cleanup.h
│   │   └── nrf_cleanup.h
│   ├── app.overlay           # CPURAD device tree overlay
│   ├── prj.conf              # Bootloader configuration
│   └── CMakeLists.txt
│
├── hid_mouse/                # HID mouse application for CPURAD
│   ├── src/
│   │   └── main.c            # HID mouse application logic
│   ├── app.overlay           # Application device tree overlay
│   ├── prj.conf              # Application configuration
│   ├── sysbuild.conf         # Sysbuild configuration
│   └── CMakeLists.txt
│
├── sysbuild/
│   └── empty_app_core/       # Minimal application for CPUAPP core
│       ├── src/
│       │   └── main.c        # Empty main loop
│       ├── app.overlay       # CPUAPP device tree overlay
│       └── CMakeLists.txt
│
└── dts_common/
    ├── memlayout.dtsi        # **CRITICAL: MRAM partition definitions**
    ├── nrf54h20_cpuapp.dtsi  # CPUAPP device tree configuration
    └── nrf54h20_cpurad.dtsi  # CPURAD device tree configuration
```

## Dual-Core Architecture

### nRF54H20 Core Assignment

The nRF54H20 is a dual-core SoC with the following core assignments:

- **CPUAPP (Application Core - Cortex-M33)**
  - Runs: `empty_app_core` - A minimal placeholder application
  - Memory: 64KB MRAM (cpuapp_slot0_partition)
  - Purpose: System compatibility (no bootloader required)

- **CPURAD (Radio Core - Cortex-M33 with radio)**
  - Runs: `cpurad_boot` (bootloader) → `hid_mouse` (application)
  - Memory: 1624KB MRAM total
    - 128KB for bootloader (cpurad_slot0_partition)
    - 748KB for primary application (cpurad_app_partition)
    - 748KB for secondary application (cpurad_app2_partition)
  - Purpose: Main application execution with radio capabilities

## MRAM Partition Layout

### Critical Partition File: `memlayout.dtsi`

The `dts_common/memlayout.dtsi` file defines the entire MRAM (2MB) partition structure. This is the **most critical configuration file** in the project.

### Partition Table

| Partition Name | Label | Start Address | Size | Purpose |
|---------------|-------|---------------|------|---------|
| CPUAPP Slot 0 | `cpuapp_slot0_partition` | 0x30000 | 64KB | CPUAPP empty application |
| CPURAD Slot 0 | `cpurad_slot0_partition` | 0x40000 | 128KB | **CPURAD bootloader (rad_boot)** |
| CPURAD App | `cpurad_app_partition` | 0x60000 | 748KB | **Primary CPURAD application** |
| CPURAD App2 | `cpurad_app2_partition` | 0x11B000 | 748KB | Secondary CPURAD application |
| Storage | `storage_partition` | 0x1D0000 | 40KB | Non-volatile storage |
| Peripheral Config | `periphconf_partition` | 0x1DA000 | 8KB | Peripheral configuration |
| CPUAPP Slot 1 | `cpuapp_slot1_partition` | 0x1DC000 | 4KB | System compatibility |
| Secure Storage | `secure_storage_partition` | 0x1FD000 | 12KB | Crypto & ITS storage |

### Critical Partition Labels - DO NOT RENAME!

⚠️ **WARNING**: The following partition labels are **system-defined and immutable**:

1. **`cpuapp_slot0_partition`**
   - Purpose: CPUAPP core's image slot
   - Required by: Nordic SDK's multi-image boot system
   - Consequence of renaming: Build failure or boot failure

2. **`cpurad_slot0_partition`**
   - Purpose: CPURAD bootloader image slot
   - Required by: Sysbuild system for bootloader identification
   - Consequence of renaming: Bootloader will not be recognized by build system

3. **`cpurad_app_partition`**
   - Purpose: Primary application partition that bootloader jumps to
   - Required by: Bootloader's `main.c` - `APP_PARTITION_NODE` macro
   - Consequence of renaming: Bootloader cannot locate application, boot failure

These labels are hardcoded in:
- Nordic SDK's `soc.c` initialization code
- Sysbuild CMake scripts
- Bootloader application logic (`cpurad_boot/src/main.c`)

**If you need additional application partitions**, use different names like `cpurad_app2_partition` (already defined).

## USB Peripheral Management

### Why Peripheral Cleanup is Critical

The `cpurad_boot` bootloader enables USB support to provide DFU (Device Firmware Update) capabilities. When the bootloader jumps to the `hid_mouse` application:

1. **Problem**: USB peripheral remains initialized from bootloader
2. **Result**: Application can reuse USB without reinitialization
3. **Requirement**: Clean peripheral state before application takeover

### Peripheral Cleanup Implementation

The bootloader implements comprehensive peripheral cleanup in `cpurad_boot/src/`:

```c
// arm_cleanup.c - ARM Cortex-M33 peripheral cleanup
void arm_cleanup(void)
{
    // Disable all interrupts
    for (int i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
    }
    
    // Clear all pending interrupts
    for (int i = 0; i < sizeof(NVIC->ICPR) / sizeof(NVIC->ICPR[0]); i++) {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
}

// nrf_cleanup.c - nRF54H20 peripheral cleanup
void nrf_cleanup(void)
{
    // Disable USB controller
    // Disable UART, SPI, I2C, etc.
    // Release GPIO pins
    // Clear DMA channels
}
```

### Supported Peripherals

Before jumping to the application, the bootloader must clean up:

- ✅ **USB** (USBD/USBHS)
- ✅ **UART** (Debug console)
- ✅ **GPIO** (Buttons, LEDs)
- ⚠️ **SPI** (If used in bootloader)
- ⚠️ **I2C** (If used in bootloader)
- ⚠️ **Timer/RTC** (If used in bootloader)

**Rule**: Any peripheral initialized in the bootloader **must** be properly shut down before application jump.

## Building the Project

### Prerequisites

- Nordic Connect SDK v3.1.0 or later
- West build tool
- ARM GCC toolchain

### Build Commands

```bash
# Navigate to project directory
cd D:\workspace\NCS\example\v3.1.0\rad_boot_hid\hid_mouse

# Build all images (sysbuild will build all three images)
west build -b nrf54h20dk/nrf54h20/cpurad

# Flash to device
west flash

# Clean build
west build -t pristine
```

### Build Output

Sysbuild generates three images:

1. `empty_app_core` → CPUAPP core
2. `cpurad_boot` → CPURAD bootloader
3. `hid_mouse` → CPURAD application

## Boot Sequence

1. **System Reset** → nRF54H20 starts both cores
2. **CPUAPP Core** → Loads `empty_app_core` from `cpuapp_slot0_partition` (0x30000)
3. **CPURAD Core** → Loads `cpurad_boot` from `cpurad_slot0_partition` (0x40000)
4. **Bootloader Logic** → `cpurad_boot` performs validation and cleanup
5. **Application Jump** → Bootloader jumps to `hid_mouse` at `cpurad_app_partition` (0x60000)
6. **HID Mouse Running** → USB HID mouse application starts

## Future Enhancements

- [ ] DFU over USB implementation in bootloader
- [ ] Dual-bank firmware update using `cpurad_app2_partition`
- [ ] Secure boot with image signing
- [ ] Rollback protection

## License

SPDX-License-Identifier: Apache-2.0

## References

- [nRF54H20 Product Specification](https://infocenter.nordicsemi.com/topic/struct_nrf54h20/struct/nrf54h20.html)
- [Zephyr RTOS Device Tree Guide](https://docs.zephyrproject.org/latest/build/dts/index.html)
- [Nordic Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
