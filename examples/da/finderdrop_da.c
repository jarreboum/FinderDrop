/*
 * finderdrop_da.c — FinderDrop in a desk accessory.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * A desk accessory that does nothing but be dropped on: drag files,
 * folders or disks from the Finder onto its window and it lists each one.
 * It is the smallest complete use of FinderDrop in a DA, written to be
 * read.  It doubles as a debugger: built with -DFDROP_DIAG=1, the window
 * also shows the library's diagnostic words.
 *
 * It runs on every Macintosh from System 1.1 on a 64K ROM to Mac OS 9.
 * Where the system has a Drag Manager (System 7.5, or 7 with Apple's
 * Macintosh Drag and Drop) FinderDrop uses it.  Everywhere else it
 * follows the Finder's own icon drag with two trap patches, except on
 * System 7.0 and 7.1 without that extension, whose Finder it does not
 * read, so nothing arrives there.  This file cannot tell which happens,
 * and does not need to: the calls are the same.
 *
 * THE CALLS A HOST MAKES, and where this file makes them:
 *
 *   fdrop_install(window, queue, n, 0)   when the window opens (da_open)
 *   fdrop_idle()            often: every accRun              (da_control)
 *   fdrop_take_drop(...)    after fdrop_idle, until it says no   (take_drops)
 *   fdrop_refused(window)   after that, to say a drag did not fit
 *   fdrop_remove(window)    when the window closes, FIRST    (da_close),
 *                           or on the goodbye kiss before it (da_control)
 *   fdrop_have_drag_manager()   only to say which door is open
 *
 * THE QUEUE IS OURS.  FinderDrop keeps the drops in memory this file
 * hands it: kDrops entries of FDROP_ENTRY_BYTES, inside the state block.
 * The block is a pointer, so it never moves, which is the one thing
 * FinderDrop asks of it: it writes there from inside the Finder's drag.
 * A drag of more than kDrops items is refused whole and the icons go
 * back.
 *
 * WHAT A DROP IS.  Every item arrives as three things: a volume reference
 * number, a directory ID and a name, an FSSpec in all but type.  A file
 * or a folder is its volume, the directory it is IN, and its name.  A
 * disk is its volume, directory 1 (the parent of every root) and the
 * volume's name.  On an MFS volume (the 400K floppies of Systems 1 to 3)
 * there are no directories, the directory is 0, and a folder the Finder
 * shows there arrives as the files in it.  The Trash arrives as the
 * startup disk's Trash folder where the system keeps one, and is left
 * out where it does not (System 6).  What an item IS, the program asks
 * the File Manager: see what_is() below.
 *
 * WHAT A DESK ACCESSORY MUST NOT DO, and this one does not:
 *
 *   - have globals.  A DA is a DRVR resource with no A5 world, so its
 *     state is a block hung off dCtlStorage (DAState below);
 *   - use absolute addresses.  The resource is loaded wherever the heap
 *     had room and never relocated, so everything is compiled -mpcrel and
 *     build.sh links it twice at two addresses and compares the bytes;
 *   - keep the trap patches past close.  They point into this resource,
 *     which the Desk Manager throws away when the DA closes, so
 *     fdrop_remove comes before anything else in da_close;
 *   - keep them past its heap.  Under MultiFinder, a DA opened with
 *     Option over an application lives in that application's heap, and
 *     when the application quits its Close never runs: what comes is the
 *     goodbye kiss, a Control call with csCode -1, which the header asks
 *     for (dNeedGoodbye) and da_control answers with fdrop_remove.
 *     Without MultiFinder, launching an application closes the DA first
 *     and no goodbye comes.
 *
 * The header (the DRVR's flags, its five entry points and its name) is
 * da_header.S.  build.sh builds FinderDrop.drvr; put it in a System
 * file as a DRVR (Font/DA Mover, or ResEdit) and choose "FinderDrop" from
 * the Apple menu.
 */
