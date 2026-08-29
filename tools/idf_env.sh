#!/bin/bash
# Source this to activate the eim-managed ESP-IDF environment.
# The eim activation script refuses to be sourced from another script
# (its is_sourced() check keys on $0), so use its -e env-dump mode instead.

IDF_VERSION_TAG=v6.1
IDF_ACTIVATE="$HOME/.espressif/tools/activate_idf_${IDF_VERSION_TAG}.sh"

if [ ! -f "$IDF_ACTIVATE" ]; then
    echo "ESP-IDF activation script not found: $IDF_ACTIVATE" >&2
    return 1
fi

while IFS='=' read -r key value; do
    export "$key=$value"
done < <(sh "$IDF_ACTIVATE" -e)

# -e reports PATH as only the ESP tool dirs and stashes the original in SYSTEM_PATH.
# The activation script exposes idf.py as a shell function rather than on PATH,
# so add $IDF_PATH/tools ourselves (its #!/usr/bin/env python picks up the venv).
export PATH="$PATH:$IDF_PATH/tools:$SYSTEM_PATH"
