/*
 * finderdrop_cdev.r — the FinderDrop example control panel, as a Control
 * Panel file: the code, the rectangle it lives in, its one item, a
 * machine mask and an icon, all at resource ID -4064, which is where the
 * Control Panel looks.  Built by build.sh, which runs Rez where the code
 * resource is.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 */

#include "Multiverse.r"

/* locked: a code resource must not move while it runs. */
data 'cdev' (-4064, "FinderDrop", locked) {
    $$read("FinderDrop.cdev")
};

/*
 * The rectangle Apple's own General panel occupies on System 6.0.8: from
 * left 87, where the icon list ends, to the window's right at 321 and
 * bottom at 253.  System 7 gives the panel a window this size.
 */
data 'nrct' (-4064, "FinderDrop", purgeable) {
    $"0001"                     /* one rectangle                       */
    $"0000 0057 00FD 0141"      /* top 0, left 87, bottom 253, right 321 */
};

/* softMask $FFFF, hardMask 0: ask the cdev (macDev), which says yes. */
data 'mach' (-4064, "FinderDrop", purgeable) {
    $"FFFF 0000"
};

/* One user item, disabled, which is where the panel draws. */
resource 'DITL' (-4064, "FinderDrop", purgeable) {
    {
        { 4, 90, 249, 318 }, UserItem { disabled }
    }
};

data 'ICN#' (-4064, "FinderDrop", purgeable) {
        $"00000000 00000000 0FFFFFF0 08000010"
        $"08000010 08000010 08000010 08000010"
        $"08018010 08018010 08018010 08018010"
        $"08018010 08018010 08018010 08018010"
        $"08018010 087FFE10 083FFC10 081FF810"
        $"080FF010 0807E010 0803C010 08018010"
        $"08000010 08000010 08000010 08000010"
        $"08000010 0FFFFFF0 00000000 00000000"
        $"00000000 00000000 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 00000000 00000000"
};

data 'ICN#' (128, "FinderDrop", purgeable) {
        $"00000000 00000000 0FFFFFF0 08000010"
        $"08000010 08000010 08000010 08000010"
        $"08018010 08018010 08018010 08018010"
        $"08018010 08018010 08018010 08018010"
        $"08018010 087FFE10 083FFC10 081FF810"
        $"080FF010 0807E010 0803C010 08018010"
        $"08000010 08000010 08000010 08000010"
        $"08000010 0FFFFFF0 00000000 00000000"
        $"00000000 00000000 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 0FFFFFF0 0FFFFFF0"
        $"0FFFFFF0 0FFFFFF0 00000000 00000000"
};

/*
 * The Finder's view of the file, which the Control Panel also reads
 * (Inside Macintosh V lists BNDL, FREF and the owner's signature among a
 * cdev file's resources): the creator's signature, the file reference
 * naming type 'cdev', and the bundle tying the icon to it.  build.sh sets
 * the bundle bit Rez leaves clear.
 */
type 'FDcp' as 'STR ';

resource 'FDcp' (0, "Signature", purgeable) {
    "FinderDrop"
};

resource 'FREF' (128, purgeable) {
    'cdev', 0, ""
};

resource 'BNDL' (128, purgeable) {
    'FDcp', 0,
    {
        'ICN#', { 0, 128 },
        'FREF', { 0, 128 }
    }
};

/* Version 0.1; keep in step with FDROP_VERSION in fdrop.h. */
data 'vers' (1) {
    $"0010 8000 0000 03302E31"                      /* 0.1, release */
    $"0E46696E64657244726F7020302E31"              /* "FinderDrop 0.1" */
};
