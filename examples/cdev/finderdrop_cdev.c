/*
 * finderdrop_cdev.c — FinderDrop in a control panel.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * A control panel that does nothing but be dropped on: drag files,
 * folders or disks from the Finder onto its area and it lists each one.
 * It is the smallest complete use of FinderDrop in a cdev, written to be
 * read.  It doubles as a debugger: built with -DFDROP_DIAG=1, the panel
 * also shows the library's diagnostic words.
 *
 * It runs wherever there is a Control Panel that takes cdevs: System 4.1
 * to Mac OS 9 (4.0 takes them too).  Where the system has a Drag Manager
 * FinderDrop uses it.  Everywhere else it follows the Finder's own icon
 * drag with two trap patches, except on System 7.0 and 7.1 without
 * Apple's Macintosh Drag and Drop extension, whose Finder it does not
 * read, so nothing arrives there.  This file cannot tell which happens, and does not need to: the
 * calls are the same.
 *
 * A CDEV'S LIFE is a series of calls from the Control Panel, one message
 * each: initDev when the panel is chosen, nulDev over and over while it
 * is showing, updateDev when its area needs drawing, closeDev when
 * another panel is chosen or the Control Panel closes.  Between closeDev
 * and the next initDev the code resource may be thrown away.
 *
 * THE CALLS A HOST MAKES, and the message each belongs to:
 *
 *   fdrop_install(window, queue, n, 0)   initDev, once there is state
 *   fdrop_idle()            nulDev
 *   fdrop_take_drop(...)    nulDev, after fdrop_idle, until it says no
 *   fdrop_remove(window)    closeDev, FIRST
 *   fdrop_have_drag_manager()   only to say which door is open
 *
 * The host window is the Control Panel's own: the dialog it passes in.
 * The queue FinderDrop keeps the drops in is ours: kDrops entries inside
 * the state handle, which stays LOCKED from initDev to closeDev, because
 * FinderDrop writes there from inside the Finder's drag, when nothing
 * could notice that it had moved.
 *
 * WHAT A DROP IS.  Every item arrives as three things: a volume reference
 * number, a directory ID and a name, an FSSpec in all but type.  A file
 * or a folder is its volume, the directory it is IN, and its name.  A
 * disk is its volume, directory 1 (the parent of every root) and the
 * volume's name.  A folder on an MFS volume, which the File Manager has
 * no name for, arrives as the files in it, each with directory 0.  The
 * Trash arrives as the startup disk's Trash folder where the system
 * keeps one, and is left out where it does not (System 6).  What an item
 * IS, the program asks the File Manager: see what_is() below.
 *
 * WHAT A CDEV MUST NOT DO, and this one does not:
 *
 *   - have globals.  A cdev is a code resource with no A5 world, so its
 *     state is a handle, returned from initDev and handed back by the
 *     Control Panel as cdevValue on every call after;
 *   - use absolute addresses.  The resource is loaded wherever the heap
 *     had room and never relocated, so everything is compiled -mpcrel and
 *     build.sh links it twice at two addresses and compares the bytes;
 *   - keep the trap patches past closeDev.  They point into this
 *     resource, so fdrop_remove comes before anything else is let go.
 *
 * The entry point is cdev_entry.S, a branch at byte zero.  The panel's
 * rectangle, its one dialog item and the bundle the Control Panel needs
 * to list it are finderdrop_cdev.r.  build.sh builds a Control Panel file;
 * put it in the System Folder (System 7: the Control Panels folder).
 */
#include <Multiverse.h>
#include "fdrop.h"

/* The messages the Control Panel sends, and what it understands back. */
#define initDev     0
#define hitDev      1
#define closeDev    2
#define nulDev      3
#define updateDev   4
#define activDev    5
#define deactivDev  6
#define keyEvtDev   7
#define macDev      8

#define cdevGenErr  (-1)
#define cdevMemErr  0
#define cdevResErr  1
#define cdevUnset   3

#define kArea       1       /* our one item: the user item we draw in */
#define LINE_H      11      /* Geneva 9 */
#define SQ          10      /* the squares along the bottom */

/* How many items one drag may bring. */
#define kDrops      8

/* How many items the list shows: fewer in the debugging build, where the
 * diagnostic words need the room. */
#if FDROP_DIAG
#define kShown      3
#else
#define kShown      kDrops
#endif

/* Everything the panel keeps, in a handle (no globals). */
typedef struct {
    Boolean        dragMgr;          /* which door FinderDrop opened */
    unsigned short drops;            /* items received this visit */
    unsigned short head;             /* the next entry to overwrite */
    unsigned short shown;            /* how many entries hold an item */
    unsigned char  what[kShown][48]; /* "folder  Name" */
    unsigned char  where[kShown][48];/* "   on Volume (-1), directory 2" */
    unsigned char  queue[kDrops * FDROP_ENTRY_BYTES];   /* FinderDrop's */
} CdevState;

typedef CdevState **CdevStateH;

