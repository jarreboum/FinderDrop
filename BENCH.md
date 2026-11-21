# BENCH.md — FinderDrop on real Macintoshes, in order

Sessions 1 to 5 have been run, on a Plus and an SE/30
(`NOTES.md`, the bench entries); Session 7 is new; before them the sender had run only on emulators, and
the one real machine this library had met, an LC475 under System 7.6.1,
took its drops through the Drag Manager. This file is the plan for
closing that:
the sessions in the order that yields the most, written for a person at
the machine.

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

Use the diagnostic builds for every session. The plain builds are for
one run at the end of Session 1, to see the shipped bytes work.

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
- [ ] **For Session 4 only:** a System 6.0.4 or earlier that boots the
      SE/30 (it shipped with 6.0.3), on floppies like everything else.
- [ ] **For Session 5 only:** an AppleShare server the System 6 machine
      can reach over AppleTalk, which the compact machines have not got
      as they stand. The LC475 with File Sharing turned on would be the
      server, and it needs a LocalTalk connection between its printer
      port and the compact's — two LocalTalk boxes and their cable, or
      whatever else joins two printer ports as a network. Without one,
      Session 5 cannot be run. A server that speaks AFP over TCP alone
      is no use here: System 6 cannot reach it.

## Session 1 — the Plus, System 6.0.8, no MultiFinder, the control panel

The first time the sender meets hardware, in the form that is easiest to
install: copy `FinderDrop` off the floppy into the System Folder,
restart, and open the Control Panel.

- [ ] **Open the Control Panel and select FinderDrop.** Expect its
      first line to read "Following the Finder's drag."
- [ ] **Drag `DragMe` out of the disk's window onto the panel.** Expect
      one line naming it, and the icon back in its window.
- [ ] **Select `DragMe` and `DragTwo`, drag both.** Expect two lines.
- [ ] **Drag the long-named file.** Expect its whole name.
- [ ] **Put `DragMe` on the desktop, drag it from there.** Expect one
      line.
- [ ] **Drag the folder.** Expect a line that says folder.
- [ ] **Drag the disk's own icon.** Expect a line that says disk,
      directory 1.
- [ ] **A file off an ejected floppy.** Open a floppy's window, select
      a file, press Command-E: the icons go dim. Drag the dimmed file
      onto the panel. Expect the icon back in its window, then "Please
      insert the disk" naming the floppy; put it back, and expect one
      line naming the file. The same with another floppy put in first:
      expect the alert to stay until the right one goes in.
- [ ] **Drag a file across the panel and let go on the desktop.**
      Expect nothing listed, and the file on the desktop: that drag is
      the Finder's.
- [ ] **Drag a file onto the Control Panel's title bar and let go
      there.** Expect nothing listed: a release on the title bar is the
      Finder's, as it is under MultiFinder.
- [ ] **With the panel still open, drag a file to the Trash and empty
      it; drag a window by its title bar.** Expect the Finder to behave
      as it does with the panel closed.
- [ ] **Close the Control Panel. Drag a file in the Finder.** Expect
      nothing unusual. Open the panel again and drag onto it. Expect
      the drop taken. Do the close, open and drag five times.
- [ ] **The plain build, once:** replace the panel with the plain one,
      restart, and take one drop out of a window.

If the very first drop is not taken, photograph the hex words and stop
the session there: every later step depends on it.

## Session 2 — the Plus, the desk accessory

The same library in the form that lives longest on a System 6 machine.
Install the accessory with Font/DA Mover and choose it from the Apple
menu.

If it bombs the moment it opens, every time, remove it with Font/DA
Mover and install it again before believing anything else: on the
Plus, one damaged copy did exactly that and the next was fine
(`NOTES.md`, the entry on bench Session 2).

- [ ] **The drops of Session 1**, the first five. Expect the same.
- [ ] **Close it by its close box, drag in the Finder, open it again,
      drag onto it.** Five times.
- [ ] **With the accessory open, launch an application from the
      Finder.** Expect the accessory to go and the application to run.
      Quit. Back in the Finder, drag a file from one window to another.
      Expect an ordinary Finder drag. A bomb here is the case the
      trampoline's pass-through exists for; photograph it.
- [ ] **Shut Down from the Special menu with the accessory open.**
      Expect an ordinary shutdown.

## Session 3 — the Plus, MultiFinder

Set the startup to MultiFinder and restart.

- [ ] **The accessory, from the Finder's Apple menu:** a drop out of a
      window, two files, the desktop. Expect them taken.
- [ ] **The example application:** launch it, and with its window
      showing, switch to the Finder and drag a file onto the window.
      Expect "Following the Finder's drag." and the file listed.
- [ ] **Both open at once.** Drag one file onto each. Expect each to
      list only its own.
- [ ] **The accessory opened with Option held, over the application.**
      Hold Option while choosing FinderDrop from the application's
      Apple menu. Drag a file onto it; expect it taken. Note where the
      accessory's window is on the screen. Quit the application with
      Quit in its File menu (or Command-Q). In the
      Finder, drag a file and let go where that window was. Expect the
      file to move there. A file that stays put, or a bomb, is the fault
      the goodbye call was added for. TeachText's Apple menu, with
      Option held, does as well as the example application's.

## Session 4 — the SE/30: a processor with a cache, and Color QuickDraw

Two open items meet on this machine (`OPEN_ITEMS.md`: a cache
with no `_HWPriv`, and a colour port). Neither could be run on an
emulator here.

- [ ] **System 6.0.8: Sessions 1 and 3 again, the first three drops of
      each.** This is the colour-port question. Whether the Finder's
      windows on this machine are colour ports is not known; the
      diagnostic words say which path was taken, so photograph them
      with each drop. If a Macintosh II family machine with a colour
      screen is to hand, the same steps there are worth more.
- [ ] **System 6.0.4 or earlier, the cache on:** open the accessory,
      take a drop, close it, open it again, take a drop. Twenty times.
      Expect twenty drops. A crash that comes only sometimes is the
      finding: it is what a stale instruction cache looks like.

## Session 5 — a file server, System 6

Run on the SE/30 (`NOTES.md`, the entry on a file server).
What the setup took, for the next time:

- The LC475 is the server: File Sharing on, a folder on its own disk
  shared, Guest allowed in Users & Groups. It cannot share a volume it
  has mounted from the NAS.
- AppleTalk on the LC475 must be set to the printer port, and Apple's
  LocalTalk Bridge extension, if installed, turned off first: it holds
  the printer port itself, and the machine froze when both wanted it.
- On System 6 the AppleShare client is installed with the Installer
  ("AppleShare (workstation software)"); the `AppleShare` file copied
  into the System Folder alone gave "Some AppleShare resources were not
  installed at system startup time".
- In the drag: a file out of the server volume's window, and the server
  volume's own icon. A file cannot be put on the desktop from a server
  volume: the Finder refuses it ("Items from shared volumes may not be
  moved onto the desktop").

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

## If one is ever to hand

Each of these would be a first, and none is needed before the library
is used:

- **A Macintosh 128K or 512K on Systems 1.1 to 3.2.** The 64K ROM's
  path has only run emulated. Session 2's first five steps with the
  accessory built for it (`examples/README.md` has the flags).
- **A 512K with a Hard Disk 20, System 3.3 or 4.1.** The emulated run
  stood an 800K disk in for the drive and left out part of Apple's
  `Hard Disk 20` file.
- **System 4.0 on anything.** Its disk freezes under emulation before a
  drag can be made.
- **A Plus with a 68020 or 68030 accelerator, below System 6.0.5.**
  Session 4's twenty-times step.
