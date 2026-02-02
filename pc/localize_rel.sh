#!/bin/bash
# Partial-link REL module objects, then localize all symbols except DLL_ID-prefixed ones.
# Usage: localize_rel.sh <dll_id> <output.o> <input1.o> <input2.o> ...
set -e
DLL_ID="$1"
OUTPUT="$2"
shift 2

# Partial link all object files into one relocatable .o
ld -r -o "$OUTPUT" "$@" 2>/dev/null

# Build exports list: only DLL_ID-prefixed symbols that actually exist in the .o
# nmedit -s keeps listed symbols global, makes all others local.
# nmedit fails if any listed symbol doesn't exist, so we filter.
nm -g "$OUTPUT" 2>/dev/null | awk -v prefix="_${DLL_ID}__" '$0 ~ prefix {print $3}' > "${OUTPUT}.exports"

if [ -s "${OUTPUT}.exports" ]; then
    nmedit -s "${OUTPUT}.exports" "$OUTPUT"
fi
rm -f "${OUTPUT}.exports"
