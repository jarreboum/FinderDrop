# BENCH.md — FinderDrop on real Macintoshes, in order

What is left to run on real Macintoshes, in the order that yields the
most, written for a person at the machine. A session that has been run
is deleted from here: its results are in `NOTES.md` (Sessions 1 to 5,
on a Plus and an SE/30, are its bench entries), and the numbers of the
rest are kept so those entries still name them.

**Results do not go in this file.** They go in `NOTES.md`, one
entry per session, and what a session closes is deleted from
`OPEN_ITEMS.md`. Every "expect" below is a **prediction** read
off an emulator. A machine that disagrees is the result, not a fault of
the session; where the day contradicts a step, fix the step here.

Photograph the screen. There is no screenshot on a Plus, and a bomb box
is a photograph.

## What a session looks at

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

Use the diagnostic builds for every session.

## The machines

The sessions are written for these four; any other is named where it
would add something.

- **Macintosh Plus, 1 MB** — the only 68000, and the machine every
  emulated run of the sender imitated. 800K floppies.
- **SE/30, 4 MB** — a 68030 with a cache and Color QuickDraw in ROM.
- **LC475, System 7.6.1** — the only one of the four on the network,
  and the one that writes the floppies. Already run, through the Drag
  Manager.
- **iMac G3, Mac OS 9** — a PowerPC running the 68K code under Apple's
  emulator.

**The Plus and the SE/30 are not networked.** Everything reaches them
on 800K floppies written by the LC475: the Plus reads nothing denser,
so write 800K for both and one set serves the two machines.

## Before you start

- [ ] **The kit.** `FinderDrop Kit.image`, one 800K Disk Copy 4.2
      image, in `FinderDropBench` at the top of the Mac share. Write it
      to a floppy on the LC475 with Disk Copy. Everything on it is
      already decoded, so nothing has to be unpacked on the compact
      machines:
  - `Diagnostic` and `Plain`, each holding the example control panel
    (`FinderDrop`), the example application (`FinderDrop App`) and the
    example desk accessory in a Font/DA Mover suitcase (`FinderDrop
    DA`). Double-clicking the suitcase opens it in Font/DA Mover; copy
    `FinderDrop` into the System file from there. Both suitcases name
    the accessory `FinderDrop`, so install one at a time.
  - `Test Files`: `DragMe`, `DragTwo`, one with a 31-character name, and
    `Folder` with `Inside` in it.
- [ ] **Copy the test files to the machine's own disk** before the
      first session, so that drags come from the disk the machine
      started from and not only from the floppy.
- [ ] **System 6.0.8 on the Plus and on the SE/30**, each with its own
      MultiFinder and Font/DA Mover.
- [ ] **For Session 7:** a 400K MFS floppy. Any old single-sided
      floppy is one; or write Apple's System 4.0 disk image (the share's
      `OS/System 4`) to a 400K floppy on the LC475 with Disk Copy. Its
      files will be dragged and one of them put in a folder, so use a
      copy. Do not start up from it: put it in after the machine has
      started from its own disk.
- [ ] **If the accessory bombs the moment it opens, every time,**
      remove it with Font/DA Mover and install it again before
      believing anything else: on the Plus, one damaged copy did
      exactly that and the next was fine (`NOTES.md`, the entry on
      bench Session 2).

## Session 6 — the machines with a Drag Manager

Short: the library patches nothing here, and the LC475 has already
taken drops.

- [ ] **LC475, 7.6.1:** the example application and the control panel,
      one drop each. Expect the first line to read "Through the Drag
      Manager."
- [ ] **iMac G3, Mac OS 9:** the same two. This is the 68K code under
      the PowerPC's emulator, which only an emulated machine has run.

## Session 7 — an MFS floppy, on the Plus and on the SE/30

Finders 5.4 to 6.1.8 take a 400K floppy's files since this was written,
and only emulated machines have done it (`NOTES.md`, the entry
on Finders 5.4 to 6.1.8 taking a drop from an MFS volume). An MFS
volume has no directories, so every file arrives in directory 0. Run
it on the Plus under System 6.0.8 without MultiFinder, then on the
SE/30 under 6.0.8 with MultiFinder; the accessory either time, the
diagnostic build.

- [ ] **Put the MFS floppy in and open its window.** Expect its files,
      and "Floppy" or whatever its name is, as on any disk.
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
      insert the disk": the accessory asks the File Manager nothing about
      a file in directory 0.
- [ ] **A file of the startup disk and a file of the floppy, both on
      the desktop, selected together and dragged.** Expect two lines:
      the startup disk's in directory 2, the floppy's in directory 0.

If a drop is refused, photograph the hex words: they say where it
stopped.

## Session 8 — the views, on the Plus, one System at a time

Since 2026-11-27 a drop is the window's selection, whatever the view,
and the press is no longer compared with an icon (`NOTES.md`, "The
selection is the drag, in every view"). Only emulated machines have
run it. The four floppies in `FinderDropBench`, folder `Plus floppies
2026-11-27`, each START the Plus: `FinderDrop Plus 4.1`, `4.3`,
`6.0.3` and `6.0.8`. Each has the diagnostic accessory already in its
System, `DragMe`, and a folder `Views` holding `Alpha` and `Beta`
overlapping by half an icon, and `Gamma`. Write them to 800K floppies
on the LC475 with Disk Copy, and do not lock them.

On each floppy, started with no MultiFinder, choose FinderDrop from the
Apple menu, then open `Views`:

- [ ] **View by Name. Drag `Alpha` onto the accessory.** Expect one
      line naming it.
- [ ] **Select `Alpha` and `Beta` (Shift-click), drag both.** Expect
      two lines.
- [ ] **View by Small Icon. Drag `Gamma` by its name.** Expect one
      line.
- [ ] **View by Icon. Drag `Gamma` by the name under its icon.** Expect
      one line.
- [ ] **Click empty space, then drag from where `Alpha` and `Beta`
      overlap.** Expect one line, naming whichever of the two the
      Finder highlights.
- [ ] **Drag `DragMe` off the desktop.** Expect one line.
- [ ] **Press empty space in `Views` and drag a rubber band out over
      the accessory, let go.** Expect nothing listed.
- [ ] **Select `Alpha` and `Beta`, hold Shift, press `Alpha` and drag
      onto the accessory.** Expect nothing listed: the Finder deselects
      it and does not drag.

On `4.3`, `6.0.3` and `6.0.8`, then choose MultiFinder with Set Startup
in the Special menu, restart, and do the first three steps again.

If a drop is refused, photograph the hex words: the fifth is a hundred
times the drags recognised plus the step it stopped at, and 97 now means
nothing was selected in that window.

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
  Open the accessory, take a drop, close it, open it again, take a
  drop: twenty times. A crash that comes only sometimes is what a stale
  instruction cache looks like.
