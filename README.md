# FinderDrop

Version 0.1 (`FDROP_VERSION` in `fdrop.h`). Licence 0BSD. `main` is always
tested; a tag marks a milestone, not every fix.

Drag files, folders and disks out of a Finder window or off the desktop
and onto your own window — a desk accessory's, a control panel's or an
application's — on every Macintosh from System 1.1 to Mac OS 9.2.2,
through **one** set of calls. The code is `fdrop.c`, `fdrop.S` and
`fdrop.h`; everything it exports is `fdrop_` or `FDROP_`.

A drop arrives by one of two routes, chosen by `Gestalt('drag')` and
nothing else; the caller cannot tell which one an item came by:

- **The receiver**, where the system has a Drag Manager: the library
  registers a tracking handler and a receive handler with it.
- **The sender**, where it has none (System 1.1 to 6.0.8): two trap
  patches stand in for one. `_FindWindow` recognises the Finder's own
  drag of an icon; `_GetMouse` makes that drag end where it began when it
  is let go in your window's content, so the Finder moves nothing and the
  items it carried are the drop. The outline follows the pointer as
  usual, and a drag let go anywhere else is the Finder's alone.

## Using it

Two terms first. The **queue** is memory you own, where drops wait
until you take them: `capacity` entries of `FDROP_ENTRY_BYTES` (74). The
**room** function is optional: with a Drag Manager it is asked for a
bigger queue when a drag will not fit (pass 0 for none).

```c
#include "fdrop.h"

/* The window opens.  A desk accessory sets windowKind to its own
   reference number BEFORE this. */
Ptr queue = NewPtr(8L * FDROP_ENTRY_BYTES);   /* must not move */
fdrop_install(win, queue, 8, 0);

/* Every null event: an application's event loop, a DA's accRun,
   a control panel's nulDev. */
short vRefNum; long dirID; Str63 name;
fdrop_idle();
while (fdrop_take_drop(win, &vRefNum, &dirID, name))
    open_item(vRefNum, dirID, name);       /* yours */
if (fdrop_refused(win))
    say_too_many();                        /* yours: why the icons went back */

/* The window closes, and for a desk accessory also on the goodbye
   kiss, whichever comes first. */
fdrop_remove(win);
DisposePtr(queue);                         /* after fdrop_remove, never before */
```

Every call is safe to make unconditionally. The three example programs
(below) are complete versions of this.

## Requirements and building

- **Retro68**, <https://github.com/autc04/Retro68>, built 2026-10-15 or
  later: its multiversal interfaces declare the Drag Manager, which the
  library does not declare itself. An older toolchain fails with ten
  conflicting-type errors (`NOTES.md`, on the selectors).
- **python3**, which the examples' build scripts use to package what
  they build.

Each example builds with its own script, from the repository root:

```sh
sh examples/da/build.sh      # or cdev/, app/; add -DFDROP_DIAG=1 for a debug build
```

The output goes under `build/examples/`. The toolchain is found at
`$RETRO68`, by default `/opt/Retro68-build/toolchain`.

To build the library into your own program:

- **Compile both halves with the same `-D` flags.** `fdrop.S` holds
  every byte of the state `fdrop.c` uses; a flag given to one and not
  the other is a link that fails, or worse, succeeds. Give the host the
  same values too.
- **Use code-resource flags**: `-mpcrel -ffunction-sections
  -fdata-sections`, `--gc-sections` at the link, nothing in `.data` or
  `.bss`. A trap patch runs in whatever A5 world the trapping code had,
  so the state is addressed PC-relative.
- **Pass `--param=min-pagesize=0`** on every file that includes
  `fdrop.h`. GCC 12 and later otherwise reads each low-memory global (a
  dereference of a literal address) as an out-of-bounds array access.
  That param is the whole fix; no `-Wno-array-bounds` is needed.
- **Keep `.text.patch` and `fdrop.o`'s text together and early in the
  link**, right after the entry: the patches reach their state
  PC-relative, within a 16-bit displacement. The examples' `da.ld` and
  `cdev.ld` are working ones.

## The three kinds of program

A desk accessory, a control panel and an application make the same
calls. What differs is where each can exist, and so which build switches
(below) it can never use and may leave out:

