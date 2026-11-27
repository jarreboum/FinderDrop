# BENCH.md — FinderDrop on real Macintoshes, in order

What is left to run on real Macintoshes, as WALKS: one start-up each,
in the order that needs the fewest floppy swaps and restarts, each
doing everything that start-up can test. A walk that has been run is
deleted from here and its results go in `NOTES.md`, whose bench entries
name the older numbered sessions (1 to 9) they came from.

**Results do not go in this file.** They go in `NOTES.md`, one
entry per walk, and what a walk closes is deleted from
`OPEN_ITEMS.md`. Every "expect" below is a **prediction** read
off an emulator. A machine that disagrees is the result, not a fault of
the walk; where the day contradicts a step, fix the step here.

Photograph the screen. There is no screenshot on a Plus, and a bomb box
is a photograph.

## What a walk looks at

Each example program lists what is dropped on it, one line an item:

    Following the Finder's drag.  Received: 3
    file DragMe  on Work (-2), directory 2

- **A drop is taken** when the line names the file that was dragged, on
  the volume it is on, and the Finder's icon is back where it was.
- **A drop is refused** when nothing is listed and the Finder puts the
  icon back, or moves it as if the window were not there.
- **A diagnostic build** (`-DFDROP_DIAG=1`) also draws the library's own
  words in hex under the list. On a real machine there is no RAM to
  read afterwards, so they are the only evidence of how far a refused
  drag got. Photograph all of them, before and after the drag;
  `fdrop.c` says what each word holds.

Every program on the floppies is the diagnostic build.

## The machines

The walks are written for these four; any other is named where it
would add something.

- **Macintosh Plus, 1 MB** — the only 68000, and the machine every
  emulated run of the sender imitated. 800K floppies.
- **SE/30, 4 MB** — a 68030 with a cache and Color QuickDraw in ROM.
- **LC475, System 7.6.1** — the only one of the four on the network,
  and the one that writes the floppies. Already run, through the Drag
  Manager.
- **iMac G3, Mac OS 9** — a PowerPC running the 68K code under Apple's
  emulator.

**The Plus and the SE/30 are not networked, and each has one floppy
drive.** Every walk on them starts from one of the floppies below,
written by the LC475: the Plus reads nothing denser than 800K, so one
set serves the two machines.

## Before you start

- [ ] **The floppies.** In `FinderDropBench`, folder `Bench floppies`,
      at the top of the Mac share; write each with Disk Copy on the
      LC475, and do not lock them (the Finder writes to them):
  - `FinderDrop System 4.1`, `4.3`, `6.0.3` and `6.0.8` (800K). Each
    STARTS the machine, from Apple's own disk for that System, and
    carries the three examples, every one the diagnostic build, already
    installed: the desk accessory `FinderDrop` in the Apple menu, the
    control panel `FinderDrop` in the Control Panel, and the
    application `FinderDrop App` at the top of the disk. And the test
    files: `DragMe`, `DragTwo`, one with a 31-character name, `Folder`
    with `Inside` in it, and `Views` with `Alpha` and `Beta` overlapping
    by half an icon and `Gamma`. The volume is named for its System.
  - `FinderDrop MFS` (400K, MFS): `Alpha`, `Beta`, `Gamma`, `Delta` and
    `Eps`, for Walk 7. It does not start a machine.
  - `FinderDrop App.bin` and `FinderDrop control panel.bin`, MacBinary,
    for the LC475 and the iMac, which do not need a floppy.
  - `Control 6.0.8`: Apple's own 6.0.8 disk with nothing of ours on
    it, for Walk 6.
- [ ] **Unlock the MFS floppy too.** Locked, the Finder dims New Folder
      on it, which Walk 7 needs; every drop still works.
- [ ] **MultiFinder** is on the 4.3, 6.0.3 and 6.0.8 floppies, not
      chosen: Set Startup in the Special menu chooses it, and the floppy
      remembers.
- [ ] **If the accessory bombs the moment it opens, every time,** write
      that floppy again before believing anything else: on the Plus, one
      damaged copy did exactly that and the next was fine (`NOTES.md`,
      the entry on bench Session 2).

## The two step lists the walks use

**The view steps.** Choose FinderDrop from the Apple menu, then open
`Views`:

- **V1. View by Name. Drag `Alpha` onto the accessory.** Expect one
  line naming it.
- **V2. Select `Alpha` and `Beta` (Shift-click), drag both.** Expect
  two lines.
- **V3. View by Small Icon. Drag `Gamma` by its name.** Expect one line.
- **V4. View by Icon. Drag `Gamma` by the name under its icon.** Expect
  one line.
- **V5. Click empty space, then drag from where `Alpha` and `Beta`
  overlap.** Expect one line, naming whichever of the two the Finder
  highlights.
- **V6. Drag `DragMe` off the desktop.** Expect one line.
- **V7. Press empty space in `Views` and drag a rubber band out over
  the accessory, let go.** Expect nothing listed.
- **V8. Select `Alpha` and `Beta`, hold Shift, press `Alpha` and drag
  onto the accessory.** Expect nothing listed: the Finder deselects it
  and does not drag.

**The title-bar steps,** on whichever window the walk names. Arrange
`Views` so that it covers part of that window and leaves a strip of its
title bar showing:

- **T1. Drag `Alpha` and let go on that strip of title bar.** Expect
  nothing listed, and `Alpha` back in `Views`, not on the desktop.
- **T2. Drag it again and let go in the window's content.** Expect one
  line.

If a drop is refused that should have been taken, photograph the hex
words: the fifth is a hundred times the drags recognised plus the step
it stopped at, and 97 means nothing was selected in that window.

