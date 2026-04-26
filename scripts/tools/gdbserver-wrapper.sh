#!/usr/bin/env bash
CLT=$(ls -d /opt/st/stm32cubeclt_* 2>/dev/null | sort -rV | head -1)
exec "${CLT}/STLink-gdb-server/bin/ST-LINK_gdbserver" "$@"