#include <Multiverse.h>
#include "fdrop.h"

/* Where the window opens and how big it is. */
#define WIN_L   40
#define WIN_T   60
#define WIN_W   368
#define WIN_H   220
#define LINE_H  11              /* Geneva 9 */
#define SQ      12              /* the squares along the bottom */

/* How many items one drag may bring, and how many lines are shown.
 * -DkDrops=1 builds an accessory that takes one file at a time. */
#ifndef kDrops
#define kDrops  8
#endif

/* The two Control calls the Desk Manager makes. */
#define accEvent  64            /* an event for our window */
#define accRun    65            /* periodic time, every drvrDelay ticks */
#define goodBye   (-1)          /* the heap we are in is about to go */

/* One line of text, its length byte included. */
#define LINE_BYTES 80

/* Everything the DA keeps, in a block of its own (no globals). */
typedef struct {
    WindowPtr      win;
    Boolean        dragMgr;          /* which door FinderDrop opened */
    unsigned short drops;            /* items received since it opened */
    unsigned short refused;          /* drags refused for being too many */
    unsigned short head;             /* the next line to overwrite */
    unsigned short shown;            /* how many lines hold an item */
    Boolean        removed;          /* fdrop_remove has been called */
    unsigned char  line[kDrops][LINE_BYTES];
    unsigned char  queue[kDrops * FDROP_ENTRY_BYTES];   /* FinderDrop's */
#if FDROP_DIAG
    short          diag[FDROP_DIAG_WORDS];
#endif
} DAState;

short da_open(void *pb, DCtlPtr dce);
short da_nothing(void *pb, DCtlPtr dce);
short da_control(CntrlParam *pb, DCtlPtr dce);
short da_close(void *pb, DCtlPtr dce);

/* ------------------------------------------------------------------ */
/* Text, without a C library                                           */
/* ------------------------------------------------------------------ */

/* Append to a Pascal string, which is what DrawString wants.  Every
 * string this file builds is LINE_BYTES long, and a longer one is cut
 * short: a name of up to 63 bytes and a volume's of up to 27, with the
 * words beside them, can come to 136, and the last line sits directly
 * in front of FinderDrop's queue in DAState. */
