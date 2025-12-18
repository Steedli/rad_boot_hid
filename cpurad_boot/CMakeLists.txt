#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cpurad_boot)
include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)

target_sources(app PRIVATE
  src/main.c
  src/arm_cleanup.c
  src/nrf_cleanup.c
)

include_directories(include)