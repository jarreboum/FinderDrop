# What is missing from the drag library

What is open, and nothing that is closed: an item that is done is deleted
from here, not marked. What was built and taken back out is
`PARKED.md`; what each platform fact cost is `NOTES.md`.

The library is to serve three hosts — a control panel, a desk accessory
and an application — from System 1.1 up, choosing the Drag Manager
wherever the machine has one. The choice is built and does not move:
below 6.0.4 there is no Gestalt and never a Drag Manager, from 6.0.4 the
library asks.

---

## 1. A cache with no `_HWPriv` and no flush: a 68000-ROM machine with an accelerator

`tramp_flush` flushes through `_HWPriv` where the trap exists. Where it
does not, the Macintosh II and the IIx-family ROM (IIx, IIcx, SE/30) on
Systems 4.1 to 6.0.4 are covered after all, because their ROM's
`_GetTrapAddress` flushes the instruction cache and `tramp_flush` calls
it (`NOTES.md`, the entries on a machine without `_HWPriv` and
on the SE/30's ROM). What is left is a 68000-ROM machine -- a Plus or
an SE -- with a 68020 or 68030 accelerator, below System 6.0.5: Apple
installs `_HWPriv` there only from 6.0.5 and `_CacheFlush` never, and
whether the cache is on at all is up to the accelerator.

**What closes it:** there is no Apple answer to read for an accelerated
Plus, so it is a bench run with the accelerator's cache on, or a
sentence saying it is unsupported.

## 2. A colour port that may not have been exercised

It is written to be safe by construction, and has not provably met the
machine it is for. The three places that want a port's origin read it
  from the PixMap when the port is a `CGrafPort`. A real SE/30, whose ROM
  has Color QuickDraw, took every drop under System 6.0.8 with and
  without MultiFinder; whether its Finder's windows were colour ports
  there is not known, since nothing records which branch ran. *What
  closes it:* System 6 on a Macintosh II with a colour screen, with and
  without MultiFinder: a drop out of a Finder window, and one onto a
  host whose window is a colour window.

## 3. System 6.0.8L has not been run

Its Finder is 6.1.8 with the code of 6.0.8's, byte for byte, so the
recognition is the one already run. Its System file is not: it stops at
startup on a Macintosh Plus, on a Macintosh II and on a Quadra, and on
a machine that says it is an LC but runs with 32-bit addressing
(`NOTES.md`, the entry on System 6.0.8L). No emulator here is
a Classic, a Classic II, an LC, an LC II or a PowerBook 100 with 24-bit
addressing.

**What closes it:** one of those five machines, real or emulated, under
6.0.8L: a file out of a window, two files in one drag, a file off the
desktop, and one of the Finder's own drags with a host open.

## 4. An ejected MFS disk on the 64K ROM under Finder 5.5

On the emulated 512K, Command-E on a 400K MFS floppy brings the
Finder's own "Please insert the disk" at once, with the floppy's window
open or closed and with no accessory open at all, so the dimmed icons a
drag would start from are never there to press. On the Plus ROM the
same floppy ejects and its files are taken unchecked, as the rule for
an ejected disk says (`NOTES.md`, the entry on Finders 5.4 to
6.1.8 taking a drop from an MFS volume). Read from the code and not
run: the 64K ROM takes the same path.

**What closes it:** a real Macintosh 512K under System 3.3 or 4.1, a
second floppy drive and an MFS floppy ejected with Command-E: a file
off it, and the disk's own icon.

## 5. Two quick drops in succession open the file

On a real Macintosh Plus, System 4.0 (Finder 5.4), a file dropped on
the accessory and pressed again at once is opened by the Finder, as a
double-click: "An application can't be found for this document." The
Finder alone does not do this after a drag that moved something.

*Reproduced on the emulator*, 6.0.8 single Finder on a Plus at 1x
(`DoubleTime` 20 ticks): a drop the accessory takes, then a press on
the same icon about 0.3 s after the release, puts up "The file
"DragMe" could not be opened/printed". A taken drop is ended for the
Finder as a drag of offset zero back into the window it came from
(`fdrop_answer` and `rom_answer` clear the result at A6+30), and the
Finder treats that as a click.

*Not the fix: ending it as an abandoned drag.* With `0x80008000` written
into the result instead, the drop is still taken, but the Finder moves
the icon to the top-left corner of the desktop: no Finder tests for
that value (NOTES.md, "The drag routine is the same in every Finder
from 5.4 to 6.1.8"), so it is applied as an offset. Whatever ends the
drag without a click has to be found in the Finder's own handling of
the result, or in what it keeps of the last click; that has not been
read.

Not run: a drag the Finder alone brings back to where it began, then a
quick press, which says whether any offset of zero is a click.

**What closes it:** a change, a quick double drop on the emulator
under the single Finder and MultiFinder and on Finder 5.4, then on a
real Plus.

## 6. Dragging out to the Finder (a goal for 0.2)

FinderDrop takes drops from the Finder; it cannot start a drag that the
Finder receives. A developer using the Drag Manager route will expect
both directions, so 0.2 adds the other: a drag begun in the host's
window and dropped in the Finder, as an existing file (the Finder moves
or copies it) or as a promised one, written where it lands (an FTP
client's download, an archive's extraction). This is the Drag Manager's
own job, so it is built on that route alone: System 7.5 and later, and
7.0 to 7.1 with Apple's Drag and Drop extension. Below that it is not
offered, because the Finder accepts drags only from its own icons and
under the single Finder is not even running beside the host; the
README says so where the feature is described.

**What closes it:** the call, a test of both kinds of drag on 7.5.3 and
Mac OS 9 under emulation, then on a real machine.
