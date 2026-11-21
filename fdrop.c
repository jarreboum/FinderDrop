/*
 * fdrop.c — FinderDrop: files, folders and disks dragged out of the
 * Finder onto a program's own window, with or without a Drag Manager.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * ---- what this is for ----
 *
 * A program (a desk accessory, a control panel or an application) wants
 * files dragged onto its window from the Finder.  Where the system
 * has a Drag Manager, the program registers a receive handler and the
 * Finder does the sending.  System 6 and earlier have no Drag Manager,
 * and 7.0 and 7.1 have one only with Apple's extension installed.  On
 * those machines this library is the sender.
 *
 * `fdrop_install` asks `Gestalt('drag')` and, where the answer is yes,
 * patches nothing.  The capability is probed, never inferred from the
 * System version: a stock System 7.0.1 has no Drag Manager.
 *
 * ---- the host window ----
 *
 * "The host window", here and below, is the one window of the program
 * that files are dropped on: the window passed to fdrop_install.  "The
 * host" is the program that owns it.
 *
 * ---- how it works ----
 *
 * Two trap patches, and neither takes the mouse: a drag of a Finder icon
 * stays the Finder's from the press to the release.
 *
 *   _FindWindow recognises the Finder's own drag of an icon, from inside
 *   it.
 *
 *   _GetMouse is answered, for that drag alone, while the pointer is over
 *   the host window, so that a release there is a drag the Finder puts
 *   back where it started ("A Finder drag that ends on the host window",
 *   below).
 *
 * The files the drag carries are read from the Finder's own records of
 * the icons it draws: found by walking its heap block by block, tied to
 * the window or the desktop they are drawn in by the Finder's own handle,
 * and every name checked with the File Manager before it is believed
 * ("The Finder's records", below).
 *
 * The evidence for every constant is in NOTES.md; the System 7
 * sender that could not be made to work is in PARKED.md.
 *
 * ---- the rules this code obeys ----
 *
 * It is built into code resources (a desk accessory, a control panel),
 * so: no globals, no absolute addresses, nothing in .data or .bss.
 * Every piece of state lives in the code resource itself, in fdrop.S,
 * addressed PC-relative.  That is also the only way a trap patch can
 * reach it, since a patch runs in whatever A5 world the trapping code
 * had.
 *
 * Code that runs inside the patches (fdrop_look, fdrop_answer and all
 * they call) runs in the middle of the Finder's drag.  It allocates
 * nothing, and asks the File Manager nothing that could put up the
 * disk-switch alert (vol_online).
 *
 * The 68000 faults on a misaligned word or long, so what is read out of
 * the Finder's heap or a trampoline is read byte-wise (blob_long,
 * rec_word).
 */

#include <Devices.h>
#include <Dialogs.h>
#include <Errors.h>
#include <Events.h>
#include <Files.h>
#include <MacMemory.h>
#include <Quickdraw.h>
#include <Windows.h>

#include "fdrop.h"

/*
 * ---- what is built ----
 *
 * Three switches in fdrop.h, one for each way a drop arrives:
 *
 *   FDROP_ROM_ROUTE   the sender, for the Finders of Systems 1.1 to 3.2;
 *   FDROP_OWN_ROUTE   the sender, for the Finders of Systems 4.0 to 6.0.8;
 *   FDROP_RECEIVER    the Drag Manager's door.
 *
 * FDROP_SENDER is worked out in fdrop.h from the first two.  It covers
 * the trap patches, the trampoline, the reading of the Finder's heap and
 * the checking of an MFS volume's files, which either route needs and
 * nothing else does.  A program that leaves
 * both routes out patches nothing.
 *
 * Everything a switch leaves out is `#if`-ed out by hand, here and in
 * fdrop.S, rather than left for `--gc-sections`.  A static function that
 * nothing calls stops a -Werror build, and a host linked without
 * `--gc-sections` (the example application) would otherwise carry code
 * it can never run.
 */

#define kWindowListAddr  0x09D6L        /* WindowList */


/* ------------------------------------------------------------------ */
/* The trap patches                                                    */
/* ------------------------------------------------------------------ */

/*
 * fdrop.S holds the two patches and every byte of their state.  This
 * section turns them on and off.
 *
 * THE LIFETIME IS THE SAFETY ARGUMENT.  The patches are installed when
 * the host window opens and removed when it closes, so nothing points
 * into this code resource while it is not loaded and locked.  A patch
 * that outlived its code would crash at the next mouse movement.
 * "Nothing" includes a patch somebody else put on top of ours, which is
 * why the trap table points at a trampoline in the system heap and not
 * at this resource (patch_install).
 *
 * _FindWindow is how the Finder's drag of an icon is recognised, and
 * _GetMouse is how that drag is made to end where it began when the
 * pointer is over the host window (fdrop.S, and "A Finder drag that ends
 * on the host window" below).
 *
 * ---- written into the trap table, not passed to SetTrapAddress ----
 *
 * The patches must be live while the FINDER runs, because it is the
 * Finder's drag they answer for, and under MultiFinder the host is not
 * running in the Finder's layer.  MultiFinder's patch on _SetTrapAddress
 * records every call an application layer makes, and takes the patch out
 * whenever that layer is switched out: installed that way, the sender
 * would never see a Finder drag (NOTES.md, "Under MultiFinder a trap
 * patch belongs to the layer that called SetTrapAddress, wherever its
 * code is").  Written straight into the table, the entries stay in every
 * layer.
 *
 * The same two stores are made with or without MultiFinder, so there is
 * one path.  The patch then runs in every application's _GetMouse, and
 * costs one comparison of A6 there (fdrop.S).
 *
 * WHICH table is asked of the machine, not assumed.  A 128K ROM keeps
 * 512 Toolbox entries at 0x0C00; later ROMs keep 1024 at 0x0E00.  The
 * table written is the one whose entries for BOTH traps hold exactly
 * what GetToolTrapAddress answers for them; that cannot happen by
 * accident.  Where neither table agrees, or both do, there is no sender,
 * and the host's own Open command is the way in, as on System 7.
 *
 * The patches are declared as BYTES rather than as functions, so that
 * nothing here converts a function pointer to an object pointer, which
 * -pedantic refuses.  C never calls them; only their addresses are
 * written into the trap table.
 */
#define kFindWindow   FDROP_TRAP_FINDWINDOW
#define kGetMouse     FDROP_TRAP_GETMOUSE

extern short         FDROP_ENABLE;
extern unsigned char FDROP_FW[];
extern unsigned long FDROP_FW_OLD;
extern unsigned char FDROP_GM[];
extern unsigned long FDROP_GM_OLD;
extern long          FDROP_TOOLTAB;
extern long          FDROP_TRAMP;
extern unsigned short FDROP_ROM64_FWW;
extern unsigned short FDROP_ROM64_GMW;
extern long          FDROP_ROMDRAG;
extern long          FDROP_ROMWRAP;
extern short         FDROP_ROMMODE;
extern long          FDROP_QUEUE;    /* the host's queue (fdrop_install) */
extern long          FDROP_ROOM;     /* its room hook, or 0 */
extern short         FDROP_QCAP;     /* entries the queue holds */
extern short         FDROP_QN;       /* entries waiting in it */
extern short         FDROP_REFUSED;  /* a drag refused for room, unasked */
extern short         FDROP_OVERFLOW; /* the sender ran out of room, this drag */
extern long          FDROP_WIN;      /* the window drops are kept for */

/* The Finder drag being followed: fdrop.S says what each one is. */
extern long          FDROP_DRAGFP;
extern long          FDROP_DRAGRET;
extern long          FDROP_START;
extern long          FDROP_GEST;
extern long          FDROP_FWRET;
extern long          FDROP_OUTSITE;
extern long          FDROP_LASTGM;
extern short         FDROP_FAKED;
extern short         FDROP_ACT;
extern short         FDROP_OVER;
extern short         FDROP_FIN;
extern short         FDROP_STATE;
extern short         FDROP_PEND;
extern short         FDROP_PENDAT;

/*
 * What the last drag carried and how far it got.  Nothing inside a trap
 * patch may print, and by the time anything can ask, the drag is over,
 * so these words are the only view into one.  fdrop_diag reads them.
 */
extern short         FDROP_LASTN;
extern short         FDROP_SEQ;
#if FDROP_DIAG
extern short         FDROP_STEP;
#define STEP(v)   (FDROP_STEP = (short)(v))
#else   /* the step is a diagnostic: what branches on one is handed back */
#define STEP(v)   ((void)0)
#endif

/*
 * ---- how far a drag got: FDROP_STEP ----
 *
 * fdrop_diag's fifth word is a hundred times the number of Finder drags
 * recognised (FDROP_SEQ), plus FDROP_STEP: how far the last one got, or
 * why it was refused.  The values are fixed: a host shows them on the
 * screen and NOTES.md cites them by number.
 *
 * The step is kept in an FDROP_DIAG build only; STEP() is nothing in any
 * other.  The one value the code itself acts on, kStepNoRoom, is not
 * read back from the word: resolve_drag and resolve_rom hand their step
 * back, and their caller sets FDROP_OVERFLOW from that.
 */
enum {
    /* The route through the Finder's own drag routine (Finders 5.4 to
       6.1.8): resolve_drag and fdrop_look. */
    kStepRecognised         = 1,    /* a Finder drag recognised */
    kStepFilesFound         = 4,    /* the files it carries found */
    kStepDropTaken          = 5,    /* released over the host window */
    kStepNothingToHand      = 85,   /* nothing to hand over: a Trash with no
                                       folder behind it, dragged alone */
    kStepOneItemTwice       = 86,   /* one item in two records */
    kStepSelectedElsewhere  = 87,   /* something selected in another window,
                                       or on the desktop */
    kStepNotTakeable        = 88,   /* a selected icon that is not a visible
                                       file or folder where its record says,
                                       nor a disk the File Manager knows by
                                       that name */
    kStepHeapUnwalkable     = 90,   /* the Finder's heap not walkable */
    kStepNotInFinderWindow  = 91,   /* not pressed in a Finder window or on
                                       the desktop */
    kStepFolderUnnamed      = 96,   /* the window's record names no folder */
    kStepNotOneIcon         = 97,   /* not exactly one icon under the press */
    kStepIconNotSelected    = 98,   /* the pressed icon not selected */
    kStepNoRoom             = 99,   /* more than the queue has room for
                                       (both routes) */

    /* A file or folder of an MFS volume, on either route (mfs_take,
       mfs_folder). */
    kStepMfsFileUnknown     = 66,   /* a file the File Manager disagrees on */
    kStepMfsTooDeep         = 69,   /* folders nested deeper than
                                       kFolderDepth */
    kStepNameTooLong        = 70,   /* a name longer than kNameMax */

    /* The route through the ROM's DragTheRgn (Finders 1.1g to 5.3). */
    kStepRomArmed           = 60,   /* a ROM drag being followed */
    kStepRomDeskNoIcon      = 62,   /* a press on the desktop, on no icon */
    kStepRomNotFinderWindow = 63,   /* not pressed in a Finder window */
    kStepRomNothingSelected = 64,   /* nothing selected there */
    kStepRomItemsFound      = 65,   /* its items found */
    kStepRomUnknownIcon     = 67,   /* a selected icon that is none of a file,
                                       a folder, a disk or the Trash */
    kStepRomUnknownDisk     = 68    /* a disk the File Manager does not know */
};

/*
 * FDROP_STATE: what the drag being followed carries.
 */
enum {
    kDragUnread  = 0,           /* not looked at yet */
    kDragCarries = 1,           /* files found and written past the depth */
    kDragRefused = 2,           /* refused: the Finder keeps the drag */
    kDragNotIcons = 3           /* ROM route: not a drag of Finder icons */
};

/*
 * FDROP_FIN: whether the drag's final _GetMouse has been seen.
 */
enum {
    kFinNotYet   = 0,
    kFinSeen     = 1,           /* the final call: the button is up */
    kFinBadShape = 2            /* a call that does not fit the drag's
                                   shape: not answered again */
};

/*
 * ---- the diagnostic words: FDROP_DG, FDROP_DIAG builds only ----
 *
 * What a drag saw on its way through, read back by fdrop_diag after
 * fdrop_diag's own eleven words.  An ordinary build has none of them.
 * Each index is fixed, for the same reason as the steps; the unnamed
 * indices are unused and stay unused.
 */
enum {
    /* resolve_drag */
    kDgPressV       = 0,    /* the press, in the coordinates its icon is */
    kDgPressH       = 1,    /*   drawn in (the window's, or global) */
    kDgRecords      = 2,    /* the Finder records the heap walk found */
    kDgHits         = 3,    /* how many were under the press */
    kDgRefName      = 4,    /* at kStepNotTakeable, the icon refused: its */
    kDgRefParent    = 5,    /*   name's first two characters, its parent, */
    kDgRefVol       = 6,    /*   its vRefNum, and which test it failed */
    kDgRefTest      = 7,    /*   (kTake...) */
    kDgDecision     = 10,   /* at the end of a drag that found files: ten
                               times "the Finder's target was the source
                               window" plus "released over the host
                               window"; 11 is a drop taken */
    kDgGetMouse     = 11,   /* _GetMouse calls answered */
    kDgFolder       = 12,   /* the folder and volume the pressed icon's */
    kDgVolume       = 13,   /*   files were looked for in */
    kDgStkRoom      = 16,   /* FDROP_STK's two numbers, two words each */
    kDgStkLow       = 18,
    kDgEnd          = 20,   /* how the drag ended: 1 its final _GetMouse
                               seen, +10 if the button still read as held,
                               +100 if released over the host window; or 2,
                               a call that did not fit the shape */
    kDgOutlineSite  = 21,   /* where the outline's and the final _GetMouse */
    kDgFinalSite    = 22,   /*   return to, from the action procedure */
    kDgCatFlags     = 23,   /* the catalog's fdFlags for the last icon
                               asked about, or the error */

    /* the Drag Manager door: a drag that does nothing looks the same
       whether the handlers were refused or the Finder never sent one */
    kDgTrackInstall = 24,   /* InstallTrackingHandler's answer, 1 for noErr */
    kDgRecvInstall  = 25,   /* InstallReceiveHandler's answer, 1 for noErr */
    kDgTrackCalls   = 26,   /* calls of the tracking handler */
    kDgRecvCalls    = 27,   /* calls of the receive handler */
    kDgDragItems    = 28,   /* 1000 + CountDragItems' answer, or the error */
    kDgFlavorSize   = 29,   /* 1000 + the last 'hfs ' flavor's size, or the
                               error */

    kDgOverWay      = 40,   /* how the pointer was last found over the host
                               window: 1 the window list, 2 the visible
                               region (over_panel) */

    /* finder_drag_call: every _FindWindow call, and which test turned it
       away */
    kDgFwCalls      = 41,
    kDgFwOdd        = 42,   /* an odd return address or A6 */
    kDgFwNotHeap    = 43,   /* returning outside the Finder's heap */
    kDgFwNotStack   = 44,   /* a frame not on the stack */
    kDgFwNotSite    = 45,   /* not the call site */
    kDgFwNotProc    = 46,   /* the action procedure not just before it */
    kDgFwSiteDist   = 47,   /* the last site's distance from its procedure */
    kDgFwA6         = 48,   /* at kDgFwNotStack, A6 and the two bounds it */
    kDgFwHeapEnd    = 49,   /*   was tested against, each shifted right */
    kDgFwStackBase  = 50,   /*   by 4 */
    kDgFwEntries    = 51,   /* fdrop.S: every entry to the _FindWindow patch */
    kDgGmEntries    = 52,   /* fdrop.S: every entry to the _GetMouse patch */

    /* fdrop_install */
    kDgInstall      = 53,   /* 1 patched, +2 through the 64K ROM's word
                               table, +4 the ROM's DragTheRgn found; or 8,
                               no Finder to drag from */
    kDgTramp        = 54,   /* the trampoline's address, two words */
    kDgDragTheRgn   = 56,   /* _DragTheRgn's address, two words */
    kDgTrampSys     = 58,   /* 64K ROM: where each heap offered a */
    kDgTrampApp     = 59,   /*   trampoline, shifted right by 4 */
    kDgSites        = 60    /* fdrop_dg_site: four _GetMouse call sites,
                               three words each, to 71 */
};

#if FDROP_DIAG
extern short         FDROP_DG[];
#define kDgWords  FDROP_DG_WORDS
#define DG(i, v)  (FDROP_DG[(i)] = (short)(v))
#define DGINC(i)  (FDROP_DG[(i)] = (short)(FDROP_DG[(i)] + 1))
#else
#define DG(i, v)  ((void)0)
#define DGINC(i)  ((void)0)
#endif

#define kTrapIndex(t)   FDROP_TRAP_INDEX(t)

#if FDROP_SENDER      /* to the end of patch_remove: the sender's alone */
/* The Toolbox dispatch table, or 0 if the machine does not say which, is
 * fdrop.h's fdrop_probe_table, which a host's own probe uses too. */

/*
 * THE 64K ROM is a third kind of dispatch: one table of 512 WORD offsets
 * at 0x0400, not the longword tables of later ROMs.  A ROM entry is
 * (addr - ROM base) / 2, the ROM base at 0x02AE; a RAM entry is
 * (addr - RAM base) / 2 with bit 15 set, the RAM base at 0x02B2.  A
 * longword written there would corrupt two entries.  So addresses are
 * read with _GetTrapAddress ($A146), and entries are written as words
 * encoded here (rom64_encode).  FDROP_TOOLTAB holds kToolTableRom64 to
 * say this is the table patched.
 */
#define kToolTableRom64  1L     /* odd: never a real table address */
#define kRom64Table      0x0400L
#define kRom64RomBase    0x02AEL /* holds the ROM base, 0x400000      */
#define kRom64RamBase    0x02B2L /* holds the base a RAM entry counts */
                                 /* from                              */

/* FDROP_ROM64K (fdrop.h): from here to rom64_encode, the first half of
 * patch_install and the first branch of patch_remove are that ROM's
 * alone.  A host that never runs on it builds without them. */
