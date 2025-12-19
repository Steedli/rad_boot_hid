# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

set (TARGET_BOARD "nrf54h20dk/nrf54h20/cpurad")

if(SB_CONFIG_RAD_BOOT)
  ExternalZephyrProject_Add(
    APPLICATION cpurad_boot
    SOURCE_DIR ${APP_DIR}/../cpurad_boot
    BOARD ${TARGET_BOARD}
  )
endif()