| program | switches it can never use | why |
|---|---|---|
| a desk accessory | none | it opens over the Finder on every System from 1.1 on |
| a control panel | `FDROP_ROM_ROUTE`, `FDROP_FINDER5` | there is no control panel before System 4.0, and those are for Systems 1.1 to 3.2 |
| an application | `FDROP_ROM_ROUTE`, `FDROP_FINDER5`, `FDROP_ROM64K` | without MultiFinder, launching one quits the Finder, so there is nothing to drag from; MultiFinder begins with System 4.2, after those Finders, and is looked for in a trap table the 64K ROM has not got |

`FDROP_FINDER5` means nothing once `FDROP_ROM_ROUTE` is off. With a Drag
Manager all three take drops through `FDROP_RECEIVER`. A control panel
may also leave out `FDROP_ROM64K` if it will never run on a Macintosh
128K or 512K; System 4.1 has control panels and boots on that ROM in a
512K, where the sender takes drops, but no control panel has been run
there, so the table does not say it cannot.

## How far it has been tested

Almost everything was run on emulated Macintoshes, not real ones.

- **Receiver:** emulated System 7.5.3, Mac OS 8.1 and 9.2.2, and a real
  LC475 under System 7.6.1.
- **Sender:** every System from 1.1 to 6.0.8 except 4.0, on an emulated
  128K, 512K, 512Ke and Plus, with and without MultiFinder. On real
  machines, all three examples: a Plus under 6.0.8 and under 4.0, and an
  SE/30 under 6.0.8 and 6.0.3, including MFS floppies, folders, disks,
  the desktop, the Trash, ejected disks and an AppleShare volume.

Known limit: a file just taken, pressed again within the double-click
time, is opened by the Finder (`OPEN_ITEMS.md`); a short pause
avoids it.

`NOTES.md` has every run and how to repeat it;
`OPEN_ITEMS.md` has what has not been run.

## Which Finders the sender supports

Oldest first; the last row is where the receiver takes over.

| Finder | System | how its drag is followed | run, emulated |
|---|---|---|---|
| 1.1g, 1.1, 4.1 | 1.1, 2.0 | from the ROM's drag routine, which these Finders call | all, on the 64K ROM and the Plus ROM |
| 5.0 to 5.3 | 2.1 to 3.2 | the same, with these Finders' own record layout | all, on both ROMs |
| 5.4 to 6.1.8 | 3.3 to 6.0.8 | from the Finder's own drag routine, recognised by its code | all but 6.0.8L; System 4.0 on a real Plus only |
| 7.0 to 7.1 | 7.0 to 7.1 | only with Apple's Drag and Drop extension, through the Drag Manager (below) | |
| 7.5 and later | 7.5 to Mac OS 9.2.2 | not by the sender: the receiver takes the drop through the Drag Manager | 7.5.3, 8.1, 9.2.2, and a real LC475 under 7.6.1 |

- **Finders up to 5.3** call the ROM's drag routine, which is checked
  byte for byte before use; on the Plus ROM, Systems 3.0 to 3.2 put a
  patch in front of it, which is recognised and followed.
- **Finders 5.4 to 6.1.8** carry one drag routine, the same in all of
  them, so there is one recognition and no table per version.
- **Every item is checked with the File Manager**: on HFS, in the
  catalog where the Finder's record says; on MFS (any 400K floppy),
  which has no catalog, by name and file number — out of a window, off
  the desktop, in a folder or in the Trash.
- **System 4.0** freezes the pointer under emulation, so it has run on a
  real Plus only. **System 6.0.8L** stops at startup in every emulated
  machine, so it has not been run (`NOTES.md`).
- **MultiFinder** is supported, run on every System from 4.2 to 6.0.8.
  The patches are written into the trap table directly, and the window
  is found by its visible region (`NOTES.md`). A release counts
  only in the window's content, never its title bar or frame.
- **On the 64K ROM**, a `_GetMouse` already patched in RAM is chained to
  as on the other ROMs. Only a machine with `_GetMouse`, `_FindWindow`
  and `_Button` all patched installs no sender, and is not told why.

**System 7.0, 7.0.1 and 7.1 with Apple's Drag and Drop extension
installed**, which brings the Drag Manager, take drops through the
receiver. Without the extension nothing is taken there: a System 7
Finder lays out its records and drags an icon differently in every
respect, and a sender for it could not be made to work properly
(`PARKED.md`).

