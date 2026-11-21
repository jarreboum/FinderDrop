#!/bin/sh
# build.sh — the FinderDrop example application.
#
# SPDX-License-Identifier: 0BSD
# Copyright (C) 2026 by the FinderDrop authors (LICENSE).
#
#   sh examples/app/build.sh [-DFDROP_DIAG=1]
#
# gives build/examples/app/FinderDropApp.bin, a MacBinary application
# ("FinderDrop Example", creator FDrp).  -DFDROP_DIAG=1 is the debugging
# build: the window shows the library's diagnostic words too.
#
# The app code is compiled with an A5 world (no -mpcrel); the library's
# two halves are compiled -mpcrel and keep their state PC-relative,
# because the trap patches run in the Finder's A5 world and not this
# one.  Both coexist in one CODE resource; the library is compiled WITHOUT
# -ffunction-sections so its code and the state in fdrop.S stay one
# contiguous blob within PC-relative reach.
#
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LIB=$(cd "$HERE/../.." && pwd)
T=${RETRO68:-/opt/Retro68-build/toolchain}
CC=$T/bin/m68k-apple-macos-gcc
REZ=$T/bin/Rez
REZTPL=$T/RIncludes
OUT=${OUT:-$LIB/build/examples/app}
mkdir -p "$OUT"
rm -f "$OUT"/*.o "$OUT"/*.code.bin "$OUT"/*.bin

DEFS="$*"
# Retro68's compiler writes every function's name after it, for MacsBug.
# Built one section per function, the linker can drop them: about 2.4 KB
# of this application.  MACSBUG=1 keeps them, for stepping through it.
if [ "${MACSBUG:-0}" = 1 ]; then
    SECT=""; STRIP=""
else
    SECT="-ffunction-sections"; STRIP="-Wl,--mac-strip-macsbug"
fi
APPFLAGS="-c -Os $SECT -std=c99 -m68000 --param=min-pagesize=0 \
 -Wall -Wextra -Werror -Wno-pointer-sign -I$HERE -I$LIB $DEFS"
# The library halves: -mpcrel.
LIBFLAGS="-c -Os $SECT -std=c99 -m68000 -mpcrel -ffreestanding -fno-common \
 --param=min-pagesize=0 -Wall -Wextra -Werror -I$HERE -I$LIB $DEFS"

$CC $APPFLAGS -o "$OUT/finderdrop_app.o" "$HERE/finderdrop_app.c"
$CC $LIBFLAGS -o "$OUT/fdrop.o" "$LIB/fdrop.c"
$CC -c -m68000 -I"$HERE" -I"$LIB" $DEFS -o "$OUT/fdrop_asm.o" "$LIB/fdrop.S"

# Link as a Retro68 application: the library objects FIRST so their text
# leads the CODE resource and every PC-relative reach inside them is short.
$CC $STRIP -o "$OUT/FinderDropApp.code.bin" \
    "$OUT/fdrop_asm.o" "$OUT/fdrop.o" "$OUT/finderdrop_app.o"

$REZ -I"$REZTPL" "$T/RIncludes/Retro68APPL.r" "$HERE/finderdrop_app.r" \
    --copy "$OUT/FinderDropApp.code.bin" \
    -o "$OUT/FinderDropApp.bin" -t APPL -c "FDrp"
echo "build: $OUT/FinderDropApp.bin, $(wc -c < "$OUT/FinderDropApp.bin") bytes"
