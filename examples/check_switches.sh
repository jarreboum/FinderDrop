#!/bin/sh
# check_switches.sh — that a build holds nothing of a switch that is off.
#
# SPDX-License-Identifier: 0BSD
# Copyright (C) 2026 by the FinderDrop authors (LICENSE).
#
#   sh examples/check_switches.sh
#
# For each build switch in fdrop.h there is a list below of the
# functions and state words that exist only for it.  The library's two
# halves are compiled with every switch on, and each list must be THERE;
# then with that one switch off, and each must be GONE.  Compiled
# -fno-inline and not linked, so that nothing is hidden inside a caller
# or dropped by the linker: what is checked is what the compiler was
# given, which is what a host linked without --gc-sections carries.
#
# It checks names, so it catches a function or a word left behind.  A
# branch inside a function both builds keep is not a name, and is found
# by reading; the lists are where to add a name when one is.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LIB=$(cd "$HERE/.." && pwd)
T=${RETRO68:-/opt/Retro68-build/toolchain}
CC=$T/bin/m68k-apple-macos-gcc
NM=$T/bin/m68k-apple-macos-nm
OUT=${OUT:-$LIB/build/examples/switches}
mkdir -p "$OUT"

ROM_ROUTE="rom_answer rom_arm resolve_rom rom_desk rom_root rom_record
 rom_disk rom_proc rom_body rom_drag_routine FDROP_ROMDRAG FDROP_ROMWRAP
 FDROP_ROMMODE"
OWN_ROUTE="resolve_drag finder_drag_call takeable container_dir
 hfs_volume mfs_volume mfs_item disk_record same_gesture drag_arm startup_trash desktop_handle
 shown_in rec_by_handle rec_next FDROP_FWRET FDROP_OUTSITE
 FDROP_LASTGM FDROP_FAKED"
RECEIVER="FDROP_RECEIVE FDROP_TRACKING drag_files call_room record_drop
 FDROP_ROOM"
# The records of the 1985-86 Finders are branches, not names; their one
# word is the wrapper System 3 puts in front of the ROM routine.
FINDER5="FDROP_ROMWRAP"
ROM64K="trap_addr64 rom64_slot rom64_present rom64_encode FDROP_ROM64_FWW
 FDROP_ROM64_GMW"
ROMPLUS="tramp_at tramp_passes tramp_reusable tramp_flush tramp_mark
 multifinder listed FDROP_ROMWRAP FDROP_TOOLTAB"
# With BOTH routes off there is no sender, and these go as well: the
# MFS files and folders are both routes'.  (FDROP_STEP is FDROP_DIAG's.)
SENDER="patch_install patch_remove fdrop_look fdrop_answer over_panel
 window_at answer_start port_origin blk_next name_ends item_name fm_entry
 mfs_take mfs_folder queue_item same_pstr rec_of vol_name vol_online
 vcb_of vcb_vol tramp_write FDROP_GM FDROP_FW FDROP_FW_OLD FDROP_GM_OLD FDROP_TRAMP
 FDROP_BUSY FDROP_DRAGFP FDROP_DRAGRET FDROP_START FDROP_GEST FDROP_ACT
 FDROP_OVER FDROP_FIN FDROP_STATE FDROP_PEND FDROP_PENDAT FDROP_LASTN
 FDROP_SEQ FDROP_OVERFLOW FDROP_ENABLE"
# The early route on the 64K ROM alone patches _GetMouse and nothing else.
ONEPATCH="FDROP_FW FDROP_FW_OLD FDROP_ROM64_FWW"

names() {       # every defined name in a build with the given -D flags
    $CC -c -Os -fno-inline -std=c99 -m68000 -mpcrel -ffreestanding \
        -fno-common --param=min-pagesize=0 -Wall -Wextra -Werror \
        -I"$LIB" "$@" -o "$OUT/c.o" "$LIB/fdrop.c"
    $CC -c -m68000 -I"$LIB" "$@" -o "$OUT/s.o" "$LIB/fdrop.S"
    $NM --defined-only "$OUT/c.o" "$OUT/s.o" | awk 'NF == 3 { print $3 }' \
        | sed 's/\.constprop.*//; s/\.part.*//; s/\.isra.*//' | sort -u
}

bad=0
names > "$OUT/all.txt"
check() {       # check "what is off" "names that must be gone" -D...
    label=$1; list=$2; shift 2
    for n in $list; do
        grep -qx "$n" "$OUT/all.txt" \
            || { echo "check_switches: $n is not in the full build: the list for $label is stale"; bad=1; }
    done
    names "$@" > "$OUT/off.txt"
    for n in $list; do
        if grep -qx "$n" "$OUT/off.txt"; then
            echo "check_switches: $n is still there with $label off"; bad=1
        fi
    done
    echo "check_switches: $label off: $(echo $list | wc -w) names gone, $(wc -l < "$OUT/off.txt") left"
}

check FDROP_ROM_ROUTE "$ROM_ROUTE" -DFDROP_ROM_ROUTE=0
check FDROP_OWN_ROUTE "$OWN_ROUTE" -DFDROP_OWN_ROUTE=0
check FDROP_RECEIVER  "$RECEIVER"  -DFDROP_RECEIVER=0
check FDROP_FINDER5   "$FINDER5"   -DFDROP_FINDER5=0
check FDROP_ROM64K    "$ROM64K"    -DFDROP_ROM64K=0
check FDROP_ROMPLUS   "$ROMPLUS"   -DFDROP_ROMPLUS=0
check "both routes" "$SENDER $ROM_ROUTE $OWN_ROUTE $ROM64K $ROMPLUS" \
    -DFDROP_ROM_ROUTE=0 -DFDROP_OWN_ROUTE=0
check "FDROP_OWN_ROUTE and FDROP_ROMPLUS" "$ONEPATCH $OWN_ROUTE $ROMPLUS" \
    -DFDROP_OWN_ROUTE=0 -DFDROP_ROMPLUS=0
check "everything after Finder 1.1g and the 64K ROM" \
    "$ONEPATCH $OWN_ROUTE $RECEIVER $ROMPLUS $FINDER5" \
    -DFDROP_FINDER5=0 -DFDROP_OWN_ROUTE=0 \
    -DFDROP_RECEIVER=0 -DFDROP_ROMPLUS=0
check "all six" "$SENDER $ROM_ROUTE $OWN_ROUTE $RECEIVER $ROM64K $ROMPLUS $FINDER5" \
    -DFDROP_ROM_ROUTE=0 -DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0 \
    -DFDROP_ROM64K=0 -DFDROP_ROMPLUS=0 -DFDROP_FINDER5=0
[ $bad = 0 ] || { echo "check_switches: FAILED"; exit 1; }
echo "check_switches: every build holds nothing of a switch that is off"
