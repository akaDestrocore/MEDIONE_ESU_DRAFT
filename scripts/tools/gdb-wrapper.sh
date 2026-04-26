#!/usr/bin/env bash
CLT=$(ls -d /opt/st/stm32cubeclt_* 2>/dev/null | sort -rV | head -1)
exec "${CLT}/GNU-tools-for-STM32/bin/arm-none-eabi-gdb" "$@"