## The calls

`fdrop.h` is the whole interface.

| call | when |
|---|---|
| `fdrop_install(window, queue, capacity, room)` | the window opens |
| `fdrop_remove(window)` | the window closes, before the code is unloaded; for a desk accessory, its Close AND the goodbye kiss, whichever comes first |
| `fdrop_idle()` | every null event |
| `fdrop_take_drop(window, &vRefNum, &dirID, name)` | after `fdrop_idle`, until it answers false; one item per call |
| `fdrop_forget_drops(window)` | you cannot take the rest; answers how many were thrown away |
| `fdrop_refused(window)` | whether a drag was refused for want of room since last asked |
| `fdrop_requeue(window, queue, capacity, &old)` | a bigger queue, from `room` or your own code; the old one comes back to be freed |
| `fdrop_have_drag_manager()` | which route is in use, asked as `fdrop_install` asks |
| `fdrop_finder_alongside(window)` | whether a Finder can be running beside you to drag from |
| `fdrop_can_drop(window)` | before loading the library from a segment of its own: false means no drop can happen here |
| `fdrop_diag(words)` | for a log: a buffer of `FDROP_DIAG_WORDS` shorts |

`fdrop_have_drag_manager`, `fdrop_finder_alongside` and `fdrop_can_drop`
are `static inline` in `fdrop.h`, so asking links none of `fdrop.c`.

Items come in **no promised order** (the Finder's or the Drag Manager's);
sort them if you need one.

**What an item is.** A volume, a directory and a name — an FSSpec in all
but type, the same by either route. A file or folder is its volume, its
parent directory and its name; a disk is its volume, directory 1 and the
volume's name; a file on an MFS volume has directory 0. The name is a
`Str63`: 1 to 31 bytes on HFS, up to 63 on MFS, never cut short. Ask the
File Manager which kind you have; the examples' `what_is()` shows how.

- An MFS **folder** exists only in the Finder's Desktop file, so it
  arrives as the files in it, each a drop of its own.
- **The Trash** arrives as the startup disk's Trash folder where the
  Drag Manager hands it over; where there is no Trash folder (System 6)
  it is left out, its MFS files arrive as files, and the rest of the drag
  still arrives.
- An item on an **ejected disk** is handed over as the Finder recorded
  it; your first File Manager call on it puts up "Please insert the
  disk", as the Finder's own copy does.

## What your program must get right

- **The queue must not move or be freed until `fdrop_remove`** — a
  pointer, or a locked handle's contents. The sender writes into it from
  inside the Finder's drag, where nothing could notice. Size it to your
  own list, or to what the heap can spare when the window opens. A drag
  of more items than fit is refused whole: the icons go back. Without a
  Drag Manager the capacity is all one drag can bring; with one, `room`
  is called in your own context and may hand over a bigger queue with
  `fdrop_requeue`. The example application does both.
- **The patches must not outlive the code.** The trap table points at a
  trampoline in the system heap that jumps into your code resource from
  `fdrop_install` to `fdrop_remove`, so the resource must stay loaded and
  locked for exactly that long. Otherwise the next mouse movement
  crashes.
- **A desk accessory must answer the goodbye kiss.** Set `dNeedGoodbye`
  in its driver flags and call `fdrop_remove` on the goodbye (`_Control`,
  `csCode` -1) as well as in Close, once, whichever comes first. Under
  MultiFinder an accessory opened with Option lives in the front
  application's heap, and when that application quits the goodbye is the
  only call it gets; without it the Finder's next drag writes into freed
  memory. `examples/da` does this; `NOTES.md` has the
  runs. A control panel needs nothing extra.