pascal long finderdrop_cdev(short message, short item, short numItems,
                            short cPanelID, EventRecord *ev, long cdevValue,
                            DialogPtr dlg);

/* ------------------------------------------------------------------ */
/* Text, without a C library                                           */
/* ------------------------------------------------------------------ */

static void put_c(unsigned char *s, char c)
{
    if (s[0] < 47)                      /* the lines are 48 bytes */
        s[++s[0]] = (unsigned char)c;
}

static void put_text(unsigned char *s, const char *t)
{
    while (*t)
        put_c(s, *t++);
}

static void put_pstr(unsigned char *s, const unsigned char *p)
{
    short i;

    for (i = 1; i <= (short)p[0]; i++)
        put_c(s, (char)p[i]);
}

/* A signed number in decimal.  No division: a 32-bit divide is a libgcc
 * call, and a code resource links no libgcc. */
static void put_dec(unsigned char *s, long v)
{
    static const long tens[] = { 1000000000L, 100000000L, 10000000L,
                                 1000000L, 100000L, 10000L, 1000L, 100L,
                                 10L, 1L };
    Boolean any = false;
    short   i;

    if (v < 0) {
        put_c(s, '-');
        v = -v;
    }
    for (i = 0; i < 10; i++) {
        char d = '0';

        while (v >= tens[i]) {
            v -= tens[i];
            d++;
        }
        if (d != '0' || any || i == 9) {
            put_c(s, d);
            any = true;
        }
    }
}

#if FDROP_DIAG
static void put_hex(unsigned char *s, unsigned short v)
{
    static const char hx[] = "0123456789ABCDEF";
    short i;

    for (i = 3; i >= 0; i--)
        put_c(s, hx[(v >> (4 * i)) & 15]);
}
#endif

/* ------------------------------------------------------------------ */
/* What a dropped item is                                              */
/* ------------------------------------------------------------------ */

/*
 * The File Manager's answer, which is the only one a program should
 * believe.  Directory 1 is a disk; directory 0 is an MFS volume, where
 * everything is a file.  Anything else is asked about.
 */
static const char *what_is(short vRefNum, long dirID, const unsigned char *name)
{
    CInfoPBRec ci;
    unsigned char nm[64];
    short i;

    if (dirID == 1L)
        return "disk";
    if (dirID == 0L)
        return "file";
    for (i = 0; i <= (short)name[0] && i < (short)sizeof nm; i++)
        nm[i] = name[i];
    for (i = 0; i < (short)sizeof ci; i++)
        ((char *)&ci)[i] = 0;
    ci.hFileInfo.ioNamePtr = nm;        /* a copy: the call may write it */
    ci.hFileInfo.ioVRefNum = vRefNum;
    ci.hFileInfo.ioDirID   = dirID;
    if (PBGetCatInfoSync(&ci) != noErr)
        return "gone";                  /* moved or deleted since */
    return (ci.hFileInfo.ioFlAttrib & 0x10) ? "folder" : "file";
}

/* The name of the volume a vRefNum names, for the list. */
static void volume_name(short vRefNum, unsigned char *out)
{
    ParamBlockRec pb;
    short i;

    for (i = 0; i < (short)sizeof pb; i++)
        ((char *)&pb)[i] = 0;
    out[0] = 0;
    pb.volumeParam.ioNamePtr  = out;
    pb.volumeParam.ioVRefNum  = vRefNum;
    pb.volumeParam.ioVolIndex = 0;
    if (PBGetVInfoSync(&pb) != noErr)
        out[0] = 0;
}

/* ------------------------------------------------------------------ */
/* Taking the drops                                                    */
/* ------------------------------------------------------------------ */

/*
 * Ask until FinderDrop says there is nothing more.  A drag of several
 * icons is several drops, in the order FinderDrop found them: the Drag
 * Manager's order through it, and without one the order the Finder's
 * records lie in its heap.  Neither is promised to be the order the
 * user selected them in, so nothing here depends on the order.  Each
 * one becomes two lines: what it is and its name, then where it is.
 * The window is the Control Panel's dialog, which is what was installed.
 */
