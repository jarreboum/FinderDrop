/*
 * fdrop.h — FinderDrop's interface: what a host program calls.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * A host is the program (a desk accessory, a control panel or an
 * application) whose window files are dropped on: the host window.
 *
 * Eight calls, and three questions a host may ask without loading the
 * library's code: fdrop_have_drag_manager, fdrop_finder_alongside and
 * fdrop_can_drop, static inline below.  Everything else is in fdrop.c
 * and fdrop.S, including the library's state, which lives in the code
 * resource because a trap patch has no A5 world to reach a global
 * through.  The drop queue is the one thing the library does not hold:
 * the host owns it, and says where it is at fdrop_install.
 *
 * `fdrop_install` installs no patch where the system has its own Drag
 * Manager (it registers that one's handlers instead), nor where no Finder
 * can be running beside the host (an application, or a desk accessory
 * opened in one, without MultiFinder).  So a host may call every one of
 * these unconditionally.
 *
 * The library includes nothing of the program it is built into.
 */
#ifndef FDROP_H
#define FDROP_H

/* The version: a string, and major * 100 + minor as a number. */
#define FDROP_VERSION     "0.1"
#define FDROP_VERSION_NUM 1

/*
 * One queued drop.  Defined here, outside the C declarations, so that
 * fdrop.S could use it too.  A window (4), a volume (2), a directory (4)
 * and a name (64: a length and at most 63 bytes, see fdrop_take_drop).
 */
#define FDROP_ENTRY_BYTES 74

/*
 * WHOSE STACK THE SENDER SPENDS.  Its work runs inside a trap patch, on
 * `_GetMouse` called by the Finder's own drag, so the stack it spends is
 * the Finder's, mid-drag, not the host's.  The deepest it goes is the
 * first time a drag reaches the host window, in `resolve_drag` and the
 * File Manager calls it makes; nothing on that path is sized by the
 * queue, since the files are written straight into it as the heap walk
 * finds them.  `FDROP_STK` in fdrop.c measures what is left, and the
 * README has the figures.
 */

/*
 * The diagnostic words' count, here because the storage is in fdrop.S,
 * the C half fills it, and the host's buffer has to be as long as both.
 * FDROP_DIAG_WORDS is the most fdrop_diag writes, and the only number a
 * caller needs.
 */
#define FDROP_DG_WORDS   72
#define FDROP_DIAG_WORDS (11 + FDROP_DG_WORDS)

/*
 * ---- what is built: a switch for each era ----
 *
 * A drop reaches a host three ways, each for its own run of Systems, and
 * each is a switch: 1 unless the build says 0.  A host builds the ones
 * for the machines it means to run on.  Any combination builds,
 * including none at all, which is a library that takes nothing.
 *
 *   FDROP_ROM_ROUTE   Systems 1.1 to 3.2: Finders 1.1g to 5.3, which
 *                     drag an icon with the ROM's own _DragGrayRgn and
 *                     keep their records in two older layouts.  The
 *                     sender follows the drag from inside the ROM
 *                     routine.
 *
 *   FDROP_OWN_ROUTE   Systems 4.0 to 6.0.8: Finders 5.4 to 6.1.8, which
 *                     carry a drag routine of their own.  The sender
 *                     recognises it at _FindWindow and checks every name
 *                     against the HFS catalog, or on an MFS volume, which
 *                     has none, by its file number.
 *
 *   FDROP_RECEIVER    System 7.5 and later, and 7.0 or 7.1 with Apple's
 *                     Drag and Drop extension: the system's own Drag
 *                     Manager, and two handlers registered with it.
 *
 * What a switch leaves out is left out at COMPILE time, not left for the
 * linker to find unreached, so a host linked without --gc-sections is as
 * small as one linked with it.  A build holds no function, no state
 * word, no store and no call that belongs to a switch that is off
 * (examples/check_switches.sh checks the names).  The switches further
 * down (FDROP_FINDER5 within the first route, and the two for which ROM's
 * trap table is known) keep the same rule.
 *
 * Who needs which.  A control panel cannot exist before System 4.0, and
 * the sender installs for an application only under MultiFinder (4.2 and
 * later), so neither can use FDROP_ROM_ROUTE.  A desk accessory for a
 * Macintosh 128K needs that one alone.  A host that only wants System
 * 7.5 and later needs FDROP_RECEIVER alone, which patches nothing.
 *
 * On a System whose switch is off nothing happens: a drag is the
 * Finder's alone, and fdrop_can_drop says so where it can tell.  With
 * FDROP_RECEIVER off, a machine that has a Drag Manager gets nothing at
 * all: fdrop_install still stands aside there, because the sender
 * cannot work beside one.  With FDROP_OWN_ROUTE off and FDROP_ROM_ROUTE
 * on, _FindWindow is still patched and passes straight on, so that the
 * trampoline is the same block in every build that patches.
 *
 * The same values go to fdrop.c, to fdrop.S and to the host, which is
 * why they are here.  (The first switch is not called FDROP_ROMDRAG
 * because that is one of the state words in fdrop.S.)
 *
 * FDROP_SENDER is not a switch: it is what the two routes add up to,
 * the trap-patching half, and it is worked out below.
 */

