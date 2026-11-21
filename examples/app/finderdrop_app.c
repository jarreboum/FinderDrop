/*
 * finderdrop_app.c — FinderDrop in an application.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * An application that does nothing but be dropped on: drag files,
 * folders or disks from the Finder onto its window and it lists each one.
 * It is the smallest complete use of FinderDrop in an application,
 * written to be read.  It doubles as a debugger: built with
 * -DFDROP_DIAG=1, the window also shows the library's diagnostic words.
 *
 * WHERE AN APPLICATION CAN BE DROPPED ON.  Only where the Finder is still
 * running beside it: under MultiFinder (System 4.2 to 6.0.8), and on
 * System 7.5 and later, or System 7 with Apple's Macintosh Drag and Drop
 * extension, where FinderDrop uses the Drag Manager.  A plain System 7.0
 * or 7.1 sends it nothing.  Without MultiFinder, launching an
 * application quits the Finder, so there is nothing to drag from;
 * FinderDrop then installs nothing, and this window says so.  On System
 * 6 under MultiFinder the Finder comes to the front when its icon is
 * pressed, so drop on the part of this window the Finder's windows leave
 * showing.
 *
 * THE CALLS A HOST MAKES, and where this file makes them:
 *
 *   fdrop_install(window, queue, n, room)  after the window is made (main)
 *   fdrop_idle()            on every pass of the event loop  (main)
 *   fdrop_take_drop(...)    after fdrop_idle, until it says no   (take_drops)
 *   fdrop_remove(window)    before quitting                  (main)
 *   fdrop_requeue(...)      from room(), to make the queue bigger
 *   fdrop_have_drag_manager()   only to say which door is open
 *
 * THE QUEUE IS OURS, and sized by asking the heap.  FinderDrop keeps the
 * drops in memory this file hands it, and a drag of more items than it
 * holds is refused whole.  The queue is a pointer, because it must not
 * move while FinderDrop is installed, made when the window is, as big as
 * a sixteenth of the largest free block allows (queue_size).  Without a
 * Drag Manager that is what one drag can bring.  With one, room() is
 * called when a drag needs more, in this application's own context, and
 * makes a queue big enough for it: as many items as the heap holds.
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
 * TWO THINGS AN APPLICATION MUST DO FOR IT:
 *
 *   - be given time in the background.  A drop arrives while the Finder
 *     is in front, so the application's SIZE resource says canBackground
 *     (finderdrop_app.r): MultiFinder then hands it null events, and the
 *     drop is taken at once rather than when the user next switches in;
 *   - keep FinderDrop in memory.  Its trap patches live in this
 *     application's code, so the segment holding fdrop.c is never
 *     unloaded while they are in.  A one-segment application like this
 *     one unloads nothing.
 *
 * This file has globals, because an application has an A5 world.
 * FinderDrop itself does not use it: fdrop.c and fdrop.S are compiled
 * -mpcrel and keep their state in their own code, because the patches run
 * while the FINDER's A5 world is current.  build.sh says how the two are
 * linked into one CODE resource.
 */
#include <Dialogs.h>
#include <Events.h>
#include <Files.h>
#include <Fonts.h>
#include <Menus.h>
#include <OSUtils.h>
#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>

#include "fdrop.h"

/* Where the window opens: inside a 512 by 342 screen with room left on
 * both sides, so that icons on the desktop stay in reach of a drag. */
#define WIN_L  60
#define WIN_T  60
#define WIN_W  360
#define WIN_H  230
#define LINE_H 11              /* Geneva 9 */
#define SQ     12              /* the squares along the bottom */
#define LINE_BYTES 96          /* one line of text, its length byte too */

static WindowPtr      gWin;
static Boolean        gDragMgr;            /* which door FinderDrop opened */
static unsigned short gDrops;              /* items received so far */
static unsigned short gHead;               /* the next line to overwrite */
static unsigned short gShown;              /* lines holding an item */
static unsigned char  gLine[8][LINE_BYTES]; /* the last eight, shown */
#define kShown 8
static Ptr            gQueue;              /* FinderDrop's, made by us */
static short          gCap;                /* entries it holds */

