/*
 * finderdrop_app.r — the FinderDrop example application's SIZE resource.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * canBackground is the one setting FinderDrop needs from an application:
 * a drop arrives while the Finder is in front, and MultiFinder gives an
 * application time in the background only if its SIZE asks.  Without it
 * the drop waits until the user switches back.  The rest is ordinary.
 */
#include "Processes.r"

resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,              /* <- the one FinderDrop needs */
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    reserved, reserved, reserved, reserved, reserved, reserved, reserved,
    384 * 1024,                 /* preferred size */
    256 * 1024                  /* minimum size */
};

/* Version 0.1; keep in step with FDROP_VERSION in fdrop.h. */
data 'vers' (1) {
    $"0010 8000 0000 03302E31"                      /* 0.1, release */
    $"0E46696E64657244726F7020302E31"              /* "FinderDrop 0.1" */
};