#ifndef FDROP_ROM_ROUTE
#define FDROP_ROM_ROUTE  1
#endif
#ifndef FDROP_OWN_ROUTE
#define FDROP_OWN_ROUTE  1
#endif

/*
 * Within the route through the ROM, the Finder's era: FDROP_FINDER5, 1
 * unless the build says 0.  Finder 1.1g (System 1.1) and the Finders 1.1
 * and 4.1 of System 2.0 keep one layout of record for an icon.  Finders
 * 5.0 to 5.3 (Systems 2.1 to 3.2) moved the name, the selected bit and
 * the file number, gave a disk's record a name, and on the Plus ROM their
 * System puts a patch in front of DragTheRgn.  All of that is
 * FDROP_FINDER5.  A build without it reads the first layout only: it
 * takes a drop on Systems 1.1 and 2.0, and on 2.1 to 3.2 it finds no
 * record it knows and leaves the drag to the Finder.  It means nothing
 * without FDROP_ROM_ROUTE.
 */
#ifndef FDROP_FINDER5
#define FDROP_FINDER5  1
#endif
#ifndef FDROP_RECEIVER
#define FDROP_RECEIVER  1
#endif

/*
 * Two switches about the MACHINE rather than the era: which ROM's trap
 * table the sender knows how to patch.  Each is 1 unless the build says
 * 0, and they mean nothing without the sender.
 *
 *   FDROP_ROM64K    the 64K ROM of the Macintosh 128K and 512K, which
 *                   keeps its traps as a table of word offsets and is
 *                   patched by a path of its own (fdrop.c, patch_install).
 *                   A host whose oldest machine is a 512Ke or a Plus
 *                   builds without it.
 *
 *   FDROP_ROMPLUS   the Plus ROM and every later one: the longword
 *                   table, and with it what only those machines have:
 *                   MultiFinder, a second copy of the library beside this
 *                   one and the trampoline it reuses, a processor with a
 *                   cache to flush.  A desk accessory for the Macintosh
 *                   128K and nothing else builds without it, and loses
 *                   nothing that machine could have used.
 *
 * On a ROM whose switch is off the sender finds no table it knows and
 * installs nothing, as any build does on a machine it cannot identify.
 * So a build without FDROP_ROMPLUS takes nothing on a 512Ke or a Plus,
 * even under System 1.1.
 * On a ROM it does support, a build patches whatever the Finder: one it
 * has no route for (FDROP_ROM_ROUTE, FDROP_FINDER5, FDROP_OWN_ROUTE) is
 * met at each drag, and the patches pass that drag through untaken.
 */
#ifndef FDROP_ROM64K
#define FDROP_ROM64K  1
#endif
#ifndef FDROP_ROMPLUS
#define FDROP_ROMPLUS  1
#endif
#if FDROP_ROM_ROUTE || FDROP_OWN_ROUTE
#define FDROP_SENDER  1
#else
#define FDROP_SENDER  0
#endif

