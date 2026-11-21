/*
 * cdev.ld — layout of the FinderDrop example control panel's cdev resource.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * The entry branch first, by section name.  Then the drag library's
 * patches and their storage, kept together and early: everything in
 * .text.patch is addressed PC-relative from the patch code, and a trap
 * patch has no A5 world to reach a global through.  .data and .bss are
 * not placed: build.sh fails on a byte of either.
 */
ENTRY(CdevStart)

SECTIONS
{
    /DISCARD/ : { *(.text.*.macsbug) }

    . = CDEV_BASE;

    .text : {
        *(.text.cdeventry)
        *(.text.patch)
        *fdrop.o(.text .text.* .rodata .rodata.*)
        *(.text .text.* .rodata .rodata.*)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
