# nRF54H20 Radio Core Custom Bootloader with HID Mouse Application

## Overview

This project demonstrates a custom bootloader implementation for the nRF54H20 dual-core SoC, utilizing the CPURAD (Radio Core) to run both a custom bootloader (`cpurad_boot`) and a USB HID Mouse application (`hid_mouse`). The CPUAPP (Application Core) runs a minimal empty application without requiring a bootloader.

## Architecture

### Dual-Core Configuration

The nRF54H20 features two cores:
- **CPUAPP (Application Core)**: Runs `empty_app_core` - a minimal placeholder application with no bootloader
- **CPURAD (Radio Core)**: Runs the custom bootloader (`cpurad_boot`) and application (`hid_mouse`)

### Project Components

```
rad_boot_hid/
├── dts_common/
│   ├── memlayout.dtsi           # MRAM partition definitions
│   ├── nrf54h20_cpuapp.dtsi     # CPUAPP device tree configuration
│   └── nrf54h20_cpurad.dtsi     # CPURAD device tree configuration
├── cpurad_boot/                 # Custom bootloader for CPURAD
│   ├── src/
│   │   ├── main.c               # Bootloader main logic
│   │   ├── arm_cleanup.c        # ARM peripheral cleanup
│   │   └── nrf_cleanup.c        # Nordic peripheral cleanup
│   └── app.overlay              # Bootloader device tree overlay
├── hid_mouse/                   # HID Mouse application
│   ├── src/
│   │   └── main.c               # Application main logic
│   ├── app.overlay              # Application device tree overlay
│   └── sysbuild/
│       └── empty_app_core/      # Empty CPUAPP application
└── README.md                    # This file
```

## MRAM Partition Layout

The `memlayout.dtsi` file defines the complete MRAM (Magnetoresistive RAM) partition structure for the nRF54H20. This is the **critical configuration file** that allocates memory resources between both cores.

### Complete Memory Map

| Partition Name | Address | Size | Core | Purpose |
|---------------|---------|------|------|---------|
| `cpuapp_slot0_partition` | 0x30000 | 64KB | CPUAPP | Empty application |
| `cpuapp_slot1_partition` | 0x1DC000 | 4KB | CPUAPP | System compatibility placeholder |
| `cpurad_slot0_partition` | 0x40000 | 128KB | CPURAD | **Custom Bootloader** |
| `cpurad_app_partition` | 0x60000 | 748KB | CPURAD | **Primary Application** |
| `cpurad_app2_partition` | 0x11B000 | 748KB | CPURAD | **Secondary Application** |
| `storage_partition` | 0x1D0000 | 40KB | Shared | Storage area |
| `periphconf_partition` | 0x1DA000 | 8KB | System | Peripheral configuration |
| `secure_storage_partition` | 0x1FD000 | 12KB | Shared | Security keys and ITS |

### Key Partition Descriptions

#### ⚠️ CRITICAL: Do NOT Rename These Labels

The following partition labels are **system-defined** and **must NOT be renamed**. Changing these labels will cause system initialization failures:

##### 1. `cpuapp_slot0_partition` (CPUAPP Core)
- **Purpose**: Primary code partition for CPUAPP
- **Size**: 64KB @ 0x30000
- **Contents**: Empty application core (`empty_app_core`)
- **System Requirement**: The Zephyr RTOS and Nordic SDK expect this label to locate the CPUAPP application code
- **Warning**: Renaming this partition will prevent the CPUAPP core from booting

##### 2. `cpurad_slot0_partition` (CPURAD Core - Bootloader)
- **Purpose**: Custom bootloader partition for CPURAD
- **Size**: 128KB @ 0x40000
- **Contents**: Custom bootloader with USB support (`cpurad_boot`)
- **System Requirement**: The system boot sequence uses this label to identify the CPURAD bootloader location
- **Warning**: Renaming will break the CPURAD boot chain

##### 3. `cpurad_app_partition` (CPURAD Core - Application)
- **Purpose**: Primary application partition for CPURAD
- **Size**: 748KB @ 0x60000
- **Contents**: HID Mouse application or user firmware
- **Bootloader Reference**: The bootloader jumps to this partition after initialization
- **Code Reference**: `cpurad_boot/src/main.c` references this label:
  ```c
  #define APP_PARTITION_NODE  DT_NODELABEL(cpurad_app_partition)
  ```
- **Warning**: Renaming requires updating bootloader source code and may break DFU functionality

##### 4. `cpurad_app2_partition` (CPURAD Core - Secondary)
- **Purpose**: Secondary application partition (future DFU support)
- **Size**: 748KB @ 0x11B000
- **Contents**: Reserved for firmware update or secondary application
- **Note**: Currently unused but allocated for future dual-bank firmware updates