/*
 * Two more that are worked out, not passed.
 *
 * FDROP_PATCH_FW: whether _FindWindow is patched at all.  Only
 * FDROP_OWN_ROUTE looks at it, but a build for the later ROMs patches it
 * regardless, because there its trampoline may be taken over by another
 * copy of the library that does want it (fdrop.c, the trampoline).  On
 * the 64K ROM no trampoline is ever taken over, so a build with neither
 * has one patch, on _GetMouse, and a trampoline of one jump.
 *
 * FDROP_ASK_DRAGMGR: whether the machine is asked for a Drag Manager.
 * The receiver needs the answer, and so does a sender that could find
 * itself beside one, to stand aside.  A build for the 64K ROM alone with
 * no receiver cannot, and does not ask.
 */
#if FDROP_SENDER && (FDROP_OWN_ROUTE || FDROP_ROMPLUS)
#define FDROP_PATCH_FW  1
#else
#define FDROP_PATCH_FW  0
#endif
#if FDROP_RECEIVER || (FDROP_SENDER && FDROP_ROMPLUS)
#define FDROP_ASK_DRAGMGR  1
#else
#define FDROP_ASK_DRAGMGR  0
#endif

/* The two traps the sender patches, and a trap's index in its table. */
#define FDROP_TRAP_FINDWINDOW  0xA92C
#define FDROP_TRAP_GETMOUSE    0xA972
#define FDROP_TRAP_INDEX(t)    ((long)((t) & 0x3FF))

#ifndef __ASSEMBLER__

#include <Dialogs.h>
#include <Windows.h>

/*
 * ---- the questions a host may ask without the library's code ----
 *
 * Static inline, so that a host keeping FinderDrop in a segment or a
 * code resource of its own can ask whether a drop is possible at all
 * without loading it.  They are the same code fdrop_install asks: fdrop.c
 * calls these and keeps no copy.  fdrop_probe_agrees, fdrop_probe_table
 * and fdrop_shell_is_current are the probes' own parts; no host needs
 * them.
 */
/*
 * Whether the system has a Drag Manager of its own.
 *
 * A host that says which door a drag comes in by (the examples say so
 * in their windows) asks the same question fdrop_install does, here,
 * without loading the library.  One probe, so the two cannot disagree.
 *
 * This is the switch the whole design turns on.  Where the system has a
 * Drag Manager, a host registers a receive handler and the Finder does
 * the sending; all the machinery here is for the systems that have none.
 *
 * PROBED, NOT INFERRED FROM A VERSION.  System 7.5 ships a Drag Manager;
 * System 7.0 and 7.1 have one only if the user installed Apple's
 * extension.  "System 7 or later" would be wrong on a stock 7.0.1.
 *
 * The version IS consulted for one thing: `_Gestalt` does not exist
 * before System 6.0.4, so it must not be called there.  `SysVersion` at
 * low memory 0x015A answers that in BCD and needs no trap.  Below System
 * 4.1 the word is not left over from anything: the Plus ROM clears it at
 * boot, and on the 64K ROM the System's own patch writes its version
 * there, below 0x0604 either way (NOTES.md, "SysVersion before System
 * 4.1").
 */
static inline Boolean fdrop_have_drag_manager(void)
{
    register long d0 __asm__("d0") = 0x64726167L;   /* 'drag' */
    register long a0 __asm__("a0");
    short sysv = *(short *)0x015AL;

    if ((unsigned short)sysv < 0x0604)
        return false;                   /* no Gestalt to ask */

    /*
     * D1, D2 and A1 are clobbered too.  A Toolbox trap may use all of
     * D0-D2 and A0-A1, and declaring only the two that carry the
     * arguments would let the compiler keep something live in the others
     * across the trap.  A missing clobber does not fail where it is
     * written.
     */
    __asm__ volatile (".short 0xA1AD"
                      : "=d"(d0), "=a"(a0) : "0"(d0)
                      : "d1", "d2", "a1", "memory");
    if ((OSErr)d0 != noErr)
        return false;
    return (Boolean)((a0 & 1L) != 0);   /* gestaltDragMgrPresent */
}

