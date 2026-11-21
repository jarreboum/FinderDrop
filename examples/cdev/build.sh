#!/bin/sh
# build.sh — the FinderDrop example control panel.
#
# SPDX-License-Identifier: 0BSD
# Copyright (C) 2026 by the FinderDrop authors (LICENSE).
#
#   sh examples/cdev/build.sh [-DFDROP_DIAG=1]
#
# gives build/examples/cdev/FinderDrop.bin, a MacBinary Control Panel
# file (type cdev, creator FDcp).  -DFDROP_DIAG=1 is the debugging build:
# the panel shows the library's diagnostic words too.
#
# Everything compiled -mpcrel and freestanding, linked twice at two
# addresses and the two images compared, with nothing in .data or .bss,
# exactly as the desk accessory is; then Rez makes the Control Panel
# document, a file of type 'cdev'.
#
# Any argument is passed to BOTH halves of the library, which must be
# built with the same flags (README.md).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LIB=$(cd "$HERE/../.." && pwd)
T=${RETRO68:-/opt/Retro68-build/toolchain}
CC=$T/bin/m68k-apple-macos-gcc
LD=$T/bin/m68k-apple-macos-ld
OC=$T/bin/m68k-apple-macos-objcopy
OD=$T/bin/m68k-apple-macos-objdump
REZ=$T/bin/Rez
OUT=${OUT:-$LIB/build/examples/cdev}
mkdir -p "$OUT"
rm -f "$OUT"/*.o "$OUT"/*.elf "$OUT"/*.cdev "$OUT"/*.bin

DEFS="$*"
FLAGS="-c -Os -std=c99 -m68000 -mpcrel -ffreestanding -fno-common \
 -ffunction-sections -fdata-sections -fomit-frame-pointer \
 -Wall -Wextra -Werror --param=min-pagesize=0 \
 -I$HERE -I$LIB $DEFS"

$CC $FLAGS -o "$OUT/finderdrop_cdev.o" "$HERE/finderdrop_cdev.c"
$CC $FLAGS -o "$OUT/runtime.o" "$HERE/../runtime.c"
$CC $FLAGS -o "$OUT/fdrop.o" "$LIB/fdrop.c"
$CC -c -m68000 -I"$HERE" -I"$LIB" $DEFS -o "$OUT/fdrop_asm.o" "$LIB/fdrop.S"
$CC -c -m68000 -o "$OUT/cdev_entry.o" "$HERE/cdev_entry.S"

OBJS="$OUT/cdev_entry.o $OUT/fdrop_asm.o $OUT/fdrop.o $OUT/finderdrop_cdev.o $OUT/runtime.o"
$LD -T "$HERE/cdev.ld" --defsym CDEV_BASE=0 --gc-sections $OBJS -o "$OUT/cdev_at_0.elf"
$LD -T "$HERE/cdev.ld" --defsym CDEV_BASE=0x40000 --gc-sections $OBJS -o "$OUT/cdev_at_40000.elf"
$OC -O binary -j .text "$OUT/cdev_at_0.elf" "$OUT/FinderDrop.cdev"
$OC -O binary -j .text "$OUT/cdev_at_40000.elf" "$OUT/moved.cdev"
if ! cmp -s "$OUT/FinderDrop.cdev" "$OUT/moved.cdev"; then
    echo "build: the image differs when linked elsewhere: an absolute address is in it" >&2
    cmp -l "$OUT/FinderDrop.cdev" "$OUT/moved.cdev" | head -5 >&2
    exit 1
fi
if $OD -h "$OUT/cdev_at_0.elf" | awk '$2 ~ /^\.(data|bss)/ && $3 != "00000000" {f=1} END {exit !f}'; then
    echo "build: .data or .bss is not empty: a global in a code resource" >&2
    $OD -h "$OUT/cdev_at_0.elf" | grep -E "\.(data|bss)" >&2
    exit 1
fi
(cd "$OUT" && $REZ -I"$T/RIncludes" "$HERE/finderdrop_cdev.r" \
    -o FinderDrop.bin -t cdev -c FDcp)
# The bundle bit Rez leaves clear (the Finder reads no BNDL without it):
# hasBundle is 0x20 in the flags' high byte, at offset 73 of the MacBinary
# header, which the CRC at 124 covers -- checked first, then recomputed.
python3 -I - "$OUT/FinderDrop.bin" <<'PY'
import sys
p = sys.argv[1]
d = bytearray(open(p, "rb").read())
def crc(b):
    c = 0
    for x in b:
        c ^= x << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c
if crc(d[:124]) != (d[124] << 8 | d[125]):
    sys.exit("build: Rez's MacBinary header does not check; not rewriting it")
d[73] |= 0x20
c = crc(d[:124])
d[124], d[125] = c >> 8, c & 0xFF
open(p, "wb").write(d)
PY
echo "build: FinderDrop.cdev, $(wc -c < "$OUT/FinderDrop.cdev") bytes, position independent; FinderDrop.bin, the panel"