##### 5. `cpuapp_slot1_partition` (CPUAPP Core)
- **Purpose**: System compatibility placeholder
- **Size**: 4KB @ 0x1DC000
- **System Requirement**: Required by Nordic's SoC initialization code to detect multi-image configurations
- **Warning**: Removing this partition may cause compilation errors in `soc.c`

### Memory Allocation Summary

- **CPUAPP Total**: 68KB (64KB + 4KB placeholder)
- **CPURAD Total**: 1624KB (128KB bootloader + 748KB app + 748KB app2)
- **Shared/System**: 60KB (storage + peripherals + security)

## USB Peripheral Management in Bootloader

### USB-Enabled Bootloader Design

The `cpurad_boot` bootloader has USB functionality enabled, which provides the foundation for implementing Device Firmware Update (DFU) over USB. This design allows:

1. **Firmware Transfer via USB**: New firmware can be received through USB while in bootloader mode
2. **HID Communication**: The bootloader can communicate with host tools for firmware updates
3. **Seamless Transition**: USB functionality continues to work after jumping to the application

### Critical: Peripheral Cleanup Before Application Jump

**⚠️ IMPORTANT**: Before the bootloader transfers control to the application, **all peripherals must be properly shut down and their interrupts released**. This is essential for the application to successfully reinitialize these peripherals.

#### Required Cleanup Steps

The bootloader implements comprehensive peripheral cleanup in the following files:

```c
// File: cpurad_boot/src/arm_cleanup.c
void arm_core_cleanup(void)
{
    // Disable all interrupts
    __set_PRIMASK(1);
    
    // Clear pending interrupts
    for (int i = 0; i < 8; i++) {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    
    // Disable and clear SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    // Clean caches if present
    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();
}
```

```c
// File: cpurad_boot/src/nrf_cleanup.c
void nrf_cleanup(void)
{
    // Disable USB
    if (usbd_initialized) {
        usbd_shutdown();
    }
    
    // Additional peripheral cleanup
    // - Reset GPIO configurations
    // - Disable timers
    // - Clear DMA channels
    // - etc.
}
```

#### Peripheral Cleanup Checklist

Before jumping to the application, the bootloader **MUST** clean up:

| Peripheral | Cleanup Actions | Reason |
|-----------|----------------|--------|
| **USB** | `usbd_shutdown()`, disable USB controller | Prevent interrupt conflicts, allow app to reinitialize USB |
| **UART** | Disable TX/RX, clear FIFOs | Release pins and interrupts |
| **SPI** | Disable SPI, release CS pins | Prevent bus conflicts |
| **I2C** | Send STOP condition, disable I2C | Release bus for app |
| **Timers** | Stop all timers, clear compare values | Prevent unexpected interrupts |
| **DMA** | Abort all channels, clear configurations | Free DMA channels |
| **GPIO** | Reset to default configuration | Clean pin states |
| **Interrupts** | Clear NVIC pending bits, disable all | Prevent bootloader ISRs in app context |

### Example: USB Cleanup Sequence

```c
void jump_to_application(uint32_t app_addr)
{
    printk("Preparing to jump to application...\n");
    
    // Step 1: Disable Zephyr system components
    sys_clock_disable();
    
    // Step 2: Nordic peripheral cleanup
    nrf_cleanup();  // Disables USB, GPIO, etc.
    
    // Step 3: ARM core cleanup
    arm_core_cleanup();  // Clears interrupts, caches
    
    // Step 4: Load application vector table
    arm_vector_table_t *app_vector = (arm_vector_table_t *)app_addr;
    
    // Step 5: Update stack pointer and jump
    __set_MSP(app_vector->msp);
    app_vector->reset_vector();
    
    // Should never return
    while (1);
}
```

### Why This Matters

**Without proper cleanup**:
- The application may receive spurious interrupts from bootloader peripherals
- USB reinitialization may fail due to hardware state conflicts
- DMA transfers may corrupt memory
- Pin configurations may cause electrical conflicts

**With proper cleanup**:
- The application starts with a clean hardware state
- USB works seamlessly after transition
- All peripherals can be reinitialized reliably
- DFU over USB is possible

## Building the Project

### Prerequisites

- nRF Connect SDK v3.1.0 or later
- Zephyr SDK
- West build tool

### Build Commands

```bash
# Navigate to the project directory
cd D:\workspace\NCS\example\v3.1.0\rad_boot_hid\hid_mouse

# Build with sysbuild (builds all components)
west build -b nrf54h20dk/nrf54h20/cpurad --sysbuild

# Flash to device
west flash
```