/* Whether the table at `base`, of `entries` longwords, holds `fw` and
   `gm` at _FindWindow's and _GetMouse's entries. */
static inline Boolean fdrop_probe_agrees(long base, long entries,
                                          unsigned long fw, unsigned long gm)
{
    const unsigned long *t = (const unsigned long *)base;

    return (Boolean)(FDROP_TRAP_INDEX(FDROP_TRAP_FINDWINDOW) < entries
                     && FDROP_TRAP_INDEX(FDROP_TRAP_GETMOUSE) < entries
                     && t[FDROP_TRAP_INDEX(FDROP_TRAP_FINDWINDOW)] == fw
                     && t[FDROP_TRAP_INDEX(FDROP_TRAP_GETMOUSE)] == gm);
}

/* The Toolbox dispatch table, or 0 if the machine does not say which.
 * fdrop.c patches the table this names (fdrop.c, "WHICH table"). */
static inline unsigned long *fdrop_probe_table(void)
{
    unsigned long fw = (unsigned long)GetToolTrapAddress(FDROP_TRAP_FINDWINDOW);
    unsigned long gm = (unsigned long)GetToolTrapAddress(FDROP_TRAP_GETMOUSE);
    /* A 128K ROM: 512 entries at 0x0C00.  The SE and the II on: 1024 at 0x0E00. */
    Boolean small = fdrop_probe_agrees(0x0C00L, 512L, fw, gm);
    Boolean large = fdrop_probe_agrees(0x0E00L, 1024L, fw, gm);

    if (small == large)
        return 0;                       /* neither, or both: not a guess */
    return (unsigned long *)(small ? 0x0C00L : 0x0E00L);
}

/*
 * Whether the current application (CurApName, $0910) is the shell
 * (FinderName, $02E0): the one test for "the Finder is running", here
 * and in the sender's ROM route (fdrop.c, rom_arm), so the two cannot
 * disagree.  It compares the name the system launched the shell by,
 * rather than the word "Finder", because that is the name the system
 * itself goes by.
 */
static inline Boolean fdrop_shell_is_current(void)
{
    const unsigned char *cur = (const unsigned char *)0x0910L;   /* CurApName */
    const unsigned char *shell = (const unsigned char *)0x02E0L; /* FinderName */
    short i;

    if (shell[0] == 0 || shell[0] > 15 || cur[0] != shell[0])
        return false;
    for (i = 1; i <= (short)shell[0]; i++)
        if (cur[i] != shell[i])
            return false;
    return true;
}

/*
 * Whether a Finder can be running beside this host: the only case the
 * sender is for, and the second half of fdrop_can_drop.
 *
 * Under MultiFinder one always is, behind every layer.  Without it, only
 * when the current application IS the shell: a desk accessory or a
 * control panel opened over the Finder.  An application without
 * MultiFinder has replaced the Finder, and so has a desk accessory opened
 * inside one, so there is nothing to drag from and the sender installs
 * nothing.  That also means no patch is left pointing into an
 * application heap that ExitToShell reuses.
 *
 * MultiFinder is asked, not inferred from a version: before System 7,
 * _OSDispatch ($A88F) is implemented only while it runs.  The 64K ROM,
 * which never ran it, has no longword table to ask.
 *
 * The name alone is not enough, which is what `onto` is for.  Systems
 * 1.1 to 3.2 launch the shell BY NAME, so an application renamed Finder
 * and started as the shell matches too, with no Finder behind it.  So
 * without MultiFinder a matching name counts only for a desk accessory's
 * window (a control panel's is one too): its windowKind is the driver's
 * reference number, negative, which an application's window never is.
 * An `onto` of 0 is a host that cannot say, and is answered as an
 * application would be.
 */
