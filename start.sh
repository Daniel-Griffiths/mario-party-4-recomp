#!/bin/bash
cd "$(dirname "$0")"
make -j8 -C build_pc mp4_pc || exit 1
exec build_pc/mp4_pc "$@"