/*
 * A queue as large as the heap can spare: a sixteenth of the largest
 * free block, in whole entries, at least one and at most kQueueMost.
 * Asked of the heap rather than fixed, so a bigger machine takes bigger
 * drags.  -DkQueueMost=1 starts it at one entry, which is how a test
 * makes room() do the growing.
 */
#ifndef kQueueMost
#define kQueueMost 30000L
#endif

static short queue_size(void)
{
    long room = MaxBlock() / 16 / FDROP_ENTRY_BYTES;

    if (room > (long)kQueueMost)
        room = (long)kQueueMost;
    return (short)(room < 1 ? 1 : room);
}

/*
 * The Drag Manager's door, when a drag needs `needed` entries in all and
 * the queue holds fewer: a bigger one, if the heap has it.  Called from
 * FinderDrop's receive handler, in this application's context, where a
 * NewPtr is fine.  The old queue is freed only once FinderDrop has let go
 * of it.
 */
static Boolean room(WindowPtr onto, short needed)
{
    Ptr   bigger = NewPtr((Size)needed * FDROP_ENTRY_BYTES);
    void *old = 0;

    if (bigger == 0)
        return false;                   /* no room: the drag is refused */
    if (!fdrop_requeue(onto, bigger, needed, &old)) {
        DisposePtr(bigger);
        return false;
    }
    if (old != 0)
        DisposePtr((Ptr)old);
    gQueue = bigger;
    gCap = needed;
    return true;
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

/* Append to a Pascal string, which is what DrawString wants.  Every
 * string this file builds is LINE_BYTES long, and a longer one is cut
 * short: a name of up to 63 bytes and a volume's of up to 27, with the
 * words beside them, can come to 136. */
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

static void put_dec(unsigned char *s, long v)
{
    unsigned char num[16];

    NumToString(v, num);
    put_pstr(s, num);
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
    Str63      nm;
    short      i;

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
 * one becomes a line: what it is, its name, and where it is.
 */
static void take_drops(void)
{
    short         vRefNum;
    long          dirID;
    unsigned char name[64], vol[32];
    Boolean       any = false;

    while (fdrop_take_drop(gWin, &vRefNum, &dirID, name)) {
        unsigned char *l = gLine[gHead];

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

        gHead = (unsigned short)((gHead + 1) % kShown);
        if (gShown < kShown)
            gShown++;
        gDrops++;
        any = true;
    }
    if (any) {
        Rect r = gWin->portRect;

        SetPort(gWin);
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

static void draw(void)
{
    unsigned char s[LINE_BYTES];
    Rect r;
    short i, row = 0;

    r = gWin->portRect;
    EraseRect(&r);
    TextFont(3);                        /* Geneva */
    TextSize(9);

    s[0] = 0;
    if (gDragMgr)
        put_text(s, "Through the Drag Manager.  ");
    else if (fdrop_can_drop(gWin))      /* the library's answer: a Finder
                                           beside this host to follow */
        put_text(s, "Following the Finder's drag.  ");
    else
        put_text(s, "No Finder to drag from: run this under MultiFinder.  ");
    put_text(s, "Received: ");
    put_dec(s, gDrops);
    put_text(s, ", room for ");
    put_dec(s, (unsigned short)gCap);
    line_at(row++, s);
    row++;

    /* The most recent kShown items, oldest first. */
    for (i = 0; i < (short)gShown; i++) {
        short k = (short)((gHead + kShown - gShown + i) % kShown);
        line_at(row++, gLine[k]);
    }

#if FDROP_DIAG
    {
        /* The debugging view: the library's own words, in hex, fourteen
         * to a line.  fdrop.h and fdrop.c say what each one holds. */
        short diag[FDROP_DIAG_WORDS];

        row = (short)(2 + kShown);
        fdrop_diag(diag);
        s[0] = 0;
        for (i = 0; i < FDROP_DIAG_WORDS; i++) {
            if (i > 0 && i % 14 == 0) {
                line_at(row++, s);
                s[0] = 0;
            }
            put_hex(s, (unsigned short)diag[i]);
            put_c(s, ' ');
        }
        line_at(row, s);
    }
#endif

    /* One black square per item along the bottom, so a test harness can
     * count them off a screenshot without reading any text. */
    for (i = 0; i < (short)gDrops && 4 + (SQ + 4) * (i + 1) < WIN_W; i++) {
        SetRect(&r, (short)(4 + (SQ + 4) * i), (short)(WIN_H - SQ - 4),
                (short)(4 + (SQ + 4) * i + SQ), (short)(WIN_H - 4));
        PaintRect(&r);
    }
}

/* ------------------------------------------------------------------ */
/* The application                                                     */
/* ------------------------------------------------------------------ */

/*
 * Not part of using FinderDrop: an Apple menu and a File menu, so that
 * desk accessories can be opened from this application, which under
 * MultiFinder is how an accessory comes to live in this application's
 * layer (Option held), and Quit.
 */
#define kAppleID  128
#define kFileID   129
static MenuHandle gApple;

static void menus(void)
{
    MenuHandle file;

    gApple = NewMenu(kAppleID, "\p\024");
    AppendResMenu(gApple, 'DRVR');
    InsertMenu(gApple, 0);
    file = NewMenu(kFileID, "\pFile");
    AppendMenu(file, "\pQuit/Q");
    InsertMenu(file, 0);
    DrawMenuBar();
}

/* A menu choice; true when it is Quit. */
static Boolean chosen(long choice)
{
    short id = (short)(choice >> 16), item = (short)choice;
    Boolean quit = false;

    if (id == kAppleID) {
        Str255 name;
        GrafPtr save;

        GetPort(&save);
        GetMenuItemText(gApple, item, name);
        OpenDeskAcc(name);
        SetPort(save);
    } else if (id == kFileID && item == 1) {
        quit = true;
    }
    HiliteMenu(0);
    return quit;
}

int main(void)
{
    EventRecord ev;
    Rect        r;
    Boolean     quit = false;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0);
    InitCursor();
    menus();

    SetRect(&r, WIN_L, WIN_T, WIN_L + WIN_W, WIN_T + WIN_H);
    gWin = NewWindow(0, &r, "\pFinderDrop: drop files, folders or disks",
                     true, noGrowDocProc, (WindowPtr)-1L, true, 0);
    SetPort(gWin);

    /* FinderDrop: once there is a window for drops to land on. */
    gDragMgr = fdrop_have_drag_manager();
    gCap = queue_size();
    gQueue = NewPtr((Size)gCap * FDROP_ENTRY_BYTES);
    if (gQueue == 0)
        gCap = 0;                       /* every drag refused, nothing worse */
    fdrop_install(gWin, gQueue, gCap, room);

    /*
     * GetNextEvent and SystemTask rather than WaitNextEvent: every System
     * has them, and MultiFinder gives a canBackground application its
     * background null events through them as well.
     */
    while (!quit) {
        SystemTask();
        if (GetNextEvent(everyEvent, &ev)) {
            switch (ev.what) {
            case mouseDown: {
                WindowPtr w;
                short part = FindWindow(ev.where, &w);

                if (part == inMenuBar)
                    quit = chosen(MenuSelect(ev.where));
                else if (part == inGoAway && w == gWin
                    && TrackGoAway(gWin, ev.where))
                    quit = true;        /* the close box quits */
                else if (part == inDrag && w == gWin)
                    DragWindow(w, ev.where, &qd.screenBits.bounds);
                else if (part == inContent && w != FrontWindow())
                    SelectWindow(w);
                else if (part == inSysWindow)
                    SystemClick(&ev, w);
                break;
            }
            case updateEvt:
                if ((WindowPtr)ev.message == gWin) {
                    BeginUpdate(gWin);
                    draw();
                    EndUpdate(gWin);
                }
                break;
            case keyDown:
                if (ev.modifiers & cmdKey)
                    quit = chosen(MenuKey((char)(ev.message & charCodeMask)));
                break;
            }
        }
        /* FinderDrop's time, then the items: every pass, null event or
         * not, so a drop is taken while the Finder is in front. */
        fdrop_idle();
        take_drops();
    }

    fdrop_remove(gWin);                 /* before the code goes away */
    if (gQueue != 0)
        DisposePtr(gQueue);             /* and the queue after it */
    return 0;
}