static inline Boolean fdrop_finder_alongside(WindowPtr onto)
{
#if FDROP_ROMPLUS       /* a 64K ROM has no such table and no MultiFinder */
    unsigned long *t = fdrop_probe_table();

    if (t != 0 && t[FDROP_TRAP_INDEX(0xA88F)] != t[FDROP_TRAP_INDEX(0xA89F)])
        return true;                    /* MultiFinder: _OSDispatch is in */
#endif
    if (onto == 0 || ((WindowPeek)onto)->windowKind >= 0)
        return false;                   /* not a desk accessory's window */
    return fdrop_shell_is_current();
}

/*
 * Whether fdrop_install has anything to install here: the system's Drag
 * Manager, or (with the sender built) a Finder beside the host to drag
 * from.  False means a drop cannot happen and the library need not be
 * loaded.
 *
 * True is not a promise.  The sender still installs nothing on a machine
 * whose trap table it cannot identify; nor on the 64K ROM when neither
 * heap gives it a trampoline the ROM can reach (fdrop.c, patch_install),
 * nor there when _GetMouse, _FindWindow and _Button are all patched in
 * RAM, since that ROM's table is recognised by a ROM entry of one of them
 * (fdrop.c, rom64_present).
 *
 * `onto` is the window fdrop_install would be given, as in
 * fdrop_finder_alongside.
 */
static inline Boolean fdrop_can_drop(WindowPtr onto)
{
#if FDROP_ASK_DRAGMGR
    if (fdrop_have_drag_manager())
        return (Boolean)(FDROP_RECEIVER != 0);
#endif
#if FDROP_SENDER
    return fdrop_finder_alongside(onto);
#else
    (void)onto;
    return false;
#endif
}

/*
 * `onto` is the host window: what a drop is aimed at, here and in
 * fdrop_take_drop and fdrop_forget_drops below.  The sender resolves a
 * drop by walking the window list and needs no argument; the system's
 * Drag Manager registers handlers per window and does.  One signature
 * serves both, because which of the two runs is not the caller's
 * business.
 *
 * One window per copy of the library: calling fdrop_install again on a
 * second window, without fdrop_remove on the first, leaves the first
 * window's Drag Manager handlers registered with nowhere for its drops
 * to go.
 */

/*
 * ---- the queue is the host's ----
 *
 * `queue` is `capacity` entries of FDROP_ENTRY_BYTES, and the HOST
 * decides both: as many as its own list holds, or as much as its heap
 * can spare when the window opens.  A drag of more items than there is
 * room for is refused whole (the icons go back), and fdrop_refused says
 * so afterwards.  Taking some of a user's selection and not the rest
 * would be a plausible wrong answer.
 *
 * IT MUST NOT MOVE OR GO AWAY until fdrop_remove: a pointer, or a locked
 * handle's contents.  The sender writes into it from inside the Finder's
 * drag, where nothing could notice that it had moved.  Free it after
 * fdrop_remove, not before.
 *
 * Without a Drag Manager, the capacity is what a drag gets: the sender
 * finds the items inside the Finder's drag, where nothing can be
 * allocated.  With one, `room` may be given: a function of the host's,
 * called from the receive handler (in the host's own context, where
 * allocating is fine) when a drag needs `needed` entries in all, those
 * waiting plus the new ones, and the queue holds fewer.  It may hand
 * over a bigger queue with fdrop_requeue and answer true; false, or a
 * queue still too small, refuses the drag.  0 for no such function.
 *
 * A desk accessory sets its window's windowKind to its own reference
 * number BEFORE fdrop_install, as it must for its events anyway.
 * Without MultiFinder that negative number is how the sender tells a
 * desk accessory over the Finder from an application started as the
 * shell (fdrop_finder_alongside).
 *
 * Call fdrop_remove when the window closes, and for a desk accessory on
 * the goodbye kiss too, whichever comes first.  Its Close routine is the
 * ordinary case: without MultiFinder, launching an application from the
 * Finder closes an open accessory before its heap goes.  But under
 * MultiFinder an accessory opened with Option held goes into the layer
 * of the application in front, and when that application quits its
 * Close never runs: the goodbye kiss (dNeedGoodbye in the driver's flags,
 * a Control call with csCode -1) is the only call it gets.  The example
 * accessory does both (examples/da; NOTES.md, "The goodbye
 * kiss: what an accessory gets when its heap goes, and the example now
 * takes it").
 */