- **The Finder's stack, not yours, is spent.** The sender works inside
  `_GetMouse`, called by the Finder's drag, and allocates nothing. The
  least room left measured is about 31 KB with the Finder alone on
  Systems 4.1 and 6.0.8, 14555 bytes under MultiFinder, and 6705 bytes on
  a Macintosh 128K under System 1.1 (`NOTES.md`, the entries on
  the Finder's stack and on the trimmed builds).

## Beside another copy

Two programs may each carry a copy, open at once: a control panel and a
desk accessory on one System 6 machine. Each copy keeps its own state and
patches and finds the other only through the trap table and its
trampolines. The rules that keep them working together are in
`fdrop.c`, above `tramp_mark` and `patch_remove`.

**Rules 1, 2 and 4 are for good**: the machine forces them on any copy
that patches these traps from unloadable code, and breaking one would
bring the machine down beside an older copy nobody can recall. **Rules 3
and 5 hold for one signature**: they let copies share trampolines, and a
revision may change them by changing the signature. Old and new copies
then still work side by side under the other three, only without reusing
each other's trampolines.

1. **The table points at a trampoline**, never into the program's code.
   The trampoline is in the system heap; on the 64K ROM, in whichever
   heap that ROM's 16-bit trap offsets can reach, system heap first.
2. **On removal, the trampoline becomes a pass-through first**: both
   jumps lead to what this copy replaced. It is unhooked and freed only
   with no MultiFinder and both entries still holding this copy's own
   trampoline; otherwise it stays in the chain, since something may have
   saved its address. Under MultiFinder a trampoline is never freed.
3. **A pass-through is reused.** An installing copy looks down
   `_GetMouse`'s chain, through this library's trampolines in the system
   heap, and takes over the first pass-through, so a session holds as
   many trampolines as copies were ever open at once. Not on the 64K ROM,
   where each copy allocates its own.
4. **A call is answered only for this copy's own window**; every other
   is passed on unchanged.
5. **No copy reads another's state; the trampoline layout is all they
   share.** 28 bytes: a `jmp.l` for `_FindWindow` at +0 and one for
   `_GetMouse` at +6, the signature `'FDtr'` at +12, the block's own
   address at +16, and the two jumps' targets at +20 and +24. A change to
   it changes the signature, and a copy meeting a block it does not
   recognise stops looking there and allocates its own.

## Build switches

A build switch is a `-D` flag, 0 or 1, that compiles a part of the
library in or out. Each era of System is one; all default to 1, any
combination builds, and a switch only adds support. Leave them all on to
run everywhere; turn some off only to save bytes where you know you will
never run.

### Which switches a machine needs

Find the machine, read off the switches: one for the ROM, one or two for
the Finder.

| ROM | Finder (System) | switches |
|---|---|---|
| 64K ROM (Macintosh 128K, 512K) | 1.1g (1.1); 1.1 and 4.1 (2.0) | `FDROP_ROM64K` `FDROP_ROM_ROUTE` |
| 64K ROM | 5.0 to 5.3 (2.1 to 3.2) | `FDROP_ROM64K` `FDROP_ROM_ROUTE` `FDROP_FINDER5` |
| 64K ROM | 5.4 and 5.5 (3.3, 4.1) | `FDROP_ROM64K` `FDROP_OWN_ROUTE` |
| Plus ROM or later (512Ke, Plus, SE, II and on) | 1.1g (1.1); 1.1 and 4.1 (2.0) | `FDROP_ROMPLUS` `FDROP_ROM_ROUTE` |
| Plus ROM or later | 5.0 to 5.3 (2.1 to 3.2) | `FDROP_ROMPLUS` `FDROP_ROM_ROUTE` `FDROP_FINDER5` |
| Plus ROM or later | 5.4 to 6.1.8 (3.3 to 6.0.8) | `FDROP_ROMPLUS` `FDROP_OWN_ROUTE` |
| Plus ROM or later | System 7.0 or 7.1 with no Drag Manager | none: nothing takes a drop there |
| any | System 7.5 and later, or 7.0 and 7.1 with Apple's Drag and Drop extension | `FDROP_RECEIVER` alone |
| every machine | every System | all of them: pass no flag |

The full build asks the machine which case it is in and takes the same
path a build for that machine alone would. The ROM switches matter only
to the sender; `FDROP_RECEIVER` alone patches nothing. MultiFinder comes
with `FDROP_ROMPLUS`.

### What each switch is

| switch | what it is | bytes it adds to the example accessory |
|---|---|---|
| `FDROP_ROM_ROUTE` | **Systems 1.1 and 2.0**: the sender's route for Finders that drag through the ROM | 2348 |
| `FDROP_FINDER5` | **Systems 2.1 to 3.2**, with `FDROP_ROM_ROUTE`: Finders 5.0 to 5.3's records, and their System's patch in front of the ROM routine on the Plus ROM | 486 |
| `FDROP_OWN_ROUTE` | **Systems 3.3 to 6.0.8**: the sender's route for Finders 5.4 to 6.1.8 | 2972 |
| `FDROP_RECEIVER` | **System 7.5 and later**, and 7.0 or 7.1 with Apple's Drag and Drop extension: the receiver | 858 |
| `FDROP_ROM64K` | the 64K ROM of the Macintosh 128K and 512K, patched by a path of its own | 518 |
| `FDROP_ROMPLUS` | the Plus ROM and later: the longword trap table, MultiFinder, a second copy alongside, cache flushing | 1202 |
| `FDROP_DIAG` | off unless given: `FDROP_DG` words behind `fdrop_diag`'s eleven, filled as a drag goes | |
| `FDROP_STK` | off unless given, with `FDROP_DIAG`: how close a drag comes to the Finder's heap | |

On a System or ROM whose switch is off nothing happens: the drag is the
Finder's alone, and a build's patches pass it through untaken (run on
every build against Systems 1.1, 3.0, 6.0.8 and 7.5.3). **A build holds
nothing of a switch that is off** — no function, state word, store or
call; `examples/check_switches.sh` checks that by name.

Two things every sender build holds anyway, so they are not taken for
faults: a build for the later ROMs patches `_FindWindow` even without
`FDROP_OWN_ROUTE`, because another copy may take over its trampoline;
and two of the sender's words behind `fdrop_diag` are kept (the files
the last drop carried, and the drags recognised), so a result can be
read out of a shipping build. How far a drag got, `FDROP_STEP`, is
`FDROP_DIAG`'s.