#if FDROP_ROM64K
static unsigned long trap_addr64(unsigned short trap)
{
    register unsigned short d0 __asm__("d0") = trap;
    register unsigned long  a0 __asm__("a0");

    /* _GetTrapAddress.  An OS trap returns its result in D0, so D0 is an
       output as well as an input: declared as an input only, the
       compiler could assume it still holds `trap` afterwards.  D1, D2
       and A1 are an OS trap's to trash (fdrop.h), and the 64K ROM's
       routine goes through a RAM vector that may be patched. */
    __asm__ volatile(".short 0xA146" : "+d"(d0), "=a"(a0) : : "d1", "d2", "a1", "cc");
    return a0;
}

static unsigned short *rom64_slot(unsigned short trap)
{
    return (unsigned short *)(kRom64Table + ((long)(trap & 0x1FF) << 1));
}

/*
 * Whether this is the 64K ROM's word-offset table: decode a trap's entry
 * at 0x0400 as the ROM decodes a ROM entry, and require it to be the
 * address the trap itself reports.  On any other ROM the word decodes to
 * something else, so this cannot pass by accident.  One trap whose entry
 * still points into the ROM is proof enough of the table, so three are
 * tried: _GetMouse may be patched in RAM by another program, or by
 * another copy of this library, and the patch then chains to it.
 */
static Boolean rom64_is_rom(unsigned short trap)
{
    unsigned long rombase = *(unsigned long *)kRom64RomBase;
    unsigned long entry = (unsigned long)*rom64_slot(trap);

    return (Boolean)((entry << 1) + rombase == trap_addr64(trap));
}

static Boolean rom64_present(void)
{
    return (Boolean)(rom64_is_rom(kGetMouse) || rom64_is_rom(kFindWindow)
                     || rom64_is_rom(0xA974));      /* _Button */
}

/*
 * A RAM address encoded as the 64K ROM encodes a patched trap: bit 15
 * set, and the low 15 bits the offset from the RAM base (0x02B2) halved.
 * The dispatcher recovers the address as those 15 bits doubled plus the
 * RAM base.  The reach is 64 KB above the RAM base; an address
 * outside it has no encoding, and `*ok` comes back false so the caller
 * can refuse.
 */
static unsigned short rom64_encode(unsigned long addr, Boolean *ok)
{
    unsigned long base = *(unsigned long *)kRom64RamBase;
    unsigned long off = addr - base;

    *ok = (Boolean)(addr >= base && off < 0x10000UL && (off & 1UL) == 0UL);
    return (unsigned short)(0x8000U | (unsigned short)((off >> 1) & 0x7FFFUL));
}
#endif  /* FDROP_ROM64K */

/* Whether a Finder can be running beside this host is fdrop.h's
 * fdrop_finder_alongside. */

/*
 * THE TRAMPOLINE, on every ROM: a block of two `jmp.l`, _FindWindow's
 * then _GetMouse's.  The trap table points at it rather than at the
 * patches.  On the 64K ROM it is needed for reach (patch_install); on
 * the others it lets the patch be TAKEN OUT when something else patched
 * the same trap after us.
 *
 * That something has saved OUR address as the one it chains to, and may
 * still be installed when this code resource goes away: another host of
 * this library (each copy keeps its own state, so the twice-install
 * guard cannot see the other), or any program that patches either trap.
 * Writing our saved address back into the table would cut it off, and
 * its next chained call would land in freed memory.  So at removal:
 *
 *   a trap whose table entry is still ours is put back as it was;
 *
 *   a trap whose entry is not has its `jmp` pointed at what we replaced,
 *   and the block stays in the chain as a PASS-THROUGH, which the next
 *   copy to install reuses (tramp_reusable).
 *
 * The block is in the system heap, so that it outlives the application
 * heap the host was opened in.  On the 64K ROM it may have to come from
 * the application heap for reach; a pass-through left there lasts only
 * until that heap is reused at the next launch.  That case is not
 * covered.
 */
#if FDROP_ROMPLUS
#define kTrampBytes 28          /* two jmp.l, then what follows */
#elif FDROP_PATCH_FW
#define kTrampBytes 12          /* two jmp.l, and nothing reuses it */
#else
#define kTrampBytes 6           /* _GetMouse's jmp.l alone */
#endif
#if FDROP_PATCH_FW
#define kTrampGM    6           /* _GetMouse's jump, after _FindWindow's */
#else
#define kTrampGM    0
#endif
#define kTrampSig   0x46447472L /* 'FDtr' */
#define kTrampAtSig    12       /* where the block keeps kTrampSig, */
#define kTrampAtSelf   16       /* its own address,                 */
#define kTrampAtFwNext 20       /* what _FindWindow's jump chains to */
#define kTrampAtGmNext 24       /* and what _GetMouse's chains to    */
#define kTrapHWPriv 0xA198      /* an OS trap: the caches */
#define kTrapUnimpl 0xA89F      /* _Unimplemented */
#define kTrapOSDispatch 0xA88F  /* implemented only under MultiFinder */
#define kTrapDragTheRgn 0xA926

static void set_blob_long(unsigned char *p, long v);   /* below */
static short rec_word(const unsigned char *p);         /* below */
static long blob_long(const unsigned char *p);         /* below */

#if FDROP_ROM64K || FDROP_ROMPLUS       /* a sender built for some ROM */
static void tramp_write(unsigned char *at, unsigned long to)
{
    at[0] = 0x4E;                       /* jmp.l to */
    at[1] = 0xF9;
    set_blob_long(at + 2, (long)to);
}
#endif

/*
 * FDROP_ROMPLUS (fdrop.h): the cache flush below, the reuse of a
 * trampoline, the second half of patch_install and the second branch of
 * patch_remove are for the Plus ROM and every later one: the longword
 * trap table, MultiFinder, a second copy beside this one, a processor
 * with a cache.  None of them exists on the 64K ROM.
 */
#if FDROP_ROMPLUS
/*
 * The trampoline is code written as data: a 68040's copyback data cache
 * may still hold it, and its instruction cache what was there before.
 * So flush both with _HWPriv 3 then 1, where the trap exists.
 *
 * A machine without _HWPriv can still have a cache.  A Macintosh II
 * below System 6.0.5 is covered because its ROM's _GetTrapAddress
 * flushes the instruction cache, and the probe below calls it.  A
 * 68000-ROM machine with an accelerator below 6.0.5 is not covered
 * (NOTES.md, "A machine without `_HWPriv` can still have a cache").
 */
static void tramp_flush(void)
{
    if (GetOSTrapAddress(kTrapHWPriv) != GetToolTrapAddress(kTrapUnimpl)) {
        HWPriv(3L, 0L);                 /* FlushDataCache */
        HWPriv(1L, 0L);                 /* FlushInstructionCache */
    }
}
#endif  /* FDROP_ROMPLUS */

/*
 * ---- a trampoline is REUSED, and marks itself so it can be ----
 *
 * Under MultiFinder a block is never freed (patch_remove), so a host
 * window opened and closed fifty times would leave fifty.  Instead the
 * block says what it is, and fdrop_install looks for one before making
 * one.  The search starts at the trap table's _GetMouse entry and goes
 * down the chain through blocks of ours, to the first PASS-THROUGH (its
 * jumps lead where its chain does).  Re-aiming that block inserts this
 * copy at a point the chain already runs through, and every link below
 * still ends where it did: nothing is skipped and nothing loops.
 *
 * A block in use by another copy is passed over.  A block is believed to
 * be ours only inside the system heap, with both jumps, the signature
 * and its own address where they should be.
 *
 * Layout: +0 jmp.l, +6 jmp.l, +12 'FDtr', +16 the block's own address,
 * +20 and +24 what its _FindWindow and _GetMouse jumps chain to.
 */
#if FDROP_ROMPLUS       /* what the next copy finds a block by: reuse */
static void tramp_mark(unsigned char *b, unsigned long fwNext,
                       unsigned long gmNext)
{
    set_blob_long(b + kTrampAtSig, kTrampSig);
    set_blob_long(b + kTrampAtSelf, (long)b);
    set_blob_long(b + kTrampAtFwNext, (long)fwNext);
    set_blob_long(b + kTrampAtGmNext, (long)gmNext);
}
#endif

#if FDROP_ROMPLUS
/* The block of ours whose _GetMouse jump is at `gm`, or 0. */
static unsigned char *tramp_at(unsigned long gm)
{
    Zone          *sz = *(Zone **)0x02A6L;          /* SysZone */
    unsigned long  lo = (unsigned long)&sz->heapData;
    unsigned long  hi = (unsigned long)sz->bkLim;
    unsigned char *b;

    if ((gm & 1UL) != 0UL || gm < lo + 6UL || gm - 6UL + kTrampBytes > hi)
        return 0;
    b = (unsigned char *)(gm - 6UL);
    if (rec_word(b) != 0x4EF9 || rec_word(b + 6) != 0x4EF9      /* jmp.l */
        || blob_long(b + kTrampAtSig) != kTrampSig
        || blob_long(b + kTrampAtSelf) != (long)b)
        return 0;
    return b;
}

/* Whether both jumps (their operands at +2 and +8) lead where the
   block's chain does: a pass-through. */
static Boolean tramp_passes(const unsigned char *b)
{
    return (Boolean)(blob_long(b + 2) == blob_long(b + kTrampAtFwNext)
                     && blob_long(b + 8) == blob_long(b + kTrampAtGmNext));
}

/* The first pass-through of ours down _GetMouse's chain, or 0. */
static unsigned char *tramp_reusable(unsigned long gm)
{
    short k;

    for (k = 0; k < 16; k++) {          /* a chain is a handful at most */
        unsigned char *b = tramp_at(gm);

        if (b == 0)
            return 0;
        if (tramp_passes(b))
            return b;
        gm = (unsigned long)blob_long(b + kTrampAtGmNext);
    }
    return 0;
}
#endif  /* FDROP_ROMPLUS */

/*
 * Install the two patches through a trampoline.  In order:
 *
 *   1. Return if this copy is already installed.
 *   2. Find the trap table (fdrop_probe_table).  None found: the 64K ROM,
 *      whose word table gets a trampoline low enough to encode, from
 *      the system heap or else the application heap; or no table this
 *      build knows, and nothing is installed.
 *   3. Later ROMs: reuse a pass-through trampoline already in the chain
 *      if there is one (tramp_reusable).
 *   4. Otherwise make one in the system heap, mark it, flush the caches,
 *      and point both table entries at it.
 *
 * FDROP_ENABLE is set last, once the patches can run.
 */
static void patch_install(void)
{
    unsigned long *t;
    unsigned char *tramp;

#if FDROP_PATCH_FW
    if (FDROP_FW_OLD != 0UL)              /* already on: do not chain twice */
#else
    if (FDROP_GM_OLD != 0UL)              /* already on: do not chain twice */
#endif
        return;
#if FDROP_ROMPLUS
    t = fdrop_probe_table();
#else
    t = 0;                              /* this build asks for no other table */
#endif
#if !FDROP_ROM64K && !FDROP_ROMPLUS
    (void)tramp;                        /* built for no ROM: installs nothing */
#endif
    if (t == 0) {
#if FDROP_ROM64K
        /*
         * No longword table: the 64K ROM.  A patched trap is a 16-bit
         * offset from a low RAM base, 64 KB of reach, and the code
         * resource loads far above that.  So a trap cannot point at the
         * patch directly; the trampoline can, if it is allocated low
         * enough to encode.  The old table words are saved and restored
         * raw.  Nothing here calls _SetTrapAddress: only the ROM's table
         * layout is relied on, not its encoder.
         */
#if FDROP_PATCH_FW
        unsigned short *fwSlot, *gmSlot;
        unsigned short  fwWord, gmWord;
        Boolean         okf, okg;
#else   /* one patch, on _GetMouse: fdrop.h, FDROP_PATCH_FW */
        unsigned short *gmSlot;
        unsigned short  gmWord;
        Boolean         okg;
#define okf okg
#endif

        if (!rom64_present())
            return;
        /*
         * The trampoline must land in the 64 KB the word offset reaches.
         * Neither heap promises that, so try the system heap, then the
         * application heap, and keep whichever is in range.  If neither
         * fits, install nothing: no sender is better than a trap patched
         * to a wild address.
         */
        tramp = (unsigned char *)NewPtrSys(kTrampBytes);
        DG(kDgTrampSys, (short)((long)tramp >> 4));
#if FDROP_PATCH_FW
        (void)rom64_encode((unsigned long)tramp, &okf);
#endif
        (void)rom64_encode((unsigned long)(tramp + kTrampGM), &okg);
        if (tramp != 0 && (!okf || !okg)) {
            DisposePtr((Ptr)tramp);
            tramp = 0;
        }
        if (tramp == 0) {
            tramp = (unsigned char *)NewPtr(kTrampBytes);
            DG(kDgTrampApp, (short)((long)tramp >> 4));
#if FDROP_PATCH_FW
            (void)rom64_encode((unsigned long)tramp, &okf);
#endif
            (void)rom64_encode((unsigned long)(tramp + kTrampGM), &okg);
            if (tramp != 0 && (!okf || !okg)) {
                DisposePtr((Ptr)tramp);
                tramp = 0;
            }
        }
        if (tramp == 0)
            return;                     /* out of the 64 KB reach: refuse */
#if FDROP_PATCH_FW
        fwWord = rom64_encode((unsigned long)tramp, &okf);
#endif
        gmWord = rom64_encode((unsigned long)(tramp + kTrampGM), &okg);
#if FDROP_PATCH_FW
        tramp_write(tramp, (unsigned long)FDROP_FW);
#endif
        tramp_write(tramp + kTrampGM, (unsigned long)FDROP_GM);
#if FDROP_ROMPLUS       /* marked for a reuse only later ROMs' path makes */
        tramp_mark(tramp, trap_addr64(kFindWindow), trap_addr64(kGetMouse));
#endif

        FDROP_DRAGFP = 0;
        FDROP_PEND = 0;
#if FDROP_PATCH_FW
        fwSlot = rom64_slot(kFindWindow);
#endif
        gmSlot = rom64_slot(kGetMouse);
        /* The patch chains to the old handler by address, so FW_OLD and
         * GM_OLD hold decoded addresses (as the trap reports them).  The
         * raw table words, which patch_remove puts back, are kept apart. */
#if FDROP_PATCH_FW
        FDROP_FW_OLD = trap_addr64(kFindWindow);
#endif
        FDROP_GM_OLD = trap_addr64(kGetMouse);
#if FDROP_PATCH_FW
        FDROP_ROM64_FWW = *fwSlot;
#endif
        FDROP_ROM64_GMW = *gmSlot;
        FDROP_TRAMP = (long)tramp;
#if FDROP_PATCH_FW
        *fwSlot = fwWord;
#endif
        *gmSlot = gmWord;
#if FDROP_ROMPLUS       /* which table: only a build that knows two asks */
        FDROP_TOOLTAB = kToolTableRom64;
#endif
        FDROP_ENABLE = 1;
#undef okf
#endif  /* without FDROP_ROM64K: no table this build knows, no sender */
        return;
    }
#if FDROP_ROMPLUS
    FDROP_DRAGFP = 0;                     /* no Finder drag followed yet */
    FDROP_PEND = 0;
    FDROP_TOOLTAB = (long)t;
    tramp = tramp_reusable(t[kTrapIndex(kGetMouse)]);
    if (tramp != 0) {
        /* A pass-through already in the chain: re-aim its jumps at this
         * copy, which chains on to what the block chained to. */
        FDROP_FW_OLD = (unsigned long)blob_long(tramp + kTrampAtFwNext);
        FDROP_GM_OLD = (unsigned long)blob_long(tramp + kTrampAtGmNext);
        tramp_write(tramp, (unsigned long)FDROP_FW);
        tramp_write(tramp + 6, (unsigned long)FDROP_GM);
        tramp_flush();
        FDROP_TRAMP = (long)tramp;
        FDROP_ENABLE = 1;
        return;
    }
    tramp = (unsigned char *)NewPtrSys(kTrampBytes);
    if (tramp == 0) {
        FDROP_TOOLTAB = 0;
        return;                         /* no block: no sender, not a bare patch */
    }
    FDROP_FW_OLD = t[kTrapIndex(kFindWindow)];
    FDROP_GM_OLD = t[kTrapIndex(kGetMouse)];
    tramp_write(tramp, (unsigned long)FDROP_FW);
    tramp_write(tramp + 6, (unsigned long)FDROP_GM);
    tramp_mark(tramp, FDROP_FW_OLD, FDROP_GM_OLD);
    tramp_flush();
    FDROP_TRAMP = (long)tramp;
    t[kTrapIndex(kFindWindow)] = (unsigned long)tramp;
    t[kTrapIndex(kGetMouse)] = (unsigned long)(tramp + 6);
    FDROP_ENABLE = 1;
#endif  /* FDROP_ROMPLUS */
}

/*
 * Removal makes the block a pass-through WHOLE, both jumps to what they
 * replaced, so nothing in it can lead into this code resource once it is
 * gone.  The block is taken out of the table and freed only where the
 * table still points at it and the table is the whole story.
 *
 * Under MultiFinder the table is not the whole story.  MultiFinder takes
 * an application's _SetTrapAddress patch out of the table while that
 * application is switched out ("written into the trap table", above), so
 * a patch made on top of ours from another layer is invisible from here
 * and comes back with its layer, still chaining to this block.  So under
 * MultiFinder the block always stays in the chain, and the next copy to
 * install reuses it.  A session then holds as many blocks as copies were
 * ever open at once, not one per close.
 */
#if FDROP_ROMPLUS
static Boolean multifinder(void)
{
    unsigned long *t = fdrop_probe_table();

    return (Boolean)(t != 0 && t[kTrapIndex(kTrapOSDispatch)]
                              != t[kTrapIndex(kTrapUnimpl)]);
}
#endif  /* FDROP_ROMPLUS */