static void put_c(unsigned char *s, char c)
{
    if (s[0] < LINE_BYTES - 1)
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
 * everything is a file (and where PBGetCatInfo, an HFS call, must not be
 * made: the 64K ROM does not have it).  Anything else is asked about.
 */
static const char *what_is(short vRefNum, long dirID, const unsigned char *name)
{
#if FDROP_OWN_ROUTE || FDROP_RECEIVER
    CInfoPBRec ci;
    unsigned char nm[64];
    short i;
#endif

    if (dirID == 1L)
        return "disk";
#if !(FDROP_OWN_ROUTE || FDROP_RECEIVER)
    /* Built for the Finders that drag through the ROM alone: everything
     * that is not a disk comes off an MFS volume, where it is a file, and
     * the catalog call below is one this build can never need. */
    (void)vRefNum;
    (void)name;
    return "file";
#else
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
#endif
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
 * one becomes a line: what it is, its name, and where it is.
 */
static void take_drops(DAState *x)
{
    short         vRefNum;
    long          dirID;
    unsigned char name[64], vol[32];
    Boolean       any = false;

    while (fdrop_take_drop(x->win, &vRefNum, &dirID, name)) {
        unsigned char *l = x->line[x->head];

        l[0] = 0;
        put_text(l, what_is(vRefNum, dirID, name));
        put_text(l, "  ");
        put_pstr(l, name);
        put_text(l, "   on ");
        volume_name(vRefNum, vol);
        put_pstr(l, vol);
        put_text(l, " (");
        put_dec(l, vRefNum);
        put_text(l, "), directory ");
        put_dec(l, dirID);

        /* Wrap without %: a division is a libgcc call (see put_dec). */
        x->head = (unsigned short)(x->head + 1 == kDrops
                                   ? 0 : x->head + 1);
        if (x->shown < kDrops)
            x->shown++;
        x->drops++;
        any = true;
    }
    if (fdrop_refused(x->win)) {        /* a drag of more than kDrops */
        x->refused++;
        any = true;
    }
    if (any) {
        Rect r = x->win->portRect;
        InvalRect(&r);                  /* drawn at the next update */
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

static void line_at(short row, const unsigned char *s)
{
    MoveTo(6, (short)(4 + LINE_H * (row + 1)));
    DrawString(s);
}

#if FDROP_DIAG
/*
 * The debugging view: the library's own words, in hex, fourteen to a
 * line.  fdrop.h and fdrop.c say what each one holds.  Some change every
 * time the idle Finder asks where the mouse is, so this is redrawn on
 * its own: each line is drawn over the last in srcCopy and only the rest
 * of its row erased, so the window does not flicker.
 */
static void draw_diag(DAState *x)
{
    unsigned char s[LINE_BYTES];
    short i, row = (short)(2 + kDrops);
    Point pen;
    Rect r;

    TextFont(3);
    TextSize(9);
    TextMode(srcCopy);
    fdrop_diag(x->diag);
    s[0] = 0;
    for (i = 0; i <= FDROP_DIAG_WORDS; i++) {
        if (i == FDROP_DIAG_WORDS || (i > 0 && i % 14 == 0)) {
            line_at(row, s);
            GetPen(&pen);
            SetRect(&r, pen.h, (short)(pen.v - LINE_H + 2), WIN_W,
                    (short)(pen.v + 2));
            EraseRect(&r);
            row++;
            s[0] = 0;
            if (i == FDROP_DIAG_WORDS)
                break;
        }
        put_hex(s, (unsigned short)x->diag[i]);
        put_c(s, ' ');
    }
    TextMode(srcOr);
}
#endif

static void draw(DAState *x)
{
    unsigned char s[LINE_BYTES];
    Rect r;
    short i, row = 0;

    TextFont(3);                        /* Geneva */
    TextSize(9);
    r = x->win->portRect;
    EraseRect(&r);

    s[0] = 0;
#if FDROP_RECEIVER
    put_text(s, x->dragMgr ? "Through the Drag Manager.  "
                           : "Following the Finder's drag.  ");
#else   /* no Drag Manager door in this build, whatever the machine has */
    put_text(s, "Following the Finder's drag.  ");
#endif
    put_text(s, "Received: ");
    put_dec(s, x->drops);
    if (x->refused) {
        put_text(s, "   Refused, too many: ");
        put_dec(s, x->refused);
    }
    line_at(row++, s);
    row++;

    /* The most recent kDrops items, oldest first. */
    for (i = 0; i < (short)x->shown; i++) {
        short k = (short)(x->head - x->shown + i);

        if (k < 0)
            k = (short)(k + kDrops);
        line_at(row++, x->line[k]);
    }

#if FDROP_DIAG
    draw_diag(x);
#endif

    /* One black square per item along the bottom, so a test harness can
     * count them off a screenshot without reading any text. */
    for (i = 0; i < (short)x->drops && 4 + (SQ + 4) * (i + 1) < WIN_W; i++) {
        SetRect(&r, (short)(4 + (SQ + 4) * i), (short)(WIN_H - SQ - 4),
                (short)(4 + (SQ + 4) * i + SQ), (short)(WIN_H - 4));
        PaintRect(&r);
    }
}

/* ------------------------------------------------------------------ */
/* The driver's entry points, called from da_header.S                  */
/* ------------------------------------------------------------------ */

short da_open(void *pb, DCtlPtr dce)
{
    DAState *x;
    Rect r;
    unsigned char title[LINE_BYTES];
    unsigned long i;

    (void)pb;
    if (dce->dCtlWindow) {              /* already open: to the front */
        SelectWindow(dce->dCtlWindow);
        return noErr;
    }
    /* The state block: application heap, as a DA's usually is. */
    x = (DAState *)NewPtr((Size)sizeof(DAState));
    if (!x)
        return memFullErr;
    for (i = 0; i < sizeof(DAState); i++)
        ((unsigned char *)x)[i] = 0;

    title[0] = 0;
    put_text(title, "FinderDrop");
    SetRect(&r, WIN_L, WIN_T, WIN_L + WIN_W, WIN_T + WIN_H);
    x->win = NewWindow((void *)0, &r, title, true, noGrowDocProc,
                       (WindowPtr)-1L, true, 0);
    if (!x->win) {
        DisposePtr((Ptr)x);
        return memFullErr;
    }
    /* A DA's window says whose it is, so the Desk Manager sends us its
     * events. */
    ((WindowPeek)x->win)->windowKind = dce->dCtlRefNum;
    dce->dCtlStorage = (Handle)x;
    dce->dCtlWindow = x->win;

    /* FinderDrop: once there is a window for drops to land on. */
#if FDROP_RECEIVER
    x->dragMgr = fdrop_have_drag_manager();
#endif
    fdrop_install(x->win, x->queue, kDrops, 0);
    return noErr;
}

short da_nothing(void *pb, DCtlPtr dce)
{
    (void)pb;
    (void)dce;
    return noErr;
}

/* fdrop_remove once, whichever of goodbye and Close comes first. */
static void unhook(DAState *x)
{
    if (!x->removed) {
        fdrop_remove(x->win);
        x->removed = true;
    }
}

short da_control(CntrlParam *pb, DCtlPtr dce)
{
    DAState *x = (DAState *)dce->dCtlStorage;
    GrafPtr  save;

    if (!x)
        return noErr;
    if (pb->csCode == goodBye) {
        /*
         * The heap our state, window and queue are in is about to go,
         * and Close may never run: under MultiFinder, an accessory
         * opened with Option over an application is in that
         * application's heap, and when it quits this call is all that
         * comes.  So the patches go now.  Nothing is disposed of (the
         * heap is going anyway), and the window is still drawn if asked;
         * a Close that does come finds the patches gone.
         */
        unhook(x);
        return noErr;
    }
    GetPort(&save);
    SetPort(x->win);
    if (pb->csCode == accEvent) {
        EventRecord *ev;

        BlockMove((Ptr)&pb->csParam[0], (Ptr)&ev, sizeof ev);
        if (ev->what == updateEvt) {
            BeginUpdate(x->win);
            draw(x);
            EndUpdate(x->win);
        }
    } else if (pb->csCode == accRun && !x->removed) {
        /*
         * FinderDrop's time: it commits a drop the Finder has finished,
         * then the items are taken.  accRun comes every drvrDelay ticks
         * (da_header.S) whichever application is in front.
         */
        fdrop_idle();
        take_drops(x);
#if FDROP_DIAG
        {
            /* In the debugging build, redraw the words whenever one
             * changes, so what the window shows is never a copy from an
             * older update. */
            short now[FDROP_DIAG_WORDS];
            short i;

            fdrop_diag(now);
            for (i = 0; i < FDROP_DIAG_WORDS; i++)
                if (now[i] != x->diag[i]) {
                    draw_diag(x);
                    break;
                }
        }
#endif
    }
    SetPort(save);
    return noErr;
}

short da_close(void *pb, DCtlPtr dce)
{
    DAState *x = (DAState *)dce->dCtlStorage;

    (void)pb;
    if (x) {
        unhook(x);                      /* FIRST: the patches point here */
        DisposeWindow(x->win);
        DisposePtr((Ptr)x);             /* and the queue in it, after */
    }
    dce->dCtlWindow = (WindowPtr)0;
    dce->dCtlStorage = (Handle)0;
    return noErr;
}