typedef Boolean (*FDropRoomProc)(WindowPtr onto, short needed);

void    fdrop_install(WindowPtr onto, void *queue, short capacity,
                      FDropRoomProc room);
void    fdrop_remove(WindowPtr onto);

/*
 * A different queue for the same window: what is waiting is copied into
 * `queue`, and the one it replaces is handed back in *old for the host
 * to free.  False, and nothing changed, if `capacity` is smaller than
 * what is waiting or `onto` is not the installed window.  Call it from
 * `room`, or at any time the host is running its own code, since the
 * Finder's drag never runs while it is.
 */
Boolean fdrop_requeue(WindowPtr onto, void *queue, short capacity,
                      void **old);

/*
 * Whether a drag onto this window was refused for want of room since
 * the last time this was asked, so that a host which takes one file can
 * say "one at a time" rather than leave the user wondering why the icons
 * went back.  Only a drag released over the window counts: one refused
 * while passing over it and let go elsewhere was not a drop.  It answers
 * whether, not how many: the sender stops counting at the first item
 * that does not fit.
 */
Boolean fdrop_refused(WindowPtr onto);

/*
 * Call often: on every pass of an application's event loop, every accRun
 * of a desk accessory, every nulDev of a control panel.  A Finder drag
 * that ended on the host window is counted into the queue here, after
 * the Finder has let go; then call fdrop_take_drop until it says no.
 */
void    fdrop_idle(void);

/*
 * Eleven words about the sender, for a diagnostic build to show or log:
 *
 *   0  the record position offset (fdrop.c, kRecPos)
 *   1  zero
 *   2  the selected flag's offset (fdrop.c, kSelOff)
 *   3  how many files the last drop carried
 *   4  a hundred times the Finder drags recognised, plus, in an
 *      FDROP_DIAG build, how far the last one got (fdrop.c names each
 *      step, kStep...)
 *   5-10  zero
 *
 * The zero words are left as gaps rather than reused, so that older
 * logs, which quote words by number, still read correctly.
 *
 * Behind them, when the library is built FDROP_DIAG, come FDROP_DG's
 * words.  fdrop.c names each one (kDg...), beside the code that fills
 * it; all a caller needs is a buffer FDROP_DIAG_WORDS long.
 *
 * Nothing reads any of it in an ordinary build.  It is the only view
 * there is into a trap patch.
 */
void    fdrop_diag(short *out);

/*
 * One file per call, and the caller asks until it says no: a drag of
 * several files is several drops, and the host takes them one at a time.
 *
 * `name` is a Str63, 64 bytes, as Standard File's names are: an HFS name
 * is 1 to 31 characters, and an MFS volume's (the 400K floppies, which
 * every System up to 6.0.8 reads) reach 63.  A name is never cut short (the front of one
 * may be another file's name), so a longer one, which no Finder writes,
 * is refused with the whole drag.
 *
 * An item may come off an EJECTED disk: the Finder still shows its icons,
 * and a drag of them is taken like any other.  Nothing in the drag may
 * ask for the disk, so such an item is handed over as the Finder recorded
 * it, and the host's own first File Manager call on it puts up the
 * system's "Please insert the disk", as the Finder's own copy does.  A
 * name that is not there once the disk is back is that call's error to
 * report, as the Finder reports it.
 */
Boolean fdrop_take_drop(WindowPtr onto, short *vRefNum, long *dirID,
                      StringPtr name);

/*
 * Throw away what is still waiting (the host's own list filling up
 * part-way through a drag), and answer how many of the entries were this
 * window's, so that the host can say what happened with a number in it.
 */
short   fdrop_forget_drops(WindowPtr onto);

#endif  /* __ASSEMBLER__ */

#endif
