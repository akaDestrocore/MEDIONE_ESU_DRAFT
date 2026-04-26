#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SETTINGS="$SCRIPT_DIR/../../.vscode/settings.json"

# Read the CLT path that find_stm32cubeclt.py wrote into settings.json
CLT_PATH=$(python3 - "$SETTINGS" <<'PY'
import sys, json
with open(sys.argv[1]) as f:
    print(json.load(f)["stm32.cltPath"])
PY
)

GDBSERVER="$CLT_PATH/STLink-gdb-server/bin/ST-LINK_gdbserver"
PROGRAMMER_BIN="$CLT_PATH/STM32CubeProgrammer/bin"

# Rebuild the argument list, dropping the wrong -cp <value> pair
ARGS=()
skip_next=false
for arg in "$@"; do
    if $skip_next; then
        skip_next=false
        continue
    fi
    if [[ "$arg" == "-cp" ]]; then
        skip_next=true
        continue
    fi
    ARGS+=("$arg")
done

exec "$GDBSERVER" "${ARGS[@]}" -cp "$PROGRAMMER_BIN"