static void patch_remove(void)
{
#if FDROP_ROMPLUS
    unsigned long *t = (unsigned long *)FDROP_TOOLTAB;
#endif
    unsigned char *tramp = (unsigned char *)FDROP_TRAMP;
    Boolean        kept = false;

    FDROP_ENABLE = 0;
#if FDROP_ROM_ROUTE
    FDROP_ROMDRAG = 0L;
#if FDROP_ROMPLUS && FDROP_FINDER5
    FDROP_ROMWRAP = 0L;
#endif
    FDROP_ROMMODE = 0;
#endif
#if FDROP_ROM64K
#if FDROP_ROMPLUS
    if (tramp != 0 && FDROP_TOOLTAB == kToolTableRom64) {
#else
    if (tramp != 0) {
#endif
#if FDROP_PATCH_FW
        unsigned short *fwSlot = rom64_slot(kFindWindow);
#endif
        unsigned short *gmSlot = rom64_slot(kGetMouse);
        Boolean         ok;

        /* No MultiFinder on this ROM: the table is the whole story.  An
           entry no longer ours keeps the block, as a pass-through. */
#if FDROP_PATCH_FW
        if (*fwSlot == rom64_encode((unsigned long)tramp, &ok))
            *fwSlot = FDROP_ROM64_FWW;
        else
            kept = true;
#endif
        if (*gmSlot == rom64_encode((unsigned long)(tramp + kTrampGM), &ok))
            *gmSlot = FDROP_ROM64_GMW;
        else
            kept = true;
        if (kept) {
#if FDROP_PATCH_FW
            tramp_write(tramp, FDROP_FW_OLD);
#endif
            tramp_write(tramp + kTrampGM, FDROP_GM_OLD);
        }
    } else
#endif
#if FDROP_ROMPLUS
    if (tramp != 0 && t != 0) {
        /*
         * A pass-through first, whatever happens next.  Then, only where
         * the table is the whole story (no MultiFinder) and both entries
         * still point at this block, out of the table and freed.
         * Otherwise it STAYS IN THE CHAIN, table untouched, for the next
         * copy to reuse (tramp_reusable).
         */
        tramp_write(tramp, FDROP_FW_OLD);
        tramp_write(tramp + 6, FDROP_GM_OLD);
        tramp_flush();
        kept = (Boolean)(multifinder()
                || t[kTrapIndex(kFindWindow)] != (unsigned long)tramp
                || t[kTrapIndex(kGetMouse)] != (unsigned long)(tramp + 6));
        if (!kept) {
            t[kTrapIndex(kFindWindow)] = FDROP_FW_OLD;
            t[kTrapIndex(kGetMouse)] = FDROP_GM_OLD;
        }
    }
#else
    {
    }
#endif
    /* Removal runs in the host's own context, so DisposePtr is legal. */
    if (tramp != 0 && !kept)
        DisposePtr((Ptr)tramp);
    FDROP_TRAMP = 0L;
#if FDROP_PATCH_FW
    FDROP_FW_OLD = 0UL;
#endif
    FDROP_GM_OLD = 0UL;
#if FDROP_ROMPLUS
    FDROP_TOOLTAB = 0;
#endif
    FDROP_DRAGFP = 0;
    FDROP_PEND = 0;
}
#endif  /* FDROP_SENDER */

/*
 * FDROP_STK: how close the sender comes to the Finder's heap.
 *
 * The sender runs inside a trap patch, so the stack it spends is the
 * Finder's, in the middle of its drag.  A host's probe of its own stack
 * cannot see that one; this is the probe for it.  It needs FDROP_DIAG
 * for somewhere to put the words.
 *
 * Two numbers, each a long in two words, 0 meaning "never taken":
 *
 *   kDgStkRoom  the room left when a drag first reaches the host window
 *               and its files are looked for: what there was to spend.
 *
 *   kDgStkLow   the LOW-WATER MARK: the least room this code has left
 *               between itself and the heap, across every drag of the
 *               session.  This is the number that matters.
 *
 * The room is measured at the deepest frames, not painted: a paint
 * needs the stack below to stay untouched while the work runs, and
 * interrupts land on this stack throughout an interactive drag.
 *
 * The answer is pessimistic, the safe direction: the probe's own frame
 * sits below the frame it reports on.
 */
#if FDROP_STK && FDROP_DIAG && FDROP_SENDER
#define kStkRoomHi  kDgStkRoom
#define kStkRoomLo  (kDgStkRoom + 1)
#define kStkLowHi   kDgStkLow
#define kStkLowLo   (kDgStkLow + 1)

static long stk_room(void)
{
    char here;

    return (long)&here - (long)LMGetHeapEnd();
}

/* What there was to spend, when this drag reached the host window. */
static void stk_note(void)
{
    long room = stk_room();

    DG(kStkRoomHi, (short)((room >> 16) & 0xFFFFL));
    DG(kStkRoomLo, (short)(room & 0xFFFFL));
}

/* The least room there has ever been.  Called from the deepest frames
   on the path, because that is where the stack is spent. */
static void stk_low(void)
{
    long room = stk_room();
    long seen = ((long)(unsigned short)FDROP_DG[kStkLowHi] << 16)
                | (long)(unsigned short)FDROP_DG[kStkLowLo];

    if (seen != 0 && room >= seen)
        return;
    DG(kStkLowHi, (short)((room >> 16) & 0xFFFFL));
    DG(kStkLowLo, (short)(room & 0xFFFFL));
}
#else
#define stk_note()  ((void)0)
#define stk_low()   ((void)0)
#endif

/* Read and write a long byte-wise.  The 68000 faults on a misaligned
   long, and nothing promises these addresses are even. */
static long blob_long(const unsigned char *p)
{
    return ((long)p[0] << 24) | ((long)p[1] << 16)
           | ((long)p[2] << 8) | (long)p[3];
}

#if FDROP_SENDER || FDROP_RECEIVER    /* nothing else writes an entry */
static void set_blob_long(unsigned char *p, long v)
{
    p[0] = (unsigned char)((v >> 24) & 0xFF);
    p[1] = (unsigned char)((v >> 16) & 0xFF);
    p[2] = (unsigned char)((v >> 8) & 0xFF);
    p[3] = (unsigned char)(v & 0xFF);
}
#endif

/*
 * Whether there is a Drag Manager is fdrop.h's fdrop_have_drag_manager,
 * a static inline so that a host can ask before loading this code.
 */

/* ------------------------------------------------------------------ */
/* The Finder's records                                                */
/* ------------------------------------------------------------------ */

/*
 * Every icon a Finder draws (a file, a folder, a disk, the Trash, in a
 * window or on the desktop) is a RELOCATABLE BLOCK of its own in the
 * Finder's application heap, and the block says what it is.  For Finders
 * 5.4 to 6.1.8 (NOTES.md, "Every icon a Finder draws is a heap block of
 * its own, and the block's size says so"):
 *
 *   the item's NAME, a Pascal string, starts kRecName bytes into the
 *   block's data, and the block's logical size is exactly kRecName + 1 +
 *   the name's length.  So a block is a record because of its size and
 *   its own header, not because some bytes in the heap look like one.
 *
 * At fixed distances before that name:
 *
 *   kRecPos    where the icon is drawn, v then h: in its window's own
 *              coordinates, or GLOBAL for an icon on the desktop;
 *   kRecCont   the HANDLE of the object it is drawn in: the record of
 *              the folder or disk whose window it is in (which is that
 *              window's refCon), or the desktop's own record;
 *   kSelOff    the selected flag, bit kSelBit;
 *   kRecVol    the vRefNum of the volume the item is on (0 for the Trash);
 *   kRecFlags  the item's fdFlags, which carry fOnDesk on the desktop;
 *   kRecParent the directory the item is in: 1 for a disk, 0 for the
 *              Trash, and 0 for everything on an MFS volume, the disk
 *              included, since MFS has no directories.
 *
 * The same record is Finder 5.1's (the ROM route's, below) with the name
 * four bytes further in, so a field both routes read is at one offset
 * into the block's data in both, Finder 1.1g's too but for its file
 * number (NOTES.md, "Finders 5.4 to 6.1.8 take a drop from an MFS
 * volume"):
 *
 *   kItemKind  a word saying what the icon is: 0 for a file, kKindDisk,
 *              kKindFolder (the desktop's own record is one too), or
 *              kKindTrash;
 *   kItemCont  the container's handle, which is kRecCont from the name;
 *   kItemVol   the vRefNum, which is kRecVol from the name;
 *   kItemNum   the file's number: an MFS file number, or an HFS file's
 *              catalog node ID (kItemNum11 in Finder 1.1g's record).
 *
 * The name is at kRecName, kRom51Name in Finder 5.0 to 5.3's record, and
 * kRom11Name in Finder 1.1g's.
 *
 * The walk goes block by block from the start of the zone to its end, by
 * each block's own size, as the Memory Manager does in a 24-bit heap,
 * which is what every machine the sender runs on has.  A block whose
 * size does not lead on to the next one, or runs past the zone's end,
 * ends the walk with a refusal, never a guess.
 */
#define kRecName     98        /* the name, from the start of the data */
#define kRecPos    (-96)
#define kRecCont   (-84)
#define kSelOff    (-77)
#define kSelBit    0x02
#define kRecVol    (-64)
#define kRecFlags  (-60)
#define kRecParent (-12)
#define kOnDesk    0x0001      /* fOnDesk, in kRecFlags */
#define kIconSize  32
#define kRootDirID 2L
#define kRootParID 1L          /* the root's parent: what names a volume */
#define kSigHFS    0x4244      /* 'BD', an HFS volume */
#define kSigMFS    0xD2D7      /* an MFS volume */

#define kItemKind  0x00
#define kKindDisk    4
#define kKindFolder  8
#define kKindTrash   12
#define kItemCont  0x0E
#define kItemVol   0x22
#define kItemNum   0x3C
#define kItemNum11 0x38
#define kRom11Name 0x3E
#define kRom51Name 0x5E
#define kFolderDepth 32        /* MFS folders within folders, at most */

/* Where a record of the name at `k` keeps its file number.  Each build
   tells apart only the layouts it reads. */
#if !FDROP_ROM_ROUTE
#define ITEM_NUM(k)  kItemNum
#elif !FDROP_OWN_ROUTE && !FDROP_FINDER5
#define ITEM_NUM(k)  kItemNum11
#else
#define ITEM_NUM(k)  ((k) == kRom11Name ? kItemNum11 : kItemNum)
#endif

/*
 * From here to the queue: reading the Finder's heap, and asking the File
 * Manager about what is found there.  Only the sender does this.
 */
#if FDROP_SENDER
static const unsigned char *heap_start(void)
{
    return (const unsigned char *)(*(Zone **)0x02AAL);
}

static const unsigned char *heap_end(void)
{
    return (const unsigned char *)((*(Zone **)0x02AAL)->bkLim);
}

/* The zone's first block, just past its header. */
static const unsigned char *rec_first(void)
{
    return (const unsigned char *)&(*(Zone **)0x02AAL)->heapData;
}

static short rec_word(const unsigned char *p)
{
    return (short)(((unsigned short)p[0] << 8) | p[1]);
}

/*
 * The next RELOCATABLE block at or after `*at`: its data, its logical
 * size and its handle, or 0 at the end.  `*at` starts as rec_first(), and
 * is left 0 if the heap could not be walked to its end, which the caller
 * must treat as a refusal.  Both record readers share this walk, so the
 * heap is parsed one way only.
 *
 * A block header is a tag byte (the top two bits its kind, 10 for
 * relocatable; the low four its size correction), a 24-bit physical
 * size, and for a relocatable block its handle, as an offset from the
 * zone.
 */
static const unsigned char *blk_next(const unsigned char **at, long *handle,
                                     long *logical)
{
    const unsigned char *p = *at;
    const unsigned char *end = heap_end();

    while (p != end) {
        long size;
        unsigned char tag;

        if (p + 8 > end) {
            *at = 0;
            return 0;
        }
        tag  = p[0];
        size = blob_long(p) & 0x00FFFFFFL;
        if (size < 8L || size > (long)(end - p)) {
            *at = 0;                    /* not a heap this can walk */
            return 0;
        }
        *at = p + size;
        if ((tag & 0xC0) == 0x80) {
            *logical = size - 8L - (long)(tag & 0x0F);
            *handle = (long)heap_start() + blob_long(p + 4);
            return p + 8;
        }
        p = *at;
    }
    return 0;
}

/*
 * Whether the block whose data is at `d`, `logical` bytes long, ends
 * exactly where a name of 1 to 63 characters at `off` does: how a block
 * is known for an item record, in every Finder's layout.
 */
static Boolean name_ends(const unsigned char *d, long logical, short off)
{
    return (Boolean)(logical >= (long)off + 2 && d[off] >= 1 && d[off] <= 63
                     && logical == (long)off + 1 + d[off]);
}

/*
 * FDROP_OWN_ROUTE (fdrop.h): rec_next and same_pstr here, and below
 * hfs_volume, the catalog's readers, and everything from
 * finder_drag_call to the end of resolve_drag, are the route for the
 * Finders that carry a drag routine of their own, 5.4 to 6.1.8.
 */
#if FDROP_OWN_ROUTE
/* The next item record at or after `*at` (blk_next), as its name, with
   its handle; 0 at the end. */
static const unsigned char *rec_next(const unsigned char **at, long *handle)
{
    const unsigned char *d;
    long logical, h;

    while ((d = blk_next(at, &h, &logical)) != 0) {
        if (name_ends(d, logical, kRecName)) {
            *handle = h;
            return d + kRecName;
        }
    }
    return 0;
}

/* Whether two Pascal strings are the same, byte for byte. */
static Boolean same_pstr(const unsigned char *a, const unsigned char *b)
{
    short i;

    if (a == 0 || b == 0 || a[0] != b[0])
        return false;
    for (i = 1; i <= (short)a[0]; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

#endif  /* FDROP_OWN_ROUTE */

/*
 * ---- a volume the Finder names, and whether it may be asked about ----
 *
 * Every volume the sender asks the File Manager about comes out of the
 * Finder's records, and an EJECTED disk is still there: its icon is
 * dimmed and its VCB is kept.  Anything that reads such a volume
 * (PBGetCatInfo, PBGetFInfo) puts up the disk-switch alert, which inside
 * the _GetMouse patch would be in the middle of the Finder's drag.
 *
 * So during a drag an OFFLINE volume is read off its VCB and nothing
 * else.  Its name and signature are in memory, and that is all a disk's
 * icon needs.  A file or folder on it is not checked at all: it is
 * handed over as the Finder recorded it, and the host's own first File
 * Manager call on it asks for the disk, in the host's context, as the
 * Finder's own copy does (NOTES.md, "A drag from an ejected disk is
 * taken, and the program's own first call asks for the disk").
 *
 * Offline is a drive number of 0: "If the value returned in ioVDrvInfo
 * is 0, the volume is either offline or ejected" (Inside Macintosh:
 * Files, 2-47).  It is read off the VCB's vcbDrvNum, the one place every
 * ROM has it: PBHGetVInfo is missing on the 64K ROM, and _GetVolInfo's
 * block stops short of ioVDrvInfo.  A vRefNum that is not negative is
 * not a volume reference number: 0 would be the default volume, which
 * no Finder record means.
 *
 * The VCB queue's head is at $0358, in VCBQHdr ($0356); the offsets are
 * a VCB's, the same for MFS and HFS (Inside Macintosh: Files, 2-77).  The
 * walk is bounded, so a queue that does not end cannot hang the drag.
 */
#define kVCBQHead     0x0358L
#define kVcbSigWord   8
#define kVcbVN        44        /* the volume's name, a Str27 */
#define kVcbDrvNum    72        /* 0 when offline */
#define kVcbVRefNum   78
#define kVcbMost      64        /* VCBs walked at most */

static const unsigned char *vcb_of(short vRefNum)
{
    const unsigned char *v = *(const unsigned char **)kVCBQHead;
    short                k;

    if (vRefNum >= 0)
        return 0;                       /* not a volume reference number */
    for (k = 0; v != 0 && k < kVcbMost; k++) {
        if (*(const short *)(v + kVcbVRefNum) == vRefNum)
            return v;
        v = *(const unsigned char * const *)v;
    }
    return 0;
}

static Boolean vol_online(short vRefNum)
{
    const unsigned char *v = vcb_of(vRefNum);

    return (Boolean)(v != 0 && *(const short *)(v + kVcbDrvNum) > 0);
}

/* A volume's name and signature, off its VCB, as _GetVolInfo reads them;
   0 if there is no such volume. */
static unsigned short vcb_vol(short vRefNum, unsigned char *name)
{
    const unsigned char *v = vcb_of(vRefNum);
    short                i, len;

    if (v == 0)
        return 0;
    len = (short)(v[kVcbVN] > 27 ? 27 : v[kVcbVN]);    /* a Str27 */
    name[0] = (unsigned char)len;
    for (i = 1; i <= len; i++)
        name[i] = v[kVcbVN + i];
    return *(const unsigned short *)(v + kVcbSigWord);
}

#if FDROP_OWN_ROUTE
/*
 * Whether the volume a vRefNum names is mounted and HFS, with its name
 * (off its VCB, so an offline volume is not touched).  An item of an HFS
 * volume is believed because PBGetCatInfo finds it where its record
 * says; an MFS volume has no directories to find it in, and its items
 * are believed by their file numbers instead (mfs_take).
 */
static Boolean hfs_volume(short vRefNum, unsigned char *name)
{
    return (Boolean)(vcb_vol(vRefNum, name) == kSigHFS);
}

/* Whether the volume a vRefNum names is an MFS volume, off its VCB. */
static Boolean mfs_volume(short vRefNum)
{
    unsigned char vn[32];

    return (Boolean)(vcb_vol(vRefNum, vn) == kSigMFS);
}

#endif  /* FDROP_OWN_ROUTE */

/*
 * The name of the mounted volume `vRefNum` names, on any file system:
 * what a disk's icon is checked against before the volume is handed
 * over.  It comes off the VCB, so nothing asks for an offline disk.
 */
static Boolean vol_name(short vRefNum, unsigned char *name)
{
    return (Boolean)(vcb_vol(vRefNum, name) != 0);
}

#if FDROP_OWN_ROUTE
/*
 * The startup disk's Trash folder, which is how the Drag Manager hands
 * over a dragged Trash: a folder at the root of the startup volume
 * (BootDrive, $0210) with the Trash icon's own name, so a Trash called
 * something else is still found.  False when there is no such folder,
 * and the Trash is then left out.
 */
static OSErr cat_entry(const unsigned char *name, short vRefNum, long dirID,
                       Boolean *isDir, long *ownID, unsigned short *flags);

static Boolean startup_trash(const unsigned char *name, short *vol)
{
    unsigned char  vn[32];
    Boolean        isDir = false;
    long           own;
    unsigned short fl;

    /* Off the VCB, which every ROM has: PBHGetVInfo is missing on the
       64K ROM, where an MFS startup disk has no Trash folder. */
    *vol = *(short *)0x0210L;           /* BootDrive */
    if (!hfs_volume(*vol, vn) || !vol_online(*vol))
        return false;
    return (Boolean)(cat_entry(name, *vol, kRootDirID, &isDir, &own, &fl)
                     == noErr && isDir);
}

/*
 * A catalog entry by name in a directory: whether there is one, whether
 * it is a folder, and for a folder its own directory ID.  `flags` gets
 * the Finder flags, a file's or a folder's.
 */
static OSErr cat_entry(const unsigned char *name, short vRefNum, long dirID,
                       Boolean *isDir, long *ownID, unsigned short *flags)
{
    CInfoPBRec ci;
    unsigned char nm[64];               /* a Str63: MFS names reach 63 */
    short i;
    OSErr err;

    /* The deepest frame on the drag path (a parameter block and a name,
       under resolve_drag): where the low-water mark is taken. */
    stk_low();
    if (name[0] > 63)
        return bdNamErr;                /* never copied past the buffer */
    for (i = 0; i <= (short)name[0]; i++)
        nm[i] = name[i];
    for (i = 0; i < (short)sizeof(ci); i++)
        ((char *)&ci)[i] = 0;
    ci.hFileInfo.ioNamePtr   = (StringPtr)nm;
    ci.hFileInfo.ioVRefNum   = vRefNum;
    ci.hFileInfo.ioDirID     = dirID;
    ci.hFileInfo.ioFDirIndex = 0;
    err = PBGetCatInfoSync(&ci);
    if (err != noErr)
        return err;
    *isDir = (Boolean)((ci.hFileInfo.ioFlAttrib & 0x10) != 0);  /* a folder */
    *ownID = *isDir ? ci.dirInfo.ioDrDirID : 0L;
    *flags = *isDir ? (unsigned short)ci.dirInfo.ioDrUsrWds.frFlags
                    : (unsigned short)ci.hFileInfo.ioFlFndrInfo.fdFlags;
    return noErr;
}

/*
 * A folder's window on an OFFLINE volume: its directory cannot be found
 * without reading the catalog, so each item is handed over in the
 * directory its own record names.
 */
#define kAskDir (-1L)

/*
 * A window of an MFS volume, its disk's or one of its folders': there is
 * no directory, and each item is believed by its file number (mfs_item).
 */
#define kMfsDir (-2L)

/*
 * The directory a container record's window shows, and the volume; 0 if
 * the record does not check out.
 *
 * A disk's record (parent 1) shows the root, and its name must be the
 * volume's.  A folder's record must be a folder the File Manager finds
 * where the record says, and its own directory ID is the answer.  Either
 * way the number comes from the File Manager, asked about names the
 * record holds, never straight out of the heap.
 *
 * On an MFS volume the answer is kMfsDir, for the disk's record when its
 * name is the volume's, and for a folder's, which the File Manager has no
 * name for: what is in the window is checked item by item.
 */
static long container_dir(const unsigned char *cn, short *vol)
{
    unsigned char vn[32];
    short v = rec_word(cn + kRecVol);
    long  par = blob_long(cn + kRecParent), own = 0;
    Boolean isDir = false;
    unsigned short fl;
    unsigned short sig = vcb_vol(v, vn);
    short kind = rec_word(cn - kRecName + kItemKind);

    if (sig == kSigMFS) {
        if (par != 0L || !(kind == kKindFolder
                           || (kind == kKindDisk && same_pstr(vn, cn))))
            return 0;
        *vol = v;
        return kMfsDir;
    }
    if (sig != kSigHFS)
        return 0;
    if (par == kRootParID) {          /* a disk */
        if (!same_pstr(vn, cn))
            return 0;
        *vol = v;
        return kRootDirID;
    }
    *vol = v;
    if (!vol_online(v))
        return kAskDir;                 /* each item's record says */
    if (cat_entry(cn, v, par, &isDir, &own, &fl) != noErr || !isDir)
        return 0;
    return own;
}

/*
 * Whether a record is an item the host can be handed from directory
 * `dirID` of volume `vol`: in that directory by its own account, a file
 * or folder the File Manager finds there by that name, and visible.  A
 * folder is handed over as a file is (volume, directory, name: an
 * FSSpec), and the host asks the File Manager what it is, as it would
 * for one from the Drag Manager.
 *
 * `onDesk` asks for an icon on the desktop as well, which the RECORD
 * must say.  The catalog cannot: the Finder writes its information back
 * when it chooses (NOTES.md, "An icon on the desktop is a record like any
 * other, and the catalog does not know it is there").
 *
 * On an offline volume only the record is tested (vol_online).  Answers
 * 0 if the item may be taken, or else which test it failed (kTake...),
 * for the diagnostic words.
 */
#define kTakeElsewhere  1       /* its record puts it somewhere else */
#define kTakeDeskWrong  2       /* its fOnDesk is not what was asked for */
#define kTakeMissing    3       /* the catalog has no such name there */
#define kTakeInvisible  5       /* fInvisible */
#define kTakeWrongVol   6       /* resolve_drag: not the volume it says */

static short takeable(const unsigned char *rec, short vol, long dirID,
                      Boolean onDesk)
{
    Boolean isDir = true;
    long own;
    unsigned short fl = 0;
    OSErr err;

    if (rec_word(rec + kRecVol) != vol
        || (dirID != kAskDir && blob_long(rec + kRecParent) != dirID))
        return kTakeElsewhere;          /* somewhere else, by its record */
    if (onDesk != ((rec_word(rec + kRecFlags) & kOnDesk) != 0))
        return kTakeDeskWrong;          /* on the desktop, or not, wrongly */
    if (!vol_online(vol))
        return 0;                       /* not asked: see vol_online */
    err = cat_entry(rec, vol, dirID, &isDir, &own, &fl);
    DG(kDgCatFlags, err == noErr ? (short)fl : err);
    if (err != noErr)
        return kTakeMissing;            /* not there */
    (void)own;
    if ((fl & 0x4000) != 0)             /* fInvisible */
        return kTakeInvisible;          /* invisible */
    return 0;
}
#endif  /* FDROP_OWN_ROUTE */
#endif  /* FDROP_SENDER */

/* ------------------------------------------------------------------ */
/* A Finder drag that ends on the host window                          */
/* ------------------------------------------------------------------ */

/*
 * The queue is the HOST'S, handed over at fdrop_install with how many
 * entries it holds.  A queue and not a slot, because one gesture can
 * carry several files.  Its depth and capacity are kept here, beside the
 * pointer, so nothing has to agree with a number compiled into the host.
 * A drag of more than there is room for is refused whole: the icons go
 * back, and fdrop_refused says so.
 *
 * An entry is FDROP_ENTRY_BYTES: the window at +0 (4), the vRefNum at
 * +4 (2), the directory at +6 (4) and the name, a Str63, at +10.
 */
#define kDropBytes  FDROP_ENTRY_BYTES

static unsigned char *queue_at(short i)
{
    return (unsigned char *)FDROP_QUEUE + (long)i * kDropBytes;
}

static short drops_queued(void)
{
    return FDROP_QN;
}

static void set_drops_queued(short n)
{
    FDROP_QN = n;
}

/* Entries the queue can hold, 0 when the host gave it none.  With
 * neither the sender nor the receiver built nothing writes to the queue,
 * so this and put_drop are not built either. */
#if FDROP_SENDER || FDROP_RECEIVER
static short queue_cap(void)
{
    return FDROP_QUEUE != 0L && FDROP_QCAP > 0 ? FDROP_QCAP : 0;
}
#endif

/*
 * The longest name a drop may carry: a Str63, as fdrop_take_drop and
 * Standard File hand back.  HFS names stop at 31; an MFS volume's reach
 * 63, and the File Manager opens such a file only by its whole name.  A
 * longer name (MFS's format allows one; no Finder writes it) is REFUSED,
 * never cut short: the front of a name may be another file's name.
 * Every writer checks before it calls put_drop.
 */
#define kNameMax 63

/*
 * Write a drop at a given place in the queue, without moving the depth.
 * The sender writes what a drag carries past the depth while the drag is
 * still going, and the depth moves only once the Finder has let go over
 * the host window (fdrop_idle).  record_drop writes at the depth and
 * moves it, for the Drag Manager's receiver.
 */
#if FDROP_SENDER || FDROP_RECEIVER
static void put_drop(short at, short vol, long dirID,
                     const unsigned char *name)
{
    unsigned char *e = queue_at(at);
    short          len, i;

    set_blob_long(&e[0], FDROP_WIN);                 /* the window */
    e[4] = (unsigned char)((vol >> 8) & 0xFF);      /* the vRefNum */
    e[5] = (unsigned char)(vol & 0xFF);
    set_blob_long(&e[6], dirID);                    /* the directory */
    len = (short)name[0];
    if (len > kNameMax)                 /* never, its callers refuse those: */
        len = kNameMax;                 /* this only keeps the entry whole  */
    for (i = 0; i <= len; i++)                      /* the name */
        e[10 + i] = (i == 0) ? (unsigned char)len : name[i];
}
#endif  /* FDROP_SENDER || FDROP_RECEIVER */

#if FDROP_RECEIVER
/*
 * Append a drop from the Drag Manager's receiver.
 *
 * One queue, two writers: the sender and the system's own Drag Manager.
 * The host cannot tell which one a drop came by, and is not meant to.
 * The sender writes with put_drop and moves the depth itself
 * (fdrop_idle).
 *
 * It APPENDS, and discards what will not fit rather than overwrite what
 * is waiting: the host reads from the front, so overwriting would lose
 * the file dropped first and keep the last, a plausible wrong answer.
 * The caller checks the room before it starts, so a full queue here
 * means that check was skipped.
 *
 * A drop onto ANOTHER window is not kept: handlers are registered per
 * window, and an entry naming a window that is not the host window is a
 * file nobody is waiting for.
 */
static void record_drop(long window, short vol, long dirID,
                        const unsigned char *name)
{
    short n = drops_queued();

    if (window != FDROP_WIN || window == 0)
        return;
    if (n < 0 || n >= queue_cap())
        return;
    put_drop(n, vol, dirID, name);
    set_drops_queued((short)(n + 1));
}

#endif  /* FDROP_RECEIVER */

/*
 * Take the entry at the front off.
 *
 * The whole array is shifted down, rather than a read index kept beside
 * the count: two numbers that can disagree are how a queue written by a
 * trap patch and read by a program goes wrong.  The cost is a copy of
 * what is waiting per file taken.
 */
static void drop_pop(void)
{
    short          n = drops_queued();
    unsigned char *q = queue_at(0);
    long           i, bytes;

    if (n <= 0)
        return;
    bytes = (long)(n - 1) * kDropBytes;
    for (i = 0; i < bytes; i++)
        q[i] = q[i + kDropBytes];
    set_drops_queued((short)(n - 1));
}

/* ------------------------------------------------------------------ */
/* An MFS volume's files and folders                                   */
/* ------------------------------------------------------------------ */

/*
 * Both routes, the ROM's and the Finders' own, hand over an MFS volume's
 * files the same way, and the code is here once.
 *
 * An MFS volume has no directories.  Its folders exist only in the
 * Finder, which keeps them in its Desktop file and draws a record for
 * each; the File Manager sees one flat list of files.  So a file is
 * believed when the File Manager finds that NAME on that volume with the
 * FILE NUMBER the Finder's record holds, and it is handed over as volume,
 * directory 0, name.  A folder is handed over as the files in it.
 */
#if FDROP_SENDER
#if FDROP_ROM_ROUTE
static short rom_record(const unsigned char *d, long logical);  /* below */
#endif

/*
 * Where an item record's name is in the block at `d`, or 0 if the block
 * is none: in the layout of Finders 5.4 to 6.1.8 when `lay` is kRecName,
 * and otherwise in either layout the ROM route reads (rom_record).  One
 * heap holds one Finder's records, so the route says which.
 */
static short item_name(const unsigned char *d, long logical, short lay)
{
#if FDROP_OWN_ROUTE
    if (lay == kRecName)
        return name_ends(d, logical, kRecName) ? kRecName : 0;
#else
    (void)lay;
#endif
#if FDROP_ROM_ROUTE
    return rom_record(d, logical);
#else
    (void)d;
    (void)logical;
    return 0;
#endif
}

/*
 * Whether the File Manager finds a visible file of that name on that
 * volume with that FILE NUMBER.  An MFS volume has no directories, so
 * the number in the Finder's record is the check a wrong record fails.
 * _GetFileInfo, which the 64K ROM has.  The name is copied out of the
 * Finder's heap first, because it is in a relocatable block.
 */
static Boolean mfs_file(short vol, const unsigned char *name, long num)
{
    ParamBlockRec pb;
    unsigned char nm[64];
    short         i;

    /* The deepest frame on the drag path, as cat_entry is: the low-water
       mark is taken here too. */
    stk_low();
    if (name[0] < 1 || name[0] > 63)
        return false;
    if (!vol_name(vol, nm))             /* a volume: see vol_name */
        return false;
    for (i = 0; i <= (short)name[0]; i++)
        nm[i] = name[i];
    for (i = 0; i < (short)sizeof pb; i++)
        ((unsigned char *)&pb)[i] = 0;
    pb.fileParam.ioNamePtr = nm;
    pb.fileParam.ioVRefNum = vol;
    if (PBGetFInfoSync(&pb) != noErr)
        return false;
    if (pb.fileParam.ioFlNum != num)
        return false;
    /* not fInvisible */
    return (Boolean)((pb.fileParam.ioFlFndrInfo.fdFlags & 0x4000) == 0);
}

/*
 * One file of the drop, whose record's data is at `d` and name at `d +
 * k`: believed once the File Manager finds that name on that volume with
 * that file number, then queued at `base + *n`.  On an OFFLINE volume
 * nothing is asked (vol_online): it is queued as its record says.
 * Answers 0, or the step that refuses the drop.
 */
static short mfs_take(const unsigned char *d, short k, short base, short *n,
                      short room)
{
    short   v = rec_word(d + kItemVol);
    long    num = blob_long(d + ITEM_NUM(k));
    Boolean off = (Boolean)!vol_online(v);

    if (off) {
        if (vcb_of(v) == 0)
            return kStepMfsFileUnknown;  /* no such volume */
    } else if (!mfs_file(v, d + k, num))
        return kStepMfsFileUnknown;     /* not a file the volume agrees on */
    if (d[k] > kNameMax)
        return kStepNameTooLong;        /* a name a drop cannot carry */
    {
        short i, j;

        for (i = 0; i < *n; i++) {      /* the same file already queued? */
            const unsigned char *e = queue_at((short)(base + i));

            if (rec_word(e + 4) != v || blob_long(e + 6) != 0L
                || e[10] != d[k])
                continue;
            for (j = 1; j <= d[k] && e[10 + j] == d[k + j]; j++)
                ;
            if (j > d[k])
                return kStepOneItemTwice;
        }
    }
    if (*n >= room)
        return kStepNoRoom;             /* more than the list can take */
    put_drop((short)(base + *n), v, 0L, d + k);
    *n = (short)(*n + 1);
    return 0;
}

/*
 * The record a handle names, with its logical size, or 0 if `h` is not
 * the handle of a relocatable block in this heap.  This is blk_next's
 * reading of a block header run backwards, checked to come back to the
 * same place: the master pointer is in the heap, its block is in the
 * heap, relocatable and of a size that fits, and that block's header
 * names `h` as its handle.  A value that is not a live handle cannot
 * pass all of that.  24-bit addresses only, as everywhere in the sender.
 */
static const unsigned char *rec_of(long h, long *logical)
{
    const unsigned char *lo = rec_first();
    const unsigned char *end = heap_end();
    const unsigned char *mp = (const unsigned char *)h;
    const unsigned char *p;
    long size;

    if (mp < lo || mp + 4 > end || (h & 1L) != 0L)
        return 0;
    {
        /* Bound the master pointer BEFORE taking 8 off it: a NIL or
           purged handle holds 0 to 7, which would wrap past zero and
           pass both bounds below. */
        unsigned long v = (unsigned long)blob_long(mp) & 0x00FFFFFFUL;

        if (v < (unsigned long)lo + 8UL || v > (unsigned long)end)
            return 0;
        p = (const unsigned char *)v - 8;
    }
    if (p + 8 > end || ((long)p & 1L) != 0L)
        return 0;
    size = blob_long(p) & 0x00FFFFFFL;
    if ((p[0] & 0xC0) != 0x80 || size < 8L || size > (long)(end - p)
        || (long)heap_start() + blob_long(p + 4) != h)
        return 0;
    *logical = size - 8L - (long)(p[0] & 0x0F);
    return p + 8;
}

/*
 * A FOLDER of an MFS volume, or the TRASH, handed over as the files in
 * it: each a drop of its own, and so are the files of folders within it,
 * up to kFolderDepth deep.  The folder's handle is all that is needed:
 * every file in it is a record whose container is that handle, and the
 * File Manager knows each of those, where it has no name for the folder.
 * `lay` is the layout of the records (item_name).
 *
 * ONE pass over the heap, and each file found is followed UP: its
 * container, that folder's container, and so on, until the dragged
 * folder or something that is not a folder.  Going up, nothing has to be
 * held, and the limit is the depth itself.  A chain that is still all
 * folders past kFolderDepth refuses the drag: it cannot be told without
 * going further, and a loop of folders would never end.
 *
 * On Finders 5.4 to 6.1.8 the Trash holds HFS items as well, and those
 * are not this function's: only the files of MFS volumes are taken.
 */
static short mfs_folder(long folder, short lay, short base, short *n,
                        short room)
{
    const unsigned char *at = rec_first();
    const unsigned char *d;
    long h, logical;

    while ((d = blk_next(&at, &h, &logical)) != 0) {
        const unsigned char *f;
        long  c, fl;
        short k, step, up;

        if (logical < (long)kItemCont + 4
            || rec_word(d + kItemKind) == kKindFolder)
            continue;
        k = item_name(d, logical, lay);
        if (k == 0)
            continue;                   /* not an item */
#if FDROP_OWN_ROUTE
        if (lay == kRecName && !mfs_volume(rec_word(d + kItemVol)))
            continue;                   /* an HFS item: not taken here */
#endif
        /* Up through the folders it is in, to the dragged one or not. */
        c = blob_long(d + kItemCont);
        for (up = 0; c != folder; up++) {
            if (up >= kFolderDepth)
                return kStepMfsTooDeep;  /* folders deeper than that */
            f = rec_of(c, &fl);
            if (f == 0 || fl < (long)kItemCont + 4
                || rec_word(f + kItemKind) != kKindFolder)
                break;                  /* reached something that is no folder */
            c = blob_long(f + kItemCont);
        }
        if (c != folder)
            continue;                   /* in some other folder */
        step = mfs_take(d, k, base, n, room);
        if (step != 0)
            return step;
    }
    if (at == 0)
        return kStepHeapUnwalkable;     /* the heap could not be walked */
    return 0;
}
#endif  /* FDROP_SENDER */

/*
 * From here to the end of fdrop_answer: following a Finder's drag, which
 * only the sender does.
 */
#if FDROP_SENDER
/*
 * The window a global point is in, by the test the real _FindWindow
 * makes: the first visible window, front to back, whose structure region
 * holds it.  So the window found is the one the Finder would be told of,
 * title bar and all, and a point where another window covers the host
 * window is not over it.  Under MultiFinder the list is the CURRENT
 * layer's only (over_panel).
 */
static WindowPeek window_at(long packed)
{
    WindowPeek w;
    Point      p;

    p.v = (short)((packed >> 16) & 0xFFFFL);
    p.h = (short)(packed & 0xFFFFL);
    for (w = *(WindowPeek *)kWindowListAddr; w != 0;
         w = (WindowPeek)w->nextWindow) {
        if (!w->visible || w->strucRgn == 0)
            continue;
        if (PtInRgn(p, w->strucRgn))
            return w;
    }
    return 0;
}

/*
 * The top left of a port's bounds: what a global point is offset by to
 * be in that port's coordinates.
 *
 * A port may be a CGrafPort, whose bounds are in its PixMap.  A
 * CGrafPort sets the top two bits of the word where a GrafPort keeps
 * rowBytes (portVersion), and the long before it is then the PixMap's
 * handle.  It is read, not asked, so nothing here calls Color
 * QuickDraw; without Color QuickDraw no rowBytes reaches 0x4000.
 */
static Point port_origin(GrafPtr port)
{
    const BitMap *b = &port->portBits;
    Point         o;

#if FDROP_ROMPLUS       /* no Color QuickDraw on a 64K ROM machine */
    if (((unsigned short)b->rowBytes & 0xC000U) == 0xC000U) {
        PixMapHandle pm = (PixMapHandle)b->baseAddr;

        o.v = (**pm).bounds.top;
        o.h = (**pm).bounds.left;
        return o;
    }
#endif
    o.v = b->bounds.top;
    o.h = b->bounds.left;
    return o;
}

/*
 * Mouse, the Event Manager's copy of the pointer, which the real
 * _GetMouse reads before converting to the current port.  The sender
 * decides from the same number the Finder would be given, so it reads
 * this one and not RawMouse.  System 6 keeps it current during a drag;
 * System 7 does not (PARKED.md).
 */
#define kMouseCooked 0x0830L

/*
 * MBTicks, the tick of the mouse button's last TRANSITION, and MBState,
 * what that transition left it at (0x80 up, 0 down).  The ROM's VBL task
 * writes the two together, so they always agree.  While the button is
 * down MBTicks is the press: the same for every look at one gesture and
 * different for the next, so it tells this drag from the one before it.
 * After the release it is the release, which the drag's final _GetMouse
 * sees.
 */
#define kMBTicksAddr 0x016EL
#define kMBStateAddr 0x0172L
#define kButtonUp    0x80     /* in MBState */

/* The application heap's end and the stack's base (HeapEnd and
   CurStackBase), which bound where a caller's A6 may be before anything
   is read through it. */
#define kHeapEndAddr      0x0114L
#define kCurStackBaseAddr 0x0908L

/* An application window's windowKind and above: the Finder's own
   windows.  A desk accessory's is negative. */
#define kUserKind 8

/*
 * ---- how the Finder drags an icon, and the one drag it ignores ----
 *
 * No Finder from 5.4 (System 4.0) to 6.1.8 (System 6.0.8) calls
 * _DragGrayRgn or _DragTheRgn.  Each has a drag routine of its own with
 * _DragTheRgn's parameters, and each pass it makes until the button
 * comes up is the same in all of them (NOTES.md, "The drag routine is
 * the same in every Finder from 5.4 to 6.1.8"):
 *
 *   the OUTLINE: _GetMouse; the outline follows it, and the move since
 *     the last pass is ADDED to the result at A6+30.  The offset is a sum
 *     of per-pass moves.
 *   the ACTION procedure: _GetMouse again, then _FindWindow at that
 *     point, and the target is decided from the answer.
 *   the button: if it is up, the FINAL _GetMouse, which only tests the
 *     point against the slop region (outside it, the result becomes
 *     0x80008000), and the routine returns the result.
 *
 * The target is whatever the LAST action procedure decided, and every
 * answer does something: a Finder window or a folder icon is a move into
 * it, and a desk accessory's window is taken to be the desktop.  The one
 * drag the Finder treats as nothing is one that ends WHERE IT BEGAN: the
 * target is the window the icons came out of, the offset is zero, and
 * nothing moves.
 *
 * So while the pointer is over the host window, two of the three
 * _GetMouse calls are answered:
 *
 *   the ACTION procedure's is told the point the drag started from, so
 *     the target is the window the icons are in;
 *   the FINAL one, if that is the target, is told the start point too
 *     (inside any slop region, since the press was there), and the offset
 *     the outline summed is cleared in the frame;
 *   the OUTLINE is told the truth, so the Finder's outline follows the
 *     pointer over the host window as it does everywhere else.
 *
 * Every other call reaches the real trap untouched, which keeps a drag to
 * the Trash or to the desktop the Finder's alone.
 *
 * The three calls are told apart without any address written down.  The
 * action procedure's call returns into the action procedure, which starts
 * at the address the frame holds at A6+8 and ends at the _FindWindow call
 * site recognised below.  The other two return into the drag routine:
 * the outline's site is LEARNED as the call made just before each action
 * procedure's (the same site every pass, or the drag is not understood),
 * and the final call is one from any other site, after that, within the
 * same routine.  A drag whose calls do not fit that shape is not answered
 * again, and its drop is not taken.
 *
 * ---- recognising the drag ----
 *
 * The drag is recognised at the action procedure's _FindWindow, by two
 * things the wrong caller cannot both have:
 *
 *   its CALL SITE: the twelve bytes around the trap, which occur exactly
 *     once in every Finder from 5.1 to 6.1.8 and nowhere in System 7's
 *     (NOTES.md, "Neither Finder calls `_DragGrayRgn`: each drags an icon
 *     with a routine of its own, and every target it can choose does
 *     something");
 *
 *   its FRAME: the drag routine links A6 and the action procedure does
 *     not, so the caller's A6 is the drag routine's frame, whose first
 *     parameter is the action procedure itself, which must start just
 *     before the call site.
 *
 * A frame that passes both holds the drag's start point (the press) at
 * A6+22 and its result at A6+30.
 */
#define kDragAction   8       /* the action procedure, from the frame */
#define kDragStart    22      /* startPt, global, v then h */
#define kDragResult   30      /* the offset, v then h, summed by the outline */
#define kDragFrame    34      /* everything up to and including the result */
#define kDragSpan     0x100L  /* how far one routine's call sites may lie apart */

#if FDROP_OWN_ROUTE
/* Whether a _FindWindow call, returning to `ret` with the caller's A6 at
   `a6`, is the action procedure of a Finder's drag (above). */
static Boolean finder_drag_call(long ret, long a6)
{
    const unsigned char *r = (const unsigned char *)ret;
    const unsigned char *f = (const unsigned char *)a6;
    long proc;

    /* Nearly every call is somebody else's and leaves at the heap test,
       so the diagnostic counts are only readable against each other. */
    DGINC(kDgFwCalls);
    if ((ret & 1L) != 0 || (a6 & 1L) != 0) {
        DGINC(kDgFwOdd);
        return false;
    }
    if (r - 8 < heap_start() || r + 4 > heap_end()) {
        DGINC(kDgFwNotHeap);
        return false;
    }
    if (a6 < *(long *)kHeapEndAddr
        || a6 + kDragFrame > *(long *)kCurStackBaseAddr) {
        DGINC(kDgFwNotStack);
        DG(kDgFwA6, (short)(a6 >> 4));
        DG(kDgFwHeapEnd, (short)(*(long *)kHeapEndAddr >> 4));
        DG(kDgFwStackBase, (short)(*(long *)kCurStackBaseAddr >> 4));
        return false;
    }
    if (blob_long(r - 8) != 0x2F04486FL     /* move.l d4,-(sp); pea 6(sp) */
        || blob_long(r - 4) != 0x0006A92CL  /* ...; _FindWindow */
        || blob_long(r) != 0x301F225FL) {   /* move.w (sp)+,d0; movea.l (sp)+,a1 */
        DGINC(kDgFwNotSite);
        return false;
    }
    proc = blob_long(f + kDragAction);
    DG(kDgFwSiteDist, (short)(ret - proc));
    if (!(ret - proc > 0L && ret - proc < kDragSpan)) {
        DGINC(kDgFwNotProc);
        return false;
    }
    return true;
}


/*
 * Whether a record is a DISK's.  On an HFS volume its parent is 1, which
 * only a volume's root can have.  An MFS volume has no directories and
 * every record of it has parent 0, the disk's too: there the record's
 * kind says disk, and the volume must be MFS by its VCB.
 */
static Boolean disk_record(const unsigned char *nm)
{
    long par = blob_long(nm + kRecParent);

    if (par == kRootParID)
        return true;
    return (Boolean)(par == 0L
                     && rec_word(nm - kRecName + kItemKind) == kKindDisk
                     && mfs_volume(rec_word(nm + kRecVol)));
}

/*
 * The desktop's own record, as a handle: the object every disk is drawn
 * in.  Every disk's record (disk_record) must name the same container.
 * 0 if they do not, if there is none, or if the heap could not be walked.
 */
static long desktop_handle(void)
{
    const unsigned char *at = rec_first();
    const unsigned char *nm;
    long h, desk = 0;

    while ((nm = rec_next(&at, &h)) != 0) {
        long c;

        if (!disk_record(nm))
            continue;
        c = blob_long(nm + kRecCont);
        if (desk != 0 && c != desk)
            return 0;
        desk = c;
    }
    return at == 0 ? 0 : desk;
}

/*
 * Whether icons in this container are on the screen: the desktop's, or a
 * visible window's.  A record keeps its selected flag after its window is
 * closed, so only these count when the selection is read.
 */
static Boolean shown_in(long cont, long desk)
{
    WindowPeek w;

    if (cont == desk)
        return true;
    for (w = *(WindowPeek *)kWindowListAddr; w != 0;
         w = (WindowPeek)w->nextWindow) {
        if (w->visible && w->windowKind >= kUserKind && w->refCon == cont)
            return true;
    }
    return false;
}

/*
 * A selected item of an MFS volume, in a window of volume `vol` or on the
 * desktop (`onDesk`): a folder as the files in it (mfs_folder), and a file
 * once the File Manager agrees on its number (mfs_take).  Its record must
 * put it on that volume, and a file's must say fOnDesk exactly when it is
 * on the desktop, as takeable asks of an HFS one.  Answers 0, or the step
 * that refuses the drag.
 */
static short mfs_item(const unsigned char *nm, long h, short vol,
                      Boolean onDesk, short base, short *n, short room)
{
    const unsigned char *d = nm - kRecName;

    if (rec_word(nm + kRecVol) != vol)
        return kStepNotTakeable;        /* somewhere else, by its record */
    if (rec_word(d + kItemKind) == kKindFolder)
        return mfs_folder(h, kRecName, base, n, room);
    if (onDesk != ((rec_word(nm + kRecFlags) & kOnDesk) != 0))
        return kStepNotTakeable;        /* on the desktop, or not, wrongly */
    return mfs_take(d, kRecName, base, n, room);
}

/* The record whose handle this is, or 0. */
static const unsigned char *rec_by_handle(long want)
{
    const unsigned char *at = rec_first();
    const unsigned char *nm;
    long h;

    while ((nm = rec_next(&at, &h)) != 0) {
        if (h == want)
            return nm;
    }
    return 0;
}

/*
 * Which files the drag is carrying, and whether the host can take them.
 *
 * Called once per drag, the first time the pointer is over the host
 * window, from inside the drag.  The Finder is in its own loop until the
 * button comes up, so the selection read here is the one being dragged
 * and nothing can change it meanwhile.  What is found is written past
 * the queue's depth, and counted in only once the drop is known to have
 * been on the host window (fdrop_idle).
 *
 * WHERE the drag started decides the container.  In a Finder window, the
 * window's refCon is the handle of its folder's or disk's own record; the
 * record must carry the window's TITLE as its name, and the directory it
 * shows comes from the File Manager (container_dir).  With no window
 * under the press it is the desktop, whose record every disk is drawn
 * in; there each file is in the root of its own volume.  An item of an
 * MFS volume, in its window or on the desktop, is checked by its file
 * number instead, and a folder of one is its files (mfs_item).
 *
 * The files are the SELECTION, because that is what the Finder drags.
 * One walk of the Finder's records checks all of it:
 *
 *   exactly ONE record in that container is under the press, and it is
 *     selected (the drag cannot be carrying an unselected icon);
 *   every selected record in that container is taken, each a visible file
 *     or folder the File Manager finds where the record says (takeable),
 *     and on the desktop one whose record says fOnDesk, or a disk, or the
 *     Trash;
 *   NOTHING is selected anywhere else on the screen, because the Finder
 *     would be carrying that too, and the host could take only part of
 *     the drag.
 *
 * WHOLE OR NOTHING.  A refused drop costs the user a second try; a wrong
 * one hands the host a file nobody dropped.
 *
 * Stages: the desktop's record; the container under the press; one walk
 * over every record, writing the takeable ones past the depth; then the
 * checks on what the walk saw.
 */
static short resolve_drag(void)
{
    WindowPeek w;
    Point      start, at, org;
    const unsigned char *pos, *nm, *walk;
    unsigned char vn[32];
    long       cont, desk, dirID = 0, h;
    short      vol = 0, base, room, n = 0, hits = 0, k, seen = 0;
    Boolean    onDesk, pressedSel = false;

    FDROP_STATE = kDragRefused;         /* refused, unless all of it holds */
    FDROP_PEND = 0;
    stk_note();

    desk = desktop_handle();
    if (desk == 0) {
        return kStepHeapUnwalkable;
    }
    start.v = (short)((FDROP_START >> 16) & 0xFFFFL);
    start.h = (short)(FDROP_START & 0xFFFFL);
    w = window_at(FDROP_START);
    if (w == 0) {
        /* No window there: the desktop, the only other place a Finder
           icon can be pressed on.  Its icons are drawn in global
           coordinates, which is what the press is in. */
        onDesk = true;
        cont = desk;
        at = start;
    } else {
        const unsigned char *cn;

        if (w->windowKind < kUserKind || (long)w == FDROP_WIN || w->refCon == 0
            || w->titleHandle == 0 || *w->titleHandle == 0) {
            return kStepNotInFinderWindow;
        }
        onDesk = false;
        cont = w->refCon;
        org = port_origin(&w->port);
        at.h = (short)(start.h + org.h);
        at.v = (short)(start.v + org.v);
        if (!PtInRect(at, &w->port.portRect)) {
            return kStepNotInFinderWindow;
        }
        cn = rec_by_handle(cont);
        if (cn == 0 || !same_pstr(cn, (const unsigned char *)*w->titleHandle)
            || (dirID = container_dir(cn, &vol)) == 0) {
            return kStepFolderUnnamed;
        }
    }
    DG(kDgPressV, at.v);
    DG(kDgPressH, at.h);
    DG(kDgFolder, (short)(dirID & 0xFFFFL));
    DG(kDgVolume, vol);

    base = drops_queued();
    room = (short)(queue_cap() - base);
    walk = rec_first();
    while ((nm = rec_next(&walk, &h)) != 0) {
        long    c = blob_long(nm + kRecCont);
        Boolean sel = (Boolean)((nm[kSelOff] & kSelBit) != 0);
        short   v = vol, top, left;
        long    d = dirID;

        seen = (short)(seen + 1);
        if (c != cont) {
            if (sel && shown_in(c, desk)) {
                return kStepSelectedElsewhere;
            }
            continue;
        }
        pos = nm + kRecPos;
        top = rec_word(pos);
        left = rec_word(pos + 2);
        if (at.v >= top && at.v < top + kIconSize
            && at.h >= left && at.h < left + kIconSize) {
            hits = (short)(hits + 1);
            pressedSel = sel;
        }
        if (!sel)
            continue;
        if (onDesk && rec_word(nm + kRecVol) == 0
            && blob_long(nm + kRecParent) == 0L) {
            /*
             * The TRASH: no volume and no parent, because one icon
             * stands for every disk's.  What was thrown away stays where
             * it was until the Trash is emptied, drawn in the Trash.  The
             * MFS files in it are handed over as the files, as an MFS
             * folder's are.  Beside them, the Trash is handed over as the
             * Drag Manager hands it, as the startup disk's Trash folder;
             * where that does not exist it is left out, and the rest of
             * the drag still arrives.
             */
            k = mfs_folder(h, kRecName, base, &n, room);
            if (k != 0) {
                return k;
            }
            if (!startup_trash(nm, &v))
                continue;
            d = kRootDirID;
            k = 0;
        } else if (onDesk && disk_record(nm)) {
            /*
             * A DISK, its vRefNum the volume's.  Handed over as the Drag
             * Manager hands one (the volume, directory 1 and the
             * volume's name) once the File Manager agrees the volume is
             * mounted under the name the icon carries.  Any file system:
             * nothing here reads a catalog.
             */
            v = rec_word(nm + kRecVol);
            d = kRootParID;
            if (!vol_name(v, vn) || !same_pstr(vn, nm)) {
                DG(kDgRefTest, kTakeWrongVol);
                return kStepNotTakeable;
            }
            k = 0;
        } else {
            if (onDesk) {
                v = rec_word(nm + kRecVol);
                d = mfs_volume(v) ? kMfsDir : kRootDirID;
                if (d == kRootDirID && !hfs_volume(v, vn)) {
                    return kStepNotTakeable;
                }
            }
            if (d == kMfsDir) {
                k = mfs_item(nm, h, v, onDesk, base, &n, room);
                if (k != 0) {
                    return k;
                }
                continue;               /* queued by mfs_item */
            }
            k = takeable(nm, v, d, onDesk);
        }
        if (k != 0) {
            DG(kDgRefName, (short)(((unsigned short)nm[1] << 8) | nm[2]));
            DG(kDgRefParent, (short)(blob_long(nm + kRecParent) & 0xFFFFL));
            DG(kDgRefVol, rec_word(nm + kRecVol));
            DG(kDgRefTest, k);
            return kStepNotTakeable;
        }
        if (d == kAskDir)
            d = blob_long(nm + kRecParent);
        for (k = 0; k < n; k++) {       /* the same item already queued? */
            const unsigned char *e = queue_at((short)(base + k));

            if (rec_word(e + 4) == v && blob_long(e + 6) == d
                && same_pstr(e + 10, nm)) {
                return kStepOneItemTwice;
            }
        }
        if (n >= room) {
            return kStepNoRoom;
        }
        put_drop((short)(base + n), v, d, nm);
        n = (short)(n + 1);
    }
    DG(kDgRecords, seen);
    DG(kDgHits, hits);
    if (walk == 0) {
        return kStepHeapUnwalkable;
    }
    if (hits != 1) {
        return kStepNotOneIcon;
    }
    if (!pressedSel) {
        return kStepIconNotSelected;
    }
    if (n == 0) {
        return kStepNothingToHand;
    }
    FDROP_PEND = n;
    FDROP_PENDAT = base;
    FDROP_STATE = kDragCarries;
    return kStepFilesFound;
}

#endif  /* FDROP_OWN_ROUTE */

#if FDROP_OWN_ROUTE
/*
 * ---- a drag released within a pass ----
 *
 * A quick flick of an icon can be over before the drag routine has made
 * a second pass, or released between two calls of one pass.  Two rules
 * keep such a drag the one it was:
 *
 *   The RELEASE of the press being followed is not a new drag: MBTicks
 *   moves on, but the button is up, later than the press, the drag's
 *   final call has not been seen, and the frame holds the same start
 *   point (same_gesture).  A new press moves MBTicks with the button
 *   down, or ends a drag whose final call was seen.
 *
 *   A new drag of the routine already recognised is armed at its FIRST
 *   action-procedure _GetMouse (fdrop_answer), not at the _FindWindow
 *   after it: the frame's action procedure must hold the very call site
 *   recognised before (finder_drag_call on FDROP_FWRET), and the call
 *   must return into it.  The outline's site, learned in that same
 *   routine, is kept.  A drag released before its first pass had no
 *   answered call at all, and the Finder took the host window for the
 *   desktop (NOTES.md, "A quick drag is taken").
 */
static Boolean same_gesture(const unsigned char *f, long mb)
{
    if (mb == FDROP_GEST)
        return true;
    return (Boolean)((*(unsigned char *)kMBStateAddr & kButtonUp) != 0
                     && mb - FDROP_GEST > 0L && FDROP_FIN == kFinNotYet
                     && blob_long(f + kDragStart) == FDROP_START);
}

/* A new drag, in the frame at `a6`, whose action procedure's _FindWindow
   returns to `fw`. */
static void drag_arm(const unsigned char *f, long a6, long fw, long mb)
{
    if (fw != FDROP_FWRET)
        FDROP_OUTSITE = 0;              /* another routine: learned again */
    FDROP_DRAGRET = blob_long(f + 4);
    FDROP_START   = blob_long(f + kDragStart);
    FDROP_GEST    = mb;
    FDROP_FWRET   = fw;
    FDROP_LASTGM  = 0;
    FDROP_STATE   = kDragUnread;
    FDROP_PEND    = 0;
    FDROP_FAKED   = 0;
    FDROP_ACT     = 0;
    FDROP_OVER    = 0;
    FDROP_FIN     = kFinNotYet;
    FDROP_SEQ     = (short)(FDROP_SEQ + 1);
    STEP(kStepRecognised);
    FDROP_DRAGFP  = a6;                 /* last: it is what arms _GetMouse */
}
#endif  /* FDROP_OWN_ROUTE */

/*
 * Called from the _FindWindow patch with the point, the caller's return
 * address and the caller's A6.  It watches, and never changes the answer.
 *
 * Nearly every call is somebody else's and leaves at the first test.  A
 * call from the action procedure of a Finder's drag records the drag, if
 * it is a new one, and whether the action procedure's own _GetMouse was
 * answered (FDROP_ACT).  That says the Finder's target, should the
 * button come up now, is the window the icons came from; the drop is
 * ours only if it is.
 */
void fdrop_look(long packed, long ret, long a6);

void fdrop_look(long packed, long ret, long a6)
{
#if FDROP_OWN_ROUTE
    const unsigned char *f = (const unsigned char *)a6;
    long mb;

    /* A drag through the ROM's DragTheRgn is followed from the ROM's end
     * (rom_answer).  Finder 5.1 has this call site too, and two
     * followers of one drag would each undo what the other decided. */
#if FDROP_ROM_ROUTE
    if (FDROP_ROMMODE != 0)
        return;
#endif
    if (!finder_drag_call(ret, a6))
        return;
    mb = *(long *)kMBTicksAddr;
    if (a6 != FDROP_DRAGFP || blob_long(f + 4) != FDROP_DRAGRET
        || !same_gesture(f, mb))
        drag_arm(f, a6, ret, mb);       /* a new drag */
    else if (ret != FDROP_FWRET || FDROP_FIN != kFinNotYet) {
        FDROP_FIN = kFinBadShape;       /* a second call site, or a pass
                                           after the release */
    }
    FDROP_ACT = (short)(FDROP_FAKED != 0 && packed == FDROP_START
                        && FDROP_FIN == kFinNotYet);
#else   /* no route recognised at _FindWindow: fdrop.S does not call this */
    (void)packed;
    (void)ret;
    (void)a6;
#endif
}

/*
 * The point the drag started from, in the caller's port: what _GetMouse
 * would say had the pointer been there.
 */
static void answer_start(Point *pt)
{
    GrafPtr port;
    Point   org;

    GetPort(&port);
    org = port_origin(port);
    pt->v = (short)(((FDROP_START >> 16) & 0xFFFFL) + org.v);
    pt->h = (short)((FDROP_START & 0xFFFFL) + org.h);
}

/*
 * Whether the pointer is over the host window, asked from inside the
 * Finder's drag.  Two arrangements, one function:
 *
 *   The host window IS in the window list the drag runs with: the plain
 *   Finder, or a desk accessory opened into the Finder's layer under
 *   MultiFinder.  The pointer is over it when window_at finds it and the
 *   point is in its content.  The frame and title bar are left to the
 *   Finder, as in the other arrangement, so a release means the same
 *   thing in both.
 *
 *   It is NOT in that list: MultiFinder, with the host in another layer.
 *   Each layer has a window list of its own, so the Finder's holds only
 *   the Finder's windows.  What MultiFinder does keep is every window's
 *   visRgn, clipped by every window in front of it in any layer.  So the
 *   pointer is over the host window when no Finder window is there and
 *   the point is in its visible region (NOTES.md, "Under MultiFinder each
 *   layer has a window list of its own, and a background window's visRgn
 *   is clipped by every layer in front").  The frame and title bar are
 *   not in a visRgn, so a release there is the Finder's.
 *
 * kDgOverWay says which of the two answered yes.  ("Panel" in the name
 * means the host window.)
 */
#if FDROP_ROMPLUS       /* another layer's window list: MultiFinder's alone */
static Boolean listed(WindowPeek want)
{
    WindowPeek w;

    for (w = *(WindowPeek *)kWindowListAddr; w != 0;
         w = (WindowPeek)w->nextWindow) {
        if (w == want)
            return true;
    }
    return false;
}
#endif

static Boolean over_panel(void)
{
    long       packed = *(long *)kMouseCooked;
    WindowPeek host = (WindowPeek)FDROP_WIN;
    WindowPeek w;
#if FDROP_ROMPLUS
    Point      p, org;
#endif

    if (host == 0)
        return false;
    w = window_at(packed);
    if (w == host) {
        Point g;

        g.v = (short)((packed >> 16) & 0xFFFFL);
        g.h = (short)(packed & 0xFFFFL);
        if (host->contRgn == 0 || !PtInRgn(g, host->contRgn))
            return false;
        DG(kDgOverWay, 1);
        return true;
    }
#if !FDROP_ROMPLUS
    return false;                       /* no MultiFinder: the list is all */
#else
    if (w != 0 || listed(host) || !host->visible
        || host->port.visRgn == 0)
        return false;
    org = port_origin(&host->port);
    p.v = (short)(((packed >> 16) & 0xFFFFL) + org.v);
    p.h = (short)((packed & 0xFFFFL) + org.h);
    if (!PtInRgn(p, host->port.visRgn))
        return false;
    DG(kDgOverWay, 2);
    return true;
#endif
}

#if FDROP_DIAG
/*
 * kDgSites: up to four distinct _GetMouse call sites, each as the return
 * address's high word, low word and a count.  It says which loop a
 * machine is in, read off a snapshot of memory rather than off the
 * screen.  Called from fdrop.S.
 */
void fdrop_dg_site(long ret);

void fdrop_dg_site(long ret)
{
    short i;

    for (i = 0; i < 4; i++) {
        short *slot = &FDROP_DG[kDgSites + 3 * i];
        long   have = ((long)(unsigned short)slot[0] << 16)
                      | (long)(unsigned short)slot[1];

        if (have == ret) {
            slot[2] = (short)(slot[2] + 1);
            return;
        }
        if (have == 0L) {
            slot[0] = (short)((ret >> 16) & 0xFFFFL);
            slot[1] = (short)(ret & 0xFFFFL);
            slot[2] = 1;
            return;
        }
    }
}
#endif

/* ------------------------------------------------------------------ */
/* A Finder drag made by the ROM's own DragTheRgn                      */
/* ------------------------------------------------------------------ */

/*
 * FDROP_ROM_ROUTE (fdrop.h): all of this section, to just above
 * fdrop_answer, is the route for the Finders that drag through the ROM,
 * 1.1g to 5.3.  A host that never runs below System 4.0 builds without
 * it.
 */
#if FDROP_ROM_ROUTE

/*
 * The Finders up to 5.3 carry no drag routine of their own: they call
 * the ROM's _DragGrayRgn.  Its DragTheRgn has the frame the later
 * Finders copied (action procedure at A6+8, start point at +22, result
 * at +30), and calls _GetMouse at two places: in its loop (the outline,
 * returning to +0x50) and once after the release (+0xCA).  The 64K ROM
 * and the Plus ROM have the same routine (NOTES.md, "System 1.1 takes a
 * drop: Finder 1.1g's drag is followed from the ROM end").
 *
 * There is no _FindWindow in the action procedure to recognise the drag
 * by, and the action procedure runs with A6 pointing at the Finder's
 * frame, not DragTheRgn's.  So the drag is followed from the ROM end:
 *
 *   ARMED by the outline's _GetMouse, whose caller is DragTheRgn itself,
 *     with the frame checked (rom_arm);
 *   the action procedure's _GetMouse is recognised by its return address
 *     lying inside the procedure the frame names;
 *   the final one by its own site.
 *
 * The answers are the same as for the Finders' own drag routines.
 */
#define kRomLoopRet   0x50L   /* the outline's _GetMouse returns here */
#define kRomFinalRet  0xCAL   /* the one after the release */

/*
 * An item's record, as offsets into the block's data, in the two layouts
 * these Finders keep:
 *
 *   Finder 1.1g's: System 1.1, and System 2.0's Finders 1.1 and 4.1;
 *   Finder 5.1's: System 3.0, and Finders 5.0, 5.2 and 5.3 (Systems 2.1,
 *     3.1 and 3.2).
 *
 * They agree up to the container and on the volume; 5.1 moved the
 * selected bit and the file number, and its name is 0x20 further in.  A
 * block is a record, and its layout known, by the test the later
 * Finders' records are found by too: the block ends exactly where the
 * name does.  Then the container is checked, and the File Manager asked
 * (NOTES.md, "Every System from 1.1 to 3.2 takes a drop: two record
 * layouts cover every Finder that drags through the ROM").
 */
#define kRomWhere    0x02     /* the icon's top left, v then h: GLOBAL on the desktop */
/* the byte whose bit kSelBit says selected */
#if FDROP_FINDER5
#define ROM_SEL(nm)  ((nm) == kRom11Name ? 0x0B : 0x15)
#else                /* one record, Finder 1.1g's: fdrop.h, FDROP_FINDER5 */
#define ROM_SEL(nm)  0x0B
#endif
#define kFinderWKind 16       /* a Finder window, as its own hit test asks */
#define kRomIcon     32       /* an icon's square, in pixels */
#define kRom11Sel    0x0B     /* 1.1g's selected byte: its nameless blocks */

/*
 * Whether a block is a DISK's record: kind kKindDisk, and a volume
 * reference number the File Manager knows.  `name` gets the volume's
 * name, off its VCB, because Finder 1.1g's disk record carries none.
 * Finder 5.1's does (at `k`, nonzero), and it must then be the volume's.
 * The vRefNum and the icon's position are where an item's record keeps
 * them.
 */
static Boolean rom_disk(const unsigned char *d, long logical, short k,
                        unsigned char *name)
{
    short v;
    short i;

    if (logical < (long)kItemVol + 2 || rec_word(d + kItemKind) != kKindDisk)
        return false;
    v = rec_word(d + kItemVol);
    if (!vol_name(v, name))
        return false;
#if FDROP_FINDER5       /* only Finder 5.1's disk record carries a name */
    if (k != 0)                         /* a named record: the same name */
        for (i = 0; i <= (short)name[0]; i++)
            if (d[k + i] != name[i])
                return false;
#else
    (void)k;
    (void)i;
#endif
    return true;
}

/* Where the name is, if the block is a record of either layout; else 0. */
static short rom_record(const unsigned char *d, long logical)
{
    short off = kRom11Name;

    for (;;) {
        if (name_ends(d, logical, off))
            return off;
#if FDROP_FINDER5
        if (off == kRom51Name)
            return 0;
        off = kRom51Name;
#else
        return 0;                       /* the one layout this build reads */
#endif
    }
}

/*
 * Whether the container `cont` is the desktop: the ROOT, the one record
 * whose own container is not a handle in the heap.  A disk's record is
 * in it, every window's container is a disk or a folder in turn, and
 * their containers are handles.
 */
static Boolean rom_root(long cont)
{
    const unsigned char *lo = heap_start();
    const unsigned char *hi = heap_end();
    const unsigned char *r;
    long up;

    if ((cont & 1L) != 0 || cont < (long)lo || cont + 4 > (long)hi)
        return false;
    r = (const unsigned char *)(*(long *)cont & 0x00FFFFFFL);
    if (r < lo + 8 || r + kItemCont + 4 > hi)
        return false;
    up = blob_long(r + kItemCont) & 0x00FFFFFFL;
    return (Boolean)(up < (long)lo || up >= (long)hi);
}

/*
 * The desktop's container, found from the press: the SELECTED record in
 * the root whose icon the press was inside.  Positions there are global,
 * as the start point is, so no record in a window can claim the press.
 * 0 when no record does; `*lay` is that record's layout, which is the
 * Finder's.
 */
static long rom_desk(short *lay)
{
    const unsigned char *at = rec_first();
    const unsigned char *d;
    long  h, logical;
    short sv = (short)((FDROP_START >> 16) & 0xFFFFL);
    short sh = (short)(FDROP_START & 0xFFFFL);

    while ((d = blk_next(&at, &h, &logical)) != 0) {
        short k = rom_record(d, logical);
        short v, x;
        unsigned char vn[32];

        if (k == 0 && logical > (long)kItemCont + 4
            && ((rec_word(d + kItemKind) == kKindDisk
                 && rom_disk(d, logical, 0, vn))
                || rec_word(d + kItemKind) == kKindFolder
                || rec_word(d + kItemKind) == kKindTrash))
            k = kRom11Name;     /* 1.1g's nameless disk, folder or Trash */
        else if (k == 0)
            continue;
        if ((d[ROM_SEL(k)] & kSelBit) == 0)
            continue;
        v = rec_word(d + kRomWhere);
        x = rec_word(d + kRomWhere + 2);
        if (sv >= v && sv < v + kRomIcon && sh >= x && sh < x + kRomIcon
            && rom_root(blob_long(d + kItemCont))) {
            *lay = k;
            return blob_long(d + kItemCont);
        }
    }
    return 0L;
}

#if FDROP_SENDER      /* to the end of rom_drag_routine: the sender's alone */
/* Whether the DragTheRgn at `addr` is the routine the frame is known for:
 * LINK A6,#-20, LEA 22(A6),A0, and _GetMouse at +0x4E and +0xC8. */
static long rom_body(unsigned long addr)
{
    const unsigned char *p = (const unsigned char *)addr;

    if (addr == 0UL || (addr & 1UL) != 0UL)
        return 0L;
    if (rec_word(p) != 0x4E56 || rec_word(p + 2) != (short)0xFFEC
        || rec_word(p + 8) != 0x41EE || rec_word(p + 10) != 0x0016
        || rec_word(p + 0x4E) != (short)0xA972
        || rec_word(p + 0xC8) != (short)0xA972)
        return 0L;
    return (long)addr;
}

/*
 * The ROM routine, either as the trap names it or behind System 3's
 * patch on the Plus ROM, which is exactly:
 *
 *     LINK    A6,#-24
 *     LEA     wrapper,A0
 *     MOVE.L  8(A6),-24(A6)      ; the caller's action procedure, kept
 *     MOVE.L  A0,8(A6)           ; and the wrapper put in its place
 *     JMP     rom+4              ; past the ROM's own LINK
 *
 * so the frame is the ROM routine's with one long more, every _GetMouse
 * still returns inside the ROM routine, and the caller's procedure is at
 * A6-24 whenever the slot holds the wrapper.  FDROP_ROMWRAP says which.
 */
static long rom_drag_routine(unsigned long addr)
{
    const unsigned char *p = (const unsigned char *)addr;
    long rom;

    if (addr == 0UL || (addr & 1UL) != 0UL)
        return 0L;
#if !(FDROP_ROMPLUS && FDROP_FINDER5)
    /* That patch is System 3's, on the Plus ROM: without both, it is
     * never there.  No System patches the 64K ROM's routine, so the
     * routine is the trap's, or it is not followed. */
    (void)p;
    (void)rom;
    return rom_body(addr);
#else
    if (rec_word(p) != 0x4E56 || rec_word(p + 2) != (short)0xFFE8
        || rec_word(p + 4) != 0x41FA
        || rec_word(p + 8) != 0x2D6E || rec_word(p + 10) != 0x0008
        || rec_word(p + 12) != (short)0xFFE8
        || rec_word(p + 14) != 0x2D48 || rec_word(p + 16) != 0x0008
        || rec_word(p + 18) != 0x4EF9)
        return rom_body(addr);
    rom = rom_body((unsigned long)blob_long(p + 20) - 4UL);
    if (rom != 0L)
        FDROP_ROMWRAP = (long)addr + 6L + rec_word(p + 6);
    return rom;
#endif
}
#endif  /* FDROP_SENDER */

#define kRomKept    (-24)     /* where System 3's patch keeps the caller's proc */

/* The caller's action procedure, from a frame of the ROM routine. */
static long rom_proc(const unsigned char *f)
{
    long proc = blob_long(f + kDragAction);

#if FDROP_ROMPLUS && FDROP_FINDER5      /* System 3's wrapper, on the Plus ROM */
    if (FDROP_ROMWRAP != 0L && proc == FDROP_ROMWRAP)
        proc = blob_long(f + kRomKept);
#endif
    return proc;
}


/*
 * What the press was on: every SELECTED item of the window it was in,
 * each a file the File Manager finds by its number, or nothing.
 * FDROP_STATE ends as kDragNotIcons when this is not a drag of the
 * Finder's icons at all, and the drag is left alone; kDragRefused when it
 * is and is refused whole, so the Finder is told the drop was where it
 * began and puts the icons back; kDragCarries when all of it holds.
 */
static short resolve_rom(void)
{
    WindowPeek           w = window_at(FDROP_START);
    const unsigned char *at = rec_first();
    const unsigned char *d;
    long                 cont, h, logical;
    short                base = drops_queued();
    short                room = (short)(queue_cap() - base);
    short                n = 0;
    short                lay = 0;

    FDROP_STATE = kDragNotIcons;
    FDROP_PEND = 0;
    stk_note();
    if (w == 0) {
        cont = rom_desk(&lay);
        if (cont == 0L) {
            return kStepRomDeskNoIcon;
        }
    } else if (w->windowKind != kFinderWKind || (long)w == FDROP_WIN
               || w->refCon == 0) {
        return kStepRomNotFinderWindow;
    } else
        cont = w->refCon;
    while ((d = blk_next(&at, &h, &logical)) != 0) {
        short k;

        if (logical < (long)kRom11Name || blob_long(d + kItemCont) != cont)
            continue;
        k = rom_record(d, logical);
        if (w == 0 && rec_word(d + kItemKind) == kKindDisk
            && (d[ROM_SEL(k != 0 ? k : lay)] & kSelBit) != 0) {
            /*
             * A DISK on the desktop, selected: handed over as the Drag
             * Manager hands one (its vRefNum, directory 1 and its name)
             * once the File Manager knows it by that number.
             */
            unsigned char vn[32];

            FDROP_STATE = kDragRefused;
            if (!rom_disk(d, logical, k, vn)) {
                return kStepRomUnknownDisk;
            }
            if (n >= room) {
                return kStepNoRoom;
            }
            put_drop((short)(base + n), rec_word(d + kItemVol), kRootParID, vn);
            n = (short)(n + 1);
            continue;
        }
        if ((rec_word(d + kItemKind) == kKindFolder
             || rec_word(d + kItemKind) == kKindTrash)
            && (d[k != 0 ? ROM_SEL(k) : kRom11Sel] & kSelBit) != 0) {
            short step;

            /*
             * A selected folder, or the TRASH, which on an MFS disk is
             * the same kind of thing: what arrives is the files in it.
             * One with nothing in it adds nothing and the rest of the
             * drag still arrives.
             */
            FDROP_STATE = kDragRefused;
            step = mfs_folder(h, 0, base, &n, room);
            if (step != 0) {
                return step;
            }
            continue;
        }
        if (k == 0) {
            if (w != 0 || (d[ROM_SEL(lay)] & kSelBit) == 0)
                continue;               /* not an item's record */
            FDROP_STATE = kDragRefused;
            return kStepRomUnknownIcon;
        }
        if ((d[ROM_SEL(k)] & kSelBit) == 0)
            continue;
        FDROP_STATE = kDragRefused;     /* the Finder's: all of it, or none */
        {
            short step = mfs_take(d, k, base, &n, room);

            if (step != 0) {
                return step;
            }
        }
    }
    if (at == 0) {
        FDROP_STATE = kDragNotIcons;
        return kStepHeapUnwalkable;
    }
    if (n == 0) {
        return kStepRomNothingSelected;
    }
    FDROP_PEND = n;
    FDROP_PENDAT = base;
    FDROP_STATE = kDragCarries;
    return kStepRomItemsFound;
}

/*
 * The outline's _GetMouse, from inside DragTheRgn: the first one of a
 * drag arms it.  The frame must be on the stack, DragTheRgn called from
 * the Finder's heap with an action procedure there too, the button down,
 * and the current application the Finder.  A drag through the same ROM
 * routine by any other caller (DragWindow, another program) is not one
 * of these.
 */
static void rom_arm(long a6, long mb)
{
    const unsigned char *f = (const unsigned char *)a6;
    long caller, proc;

    if ((a6 & 1L) != 0 || a6 < *(long *)kHeapEndAddr
        || a6 + kDragFrame > *(long *)kCurStackBaseAddr)
        return;
    caller = blob_long(f + 4);
    proc = rom_proc(f);
    if (caller < (long)heap_start() || caller >= (long)heap_end()
        || proc < (long)heap_start() || proc >= (long)heap_end()
        || (*(unsigned char *)kMBStateAddr & kButtonUp) != 0
        || !fdrop_shell_is_current())
        return;
    FDROP_DRAGRET = caller;
    FDROP_START   = blob_long(f + kDragStart);
    FDROP_GEST    = mb;
#if FDROP_OWN_ROUTE      /* the other route's words, set as it expects them */
    FDROP_FWRET   = 0;
    FDROP_OUTSITE = FDROP_ROMDRAG + kRomLoopRet;
    FDROP_LASTGM  = 0;
#endif
    FDROP_STATE   = kDragUnread;
    FDROP_PEND    = 0;
#if FDROP_OWN_ROUTE
    FDROP_FAKED   = 0;
#endif
    FDROP_ACT     = 0;
    FDROP_OVER    = 0;
    FDROP_FIN     = kFinNotYet;
    FDROP_SEQ     = (short)(FDROP_SEQ + 1);
    STEP(kStepRomArmed);
    FDROP_DRAGFP  = a6;
    FDROP_ROMMODE = 1;
}

/* fdrop_answer for a drag through the ROM's DragTheRgn. */
static long rom_answer(Point *pt, long ret, long a6)
{
    const unsigned char *f;
    long    mb = *(long *)kMBTicksAddr;
    long    proc;
    Boolean held = (Boolean)(mb == FDROP_GEST);
    Boolean released;

    if (ret == FDROP_ROMDRAG + kRomLoopRet) {
        /* The outline's: never answered, so it follows the pointer. */
        if (FDROP_ROMMODE == 0 || a6 != FDROP_DRAGFP || !held
            || blob_long((const unsigned char *)a6 + 4) != FDROP_DRAGRET)
            rom_arm(a6, mb);
        return 0;
    }
    if (FDROP_ROMMODE == 0)
        return 0;
    f = (const unsigned char *)FDROP_DRAGFP;
    released = (Boolean)(!held
                         && (*(unsigned char *)kMBStateAddr & kButtonUp) != 0
                         && mb - FDROP_GEST > 0L);
    if (blob_long(f + 4) != FDROP_DRAGRET || (!held && !released)) {
        FDROP_ROMMODE = 0;                /* not the drag this was following */
        FDROP_DRAGFP = 0;
        return 0;
    }
    if (ret == FDROP_ROMDRAG + kRomFinalRet && a6 == FDROP_DRAGFP) {
        /* The last one, after the release. */
        FDROP_ROMMODE = 0;
        FDROP_DRAGFP = 0;
        if (!released) {
            FDROP_FIN = kFinBadShape;
            return 0;
        }
        FDROP_FIN = kFinSeen;
        FDROP_OVER = over_panel();
        DG(kDgEnd, (short)(1 + (FDROP_OVER ? 100 : 0)));
        if (!FDROP_ACT)
            return 0;                   /* the Finder's own target */
        set_blob_long((unsigned char *)f + kDragResult, 0L);
        DGINC(kDgGetMouse);
        answer_start(pt);
        return 1;
    }
    proc = rom_proc(f);
    if (ret - proc > 0L && ret - proc < kDragSpan) {
        /*
         * The action procedure's, on whatever frame it chose.  Its hit
         * test uses this point and no other, so answering the start point
         * makes the window the icons came from the target, and not
         * answering lets the real one stand.  So FDROP_ACT is decided here
         * every time.
         */
#if FDROP_OWN_ROUTE
        FDROP_FAKED = 0;
#endif
        FDROP_ACT = 0;
        if (!over_panel())
            return 0;
        if (FDROP_STATE == kDragUnread) {
            short k = resolve_rom();

            STEP(k);
            FDROP_OVERFLOW = (short)(k == kStepNoRoom);
        }
        if (FDROP_STATE == kDragNotIcons)
            return 0;                   /* not the Finder's icons */
        DGINC(kDgGetMouse);
#if FDROP_OWN_ROUTE
        FDROP_FAKED = 1;
#endif
        FDROP_ACT = 1;
        answer_start(pt);
        return 1;
    }
    return 0;
}

#endif  /* FDROP_ROM_ROUTE */

/*
 * Called from the _GetMouse patch, only when the caller's A6 is the frame
 * fdrop_look (or rom_arm) recorded, with the caller's return address.
 * Answers 1 having written the point, or 0 to let the real trap answer.
 *
 * The frame is checked again here, against what it held when it was
 * recorded: a frame address is reused by the next routine called from the
 * same depth, but its return slot and the gesture's tick are not.  The
 * tick is MBTicks, which the release moves on as well as the press, so
 * the final call (made after the release) is matched by a button that is
 * up and a tick later than the press.
 *
 * Then which of the drag's three calls this is, by where it returns to
 * ("how the Finder drags an icon", above):
 *
 *   the ACTION procedure's, returning between the procedure's start and
 *     its _FindWindow: answered with the start point while the pointer is
 *     over the host window, and only once the outline's site is known;
 *   the OUTLINE's, the drag routine's call from the site learned at the
 *     action procedure's: never answered;
 *   the FINAL one, the drag routine's call from any other site after the
 *     outline's: answered, and the offset cleared, if the last action
 *     procedure's was, whatever the pointer is over, because the Finder's
 *     target is then one this code chose.  FDROP_OVER says whether the
 *     release was over the host window, the other half of a drop.
 *
 * Any other call in this frame is a drag of a shape this code does not
 * know: it is not answered again and its drop is not taken.
 */
long fdrop_answer(Point *pt, long ret, long a6);

long fdrop_answer(Point *pt, long ret, long a6)
{
#if FDROP_OWN_ROUTE
    const unsigned char *f = (const unsigned char *)a6;
    long proc, site, mb;
    Boolean held;
#endif

#if FDROP_ROM_ROUTE
    if (FDROP_ROMMODE != 0
        || (FDROP_ROMDRAG != 0L
            && (unsigned long)(ret - FDROP_ROMDRAG) < 0x100UL))
        return rom_answer(pt, ret, a6);
#endif
#if FDROP_OWN_ROUTE
    if (blob_long(f + 4) != FDROP_DRAGRET)
        return 0;
    mb = *(long *)kMBTicksAddr;
    proc = blob_long(f + kDragAction);
    if (!same_gesture(f, mb) && FDROP_FWRET != 0L
        && ret - proc > 0L && ret < FDROP_FWRET
        && finder_drag_call(FDROP_FWRET, a6))
        drag_arm(f, a6, FDROP_FWRET, mb);   /* a new drag, at its first
                                               action procedure */
    /*
     * The same press, still held, or the release that ends it, which
     * moves MBTicks on: a button that is up, later than the press.
     */
    held = (Boolean)(mb == FDROP_GEST);
    if (!held && ((*(unsigned char *)kMBStateAddr & kButtonUp) == 0
                  || mb - FDROP_GEST <= 0L))
        return 0;
    if (FDROP_FIN != kFinNotYet) {
        FDROP_FIN = kFinBadShape;       /* a call after the final one */
        return 0;
    }

    if (ret - proc > 0L && ret < FDROP_FWRET) {
        /* The action procedure's, held or released. */
        site = FDROP_LASTGM;
        FDROP_LASTGM = 0;
        FDROP_FAKED = 0;
        FDROP_ACT = 0;
        if (site == 0) {
            if (FDROP_OUTSITE == 0)
                return 0;               /* nothing before it to learn from */
        } else if (FDROP_OUTSITE == 0) {
            FDROP_OUTSITE = site;
            DG(kDgOutlineSite, (short)(site - proc));
        } else if (site != FDROP_OUTSITE) {
            FDROP_FIN = kFinBadShape;
            DG(kDgEnd, 2);
            return 0;
        }
        if (!over_panel())
            return 0;
        if (FDROP_STATE == kDragUnread) {
            short k = resolve_drag();

            STEP(k);
            FDROP_OVERFLOW = (short)(k == kStepNoRoom);
        }
        DGINC(kDgGetMouse);
        FDROP_FAKED = 1;
        answer_start(pt);
        return 1;
    }

    /* The drag routine's own. */
    if (FDROP_OUTSITE == 0 || ret == FDROP_OUTSITE) {
        FDROP_LASTGM = ret;               /* the outline: the truth */
        return 0;
    }
    DG(kDgFinalSite, (short)(ret - proc));
    if (ret - FDROP_OUTSITE <= 0L || ret - FDROP_OUTSITE >= kDragSpan) {
        FDROP_FIN = kFinBadShape;
        DG(kDgEnd, 2);
        return 0;
    }
    FDROP_FIN = kFinSeen;               /* the final one: the button is up */
    FDROP_OVER = over_panel();
    DG(kDgEnd, (short)(1 + (held ? 10 : 0) + (FDROP_OVER ? 100 : 0)));
    if (!FDROP_ACT)
        return 0;                       /* the Finder's own target */
    set_blob_long((unsigned char *)f + kDragResult, 0L);
    DGINC(kDgGetMouse);
    answer_start(pt);
    return 1;
#else   /* only the ROM's drag is followed, and that was not one */
    (void)pt;
    (void)ret;
    (void)a6;
    return 0;
#endif
}
#endif  /* FDROP_SENDER */

/* ------------------------------------------------------------------ */
/* The other door: the system's own Drag Manager                       */
/* ------------------------------------------------------------------ */

/*
 * FDROP_RECEIVER (fdrop.h): this section, and record_drop above, are the
 * Drag Manager's door, and a host that will only ever run where there
 * is no Drag Manager builds without it.
 */
#if FDROP_RECEIVER

/*
 * Where a Drag Manager exists, this replaces everything above: two
 * handlers registered on the host window, and the system does the
 * sending.
 *
 * Both doors end in the same queue, so the host asks one question
 * (fdrop_take_drop) and cannot tell which door a file came in by.
 *
 * The Drag Manager's declarations are the toolchain's own (Retro68's
 * multiversal interfaces), reached through `<Devices.h>` above like
 * every other Toolbox header.
 */

/*
 * `flavorTypeHFS` data: what the Finder puts in a drag for a file.  A
 * struct rather than byte-wise parsing is safe here: the Drag Manager
 * copies it into a buffer of ours, as the File Manager fills a
 * `CInfoPBRec`, so it is aligned as the compiler expects.
 */
typedef struct {
    OSType         fileType;
    OSType         fileCreator;
    unsigned short fdFlags;
    FSSpec         fileSpec;
} FDropHfsFlavor;

/*
 * The smallest `hfs ` flavor that still says which file it is.
 *
 * An `HFSFlavor` ends in a `Str63`, and a Finder may send only as many
 * bytes of the name as the name has, so the flavor is often shorter than
 * the structure (NOTES.md, "The Finder's `hfs ` flavor is SHORTER than an
 * `HFSFlavor`").  The minimum is everything up to and including the
 * name's LENGTH byte; how much name arrived is checked against what the
 * flavor held.  Computed from the structure, so there is no second
 * number to keep true.
 */
static long hfs_min_bytes(void)
{
    FDropHfsFlavor shape;

    return (long)((unsigned char *)shape.fileSpec.name
                  - (unsigned char *)&shape) + 1;
}

/*
 * How many files this drag carries, or 0 if it is not a drag of files.
 *
 * Asked before the window is highlighted as well as before the drop is
 * taken, so that dragging a piece of text over the host window does not
 * light it up.
 *
 * EVERY item must be a file: a drag holding anything else is refused
 * whole, never partly taken.  Taking two of a user's three files is a
 * plausible wrong answer, worse than a refusal the user can see.
 *
 * `most` is how many it may carry; more answers 0.  More than the queue
 * can hold is refused the same way, unless the host's room hook makes
 * the queue bigger first (fdrop_receive): the icons go back, and
 * fdrop_refused tells the host.
 */
static short drag_files(DragRef drag, short most)
{
    unsigned short items = 0;
    unsigned short i;

    {
        OSErr e = CountDragItems(drag, &items);

        /* 1000 + the answer, so that a slot nobody wrote (0) cannot be
           read as a drag of no items. */
        DG(kDgDragItems, e == noErr ? (short)(1000 + items) : e);
        if (e != noErr)
            return 0;
    }
    if (items == 0 || most <= 0 || items > (unsigned short)most)
        return 0;
    for (i = 1; i <= items; i++) {
        unsigned long item = 0;
        long          size = 0;

        if (GetDragItemReferenceNumber(drag, i, &item) != noErr)
            return 0;
        {
            OSErr e = GetFlavorDataSize(drag, item, flavorTypeHFS, &size);

            DG(kDgFlavorSize, e == noErr ? (short)(1000 + size) : e);
            if (e != noErr)
                return 0;
        }
        if (size < hfs_min_bytes())
            return 0;
    }
    return (short)items;
}

/*
 * The host's room hook, called from the receive handler, which runs in
 * the host's own context: the one place a queue may be made bigger while
 * a drag is arriving.  The function's address is kept as a long in
 * fdrop.S, and ISO C has no conversion between the two, hence the union.
 */
static Boolean call_room(WindowPtr w, short needed)
{
    union {
        long          addr;
        FDropRoomProc proc;
    } r;

    r.addr = FDROP_ROOM;
    return (Boolean)(r.proc != 0 && r.proc(w, needed));
}

/* The tracking handler: highlights the host window for a drag it can
   take. */
static pascal OSErr fdrop_tracking(short message, WindowPtr w, void *refCon,
                                 DragRef drag)
{
    GrafPtr   save;
    RgnHandle rgn;
    Rect      r;

    (void)refCon;
    if (w == 0)
        return noErr;

    DGINC(kDgTrackCalls);
    if (message == kDragTrackingLeaveWindow) {
        (void)HideDragHilite(drag);
        return noErr;
    }
    if (message != kDragTrackingEnterWindow)
        return noErr;
    /*
     * Not lit for a drag the queue cannot take, so the user sees that
     * before letting go, unless the host can make room when it arrives.
     * The room is what is FREE, by the same sum fdrop_receive refuses on,
     * so the window never lights up for a drag it then refuses.
     */
    if (drag_files(drag, FDROP_ROOM != 0L ? 0x7FFF
                         : (short)(queue_cap() - drops_queued())) == 0)
        return noErr;                   /* not for us: no highlight */

    /*
     * The port has to be ours before anything is drawn in it, and put
     * back afterwards: a tracking handler is called from inside somebody
     * else's event loop and inherits whatever port that loop was using.
     */
    GetPort(&save);
    SetPort(w);
    r = w->portRect;
    rgn = NewRgn();
    if (rgn != 0) {
        RectRgn(rgn, &r);
        (void)ShowDragHilite(drag, rgn, true);
        DisposeRgn(rgn);
    }
    SetPort(save);
    return noErr;
}

/* The receive handler: queues every file of the drop, or none. */
static pascal OSErr fdrop_receive(WindowPtr w, void *refCon, DragRef drag)
{
    FDropHfsFlavor  hfs;
    short         n, i, before;

    (void)refCon;
    DGINC(kDgRecvCalls);
    (void)HideDragHilite(drag);

    n = drag_files(drag, 0x7FFF);
    if (n == 0)
        return dragNotAcceptedErr;
    /*
     * Room for all of them, asked at the moment of the drop: the host
     * drains the queue on its own schedule, so the room depends on when
     * the user let go.  A drag that will not fit is refused whole, and
     * the icons go back.
     */
    before = drops_queued();
    if (n > (short)(queue_cap() - before)
        && (FDROP_ROOM == 0L || (long)w != FDROP_WIN
            || !call_room(w, (short)(before + n))
            || n > (short)(queue_cap() - drops_queued()))) {
        FDROP_REFUSED = 1;              /* fdrop_refused: no room */
        return dragNotAcceptedErr;
    }
    before = drops_queued();

    for (i = 0; i < n; i++) {
        unsigned long  item = 0;
        long           size = (long)sizeof hfs;
        long           nameOff = hfs_min_bytes() - 1;
        unsigned char *p = (unsigned char *)&hfs;
        unsigned short j;

        /*
         * Zeroed first: only the front of it is written, since the name
         * arrives only as long as the name is.
         */
        for (j = 0; j < (unsigned short)sizeof hfs; j++)
            p[j] = 0;

        if (GetDragItemReferenceNumber(drag, (unsigned short)(i + 1), &item)
                != noErr
            || GetFlavorData(drag, item, flavorTypeHFS, &hfs, &size, 0)
                != noErr
            || size < hfs_min_bytes()
            || (long)hfs.fileSpec.name[0] + 1 > size - nameOff
            || hfs.fileSpec.name[0] > kNameMax) {
            /*
             * Whole or nothing: entries only go on the end, so putting
             * the depth back where it started un-takes the files already
             * recorded.  This should not be reachable, since every item's
             * flavor size was checked above; it guards against a partial
             * drag arriving by accident.
             */
            set_drops_queued(before);
            return dragNotAcceptedErr;
        }
        /*
         * An FSSpec is a volume, a parent directory and a name: the three
         * things fdrop_take_drop hands back.
         */
        record_drop((long)w, hfs.fileSpec.vRefNum, hfs.fileSpec.parID,
                    hfs.fileSpec.name);
    }
    return noErr;
}

#endif  /* FDROP_RECEIVER */

/* ------------------------------------------------------------------ */
/* What the host calls                                                 */
/* ------------------------------------------------------------------ */

/*
 * fdrop.h says what this does for the host.  In order:
 *
 *   1. Record the window and the queue, and empty it.
 *   2. Where the system has a Drag Manager: register the two handlers
 *      (with FDROP_RECEIVER), and install nothing else.
 *   3. Otherwise, where a Finder can be running beside the host: install
 *      the trap patches (patch_install), and find the ROM's DragTheRgn
 *      for the Finders that drag through it (FDROP_ROM_ROUTE).
 */
void fdrop_install(WindowPtr onto, void *queue, short capacity,
                   FDropRoomProc room)
{
#if FDROP_RECEIVER
    union {
        long          addr;
        FDropRoomProc proc;
    } r;
#else
    (void)room;                         /* only the receiver calls it */
#endif

    /*
     * Which window, and where its drops go, before either door is
     * opened.  The queue starts empty: a drop from a previous visit is
     * not something the user is still asking for.
     */
#if FDROP_RECEIVER
    r.addr = 0L;
    r.proc = room;
#endif
    FDROP_QUEUE = (long)queue;
    FDROP_QCAP = queue != 0 && capacity > 0 ? capacity : 0;
#if FDROP_RECEIVER
    FDROP_ROOM = r.addr;
#endif
    FDROP_REFUSED = 0;
#if FDROP_SENDER
    FDROP_OVERFLOW = 0;
#endif
    FDROP_WIN = (long)onto;
    set_drops_queued(0);

    /*
     * The system's Drag Manager where there is one, and the patches only
     * where there is not.  This is the one place the choice is made, and
     * it is made by asking the machine.
     */
#if FDROP_ASK_DRAGMGR
    if (fdrop_have_drag_manager()) {
#if FDROP_RECEIVER
        if (onto != 0) {
            OSErr te = InstallTrackingHandler(fdrop_tracking, onto,
                                              (void *)0);
            OSErr re = InstallReceiveHandler(fdrop_receive, onto, (void *)0);

            /*
             * 1 rather than 0 for noErr, so that a word never written and
             * a handler installed do not read the same.
             */
            DG(kDgTrackInstall, te == noErr ? 1 : te);
            DG(kDgRecvInstall, re == noErr ? 1 : re);
            (void)te;
            (void)re;
        }
#endif  /* without the receiver: nothing, and no sender either, since
           the sender cannot work beside a Drag Manager */
        return;
    }
#endif  /* FDROP_ASK_DRAGMGR */
#if FDROP_SENDER
    if (!fdrop_finder_alongside(onto)) {
        DG(kDgInstall, 8);              /* no Finder to drag from: nothing */
        return;
    }
    patch_install();
    /*
     * The ROM's DragTheRgn, for the Finders that drag through it: found
     * by asking for the trap, and believed only if it is the routine the
     * frame is known for, byte for byte, or System 3's patch in front of
     * it (rom_drag_routine).  Anything else leaves FDROP_ROMDRAG 0 and
     * that route off.
     */
#if FDROP_ROM_ROUTE
    FDROP_ROMDRAG = 0L;
#if FDROP_ROMPLUS && FDROP_FINDER5
    FDROP_ROMWRAP = 0L;
#endif
    FDROP_ROMMODE = 0;
    if (FDROP_ENABLE) {
#if FDROP_ROM64K && FDROP_ROMPLUS
        unsigned long dt = (FDROP_TOOLTAB == kToolTableRom64)
                           ? trap_addr64(kTrapDragTheRgn)
                           : (unsigned long)GetToolTrapAddress(kTrapDragTheRgn);
#elif FDROP_ROM64K
        unsigned long dt = trap_addr64(kTrapDragTheRgn);
#else
        unsigned long dt = (unsigned long)GetToolTrapAddress(kTrapDragTheRgn);
#endif

        FDROP_ROMDRAG = rom_drag_routine(dt);
        DG(kDgDragTheRgn, (short)(dt >> 16));
        DG(kDgDragTheRgn + 1, (short)dt);
    }
#endif
    /* What install did (kDgInstall, kDgTramp). */
#if FDROP_ROM_ROUTE
#define kDgRomFound (FDROP_ROMDRAG != 0L ? 4 : 0)
#else
#define kDgRomFound 0
#endif
#if FDROP_ROM64K && FDROP_ROMPLUS
#define kDgRom64 (FDROP_TOOLTAB == kToolTableRom64 ? 2 : 0)
#elif FDROP_ROM64K
#define kDgRom64 (FDROP_ENABLE ? 2 : 0)
#else
#define kDgRom64 0
#endif
    DG(kDgInstall, (FDROP_ENABLE ? 1 : 0) | kDgRom64
           | kDgRomFound);
    DG(kDgTramp, (short)(FDROP_TRAMP >> 16));
    DG(kDgTramp + 1, (short)FDROP_TRAMP);
#endif
}

void fdrop_remove(WindowPtr onto)
{
    /*
     * Nothing is left pointing at a window about to go away.  A queued
     * drop names its window by ADDRESS, and the host's next window may be
     * given the same address, so an entry left behind would arrive in a
     * window it was never dropped on.
     */
    FDROP_WIN = 0;
    set_drops_queued(0);
    FDROP_QUEUE = 0L;                   /* the host may free it now */
    FDROP_QCAP = 0;
#if FDROP_RECEIVER
    FDROP_ROOM = 0L;
#endif

    /*
     * A handler left registered would point into a code resource about
     * to be unloaded, the same hazard as a trap patch left installed.
     */
#if FDROP_ASK_DRAGMGR
    if (fdrop_have_drag_manager()) {
#if FDROP_RECEIVER
        if (onto != 0) {
            (void)RemoveTrackingHandler(fdrop_tracking, onto);
            (void)RemoveReceiveHandler(fdrop_receive, onto);
        }
#else
        (void)onto;
#endif
        return;
    }
#else
    (void)onto;
#endif
#if FDROP_SENDER
    patch_remove();
#endif
}


/*
 * fdrop.h lists the eleven words.  Words 1 and 5 to 10 are always zero
 * and are not reused, so that an older log still reads correctly.
 */
void fdrop_diag(short *out)
{
    out[0] = kRecPos;
    out[1] = 0;
    out[2] = kSelOff;
#if FDROP_SENDER
    out[3] = FDROP_LASTN;
#if FDROP_DIAG
    out[4] = (short)(FDROP_SEQ * 100 + FDROP_STEP);
#else
    out[4] = (short)(FDROP_SEQ * 100);
#endif
#else   /* the sender's words: zero in a build without it */
    out[3] = 0;
    out[4] = 0;
#endif
    out[5] = 0;
    out[6] = 0;
    out[7] = 0;
    out[8] = 0;
    out[9] = 0;
    out[10] = 0;
#if FDROP_DIAG
    {
        short k;

        for (k = 0; k < kDgWords; k++)
            out[11 + k] = FDROP_DG[k];
    }
#endif
}

/*
 * A Finder drag that ended on the host window: what it carried was
 * written past the queue's depth while it was going (resolve_drag), and
 * here it is counted in, or thrown away.
 *
 * Here, at the host's idle time, because this cannot run inside the
 * drag.  The Finder's drag loop calls nothing that would hand a desk
 * accessory a null event, so by the time this runs the button has come
 * up.
 *
 * Counted in only if BOTH ends agree it was ours, and the drag was seen
 * to its end:
 *
 *   FDROP_ACT: the Finder's last target was the one this code gave it,
 *     the window the icons came from;
 *   FDROP_FIN kFinSeen: its final _GetMouse was seen, so the offset was
 *     cleared and the Finder is putting the icons back;
 *   FDROP_OVER: the pointer was over the host window when the button
 *     came up.
 *
 * The ends can disagree only for a pointer that crossed the window's
 * edge in the last pass, and either way nothing is taken: off the window
 * with the target ours, the Finder puts the icons back; on it with the
 * target the Finder's, the Finder has moved them.
 *
 * The depth may have moved DOWN since the drag was resolved, if the host
 * took or forgot what was waiting first.  Neither touches what lies past
 * the depth, so the drag is still whole, and it is moved down to the
 * depth before it is counted in.  A depth that has moved UP past where
 * the drag begins means something was written over it, and the drag is
 * refused whole.
 */
void fdrop_idle(void)
{
#if FDROP_SENDER
    if (FDROP_PEND > 0) {
        short at = drops_queued();

        DG(kDgDecision, (short)(FDROP_ACT * 10 + FDROP_OVER));
        if (FDROP_ACT && FDROP_OVER && FDROP_FIN == kFinSeen
            && at >= 0 && at <= FDROP_PENDAT) {
            if (at < FDROP_PENDAT) {
                unsigned char *to = queue_at(at);
                unsigned char *from = queue_at(FDROP_PENDAT);
                long           i, bytes = (long)FDROP_PEND * kDropBytes;

                for (i = 0; i < bytes; i++)
                    to[i] = from[i];
            }
            set_drops_queued((short)(at + FDROP_PEND));
            FDROP_LASTN = FDROP_PEND;
            STEP(kStepDropTaken);
        }
        FDROP_PEND = 0;
    }
    /*
     * A drag the sender refused for want of room is reported to the host
     * only if it was a drop: released over the host window with the
     * Finder's target ours (the icons going back), by the same two ends.
     */
    if (FDROP_OVERFLOW && FDROP_FIN != kFinNotYet) {
        if (FDROP_ACT && FDROP_OVER && FDROP_FIN == kFinSeen)
            FDROP_REFUSED = 1;
        FDROP_OVERFLOW = 0;
    }
#endif  /* FDROP_SENDER: with no sender there is nothing to do at idle */
}

/*
 * Take the next file dropped on this window, if there is one.
 *
 * What is handed back is an FSSpec in all but type: a volume reference
 * number, a directory and a name.  One file per call; a drag of six
 * files is six calls.
 *
 * An entry for some other window is thrown away rather than skipped: a
 * drop somewhere else will not become ours later.
 */
Boolean fdrop_take_drop(WindowPtr onto, short *vRefNum, long *dirID,
                      StringPtr name)
{
    const unsigned char *e;
    short i, len;

    while (drops_queued() > 0) {
        e = queue_at(0);
        if (blob_long(&e[0]) != (long)onto) {
            drop_pop();
            continue;
        }

        *vRefNum = (short)(((unsigned short)e[4] << 8) | e[5]);
        *dirID   = blob_long(&e[6]);         /* the entry: put_drop */

        len = (short)e[10];
        if (len > kNameMax)             /* never: put_drop wrote at most this */
            len = kNameMax;
        for (i = 0; i <= len; i++)
            name[i] = (i == 0) ? (unsigned char)len : e[10 + i];
        drop_pop();
        return true;
    }
    return false;
}

/*
 * The host's queue, swapped for another.  What is waiting, and what the
 * sender has written past it for a drag not yet counted in (FDROP_PEND),
 * is copied across.  The old queue is handed back in *old for the host
 * to free.
 *
 * Safe from the host's own code at any time: the sender writes only
 * inside the Finder's drag, which never runs while the host does, and
 * the receive handler calls this only through `room`.
 */
Boolean fdrop_requeue(WindowPtr onto, void *queue, short capacity,
                      void **old)
{
    unsigned char *from = (unsigned char *)FDROP_QUEUE;
    unsigned char *to = (unsigned char *)queue;
    short          keep = drops_queued();
    long           i, bytes;

    if ((long)onto != FDROP_WIN || onto == 0 || queue == 0)
        return false;
#if FDROP_SENDER
    if (FDROP_PEND > 0 && FDROP_PENDAT + FDROP_PEND > keep)
        keep = (short)(FDROP_PENDAT + FDROP_PEND);
#endif
    if (capacity < keep)
        return false;
    bytes = (long)keep * kDropBytes;
    for (i = 0; i < bytes; i++)
        to[i] = from[i];
    FDROP_QUEUE = (long)queue;
    FDROP_QCAP = capacity;
    if (old != 0)
        *old = from;
    return true;
}

/*
 * Whether a drag onto this window was refused for want of room since
 * this was last asked: set by the receive handler, and by fdrop_idle for
 * a drag the sender refused and the user let go of over the window.
 */
Boolean fdrop_refused(WindowPtr onto)
{
    Boolean was = (Boolean)(FDROP_REFUSED != 0 && (long)onto == FDROP_WIN);

    FDROP_REFUSED = 0;
    return was;
}

/*
 * Throw away whatever is still waiting, and say how much of it was this
 * window's.  For a host whose own list fills up part-way through a drag:
 * it can say so once, with a number in it, rather than leave the rest to
 * arrive later when nobody expects them.
 */
short fdrop_forget_drops(WindowPtr onto)
{
    short n = drops_queued();
    short mine = 0;
    short i;

    for (i = 0; i < n; i++)
        if (blob_long(queue_at(i)) == (long)onto)
            mine = (short)(mine + 1);
    set_drops_queued(0);
    return mine;
}
