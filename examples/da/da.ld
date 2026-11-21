/*
 * da.ld — layout of the FinderDrop example desk accessory's DRVR resource.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * The header first, by section name.  Then the drag library's patches
 * and their storage, kept together and early: everything in .text.patch
 * is addressed PC-relative from the patch code, and a trap patch has no
 * A5 world to reach a global through.  .data and .bss are not placed: a
 * code resource has nowhere to reach them from, and build.sh fails on a
 * byte of either.
 */
ENTRY(DAStart)

SECTIONS
{
    /DISCARD/ : { *(.text.*.macsbug) }

    . = DA_BASE;

    .text : {
        *(.text.drvrheader)
        *(.text.patch)
        *fdrop.o(.text .text.* .rodata .rodata.*)
        *(.text .text.* .rodata .rodata.*)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