### What each build weighs

The example accessory as a `DRVR`, about 1.5 KB of which is the example:

| built for | flags | bytes |
|---|---|---|
| everything | none | 11224 |
| the 64K ROM, Systems 1.1 and 2.0: the smallest sender | `FDROP_FINDER5` `FDROP_OWN_ROUTE` `FDROP_RECEIVER` `FDROP_ROMPLUS` off | 5700 |
| the 64K ROM, Systems 1.1 to 3.2 | `FDROP_OWN_ROUTE` `FDROP_RECEIVER` `FDROP_ROMPLUS` off | 5892 |
| the 64K ROM, Systems 3.3 and 4.1 | `FDROP_ROM_ROUTE` `FDROP_FINDER5` `FDROP_RECEIVER` `FDROP_ROMPLUS` off | 7146 |
| Systems 1.1 to 3.2 on any ROM | `FDROP_OWN_ROUTE` `FDROP_RECEIVER` off | 7300 |
| Systems 4.0 to 6.0.8 alone | `FDROP_ROM_ROUTE` `FDROP_RECEIVER` `FDROP_ROM64K` off | 7528 |
| System 4.0 and later (a control panel) | `FDROP_ROM_ROUTE` `FDROP_ROM64K` off | 8394 |
| the Drag Manager alone, patching nothing | `FDROP_ROM_ROUTE` `FDROP_OWN_ROUTE` off | 3064 |

Every row has taken its drops on an emulated Macintosh
(`NOTES.md`, the entries on the trimmed builds).

## The examples

`examples/` holds three programs that do nothing but be dropped
on — a desk accessory, a control panel and an application — each the
smallest complete use of the library in its form, commented to be read,
and each a debugger when built `-DFDROP_DIAG=1`.
`examples/README.md` is the index.

## Licence

**0BSD**, `LICENSE`: use, copy, modify and distribute for any
purpose, with no conditions — not even keeping the notice. Every file
carries `SPDX-License-Identifier: 0BSD`, so a file copied alone still
says what it is under.

## Documents

- `NOTES.md` — what the Finder does not tell you: its item
  records, its drag routine in every version, what a press looks like
  from inside a patch. **Grep its headings before changing the sender.**
- `OPEN_ITEMS.md` — what is missing.
- `BENCH.md` — the sessions on real Macintoshes, in order.
  Sessions 1 to 5 have been run, on a Plus and an SE/30.
- `PARKED.md` — the System 7 sender: what was tried, and why it
  could not be made to work properly.
- `ARCHAEOLOGY.md` — the runs behind numbers in the code.
- `HISTORY.md` — the notebook of the probe that came before the
  library: how the sender was found, wrong turns included. Stale by
  design; `NOTES.md` is what to believe.