static Boolean take_drops(CdevState *x, DialogPtr dlg)
{
    short         vRefNum;
    long          dirID;
    unsigned char name[64], vol[32];
    Boolean       any = false;

    while (fdrop_take_drop((WindowPtr)dlg, &vRefNum, &dirID, name)) {
        unsigned char *a = x->what[x->head];
        unsigned char *b = x->where[x->head];

        a[0] = 0;
        put_text(a, what_is(vRefNum, dirID, name));
        put_text(a, "  ");
        put_pstr(a, name);
        b[0] = 0;
        put_text(b, "    on ");
        volume_name(vRefNum, vol);
        put_pstr(b, vol);
        put_text(b, " (");
        put_dec(b, vRefNum);
        put_text(b, "), directory ");
        put_dec(b, dirID);

        x->head = (unsigned short)(x->head + 1 == kShown ? 0 : x->head + 1);
        if (x->shown < kShown)
            x->shown++;
        x->drops++;
        any = true;
    }
    return any;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/* Our item's rectangle, which is where the panel is on every System:
 * System 6 draws a panel's items at their DITL coordinates in its own
 * window, and System 7 gives the panel a window of its own and moves
 * them.  numItems is how many items the Control Panel had before ours. */
static void area(DialogPtr dlg, short numItems, Rect *r)
{
    short  kind;
    Handle h;

    GetDialogItem(dlg, (short)(numItems + kArea), &kind, &h, r);
}

static void line_at(const Rect *a, short row, const unsigned char *s)
{
    MoveTo((short)(a->left + 4), (short)(a->top + 2 + LINE_H * (row + 1)));
    DrawString(s);
}

static void draw(CdevState *x, DialogPtr dlg, short numItems)
{
    unsigned char s[64];
    Rect a, r;
    short i, row = 0;

    SetPort(dlg);
    area(dlg, numItems, &a);
    TextFont(3);                        /* Geneva */
    TextSize(9);
    EraseRect(&a);

    s[0] = 0;
    put_text(s, x->dragMgr ? "Through the Drag Manager."
                           : "Following the Finder's drag.");
    line_at(&a, row++, s);
    s[0] = 0;
    put_text(s, "Received: ");
    put_dec(s, x->drops);
    line_at(&a, row++, s);
    row++;

    /* The most recent kShown items, oldest first. */
    for (i = 0; i < (short)x->shown; i++) {
        short k = (short)(x->head - x->shown + i);   /* oldest first */

        if (k < 0)
            k = (short)(k + kShown);

        line_at(&a, row++, x->what[k]);
        line_at(&a, row++, x->where[k]);
    }

#if FDROP_DIAG
    {
        /* The debugging view: the library's own words, in hex, eight to
         * a line.  fdrop.h and fdrop.c say what each one holds. */
        short diag[FDROP_DIAG_WORDS];

        row = (short)(4 + 2 * kShown);
        fdrop_diag(diag);
        s[0] = 0;
        for (i = 0; i < FDROP_DIAG_WORDS; i++) {
            if (i > 0 && i % 8 == 0) {
                line_at(&a, row++, s);
                s[0] = 0;
            }
            put_hex(s, (unsigned short)diag[i]);
            put_c(s, ' ');
        }
        line_at(&a, row, s);
    }
#endif

    /* One black square per item along the bottom, so a test harness can
     * count them off a screenshot without reading any text. */
    for (i = 0; i < (short)x->drops
                && a.left + 4 + (SQ + 4) * (i + 1) < a.right; i++) {
        SetRect(&r, (short)(a.left + 4 + (SQ + 4) * i),
                (short)(a.bottom - SQ - 4),
                (short)(a.left + 4 + (SQ + 4) * i + SQ),
                (short)(a.bottom - 4));
        PaintRect(&r);
    }
}

/* ------------------------------------------------------------------ */
/* The Control Panel's calls                                           */
/* ------------------------------------------------------------------ */

pascal long finderdrop_cdev(short message, short item, short numItems,
                            short cPanelID, EventRecord *ev, long cdevValue,
                            DialogPtr dlg)
{
    CdevStateH h;
    CdevState *x;

    (void)item;
    (void)cPanelID;
    (void)ev;

    /* Asked first, before initDev: does this panel run on this machine?
     * Every machine with a Control Panel, as far as FinderDrop goes. */
    if (message == macDev)
        return 1;

    if (message == initDev) {
        h = (CdevStateH)NewHandleClear((Size)sizeof(CdevState));
        if (h == 0)
            return cdevMemErr;
        HLock((Handle)h);               /* pointers into it are kept */
        (*h)->dragMgr = fdrop_have_drag_manager();
        /* FinderDrop: once there is state, so that a failed initDev
         * (which gets no closeDev) leaves no patch behind. */
        fdrop_install((WindowPtr)dlg, (*h)->queue, kDrops, 0);
        return (long)h;                 /* comes back as cdevValue */
    }

    /* After a failed initDev, cdevValue is one of these and there is no
     * state to use; the Control Panel keeps calling regardless. */
    if (cdevValue == cdevMemErr || cdevValue == cdevResErr
        || cdevValue == cdevUnset || cdevValue == cdevGenErr)
        return cdevValue;
    h = (CdevStateH)cdevValue;
    x = *h;

    switch (message) {
    case updateDev:
        draw(x, dlg, numItems);
        break;

    case nulDev:
        /* FinderDrop's time: it commits a drop the Finder has finished,
         * then the items are taken, and the area redrawn if any came. */
        fdrop_idle();
        if (take_drops(x, dlg))
            draw(x, dlg, numItems);
        break;

    case closeDev:
        /* The patches FIRST: they point into this resource, which the
         * Control Panel may throw away the moment this returns. */
        fdrop_remove((WindowPtr)dlg);   /* and only then the queue */
        HUnlock((Handle)h);
        DisposeHandle((Handle)h);
        return cdevUnset;

    default:
        break;
    }
    return cdevValue;
}
