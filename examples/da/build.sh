#!/bin/sh
# build.sh — the FinderDrop example desk accessory.
#
# SPDX-License-Identifier: 0BSD
# Copyright (C) 2026 by the FinderDrop authors (LICENSE).
#
#   sh examples/da/build.sh [-DFDROP_DIAG=1]
#
# gives build/examples/da/FinderDrop.drvr, the DRVR resource to put in a
# System file, and FinderDrop.bin, that resource in a Font/DA Mover
# suitcase as a MacBinary file, which is what a Macintosh installs.
# -DFDROP_DIAG=1 is the debugging build: the window shows the library's
# diagnostic words too.  Any -D goes to BOTH halves of the
# library, which must agree.
#
# A desk accessory is a code resource: loaded wherever the heap had room,
# never relocated, no A5 world.  So everything is compiled -mpcrel and
# freestanding, the result is linked twice at two addresses and the two
# images compared (an absolute address would differ), and .data and .bss
# must be empty (a global would need an A5 world).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LIB=$(cd "$HERE/../.." && pwd)            # where fdrop.c, .S and .h are
T=${RETRO68:-/opt/Retro68-build/toolchain}
CC=$T/bin/m68k-apple-macos-gcc
LD=$T/bin/m68k-apple-macos-ld
OC=$T/bin/m68k-apple-macos-objcopy
OD=$T/bin/m68k-apple-macos-objdump
OUT=${OUT:-$LIB/build/examples/da}
mkdir -p "$OUT"
rm -f "$OUT"/*.o "$OUT"/*.elf "$OUT"/*.drvr "$OUT"/*.bin

DEFS="$*"
FLAGS="-c -Os -std=c99 -m68000 -mpcrel -ffreestanding -fno-common \
 -ffunction-sections -fdata-sections -fomit-frame-pointer \
 -Wall -Wextra -Werror --param=min-pagesize=0 \
 -I$HERE -I$LIB $DEFS"

$CC $FLAGS -o "$OUT/finderdrop_da.o" "$HERE/finderdrop_da.c"
$CC $FLAGS -o "$OUT/runtime.o" "$HERE/../runtime.c"
$CC $FLAGS -o "$OUT/fdrop.o" "$LIB/fdrop.c"
$CC -c -m68000 -I"$HERE" -I"$LIB" $DEFS -o "$OUT/fdrop_asm.o" "$LIB/fdrop.S"
$CC -c -m68000 -o "$OUT/da_header.o" "$HERE/da_header.S"

# The header first (da.ld places it by name), then the library.
OBJS="$OUT/da_header.o $OUT/fdrop_asm.o $OUT/fdrop.o $OUT/finderdrop_da.o $OUT/runtime.o"
$LD -T "$HERE/da.ld" --defsym DA_BASE=0 --gc-sections $OBJS -o "$OUT/da_at_0.elf"
$LD -T "$HERE/da.ld" --defsym DA_BASE=0x40000 --gc-sections $OBJS -o "$OUT/da_at_40000.elf"
$OC -O binary -j .text "$OUT/da_at_0.elf" "$OUT/FinderDrop.drvr"
$OC -O binary -j .text "$OUT/da_at_40000.elf" "$OUT/moved.drvr"
if ! cmp -s "$OUT/FinderDrop.drvr" "$OUT/moved.drvr"; then
    echo "build: the image differs when linked elsewhere: an absolute address is in it" >&2
    cmp -l "$OUT/FinderDrop.drvr" "$OUT/moved.drvr" | head -5 >&2
    exit 1
fi
if $OD -h "$OUT/da_at_0.elf" | awk '$2 ~ /^\.(data|bss)/ && $3 != "00000000" {f=1} END {exit !f}'; then
    echo "build: .data or .bss is not empty: a global in a code resource" >&2
    $OD -h "$OUT/da_at_0.elf" | grep -E "\.(data|bss)" >&2
    exit 1
fi
echo "build: $OUT/FinderDrop.drvr, $(wc -c < "$OUT/FinderDrop.drvr") bytes, position independent"
python3 "$HERE/suitcase.py" "$OUT/FinderDrop.drvr" "$OUT/FinderDrop.bin"
echo "build: $OUT/FinderDrop.bin, the same in a Font/DA Mover suitcase (MacBinary)"