### Build Outputs

The build process generates three images:
1. `empty_app_core` - CPUAPP minimal application
2. `cpurad_boot` - CPURAD custom bootloader (with USB)
3. `hid_mouse` - CPURAD HID Mouse application

## Device Tree Configuration

### CPUAPP Configuration (`nrf54h20_cpuapp.dtsi`)

Key features:
- Disables unused peripherals (USB, UART, GPIO, etc.)
- Removes VPR (Vector Processor) nodes (cpuflpr, cpuppr)
- Minimal configuration for empty application

### CPURAD Configuration (`nrf54h20_cpurad.dtsi`)

Key features:
- Enables USB HS (High Speed) controller
- Configures GPIO for buttons and LEDs
- Enables UART for debug output
- HID device configuration

## Bootloader Operation

### Boot Sequence

1. **Power-On**: nRF54H20 boots, both cores initialize
2. **CPUAPP**: Loads and runs `empty_app_core` (stays idle)
3. **CPURAD**: Loads and runs `cpurad_boot` bootloader
4. **USB Initialization**: Bootloader enables USB for potential DFU
5. **Peripheral Cleanup**: Bootloader shuts down all peripherals
6. **Application Jump**: Bootloader transfers control to `cpurad_app_partition`
7. **Application Start**: HID Mouse application runs, USB can be reinitialized

### Jump Mechanism

```c
// Bootloader jumps to application
#define APP_PARTITION_NODE  DT_NODELABEL(cpurad_app_partition)
#define TARGET_IMAGE_ADDRESS (DT_REG_ADDR(MRAM_NODE) + DT_REG_ADDR(APP_PARTITION_NODE))

static void jump_to_image(uint32_t image_addr)
{
    // Load vector table
    arm_vector_table_t *app_vector = (arm_vector_table_t *)image_addr;
    
    // Clean up peripherals (CRITICAL!)
    nrf_cleanup();
    arm_core_cleanup();
    
    // Update MSP and jump to reset vector
    __set_MSP(app_vector->msp);
    app_vector->reset_vector();
}
```

## Future Enhancements

### DFU Over USB
With USB enabled in the bootloader and proper cleanup implemented:
- Implement USB DFU class in bootloader
- Receive firmware images via USB
- Write to `cpurad_app2_partition`
- Swap partitions and reboot

### Dual-Bank Firmware Update
- Use `cpurad_app_partition` and `cpurad_app2_partition` as A/B banks
- Implement bank swapping logic
- Rollback support in case of failed updates

## Important Notes

### Partition Label Naming Convention

⚠️ **CRITICAL**: The following partition labels are **hard-coded** in the Nordic SDK and Zephyr RTOS:
- `cpuapp_slot0_partition` - Expected by CPUAPP boot code
- `cpuapp_slot1_partition` - Required for multi-image detection
- `cpurad_slot0_partition` - Expected by CPURAD boot code

**Never rename these labels** unless you also modify:
1. Nordic SDK SoC initialization code (`zephyr/soc/nordic/nrf54h/soc.c`)
2. Bootloader source code (`cpurad_boot/src/main.c`)
3. Zephyr device tree binding expectations

### Memory Alignment

All partitions must be aligned to 4KB boundaries due to MRAM hardware requirements. The partition addresses and sizes in `memlayout.dtsi` already follow this requirement.

### Bootloader Size

The bootloader partition (`cpurad_slot0_partition`) is set to 128KB, which is sufficient for a bootloader with USB support. If adding extensive features, monitor the build output to ensure it fits within this limit.

## Troubleshooting

### Compilation Errors

**Error**: `undefined node label 'cpurad_app_partition'`
- **Solution**: Verify `memlayout.dtsi` defines this partition correctly
- **Check**: Ensure the label matches exactly in bootloader source code

**Error**: `region FLASH overflowed`
- **Solution**: Reduce bootloader features or increase partition size
- **Action**: Adjust sizes in `memlayout.dtsi` while maintaining alignment

### Runtime Issues

**USB not working after jump**
- **Cause**: Peripherals not properly cleaned up in bootloader
- **Solution**: Review and enhance `nrf_cleanup()` function

**Application doesn't start**
- **Cause**: Incorrect jump address or corrupted vector table
- **Solution**: Verify application is programmed to `cpurad_app_partition` address

## License

Copyright (c) 2025 Nordic Semiconductor ASA

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

## References

- [nRF54H20 Product Specification](https://www.nordicsemi.com/Products/nRF54H20)
- [nRF Connect SDK Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html)
- [Zephyr Device Tree Guide](https://docs.zephyrproject.org/latest/build/dts/index.html)