## Walk 1 — the Plus, 6.0.8, no MultiFinder

Start from `FinderDrop System 6.0.8`.

- [ ] **T1 and T2 on the accessory.** Then let go once on its frame,
      the thin edge below the content: nothing listed, `Alpha` back.
- [ ] **T1 and T2 on the Control Panel**, with FinderDrop selected in
      it.
- [ ] **Choose MultiFinder** with Set Startup in the Special menu, and
      restart: the floppy keeps it for Walk 2 and for Walk 7.

## Walk 2 — the Plus, 6.0.8, MultiFinder

- [ ] **T1 and T2 on the accessory.**
- [ ] **Launch `FinderDrop App`,** switch back to the Finder, **T1 and
      T2 on the application's window.** (Without MultiFinder, launching
      it quits the Finder, and there is nothing to drag from.)

## Walk 3 — the Plus, 6.0.3

Start from `FinderDrop System 6.0.3`.

- [ ] **V1 to V8.**
- [ ] **T1 on the accessory.**
- [ ] **Choose MultiFinder** with Set Startup, restart, and **V1 to V3**
      again.

## Walk 4 — the Plus, 4.3

Start from `FinderDrop System 4.3`: as Walk 3.

- [ ] **V1 to V8.**
- [ ] **T1 on the accessory.**
- [ ] **Choose MultiFinder** with Set Startup, restart, and **V1 to V3**
      again.

## Walk 5 — the Plus, 4.1

Start from `FinderDrop System 4.1`, which has no MultiFinder.

- [ ] **V1 to V8.**
- [ ] **T1 on the accessory.**

## Walk 6 — the Plus, left alone

Not FinderDrop's, and the reason it is here: started from the
FinderDrop 6.0.8 floppy and left at the desktop with nothing opened,
the Plus once put up "coprocessor not installed" after about half an
hour (`NOTES.md`, the entry on Session 7 on the Plus). This is the
control.

- [ ] **Start from `Control 6.0.8`, open nothing, and leave it an hour.**
      A bomb means the machine or the floppies, not the library; none
      means the next step is the FinderDrop floppy left the same way.

## Walk 7 — the SE/30, 6.0.8 with MultiFinder, and an MFS floppy

Start from the `FinderDrop System 6.0.8` floppy Walk 1 set to
MultiFinder, and choose FinderDrop from the Apple menu. An MFS volume
has no directories, so every file arrives in directory 0. With one
drive the two floppies share it, and the Finder asks for each by name
as it needs it: put in the one it names, as often as it asks. Asking
is not a result; a refusal of a drop is.

- [ ] **T1 and T2 on the accessory,** before the swap.
- [ ] **Eject the startup disk** (select its icon, Eject in the File
      menu) **and put in `FinderDrop MFS`.** Open its window: `Alpha` to
      `Eps`.
- [ ] **Drag one file out of its window.** Expect "file NAME on DISK
      (−2), directory 0" (the number is the volume's and may differ).
- [ ] **Select two files, drag both.** Expect two lines, directory 0.
- [ ] **New Folder in the floppy's window, drag a file into it, then
      drag the folder.** Expect one line naming the FILE, directory 0:
      an MFS folder exists only in the Finder, so it arrives as the
      files in it.
- [ ] **Open the folder, drag the file out of the folder's window.**
      Expect the same line again.
- [ ] **Put a file of the floppy on the desktop, drag it from there.**
      Expect one line, directory 0.
- [ ] **Drag the floppy's own icon.** Expect "disk NAME on NAME,
      directory 1".
- [ ] **Throw a file of the floppy away (do not empty the Trash), then
      drag the Trash.** Expect that file, directory 0, and nothing else.
- [ ] **Command-E with the floppy's window in front, then drag a dimmed
      file.** Expect one line naming it, directory 0, and no "Please
      insert the disk".
- [ ] **A file of the startup disk and a file of the floppy, both on
      the desktop, selected together and dragged.** Put `DragMe` on the
      desktop first, while the startup disk is in. Expect two lines:
      the startup disk's in directory 2, the floppy's in directory 0.

## Walk 8 — the LC475, 7.6.1, the Drag Manager

The library patches nothing here. Decode `FinderDrop App.bin` and
`FinderDrop control panel.bin` with StuffIt Expander, the control panel
into the Control Panels folder. Expect each window's first line to read
"Through the Drag Manager."

- [ ] **The application: a file into its content, then one let go on
      its title bar.** Expect one line; then nothing more, and the icon
      going back.
- [ ] **The control panel: the same two.**

## Walk 9 — the iMac G3, Mac OS 9

- [ ] **As Walk 8.** This is the 68K code under the PowerPC's emulator,
      which only an emulated machine has run.

## If one is ever to hand

Each of these would be a first, and none is needed before the library
is used:

- **A Macintosh 128K or 512K on Systems 1.1 to 3.2.** The 64K ROM's
  path has only run emulated. With the accessory built for it
  (`examples/README.md` has the flags): a file out of a window, two
  files, the long-named one, a file off the desktop, the disk's icon.
- **A 512K with a Hard Disk 20, System 3.3 or 4.1.** The emulated run
  stood an 800K disk in for the drive and left out part of Apple's
  `Hard Disk 20` file.
- **System 4.0 on anything.** Its disk freezes under emulation before a
  drag can be made.
- **A Plus with a 68020 or 68030 accelerator, below System 6.0.5.**
  From the `FinderDrop System 6.0.3` floppy: open the accessory, take
  a drop, close it, open it again, take a drop: twenty times. A crash
  that comes only sometimes is what a stale instruction cache looks
  like.
