# What the Finder does not tell you

The platform facts the drag library depends on — what a Finder keeps in
its own heap, what it asks the File Manager and what it does not, what a
press looks like from inside a trap patch. Each one cost a run to find,
is written down nowhere else, and fails as a drag that silently picks up
the wrong file or nothing at all. Each names the code that depends on it;
**the code is the source of truth**, and this is the index.

**The headings are the facts.** `grep -n '^#' NOTES.md` and
read the one entry you need.

Most of these were found in a control panel built on the library, driven
in emulated Macintoshes by that program's own harness; the later ones
with the library's examples. "The panel" in an entry is that control
panel, and an image file is what it was dragging. The "how to repeat
it" lines name that harness's scripts and options (`dragshell.py`,
`--mm`, `MM_ROOT` and the rest); the harness is not published.

---

### Opening a volume's window asks the File Manager nothing

*Found on:* System 6.0.8, Macintosh Plus.

The drag sender learns which folder a Finder window is showing by watching
`PBGetCatInfo` go past on `_HFSDispatch` and matching the folder's name
against the window's title. That works for a **folder** window: the Finder
reads the folder when the window opens.

It does not work for a **volume's own** window, and the measurement is
stark. A session in which a disk window was opened and dragged from
produced **three hundred `PBGetCatInfo` calls and not one of them named
that disk** — every one was ours. The Finder reads a volume's contents when
it MOUNTS the volume, which is before a control panel has been opened;
double-clicking the disk icon afterwards draws a window out of what it
already has.

The fix is not a better watch on the trap, because there is nothing to
watch. A window whose title is a mounted volume's name **is** that volume's
root, and a root is directory 2 on every HFS volume — so the question is
only which volume, and `PBHGetVInfo` over the mounted ones answers it
exactly. `folder_of` in `fdrop.c`, last of three routes.

The general shape: *the Finder asking the File Manager is evidence that it
is reading something, and its silence is not evidence that it is not
showing something.* A rule built on observed traffic has a blind spot
exactly where the Finder has a cache.

*Since 2026-10-19 no code depends on this:* `folder_of` and the
`_HFSDispatch` patch are gone, because a window's own record says which
folder it shows (the entry on the Finder's records, below).

### The Finder lays out a window its own way, whatever the catalog says

*Found on:* System 6.0.8, Macintosh Plus.

A file copied onto a volume by `hfsutils` has `fdLocation` 0,0 — the
Finder's "nobody has placed this" — and anything that identifies a file
from its icon position cannot see it. The obvious fix is to write a
position into the catalog record on the host before the machine boots. **It
does not take.** Written correctly (`filUsrWds` at record offset 4,
`fdLocation` ten bytes into it, and a Point is v then h), with the record
read back on the host to confirm the bytes are there, the Finder still
arranged both icons in its own grid at its own coordinates.

What does work is the thing a user has already done: drag the icon on the
machine, and the Finder writes the position down itself. In a harness that
has to happen **before** the drag sender is installed, or the sender takes
that drag and the Finder never sees it — its own small lesson about testing
a thing that intercepts the input you are testing with.

### A Finder marks a selected icon in its own heap, and every number in that record moves between versions

*Found on:* System 4.1 (Finder 5.5) and 6.0.8 on a Macintosh Plus at 1 MB,
and System 7.0.1.

The Finder keeps one record per item in a window, in an array, with the
item's name inside it as a Pascal string. Three things in that record
matter here and **not one of them is the same on all three systems**:

| | 4.1 | 6.0.8 | 7.0.1 |
|---|---|---|---|
| `fdLocation`, from the name | −96 | −96 | −82 |
| selected, from the name | −77 | −77 | −63 |
| what "selected" reads as | `0x02` | `0x02` | bit `0x08` of `0x80` |
| one record to the next | 118 | 118 | 120 |

There is no selection list anywhere else: the fact is in the item's own
record. That was the open question: whether it was reachable at all
had not been looked at.

*What makes it believable rather than plausible.* Twelve dumps of the
whole application heap — 840 KB on 6.0.8, 899 KB on 4.1 — in four
selection states: one icon left selected by the drag that placed it, then
nothing selected, then one clicked, then two with a shift-click. Searching
every byte of the heap for one that splits exactly the way the selection
did, **the record byte is the only one in memory** whose state goes on,
off, on with the second icon's. (A single step is weaker: 21 other bytes
follow the first icon's by coincidence, and not one of them is at the same
offset in both records.)

*The last row is not a stride, and nothing walks by it any more.* Each
record is a heap block of its own, and 118 is the size of the block for a
name ten characters long, which every test image's is; records are 118
apart only when blocks of that size happen to be allocated side by side.
The entry on the Finder's records, below, has the measurement.

**None of these numbers is written down in the program**, and the table
above is here as evidence rather than as a constant to copy: three
Finders disagreeing about all four is what says the offsets have to be
measured on the machine. `fdrop.c` calibrates them from what the
user does — see the next entry.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### Several fields near a Finder record read as plausible positions, and three of them are not

*Found on:* System 4.1 and 6.0.8, Macintosh Plus.

*About a search the sender no longer makes:* records are found by their
heap block now (the entry on the Finder's records), and no offset is
searched for.  Kept because what it measured about the bytes is true.

`calibrate` in `fdrop.c` measures where `fdLocation` sits relative
to the name by searching offsets and keeping one that explains what the
catalog says. When the catalog has no position for an icon — which is
usual, because the Finder writes one back when it feels like it — all it
can ask is that the value look like a position **inside the window**. That
is far too weak, and the search runs upwards from −512, so it settles on
the first impostor it meets:

| offset | what is really there | why it passed |
|---|---|---|
| −104 (4.1) | `0x0076, 0x0000` — the record stride, and a zero | inside any window, same for every item |
| −98 (4.1) | half of one field and half of the next | varied between items, and 0,0 for the third |
| −212 (6.0.8) | 94 bytes into the PREVIOUS record | every item read a different in-window position |

The symptom is not a wrong file: it is **every drag refused**, because no
icon is where the sender thinks it is, and — before this was understood —
a `--drop-many` run in which the Finder dragged the icons itself and moved
the user's files onto the desktop.

Three checks fix it, and the third is the one that matters: a candidate
offset must not read the **same position for every item** (two icons
cannot be in one place); a position of **0,0** is the absence of one and
never counts as agreement; and **something must be under the pointer** —
the user has just clicked, so one item in the window has to claim a
32-pixel square out of 512 by 342, and none of the impostors does. The
click arrives at the panel's null event through the trap patch, which is
the only reason the strongest of the three is available at all.

### StillDown answers about the event QUEUE, so a quick click is a press that never happened

*Found on:* System 4.1 and 6.0.8, Macintosh Plus.

`StillDown` is false as soon as a mouse-up is **in the queue**, whether or
not the button is still down under the finger. A trap patch on
`_FindWindow` is therefore not looking at the switch: it is looking at
whether the Finder has got round to asking about the press before the
release arrived. On a busy Finder it has not, and the press is invisible.

The drag sender used that as its gate — reasonably, since `_FindWindow`
is called constantly by event loops handling nothing — and so it saw
**no clicks at all** in about one run in three, silently, without so much
as reaching its own counter. A drag was unaffected, because a drag holds
the button for as long as it takes; only the CLICKS vanished, which is
what the selection calibration is built out of.

`MBTicks` (0x016E) is the tick of the button's last transition -- the
press, while the button is held -- and tells the two apart: a press that
young, with the button already up, is a click rather than an idle loop.
(It moves at the release too, which the entry on `MBTicks`, below,
measures in the ROM.) That is exactly the thing the sender wanted to
record, so catching it there costs a long read.

**And a press arrives about eight times over.** The shift key has to be
read from the keyboard, because `_FindWindow` has no event to read it
from — so the FIRST look at a press is the truest one, and every later
look at the same press can only be wronger. Measured: a shift-click seen
again after the key had been let go was recorded as a plain click; the
moment it produced then contradicted the correct one from a second
earlier, and the panel calibrated the flag exactly right and threw the
answer away in the same breath. One press, noted once.

### The Finder drags the icon too, and moves the file, unless the pointer is put back

*Found on:* System 4.1, Macintosh Plus; the same shape on 6.0.8.

*This is what the sender USED to do, and it no longer takes the mouse at
all* — two entries down. What the Finder does with a press it is handed
back is still true and is kept for that.

`fdrop.c` was a HEAD patch on `_FindWindow`: it took the mouse,
ran the drag, and returned — so the Finder still had the press to handle,
and the comment there said it treated it as a plain click.

It does not, if the press was in one of ITS windows. It finds the icon,
starts its own drag, asks where the mouse is now, and is told the far side
of the screen — so it **moves the file**, out of the folder and onto the
desktop, which is what a drag ending over a window it does not own means
to it. Measured: three items in the window before the gesture and one
after, twice, with the panel showing nothing because the drop had gone to
the queue and the files had gone somewhere else.

It had never shown before because the run that exercises this presses on
an icon the panel is covering, and a press in a system window is one the
Finder does not act on at all. A user with their Finder window in front —
the ordinary way to have one — was losing the file's place.

The fix is two longs: put the pointer back where the press was, in
`RawMouse` (0x082C) and `Mouse` (0x0830), before returning. The Finder's
drag is then a drag of no distance and it treats it as the click the
comment always said it would. The cursor follows, so the icons appear to
fly back, which is what a refused drag looks like everywhere else on this
machine, and the next movement of a real mouse corrects both.

**That move is 4.1's, and 6.0.8's Finder does not make it.** Measured on
6.0.8 with the pointer left where a drop on the open desktop or on the
Trash let go: the file stayed in its window, selected, which is the
click that the entry below describes. So a Finder handed a press with the
button already up cannot be relied on to finish the drag, and putting
the pointer back is not what stops 6.0.8 moving the file. It is what
stops 4.1.

### Neither Finder calls `_DragGrayRgn`: each drags an icon with a routine of its own, and every target it can choose does something

*Found on:* System 4.1 (Finder 5.5) and 6.0.8, read on the host out of
the Finder files on the harness's own boot disks.

Not one `_DragGrayRgn` (`A905`) or `_DragTheRgn` (`A926`) trap word is
in either Finder's code. Each has a routine in `CODE` 2 with
`_DragTheRgn`'s parameters — 4.1 at +0x35E, 6.0.8 at +0x45A — which links
A6, so its caller's `startPt` is at **A6+22** and its action procedure at
**A6+8**, and it makes the same pass on both until the button comes up:

1. `_GetMouse`; the outline follows it, and the result sums the moves.
2. The action procedure (4.1 +0x274, 6.0.8 +0x344), which does not link,
   so it runs with A6 still on that frame: `_GetMouse` again, then
   `_FindWindow` at that point, and the drop target is chosen from the
   answer.
3. `DragHook`, a wait of about two ticks, `_WaitMouseUp`.
4. Once the button is up: one last `_GetMouse` and a `PtInRgn` against
   the slop region, which sets the result to `0x80008000` when it is
   outside — and **nothing in either Finder tests for that value**. The
   target the last action procedure chose is what is acted on.

The loop calls nothing that would hand a desk accessory a null event —
no `SystemTask`, no `GetNextEvent` — so a control panel's idle code
cannot run during a Finder drag, only after it.

What the action procedure makes of `_FindWindow`'s answer:

| part | 4.1 | 6.0.8 |
|---|---|---|
| inDesk | nil | the desktop (the window at A5−258) |
| inSysWindow — a desk accessory, the panel | the desktop | the desktop |
| inContent, a Finder window (kind 16) | that window, or a folder icon under the point | the same |
| anything else, a non-Finder window included | nil | nil |

A nil target is handed to a routine that is applied to **every selected
item** — byte 21 bit 1 of the item's record, which is the selected flag
at name−77 in the heap entry above — with the pointer's position. On
4.1 that is how a drop on the desktop is done. So there is no answer
`_FindWindow` can give that means "nothing": the panel's window is the
desktop, which is what moved files on 4.1, and nil is a desktop drop
too. The one drag a Finder does nothing about is the one in the next
entry.

**The action procedure's call is recognisable to the byte.** Around the
trap, both Finders read `2F04 486F 0006 A92C | 301F 225F` — `move.l
d4,-(sp)`, `pea 6(sp)`, `_FindWindow`, then `move.w (sp)+,d0`, `movea.l
(sp)+,a1`. Every other `_FindWindow` in either Finder differs: the main
event loop pushes D7 and pops into D3 (`2F07 … 361F`), and 6.0.8's
one other call pushes A2 (`2F0A`). `finder_drag_call` in
`fdrop.c` checks those twelve bytes, and that A6+8 holds a
procedure starting within 256 bytes before the call — 0x24 bytes on
4.1, 0x34 on 6.0.8.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A Finder drag that ends where it began moves nothing, and two of its three `_GetMouse` calls are what make it end there

*Found on:* System 4.1 at 512 KB and 6.0.8 at 1 MB, Macintosh Plus,
a control panel built on the library, and read out of Finder 5.5's and
6.1.8's own code.

The Finder acts on two things when its drag routine returns: the target
its action procedure chose on the last pass, and the routine's result,
the OFFSET. A drag whose target is the window the icons came from and
whose offset is zero is a drag the Finder does nothing about -- it puts
the icons back.

**The offset is summed by the outline's `_GetMouse`, and nothing else
touches it.** Each pass reads the pointer, compares it with the last
point, and ADDS the move to the result at A6+30 (`addw d1,30(a6)`,
`addw d4,32(a6)`: Finder 5.5 at CODE 2 +0x3E8, 6.1.8 at +0x4FE). The
last `_GetMouse`, after the release, is tested against the slop region
and can only replace the result with 0x80008000; it never moves it. So
answering only the action procedure's call and the last one, which is
what keeping the outline under the pointer seemed to need, leaves the
offset at the distance the pointer really went, and the Finder then
moves the icon that far inside its own window.

What `fdrop.c` does instead, for a drag recognised by its
call site while the real pointer (`Mouse`, 0x0830) is over the panel:

- the **outline's** call is told the truth, so the Finder's own outline
  follows the pointer over the panel exactly as everywhere else;
- the **action procedure's** call is told the drag's `startPt` (A6+22),
  so `_FindWindow` finds the window the icons are in and that is the
  target;
- the **final** call, if the last action procedure's was answered, is
  told `startPt` as well (inside any slop region, since the press was
  there) and the offset at A6+30 is CLEARED in the frame -- a target this
  code chose is a target whose offset this code owns.

The three are told apart by where they return to, with nothing written
down per Finder. The action procedure's returns between the procedure's
start (A6+8) and its recognised `_FindWindow`. The outline's is LEARNED:
the drag routine's call made just before each action procedure's, the
same site every pass. The final one is any other call from the drag
routine, after that site and within 256 bytes of it. Measured, the
learned sites are exactly the disassembly's: 0x146 and 0x1C0 past the
action procedure on 4.1 (CODE 2 +0x3BA and +0x434), 0x18E and 0x244 on
6.0.8 (+0x4D2 and +0x588) -- `dg21`, `dg22`.

Measured: photographed with the button down and the pointer over the
panel, the Finder's grey outline is under the pointer; after the
release the file is still in its window at its place, the window's item
count unchanged, and the panel has loaded it.

*What it replaced*, the same day: all three calls were answered, so the
drag still ended where it began, but the outline jumped back onto the
icon's home whenever the pointer was over the panel -- which looked to a
user exactly like a refused drag.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A press is a click before it is a drag, because the queue still holds the LAST gesture's mouse-up

*Found on:* System 6.0.8 and 7.0.1, and the guards against it on System
4.1 as well.

*About the sender before 2026-10-19, which took the mouse at the press
and handed it back; the one now in `fdrop.c` takes nothing
and records no clicks. The facts about the Finder below are unchanged.*

The sender recorded quick clicks on purpose — `StillDown` false with
`MBTicks` young is a press the Finder asked about after the release
reached the queue, which is the entry two above. It catches something
else as well. `StillDown` is false whenever a mouse-up is in the queue,
**any** mouse-up, including the one belonging to the gesture before. So
the Finder's first look at a press that is about to become a drag, made
while the previous click's release is still sitting there, reads as a
press with the button already up, and the press is recorded as a click.

**And the press comes back a second time, after the drag.** The sender
hands it to the Finder when it is done, the Finder asks about it again
with the button up and `MBTicks` no younger, and that is a click by the
same description.

**What the Finder does with it is not what it looks like.** It treats it
as a click on the icon that was under the pointer and it does NOT make
that icon the selection: photographed the instant after a two-file drop,
both selected icons are still inverted and every item is still in the
window. So the moment such a press makes is false — it says one item is
selected where the Finder has two — and a false moment agrees with no
offset at all, which empties the candidate set and un-calibrates the
selected-flag byte.

Three further facts, each of which killed a guard written against this:

- **`Button` reports the button up with a finger on it.** Measured at a
  null event, with nobody holding the machine, on 6.0.8 and on 7.0.1.
  The entry above records `MBState` going stale inside a trap patch;
  it is not only there.
- **The Finder's repeat asks carry the pointer's CURRENT position**, not
  the event's, so a press cannot be recognised by its point: once the
  pointer is put back the emulator walks it toward where the host mouse
  really is, and the asks arrive a few pixels apart — every one of them
  still inside the same 32-pixel icon.
- **A press the sender is still tracking is one the Finder has not acted
  on**, so "this click has been overtaken by a later press, drop it" is
  wrong: the heap has not moved on, and dropping it threw away the
  shift-click a two-file drag was about to need.

*What to do:* not any of those. `MBTicks` will identify a gesture — it
is the tick the button last went down, the same across every look at one
press and never the same for the next — and that is worth having to
refuse the press a drag was made of. But the thing that actually holds
is in `sel_moment`: **once the flag is calibrated, stop weighing
moments.** A moment can only narrow the field; after the field is one
offset wide, all a further moment can do is disagree, and the ones that
arrive are the bad ones.

### System 7's catalog `fdLocation` is not the position its Finder draws from, and neither Finder writes it during the session

*Found on:* System 7.0.1 and 6.0.8, Macintosh Plus.

Two facts from one run, both from reading the volume back after shutdown
beside the heap dumps and the diagnostic log.

**The frame.** After the harness dragged two icons, the System 7 Finder's
own records held their positions at name − 82 as 120,78 and 120,180. The
catalog, flushed at shutdown, held 92,62 and 92,164 — the same 28,16
short for both. A System 6 Finder writes the record's own numbers, which
is what `calibrate`'s exact-match rule for placed items was built on. On
System 7 that rule can never match once the positions have been written,
and `file_at`'s catalog hit test is off by 28,16 for a placed icon. The
pointer test against the HEAP position is right on both — the press at
local 84,120 sat six pixels inside the record's square — so the heap is
the frame the Finder draws from and the catalog is a translation of it.
The record holds both, in fact: the catalog's 92,62 sits at name − 86,
four bytes before the drawn 120,78 at name − 82, and 28,16 is what
System 7's icon view adds to turn one into the other.

**And System 7 keeps more than one kind of record.** In the same dump
the Control Panels folder's items — a window the Finder had laid out by
itself and nobody had touched — carry a grid position (0,28; 128,28;
64,80; 128,80 …) at a fixed distance from the START of a
variable-length record, so it sits at a different distance from each
name: −104 before `Memory`, somewhere else before `Labels`. No single
offset before the name reads a position for all of them, which is why
the idle calibration on that folder settled on an impostor (−98) and
why nothing in that window could be dragged by the sender. Which windows
get which kind is not yet known — `PARKED.md`.

**The vector, measured at the moment of the drag rather than worked out
from a flushed catalog afterwards.** The run boots twice: the first boot places the icons and shuts down, so the second one meets a
catalog that says where they are. `frame_probe` in `fdrop.c` then
asks the heap for every offset at which BOTH placed items differ from
what the catalog says by the SAME vector. There is exactly one on each
machine and no second candidate in a thousand bytes: on 7.0.1, name − 82,
vector **v +28, h +16**; on 6.0.8, no such offset at all, and the record
repeats the catalog at name − 96.

**What it costs is not a mis-hit; it is the whole drag** — and what
closes it is one rule: a placed item's value must
differ from the catalog by a vector every placed item agrees on, zero
being the vector these two Finders give. Before that, on 7.0.1 with the
positions written, a press standing still ON the icon calibrated to
nothing (`dg13` 999) and the drag was refused — the Finder drags the icon
itself. `calibrate` requires a placed item's heap value to be exactly what
the catalog says AND some item to be under the pointer, and those are
stated in two different frames: name − 86 satisfies the first and fails
the second, name − 82 the other way about, and with every item in the
window placed there is none left to carry the offset. The same run on
6.0.8 calibrates to −96 and loads the image through the CATALOG route,
which nothing here had ever reached.

**The timing.** In neither of the two runs that found the frame did the
catalog hold a position at the moment of the drag: `dg03`, the mask of
items `file_at` found placed, was 0 on 6.0.8 and on 7.0.1, for icons the
harness had dragged a minute earlier. Both Finders write `fdLocation`
back later — at shutdown, which is what the two boots above are for.
So during a session **the heap is the ordinary route and the catalog is
the exception**, the opposite of how the code read. Which is why a heap
lookup per item inside the drag was not a rare cost: on 7.0.1 a press in
a folder of thirteen unplaced items ran thirteen calibrations and
thirteen sweeps, and the photograph four seconds after the button came
up still showed the sender's mark on the screen with the machine inside
it. One calibration and one sweep per press now (`heap_item_at`).

### A Finder's item record says which folder the item is in

*Found on:* System 6.0.8 and System 4.1, Macintosh Plus.

Twelve bytes before an item's name, four bytes, big-endian: the
**directory ID of the folder that item is in**. On both machines, and
across six heap dumps each, the two items in a subfolder read 18 — the
directory ID that folder was given — and the two at the volume root read
2. Searching every offset within 160 bytes either side of the name for
one that explains all four, on each machine independently, returns
**−12 and nothing else**.

That is a fourth number for the record beside the position (−96), the
selected flag (−77, bit 0x02) and the stride (118), and the two Finders
agree on it as they agree on the others.

*And since 2026-10-19 it is one of several such fields, read from a
record found by its heap block* (the entry on the Finder's records): the
code no longer reads it on its own to name a window's folder.

**What it was worth:** a press no longer needed its window paired with a
folder. The pairing watches the Finder ask the File Manager about a
folder and cannot answer for one read before the panel existed (the entry
below), which is what made a drag out of a folder's own window refuse.
Asking the icon instead has no such hole — and a number read out of
somebody else's heap is checked before it is believed: the volume must
hold a directory of that ID whose NAME is what the window is TITLED, and
the item's own name must be in it.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A Finder does not ask the File Manager about a folder whose window it opens

*Found on:* System 7.0.1, and the selectors on 6.0.8 and 4.1 as well.

*No code depends on this since 2026-10-19:* the `_HFSDispatch` patch and
its ring are gone (the entry on the Finder's records).

`fdrop.c` pairs a window with the folder it is showing by watching
`_HFSDispatch` for `PBGetCatInfo` and keeping the last eight folders in a
ring, then matching one of them against the window's title. It works for
the Control Panels folder and it never worked for a folder made by the
harness, so the two halves were instrumented: how many ring slots held a
folder, how many of those could be NAMED, and the first four characters
of the window's title and of the newest entry.

**The ring is full and healthy and the folder is not in it.** At a press
in a window titled `Inne…`, the ring held **8** folders and **all 8**
could be named — so the name comparison was reached on every one of them
and there was simply nothing to match. The newest was `MM…`, directory 2,
the volume's root.

**What the Finder does instead is nothing.** A bitmask of every
`_HFSDispatch` selector the patch saw over a whole session reads
**0x0300 on 7.0.1 — selectors 9 and 8, `PBGetCatInfo` and
`PBGetFCBInfo`, and no others ever**. Opening the folder's window put not
one call into it. This is the same fact as "opening a VOLUME's window
asks the File Manager nothing" one level down: the Finder reads the
catalog when it MOUNTS the volume, which is before the panel exists, and
a window opened later is drawn from what it already has.

*Two ways out were tried on the machine and neither is one.*
`PBOpenWD` — a Finder might be expected to open a working directory for
a window, and that block puts `ioWDDirID` at offset 48 where
`CInfoPBRec` puts `ioDirID`, so one piece of code would have caught both
— **is never called at all** on 7.0.1, and on 6.0.8 and 4.1 selectors 1
and 7 light up only while the PANEL is the one asking the File Manager
(they appear after a load, never before). And the ring is not being
filled by our own calls: hushing the patch through the whole of the
panel's null-event work left the same entries arriving at the same
cadence, so `MM…`/2 and `Ex…`/21 are the Finder's own housekeeping.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

*And one thing about the gestures, because it cost two runs.* A CLICK
never reaches any of this. `fdrop_track`, the sender of the time, returned
early when `StillDown()` is false — on System 7 the Finder asks which
window a quick press was in only after the mouse-up — and that path
recorded the click for the idle
calibration and nothing else (`step 94`, and `step 92` for a press that
was seen but never moved). So a run that clicks an icon and concludes
anything about pairing has measured its own gesture. The pairing failure
above is the IDLE walk's, which is where it was finally instrumented.

### The Drag Manager's selectors are transcribed, and the toolchain carries them now

The Drag Manager's entire 68K interface is one trap — `_DragDispatch`,
0xABED — with a routine number in D0: `MOVEQ #n,D0` then the trap, which
Apple's header writes as `TWOWORDINLINE(0x70nn, 0xABED)` and multiversal
as `M68K_INLINE(0x70nn, 0xABED)`.

Retro68 had no `Drag.h`, so `fdrop.c` declared the ten calls
it uses itself. Multiversal defines them since 2026-09-26
(`multiversal/defs/DragMgr.yaml`, the patch this repository sent, merged
verbatim), Retro68 picked that up on 2026-10-15, and the declarations
here were deleted: this code now needs a Retro68 that new. Built against
the toolchain's, `fdrop.o` is byte for byte the object the transcribed
declarations produced, so the hardware runs carry over.

**Multiversal is ONE header, and that is why an older toolchain fails
rather than falls back.** Every name in its `CIncludes` is `#pragma once`
and `#include "Multiverse.h"`, so a program that declares a Drag Manager
call itself collides with the toolchain's whether or not it includes
`Drag.h` — ten conflicting-type errors in `fdrop.c`, which is
how the day the toolchain moved announced itself. The same is true of
any program carrying its own copy.

**Do not derive the numbers.** A wrong routine number on a live trap is not
an error code, it is a machine that stops — and there is no way to probe
for the right one. For the record, from Apple's `Drag.h` (Universal
Interfaces, Mac OS 9 / QuickTime 6.0.2), and what the generated header
must agree with: install tracking 0x7001, install receive 0x7002, remove
tracking 0x7003, remove receive 0x7004, count items 0x700E, item
reference 0x700F, flavor size 0x7013, flavor data 0x7014, show hilite
0x701E, hide hilite 0x701F. `dragNotAcceptedErr` is -1857, from
`MacErrors.h`.

### The Finder's `hfs ` flavor is SHORTER than an `HFSFlavor`

An `HFSFlavor` ends in the `FSSpec`'s `Str63`, and the Finder sends only
as many bytes of the name as the name has. On **System 7.5.3** a drag of
`A Test.img` arrives as **28 bytes** where the structure measures 80.
Checking `size < sizeof(HFSFlavor)` therefore refuses every file drag on
that system — and **Mac OS 8.1 sends the whole structure**, so the check
passed there for two milestones and the bug reached 7.5 alone.

What makes it expensive to find is that everything else says the door is
open: `Gestalt('drag')` answers present, both handler installs return
`noErr`, the tracking handler is called *thousands* of times as the
pointer crosses the window, and the drop reaches the receive handler.
Only the flavor's size is wrong, and the visible result is a drag that
flies back — indistinguishable from a Finder that never sent one.

So the minimum is the fixed part **plus the name's length byte**, and
what arrived is checked against what the name claims. Measured with
`FDROP_DIAG`, which now carries four words about the system's door
beside the sender's twenty-four: `dg24`/`dg25` the two install results,
`dg26` tracking calls, `dg27` receives, and `dg28`/`dg29` the item count
and flavor size. On 7.5.3: `dg24 1`, `dg25 1`, `dg26 5915`, `dg27 1`,
`dg28 1001`, `dg29 1028`. (`fdrop.c`)

### `MBTicks` moves at the release as well as the press

*Found on:* the Macintosh Plus ROM, read on the host; and System 4.1 and
6.0.8 in Mini vMac, where a drag's last `_GetMouse` was never seen.

The ROM's VBL task (Plus ROM 0x1B62 to 0x1B90) compares the button's
VIA bit with `MBState` (0x0172) and, on ANY change at least three ticks
after the last one, stores `Ticks` into `MBTicks` (0x016E), the new state
into `MBState` -- 0x80 up, 0 down -- and posts a mouseDown or a mouseUp.
So `MBTicks` is the last transition, not the last press: while the button
is held it is the press, and from the release on it is the release.

What that broke: the sender tells one drag from the next at the same
stack depth by `MBTicks`, and the Finder's drag routine makes its last
`_GetMouse` straight after `_WaitMouseUp` sees the release -- so that
call never matched, and "was the release over the panel" was read from
the last pass BEFORE the release instead. It happened to agree; the new
sender needs the last call itself, to clear the offset (the entry on a
drag that ends where it began), and saw the drop refused with `dg10 10`
until this was found. The last call is now accepted with `MBTicks` later
than the press and `MBState` up, and only the last call.

*How to repeat it:* `m68k-apple-macos-objdump -D -b binary -m m68k:68000
--adjust-vma=0x1b00` over bytes 0x1B00 to 0x1C00 of `vmac/vMac.ROM`.

### Every icon a Finder draws is a heap block of its own, and the block's size says so

*Found on:* Finder 5.5 (System 4.1) and 6.1.8 (System 6.0.8), Macintosh
Plus, from heap dumps; read back on the host.

A file, a folder, a disk and the Trash, in a window or on the desktop,
are each one RELOCATABLE block in the Finder's application heap, and the
block's own header identifies it: the item's name, a Pascal string,
starts 98 bytes into the block's data, and the block's logical size
(physical size, less the 8-byte header, less the size correction in the
tag's low nibble) is exactly **99 + the name's length**. Walking the zone
block by block, by each block's own size, from its first block to
`bkLim`, lands exactly on `bkLim` in every one of 48 dumps, and the size
rule picks out every Finder object and nothing else -- not one false
hit, where a byte sweep for a name with a position before it found about
500 record-shaped places in each dump with the selected bit set.

At fixed distances before the name, the same on both Finders:

| from the name | what | measured |
|---|---|---|
| −96 | where the icon is drawn, v then h | window coordinates in a window, GLOBAL on the desktop |
| −84 | the HANDLE of the object it is drawn in | a folder's or disk's record for a window -- the window's `refCon`, which is what the Finder's action procedure reads -- or the desktop's own record |
| −77 | selected, bit 0x02 | the known one |
| −64 | the item's volume, a vRefNum | −2 for MMTest; 0 for the Trash |
| −60 | the item's fdFlags | 0x0100 in a window, 0x0101 (fOnDesk) on the desktop |
| −16 | where the item came from | 0 in a window, 2 on the desktop: fdPutAway, so far as two runs say |
| −12 | the directory it is in | 1 for a disk, 0 for the Trash |

A disk's record sits in the desktop's record, and so do the Trash and
every desktop file: the desktop is a record too ("DeskTop" on 4.1,
"Desktop" on 6.0.8, at 0,0).

**118 is not a stride.** It is the physical size of the block for a
ten-character name: "MMTest" is 114, "Macintosh" 116, "System Tools"
120. The sender used to walk out from the pressed icon by 118 and count
what it passed against the catalog, which only works while every name in
the folder is ten characters long -- as every test image's is. A
selection of files with names of different lengths would have refused,
safely and always.

What `fdrop.c` does with it (`rec_next`, `resolve_drag`): one
walk of the blocks finds every record; the container handle says which
window or the desktop it is drawn in; the window's own record must carry
the window's title as its name, and the folder it shows comes from the
File Manager, asked about that name; every selected name must be a
visible file the File Manager finds where its record says. That retired
the `_HFSDispatch` patch, the window-to-folder table, the search of a
window's data for a directory ID, the stride walk and the catalog route
for a pressed icon.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### An icon on the desktop is a record like any other, and the catalog does not know it is there

*Found on:* System 4.1 and 6.0.8, Macintosh Plus, a control panel built on the library, with `FDROP_DIAG`.

On these Finders a file on an HFS desktop stays in its volume's root
directory. The Finder's record for it is the record it had in the
window, kept in place: its position becomes global, its fdFlags gain
fOnDesk, and its container becomes the desktop's record.

**The catalog says none of that during the session.** Asked with
PBGetCatInfo, a file the Finder had dragged to the desktop a minute
earlier reads fdFlags **0** -- neither fOnDesk nor even fInited -- on
both systems (`dg23`). The Finder writes Finder information back when it
flushes, at shut down at the latest, which is when the host check of
`--finder-drag` reads fOnDesk set. So a desktop file is recognised from
the Finder's record, and the File Manager is asked only what it can
answer: that this volume's root holds a visible FILE of that name.

Desktop files can be on several volumes at once, each in its own root,
and the record's vRefNum says which. MFS volumes are refused by design:
the sender believes a name because PBGetCatInfo finds it where the record
says, and that is an HFS call. *Since 2026-11-15 they are taken: the entry on Finders 5.4 to 6.1.8 taking a drop from an MFS volume.*

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A Finder selection cannot span the desktop and a window

*Found on:* System 4.1 and 6.0.8, Macintosh Plus, a control panel built
on the library, with heap dumps taken mid-drag.

With two images selected on the desktop and the third in MMTest's
window, a shift-click on the third leaves it selected and ALONE: in the
photograph only its icon is inverted, and in the heap dump only its
record has the selected bit. So a drag carries files from one container
only, and the sender refuses -- rather than half-takes -- a drag while
anything selected is drawn anywhere else on the screen: a disk, the
Trash, an icon in another open window (`step 87`). Records in windows
that have been closed keep their selected bits, so only the desktop's and
the visible windows' containers count.

### The drag routine is the same in every Finder from 5.4 to 6.1.8

*Found on:* the Finder file of every System 3.0 to 7.1 release that could
be fetched, read statically on the host; `fdrop.c`'s
recognition depends on it.

The action procedure's call site, `2F04 486F 0006 A92C 301F 225F`, was
searched for in every CODE resource, and the drag routine that takes the
procedure was disassembled:

| Finder | System | signature | action proc → return | drag routine | layout (LINK; proc A6+8, startPt A6+22, result A6+30; pops 22) | `_GetMouse` in the routine | outline → final |
|---|---|---|---|---|---|---|---|
| 5.1 | 3.0 | 1 | 36 | ROM `_DragGrayRgn` | — | — | — |
| 5.2 | 3.1.1 | 1 | 36 | ROM `_DragGrayRgn` | — | — | — |
| 5.3 | 3.2 | 1 | 36 | ROM `_DragGrayRgn` | — | — | — |
| 5.4 | 4.0 | 1 | 36 | CODE 2 +0x35E | yes | 2: outline sums, final tests slop | 122 |
| 5.5 | 4.1, 3.3 | 1 | 36 | CODE 2 +0x35E | yes | 2, the same | 122 |
| 6.0 | 4.2, 4.3 | 1 | 50 | CODE 2 +0x3A6 | yes | 2, the same | 182 |
| 6.1 | 6.0 to 6.0.3 | 1 | 52 | CODE 2 +0x3AA | yes | 2, the same | 182 |
| 6.1.4 to 6.1.8 | 6.0.4 to 6.0.8, 6.0.8L | 1 | 52 | CODE 2 +0x45A | yes | 2, the same | 182 |
| 7.0 | 7.0, 7.0.1 | 0 | — | its own loop, C arguments, one `_GetMouse` | no | — | — |
| 7.1 | 7.1 | 0 | — | the same as 7.0 | no | — | — |
| 7.5.5 | 7.5.3 | 0 | — | not examined | — | — | — |

So **one recognition covers every Finder the sender is for**, with no
per-version table, and recognises nothing on System 7, where a stock 7.0
or 7.1 therefore has no drag onto the panel, and the panel's own Open
command is the way in, as
`PARKED.md` says. Finders 5.1 to 5.3 match the call site but drag
through the ROM's `_DragGrayRgn`, whose frame is not this one; they are
below the floor and nothing has been run on them. *(Since overtaken: the
ROM path takes a drop on every System from 1.1 to 3.2 -- the entries
from "System 1.1 takes a drop" on.)* Six Finder 6.1.x
versions share one CODE byte for byte, and 6.1 in four Systems another.
No Finder contains `_DragTheRgn` (A926); only 5.1 to 5.3 contain
`_DragGrayRgn` (A905), as a real call.

Every other `_FindWindow` site differs from the signature in at least
three words but one: from 6.1.4, CODE 2 +0x290 reads `2F0A 486F 0006 A92C
301F 225F`, the click handler that starts the drag, which differs only
in the first word (`move.l a2` for `move.l d4`). The twelve bytes are
exact for that reason, and must stay exact.

*How to repeat it:* the Systems are on archive.org (the "Apple Mac OS
System x Finder y" 800K sets, and `mac-system-software-6.0.1`,
`-6.0.8`, `-7.0.1`, `-3.4`, `-4.0`); take the Finder out with hfsutils'
`hcopy -m` under a private `HOME`, read the resource fork, and search
every CODE resource for the twelve bytes. System 7's CODE is
`dcmp`-compressed and needs a decompressor first.

*Run on:* every site in the table from 5.5 to 6.1.8, in an emulated
Plus -- "The recognition the survey read is the one that runs", below.

### Under MultiFinder a trap patch belongs to the layer that called SetTrapAddress, wherever its code is

*Found on:* System 6.0.8 under MultiFinder, Macintosh Plus at 4 MB,
a control panel built on the library, with the emulated Mac's RAM read
back twice, and MultiFinder's CODE 1 read on the host.

A control panel chosen from the Apple menu opens in the DA Handler's
layer, and its code resource is loaded into the SYSTEM heap (a
relocatable block at 0x29B0A). Installed with `SetToolTrapAddress`, the
two patches were in the Toolbox dispatch table -- `_FindWindow` at
0x0C00 + 4 × $12C, `_GetMouse` at 0x0C00 + 4 × $172 -- while the DA
Handler was in front, and the moment the Finder was, both held the
values the patches had saved as their old ones (0xAB94 and the ROM's
0x410FA0). The sender saw no Finder drag at all: no `step` in the log,
and a drag released over the panel did what it does with no sender.

**The address does not enter into it.** MultiFinder's patch on
`_SetTrapAddress` (CODE 1 +0x3F7E) records every call made from an
application layer in that layer's table and swaps the entry with the
layer, with special cases only for two traps and two application
signatures, none of them these. A patch in the system heap is therefore
no more global than one in the application's partition.

**A store straight into the table is.** The same two entries written
directly, with no `SetTrapAddress`, still pointed into the panel with the
Finder in front. `fdrop.c` patches that way on every system,
having first asked the machine which table it has: the one, of 0x0C00
(512 entries, the 128K ROM) and 0x0E00 (1024), whose slots for both traps
hold exactly what `GetToolTrapAddress` says.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### Under MultiFinder each layer has a window list of its own, and a background window's visRgn is clipped by every layer in front

*Found on:* System 6.0.8 under MultiFinder, Macintosh Plus at 4 MB, the
same two RAM images.

`WindowList` (0x09D6) is switched with the layer. With the DA Handler in
front it held the Control Panel alone; with the Finder in front it held
MMTest's window and the Clipboard and nothing else -- so a walk of the
window list from inside the Finder's drag cannot find the panel, and
"is the pointer over the panel" answered no every time.

What MultiFinder does keep current is every window's `visRgn`: with
MMTest's window over the top half of the panel and the Finder in front,
the panel's `visRgn` was local (200,0)-(252,320), exactly the strip of
its content below MMTest's window. So from the Finder's context the
panel is under the pointer when no window of the Finder's is and the
point is in the panel's `visRgn` -- which is `over_panel` in
`fdrop.c`, with `dg40` saying which way it answered.

`ApplZone`, `HeapEnd` and `CurStackBase` are switched too, and during the
Finder's drag they are the Finder's (0x3C4434, 0x3E7134, 0x3EB13C on
that machine), so the heap walk and `finder_drag_call`'s bounds need
nothing new. The panel's window record and its regions are in the system
heap, where the panel's layer put them, and are as readable from the
Finder's layer as from its own.

*How to repeat it:* the run in the entry above; the window record is
`FDROP_WIN`, the visRgn handle at +24 of it, and the port's
`portBits.bounds` at +8 turns a global point into its coordinates.

### Under MultiFinder a Finder drag released over another layer's window moves nothing, and the panel takes it without being brought forward

*Found on:* System 6.0.8 under MultiFinder, Macintosh Plus,
a control panel built with no sender at 4 MB, and with this one at 4 MB
and at 1 MB.

**With no sender** the Finder's outline follows the pointer over the
panel and, after the release, the file is still in its window, selected,
at its place: off the host after the shutdown, fdFlags 0x0100 (no
`fOnDesk`) and fdLocation 80,56, where `place` had put it. A window of
another layer is neither the desktop nor a Finder window to the drag's
action procedure. This is the baseline the sender has to leave alone,
and it does: the drop it takes ends where it began exactly as under the
plain Finder (`step 105`, `dg10 11`, `dg20 101`), and the learned call
sites are the plain 6.0.8 Finder's, 398 and 580 (`dg21`, `dg22`).

**The panel takes the drop while the Finder is still in front.** Six
seconds after the release, with MMTest's window still over it, the
panel's row was loaded and its Unload and Mount buttons were live: the
DA Handler's layer gets null events in the background, so `fdrop_idle`
runs and counts the drop in without the panel being clicked.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### The recognition the survey read is the one that runs, on Finders 6.0, 6.1 and 6.1.4 to 6.1.7

*Found on:* Systems 4.2, 4.3, 6.0, 6.0.2, 6.0.3, 6.0.4, 6.0.5 and 6.0.7,
each off its own System Tools floppy, Macintosh Plus at 1 MB, a control
panel built on the library, ordinary build; MultiFinder on 4.2, 6.0.2
and 6.0.5.

The entry above reads the drag routine of every Finder from 5.4 to 6.1.8
and finds one shape, at three different places in CODE 2. Before this
the shape had run only at two of them, 5.5's and 6.1.8's. It now has run
at all three -- Finder 6.0 (+0x3A6), 6.1 (+0x3AA) and 6.1.4 to 6.1.7
(+0x45A) -- and a drop out of a window, two files and then a third out of
one, two files off the desktop, and a Finder drag to the Trash and to
the desktop with the panel open all do what they do on 4.1 and 6.0.8,
with nothing in the sender changed and no table per version.
`FDROP_DIAG` says the sender learned the drag's call sites on its own
(`dg21`/`dg22`, relative to the action procedure): 348/530 on Finder 6.0,
350/532 on 6.1, 398/580 on 6.1.5 -- 6.1.8's exactly, as one CODE byte
for byte should be -- and outline to final is 182 on all three, the
table's figure, where 5.5's is 122 (326/448).

Under MultiFinder 1.0 (System 4.2) and MultiFinder 6.0 (6.0.2, and 6.0.5
with MultiFinder copied off its Utilities 1 floppy) the panel opens in
the DA Handler's layer as it does under 6.0.8's, and the drop is taken
with MMTest's window still in front, before the panel is clicked. So the
two MultiFinder entries above hold from MultiFinder's first release.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A Finder lays out the icons nobody placed in catalog order, folders first, and lets long names overlap

*Found on:* System 4.1 and 6.0.8, Macintosh Plus, a control panel built
on the library, from the photographs.

A window whose items have never been placed (fdLocation 0,0, which is
everything hfsutils writes) is laid out by the Finder in a row 62 pixels
apart, whatever the names: a one-character name, a nineteen and a
thirty-one were drawn at the same three places as three ten-character
ones, and the thirty-one-character label was drawn straight over its
neighbour's. A FOLDER takes the first place, ahead of every file,
though its name sorts after them ("Z Folder"), and the files move one
place on. Nothing in the sender depends on this; a harness clicking at
fixed places does.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A Finder record is the same for a name of 1 character and of 31, and for one past ASCII

*Found on:* System 4.1 and 6.0.8, Macintosh Plus, the same runs.

The rule two entries up -- a record's block is 99 + the name's length,
the name at 98 -- was measured on ten-character names only, and every
run before this had one length. With "A", "B Ångström café.img" (19
bytes, three of them past 0x7F) and a 31-character name with spaces and
a bullet, all three selected in one window and dragged together, the
panel got three rows with the three names, on both Finders; and the same
three off the desktop. The bytes past 0x7F are Mac Roman and reach the
panel as they are on disk: the sender compares and copies names as bytes
and asks the File Manager about them, and nothing in it reads them as
characters.

*How to repeat it:* the command in the entry above, and the same with
`--drop-desktop`.

### The Finder opens a second volume's first window ten pixels right of the first one's and twenty down

*Found on:* System 4.1 and 6.0.8, Macintosh Plus, a control panel built
on the library, with desktop files of two volumes.

Two volumes whose windows have never been opened: the first opens with
its close box at (30, 52) on the screen, the second, opened after the
first was closed, at (37, 72). A harness that opens the second at the
first's coordinates drags from empty window.

And the drag that follows -- one desktop file from each volume, both
selected, one gesture -- loads two rows, and BOTH mount, which is what
says each row named the volume its file is on: `resolve_drag` takes the
volume from each desktop record, not from the press.

*How to repeat it:* that command; `--panel` a diagnostic build adds
nothing, the claim is two `loaded` and two `mount` in the log.

### A desktop selection holding a disk, the Trash or a folder is refused whole, and each by the test the code says

*Found on:* System 4.1 and 6.0.8, Macintosh Plus, a control panel built
on the library, with `FDROP_DIAG`.

An image on the desktop, shift-clicked together with MMTest's disk, the
Trash, or a folder dragged out of MMTest's window, and the image dragged
onto the panel: the Finder's outline carries both icons, both go back,
no row appears, and the image is still on the desktop off the host
afterwards. `step 188` on all six runs, and the diagnostic words say
which test refused: for the disk `dg05 1`, `dg07 1` (its record's
directory is 1, not the root it would have to be in); for the folder
`dg04` "Z ", `dg07 4` (the File Manager says it is a folder); for the
Trash no `dg04` at all, because its record's volume is 0 and the walk
stops at `hfs_volume` before `takeable` is asked.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### Every MultiFinder from 1.0 to 6.0.7's takes the drop, and the refusals and name lengths hold on Finders 6.0, 6.1 and 6.1.5

*Found on:* Systems 4.3, 6.0, 6.0.3, 6.0.4 and 6.0.7 under their own
MultiFinder at 1 MB; refusals under MultiFinder on 6.0.8 and 4.2; names,
two volumes and refusals without it on 4.3, 6.0.3 and 6.0.5. Macintosh
Plus, emulated, a shipped control panel built on the library, and
`FDROP_DIAG` for the refusals.

The MultiFinder entries above were measured on 6.0.8's MultiFinder and
then on 4.2's (1.0), 6.0.2's and 6.0.5's. Each of the other five releases'
own MultiFinder behaves the same: the panel opens in the DA Handler's
layer, a drop out of a window and two off the desktop are taken with the
Finder's layer still in front, and the Finder's drags to the Trash and
the desktop are its own. 4.3's MultiFinder has 4.2's size and
date; 6.0's and 6.0.3's are on their own System Tools disk, 6.0.4's and
6.0.7's on Utilities 1.

The three refusals two entries up -- a disk, the Trash, a folder selected
with the image -- give the same diagnostic words on Finder 6.0, 6.1 and
6.1.5 as on 5.5 and 6.1.8 (`dg05 1`/`dg07 1`, no `dg04`, `dg07 4`), and
under MultiFinder too, where only `dg40` differs: `2`, the panel found by
its visible region, against `1` by the window list. And names of 1, 19
and 31 bytes, three in one gesture, give three rows with their own names
on those three Finders too, from a window and from the desktop, so the
record rule is the same there.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A file on an MFS volume's desktop is refused at the volume, and the Finder marks it fOnDesk as on HFS

*Found on:* System 4.1 (Finder 5.5, 512 KB) and 6.0.8 (6.1.8, 1 MB),
Macintosh Plus, emulated; a control panel built on the library, with
`FDROP_DIAG`.

A third disk, MMMfs, is a 400K MFS volume written by
`make_fixtures.build_mfs` holding one file, `udif_raw_800k.dmg`'s bytes,
which the panel mounts from an HFS volume. Both Finders mount it, open
its window, and move the file to the desktop with their own drag. That
file, selected alone and dragged onto the panel, is refused whole: the
outline is over the panel, the Finder puts the icon back, no row
appears, and the log has no `loaded`. It says `step 188` with no `dg04`.
That is the Trash's pattern, refused at `hfs_volume` before `takeable` is
asked, which is the test the code says an MFS volume meets. *Since 2026-11-15 they are taken: the entry on Finders 5.4 to 6.1.8 taking a drop from an MFS volume.* The call
sites are each Finder's own, 326/448 and 398/580.

*And the Finder's mark for "on the desktop" is the HFS one on MFS too.*
Read off MMMfs's own file directory after the shutdown, the file has
fdFlags 0x0101 (fOnDesk and inited), fdLocation v=210 h=456 (the desktop
spot the drag left it at), and **fdFldr 0**, not the -2 that Inside
Macintosh I gives for the desktop. A host check written from the book
failed both runs on a file that had not moved, so the harness asks
fOnDesk. A `--desk-refuse mfs` on 4.1 at 512 KB also says `open -32 4`
where the same machine with an HFS third disk says five; that was seen
twice and has not been isolated.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### Below System 4.1 the sender's anchor is not run: the System 3 Finders drag through the ROM and never call `_FindWindow`, Finder 1.1g never calls it at all, and the 64K ROM has neither trap table

*Found on:* System 1.1 (Finder 1.1g) and System 3.0 (Finder 5.1) on an
emulated 512Ke — the Plus's 128K ROM, 512 KB — and System 3.0 on an
emulated 512K with the 64K ROM; 2026-10-22, with the shell desk
accessory `examples/da` built `FDROP_DIAG`, a file dragged out
of the disk's window onto its window. The same accessory on System
6.0.8 under MultiFinder at 1 MB, off the desktop, took the drop —
`drops: 1`, volume -1, directory 2, one drag recognised to step 105 —
which is what says the accessory is a sound host before the floor is
believed.

**What happened on each tier**, read off the accessory's window:

| System | ROM | `_GetMouse` patched | `_FindWindow` patch entered | drags recognised | the file |
|---|---|---|---|---|---|
| 1.1, Finder 1.1g | 128K | yes | **never** (`dg51` 0) | 0 | moved to the desktop by the Finder |
| 3.0, Finder 5.1 | 128K | yes | **once**, at the click (`dg51` 1); A6 was `0x40000`, not a frame (`dg44`) | 0 | the same |
| 3.0, Finder 5.1 | 64K | **no**: neither table agrees | — | 0 | the same |

**But the static disassembly says the recognisable shape IS in the
System 3 drag path, so the live counters above understate it and the
floor is a tuning gap rather than an absence.** Finder 5.1's CODE 3 icon
drag, read off the resource fork on the host: at `0x1E1A` it calls the
ROM's `_DragGrayRgn` (`A905`) passing an action procedure at `0x1E66`,
and that procedure calls `_FindWindow` (`A92C`) at `0x1E88` preceded and
followed by the sender's exact twelve bytes — `2F04 486F 0006 A92C 301F
225F` — the same signature `finder_drag_call` matches on every Finder
from 5.4 up. So the icon drag DOES reach a recognisable `_FindWindow`
through the dispatcher; what the live run showed is that the sender did
not arm on it, and the one clue is a frame test that failed with A6 =
`0x40000` (`dg44`). Whether that was this call or the click's was not
isolated, and isolating it is the next run, not a redesign: the shape
the recognition needs is present.

**The reason the earlier table read "never" is that the harness drag and
the counters disagreed with the code: a result that could not have come
out otherwise is not a result.** The disassembly is the ground truth here; the shell's live
`dg` words are a second instrument that needs its own calibration on a
System this old (the drag may take a different CODE 3 path — the rubber
band selection at `0x1D7E` uses `_StillDown`/`_GetMouse` directly, the
icon drag at `0x1E1A` uses `_DragGrayRgn`), and the two must be made to
agree before any number off the window is a result.

**The ROM routine is the Finder's own by another name.** `_DragTheRgn`
does `LINK A6,#-20`, reads `startPt` at A6+22, the region at A6+26 and
clears the result at A6+30 — the layout the sender already knows, with
the action procedure at A6+8 — and calls `_GetMouse` through the
dispatcher at `$11EB4` and `$11F2E`, **122 bytes apart, which is
Finder 5.4's "outline to final" figure to the byte**: Finders 5.4 and
5.5 carry a copy of this routine. So on the 128K ROM the `_GetMouse`
patch IS reached during a System 3 drag, the frame has the shape the
sender wants, and the question is only why `fdrop_look` did not arm
`FDROP_DRAGFP` from the `_FindWindow` at `0x1E88`. The likeliest answer is
the frame-bounds test in `finder_drag_call` (`a6 < heap_start()` and the
stack-base comparison), which was tuned on Finders 5.5 to 6.1.8 and has
never been checked against a 512 KB machine's heap and stack geometry —
a System 3 sender is more likely a loosened bounds test and a
recalibrated record layout than a second recognition. That is the next
run (`OPEN_ITEMS.md` §1).

**Finder 1.1g has no anchor of either kind.** Zero entries into the
`_FindWindow` patch across a click and a drag: it does not call that
trap through the dispatcher at all. Its drag is a survey of its own
code before anything is designed.

**The 64K ROM is a third case.** Its dispatcher has one table at
`$0400`, and `tool_table` knows the Plus's 512 Toolbox entries at
`$0C00` and the SE's 1024 at `$0E00`; neither agrees, so the sender
installs nothing there, by design, and the accessory says `patched:
no`. That is the right answer for a machine nobody has measured, and it
is also why System 3.0 on a 512K says nothing about the Finder.

*Not reached:* the MFS check (`OPEN_ITEMS.md` §1), because no
drag got as far as a file on any of the three.

*How to repeat it:* `sh examples/da/build.sh -DFDROP_DIAG=1`
gives `build/examples/da/FinderDrop.drvr`, 6.9 KB; merged into the System
file of a 400K MFS floppy as `DRVR` 19 (1.1) or 20 (3.0), it is in the
Apple menu. The driver that does the merging, boots Mini vMac and makes
the drag is outside this repository until the library has a harness of
its own. `dg41` to `dg51` are the counts
above and are in `finder_drag_call` and `FDROP_FW`.

### The library runs in an application, and the Drag Manager door works there

*Found on:* System 7.5, 4 MB, emulated; 2026-10-22, with the shell
application `examples/app`, launched from the Finder and a file
dragged onto its window. The application is the third host form and the
first program of ours to run the library in an application rather than a
code resource.

**It builds, which is the first thing the form had to prove.** An
application has an A5 world; the library does not, because its trap
patches run in the Finder's A5 world. So `build.sh` compiles the app
code normally and the two library halves `-mpcrel`, and the two
addressing worlds coexist in one CODE resource — the library objects
linked first so their PC-relative reaches stay short, and compiled
without `-ffunction-sections` so each half is one contiguous blob. 22 KB.

**It runs and takes a drop.** Launched on System 7.5, the window drew
`application host. door: Drag Manager drops: 0`; a file dragged from the
Finder onto it gave `drops: 1`, `vFFFF d00000002 DragMe`, one black
square. That is the Drag Manager door, `fdrop_install` registering its
handlers on the application's window exactly as it does on a cdev's.

**The sender door in an application** is the entry on MultiFinder
below. *The boot was not the obstacle this paragraph said it was:* the
emulator's own System 6.0.8 disk boots straight to MultiFinder through
its own boot blocks, and copying the application onto a copy of it is
the whole of the setup.

*How to repeat the 7.5 run:* `sh examples/app/build.sh` gives
`build/examples/app/FinderDropApp.bin`; put it and a plain file at the
root of a System 7.5 boot volume with the Finder as shell, double-click
the app, drag the file onto its window. The driver that does this is
outside the repository.

### The 64K ROM: the sender installs there now, through a trampoline, and the DA fits in 128 KB

*Found and built on:* System 1.1, Finder 1.1g, on an emulated Macintosh
128K (64K ROM, 128 KB RAM) in Mini vMac; 2026-10-22, with the shell desk
accessory `examples/da`. This is the floor of the floor, the oldest
and smallest machine the library is meant to reach.

**The DA fits and runs in 128 KB.** `FinderDrop.drvr` (6.4 KB) merged into
System 1.1's System file opens its window and draws, with room to spare —
feasibility that was in doubt and is no longer. Apple's own Puzzle DA
opens there too, so a windowed desk accessory on 128 KB is ordinary.

**The 64K ROM dispatches traps completely unlike the Plus, and that is
why `tool_table` returned 0 and nothing installed.** Read out of the
ROM's own A-line handler (0x401018 on this ROM): a SINGLE table of 512
**word** offsets at **0x0400**, not the Plus's longword tables at 0x0C00
and 0x0E00. The dispatcher indexes `trap & 0x1FF` (OS and Toolbox share
one table), reads a word, doubles it, and adds the ROM base [0x02AE] when
the double does not carry or the RAM base [0x02B2] when it does — bit 15
of the word is the RAM flag. So an entry is `(addr - base) / 2`, and a
longword written there corrupts two entries. `_GetTrapAddress` ($A146)
works and reports the real handler; `GetToolTrapAddress` ($A746) aliases
to it through the 9-bit mask, which is why `tool_table` did not bomb, only
failed to match.

**The patch cannot point at the code resource directly: 64 KB of reach,
and the resource loads far past it.** Measured: the RAM base [0x02B2] is
0x000B00, and the DA's code loads at 0x01222E — 71470 bytes out, where
the word offset reaches only 0x10000. Writing the patch address through
the ROM's own `_SetTrapAddress` encoded a truncated offset and the next
`_GetMouse` jumped into garbage (ID=03). So the sender installs a
**trampoline**: a 12-byte block holding two `jmp.l` to the real patches,
with the table words pointing at it through the ROM's own encoding — no
`_SetTrapAddress`, only its layout. The block has to land in the 64 KB
the offset reaches, and neither heap promises it: `NewPtrSys` landed at
0x001716 on one run and 0x019800 (out of reach) on another, so the install
tries the system heap, then the application heap, and keeps whichever is
in range (a desk accessory has the Finder's heap under it, low enough on
the runs measured), refusing if neither fits. The old table WORDS are saved raw and
restored; the patch chains to the old handler by its decoded ADDRESS, so
those are kept apart (`FW_OLD`/`GM_OLD` the addresses, `ROM64_FWW`/`GMW`
the words). `rom64_present`, `rom64_encode` and `rom64_slot` in
`fdrop.c`; the trampoline is freed at remove, in application context.
*Since 2026-11-08 every ROM has one* and the table points at it, so that
a trap patched again after ours can be left a pass-through rather than
a pointer into an unloaded resource (`patch_install`). The run is "Two
hosts at once, the first closed first", at the end of this file; on the
64K ROM itself, closing with something patched on top has not been run.

**Proven live:** with the patch in, a `_GetMouse` the DA made itself
ran through `FDROP_GM` (`dg52` climbed). The Plus path is unchanged — the 64K branch is only reached when no longword table is
found, and `ci.sh` is green.

**The mouse CAN be held — the ROM drag polls through `_GetMouse`.** The
ROM's `DragGrayRgn` (0x40C3D4) and `DragTheRgn` (0x40D55A, frame `link
#-28`) call `_GetMouse` at five sites (0x40C47E, 0x40C4F8, 0x40C684,
0x40CAE0, 0x40D5C8), and with the patch in, a `_GetMouse` fires through
it — confirmed by `dg52` and by painting the screen from inside the
patch. So the sender's hold has a trap to work on here.

**The recognition was built after this and works** — the next entry,
on System 1.1. One correction to this entry first: the `dg52` readings
that said the patch never fired during a Finder drag came from the
shell's DISPLAY, which held what was copied at its last redraw. Read
straight out of RAM (`ramdump.py`, below), the counter stood at tens of
thousands. The patch was live throughout.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.


**Where the trampoline can land, measured.** The reach's base, `[0x02B2]`,
is 0xB00 on every 64K ROM run, and 0xB00 is where the system heap
begins: it ends at 0x4DF4 on the 128K and at 0xCAF4 on the 512K, so the
whole system heap lies inside the 64 KB the word offset reaches and a
block `NewPtrSys` hands back always encodes. The trampoline was in the
system heap on every run (0x0C36 to 0x1D6C, Systems 1.1 to 3.2). What can
still fail is a system heap without twelve bytes free; then the
application heap is tried, which begins at 0x4E00 or 0xCB00 and holds
its non-relocatable blocks at the bottom, and where that too is out of
reach the sender installs nothing. That last refusal has not been run.

### System 1.1 takes a drop: Finder 1.1g's drag is followed from the ROM end

*Found and built on:* System 1.1, Finder 1.1g, on an emulated Macintosh
128K (64K ROM, 128 KB) and on a 512Ke (Plus ROM, 512 KB); 2026-10-22,
with the shell desk accessory. Read off the machine by snapshotting its
RAM from the host mid-drag (below), which is how every number here was
measured rather than inferred.

**Finder 1.1g has no drag routine of its own.** Its icon drag (CODE 2
0x3200) waits out a movement threshold, builds the outline and calls
the ROM's `_DragGrayRgn` with an action procedure at CODE 2 0x3300. The
ROM's `DragTheRgn` (64K ROM 0x40C430, Plus ROM 0x411E66, the same
routine) has the frame the Finders from 5.4 copied — action procedure at
A6+8, start point at +22, result at +30 — and calls `_GetMouse` in its
loop (returning to +0x50) and once after the release (+0xCA).

**Three things make it unlike the copied routines, and decide the design:**

- The action procedure loads A6 from a slot in its own CODE segment
  (0x31F4) with the Finder routine's frame, so its `_GetMouse` runs on a
  different frame from the ROM's. The `_GetMouse` patch's gate compares
  A6, so it now also lets through a call returning inside `DragTheRgn`,
  and any call while such a drag is being followed (`FDROP_ROMMODE`);
  `rom_answer` in C sorts them by return address.
- The action procedure has no `_FindWindow` to recognise, so the drag is
  armed by the loop's `_GetMouse` instead, with the frame checked: on
  the stack, called from the Finder's own code, an action procedure in
  the application heap, the button down, and `CurApName` "Finder".
  `DragWindow` and every other caller of the ROM routine fail that.
- The drop target is the action procedure's hit test (CODE 2 0x39A2),
  which takes the point its `_GetMouse` returned and nothing else. When
  `_FindWindow` says a desk accessory's window, it substitutes the
  desktop — which is why a drop on the accessory used to move the file
  to the desktop. Answering that `_GetMouse` with the start point makes
  the source window the target; answering the final one with the start
  point and a zero result makes the move a move of nothing. These are
  the copied routines' answers, given from the ROM end.

**Finder 1.1g's item record**, as data offsets in a relocatable block
that ends exactly where the name does:

| offset | field |
|---|---|
| 0x02 | where the icon is drawn, v then h: global on the desktop |
| 0x0B | bit 1: selected (the one byte that moved between an unselected and a selected snapshot) |
| 0x0C | the file's folder: −2, MFS's `fDesktop`, on the desktop; 0 in the disk's window |
| 0x0E | the container's handle, long — equal to the Finder window's `refCon` |
| 0x16 | the next item's handle (DragMe → Finder → System → nil) |
| 0x22 | the volume's `vRefNum` |
| 0x38 | the MFS file number |
| 0x3E | the name |

**What makes it safe is the File Manager, not the layout.** Each selected
record of the window the press was in is believed only when
`_GetFileInfo` finds that name on that volume with that file number;
one that fails refuses the whole drop, and the Finder is told to put
the icons back. The drop arrives as `vRefNum`, directory 0 and the name.

**Runs** (`dragcheck.py` reading each snapshot after the release):

| run | result |
|---|---|
| System 1.1, 64K ROM, 128 KB, onto the shell, ×3 with the shipping build | taken each time, step 5; the icon stays in its window |
| the same, two files selected | `drops: 2`, DragMe and DragTwo |
| released inside the Finder window, never over the shell | armed, never resolved, nothing taken; the Finder moved the icon |
| crossed the shell, let go back in the window | resolved while over it, `ACT` 0 after, nothing committed; the Finder moved the icon |
| System 1.1, Plus ROM, 512 KB, onto the shell | taken |
| System 3.0, 64K ROM | armed, refused whole (Finder 5.1's records, below) |
| System 3.0, Plus ROM | left alone: System 3 patches `DragTheRgn` in RAM, the byte check refuses it |
| System 6.0.8, the shell; a control panel built on the library, on 4.1 and 6.0.8 | unchanged: taken |

**Finder 5.1 keeps a third layout** — the name at data+0x5E, between
1.1g's 0x3E and 5.5's 0x62; the entry on System 3.0 below has it — and
it HAS the old anchor: its action
procedure calls `_FindWindow` with the twelve-byte signature on
`DragTheRgn`'s frame. So `fdrop_look` stands aside while a ROM drag is
followed; with both recognisers live, each undid what the other decided.

**Three wrong readings on the way, each of which looked like a result:**

- *The harness pressed on the icon's NAME.* In Finder 1.1g, pressing the
  name of a selected icon starts editing it; the press ran `TEClick`'s
  tracking loop (ROM 0x40F5C2) and no drag ever began. Found by
  counting `_GetMouse` calls per return address, not by the screenshot.
- *The shell's display was stale* — the entry above.
- *+0x24 held 0x003E on the 128K*, exactly the name's offset, and read as
  a self-describing field. On the 512Ke it holds 0x00BE. The second
  machine is what caught it; the test is now the block ending where the
  name does, which the later Finders' reader already uses.

*How to repeat it:* `sh examples/da/build.sh`, then (outside the
repository) `dragshell.py --system 1.1
--machine 128K --item 70,170 --move 300,150 --at 90,74 --target
290,250 --dump`, and `dragcheck.py <run>`. `ramdump.py` finds Mini
vMac's RAM in its process by `MemTop` and `ROMBase` and must run in the
harness that spawned it, Yama allowing only an ancestor to read it.
`FDROP_DIAG` adds `dg53` to `dg59` (what install did) and `dg60` to
`dg71` (`_GetMouse` call sites and counts).

### On Finder 1.1g's desktop the press finds the desktop, and a disk selected with a file refuses it

*Found and built on:* System 1.1, Finder 1.1g, on an emulated Macintosh
128K (64K ROM, 128 KB) and a 512Ke (Plus ROM); 2026-10-22, with the
shell desk accessory and RAM snapshots of a file moved out of the disk's
window onto the desktop and then dragged from there.

**A desktop item is a record like a window's, in another container.**
DragMe on the desktop is the same 69-byte record with `0x61B4` at +0x0E
where the window's items have `0x619C`, −2 at +0x0C where they have 0,
and its icon's top left at +0x02 in GLOBAL coordinates (v 272, h 34 for
an icon drawn at 34,272). `0x619C` is the disk's record, the window's
`refCon`; its own container is `0x61B4`, the desktop's record, whose
+0x0E holds `0x004008B4` — not a handle, so the desktop is the root.

**What finds the desktop's container is the press.** No window answers
for the desktop, so `resolve_rom` asks the records: the SELECTED one
whose 32-pixel square holds the start point, which `DragTheRgn` keeps in
global coordinates too, and whose container is the ROOT — the one
record whose own container is not a handle in the heap. An item in a
window is in a disk's or a folder's record, whose container is a handle,
and an unselected one fails the first test, so nothing else can claim
the press. From there it is the window path: every selected record in
that container, each believed only once `_GetFileInfo` finds it by its
file number. *The folder word is not the test*, though 1.1g writes −2
there: Finder 5.1 leaves it 0 for a file on its desktop (the entry on
System 3.0).

**A disk selected with the file refuses the drag whole** (step 67). The
disk's record is in the desktop's container too and is no file record —
it does not end at a name — and its +0x0B went from `0x80` to `0x82`
when it was shift-clicked, the same selected bit. Selected and not a
file is a refusal on the desktop, where only the disk and the Trash can
be that; in a window such a block is passed over as before, which is the
verified window path unchanged.

| run (System 1.1) | result |
|---|---|
| 128K, a file off the desktop onto the shell, shipping build | taken, `drops: 1`, step 5; the icon stays where it was |
| 128K, two files off the desktop | `drops: 2`, DragMe and DragTwo; both stay |
| 128K, a file and the disk selected | refused whole, step 67; nothing moves, `drops: 0` |
| 128K, desktop to desktop, never over the shell | armed, never resolved; the Finder moved it 10 right and 45 up, as the pointer went |
| 128K, desktop across the shell, let go on the desktop | resolved while over it, `ACT` 0 after; the Finder moved it the same |
| 128K, out of the disk's window onto the shell | taken, as before |
| 512Ke, Plus ROM, a file off the desktop | taken |

*How to repeat it:* as for the entry above, with `--desk 50,290` (the
file dragged from the window to there first, then from there onto the
shell); `--two 40,300 --pre 106,90,40,300 --pre 170,88,90,260 --at
74,244` for two files; `--two 468,45` for the disk; `--target 60,245`
and `--via 290,250 --target 60,245` for the controls.

### System 3.0 takes a drop: Finder 5.1's records, and System 3's patch in front of the ROM routine

*Found and built on:* System 3.0, Finder 5.1, on an emulated Macintosh
512K (64K ROM) and a 512Ke (Plus ROM); 2026-10-22, with the shell desk
accessory and RAM snapshots, as for System 1.1.

**Finder 5.1's record is 1.1g's with three fields moved**, read off a
snapshot before and after DragMe was clicked (one byte changed):

| field | Finder 1.1g | Finder 5.1 |
|---|---|---|
| where the icon is drawn, v then h | 0x02 | 0x02 |
| folder | 0x0C | 0x0C (0 on the desktop too) |
| container's handle, long | 0x0E | 0x0E |
| selected, bit 1 | 0x0B | **0x15** |
| next item's handle | 0x16 | 0x16 |
| `vRefNum` | 0x22 | 0x22 |
| MFS file number, long | 0x38 | **0x3C** |
| name | 0x3E | **0x5E** |

So one reader serves both, and the block says which it is: a record is
a block that ends exactly where a name at 0x3E, or at 0x5E, does. The
desktop is a named record in 5.1 ("DeskTop", whose own container is
`0x0007A2FC`) where 1.1g's has no name, and the disk and the Trash are
named records in it — so a disk selected with a file passes as a record
and is refused whole by the File Manager check (step 66), where 1.1g's
nameless disk is refused by step 67.

**On the Plus ROM System 3 puts a patch in front of `DragTheRgn`**, and
that is what kept this path off there. The trap names it in the system
heap (0x7AF2 on the run), and it is exactly:

    LINK    A6,#-24
    LEA     wrapper,A0
    MOVE.L  8(A6),-24(A6)      ; the caller's action procedure, kept
    MOVE.L  A0,8(A6)           ; the patch's wrapper in its place
    JMP     $411E6A            ; the ROM routine, past its own LINK

So the frame is the ROM routine's with one long more, the ROM's body
runs unchanged, and every `_GetMouse` still returns inside it. Install
now recognises those eight words, follows the `JMP` back four bytes to a
routine that must pass the same byte check as before, and keeps the
wrapper's address; while the action slot holds the wrapper, the caller's
procedure is read from A6−24. The wrapper calls it through a ROM helper,
so its `_GetMouse` is still recognised by returning inside it.

**System 6.0.8 has the same patch** (`DragTheRgn` at 0x8D3C, `dg53` 5
where it was 1), so the ROM path is live there now, where it was off.
Its Finders drag through routines of their own and their records match
neither layout, so nothing a run can see changed: the shell took a file
off 6.0.8's desktop, and a control panel built on the library took a
dropped file on 4.1 and 6.0.8.

| run (System 3.0) | result |
|---|---|
| 512K, 64K ROM, a file out of the window | taken, `drops: 1`; the icon stays in the window |
| the same, two files | `drops: 2` |
| the same, a file off the desktop | taken |
| the same, a desktop file and the disk | refused whole, step 66; nothing moves |
| the same, released in the window, and across the shell back into it | the Finder's: moved 140 right and 8 down, as the pointer went |
| 512Ke, Plus ROM, a file out of the window | taken, through the patch |
| the same, a file off the desktop | taken |
| the same, released in the window, and across the shell back into it | the Finder's |
| System 1.1 again, desktop and disk refusal on the 128K | unchanged: taken, and step 67 |

*How to repeat it:* `dragshell.py --system 3.0 --machine 512K --item
70,187 --move 300,150 --at 94,76 --target 300,310 --dump`; on the 512Ke
the shell is `--item 70,123`. `--desk 50,290`, `--two 174,100`, `--two
468,45` with `--desk`, `--target 250,100` and `--via 300,310 --target
250,100` for the rest. `heaprecs.py <ram.bin> 0x5E` lists Finder 5.1's
records.

### Every System from 1.1 to 3.2 takes a drop: two record layouts cover every Finder that drags through the ROM

*Found on:* Apple's 400K MFS disks of System 2.0 (two: one with Finder
1.1, one with Finder 4.1), System 2.1 (Finder 5.0, the HD 20 Startup
disk), System 3.1 (Finder 5.2) and System 3.2 (Finder 5.3), from
Macintosh Garden's System 2.x and early System 3 archives; emulated
Macintosh 128K and 512K (64K ROM) and 512Ke (Plus ROM); 2026-10-22, with
the shell desk accessory, and with no change to the library.

**Every one of them drags through the ROM's `DragTheRgn` and keeps one
of the two layouts already read**:

| System | Finder | layout | Plus ROM `DragTheRgn` |
|---|---|---|---|
| 1.1 | 1.1g | 1.1g's, name at 0x3E | the ROM's |
| 2.0 | 1.1 | 1.1g's | the ROM's |
| 2.0 | 4.1 | 1.1g's | the ROM's |
| 2.1 | 5.0 | 5.1's, name at 0x5E | the ROM's |
| 3.0 | 5.1 | 5.1's | behind System 3's patch |
| 3.1 | 5.2 | 5.1's | behind the patch, the same eight instructions |
| 3.2 | 5.3 | 5.1's | behind the patch |

So the layout changed once, between Finder 4.1 and 5.0, and the patch
arrived with System 3.0 and was still there in 6.0.8. Finder 4.1 here is
System 2.0's, an MFS Finder, and is no relation of System 4.1.

| run | result |
|---|---|
| 2.0, Finder 1.1: 128K, 512K and 512Ke, out of the window | taken |
| 2.0, Finder 1.1: 512K, off the desktop | taken |
| 2.0, Finder 4.1: 128K, 512K and 512Ke, out of the window | taken |
| 2.0, Finder 4.1: 512K, off the desktop | taken |
| 2.0, Finder 4.1: 512K, released in the window | the Finder's: moved 189 right and 14 down, as the pointer went |
| 2.1: 512K and 512Ke, out of the window and off the desktop | taken |
| 3.1: 512K, out of the window and off the desktop; 512Ke, out of the window | taken |
| 3.2: 512K, out of the window and off the desktop; 512Ke, both | taken |
| 3.2: 512K, across the shell and back into the window | the Finder's: moved 140 right |

*Harness:* a Plus ROM sorts the Apple menu alphabetically and the 64K
ROM does not, which is why the shell's item is at a
different height on each machine for one System: on the 512Ke it is
`--item 70,75` (2.0), `70,123` (2.1, 3.0, 3.2), `70,91` (3.1); on the
128K and 512K `70,75` (2.0), `70,187` (2.1, 3.0), `70,107` (3.1),
`70,155` (3.2). The icon is `--at 90,74` under Finder 1.1, `45,90` under
Finder 4.1 and `94,84` under 5.0 to 5.3.

### An application under MultiFinder takes a drop through the sender, and without MultiFinder the sender installs nothing

*Found and built on:* System 6.0.8 under MultiFinder on an emulated
Macintosh Plus at 4 MB and at 1 MB, and the same System with the Finder
alone; 2026-10-22, with the shell application `examples/app`.

**It takes the drop.** The application launched from the disk's window,
DragMe on the desktop: pressing the file brings the Finder forward, as
MultiFinder does, so its disk window covers the middle of the
application's window and the drop is made on the part still showing.
`drops: 1`, volume −1, directory 2 (the desktop), DragMe, and the icon
back where it was. Nothing in the sender changed for it: a background
layer's window is found by its region, as the desk accessory's is in the
DA Handler's layer, and the drop is queued from the Finder's context.

**It is drained in the background.** The black square that marks a drop
was drawn before the application was clicked forward: `finderdrop_app.r`'s
`SIZE` says `canBackground`, so `fdrop_idle` runs on the null events it is
given while the Finder is in front. An application without that bit
would show its drops when the user switched back to it.

**Without MultiFinder the sender installs nothing**, and that is now a
rule rather than a hope. With the Finder alone, launching an application
quits the Finder, so nothing can be dragged onto the application's
window; the patches went in anyway, harmless while it ran and pointing
into a reused heap if it ever left without calling `fdrop_remove`.
`fdrop_install` now asks whether a Finder can be running beside its host:
under MultiFinder always — `_OSDispatch` ($A88F) is implemented only
while it runs, before System 7 — and otherwise only when the current
application (`CurApName`) is the shell (`FinderName`, $02E0), which is
the desk accessory or control panel opened over the Finder. `FinderName`
reads "Finder" in the snapshots of every System from 1.1 to 3.2. The
application without MultiFinder now shows `patched: no`.

| run | result |
|---|---|
| 6.0.8, MultiFinder, 4 MB: desktop file onto the visible part of the app's window | taken, `patched: yes`, `drops: 1` |
| the same at 1 MB | taken |
| the same, let go on the desktop below the app's window | the Finder's: moved there, `drops: 0` |
| the same, let go over the Finder's disk window in front of the app's | the Finder's: moved into the window, nothing taken |
| 6.0.8, the Finder alone, the app launched | `patched: no` |
| after the change: the desk accessory on 1.1 (128K), 3.0 (512Ke) and 6.0.8; a control panel built on the library, on 4.1 and 6.0.8 | unchanged: taken |

*How to repeat it:* `sh examples/app/build.sh`, then (outside the
repository) `dragshellapp.py --system 6 --multifinder --app-icon 346,206
--me-desk 458,94 --drag-from 474,110 --target 230,280 --front 300,290`,
with `--mem 1M` for the smaller machine; `dragshellapp.py --system 6
--app-icon 237,105` for the Finder alone.

### The shell control panel takes a drop through both doors, and System 6 lists no cdev without a bundle

*Found and built on:* System 4.1 at 512 KB with the Finder alone, System
6.0.8 at 1 MB and System 7.5.3 at 4 MB, on an emulated Macintosh Plus;
2026-10-22, with `examples/cdev`, installed in the System Folder
(System 7's Control Panels folder) and driven by a control-panel
harness that drags a file out of a Finder window onto whichever panel it
is given.

| System | door | the panel said |
|---|---|---|
| 4.1, 512 KB | sender | `patched: yes`, `drops: 1`, volume −2, directory 2, A Test.img |
| 6.0.8, 1 MB | sender | the same |
| 7.5.3, 4 MB | Drag Manager | `patched: no`, `drops: 1`, the same file |

**A cdev file with no `BNDL` is not a Control Panel file to System 6.**
The first build carried the code, `nrct`, `mach`, `DITL` and `ICN#` at
−4064 and nothing else, and the Control Panel said *There are no Control
Panel files in the System Folder* — the file's type was `cdev` and its
MacBinary header checked. With a `BNDL`, an `FREF` naming `cdev` and the
signature resource added together, it is listed and opens. Inside
Macintosh V lists those among a cdev's resources; which of the three the
Control Panel was looking for was not isolated.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### System 3.4 takes a drop through the Finder's own drag routine

*Found on:* Apple's AppleShare Workstation 2.0.1 disk for the 512Ke, an
800K HFS disk with System 3.4 and Finder 6.1, from Macintosh Garden;
emulated 512Ke; 2026-10-22, with the shell desk accessory merged into its
System file as `DRVR` 21 (20 is Access Privileges there), the disk
booted as Apple shipped it.

Finder 6.1 drags with the routine of its own the sender was built on, so
this is the later path and not the ROM's: recognised to step 4 and
committed at step 5, `ROMMODE` 0 throughout. A file dragged out of the
disk's window onto the accessory: `patched: yes`, `drops: 1`, volume −1,
directory 2, DragMe, the icon left in its window. With the entries
above, every System from 1.1 to 3.4 but 3.3 has now taken a drop.

*How to repeat it:* `dragshell.py --system 3.4 --machine 512Ke --wait 50
--item 70,139 --move 300,150 --at 230,84 --target 350,300 --dump`.

### Folders and disks are handed over, as the Drag Manager hands them

*Built and run on:* System 6.0.8 (Finder 6.1.8) on an emulated Plus at
1 MB, and Systems 1.1 and 3.0 on the 64K ROM; 2026-10-23, with the
example desk accessory.

**The rule changed: the library hands over everything the Finder is
carrying that the File Manager can name**, and the program decides what
to do with it. Until now FinderDrop's own path refused a folder in the
selection, and a disk, whole, while the Drag Manager's path had always
passed both through; a program saw a different library depending on the
System. Now both arrive the way an FSSpec names them:

| the item | volume | directory | name |
|---|---|---|---|
| a file or a folder | its volume | the directory it is in | its name |
| a disk | its volume | 1, the parent of every root | the volume's name |
| a file of an MFS volume | its volume | 0 | its name |

The **Trash** was still refused whole here, its record having neither a
volume nor a parent, and a **folder of an MFS volume** too; both arrive
now (the entries below).

**How each Finder's disk is recognised and checked:**

- *Finders 5.4 to 6.1.8:* a disk's record has parent 1 and its vRefNum.
  It is believed only once `_GetVInfo` names that volume with the name
  the icon carries, on any file system, since nothing here reads a
  catalog. A folder's record passes the test a file's always did, the
  catalog finding it where the record says, now without a refusal for
  being a directory.
- *Finder 1.1g:* the disk's record has no name, so it is found by its
  kind word, 4 at +0x00, with the vRefNum at +0x22 and its global
  position at +0x02 like an item's. The name is the File Manager's. The
  desktop anchor also accepts a press on the disk.
- *Finder 5.1:* the disk's record is an ordinary named record, kind 4,
  and its name must be the volume's.

| run | the program received |
|---|---|
| 6.0.8, the disk icon | volume −1, directory 1, "Macintosh" |
| 6.0.8, a folder on the desktop | directory 2, "DragFolder" |
| 6.0.8, disk, folder and file in one drag | all three |
| 6.0.8, a file and the Trash | refused whole, `drops: 0`, the icons put back |
| 1.1, 128K, the disk icon | volume −1, directory 1, "Early" |
| 1.1, 128K, a desktop file and the disk | both |
| 3.0, 512K, the disk icon; a desktop file and the disk | the volume; both |
| 3.0, 512K: a new folder of the MFS disk, alone, and with DragMe | refused whole, step 66; both icons stay in the window |

*How to repeat it:* `dragshell.py --system 6 --item 70,171 --at 458,94
--target 224,170 --front 224,120 --folder DragFolder@458,150 --from
474,110 --shift 474,166 --shift 472,45`; `--system 1.1 --machine 128K
--item 70,170 --move 300,150 --at 456,29 --target 290,250` for the disk.

### The three examples, and the Drag Manager's disks and folders

*Run on:* emulated Macintoshes, 2026-10-23, with `examples/`
built as they are committed — no diagnostic words — and read off the
screen.

| example | System | dropped | listed |
|---|---|---|---|
| desk accessory | 1.1, 128K | DragMe from the window | file, directory 0 |
| desk accessory | 1.1, 128K | the disk | disk "Early", directory 1 |
| desk accessory | 3.0, 512Ke | DragMe from the desktop and the disk | both |
| desk accessory | 6.0.8, 1 MB | disk, folder and DragMe | disk, folder, file |
| control panel | 4.1, 512 KB; 6.0.8 | A Test.img from a window | file, "Following the Finder's drag" |
| control panel | 7.5.3 | the same | file, "Through the Drag Manager" |
| application | 6.0.8, MultiFinder, 4 MB | DragMe from the desktop | file, drained in the background |
| application | 7.5.3 | DragMe; the disk; the System Folder | file; disk, directory 1; folder, directory 2 |

**The Drag Manager hands a disk and a folder over exactly as FinderDrop
now does** — the disk as directory 1 and its name, the folder as the
directory it is in and its name — which the last row measures rather
than assumes: the two doors agree.

**The application takes its time through `GetNextEvent` and
`SystemTask`**, which every System has; under MultiFinder that is also
how a `canBackground` application is given its background null events,
and the drop was taken before the application was brought forward.

*How to repeat it:* in a control panel built on the library, driven by
that program's own emulator harness, which is not part of FinderDrop.

### A folder of an MFS disk arrives as the files in it

*Built and run on:* System 1.1 (Finder 1.1g) on an emulated 128K and
System 3.0 (Finder 5.1) on a 512K, both 64K ROM; 2026-10-23, with the
example desk accessory; decided by the maintainer that day.

An MFS volume has no directories: a folder is the Finder's arrangement,
kept in its Desktop file, and the File Manager has no name for it, so
there is nothing to hand over as a folder. What the user meant by
dragging it is its files, and those the File Manager does name. So a
selected folder arrives as every file whose record's container is the
folder's handle, each a drop of its own with directory 0 and each
believed only once `_GetFileInfo` finds it by its file number; a folder
inside it is opened in turn, thirty-two deep at most (step 69 past that).
*Changed 2026-11-08:* this said eight, and the eight was the size of a
stack of folders still to open, siblings included, so a folder holding
nine folders was refused at a depth of one. The walk now goes up from
each file through its containers' handles, one pass over the heap, and
the limit is the depth (`rom_folder`, `rec_of`). The first two rows of
the table were run again on the new walk, and the last two rows are new:

**A folder's record**, read off snapshots: kind 8 at +0x00, the kind the
desktop's root record has too. Finder 5.1's is an ordinary named record
(name at 0x5E, selected at 0x15). Finder 1.1g's carries no name — the
stock "Empty Folder" is a 158-byte block whose name is in the Desktop
file — and is selected at 0x0B like its files. Neither name is needed:
the folder's handle is what its files point at.

| run | received |
|---|---|
| 3.0: a new folder holding DragMe | DragMe, directory 0; the folder stays in its window |
| 3.0: a folder holding a folder holding DragMe | DragMe |
| 3.0: an empty folder | nothing: refused, step 64, the folder put back |
| 1.1: Finder 1.1g's "Empty Folder" holding DragMe | DragMe, directory 0 |
| 3.0, before 2026-11-08: F1 holding DragMe and nine folders | nothing: refused |
| 3.0, the walk up: the same | DragMe, directory 0 |

*How to repeat it:* `dragshell.py --system 3.0 --machine 512K --item
70,187 --move 300,150 --pick 55,10,55,28 --pre 110,104,172,104 --at
156,88 --target 300,310` (`--pick` is File ▸ New Folder; a second one
makes "Copy of Empty Folder" at 236,104); on 1.1, `--system 1.1
--machine 128K --item 70,170 --move 300,150 --pre 42,90,180,95 --pre
42,90,240,95 --pre 106,90,42,92 --at 26,76 --target 290,250`, the first
two spreading the three icons the window stacks at its first slot.

### The Trash arrives as Apple's Drag Manager hands it over, or is left out

*Measured and built on:* System 7.5.3 (the Drag Manager's answer), 6.0.8
(Finder 6.1.8), 3.0 (Finder 5.1) and 1.1 (Finder 1.1g); 2026-10-23, with
the example application and desk accessory. Decided by the maintainer:
match Apple, and where there is nothing to match, leave the Trash out
rather than refuse the drag.

**Apple's answer, measured:** the Trash dragged onto the example
application on System 7.5.3 arrived through the Drag Manager as the
folder "Trash" in directory 2 of the startup disk — the real folder the
Finder keeps trashed items in.

**System 6 keeps no such folder.** A folder dragged to the Trash on
6.0.8 was still at the root of the disk five seconds later, its date
changed — the Finder wrote it and moved nothing; trashed items stay
where they are until the Trash is emptied. So on Finders 5.4 to 6.1.8
the Trash (the desktop record with volume 0 and parent 0) is looked up
as a folder of its own name at the root of the startup disk (BootDrive,
`$0210`), handed over if there is one, and otherwise **left out: the
rest of the drag still arrives.** A drag of nothing but such a Trash is
refused (step 85).

**On an MFS disk the Trash is a container like a folder** — kind 12 at
+0x00, nameless on Finder 1.1g and named on 5.1 — and arrives as the
files in it, exactly as an MFS folder does. Empty, it adds nothing.

| run | received |
|---|---|
| 7.5.3, the Trash, through the Drag Manager | folder "Trash", startup disk, directory 2 |
| 6.0.8, an empty Trash and DragMe | DragMe |
| 6.0.8, the Trash alone, holding a folder | nothing (no Trash folder): refused, put back |
| 6.0.8, the Trash alone, empty | nothing: refused, put back |
| 3.0 and 1.1, the Trash holding DragMe | DragMe, directory 0 |
| 3.0, an empty Trash and DragMe | DragMe |

*How to repeat it:* `dragshellapp.py --system 75 --app-icon 110,195
--drag-from 462,298 --target 240,200`; `dragshell.py --system 6 --item
70,171 --at 458,94 --target 224,170 --front 224,120 --folder
DragFolder@458,150 --from 474,110 --shift 472,318`, or `--pre
474,166,472,318 --from 472,300` with `--snap-disk FILE` to read the disk
afterwards; on 3.0 and 1.1, `--move 224,200 --target 200,300 --pre
<DragMe>,472,318 --at 456,284` (the Trash's square is v 284 to 316).

### Two hosts at once, the first closed first; and a name longer than 31

*Run on:* System 6.0.8 under MultiFinder, emulated Plus at 1 MB, and
System 3.0 on a 512K (64K ROM); 2026-11-08, with the example desk
accessory, each run made twice -- built from the commit before these
fixes, and after.

**Two copies of the library.** The example accessory merged into the
System as DRVR 22 `FinderDrop` and DRVR 23 `FinderDropB`, each with its
own state. A opened, then B (B's patches on top of A's), B moved, A
closed, DragMe dragged onto B, B closed, then a plain Finder drag and the
disk opened:

| build | B after A closed | after B closed |
|---|---|---|
| before | Received: 0 -- A had put the table back to the ROM | the desktop's icons and menus gone: the table pointed into A's freed code |
| the trampoline | DragMe, directory 2 | the Finder drag moved DragMe; the disk opened |

**A name longer than 31.** An MFS floppy built with a file named "A file
whose name is forty-one bytes long" (Finder 5.1 shows it whole), dragged
onto the accessory:

| build | received |
|---|---|
| before | "A file whose name is forty-one" -- 31 bytes, and not that file's name |
| kNameMax | nothing: refused whole (step 70) |

*How to repeat it:* `dragtwice.py [--mm ROOT]` (System 6), `dragname.py`
and `dragnine.py --f1 172,110 --dragme 110,110 --others
"236,110;300,110;364,110;46,172;110,172;172,172;236,172;300,172;364,172"`
(System 3.0), all beside `dragshell.py`; `--mm` builds the accessory
from another checkout, a worktree of the older commit for the "before".
The Drag Manager's half of the name check (System 7 with an MFS volume)
has not been run.

**Both open, each taking its own** (6.0.8 under MultiFinder, the same two
copies, `dragtwice.py --both`). A opened, then B in front of it and
moved to the lower left; four drags of DragMe with both open, then B
closed and one more:

| drag | B (front) | A (behind) |
|---|---|---|
| onto B | 1 | 0 |
| onto A, where it shows above B | 1 | 1 |
| across A, released on B | 2 | 1 |
| across B, released on A | 2 | 2 |
| B closed; onto A | -- | 3 |

Each drop went to the window it was released over and to no other; A
closed after, a Finder drag and the disk's window were the Finder's.

**The rest of that day's sweep, with the trampoline and the shell test
below:** System 3.0 on the 64K ROM, a drop, the accessory closed, then a
Finder drag and the disk opened (`dragname.py --name DragMe --close`);
System 7.5.3 through the Drag Manager, DragMe received
(`dragshellapp.py --system 75 --app-icon 110,195 --drag-from 36,195
--target 240,200`); and the program this library is built into, at its
512 KB System 4.1 floor, a drop that loaded and mounted, and with its
panel open a drag to the desktop and one to the Trash, both the
Finder's own.

### A desk accessory's window is what tells it from an application named Finder

*Read off the code, 2026-11-08, by a program that runs as the shell.*
Systems 1.1 to 3.2 launch the shell by the name in FinderName, so an
application renamed Finder and started that way matched the name test
in `fdrop_finder_alongside` exactly as a desk accessory over the real
Finder does -- and patched the traps with no Finder to drag from. Without
MultiFinder a matching name now counts only when the window the library
serves has a negative windowKind, a driver's reference number: a desk
accessory's, and a control panel's, which is the Control Panel's own.
`fdrop_can_drop` takes that window for the same reason. The two hosts
measured above set it before `fdrop_install` and still took their
drops.

*And the application, run by the program that found it* (2026-11-08,
at a863860): emulated 128K, System 1.1 on the 64K ROM, that program
renamed Finder and started as the shell, asking `fdrop_can_drop` with
its own window before loading the segment the library is in. Its batch
run took and processed 94 of 101 files exactly as it did before it
carried this library, which says the segment was not loaded. That is
indirect: nothing in the run shows the answer itself. Not run: two
copies of that program at once, or it beside another host.

### The queue is the program's

*Built 2026-11-08, decided with the maintainer; run that day on
emulated Macintoshes with the three examples and the program this
library is built into.*

The drop queue was storage in `fdrop.S`, sized by a number every host
had to define in a configuration file. It is now memory the program
hands `fdrop_install` -- `capacity` entries of `FDROP_ENTRY_BYTES`, 42
since names stopped at 31 -- with an optional `room` function the Drag
Manager's receive handler calls, in the program's own context, when a
drag needs more; `fdrop_requeue` hands it a bigger queue. The sender
cannot do that: it finds the items inside the Finder's drag, where
nothing can be allocated, so without a Drag Manager the capacity given
at install is what a drag gets. `fdrop_refused` tells a program that a
drag released over its window was refused for want of room, which a
one-file host needs in order to say why the icons went back.

At 32 entries the library's two objects went from 10342 bytes (8474 +
1868, the queue being 1602 of the second) to 9120 (8838 + 282); the
queue's bytes are now the host's to place and to size.

| run | received |
|---|---|
| 6.0.8, the desk accessory built `-DkDrops=1`, the disk, a folder and DragMe | nothing; "Refused, too many: 1"; the three icons where they were |
| 6.0.8, the same accessory with 8, the same three | all three |
| 7.5.3, the application built `-DkQueueMost=1` ("room for 1"), DragMe and the System Folder | both, through `room` |
| 3.0 on the 64K ROM, a folder holding DragMe | DragMe, directory 0 |
| 6.0.8, two copies open, each with its own queue | as "Two hosts at once": each drop to the window it was released over |
| the program's panel at its 512 KB System 4.1 floor | a drop that loaded and mounted |

*How to repeat it:* `DRAGSHELL_DEFS=-DkDrops=1 dragshell.py --system 6
--item 70,171 --at 458,94 --target 224,170 --front 224,120 --folder
DragFolder@458,150 --from 474,110 --shift 474,166 --shift 472,45`, and
without the variable for eight; `DRAGSHELL_DEFS=-DkQueueMost=1
dragshellapp.py --system 75 --app-icon 110,195 --drag-from 36,195
--shift 40,100 --target 240,265` -- the target is the strip of the
application's window below the Finder's, which comes to the front on
the first click. The 3.0 folder run is "A folder of an MFS disk" above,
with `--pre 40,104,172,104` on a floppy that shows its System file.

*And in another host, the same day at the same commit:* a program that
carries this library in an application and two desk accessories ran its
own suite -- drops onto both accessories on 6.0.8 and 7.5, its
application's 7.5 folder drop (80 files, the queue sized from
`MaxBlock` and grown through `room`), four drops under MultiFinder on
6.0.8 and a 100-file run on System 6, and on a 128K the application
started as the shell, which again did not load the library. A drag
refused for room was not among its runs; the one-entry accessory above
is the evidence for that path.

### Two different hosts open at once

*Run on:* System 6.0.8, the single Finder, emulated Plus at 1 MB;
2026-11-10, with the library at 81b4c88 in both. One disk, built the way
the second host's own harness builds its System 6 disk: that host's
one-file desk accessory merged into the System, the program this
library is built into as the only control panel, two 800K images and a
blank zip on the desktop.

| order | both open | the first-opened closed, the other dropped on alone | both closed |
|---|---|---|---|
| control panel first | the image a row on the panel; the zip loaded in the accessory and stamped | the accessory took the zip again ("Nothing to change": it was stamped) | a Finder drag, and the disk opened |
| accessory first | the same | the panel took the second image, a new row | the same |

Each drop went to the window it was released over, with the other
window open and overlapping it; the zip read back stamped from the
disk's catalog after both runs.

*How to repeat it:* `pairdrop.py --order cp-first|lite-first --cp-item
70,107 --lite-title 85,50 --cp-title 380,40`, beside `dragshell.py`; it
uses the second host's checkout for its accessory, disk builder and
samples. MultiFinder, where both open in the DA Handler's layer, has
not been run with the two.

*And onto the one behind* (`--unfocused`, the same disk and Finder):
neither window brought forward first. With the accessory in front, the
image released on the part of the panel showing below it became a row;
with the Control Panel in front, the zip released on the strip of the
accessory showing to its left loaded there. Without MultiFinder the
sender's test is the window list's, front to back, as `_FindWindow`'s
is: what counts is the window visible at the release, not the active
one. Under MultiFinder that was already every drop -- the Finder comes
forward when its icon is pressed -- and it is the content's visible
region that counts there, not the frame.

### Names to 63, and a trampoline MultiFinder can hide a patch from

*Built 2026-11-10 after a second review of the library; run that day.*

**Names.** The limit of 31 refused what Standard File would have handed
the program: an MFS volume's names go to 63, the File Manager opens such
a file by its whole name, and nothing downstream needs fewer. So a drop
now carries a Str63 (`kNameMax` 63, entries of 74 bytes,
`fdrop_take_drop`'s buffer 64 bytes), and only a name past 63 -- which no
Finder writes -- is refused. The review found the record test of Finders
5.x and 6.x stopping at 31, which made a longer-named MFS item INVISIBLE
in a selection rather than refused; it now goes to 63, and the catalog
lookup that copies such a name (`cat_entry`) has a 64-byte buffer and
refuses anything longer before copying.

**The trampoline under MultiFinder.** MultiFinder takes an application's
`_SetTrapAddress` patch out of the table while that application is
switched out, so a patch made on top of ours from another layer cannot
be seen from the table when the host closes. Under MultiFinder the block
is now never freed -- twelve bytes of system heap per close -- and a
kept block is made a pass-through whole, both jumps, so neither can
lead into the unloaded host. Not run with a third-party patch; that is
the case it guards.

| run | result |
|---|---|
| 3.0, the 41-byte MFS name ("A file whose name is forty-one bytes long") | received whole, directory 0 |
| 6.0.8 single Finder, the disk, a folder and DragMe | all three |
| 6.0.8 MultiFinder, two copies open, drops on each and across | as before; both closed, a Finder drag and the disk opened |
| 6.0.8 MultiFinder, the first-opened closed first, a drop on the other | taken; both closed, the Finder unharmed |
| the program's panel at its 512 KB System 4.1 floor | a drop that loaded and mounted |

### A trampoline is reused, so they do not pile up

*Built and run 2026-11-10, on emulated 6.0.8.*

The rule above -- under MultiFinder a trampoline is never freed -- left
one block of system heap per close, every program's copy alike: fifty
visits to a panel, fifty blocks. So a block now carries, after its two
jumps, a signature, its own address and the two addresses it chains to
(28 bytes), and `fdrop_install` looks for one before it makes one: from
the table's `_GetMouse` entry down the chain, past blocks in use, to the
first that is a pass-through. Re-aiming that one puts the new copy at
that point of a chain that already runs through it, so nothing below is
skipped and nothing can loop. Under MultiFinder a closed copy's block
also stays where it is in the chain rather than being taken out of the
table, so it cannot become an orphan nothing can find. The blocks a
session holds are as many as copies were ever open at once.

| run | result |
|---|---|
| MultiFinder, one copy opened, closed, opened again | the same block both times (0x28172, the diagnostic build's words 54/55) |
| MultiFinder, a second copy opened while the first is open | a new block (0x27E5A); both closed, the second reopened: 0x27E5A again |
| single Finder, A then B over it, A closed (its block kept under B's), A reopened | A walked past B's block to its own; a drop on each, each took its own; both closed, the Finder's drags unharmed |

**A limit of the harness, not the library:** under MultiFinder the
second of these accessories could not be opened alongside the first
once one had been opened and closed in that boot -- at 1 MB and at 4
MB, and with the build from before this change too, so it is the DA
Handler's heap and not this code; the walk past a block in use was
measured under the single Finder instead.

*How to repeat it:* `DRAGSHELL_DEFS=-DFDROP_DIAG=1 dragtwice.py --reuse`
for the addresses, and `dragtwice.py --single --deep --item-a 70,155
--item-b 70,171` for the walk, beside `dragshell.py`.


### An application launched over an open accessory: the system closes it first

*Run 2026-11-11, emulated Macintosh, the single Finder, the library at
e93351a; each run again with the examples as this entry's commit leaves
them (their lines capped), with the same result.* The question came out of a review: the example accessory does
not set `dNeedGoodbye`, and Inside Macintosh says a driver in the
application heap is lost when that heap is reinitialized. Without
MultiFinder the Finder's launch of an application reinitializes it, so
if nothing closed the accessory, `fdrop_remove` would never run and the
trampoline would jump into freed memory at the next `_GetMouse`. On
Systems 1.1 to 3.2 an accessory is the only host there is, so leaving
one open and launching something would be the ordinary way to use it.

The sequence: the accessory opened over the Finder, a file dropped on
it, an application double-clicked in the Finder with the accessory
still open, the mouse moved and clicked in the application, Quit, the
mouse moved and an icon dragged in the Finder again. RAM was read out
of the emulator at each stage, and what it said:

| stage | 6.0.8, Plus at 1 MB | 1.1, 512Ke |
|---|---|---|
| accessory open | `_FindWindow`/`_GetMouse` at the trampoline, its jumps into the accessory's code in the application heap, the code's bytes intact; DCE open | the same |
| application running (Font/DA Mover 4.1; Font Mover) | the table holds the entries the trampoline had saved (00AB94 and ROM 410FA0; ROM 4120F6 and 410FA0), the trampoline's block is a pass-through and FREE; the DCE closed, its storage and window zero | the same |
| back in the Finder, moved and dragged | the same entries; nothing stopped | the same |

The storage and window going to zero, the table going back and the
block being freed are each written by the accessory's own Close and
nothing else (`da_close` clears the two fields; `patch_remove`, without
MultiFinder and with the table still ours, does the rest). So the close ran, with the code still
there, before the application started: **the system closes an open
desk accessory before the launch, and no goodbye handler is needed for
FinderDrop's sake.** Nothing here says which part of the system makes
that close -- only that it has happened by the time the application
runs.

The control panel example, in Apple's Control Panel on 6.0.8 (the disk a
control panel built on the library is tested on: the stock System
Folder stripped to the Finder and the panel), was dropped on, then an
application launched with the panel showing: the same three facts, the
Control Panel's DCE closed. Apple's Control Panel itself sets
`dNeedGoodbye` (its DCE flags 0x3460), which the example accessory has
no need to copy.

Not run: Systems 2.0 to 4.x for the accessory; the control panel on
4.1. The 64K ROM was run later (the entry on an application launched
over an open accessory on the 64K ROM), and an accessory opened with
Option under MultiFinder is the case this does NOT cover (the entry on
it, which is a fault).

*How to repeat it:* `goodbye.py --system 6 --repo <checkout> --quit
256,249 --after 474,110,300,200`, `goodbye.py --system 1.1 --machine
512Ke --repo <checkout> --move 324,150 --dragme 174,85 --app 80,168 --quit
255,195 --after 80,168,300,120`, and `--host cdev --system 6` with the
6.0.8 arguments, beside `dragshell.py`; `goodbyecheck.py` reads a
snapshot on its own. The stock System 6 disk those scripts start from
boots MultiFinder, and the script deletes MultiFinder, DA Handler and
Backgrounder from it -- a run that leaves them reports "MultiFinder" and
is not this question.


### An ejected disk's icon is refused before the File Manager is asked

*Replaced on 2026-11-14: an ejected disk's items are no longer refused
but taken, unchecked, and the program's own first File Manager call on
one asks for the disk -- the entry on a drag from an ejected disk, at
the end. What follows is how the refusal came about and stays true of
the drag: nothing in it may read an offline volume.*

*Run on:* emulated Macintosh Plus, 1 MB, System 6.0.8 under MultiFinder,
2026-11-12, with the example desk accessory built as committed, once
from the commit before this change and once from this change.

A Finder keeps an ejected disk's VCB, dims its icons and goes on drawing
them, and `_GetVolInfo` answers for it from the VCB alone -- so the sender
believed the volume and asked the catalog, from inside the `_GetMouse`
patch, and the File Manager put up its disk-switch alert in the middle of
the Finder's drag. A floppy in the second drive, its window opened, its one
file selected, the disk ejected with Command-E, and the dimmed file
dragged onto the accessory's window:

| build | result |
|---|---|
| before | "Please insert the disk: Floppy" as the drag reaches the window; still up in the last frame |
| after | no alert; the outline follows the pointer, the drag is refused, the icon stays where it was |

What refuses it is the VCB's `vcbDrvNum`: 0 for a volume that is offline
or ejected (Inside Macintosh: Files, 2-47 and 2-77), read off the VCB
queue because the 64K ROM has no `PBHGetVInfo` and `_GetVolInfo`'s block
stops before `ioVDrvInfo`. `vol_name`, `hfs_volume`, `startup_trash` and
the ROM path's `mfs_file` all ask it, and `mfs_file` now refuses a vRefNum
that is not negative, which the others already did. The ROM path
(Systems 1.1 and 3.0) was run later (the entry on an ejected disk on
the Finders that drag through the ROM). Not run: a network volume -- an
AppleShare volume's drive number has not been looked at, and if it is 0
such a volume is now refused.

*The other changes of the same commit, run as a regression:* 6.0.8 under
MultiFinder and under the single Finder, a disk, a folder and a file off
the desktop in one drag (three received) and then a file out of the
disk's icon-view window (taken, put back); under MultiFinder one file off
the desktop alone; System 3.0 on the 512K, DragMe out of the window, off
the desktop, and DragMe with DragTwo in one drag -- each received as
before, directory 0.

*The stack:* the three-item desktop drag under MultiFinder, built
`-DFDROP_DIAG=1 -DFDROP_STK=1`, leaves a low-water mark (dg18/dg19) of
0x38E3, 14563 bytes, before the change and after it -- the same number.
That is the accessory in the DA Handler's layer at 1 MB, which is not the
arrangement the README's 31115 was measured in.

*A colour port, not run.* `port_origin` reads a port's bounds from its
PixMap when the top two bits of the rowBytes word say CGrafPort, and the
three places that wanted a port's origin use it. Every sender run has
been on a Plus; Mini vMac's Mac II build (`mv-II256` on `MacII.ROM`)
stops at "Welcome to Macintosh" with this System 6 disk, at 60 and at 200
seconds, so System 6 with a colour Finder window is still unexercised and
the fix is by construction.

*How to repeat it:* `dragshell.py --system 6 --item 70,171 --at 458,200
--target 224,268 --floppy Gone --step d:474,110 --step w:3 --step
c:110,105 --step K:e --step w:4 --step s:ejected --from 110,105`, with
`MM_ROOT=<checkout>` for the build to test; the regression is the 6.0.8
command of "Folders and disks are handed over" above, without
`--front`, with `--after c:300,330 --after d:468,45
--after w:3 --after 330,110,224,268`, with and without `--single`, and
`--system 3.0 --machine 512K --item 70,187 --move 300,150 --at 28,88
--target 300,310` with nothing, `--desk 50,290`, or `--two 176,108
--dump`. The stack run is the first with
`DRAGSHELL_DEFS="-DFDROP_DIAG=1 -DFDROP_STK=1"`; the Mac II attempt is
`--emu <mv-II256>/minivmac --rom <MacII.ROM> --menu --wait 200`.

### SysVersion before System 4.1: the Plus ROM clears it, and on the 64K ROM the System's patch writes its own version there

*Read out of the ROMs and the System files 2026-11-12, and checked
against 294 RAM snapshots from 66 earlier emulator runs.*

`fdrop_have_drag_manager` calls `_Gestalt` only where the word at
`$015A` is at least `$0604`. `SysVersion` is documented from System 4.1,
so the question was whether below that the word is whatever was left in
RAM. **It is not, on either ROM**, and an emulator that starts with zeroed
RAM shows the same value a real machine does, because nothing reaches it
from before the boot:

- **64K ROM** (`Mac128K.ROM`, checksum `$28BA61CE`). After its RAM test
  the reset code fills all of RAM with `$FFFFFFFF` -- ROM `$40026C`:
  `movea.l a3,a0` (`a3` = `$600000`, RAM before the overlay is switched
  off), `moveq #-1,d1`, `$20000` longs of `move.l d1,(a0)+`. Nothing in
  the ROM refers to `$015A` afterwards (no absolute reference anywhere
  in it: the word occurs four times, at `$400966` and `$4009C6` as the
  immediates of `suba.w`/`adda.w #$015A,sp`, and at `$404506` and
  `$40DB8A` as `bsr.w` displacements). The System's patch for this ROM then writes it: every System
  file from 1.1 to 3.2 holds `move.w (d16,PC),$015A.w` (`31FA xxxx
  015A`), whose source word is that patch's version, directly behind
  `'PTCH' $0069` (`$69` being this ROM's version byte) in 3.0, 3.1 and 3.2:

  | System file | instruction at | word | in RAM, from snapshots |
  |---|---|---|---|
  | 1.1 | `$2D746` | `$0014` | `$0014` |
  | 2.0, both Finders | `$15346`, `$20B46` | `$0014` | `$0014` |
  | 2.1 | `$56F6C` | `$0014` | `$0014` |
  | 3.0 | `$3A93A` | `$0017` | `$0017` |
  | 3.1 | `$109B8` | `$0017` | `$0017` |
  | 3.2 | `$34662` | `$0018` | `$0018` |

  Offsets are into the 400K disk images the shell boots; the next
  instruction in each is `move.l $2A6,$2B2`, the same one that follows
  the Plus ROM's clear.

- **Plus ROM** (`vMac.ROM`, `$4D1F8172`). `clr.w $015A` at ROM `$400642`,
  on the straight path from building the trap tables (`$400516`) with
  only the CPU probe and the expansion-ROM hook at `$400628` before it.
  No System from 1.1 to 3.4 writes it on this ROM: every snapshot taken
  on it under those Systems reads `$0000`.

- **From 4.1** the System's patch loader writes the version: 6.0.5's
  `PTCH 0` has `move.w #$0605,$015A` at `+$E8`. 6.0.8's snapshots read
  `$0607`, not `$0608`: that System reports itself as 6.0.7 there.

So the test is safe as written on every System the library supports,
and rests on two things that cannot change: a fixed ROM and Apple's own
System files. A third-party System patch writing `$015A` below 4.1 is
the only way round it, and none is known.

*How to repeat it:* the snapshots are the `ram-*.bin` files beside
the drag shell's evidence; the word is at offset `$15A`, and `ROM85`
(`$28E`) says which ROM (`$FFFF` the 64K ROM, `$7FFF` the Plus).

### A machine without `_HWPriv` can still have a cache: a Macintosh II below 6.0.5, covered by its own `_GetTrapAddress`

*Read out of the ROMs and the System files 2026-11-12. Not run: Mini
vMac emulates no cache.*

`tramp_flush` used to say that a machine without `_HWPriv` has no cache.
**That is false of the Macintosh II.** What was found:

- **The Macintosh II ROM does not implement `_HWPriv`** (`MacII.ROM`,
  version `$0178`, checksum `$9779D2C4`). Its trap tables are built at
  ROM `+$D0A` the same way as the Plus ROM's (fill with the handler at
  `+$D98`, `moveq #12,d0`; then the delta stream whose offset is the long
  at ROM `+$22`; 1024 Toolbox entries, then 256 OS). Decoded: 719
  Toolbox and 201 OS traps; `$A198` unimplemented. The decode agrees with
  what is known of the machine: `_StripAddress` and `_DTInstall`
  present, `_Gestalt` and `_SysEnvirons` absent. The Plus ROM decodes,
  by the same code, to 144 OS and 482 Toolbox traps, `$A198` and `$A0BD`
  both unimplemented.

- **`_HWPriv` reaches a Macintosh II with System 6.0.5**, as `ptch 38`
  (150 bytes, byte-identical in 6.0.5, 6.0.7 and 6.0.8): selectors 0 to
  3 -- swap and flush the instruction cache, swap and flush the data
  cache -- by `movec` on CACR, `-502` for any other, one install entry
  for `$A098`. It uses `bfextu`, so it is 68020-only. `PTCH 376`, the
  patch for ROM `$0178`, loads it at `+$9622` only where the ROM lacks
  the trap: `moveq #-104,d0` (`$98`), `_GetOSTrapAddress`, compared with
  `_Unimplemented`, and `ptch 38` if they are equal. In 6.0.4 nothing
  loads a `ptch 38` and none exists; Systems 4.1 to 6.0.4 have no CACR
  instruction anywhere in their `PTCH 376`. The Plus's `PTCH 117` and the
  SE's `PTCH 630` load the same `ptch 38` from 6.0.5 where `CPUFlag`
  (`$12F`) is at least 2, which is an accelerator. System 7.0.1 and
  later were not read: their patches are `dcmp`-compressed.

  So on a Macintosh II, Systems 4.1 to 6.0.4 have no `_HWPriv`; from
  6.0.5 they have.

- **The supported flush that needs no `_HWPriv` is `_CacheFlush`**,
  `$A0BD`: *Inside Macintosh: Memory* (1992), page 4-29, says to check
  that `_HWPriv` is implemented before calling any of its routines,
  "the only exception is the FlushCodeCache procedure, which is
  available whenever the processor has a cache that can be flushed"; on
  4-31, its trap macro is `_CacheFlush`, and on a copy-back processor it
  flushes the data cache first. In the Macintosh II ROM, OS `$BD` is
  ROM `+$65F2`: `movec cacr,d1; bset #3,d1; movec d1,cacr; rts` (bit 3
  clears the 68020's instruction cache). **It is not in the Plus ROM**,
  and nothing in 6.0.5's `PTCH 117` or `PTCH 630` installs it, so an
  accelerated 68000-ROM machine has `_HWPriv` from 6.0.5 and no
  `_CacheFlush` from Apple. Neither trap covers both kinds of machine.

- **Why the exposure is closed on a Macintosh II anyway.** The ROM
  calls the cache flush vector `jCacheFlush` (`$06F4`), as `jsr
  ([$06F4])`, at six places, and two of them are the first instructions
  of `_SetTrapAddress` (OS `$47`, ROM `+$6568`) and of
  `_GetTrapAddress` (OS `$46`, ROM `+$65C2`): `move.l d1,-(sp); jsr
  ([$06F4]); move.l (sp)+,d1`. A third is the start of `_BlockMove`
  (`+$EEA2`, before any byte is moved, for any count above zero), and
  *Memory* 4-10 documents that `_BlockMove` flushes the instruction
  cache -- so the vector is a flush. It is more than inferred: `$06F4`
  is `$0400 + $BD × 4`, the OS trap table's own cell for `_CacheFlush`,
  so `jCacheFlush` holds whatever the table build put in that entry --
  the `movec` routine at `+$65F2` below -- and nothing in the ROM writes
  `$06F4` by its address (its seven `$06F4` words are the six `jsr`s and
  one branch displacement). `tramp_flush` asks
  `GetOSTrapAddress(_HWPriv)` after both jumps are written, at all three
  places that rewrite a trampoline, so on that ROM the question flushes
  the cache before the answer comes back "no". No System from 4.1 to
  6.0.4 replaces `_GetTrapAddress` on this ROM: no `PTCH 376` installs a
  patch for trap `$46`, and no `ptch` table names it.

  This rests on how a ROM is written and not on anything documented,
  but the ROM cannot change, and a machine whose ROM has `_HWPriv` does
  not need it. Without it the exposure would be real but narrow: the
  68020's cache is 256 bytes, direct-mapped by address bits 2 to 7, and
  not updated by data writes, so a stale jump survives only until an
  instruction fetched from an address with the same low bits evicts it.
  Any 256 bytes of straight-line code do that, and a trap call is longer
  than that. That makes a stale jump unlikely, not impossible, and the
  case where it matters -- `patch_remove`, with the old target about to
  be unloaded -- is not one to settle on likelihood.

What remains open is machines that could not be read here:
`OPEN_ITEMS.md`.

### The Finder's stack under the sender: about 31 KB left at the deepest, on System 4.1 and 6.0.8

*Run 2026-10-19 in a control panel built on the library, on emulated
System 4.1 and 6.0.8 on a Macintosh Plus, built `-DFDROP_DIAG=1
-DFDROP_STK=1`, after the heap walk had replaced the folder pairing.
Brought here from that program's records on 2026-11-12, where the run
was written down; the README's figures are this table's last row.*

The sender runs inside `_GetMouse`, called by the Finder's drag, so the
stack it spends is the Finder's. `dg16`/`dg17` is the room left when a
drag first reaches the window and its files are looked for, at
`resolve_drag`; `dg18`/`dg19` the low-water mark, taken in `cat_entry`,
the parameter block every File Manager question on the drag path goes
through. Both high words were 0.

| run | room at `resolve_drag`, 4.1 | low water, 4.1 | room, 6.0.8 | low water, 6.0.8 |
|---|---|---|---|---|
| one file out of a window | 31584 | 31341 | 31398 | 31155 |
| several files off the desktop | 31544 | 31301 | 31358 | 31115 |

243 bytes from `resolve_drag` to the bottom on both Systems and both
paths, which is what says the figure is the code's and not the
machine's. The desktop drag starts 40 bytes deeper because the Finder
calls its drag routine from deeper for a desktop icon. Neither figure
counts what the File Manager spends below `PBGetCatInfo`, which no probe
here can see.

*Not since re-run.* Names to 63, the trampolines and the ejected-disk
checks all came later; the accessory under MultiFinder measured 14563
bytes in a different arrangement (the entry on an ejected disk's icon),
and MultiFinder at 512 KB and 1 MB, accessory and application, 14555
in every run (the entry on the Finder's stack under MultiFinder).

### The receiver on a real Macintosh: an LC475 under System 7.6.1

*2026-09-28, in a control panel built on the library with
`-DFDROP_SENDER=0`. Brought here from that program's records on
2026-11-12.*

Files dragged out of the Finder onto the panel's window were taken. The
only physical Macintosh this library has met. It says nothing about the
sender: 7.6.1 has a Drag Manager, `fdrop_install` registers its handlers
and returns before `patch_install`, so a build with the sender would
have done exactly the same. The sender has never run on hardware.


### The route through the ROM is 2940 bytes, and a host above System 4.0 can leave it out

*Built 2026-11-12; run later the same day, the last entry.* The sender follows a Finder's drag two
ways: Finders 5.4 to 6.1.8 by their own drag routine, and Finders 1.1g
to 5.3 through the ROM's `DragTheRgn`, with two older record layouts and
MFS's file numbers. The second is reachable only on Systems 1.1 to 3.2.
A control panel cannot exist there, and the sender installs for an
application only under MultiFinder, so two of the three host forms carry
it for nothing. `FDROP_ROM_ROUTE=0` leaves it out: its functions, the
lookup at install, the dispatch in `fdrop_answer`, and the test in the
`_GetMouse` patch that sent every call from inside `DragTheRgn` into C.

| example, as a code resource | default | `FDROP_ROM_ROUTE=0` |
|---|---|---|
| desk accessory | 10622 | 7682 |
| control panel | 10300 | 7360 |

With the switch at its default, and with `FDROP_SENDER=0`, the two code
resources and the application's two library objects are byte for byte
what they were before the switch existed. Built with it off and
`-Werror`: alone, with `FDROP_SENDER=0`, and with `FDROP_DIAG=1
FDROP_STK=1`; no symbol of the route is left in the linked image.

What it does not take out is the 64K ROM's trap table and trampoline in
`patch_install`, which is a separate question: whether any System with
a Finder of 5.4 or later runs on that ROM.

*How to repeat it:* `sh examples/cdev/build.sh
-DFDROP_ROM_ROUTE=0`, and the same for `da`.


### The other way round: a build for Systems 1.1 to 3.2 alone is 3192 bytes smaller

*Built 2026-11-12; run later the same day, the last entry.* A desk accessory made for a Macintosh 128K
never meets a Finder of 5.4 or later and never meets a Drag Manager, and
carries both. Two switches leave them out, each on its own:

| example desk accessory, the `DRVR` | bytes |
|---|---|
| everything (the default) | 10622 |
| `FDROP_OWN_ROUTE=0`: no route for Finders 5.4 to 6.1.8 | 8188 |
| `FDROP_RECEIVER=0`: no Drag Manager door | 9882 |
| both: Systems 1.1 to 3.2 and nothing later | 7430 |
| for comparison, `FDROP_ROM_ROUTE=0` | 7682 |
| `FDROP_ROM_ROUTE=0 FDROP_RECEIVER=0`: Systems 4.0 to 6.0.8 alone | 6906 |

Without `FDROP_OWN_ROUTE` the `_FindWindow` patch is two instructions
that pass the call on, and it is still installed: the trampoline is the
one thing two copies of the library share, and it stays the same block
in every build. Without `FDROP_RECEIVER`, `fdrop_install` still asks for
a Drag Manager and still stands aside where there is one, so such a
build does nothing at all on System 7.5 -- it does not fall back to a
sender that cannot work there -- and `fdrop_can_drop` answers false. A
sender with neither route is refused at compile time.

The default build, `FDROP_SENDER=0` and `FDROP_ROM_ROUTE=0` are byte for
byte what they were before these two existed. All three examples build
`-Werror` with each switch off alone, both off, both off with
`FDROP_DIAG=1 FDROP_STK=1`, and the receiver off with each of the other
two. `FDROP_STK`'s low-water mark is now taken in the ROM route's
deepest frame as well (`mfs_file`), since a build without the other
route had nowhere to take it; that changes the `FDROP_STK` build and no
other.

*How to repeat it:* `sh examples/da/build.sh -DFDROP_OWN_ROUTE=0
-DFDROP_RECEIVER=0`.


### The trimmed builds, run: each takes what the default takes on its Systems, and nothing on the others

*Run 2026-11-12 on emulated machines, with the example desk accessory
and the example control panel built from this commit's source. Each
image was checked for its size before it was booted (7430, 7682 and
7360 bytes), and the default build was run beside it the same day with
the same gestures wherever a row says "as the default".*

**The build for the early Finders alone**, the accessory built
`-DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0`, 7430 bytes. System 1.1 on the
64K ROM at 128 KB, and System 3.0 on the Plus ROM (512Ke):

| drag | 1.1, 128 KB | 3.0, Plus ROM |
|---|---|---|
| a file out of the disk's window | DragMe, directory 0; the icon stays | the same |
| the file moved to the desktop, then dragged from there | DragMe, directory 0; stays on the desktop | the same |
| two files in one drag, out of the window | DragMe and DragTwo | DragMe and DragTwo |
| two files in one drag, off the desktop | DragMe and DragTwo | not run |
| the disk | "Early", volume −1, directory 1 | the same |

Every row ended at step 5 with the count the window shows, read out of
RAM after the release as well as off the screen. The default build, run
through the same nine gestures, ended each at step 5 with the same
count, read out of RAM; its screens were not read one by one.

**The stack on the route through the ROM**, the same build with
`-DFDROP_DIAG=1 -DFDROP_STK=1` (8240 bytes), which had never been
measured anywhere:

| run | room at the first look (`dg16`/`dg17`) | low water (`dg18`/`dg19`) |
|---|---|---|
| 1.1, 128 KB: one file out of the window | 7087 | 6859 |
| 1.1, 128 KB: two files off the desktop | 6933 | **6705** |
| 1.1, 128 KB: the disk | 6933 | not taken: a disk is never asked of `mfs_file` |
| 3.0, Plus ROM, 512Ke: two files out of the window | 31573 | 31345 |

228 bytes from the first look to the bottom in all three, on both ROMs,
so that part is the code's. What is small on the 128K is the stack
itself: the Finder's is 8592 bytes there, from `HeapEnd` ($179F4) to
`CurStackBase` ($19B84), and its own drag is 1.5 to 1.7 KB deep when
the sender first looks. `ApplLimit` is 238 bytes above `HeapEnd`, so the
least there could be is 6467. As on the other route, nothing here counts
what the File Manager spends below `_GetFileInfo`.

**The same 7430-byte accessory where it has no route**, System 6.0.8 on
a Plus at 1 MB. For "the Finder alone" the accessory was also built
`-DFDROP_SENDER=0`, which patches nothing, and put through the same
gestures; the frames at the drop are the same file, byte for byte.

| gesture, accessory open | single Finder | MultiFinder |
|---|---|---|
| a file and a folder off the desktop onto its window | Received: 0; both icons leave where they were, the Finder's own move, as with no sender | Received: 0; both stay where they were, as with no sender |
| a file out of the disk's window onto it | not taken; it leaves the window, the Finder's own move | not taken; stays in the window |
| a folder dragged inside the disk's window | moved | moved |
| the disk's window dragged by its title bar | moved | moved |
| the accessory's window dragged by its title bar | moved | brought forward, not moved: the press changed layers, in the default build too |

**And on System 7.5**, which has a Drag Manager, as a desk accessory
file in Apple Menu Items on a Plus at 4 MB: a file dragged out of the
disk's window onto it gives `Received: 0` and stays where it was, where
the default build in the same run gives `Received: 1` through the Drag
Manager. An icon dragged inside the Finder's window and the window
dragged by its title bar both moved, in both builds (and a folder's
window opened behind, in both: the harness's clicks, not looked into).
The example still
writes "Through the Drag Manager." above the zero: it prints what
`fdrop_install` found on the machine, not what the build can do.

**The build without the route through the ROM**, `-DFDROP_ROM_ROUTE=0`:
the control panel (7360 bytes of code resource) on System 4.1 at 512 KB
and on 6.0.8 at 1 MB with and without MultiFinder, and the accessory
(7682 bytes) on 6.0.8 both ways.

| run | 4.1, 512 KB | 6.0.8 | 6.0.8, MultiFinder |
|---|---|---|---|
| control panel: a file out of a window | received | received | received |
| control panel: two files out of a window in one drag | both | both | both |
| control panel: two files off the desktop in one drag | both | both | both |
| control panel open: a file to the desktop, a file to the Trash, the Trash emptied | the Finder's, done | done | done |
| control panel open: its own window and the Finder's dragged by their title bars | both moved | both moved | the panel's moved; the first press only brought it forward |
| accessory: a disk, a folder and a file off the desktop in one drag | — | all three | all three |
| accessory: then a file out of the disk's window | — | received, four in all | received, four in all |
| accessory open: a folder dragged inside a window; the disk's window by its title bar | — | both moved | both moved |
| accessory open: its own window by its title bar | — | moved | brought forward, not moved |

The window drags are the rows this build could have changed: on a Plus
under 6.0.8 the default finds the ROM's drag routine behind the System's
patch and tests every `_GetMouse` made inside it, and this build has no
such test. The default control panel was run through the same Finder
drags and window drags on all three Systems, and the default accessory
through the whole 6.0.8 sequence both ways: every frame after a gesture
is the same file as the trimmed build's, byte for byte, where the disk's
free space is not in the picture; where it is, the last frames were
looked at and show the same windows in the same places.

**Not run:**

- `FDROP_RECEIVER=0` or `FDROP_OWN_ROUTE=0` each on its own, and
  `FDROP_ROM_ROUTE=0 FDROP_RECEIVER=0`: built only.
- The accessory built `-DFDROP_ROM_ROUTE=0` on System 4.1, which the
  accessory's harness does not boot; the control panel stands for it
  there. The example application with any switch off.
- Under MultiFinder, the Finder's own window dragged with the control
  panel open (the accessory run has it), and a second press on the
  accessory's title bar, which would have moved it.
- Two files off the desktop on 3.0, an ejected disk, a folder of an MFS
  disk and the Trash in the 7430-byte build: the walk is the default's
  code, and these were not repeated.
- The stack with a folder in the drag, which goes deeper than a file
  does (`rom_folder`), and on the 512K with the 64K ROM.
- Nothing here is a real machine.

*How to repeat it:* `DRAGSHELL_DEFS="-DFDROP_OWN_ROUTE=0
-DFDROP_RECEIVER=0" dragshell.py --system 1.1 --machine 128K --item
70,170 --move 300,150 --target 290,250 --dump --at 154,72`, with
`--desk 50,290`; with `--at 282,72 --two 170,88`; with `--two 40,300
--pre 298,88,40,300 --pre 170,88,90,260 --at 74,244`; and with `--at
456,29` alone for the disk. On 3.0, `--system 3.0 --machine 512Ke --item
70,123 --move 300,150 --target 300,310 --dump --at 28,88`, with `--desk
50,290`, `--two 176,108`, or `--at 456,29`. The icons are not where the
earlier entries' commands press: look at the `disk-open` frame first.
`trimcheck.py <run> <elf> <drvr>`, beside `dragshell.py`, finds the
accessory in each RAM snapshot by its own bytes and prints the state
words and `dg16` to `dg19`. On 6.0.8, `--system 6 --item 70,171 --at
458,94 --target 224,170 --folder DragFolder@458,150 --from 474,110
--shift 474,166 [--shift 472,45] --after c:300,330 --after d:468,45
--after w:3 --after 330,110,224,268 --after 74,160,250,185 --after
100,52,130,90 --after 100,50,360,280`, with and without `--single`. On
7.5, `drag75da.py --item 80,169 --move 300,220 --from 78,190 --target
300,290 --after 112,100,240,180 --after 60,37,80,50`, which wraps the
`DRVR` as a desk accessory file with Rez. The control panel is
`sh examples/cdev/build.sh -DFDROP_ROM_ROUTE=0` in a control
panel harness that is not part of FinderDrop, with its drop, two-file,
desktop and Finder-drag runs; `trimpanel.py`, beside `dragshell.py`, adds the two title-bar
drags (260,40 to 290,60 and 200,50 to 220,88) after the Trash is
emptied.


### Three switches, one for each era, and what is left out is left out at compile time

*Built 2026-11-12; nothing new run.* The switches had grown as four --
`FDROP_SENDER` and, under it, the two routes and the receiver -- with a
combination that was refused. They are three now, one for each way a
drop arrives, and any combination builds: `FDROP_ROM_ROUTE` (Systems 1.1
to 3.2), `FDROP_OWN_ROUTE` (4.0 to 6.0.8) and `FDROP_RECEIVER` (the Drag
Manager, 7.5 and later). `FDROP_SENDER` is worked out from the first
two, and a build that still passes it stops with an error.

What changed in the code is where the sender's shared half goes when
both routes are off. It used to be left for `--gc-sections` to drop,
which a host linked without that option did not get; it is now under
`#if`, with the helpers only the sender reaches.

| | before | now |
|---|---|---|
| example application's `fdrop.o`, Drag Manager alone | 11308 | 3524 |
| the application, as a MacBinary file | 24832 | 19072 |
| example accessory, Drag Manager alone | 3140 | 3094 |
| example control panel, Drag Manager alone | 2818 | 2772 |
| example accessory, all three off | did not build | 2196 |

The 46 bytes off the two code resources are `put_drop` inlined into the
receive handler, its one caller left. Every other configuration is byte
for byte what it was, compared image against image for the accessory and
the control panel and object against object for the application: the
default, each switch off alone, the build for Systems 1.1 to 3.2, the
build for 4.0 to 6.0.8, and the three diagnostic builds. So the runs of
the entry above still describe the builds they were made with.

Not run: the Drag Manager alone as it is built now, and all three off
(`OPEN_ITEMS.md`).

*How to repeat it:* `sh examples/app/build.sh -DFDROP_ROM_ROUTE=0
-DFDROP_OWN_ROUTE=0`, and the same for `da` and `cdev`.


### The 64K ROM's trap table is 514 bytes, and has a switch of its own

*Built 2026-11-12, not run.* Beside the three switches for the eras
there is one seam in the code that is about the machine: the 64K ROM
keeps its traps as a table of word offsets, and patching it is a path of
its own -- `trap_addr64`, `rom64_present`, `rom64_encode`, the first
half of `patch_install` and the first branch of `patch_remove`.
`FDROP_ROM64K=0` leaves it out. A host whose floor is a 512Ke or a Plus
can never reach it; on a 64K ROM a build without it finds no table it
knows and installs nothing.

| example, as a code resource | with | `FDROP_ROM64K=0` |
|---|---|---|
| desk accessory, everything else in | 10622 | 10108 |
| control panel, `FDROP_ROM_ROUTE=0` | 7360 | 6868 |
| desk accessory, Systems 4.0 to 6.0.8 alone | 6906 | 6416 |

With the switch at its default every configuration compared is byte for
byte what it was: the default, `FDROP_ROM_ROUTE=0`, the build for
Systems 1.1 to 3.2, the Drag Manager alone and the diagnostic build, in
all three examples.

One other kind of switch was considered and not made: for what a drag
may carry (folders, disks, the Trash). It would not leave code out so
much as change what is refused, which is behaviour to decide and run,
not a seam to cut along. The opposite switch to this one was first
refused here and then made: the next entry.

*How to repeat it:* `sh examples/cdev/build.sh
-DFDROP_ROM_ROUTE=0 -DFDROP_ROM64K=0`.


### A build for the 64K ROM alone: 6888 bytes, and it takes its drops on a Macintosh 128K

*Run 2026-11-12, emulated Macintosh 128K (64K ROM, 128 KB), System 1.1,
the example accessory built `-DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0
-DFDROP_ROMPLUS=0`.* `FDROP_ROMPLUS=0` leaves out the trap-table path of
the Plus ROM and every later one, and what only those machines have:
the reuse of a trampoline (`tramp_at`, `tramp_reusable`), the MultiFinder
test, the cache flush, the second half of `patch_install` and the second
branch of `patch_remove`. None of it runs on a 64K ROM in any build --
that ROM's path makes its own block, there is no MultiFinder to hide a
patch and no cache -- so a build for that ROM alone gives nothing up.
It was first written off in the entry above as costing what keeps two
copies safe; on the machine it is for there is never a second copy to
be safe beside.

| example desk accessory | bytes |
|---|---|
| everything | 10622 |
| Systems 1.1 to 3.2 on any ROM | 7430 |
| the same, 64K ROM alone | 6888 |

| drag, System 1.1 at 128 KB, the 6888-byte image | result |
|---|---|
| a file out of the disk's window | received (step 5, one item) |
| a file off the desktop | received |
| two files out of the window in one drag | both, DragMe and DragTwo, directory 0, read off the window |
| the disk | received |
| two files off the desktop, built `FDROP_DIAG=1 FDROP_STK=1` (7742 bytes) | both; room at first look 6933, low water 6705 |
| a file out of the window on System 3.0, a 512Ke (the Plus ROM) | nothing installed (`FDROP_ENABLE` 0), nothing taken |

Each result is the library's state words read out of a RAM snapshot,
with the image in RAM checked against the image as built; the two-file
run was also read off the screen. The stack figures are the ones the
build with the later ROMs' path gave, to the byte, which is as it should
be: nothing on the drag's path changed. The last row is the switch doing
what it says: on a Plus ROM this build knows no table and patches
nothing.

With `FDROP_ROMPLUS` at its default every configuration compared is byte
for byte what it was (the default, Systems 1.1 to 3.2 on any ROM, the
control panel's two switches, the Drag Manager alone, the diagnostic
build), in all three examples. Twelve configurations build `-Werror`,
among them a sender with both ROM switches off, which installs nothing
anywhere.

Not run: this build on a Macintosh 512K, the other machine it is for; on
Systems 2.0 to 3.2 on the 64K ROM; with a folder, the Trash or an
ejected disk in the drag.

*How to repeat it:* `DRAGSHELL_DEFS="-DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0
-DFDROP_ROMPLUS=0"` with the System 1.1 commands of the entry on the
trimmed builds, run, and its `--system 3.0 --machine 512Ke` command for
the last row.


### A build holds nothing of a switch that is off, and the smallest sender is 5476 bytes

*Run 2026-11-12 on emulated Macintoshes; every image named here is byte
for byte the one committed with this entry.* Two rules were applied to
the whole library, and one switch and one preset added.

**The rule.** A build holds nothing that belongs to a switch that is
off: not a function, not a state word in `fdrop.S`, not a store into one
or a call made on its behalf. Before, a switch took out its functions
and left the rest. `examples/check_switches.sh` now compiles
the two halves with each switch off, unlinked and with nothing inlined,
and fails if a name on that switch's list is still there -- or if a
name on a list is not in the full build, which is how a stale list is
caught, and what it did on its first run.

What that took out, beyond the functions already guarded: every state
word outside its switch (the block in `fdrop.S` is guarded word by
word); the stores the ROM route made into the other route's words; the
`_FindWindow` patch, its trampoline jump and its saved table word in a
build for the 64K ROM without `FDROP_OWN_ROUTE`, which patches one trap
through a block of six bytes; the question whether there is a Drag
Manager, in a build that could not find itself beside one; the word
that says which trap table was patched, in a build that knows one; and
from a 64K ROM build everything only a later ROM can use -- the
recogniser for the patch System 3 puts in front of `DragTheRgn`, the
MultiFinder probe, the test for a window in another layer, the colour
port, the signature a trampoline is reused by.

That recogniser was checked before it went: in every RAM snapshot kept
from a 64K ROM machine -- Systems 1.1, 2.0, 2.1, 3.0, 3.1 and 3.2, at
128 KB and 512 KB -- the table entry for `_DragTheRgn` is the ROM's own
(word `$6218`, bit 15 clear). No System patches it on that ROM.

**`FDROP_FINDER5`** is the records of Finders 5.0 to 5.3 and that
System 3 patch: Systems 2.1 to 3.2. Without it the route through the ROM
reads Finder 1.1g's record alone. **The smallest sender** turns off that,
`FDROP_OWN_ROUTE`, `FDROP_RECEIVER` and `FDROP_ROMPLUS`.

| example accessory | bytes |
|---|---|
| everything (unchanged, byte for byte) | 10622 |
| the smallest sender: the 64K ROM, Finder 1.1g's records | 5476 |
| the 64K ROM, Systems 1.1 to 3.2 | 5698 |
| Systems 1.1 to 3.2 on any ROM | 7126 |
| without the ROM route | 7628 |
| the Drag Manager alone | 3004 |
| every switch off | 1826 |

The example control panel without `FDROP_ROM_ROUTE` and `FDROP_ROM64K`
is 6804 bytes. The default and the diagnostic build are the only
configurations whose bytes did not change, so every trimmed build was
run again:

| build | machine | result |
|---|---|---|
| the smallest sender (5476) | System 1.1, 128 KB, 64K ROM | a file out of the window, a file off the desktop, two files in one drag, the disk: each received (step 5; one, one, two and one item) |
| the same with `FDROP_DIAG=1 FDROP_STK=1` (6308) | the same | two files off the desktop received; room at first look 6933, low water 6715 |
| the smallest sender | System 3.0, 512K, 64K ROM | installed; the drag recognised and left to the Finder (step 64, state 3: no record it knows); nothing taken |
| the smallest sender | System 3.0, 512Ke, Plus ROM | nothing installed, nothing taken |
| the 64K ROM, 1.1 to 3.2 (5698) | System 1.1 at 128 KB; System 3.0 on a 512K | a file out of the window received on each |
| 1.1 to 3.2 on any ROM (7126) | System 1.1 at 128 KB; System 3.0 on a 512Ke | a file received on each; two in one drag on 3.0 |
| accessory without the ROM route (7628) | 6.0.8, under MultiFinder and under the single Finder | a disk, a folder and a file off the desktop in one drag, then a file out of the window: four squares, each way |
| control panel without the ROM route or the 64K ROM (6804) | 4.1; 6.0.8; 6.0.8 under MultiFinder | one file on 4.1 and 6.0.8; two files in one drag on 6.0.8 each way |
| the Drag Manager alone (3004) | System 7.5 | one file, "Through the Drag Manager" |

The accessory's rows on 1.1 and 3.0 are the library's state words read
out of RAM snapshots, the image in RAM checked against the image built;
the others were read off the screen. System 3.0 on a 512K is a run the
earlier entries did not have, and it is also what says the System 3
patch is not missed on that ROM: the build without its recogniser took
the drop there.

Those runs were made with a single flag, `-DFDROP_1984`, that set those
four switches off at once. The flag was taken out the same day; the
four switches passed by hand build the same image, byte for byte.

Not run then, and run since: the entries on the trimmed builds not
yet run, and on the rest of them.

*How to repeat it:* `sh examples/check_switches.sh`;
`DRAGSHELL_DEFS="-DFDROP_FINDER5=0 -DFDROP_OWN_ROUTE=0
-DFDROP_RECEIVER=0 -DFDROP_ROMPLUS=0"` with the System 1.1 commands of the
entry on the trimmed builds, run, and `--system 3.0 --machine 512K
--item 70,187 --move 300,150 --target 300,310 --dump --at 28,88` for the
512K; the other rows are that entry's commands with the flags of the
table.



### A control panel without the ROM route or the 64K ROM, through every drag and the Finder's own

*Run 2026-11-12 on an emulated Macintosh Plus.* A control panel built
on the library, larger than the example and with a list its drops load
into, was built with `-DFDROP_ROM_ROUTE=0 -DFDROP_ROM64K=0` given to
both halves of the library and to the host, and run beside the same
panel built with no flag. Compiled as that host compiles it (`-Os
-mpcrel -ffunction-sections`), `fdrop.o` is 5944 bytes of text against
9542 and `fdrop.S`'s object 240 against 282; the panel's code resource
is 3496 bytes smaller. `-DFDROP_FINDER5=0` beside `-DFDROP_ROM_ROUTE=0`
changes no byte of either object, and nor does `-DFDROP_ROM64K=0` in a
build with both routes off.

| drag onto the panel | 4.1, 512 KB | 6.0.8, 1 MB | 6.0.8, MultiFinder |
|---|---|---|---|
| a file out of a window | received | received | received |
| two files out of a window in one drag | both | both | both |
| a file off the desktop | received, and still on the desktop | the same | the same |

On System 7.5.3 a file out of a window arrived through the Drag
Manager. On System 7.0.1, which has none, nothing arrived and the file
stayed where it was.

| the Finder's own, panel open | 4.1, 512 KB | 6.0.8, 1 MB | 6.0.8, MultiFinder |
|---|---|---|---|
| a file to the desktop; a file to the Trash, the Trash emptied | done | done | done |
| the panel's window by its title bar | moved | moved | brought forward by the first press, moved by the second |
| the Finder's window by its title bar | moved | moved | not pressed |
| an icon moved inside the Finder's window | not run | moved, nothing received | not run |

The panel built with no flag was put through the same Finder drags and
title-bar drags on all three: every frame up to and after each gesture,
the two taken with the button down included, is the same file as the
trimmed build's, byte for byte, except those that show a disk's free
space. The icon moved inside a window has no such control run.

Not run: a folder, a disk, the Trash, a file of a second volume or of
an MFS volume in the drag; a real machine.

*How to repeat it:* a control panel harness that is not part of
FinderDrop, with its drop, two-file, desktop and Finder-drag runs;
`trimpanel.py`, beside `dragshell.py`, adds the two title-bar drags
(260,40 to 290,60 and 200,50 to 220,88) after the Trash is emptied, and
the icon's move (200,180 to 300,150) once the Finder's window is in
front.

### Finders 5.4 and 5.5 on the 64K ROM: drops from an HFS volume are taken, and with MFS disks alone the desktop is not found

*Run 2026-11-12 and 13, emulated Macintosh 512K (64K ROM, 512 KB) in
Mini vMac, with the example accessory merged into the System file.*
What booted, read off the disks and then off the screen ("About the
Finder"):

| System, as its `STR ` 0 says | Finder | on the 64K ROM |
|---|---|---|
| 3.3 (12-Jan-87), Apple's 400K MFS "Work Station Installer" disk | 5.5 | boots, runs |
| 4.0 (15-Jan-87), Apple's 400K MFS "System Software" disk | 5.4 | the desktop draws and the pointer never moves again: the disk untouched, with the accessory, and with Finder 5.5 in place of its own |
| 4.1, off Apple's 800K HFS "System Tools" disk | 5.5 | boots, runs |
| 3.3 and 4.1, with 4.0's Finder | 5.4 | boots, runs |

So System 3.3's Finder is 5.5, byte for byte 4.1's, and not 5.4; and the
4.0 freeze is the System file's, on the 64K ROM as on the 512Ke and the
Plus (`OPEN_ITEMS.md`). Each System carries `PTCH` 105, the
64K ROM's patches. A 64K ROM boots only an MFS floppy, and 4.1's System
file (318 KB) does not fit on one beside its Finder, so the copy run here
leaves out the patches for the other ROMs (`PTCH` 117, 376 and 630) and
the 18- to 24-point fonts; nothing else of it was changed.

**HFS, on a ROM that has none.** Apple's `Hard Disk 20` file, in the
boot floppy's folder, loads the HFS File Manager at startup; Mini vMac
has no HD20, so an 800K HFS disk in the second drive stands in for it.
The file's third patch, a floppy driver, hangs the emulated 512K and was
left out; its other two loaded under 3.3 and 4.1 and the HFS disk
mounted.

| System, Finder | build | volume | out of the window | two in one drag | off the desktop | the Finder's, accessory open |
|---|---|---|---|---|---|---|
| 4.1, 5.5 | everything (10622 bytes) | HFS | received | both | received | to the desktop, to the Trash: done |
| 4.1, 5.5 | the smallest (5810) | HFS | received | both | received | done |
| 3.3, 5.5 | everything | HFS | received | both | received | done |
| 3.3, 5.5 | the smallest | HFS | received | both | received | done |
| 4.1, 5.4 | everything | HFS | received | both | received | done |
| 4.1, 5.4 | the smallest | HFS | received | both | received | done |
| 3.3, 5.4 | everything | HFS | received | both | received | done |
| 3.3, 5.4 | the smallest | HFS | received | both | received | done |
| 4.1, 5.5 | everything | MFS alone | refused | refused | refused | done |
| 4.1, 5.5 | the smallest | MFS alone | refused | refused | refused | done |
| 3.3, 5.5 | everything | MFS alone | refused | refused | refused | done |
| 3.3, 5.5 | the smallest | MFS alone | refused | refused | refused | done |
| 4.1, 5.5 | everything | an MFS file, HFS mounted | refused (step 96) | | refused (step 88) | |

"The smallest" is `-DFDROP_ROM_ROUTE=0 -DFDROP_FINDER5=0
-DFDROP_RECEIVER=0 -DFDROP_ROMPLUS=0`, which leaves `FDROP_ROM64K` and
`FDROP_OWN_ROUTE`. Each received item was read off the accessory's
window -- `DragMe`, then `DragMe` and `DragTwo`, on Work, volume −2,
directory 2 -- with the Finder's icons back where they were, and each
drag's state words out of a RAM snapshot with the button down and
after it: step 4 held, step 5 after, `LASTN` 1, 2 and 1, `ROMMODE` 0
(−1 in the smallest build, which has no ROM route). The Finder's own
drags moved the file to the desktop and into the Trash, which bulged.
Each run is one boot from a fresh disk; no frame was compared across
runs.

**Why MFS alone takes nothing, and where.** Every refused drag stopped
at step 90, which is `resolve_drag` finding no desktop, before any
item is looked at. The desktop is found as the container of the disks'
records, and a disk's record is the one whose parent is 1. Read out of
RAM mid-drag under Finder 5.5: the MFS disk `Early` and every file on
it have parent 0; with the HFS disk mounted, `Work` has parent 1 and its
files parent 2. So on a machine with only MFS disks no record says
"disk" in the way the sender reads, and every drag is refused whole and
put back by the Finder. With an HFS disk mounted the desktop is found,
and a file of the MFS disk is refused where the Plus ROM refused one:
out of its window at step 96, where `container_dir` asks `hfs_volume`
about the window's disk, and off the desktop at step 88, where the item's
own volume is asked the same. Taking an
MFS file on these Finders would want a check other than the catalog's,
as Finder 1.1g's route has, and that is not a few lines; not done. *Since 2026-11-15 they are taken: the entry on Finders 5.4 to 6.1.8 taking a drop from an MFS volume.*

Not run: a real 512K, or an HD20 and the `Hard Disk 20` file's own
floppy driver; System 4.1's System file whole; System 4.0, which does
not boot; a folder, a disk, the Trash or a second HFS volume in the
drag; the Finder's stack under `FDROP_STK` on this ROM; a control
panel, though System 4.1 lists the Control Panel here.

*How to repeat it:* `dragown64.py` and `own64seq.sh` beside
`dragshell.py`: `own64seq.sh NAME 4.1 "DEFS" --hd20` for the HFS rows
(`--finder 4.0` for Finder 5.4), and without `--hd20` for the MFS ones.

### The Finder's stack under MultiFinder: 14555 bytes left on 4.2 and 6.0.2 at 512 KB and on 6.0.8 at 1 MB, under the accessory and the application alike

*Run 2026-11-13 on an emulated Macintosh Plus, each System booted from
its stock disk under its own MultiFinder (the startup program set to it,
as Set Startup does; the disk's INITs out), with the library at
7a36f68 built `-DFDROP_DIAG=1 -DFDROP_STK=1`.*

The figure there was for MultiFinder -- 14563 bytes, 6.0.8 at 1 MB, the
accessory in the DA Handler's layer, a disk, a folder and a file off
the desktop -- was one drag on one System. This is the rest of what the
earlier entry left open: MultiFinder 1.0 (System 4.2) and System 6.0.2's
at 512 KB, which is the smallest machine either runs on, and 6.0.8 at
1 MB with the example application as the host, whose window is in a
layer of its own. Each run is one boot of a fresh copy of the disk and
one drag: files on the desktop, the first clicked and the rest
shift-clicked, dragged onto the host's window. The diagnostic words were
read out of the host's code image in a RAM snapshot (`FDROP_DG` is in
the code), before the drag and after it: every word read 0 before, so
the low-water mark is that drag's.

| System, MultiFinder | RAM | host | dragged | received | room at `resolve_drag` (dg16/17) | low water (dg18/19) |
|---|---|---|---|---|---|---|
| 4.2, MultiFinder 1.0 | 512 KB | accessory, DA Handler's layer | one file off the desktop | DragMe, directory 2 | 14848 | **14555** |
| 4.2 | 512 KB | accessory | three files off the desktop in one drag | all three | 14848 | **14555** |
| 6.0.2 | 512 KB | accessory | one file | DragMe | 14848 | **14555** |
| 6.0.2 | 512 KB | accessory | three files | all three | 14848 | **14555** |
| 6.0.8 | 1 MB | example application, its own layer | one file | DragMe | 14848 | **14555** |
| 6.0.8 | 1 MB | application | three files | all three | 14848 | **14555** |

Received means the host's window listed each file by name on the
startup disk, directory 2, with one black square per item, in a frame
from that boot, and `step` read 5. The three-file drags ask the catalog
once per file (`takeable`, then `cat_entry`), one after another from
the same frame, so they reach no deeper than one file does: the 293
bytes from `resolve_drag` to the bottom are the same in every row.

**Every row is the same number**, on two Finders, at two sizes of
machine and under both kinds of host, and the room at `resolve_drag` is
14848 = `$3A00` in all six. The probe measures from the stack pointer to
`HeapEnd`, which under MultiFinder is the Finder's own partition, so the
figure is what the Finder's partition leaves below its drag -- the
host's layer and the machine's RAM do not enter into it, which is what
the table shows. It is 8 bytes less than the 14563 of 2026-11-12, which
was a different drag (a disk and a folder with the file) of an earlier
build. Nothing here comes near the point where the sender would have to
spend less: 14.5 KB is left at the bottom of the deepest frame.

Not measured: 6.0.8's MultiFinder at 512 KB; the other MultiFinders,
4.3 to 6.0.7. A drag out of a window and a folder in the drag were
measured afterwards (the entry on the rest of the Finder's stack under
MultiFinder).

*An aside the runs turned up, in the example application, not in the
library:* its first line read "No Finder to drag from: run this under
MultiFinder" in every frame while the drops were being taken. The line
comes from `patched_now()` in `examples/app/finderdrop_app.c`, which
looks for `_GetMouse` within 32 KB of the application's own code; since
the table points at a trampoline in the system heap, that test cannot
pass. The library's state is not involved.

*How to repeat it:* `mfstk.py --stock System42.img --mem 512K --host da
--files 1 --item 70,139 --tag NAME` beside `dragshell.py` (outside the
repository), `--stock System602.img` for 6.0.2, `--files 3` for three,
and `--stock System6.hfv --mem 1M --host app --target 230,200` for the
application. `--repo` names the checkout to build.

### An accessory opened with Option under MultiFinder is not closed when its application quits, and its patches stay live

*Run 2026-11-13 on an emulated Macintosh Plus at 1 MB, System 6.0.8
under MultiFinder, the example accessory at 7a36f68 built
`-DFDROP_DIAG=1` and merged into the System file. The application was
Retro68's WDEFShell sample, which lists the accessories in its Apple
menu and fits beside the Finder at 1 MB; Font/DA Mover has no Apple
menu. The stock disk's INITs were taken out: Suitcase replaces the
accessory list when Option is held.* **This is a fault, and it is not
fixed here.**

Under MultiFinder an accessory chosen with Option held opens in the
layer of the application in front, not the DA Handler's. The sequence:
WDEFShell launched; FinderDrop chosen from its Apple menu with Option
held; DragMe dragged off the desktop onto the accessory's window;
the accessory's window clicked, which brings WDEFShell's layer forward;
Command-Q; then the mouse moved in the Finder and a Finder drag made.
RAM was read at each stage and checked with `goodbyecheck.py`:

| stage | trap table, trampoline | accessory |
|---|---|---|
| WDEFShell in front, accessory open | `_FindWindow`/`_GetMouse` at the trampoline (`$18452`, system heap), both jumps LIVE into the code at `$2572C` | DCE `$1E290` in the system heap, open; storage `$AB5AC`, window `$A9458` and the drop queue `$AB83A` all in WDEFShell's heap (`$A9308`-`$AC2FC`); the code resource locked in the system heap (master pointer flags `$E0`) |
| a file dropped on it | the same | received: DragMe, directory 2 |
| WDEFShell quit; the Finder in front | **the same: LIVE, both jumps into the code**; `FDROP_ENABLE` 1 | the DCE wiped whole -- driver pointer, flags, storage and window all 0; the code still there, still locked |
| the mouse moved; a Finder drag across the desktop | the same | the machine runs; the file moved where it was let go |

**The accessory's Close never ran.** `patch_remove` clears
`FDROP_ENABLE` first and makes the trampoline a pass-through, and
neither happened (read from the code, and the words read 1 and LIVE in
every snapshot after the quit). And the DCE was cleared of more than the
two fields `da_close` clears, so what emptied it was not the accessory.
Nothing on this path called it.

So the patches outlive the host's layer, holding three pointers into a
partition that no longer exists: the window, the queue, and the storage
behind them. The code itself survives -- it is a locked resource in the
system heap, never unlocked or purged after the quit -- so `_GetMouse`
keeps running a sender whose program is gone. Two runs said what that
does:

| after the quit | what the dead pointers held | the Finder's drag |
|---|---|---|
| DragMe dragged to where the accessory's window had been (224,170), on bare desktop | the old window record, untouched: visible, its `visRgn` handle `$A93F4` still describing the window | **swallowed**: recognised (`SEQ` 2), the file found (`step` 4), the release taken as over the window by its visible region (`dg40` 2, `OVER` 1), the entry written into the dead queue at `$AB83A` (`PEND` 1); the Finder was handed the start point, so the icon stayed where it was |
| WDEFShell launched again first, then the same drag | WDEFShell's new heap, which MultiFinder put in the same place (`$A9308`-`$ABAFC`): the byte `over_panel` reads as `visible` held 0, `visRgn` `$01F40001` | the Finder's: `over_panel` returned false at the `visible` test, nothing written, the file moved |

The first shows the drag going wrong and a write into memory nobody
owns. The second shows that once the partition is reused, what happens
depends on the bytes another program leaves at those addresses: here
they happened to stop the test early. With a visible byte set,
`over_panel` would hand a word of that program's heap to `PtInRgn` as a
region handle, and a drop judged over it would be written into that
program's heap. Neither was produced here; the mechanism is read from
the code.

What would close it was named then: an
accessory that asks for the goodbye call (`dNeedGoodbye`) and removes
the patches on it -- not run, because nothing in `fdrop.c` or the example
was changed in this task. Whether MultiFinder sends a goodbye to an
accessory in a layer it is closing had not been checked either. *Since
fixed, in the example accessory: MultiFinder does send it (the entry on
the goodbye kiss, below).*

*How to repeat it:* `mfgoodbye.py --tag NAME --item 70,155 --app-front
300,50` beside `dragshell.py` (outside the repository), with `--after
474,110,224,170` for the swallowed drag and `--relaunch` for the reused
partition; `goodbyecheck.py` reads any snapshot on its own.

### An application launched over an open accessory on the 64K ROM: closed first there too

*Run 2026-11-13 on an emulated Macintosh 128K (64K ROM, 128 KB),
System 1.1 and Finder 1.1g, the example accessory at 7a36f68 built
`-DFDROP_DIAG=1` (11776 bytes) as DRVR 19, the single Finder.*

The run of "An application launched over an open accessory" above, on
the ROM it had not been run on: the accessory opened over the Finder and
moved, DragMe dropped on it, Font Mover double-clicked in the disk's
window with the accessory still open, the mouse moved and clicked in
Font Mover, Quit, and a Finder drag in the disk's window afterwards.
RAM was read at each stage; `goodbyecheck.py` now decodes this ROM's
table of word offsets at `$0400` as the entry on the 64K ROM describes.

| stage | `_FindWindow`, `_GetMouse` (table words) | trampoline at `$1716` | accessory |
|---|---|---|---|
| before it opens | `$6362`, `$5B81`: ROM `$40C6C4`, `$40B702` | -- | -- |
| open | `$860B`, `$860E`: the trampoline, system heap | LIVE, both jumps into the code at `$124B4` in the application heap | unit 19 open, storage `$E0C8`, window `$E654` |
| a file dropped | the same | the same | received: `step` 5, `LASTN` 1 |
| Font Mover running | `$6362`, `$5B81` again | its block FREE (header `$40` → `$00`) | DCE not open, storage and window 0 |
| back in the Finder; a Finder drag | the same | the same | the Finder moved the icon; nothing stopped |

The table words put back exactly, the trampoline's block freed, and the
storage and window cleared are each the accessory's own Close
(`patch_remove` on a ROM with no MultiFinder, and `da_close`), so on the
64K ROM too **the accessory is closed before the application starts**,
with its code still there. As on the Plus ROM, nothing here says which
part of the system makes the close.

The trampoline landed in the system heap on this run, as on every 64K
ROM run before it, so the case where it is in the application heap
itself has still not met a launch.

*How to repeat it:* `goodbye.py --system 1.1 --machine 128K --repo
<checkout> --item 70,171 --move 324,150 --dragme 174,85 --app 80,168
--quit 255,195 --after 80,168,300,120` beside `dragshell.py`.

### An ejected disk on the Finders that drag through the ROM: refused, and no alert, on 1.1 and 3.0

*Replaced on 2026-11-14, as the entry above is: these drags are taken
now (the entry on a drag from an ejected disk).*

*Run 2026-11-13 on an emulated 512Ke (Plus ROM), Systems 1.1 and 3.0
on a 400K MFS floppy, the example accessory at 7a36f68 built
`-DFDROP_DIAG=1`, a second 400K MFS floppy "Floppy" holding one file,
Gone.* The run of the entry on an ejected disk's icon, on the route
through the ROM: the accessory opened and moved, the floppy ejected
with Command-E (its icon and window dimmed), then one drag onto the
accessory. The state words were read out of RAM with the button held
and after the release.

| System, Finder | dragged | step | result |
|---|---|---|---|
| 1.1, 1.1g | Gone, out of the ejected floppy's window | 66, a selected item the File Manager does not agree on | refused; no alert; the icon stayed in its window; nothing queued |
| 1.1, 1.1g | the ejected floppy's own icon, alone | 62, a press on the desktop on no icon of it | the drag is the Finder's: the icon went where it was let go, behind the accessory's window; nothing queued |
| 3.0, 5.1 | Gone, out of the ejected floppy's window | 66 | refused; no alert; the icon stayed |
| 3.0, 5.1 | the ejected floppy's own icon, alone | 68, a disk the File Manager does not know | refused; no alert; the icon went back |

No "Please insert the disk" appeared in any frame, which is what the
refusal by `vcbDrvNum` is for. The 1.1g row with the disk's icon is the
case the earlier entry read from the code: Finder 1.1g's disk record
carries no name, so an ejected disk's icon is not recognised as an item
and the drag is left to the Finder. Under Finder 5.1 the same icon is
recognised as a disk and refused.

Not run: the 64K ROM with an ejected disk; Systems 2.0 to 3.2 other
than 3.0.

*How to repeat it:* `dragshell.py --system 1.1 --machine 512Ke --item
70,107 --move 300,150 --floppy Gone --step d:468,95 --step w:4 --step
c:106,92 --step K:e --step w:5 --at 90,76 --target 290,250 --dump`
beside `dragshell.py`, with `MM_ROOT=<checkout>`; for the disk's icon,
`--step c:470,112 --step K:e --step w:5 --at 454,96` instead of the four
steps before it; on 3.0, `--system 3.0 --item 70,123 --target 300,310`
with the file at `c:56,128` and `--at 40,112`. `--floppy` and `--step`
on the early Systems were added to the script for this run.

### The Finder's own drags off an ejected disk: an alert for a copy and for Empty Trash, none for a move

*Run 2026-11-13 on an emulated 512Ke (Plus ROM) under Systems 1.1 and
3.0, on the setup of the entry above, and on an emulated Plus at 1 MB
under System 6.0.8 with MultiFinder, DA Handler and Backgrounder deleted
from the disk (the single Finder), Gone on an 800K HFS floppy "Floppy"
there. The example accessory at bb4cbb0, built `-DFDROP_DIAG=1` for the
rows with it open.* The baseline the library's refusal is measured
against: no drag here goes to the library. Floppy's window opened and
moved down, Gone selected, the disk ejected with Command-E (its icon and
window dimmed), then one drag per boot. Each result is read off three
frames of that boot taken over about 16 seconds after the release, which
agree.

| System | Finder | gesture | accessory | alert, exact text | Gone ended up |
|---|---|---|---|---|---|
| 1.1 | 1.1g | moved within Floppy's window | closed | none | where it was let go, in the window |
| 1.1 | 1.1g | to the desktop | closed | none | on the desktop; the window says "1 item" |
| 1.1 | 1.1g | into the startup disk's window | closed | "Files remaining to copy 1", and in front of it "Please insert the disk: Floppy", no button; still up in the last frame | in Floppy's window; the startup window's header says "5 items" behind the alert |
| 1.1 | 1.1g | to the Trash | closed | none | out of the window ("1 item") |
| 1.1 | 1.1g | then Special, Empty Trash | closed | "Please insert the disk: Floppy", no button; still up | -- |
| 3.0 | 5.1 | moved within Floppy's window | closed | none | where it was let go |
| 3.0 | 5.1 | to the desktop | closed | none | on the desktop; "0 items" |
| 3.0 | 5.1 | into the startup disk's window | closed | "Files/Folders remaining to copy 1", and "Please insert the disk: Floppy", no button; still up | in Floppy's window; a selected "Gone" icon drawn in the startup window, whose header says "4 items" |
| 3.0 | 5.1 | to the Trash | closed | none | out of the window ("0 items") |
| 3.0 | 5.1 | then Special, Empty Trash | closed | "Please insert the disk: Floppy", no button; still up | -- |
| 6.0.8 | 6.1.8 | moved within Floppy's window | closed | none | where it was let go |
| 6.0.8 | 6.1.8 | to the desktop | closed | none | on the desktop; "0 items" |
| 6.0.8 | 6.1.8 | into the startup disk's window | closed | "Files/Folders remaining to be copied: 1", "Reading: Floppy", and "Please insert the disk: Floppy", no button; still up | in Floppy's window |
| 6.0.8 | 6.1.8 | to the Trash | closed | none | out of the window ("0 items"); the Trash bulged |
| 6.0.8 | 6.1.8 | then Special, Empty Trash | closed | "Please insert the disk: Floppy", no button; still up | -- |
| 1.1 | 1.1g | moved within Floppy's window | open | none | where it was let go |
| 1.1 | 1.1g | into the startup disk's window | open | the same two as without it | in Floppy's window |
| 3.0 | 5.1 | moved within Floppy's window | open | none | where it was let go |
| 3.0 | 5.1 | into the startup disk's window | open | the same two as without it | in Floppy's window |
| 6.0.8 | 6.1.8 | moved within Floppy's window | open | none | where it was let go |
| 6.0.8 | 6.1.8 | into the startup disk's window | open | the same three as without it | in Floppy's window |

So the Finder asks for the disk only when a file's bytes are wanted --
a copy, and Empty Trash -- and moves a file of an ejected disk within
its window, to the desktop or to the Trash without asking; the library's
refusal of the same file (no alert, the icon stays) is quieter than the
Finder's own copy and stricter than its move. With the accessory open
every frame matches the run without it, except one thing: on 1.1 and 3.0
the accessory's window is gone from the screen from the first frame after
the copy's release, and the desktop is drawn where it was; on 6.0.8 it is
still there. Whether the Finder closed it was not looked at. The
accessory's window read "Following the Finder's drag. Received: 0"
wherever it could be brought to the front. No alert had a Cancel button,
so none was pressed; nothing was typed at one either.

Not run: an alert answered (Command-period, or the disk put back); the
accessory open for gestures 2 and 4; MultiFinder.

*How to repeat it:* `ejf-batch1.sh` and `ejf-batch2.sh` beside
`dragshell.py` (each under `emu-gate.sh`), which hold every command line;
`--no-da`, `--no-drag` and the `g:` (a drag) and `p:` (a menu choice)
steps were added to `dragshell.py` for this run.

### System 3.3 on a 512Ke takes a drop from an HFS disk

*Run 2026-11-13 on an emulated 512Ke (Plus ROM, 512 KB), System 3.3
and its Finder 5.5 on a 400K MFS boot floppy, the example accessory at
7a36f68 with no flag (10622 bytes), and an 800K HFS disk Work in the
second drive holding DragMe and DragTwo.* The entry on Finders 5.4 and
5.5 on the 64K ROM, repeated on the Plus ROM, which has HFS in ROM and
so needs the HFS disk without Apple's `Hard Disk 20` file. One boot,
the whole gesture set:

| gesture | result |
|---|---|
| DragMe out of Work's window onto the accessory | received: DragMe on Work (−2), directory 2 |
| DragMe and DragTwo in one drag | both |
| the Finder's own drag of DragMe to the desktop, the accessory open | done |
| DragMe off the desktop onto the accessory | received |
| the Finder's own drag of DragTwo to the Trash | done; the Trash bulged |

Each received item was read off the accessory's window in a frame from
that boot, and `step` 5 and `LASTN` 1, 2 and 1 out of RAM after each
release. The Plus ROM sorts the Apple menu, so the accessory is at
`70,139` here, not the 64K ROM's `70,171`.

Not run: Finder 5.4 on this machine; the smallest build for this ROM
and Finder, which is the one for Systems 4.0 to 6.0.8 alone.

*How to repeat it:* `ITEM=70,139 own64seq.sh NAME 3.3 "" --work
--machine 512Ke` beside `dragshell.py`; `--work` is the HFS disk
without the `Hard Disk 20` file, added to `dragown64.py` for this run.

### The trimmed builds not yet run: the smallest sender on System 2.0 and on a 512K, Systems 4.0 to 6.0.8 alone on 6.0.8, and a folder's stack on the 128K

*Run 2026-11-13 on emulated Macintoshes, the example accessory at
7a36f68 built with the flags given; each row one boot.* What "A build
holds nothing of a switch that is off" left unrun, made. "The smallest
sender" is `-DFDROP_FINDER5=0 -DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0
-DFDROP_ROMPLUS=0` (5476 bytes); "4.0 to 6.0.8 alone" is
`-DFDROP_ROM_ROUTE=0 -DFDROP_RECEIVER=0 -DFDROP_ROM64K=0` (6252 bytes).

| build | machine, System | dragged | result |
|---|---|---|---|
| the smallest sender | 128K, System 2.0 with Finder 1.1 | DragMe out of the disk's window | received: DragMe on Early (−1), directory 0; `step` 5, `LASTN` 1 |
| the smallest sender | 128K, System 2.0 with Finder 4.1 | the same | received, the same |
| the smallest sender | 512K (64K ROM), System 1.1 | the same | received, the same |
| the smallest sender, `FDROP_DIAG=1 FDROP_STK=1` (6308) | 128K, System 1.1 | DragMe put into Empty Folder by the Finder, then the folder | received as the file in it: DragMe, directory 0; room at first look 7087, **low water 6869** |
| 4.0 to 6.0.8 alone | Plus at 1 MB, 6.0.8 under MultiFinder | the disk, DragFolder and DragMe off the desktop in one drag; then Font/DA Mover out of the disk's window | all three, then the fourth: disk (directory 1), folder and file (directory 2) |
| 4.0 to 6.0.8 alone | the same, the single Finder | the same | the same four |

Each received item was read off the accessory's window in a frame from
that boot; the 64K ROM rows' state words are out of RAM snapshots, with
the image in RAM checked against the image built. After the four drops
on 6.0.8 the Finder's own drags were made with the accessory open --
a folder moved inside the disk's window, the window moved by its title
bar -- and each did what the Finder does, under MultiFinder and without.

**The folder's walk is not the deepest drag on the 128K.** 6869 bytes
left at the bottom against 6715 for two files off the desktop
(2026-11-12): 154 bytes more room, so `rom_folder` reaches no deeper
than the files' path does. The low-water words read 0 in the snapshots
before the drag, so the figure is this drag's.

*Three runs did not count and are not in the table:* on System 2.0 the
icon was first pressed where the earlier entry's window had it, which
on this disk is empty space (a selection rectangle, nothing
recognised), and then let go on the lower edge of Finder 4.1's window
rather than on the accessory -- `step` 60, the Finder's own move, and
the full build, run as a control, did the same; and the first folder
run's pre-drags were written for another layout and put the folder over
DragMe instead of DragMe in the folder.

*How to repeat it:* `trun.sh NAME "FLAGS" --system 2.0f1 --machine 128K
--item 70,75 --move 300,150 --target 290,250 --dump --at 154,72` beside
`dragshell.py` with `MM_ROOT=<checkout>`; `--system 2.0 --at 110,91
--target 290,300` for Finder 4.1; `--system 1.1 --machine 512K --item
70,170 --at 154,72`; for the folder, `--system 1.1 --machine 128K --item
70,170 --pre 170,85,42,92 --at 26,76 --target 290,250`; on 6.0.8 the
commands of the entry on the trimmed builds, with the flags above.

### The goodbye kiss: what an accessory gets when its heap goes, and the example now takes it

*Run 2026-11-13 on emulated Macintoshes, each run one boot from a fresh
disk. The measurement used the example accessory at 6a72c4d with two
things added and nothing else: `dNeedGoodbye` in its flags, and a mark
left by Open, by the goodbye call (`_Control`, `csCode` -1) and by
Close -- a sequence number each, and at the goodbye the storage
pointer, `ApplZone`, the first letters of `CurApName` and the DCE's
flags -- written into the code image and into a 44-byte block in the
system heap, which outlives an application heap being reinitialized.
The marks were read out of RAM snapshots. The goodbye call did nothing
else, so what happened to the patches in these runs is the old code's.*

**MultiFinder sends the goodbye to an accessory in a layer it is closing,
and it is the only call that accessory gets.** Everywhere else what
comes is Close:

| where the accessory is, and what happens | System, machine | goodbye | Close | order, and what the mark says |
|---|---|---|---|---|
| opened with Option over WDEFShell, in its layer; WDEFShell quits | 6.0.8 under MultiFinder, Plus 1 MB | **once** | **never** | the goodbye at the quit: storage `$AB5AC` still the accessory's, `ApplZone` `$A9308` (WDEFShell's heap), `CurApName` "WDEF...", flags `$74E0`; then the DCE wiped, as before |
| opened without Option, in the DA Handler's layer; WDEFShell quits | the same | none | none | the accessory stays open and live, and takes a second drop after the quit |
| the same, closed by its close box | the same | none | once | Close alone, `CurApName` "DA Handler" |
| the same, left open; Shut Down | the same | **once** | never | the goodbye, `CurApName` "Finder", storage intact; then "You may now switch off your Macintosh safely" |
| the same, left open; Restart | the same | not measured | not measured | RAM read all `$FF` within a second of choosing Restart: the reboot clears it before a snapshot can be taken |
| open over the Finder; Font/DA Mover launched | 6.0.8, the single Finder, Plus 1 MB | none | once | Close with `CurApName` still "Finder", before the launch |
| open over the Finder; Font Mover launched | 1.1, Plus ROM (512Ke) | none | once | the same |
| the same | 1.1, 64K ROM (Macintosh 128K) | none | once | the same |

**And the same under MultiFinder 1.0**, run the same day on Systems 4.2
and 4.3, each its stock disk booted under its own MultiFinder (the
disk's INITs out) on a Plus at 1 MB, WDEFShell beside the Finder: the
accessory chosen with Option went into WDEFShell's heap (storage
`$AE008`, `ApplZone` `$ABD64`), and when WDEFShell quit it got one
goodbye, `CurApName` "WDEF...", flags `$74E0`, and no Close. With the
marked build, which removes nothing on a goodbye, the fault is there as
on 6.0.8: the trampoline LIVE after the quit, and a Finder drag let go
where the window had been swallowed -- `SEQ` 1 to 2, `PEND` 1, DragMe
left where it was. With the fixed accessory, on both: the trampoline a
pass-through to `$00C794` and ROM `$410FA0`, `FDROP_ENABLE`,
`FDROP_TRAMP` and `FDROP_WIN` 0, `SEQ` still 1, and DragMe moved to
where the window had been, in the frame.

**And under every MultiFinder between**, run the same way on the stock
disks of 6.0, 6.0.2, 6.0.3, 6.0.4, 6.0.5 and 6.0.7, the marked build and
then the fixed one on each -- twelve boots, the same result in all six
pairs:

| System | storage, `ApplZone` at the goodbye | marked build: goodbye, Close; the drag after the quit | fixed: the drag after the quit |
|---|---|---|---|
| 6.0 | `$A7B04`, `$A5860` | one, none; swallowed (`SEQ` 2, `PEND` 1) | pass-through, `FDROP_ENABLE` 0, DragMe moved |
| 6.0.2 | `$ABB30`, `$A988C` | the same | the same |
| 6.0.3 | `$ABB30`, `$A988C` | the same | the same |
| 6.0.4 | `$ADCAA`, `$ABA06` | the same | the same |
| 6.0.5 | `$ADCEA`, `$ABA46` | the same | the same |
| 6.0.7 | `$ADCCC`, `$ABA28` | the same | the same |

`CurApName` read "WDEF..." at every goodbye and the flags `$74E0`. So
the goodbye comes, alone, under every MultiFinder from 1.0 (4.2) to
6.0.8. The stock disks of 6.0.4, 6.0.5 and 6.0.7 list three accessories
and put FinderDrop at y 91 in the Apple menu; the first runs on them
pressed y 139, opened nothing, and are not counted.

So without MultiFinder the system closes the accessory first and sends
no goodbye at all, though the accessory asked for one; the goodbye is
not sent "as well as" Close anywhere measured. Under MultiFinder it
comes alone, for the two moments a layer goes from under an accessory:
its application quitting and the machine shutting down.

**The fix is in the example accessory, and the library is unchanged.**
The header sets `dNeedGoodbye` (`drvrFlags` `$7400`), and `da_control`
answers the goodbye with `fdrop_remove`. Close calls it too; a flag in
the accessory's state makes the second call nothing, whichever order
they come in. On the goodbye nothing is disposed of -- the heap is
going -- and the window is still drawn if an update comes. `fdrop.o`
and `fdrop_asm.o` are byte for byte what they were; the accessory is 60
bytes bigger in every build of the README's table.

Then the same runs again with the fixed accessory, built
`-DFDROP_DIAG=1`, and the library's state words read out of its code
image (`statewords.py`):

| run | what was read |
|---|---|
| the fault's own run: Option over WDEFShell, a drop, WDEFShell quit, DragMe dragged to where the window was (224,170) | the drop received (`SEQ` 1, `step` 5); after the quit the trampoline a pass-through to `$00AB94` and ROM `$410FA0`, `FDROP_ENABLE` 0, `FDROP_TRAMP` and `FDROP_WIN` 0; the Finder's drag NOT recognised (`SEQ` still 1, `PEND` 0) and DragMe **moved** to 224,170 in the frame. The same run before the fix: `SEQ` 2, `PEND` 1, the icon left where it was |
| the same, WDEFShell launched again first (`--relaunch`), its heap at the same place | the same words through the relaunch and the drag; DragMe moved |
| Option over WDEFShell, a drop, the close box, WDEFShell still running | the drop received; then the DCE closed, storage and window 0, the trampoline a pass-through, `FDROP_ENABLE` 0 |

And everything the accessory did before, since its bytes changed -- one
drop each, the right file's name read off its window in a frame from
that boot; built as it ships, but `-DFDROP_DIAG=1` on 4.1:

| System, machine | host's place | received |
|---|---|---|
| 1.1, Macintosh 128K (64K ROM), 128 KB | over the Finder | DragMe, on Early, directory 0 |
| 3.0, 512Ke | over the Finder | DragMe, on Early, directory 0 |
| 4.1, Plus at 512 KB, the stock disk (`SysVersion` `$0410`) | over the Finder | DragMe, on System Tools, directory 2 |
| 6.0.8, the single Finder, Plus 1 MB | over the Finder; then closed by its close box, opened again ("Received: 0") and dragged on | DragMe, then DragMe again in the new window |
| 6.0.8 under MultiFinder, Plus 1 MB | the DA Handler's layer | DragMe, directory 2 |
| 7.5, 4 MB | through the Drag Manager | DragMe, "Through the Drag Manager" |

And the launch over an open accessory, with the fixed one: on System 1.1
on the Plus ROM and on the 64K ROM, and on 6.0.8 with the single Finder,
the drop taken (`step` 5, `LASTN` 1), the accessory closed before the
application started -- the trap table back to its own entries, the DCE
closed -- and the Finder's drag after the return its own. On the Plus
ROM (the 512Ke) the image left behind reads `FDROP_ENABLE` 0. These
three were built `-DFDROP_DIAG=1`.

`examples/check_switches.sh` passes, since `fdrop.h` changed
(a comment). Two trimmed builds of the fixed accessory took a drop each,
read off the window: the smallest sender (5536 bytes) on System 1.1 on
the Macintosh 128K, DragMe on Early, directory 0; and the build for
Systems 4.0 to 6.0.8 alone (6310 bytes) on 6.0.8 with the single Finder,
DragMe, directory 2.

*Two runs did not count and are not in the tables:* the first drops on
1.1 and 3.0 pressed where the batch script's coordinates put DragMe,
which on these disks is empty space (the window read "Received: 0"),
and were made again with the icon's place read off their frames. The
first DA Handler run quit nothing -- Command-Q went to the accessory's
layer, which a click on its title bar had brought forward -- and is
counted only for its close box.

*Not run:* Restart, which the emulator's RAM cannot show (read from
nothing: that it goes through the same Shut Down Manager path is not
measured);
an accessory opened with Option over the Finder, which goes into the
Finder's layer, which does not quit; System 7, where every accessory
has a layer of its own. Nothing here is a real machine.

*How to repeat it,* beside `dragshell.py`, outside the repository:
`mfgoodbye.py --repo <checkout> --tag NAME --item 70,155 --app-front
300,50 --after 474,110,224,170` (`--stock System42.img --item 70,139`
for 4.2, `System43.img` for 4.3, the 6.0.x disks likewise with
`--item 70,91` on 6.0.4, 6.0.5 and 6.0.7; `--relaunch` for the reused partition;
`--no-quit --close-box 50,51` for the close box; `--no-option --app-front
none --app-item 70,273 --special-item 185,123` for Shut Down in the DA
Handler's layer); `goodbye.py` with the arguments of the entries on an
application launched over an open accessory; `statewords.py RUNDIR` and
`statewords2.py RUNDIR` for the state words, and `goodbyecheck.py`,
which also prints the marks of a build that leaves them. The marked
build is a copy of the example with the changes described above; its
source is kept with the evidence, outside the repository.

### A control panel opened with Option over an application: closed before the heap goes

*Run 2026-11-13, System 6.0.8 under MultiFinder, Plus at 1 MB, the
example control panel at 6a72c4d (unchanged since) built
`-DFDROP_DIAG=1`, the only `cdev` in the System Folder.*

The question the accessory's fault left open. Apple's Control Panel
chosen from WDEFShell's Apple menu with Option held opens in
WDEFShell's layer, and the example panel's code was loaded into
WDEFShell's heap (`$AB5AC`, inside `$A9308`-`$AC2FC`), its trampoline
in the system heap. DragMe dropped on it: received (`step` 5). Then
WDEFShell quit -- through its File menu, since with the Control Panel in
front Command-Q went to the Control Panel -- and:

| stage | trap table, trampoline | the panel's words |
|---|---|---|
| open, a file dropped | `_FindWindow`/`_GetMouse` at the trampoline, its jumps LIVE into the panel's code in WDEFShell's heap | `FDROP_ENABLE` 1 |
| WDEFShell quit | the trampoline a **pass-through** to `$00AB94` and ROM `$410FA0`; the Control Panel gone from the unit table | `FDROP_ENABLE` 0, read in the freed heap |
| a Finder drag to where the panel had been | the same | the Finder's: DragMe moved there |

The pass-through and `FDROP_ENABLE` 0 are written by `fdrop_remove`
(`patch_remove`) and nothing else, so the panel's `closeDev` ran before
the heap went. Apple's Control Panel asks for the goodbye itself (its
DCE flags `$3460`); that it is what turned the goodbye into `closeDev`
is read from those flags and not measured. **No change is needed in a
control panel.**

*Two runs did not count:* in both WDEFShell never quit -- once the
switch to it from the Finder's Apple menu missed (without the example
accessory that menu is a line shorter), once Command-Q went to the
Control Panel -- and the patches stayed live with the panel open, as
they should.

*How to repeat it:* `mfgoodbye.py --host cdev --repo <checkout> --tag
NAME --item 70,107 --target 300,180 --app-front 300,43 --quit-menu
70,124 --after 474,110,300,180`.

### The example application's first line says what the library says

Its first line read "No Finder to drag from: run this under
MultiFinder" while it was taking drops under MultiFinder (the aside at
the end of the entry on the Finder's stack under MultiFinder). It now
asks `fdrop_can_drop(window)` from `fdrop.h`, the library's own answer,
instead of looking for `_GetMouse` near its own code. Run 2026-11-13 on
6.0.8 at 1 MB: under MultiFinder the line read "Following the Finder's
drag. Received: 1, room for 4" after a drop of DragMe; with the single
Finder, "No Finder to drag from: run this under MultiFinder. Received:
0", which is true there. *How to repeat it:* the commands of the entry
on an application under MultiFinder, `dragshellapp.py` with `--repo
<checkout>`.

### System 6.0.8L refuses every emulated machine here, at startup; its Finder's code is 6.0.8's, byte for byte

*Run 2026-11-13: Apple's 1.44 MB "6.0.8L System Startup" disk (the
`Z` release), booted with nothing added and nothing taken away. Nothing
of the library ran.*

| machine | what the screen shows |
|---|---|
| Mini vMac, Macintosh Plus, 1 MB | a stop alert in place of the desktop: "This startup disk will not work on this Macintosh model. System 6.0.8L supports the following Macintosh models : Classic, Classic II, LC, LC II, and Powerbook 100." One button, Restart. The same frame from 10 s to 60 s |
| Mini vMac, Macintosh II | the same alert |
| Basilisk II, the Quadra 650's ROM, told to be a Quadra 900 | the same alert |
| the same, told to be an LC (Gestalt 19) with a 68020, and with a 68040; told to be a Classic (Gestalt 17) with a 68030 | a second alert: "This Macintosh is set for 32-bit addressing. You may switch to 24-bit addressing and use this disk or restart and use a newer startup disk. (System 6.0.8L does not work with 32-bit addressing.)" Buttons 24-Bit and Restart |
| the same as an LC, the 24-Bit button pressed | the same second alert again, 10 s later and until the run ended at 90 s |

A RAM snapshot of the Plus at the alert has `SysVersion` 0 and no
application name: the System stopped before either was set. So the
model is checked first and the addressing second, and no machine here
passes both: the Mini vMac builds are a Plus and a Macintosh II, and
Basilisk II with this ROM runs with 32-bit addressing whatever it is
told to be. Not tried: the 128K, 512K and 512Ke builds (the 512Ke has
the Plus's ROM), and QEMU's Quadra 800.

**Read off the disk, not run.** The System file's `vers` 1 is
"Z1-6.0.8L", 1983-92; MultiFinder's is the same; the Finder's is
"Z1-6.1.8", 1983-91. The Finder was compared, resource by resource,
with the Finder 6.1.8 of the 6.0.8 disk every 6.0.8 run here boots:
90 resources in each, and all eleven `CODE` resources are the same
bytes. Four resources differ: `vers` 1 and 2, `MENU` 16 ("Colour" for
"Color") and `STR#` 128. So the drag routine the sender recognises is
there unchanged, and what has not been run is this System file
underneath it.

*How to repeat it:* `boot608.py IMAGE EMULATOR ROM OUTDIR` beside
`dragshell.py` boots a copy of a disk untouched and photographs it
every ten seconds; `bii608.py IMAGE MODELID CPU OUT.png` does the same
under Basilisk II with a model (Gestalt's number less 6) and a
processor, and `bii608b.py` presses 24-Bit. The disk is the one the
survey entry read 6.0.8L's Finder from.

### The Finder's stack under MultiFinder, the rest: a folder in the drag leaves 14555 bytes too, and a drag out of a window 14567

*Run 2026-11-13 on an emulated Macintosh Plus at 1 MB, System 6.0.8
under its MultiFinder, arranged as in the entry on the Finder's stack
under MultiFinder (the startup program set to MultiFinder, the disk's
INITs out), the library as it stood at e6674a1 built `-DFDROP_DIAG=1
-DFDROP_STK=1`.* The two drags that entry lists as not measured. Each
row is one boot of a fresh copy of the disk and one drag; the
diagnostic words were read out of the host's code image in a RAM
snapshot before the drag and after it, and every word read 0 before.

| host | dragged | received | room at `resolve_drag` (dg16/17) | low water (dg18/19) |
|---|---|---|---|---|
| accessory, DA Handler's layer | Font/DA Mover 4.1 out of the disk's window | the file, directory 2 (see below) | 14860 | **14567** |
| accessory | the same, a second boot | the same | 14860 | **14567** |
| application, its own layer | Font/DA Mover 4.1 out of the disk's window | "file Font/DA Mover 4.1 on Macintosh (−1), directory 2" | 14860 | **14567** |
| accessory | DragFolder, alone, off the desktop | "folder DragFolder on Macintosh (−1), directory 2" | 14848 | **14555** |
| accessory | DragFolder and DragMe off the desktop in one drag | the folder and the file | 14848 | **14555** |
| application | DragFolder, alone, off the desktop | the folder | 14848 | **14555** |
| application | DragFolder and DragMe in one drag | the folder and the file | 14848 | **14555** |

Received is the line read off the host's window in a frame from that
boot, with `step` 5, in every row but the accessory's two window drags.
There the Finder's window was in front of the accessory's, and the
frame shows one black square in the strip of it left uncovered; the
name was read out of the accessory's queue in the RAM snapshot taken
after the drop -- volume −1, directory 2, "Font/DA Mover 4.1" -- where
the snapshot before the drag has no such entry.

**A folder costs nothing more:** 14848 and 14555, the figures of every
desktop drag in the earlier entry. On this route a folder is handed
over as a folder and not walked (the entry on folders and disks), so
that is what was expected. **A drag out of a window leaves 12 bytes
more** than a drag off the desktop, 14860 and 14567, under both hosts.
Without MultiFinder the same difference was 40 bytes (the entry on the
Finder's stack under the sender); where the 12 come from was not
looked into. The 293 bytes from `resolve_drag` to the
bottom are the same in all seven. So 14555 is still the least measured
under MultiFinder, and nothing here comes near 4 KB.

*One run did not count:* the accessory's first window drag was made
after a click on the desktop, which under MultiFinder left the DA
Handler in front; the press then went to no icon, and every word still
read 0 afterwards. A click inside the Finder's own window brings it
forward.

Not measured: 6.0.8's MultiFinder at 512 KB; the MultiFinders of 4.3
to 6.0.7 other than 6.0.2's.

*How to repeat it:* `mfstk2.py`, beside `mfstk.py`, is that script
with `--window X,Y` (the disk's window is left open and the drag starts
there), `--wake X,Y` (clicked first), `--folder` and `--front X,Y`:
`mfstk2.py --stock System6.hfv --mem 1M --host da --item 70,171 --files
0 --window 330,110 --wake 22,180 --target 224,268 --tag NAME`; for the
application `--host app --move 240,50,240,270 --wake 490,200 --target
230,310`; for the folder `--files 0 --folder`, and `--files 1 --folder`
with a file.

### The trimmed builds, the rest: each switch off on its own, the application for 4.0 to 6.0.8, every switch off, and the 64K ROM builds with the Trash and an ejected disk

*Run 2026-11-13 on emulated Macintoshes, the library as it stood at
e6674a1, the example accessory built with the flags given unless a row
says the application; each row one boot of a fresh disk.* What the
entries on the trimmed builds left unrun. Nothing here showed a fault.

**Each switch off on its own**, every other at its default, on a System
the switch leaves in and on one it takes out. "1.1" is System 1.1 on a
Macintosh 128K (64K ROM), DragMe out of the disk's window; "3.0" is
System 3.0 on a 512Ke (Plus ROM), the same drag; "6.0.8" is a Plus at
1 MB under MultiFinder, DragMe off the desktop; "7.5" is a Plus at 4 MB,
DragMe out of the disk's window.

| switch off | bytes | where it is left in | where it is taken out |
|---|---|---|---|
| `FDROP_OWN_ROUTE` | 8192 | 1.1: received, DragMe on Early (−1), directory 0. 7.5: "Through the Drag Manager. Received: 1", DragMe on System 7.5, directory 2 | 6.0.8: Received: 0; the icon stayed on the desktop |
| `FDROP_RECEIVER` | 9836 | 6.0.8: received, DragMe on Macintosh (−1), directory 2 | 7.5: Received: 0; the icon stayed in its window |
| `FDROP_FINDER5` | 10152 | 1.1: received | 3.0: installed (`FDROP_ENABLE` 1) and the drag not seen at all, `SEQ` and `STEP` 0 with the button held and after; Received: 0; the Finder moved the icon out of the window |
| `FDROP_ROM64K` | 10164 | 3.0: received, DragMe on Early (−1), directory 0 | 1.1: nothing installed (`FDROP_ENABLE` 0); Received: 0; the Finder moved the icon |
| `FDROP_ROMPLUS` | 9452 | 1.1: received | 3.0: nothing installed (`FDROP_ENABLE` 0), Received: 0, the Finder moved the icon. 6.0.8: Received: 0; the icon stayed on the desktop |

Received is the name read off the accessory's window in a frame from
that boot. On 1.1 and 3.0 the state words were also read out of RAM
snapshots, the accessory found there by its own bytes: `STEP` 5 and
`LASTN` 1 after each drop that was taken. `FDROP_ROM_ROUTE` off alone
was run on 2026-11-12 (the entry on the trimmed builds, run) and was
not repeated. The `FDROP_FINDER5` row on 3.0 is the Plus ROM's case:
that switch also holds the recogniser for the patch System 3 puts in
front of the ROM's routine (the entry on a build that holds nothing of
a switch that is off), which fits a drag never seen; on the 64K ROM
the same build sees it and leaves it (step 64, the last table below).

The sizes say one thing the README's table did not: `FDROP_RECEIVER`
adds 846 bytes to this accessory, where the table had 844. The other
four differences are the table's figures to the byte.

The accessory's first line reads "Following the Finder's drag." in
every frame above but the one that came through the Drag Manager, taken
or not, 7.5 with the receiver out included: with no Drag Manager door
in use the example prints that line whatever `fdrop_install` did. It is the example's wording and not the library's
answer.

**The example application built for Systems 4.0 to 6.0.8 alone**,
`-DFDROP_ROM_ROUTE=0 -DFDROP_RECEIVER=0 -DFDROP_ROM64K=0`, on 6.0.8 at
1 MB under MultiFinder: DragMe off the desktop onto its window gave
"Following the Finder's drag. Received: 1, room for 4" and "file DragMe
on Macintosh (−1), directory 2".

**Every switch off**, all six, 1886 bytes, on 6.0.8 at 1 MB. Nothing
happened, both ways:

| | DragMe off the desktop onto the accessory's window |
|---|---|
| MultiFinder | Received: 0; the icon stayed where it was |
| the single Finder | Received: 0; the icon left where it was, the Finder's own move, as with the sender-less build of 2026-11-12 |

**The 64K ROM builds.** "Smallest" is `-DFDROP_FINDER5=0
-DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0 -DFDROP_ROMPLUS=0` (5536 bytes);
"1.1 to 3.2" is the same with `FDROP_FINDER5` left on (5758). The 128K
runs System 1.1; the others are a Macintosh 512K, 64K ROM.

| build | machine, System | dragged | result |
|---|---|---|---|
| smallest | 128K, 1.1 | DragMe put in the Trash by the Finder, then the Trash | received as the file in it: DragMe on Early (−1), directory 0; `STEP` 5, `LASTN` 1 |
| smallest | 128K, 1.1 | Gone, out of the window of a floppy ejected with Command-E | refused, `STEP` 66 with the button held and after; no alert; the icon stayed in its dimmed window |
| 1.1 to 3.2 | 512K, 3.0 | DragMe put in the Trash, then the Trash | received: DragMe, directory 0; `STEP` 5, `LASTN` 1 |
| 1.1 to 3.2 | 512K, 3.0 | Gone, out of the ejected floppy's window | refused, `STEP` 66; no alert; the icon stayed |
| 1.1 to 3.2 | 512K, 2.1 | DragMe out of the disk's window | received: DragMe on Early (−1), directory 0 |
| 1.1 to 3.2 | 512K, 3.1 | the same | received, the same |
| 1.1 to 3.2 | 512K, 3.2 | the same | received, the same |
| smallest | 512K, 2.1 | the same | installed; the drag seen and left to the Finder (`STEP` 64, `STATE` 3: no record it knows); Received: 0; the Finder moved the icon |

**The Finder's own drag afterwards**, the accessory still open: in the
smallest sender's Trash run and in the four taken rows of the build for
1.1 to 3.2 (the Trash on 3.0, and 2.1, 3.1, 3.2), the Finder icon was
then dragged a hundred pixels across the disk's window. It moved, and
the state words afterwards read `STEP` 60 with `LASTN` still 1: a drag
the sender followed and left alone, nothing queued.

Each taken row was read off the accessory's window in a frame from its
boot. For the Trash on 3.0 and for 2.1, 3.1 and 3.2 that took a second
boot of the same run: in the first the accessory's window stayed behind
the disk's and showed no line, though the state words read the same; in
the second the accessory was clicked forward at the end and
photographed.

Not run: any switch off alone in the application; a real machine. The
rest of this list was run on 2026-11-14 (the entry on the trimmed
builds, the remainder, and for an ejected disk's own icon the entry on
an ejected disk, the rest).

*How to repeat it:* `dragshell2.py`, beside `dragshell.py`, is that
script with `--after` on the early Systems, and `trun2.sh NAME "FLAGS"
ARGS` runs it. The single switches: the System 1.1 command of the entry
on the trimmed builds, run, with `--at 154,72`; `--system 3.0 --machine
512Ke --item 70,123 --move 300,150 --target 300,310 --dump --at 28,88`;
`--system 6 --item 70,171 --at 458,94 --target 224,170 --front
224,120`, with `--single` for the single Finder; and `t75run.sh` with
that entry's System 7.5 arguments. The application: `dragshellapp.py
--repo <checkout> --system 6 --multifinder --mem 1M --app-icon 346,206
--me-desk 458,94 --drag-from 474,110 --target 230,280 --front 300,290`
with the flags in `DRAGSHELL_DEFS`. The Trash: `--system 1.1 --machine
128K --item 70,170 --dump --move 224,200 --target 200,300 --pre
170,88,472,318 --at 456,284 --after 232,105,330,120`; on 3.0 `--system
3.0 --machine 512K --item 70,187`, `--pre 44,104,472,318` and `--after
232,125,330,110 --after c:200,300 --after s:front`. The ejected floppy:
`--move 300,150 --floppy Gone --step d:468,105 --step w:4 --step
c:106,92 --step K:e --step w:5 --at 90,76 --target 290,250` on 1.1, and
`--step c:56,128 --at 40,112 --target 300,310` on 3.0. Systems 2.1, 3.1
and 3.2: `--machine 512K --item 70,187` (2.1), `70,107` (3.1), `70,155`
(3.2), `--move 300,150 --target 300,310 --dump --at 28,88 --after
232,125,330,110 --after c:300,320 --after s:front`.

### On Systems 1.1 and 3.0 the Finder closes an open accessory before a copy that asks for a disk, and its patches go with it

*Run 2026-11-13 on an emulated 512Ke (Plus ROM), Systems 1.1 and 3.0,
the example accessory at 95855d2 built `-DFDROP_DIAG=1`, on the setup of
the entry on the Finder's own drags off an ejected disk.* That entry saw
the accessory's window vanish on 1.1 and 3.0 as soon as the Finder
started copying a file off the ejected floppy, and not on 6.0.8, and
left open whether the accessory had been closed or had lost its window
some other way -- the second would leave the trap patches live with
nothing behind them. RAM was read out of the emulator before the copy,
with the "Please insert the disk" alert up, eight seconds later, and
after a click where the accessory had been:

| System | before the copy | from the copy on |
|---|---|---|
| 1.1 | the accessory's window in the window list; its DCE open, storage $015DC6, window $016354; `_FindWindow` and `_GetMouse` at the trampoline, LIVE, its jumps into the accessory's code | the window gone from the list, the copy's dialog in front; the DCE closed, storage and window 0; both table entries the ROM's own, $4120F6 and $410FA0 |
| 3.0 | the same, storage $018526, window $018AB4 | the same as 1.1 |

The storage and window going to zero are written by the accessory's
Close and nothing else, and the table going back to the ROM's entries
is `patch_remove` without MultiFinder: **the accessory was closed, its
Close ran, and nothing of it is left patched.** Not a fault. The
library's part is the same as in the entry on an application launched
over an open accessory. Nothing else was running (`CurApName` "Finder"
throughout), so the close is the Finder's; which instruction makes it
was not looked for. On 1.1 the DRVR resource itself is purged by then:
the DCE was found by its unit number, 19, its name no longer readable.

*How to repeat it:* `ejt-step0.sh` beside `dragshell.py`, which holds
both command lines; `--step r:NAME` (a RAM snapshot) was added to
`dragshell.py` for it, and `winlist.py RAM` reads the window list and
the accessory's DCE.

### A drag from an ejected disk is taken, and the program's own first call asks for the disk

*Run 2026-11-14 on an emulated 512Ke (Plus ROM) under Systems 1.1 and
3.0 with a 400K MFS floppy, and on an emulated Plus at 1 MB under 6.0.8
with an 800K HFS floppy, single Finder and MultiFinder; the example
accessory built as committed with this entry.* The two entries on an
ejected disk above refused every item of one, so that nothing in the
drag would read the disk. The maintainer's decision: such a drag is
taken like any other, and the disk is asked for afterwards, as the
Finder's own copy asks for it.

**The rule, one for every item and every route.** During the drag an
offline volume -- `vcbDrvNum` 0 -- is read off its VCB and nothing
else. Every volume's name and signature now come off the VCB, online or
not (`vcb_vol`), which is where `_GetVolInfo` read them, and that
replaced two parameter-block calls. An item on such a volume gets every
check its record can answer -- its volume is a volume, it is where its
window or the desktop says, its name fits -- and none that would read
the disk: no catalog, no `_GetFInfo`. It is then queued as the Finder
recorded it and `fdrop_take_drop` hands it over as it is. The program's
own first File Manager call on it is what puts up "Please insert the
disk". So:

| item | route | what is handed over |
|---|---|---|
| a file or folder in the disk's window, or on the desktop | own route (HFS) | its volume, the directory its window shows, its name |
| a file or folder in a FOLDER's window on that disk | own route | the directory its own record names: the window's folder cannot be found by name without the catalog |
| a file, or a folder's files | through the ROM (MFS) | its volume, directory 0, its name; the file-number check is not made on an offline volume; one file in two records is refused |
| the disk's own icon | both | its volume, directory 1 and the name off the VCB -- the whole of its check, so nothing differs; Finder 1.1g's disk record, which has no name, is now recognised by it |

*Why nothing is checked when the drop is taken.* A check at take time
was built first (+708 bytes on the full build, then +128 once it shared
the drag's own calls) and run against a stale record: the floppy
changed on the host while it was out of the drive, then dragged from
the dimmed window, 6.0.8 single Finder, the build that checked and the
build that did not:

| changed while ejected | checked at take | not checked |
|---|---|---|
| `Gone` renamed `Moved` | the alert; the item dropped in silence, "Received: 0" | the alert, from the accessory's own call; "gone Gone on Floppy (-2), directory 2" |
| `Gone` renamed, another file written as `Gone` | the alert; "file Gone", the other file | the same |

and against the Finder's own copy of the same file into the startup
disk's window, nothing of the library involved: the renamed one
brought "The file "Gone" couldn't be read and was skipped (the
file/folder is missing).", Cancel and Continue; the replaced one was
copied without a word, the startup window going from 4 items to 5. So a
check at take could only hide the case the Finder reports, and passes
the one nothing catches. With none, the build is 38 bytes smaller than
before on the full build, and nothing of the library runs while the
alert is up, so whether anything re-enters the program then is the
program's business and was not measured.

*The runs*, each one boot, the floppy ejected with Command-E, the item
dragged out of its dimmed window (or the dimmed disk icon) onto the
accessory's window, each result read off that boot's frames:

| System | Finder | dragged | alert | received |
|---|---|---|---|---|
| 1.1 | 1.1g | `Gone` | none | "file Gone on Floppy (-2), directory 0" |
| 1.1 | 1.1g | `Gone` and `Gone2`, one drag | none | both, directory 0 |
| 1.1 | 1.1g | the disk's icon | none; the icon went back | "disk Floppy on Floppy (-2), directory 1" |
| 3.0 | 5.1 | `Gone` | none | "file Gone", directory 0 |
| 3.0 | 5.1 | `Gone` and `Gone2` | none | both |
| 3.0 | 5.1 | the disk's icon | none | "disk Floppy", directory 1 |
| 6.0.8, single and MultiFinder | 6.1.8 | `Gone` | "Please insert the disk: Floppy" once the accessory took it; gone when the floppy went back | "file Gone on Floppy (-2), directory 2" |
| 6.0.8, single and MultiFinder | 6.1.8 | `Gone`, another floppy put in first | the alert unchanged 6 and 12 seconds after the wrong disk; gone when the right one went in | "file Gone", directory 2 |
| 6.0.8, single and MultiFinder | 6.1.8 | `Gone` and `Gone2` | once | both, directory 2 |
| 6.0.8, single and MultiFinder | 6.1.8 | the disk's icon | none | "disk Floppy", directory 1 |
| 6.0.8, single and MultiFinder | 6.1.8 | a folder, `Fold` | once | "folder Fold on Floppy (-2), directory 2" |

No alert on 1.1 and 3.0 is the example's doing: its `what_is()` asks
nothing about an item in directory 0, and the volume's name it shows
comes off the VCB, so nothing it does reads the disk. A program that
opened the file would put the alert up there too; that was not run.
On 6.0.8 the alert is the accessory's `PBGetCatInfo` in `what_is()`.

*The wrong disk*, read in RAM (6.0.8, single Finder): with the alert
up, the VCB queue holds Macintosh and Floppy, Floppy with drive number 0
and `vcbDRefNum` 2. Three seconds after the other floppy, Other, goes
in, the queue holds a VCB for it too, already with drive number 0 and
`vcbDRefNum` 2, and every drive's disk-in-place byte is 0: the File
Manager mounted it, found it was not Floppy and ejected it, and the
alert stays. Eleven seconds in, the same. Once Floppy goes in, its VCB
is online in drive 2 and Other's VCB has left the queue.

*Step 0 of this work* is the entry before this one: the accessory's
window going away on 1.1 and 3.0 during the Finder's copy is its proper
close.

*Regressions, the same build:* one drop each on System 1.1 on a 128K,
full build and the smallest sender (`FDROP_FINDER5`, `FDROP_OWN_ROUTE`,
`FDROP_RECEIVER` and `FDROP_ROMPLUS` off); System 3.0 on a 512Ke; System
4.1 with the control panel example; 6.0.8, single Finder and
MultiFinder, a disk, a folder and a file in one drag and then the
Finder's own drags with the accessory open, each as before; the build
for 4.0 to 6.0.8 alone on 6.0.8, a folder and a file; System 7.5
through the Drag Manager; an application launched over the open
accessory, and the accessory opened with Option over an application
under MultiFinder -- each received what it received before, and the
trap table afterwards as the entries on those say. Two runs of two
files at first carried one: a rubber band in Finders 1.1g and 5.1 drawn
from where it was drawn selected nothing, and Command-E clears the
selection on 6.0.8; the selection made after the eject is what the
table above records.

Not run: a network volume (`OPEN_ITEMS.md`). The rest of this
list was run on 2026-11-14: the entry below on the ejected disk, the
rest, and the one after it on the size table's builds.

*How to repeat it:* `ejt-step2.sh` and `ejt-step2b.sh` beside
`dragshell.py` hold every command line; the stale record is
`ejt-stale.sh`, the Finder's copy `ejt-fcopy.sh`. `dragshell.py` gained
for this `--step I:floppy` (the disk put back through Mini vMac's own
drag and drop), `--step X:CMD` (a command on the host, with the run's
directory in `WORK`), `--step S:X,Y` (a shift-click), `--floppy-more`,
`--floppy-folder` and `--wrong`; `vcbs.py RAM` reads the VCB and drive
queues.

### An ejected disk, the rest: a folder's window, a program that opens the file, Command-period, every host, the 64K ROM and Systems 2.0 to 4.1

*Run 2026-11-14 on emulated Macintoshes, the library at cd94182, the
example accessory as committed unless a row says otherwise; each row one
boot of a fresh disk, the floppy ejected with Command-E and the item
dragged out of its dimmed window (or the dimmed disk icon), each result
read off that boot's frames.* What the entry above left unrun. **No run
showed a fault.**

**A file in a folder's window**, the case where the directory comes off
the Finder's own record. 6.0.8 on a Plus at 1 MB, an 800K HFS floppy
holding `Gone` and a folder `Fold` with a file `Inside` in it; `Fold`'s
window opened, `Inside` selected, the floppy ejected, `Inside` dragged
out of `Fold`'s dimmed window. The floppy's catalog, read on the host
with hfsutils while it was out of the drive, gives `Fold` node ID 17 and
`Inside` 18 inside it:

| Finder | alert | received, once the floppy went back |
|---|---|---|
| single | "Please insert the disk: Floppy" once the accessory took it | "file Inside on Floppy (-2), directory 17" |
| MultiFinder | the same | the same |

With `Fold`'s window closed after the eject (Command-W), double-clicking
`Fold` in the floppy's dimmed window put up the Finder's own "Please
insert the disk: Floppy" before any window opened; with the floppy back,
`Fold`'s window opened. So Finder 6.1.8 does not reopen a folder's
window off an ejected disk, and a drag out of a window reopened that
way cannot be made.

**A program that opens an MFS file.** The example accessory asks
nothing about an item in directory 0, so on 1.1 and 3.0 no alert had
been seen. A variant built for the harness alone, not in this
repository, also opens such an item with `PBOpen` (read permission) and
closes it, and adds the open's result code to the line, as ", err N";
for any other item it adds `PBGetCatInfo`'s. 512Ke, 400K MFS floppy:

| System, Finder | alert | received, once the floppy went back |
|---|---|---|
| 1.1, 1.1g | "Please insert the disk: Floppy" when the accessory opened the file | "file Gone on Floppy (-2), directory 0, err 0" |
| 3.0, 5.1 | the same | the same |

**Command-period at the alert**, the same variant, the alert up after
the drop:

| System | the alert | the program's call | afterwards |
|---|---|---|---|
| 1.1, 512Ke | gone at once | `PBOpen` returned −53 (`volOffLinErr`): "file Gone on Floppy (-2), directory 0, err -53" | the machine carried on: the accessory's window dragged, the floppy put back and its window undimmed |
| 6.0.8, single Finder | gone at once | `PBGetCatInfo` returned −53: "gone Gone on Floppy (-2), directory 2, err -53" (the example's word for any error) | the same |

**The control panel and the application**, 800K HFS floppy, one file:

| host | System | alert | received, once the floppy went back |
|---|---|---|---|
| control panel | 4.1, Plus ROM at 512 KB | "Please insert the disk: Floppy" | "file Gone on Floppy (-2), directory 2" |
| control panel | 6.0.8, 1 MB, single Finder | the same | the same |
| application | 6.0.8, 1 MB, MultiFinder, its window behind the Finder's | the same | the same, "Received: 1, room for 4" |

**The 64K ROM.** 1.1 on a 128K and 3.0 on a 512K with a 400K MFS
floppy; 4.1 on a 512K with the `Hard Disk 20` setup of the entry on
Finders 5.4 and 5.5 on the 64K ROM, the 800K HFS disk `Work` ejected.
"Smallest" is the smallest sender of the size table. The state words
read out of RAM after each drop: `STEP` 5, `LASTN` 1.

| machine, System | build | dragged | alert | received |
|---|---|---|---|---|
| 128K, 1.1 | full | `Gone` | none | "file Gone on Floppy (-2), directory 0" |
| 128K, 1.1 | full | the disk's icon | none | "disk Floppy on Floppy (-2), directory 1" |
| 128K, 1.1 | smallest | `Gone` | none | "file Gone", directory 0 |
| 128K, 1.1 | smallest | the disk's icon | none | "disk Floppy", directory 1 |
| 512K, 3.0 | full | `Gone` | none | "file Gone", directory 0 |
| 512K, 3.0 | full | the disk's icon | none | "disk Floppy", directory 1 |
| 512K, 4.1 (Finder 5.5) | full | `DragMe` | "Please insert the disk: Work" | "file DragMe on Work (-2), directory 2" |
| 512K, 4.1 (Finder 5.5) | full | the disk's icon | none | "disk Work on Work (-2), directory 1" |

**Systems 2.0 to 4.1 on the Plus ROM**, the full build, one file:

| System, Finder | machine, floppy | alert | received |
|---|---|---|---|
| 2.0, 1.1 | 512Ke, 400K MFS | none | "file Gone on Floppy (-2), directory 0" |
| 2.0, 4.1 | the same | none | the same |
| 2.1, 5.0 | the same | none | the same |
| 3.1, 5.2 | the same | none | the same |
| 3.2, 5.3 | the same | none | the same |
| 4.1, 5.5 | Plus at 512 KB, 800K HFS | "Please insert the disk: Floppy" | "file Gone on Floppy (-2), directory 2", once the floppy went back |

One run of 2.0 with Finder 4.1 pressed where Finder 1.1 puts the icon,
which in Finder 4.1's floppy window is empty space; it is not counted,
and the row is the run made after it.

An alert follows the program's own call and nothing else: no alert
wherever the example asks nothing (directory 0 and directory 1), one
wherever it calls `PBGetCatInfo` or the variant calls `PBOpen`.

*How to repeat it:* beside `dragshell.py`, `all-run.sh` runs one gated
`dragshell.py` run (or, with `--host`, one `allhost.py` run: the
accessory, control panel or application on System 4.1's or 6.0.8's
own disk, with a floppy). `all-batchA1A3.sh` holds the folder's window
and the three hosts, `all-batchA2.sh` the variant and Command-period,
`all-batchA4.sh` the 64K ROM, `all-batchA5.sh` Systems 2.0 to 4.1 (the
2.0 with Finder 4.1 row as re-run: `--step c:125,105` and the drag from
there). The variant is `all-variant/`, beside them; `all-cat.sh` reads
the floppy's catalog while it is out. `dragown64.py` gained `K:`, `X:`,
`I:` (`I:work` is the HFS disk) and `i:` steps for the 4.1 rows.

### The size table's builds, all run again at cd94182

*Run 2026-11-14 on emulated Macintoshes, the example accessory built
with each row's flags; one boot of a fresh disk each, one drop from an
ordinary disk, the result read off the accessory's window in a frame
from that boot.* The README's table of what each build weighs was
rebuilt at cd94182, and its first, second and sixth rows run again then
(the entry on a drag from an ejected disk). The other five, now. No
fault; every size is the table's to the byte.

| row | bytes | machine, System | dragged | received |
|---|---|---|---|---|
| the 64K ROM, Systems 1.1 to 3.2 | 5744 | 512K, 3.2 | `DragMe` out of the disk's window | "file DragMe on Early (-1), directory 0"; `STEP` 5, `LASTN` 1 |
| the 64K ROM, Systems 3.3 and 4.1 | 5868 | 512K, 4.1, `Hard Disk 20` | `DragMe` out of the HFS disk `Work`'s window | "file DragMe on Work (-2), directory 2" |
| Systems 1.1 to 3.2 on any ROM | 7172 | 512Ke, 3.0 | `DragMe` out of the disk's window | "file DragMe on Early (-1), directory 0"; `STEP` 5, `LASTN` 1 |
| System 4.0 and later (a control panel) | 7198 | Plus, 1 MB, 6.0.8, single Finder | `DragMe` off the desktop | "file DragMe on Macintosh (-1), directory 2" |
| the Drag Manager alone | 3064 | Plus, 4 MB, 7.5.3 | `DragMe` out of the disk's window | "Through the Drag Manager. Received: 1", "file DragMe on System 7.5 (-1), directory 2" |

*How to repeat it:* `all-batchB.sh` beside `dragshell.py`, which holds
each command line.

### The trimmed builds, the remainder: the smallest sender on 3.1 and 3.2, the Trash and an ejected disk on 2.1 to 3.2, each switch off on its own under the single Finder and in the control panel, and the accessory's title bar on the 64K ROM

*Run 2026-11-14 on emulated Macintoshes, the library at cd94182; each
row one boot of a fresh disk, each result read off a frame from that
boot.* What the entry on the trimmed builds, the rest, left unrun. **No
run showed a fault.** "Smallest" and "1.1 to 3.2" are the 64K ROM
builds of that entry (5554 and 5744 bytes now); the 512K is a Macintosh
512K, 64K ROM.

**The smallest sender on 3.1 and 3.2**, which holds no records of
Finders 5.0 to 5.3, `DragMe` out of the disk's window onto the
accessory, 512K:

| System | result |
|---|---|
| 3.1 | installed; the drag seen and left to the Finder, `STEP` 64 and `STATE` 3 with the button held and after; Received: 0; the Finder moved `DragMe` out of the window |
| 3.2 | the same |

As on 2.1 in that entry: not a System this build is for.

**The Trash and an ejected floppy with the "1.1 to 3.2" build**, 512K;
the Trash after the Finder put `DragMe` in it, the floppy ejected with
Command-E and `Gone` dragged out of its dimmed window:

| System | the Trash | the ejected floppy |
|---|---|---|
| 2.1 | received as the file in it: "file DragMe on Early (-1), directory 0"; `STEP` 5, `LASTN` 1 | received: "file Gone on Floppy (-2), directory 0"; no alert; `STEP` 5, `LASTN` 1 |
| 3.1 | the same | the same |
| 3.2 | the same | the same |

**Each switch off on its own under the single Finder**, the accessory
on 6.0.8 at 1 MB, MultiFinder deleted, `DragMe` off the desktop:

| switch off | bytes | result |
|---|---|---|
| `FDROP_OWN_ROUTE` | 8178 | Received: 0; `DragMe` left the desktop, the Finder's own move |
| `FDROP_RECEIVER` | 9798 | "file DragMe on Macintosh (-1), directory 2" |
| `FDROP_FINDER5` | 10118 | the same |
| `FDROP_ROM64K` | 10126 | the same |
| `FDROP_ROMPLUS` | 9414 | Received: 0; `DragMe` left the desktop |

Each is the full build less the switch's bytes in the README's table,
to the byte.

**Each switch off on its own in the control panel**, the example panel
in the control panel harness that is not part of FinderDrop (the entry
on a control panel without the ROM route), a file dragged out of a
second disk's window onto it; 4.1 at 512 KB and 6.0.8 at 1 MB, single
Finder:

| switch off | panel bytes | 4.1 | 6.0.8 |
|---|---|---|---|
| `FDROP_OWN_ROUTE` | 7796 | Received: 0; the file left the window | the same |
| `FDROP_RECEIVER` | 9504 | received, directory 2 | the same |
| `FDROP_FINDER5` | 9736 | received, directory 2 | the same |
| `FDROP_ROM64K` | 9744 | received, directory 2 | the same |
| `FDROP_ROMPLUS` | 9032 | Received: 0; the file left the window | the same |

So a control panel needs `FDROP_OWN_ROUTE` and `FDROP_ROMPLUS` on those
two Systems, and nothing else, as the README's table of which switches
a machine needs says.

**The accessory's own window by its title bar on the 64K ROM**, the
smallest sender, System 1.1 on a 128K: `DragMe` dropped (received), the
accessory clicked forward, then its title bar dragged fifty pixels up.
The window moved and stayed in front; the state words afterwards were
those of the drop, `SEQ` 1, `STEP` 5, `LASTN` 1: nothing followed the
title-bar drag. `DragMe` dropped on it again then: "Received: 2", `SEQ`
2, `LASTN` 1. A first run aimed that second drop where the Finder's
window came forward over the accessory with the press; it does not
count, and the figures are the second run's.

*How to repeat it:* `all-batchC.sh` beside `dragshell.py` holds every
command line, the title bar's as counted.

### The sender on a real Macintosh: a Plus under System 6.0.8 takes every drop of the first bench session

*Run 2026-11-14 on a real Macintosh Plus, 1 MB, System 6.0.8 (single
Finder) booted from a floppy, Control Panel 3.3.3, the example control
panel built `-DFDROP_DIAG=1` at 3a00383 copied off the bench kit
(`BENCH.md`, Session 1), the test files copied to the startup
disk, System.* The first time the sender has met hardware: every run
before this was emulated. Read off photographs of the screen, one per
drop:

| dragged onto the panel | received |
|---|---|
| `DragMe`, out of the `Test Files` window | file DragMe on System (−1), directory 50 |
| `DragMe` and `DragTwo`, one drag | both, directory 50 |
| `Thirty-one characters, exactly!` | the whole name, directory 50 |
| `DragMe`, from the desktop | file DragMe, directory 2 |
| `Folder` | folder Folder, directory 50 |
| the startup disk's own icon | disk System, directory 1 |

The count on the first line rose by the items of each drop, to
"Received: 7", with one square per item at the foot of the panel. A file dragged across the panel and let go on
the desktop was the Finder's, and nothing was listed; with the panel
open, a file to the Trash and a window by its title bar behaved as
usual. Those three were reported, not photographed. Directory 50 is
`Test Files`' own number on that disk, which is not known here: it is
the same in every drop out of that window and different from the
desktop's 2, which is what the drops can say.

*The rest of the session, the same day.* After a restart, seven drops
in one visit, every one taken. Then five drops of `DragMe`, the Control
Panel closed by its close box, a file moved in the Finder, the panel
opened again and five more drops: the count started again at 1 and all
five were taken, so the patches came off at the close and went back on
at the open. The plain build, put in place of the diagnostic one, took
its drop after a restart (reported, not photographed).

*A drop on the Control Panel's title bar is taken* (reported). Read
from the code, not run: without MultiFinder the window under the
pointer is found by its structure region (`window_at`), which holds the
title bar; under MultiFinder, a host window in another layer is found
by its visible region (`over_panel`), which does not, so there the same
drop would be the Finder's. Not a fault either way, and not run under
MultiFinder.

*Changed after this session*: a release on the title bar is now the
Finder's without MultiFinder too (the entry on the title bar left to the
Finder, below).

The photographs are kept beside the harness, outside the repository.

### The desk accessory on the same Plus: bench Session 2

*Run 2026-11-14 on the real Macintosh Plus of the entry above, 1 MB,
System 6.0.8 without MultiFinder, the example desk accessory from the
bench kit's suitcase, installed with Apple's Font/DA Mover 3.8.*
Reported, with photographs of the screen.

**The first install bombed, every time it was opened**: "Sorry, a
system error occurred. coprocessor not installed", a line-F exception,
as soon as the accessory was chosen from the Apple menu. Removed with
Font/DA Mover and installed again from the same suitcase, it opened
and worked every time after. So that one copy in the System file was
damaged, and the code was not: on an emulated Plus, Font/DA Mover 3.8
copied the same suitcase into a System file byte for byte (11794 bytes,
compared), and the accessory installed that way opened. What damaged
the first copy cannot be known now that it is gone; a bad read from the
floppy and a startup floppy too full for the System file to grow are
the likely ones, neither checked.

Installed again, the diagnostic build:

| what | result |
|---|---|
| a file, two files, the 31-character name, a file off the desktop, the folder, the startup disk | each taken |
| closed by its close box, a Finder drag, opened again, a drop | taken |
| TeachText launched from the Finder with the accessory open | the accessory closed, TeachText ran; quit, a Finder drag between windows was ordinary |
| Shut Down with the accessory open | ordinary |

The plain build opened and took a drop too.

*The diagnostic window flickers on the machine*, and is steady during a
drag. That build redraws the whole window whenever a diagnostic word
changes, some words change on every `_GetMouse` the idle Finder makes,
and during a drag the Finder runs its own loop and the accessory gets
no time. Cosmetic, and the example's, not the library's. An emulator
screenshot of the same window once came out with its first line
missing, which was such a redraw caught half way.

### MultiFinder on the same Plus: bench Session 3

*Run 2026-11-15 on the real Macintosh Plus of the two entries above,
1 MB, System 6.0.8 with MultiFinder set at startup, the diagnostic
builds of the example desk accessory and application off the bench
kit.* Reported.

| what | result |
|---|---|
| the accessory, chosen from the Finder's Apple menu: a file out of a window, two files, a file off the desktop | each taken |
| a file let go on the accessory's title bar | not taken: the Finder's |
| the example application, the Finder's window clicked forward, a file dragged onto the application's window | taken |
| the accessory and the application open at once, one file onto each | each listed its own |
| TeachText launched, the accessory chosen from TeachText's Apple menu with Option held, a file dropped on it, TeachText quit, a Finder drag let go where the accessory's window had been | the drop taken while TeachText ran; afterwards the file moved, as a Finder drag does |

The last row is the case the goodbye call was added for (the entry on
an accessory opened with Option under MultiFinder), and the first time
it has run on hardware. It needed TeachText: the example application
has no menu bar, so there is no Apple menu of its own to open the
accessory from, and an accessory chosen with Option while the Finder is
in front goes into the Finder's layer, which the application quitting
does not touch.

*The title bar*, as the first bench entry read from the code: without
MultiFinder the window under the pointer is found by its structure
region and a drop on the title bar is taken (Session 1); under
MultiFinder a window in another layer is found by its visible region,
and the same drop is the Finder's. Now measured both ways.

*Changed after this session*: the title bar is now the Finder's in both
arrangements (the next entry). What this session measured is the
library before that change.

### The title bar left to the Finder in both arrangements, the example application's menus, and the diagnostic window redrawn in place

*Run 2026-11-15 on emulated Macintoshes, the library and the examples
as committed with this entry; each row one boot of a fresh disk, each
result read off a frame from that boot.* Three changes:

- **The library**: without MultiFinder, or with the host in the
  Finder's own layer, `over_panel` found the host window by its
  structure region and took a release on its title bar; under
  MultiFinder, a host in another layer is found by its visible region,
  which holds no title bar, and the same release was the Finder's (the
  bench entries on Sessions 1 and 3). Now the window list's answer
  counts only where the point is also in the window's `contRgn`, so a
  release on the title bar or the frame is the Finder's in both. The
  full accessory grew 10644 to 10660 bytes.
- **The example application** has an Apple menu (the accessories,
  opened with `OpenDeskAcc`) and a File menu with Quit, Command-Q going
  through `MenuKey`. Before, it had no menu bar, and Session 3 needed
  TeachText to open the accessory into an application's layer.
- **The example accessory's diagnostic build** redraws the hex rows
  alone, in `srcCopy`, erasing only the tail of each row, where it
  invalidated the whole window whenever a word changed: on the Plus the
  window flickered (the entry on Session 2). The plain build's bytes
  are unchanged.

**The title bar**, a file let go on the host window's title bar after
one let go in its content; diagnostic builds, a Plus at 1 MB (the
4.1 row at 512 KB):

| host, System | the content | the title bar | the library before the change, the same run |
|---|---|---|---|
| the accessory, 6.0.8, single Finder | `DragMe` off the desktop: "file DragMe on Macintosh (-1), directory 2", Received: 1 | Received: 1 still; `DragMe` left its place, and moving the accessory showed it on the desktop where it was let go | Received: 2, the title-bar drop taken |
| the accessory, 6.0.8, MultiFinder (DA Handler's layer) | the same | Received: 1 still; `DragMe` stayed where it was | not run: unchanged code path, measured on the Plus in Session 3 |
| the example control panel, 6.0.8, single Finder | `A Test.img` out of a disk's window: "file A Test.img on MMTest (-2), directory 2", Received: 1 | `B Test.img` let go on the strip of the Control Panel's title bar above the disk's window: Received: 1 still; the disk's window down to 1 item, and the file's icon on the desktop above the Control Panel | Received: 2, `B Test.img` listed |

The two runs of the library before the change are what says the
title-bar runs could fail: the same points, the same harness, and the
release was taken.

**One ordinary drop per route**, plain builds unless said:

| build, bytes | machine, System | dragged | received |
|---|---|---|---|
| the smallest sender, 5590 | 128K, 1.1 | `DragMe` out of the disk's window | "file DragMe on Early (-1), directory 0"; `STEP` 5, `LASTN` 1 |
| everything, 10660 | 512Ke, 3.0 | the same | the same; `STEP` 5, `LASTN` 1 |
| the example control panel, 10278 | Plus at 512 KB, 4.1 | `A Test.img` out of a disk's window | "file A Test.img on MMTest (-2), directory 2" |
| everything, 10660 | Plus, 1 MB, 6.0.8, single Finder | `DragMe` off the desktop | "file DragMe on Macintosh (-1), directory 2" |
| everything, 10660 | Plus, 1 MB, 6.0.8, MultiFinder | the same | the same |
| everything, 10660 | Plus, 4 MB, 7.5.3 | `DragMe` out of the disk's window | "Through the Drag Manager. Received: 1", "file DragMe on System 7.5 (-1), directory 2" |
| the example application, plain | Plus, 1 MB, 6.0.8, MultiFinder | `DragMe` off the desktop onto its own window, the Finder in front | "file DragMe on Macintosh (-1), directory 2", "Received: 1, room for 3" |

**The goodbye runs**, the diagnostic accessory, 6.0.8 at 1 MB:

| run | result |
|---|---|
| an application launched over the open accessory (`goodbye.py`, single Finder) | the drop taken; at the launch the accessory was closed and the trap table held no trampoline of ours; back in the Finder a drag of `DragMe` moved it |
| the accessory opened with Option over WDEFShell, WDEFShell quit (`mfgoodbye.py`) | the drop taken; after the quit the trampoline a pass-through to what it replaced and no `FinderDrop` in the unit table; a Finder drag let go where the accessory had been moved `DragMe` there |

**The example application's menus**, MultiFinder, 1 MB, the
application plain and the accessory diagnostic (`mfgoodbye.py
--app-bin`):

| what | result |
|---|---|
| its Apple menu, Option held | the eleven accessories of the disk, FinderDrop among them, then MultiFinder's list of the Finder and FinderDrop App |
| FinderDrop chosen from it with Option held | opened; it was closed when the application quit (below), which an accessory in the Finder's layer is not, so it was in the application's |
| `DragMe` dropped on it | "file DragMe on Macintosh (-1), directory 2" |
| Quit from its File menu (its one item, "Quit ⌘Q") | back in the Finder; the accessory gone from the unit table, the trampoline a pass-through; `DragMe` dragged to where the accessory's window had been moved there |
| the same, the application's window clicked and Command-Q | the same |

A first File-menu run brought the application forward by MultiFinder's
line in the Finder's Apple menu at the place WDEFShell's menu had put
it; that place is a line lower in the Finder's menu, the click missed,
and the File menu pulled down was the Finder's. It does not count; the
row is the second run's.

**The diagnostic window** of the accessory, 6.0.8, single Finder and
MultiFinder: in two frames taken three seconds apart with the machine
idle, the hex rows were drawn whole and readable and words in three
of the six rows had changed between them; "Following the Finder's
drag. Received: N" and the item line were present in every frame of
these runs that was looked at with the accessory in view. A screenshot cannot show flicker
or its absence; whether the window still flickers on a Plus is for the
bench. On the example control panel, which this change did not touch,
the last hex row is drawn over the first square of the row of squares
at the foot of the window, as it was before.

**The sizes**, rebuilt: every row of the README's size table but the
last grew, by 16 bytes where the build has `FDROP_ROMPLUS` and by 36
where it is the 64K ROM's alone (5590, 5780, 5904); "the Drag Manager
alone" is unchanged at 3064, because it has no sender. `FDROP_ROMPLUS`
now adds 1210 bytes, where it added 1230; every other switch adds what
it did. `check_switches.sh` passes. The diagnostic accessory is 11892
bytes and the diagnostic control panel 11402.

*How to repeat it:* `tb-batch1.sh`, `tb-batch2.sh` and `tb-batch3.sh`
beside `dragshell.py`.

### The example application without MacsBug names

*Run 2026-11-15 on an emulated Plus at 1 MB, System 6.0.8 under
MultiFinder.* Retro68's compiler writes each function's name after it,
for MacsBug; the accessory and the control panel discard those in their
linker scripts, and the application kept them, inline in its text,
because it was not built one section per function. Built with
`-ffunction-sections` and linked with Retro68's `--mac-strip-macsbug`,
its CODE resource went from 27520 to 25088 bytes with no function name
left in it; `MACSBUG=1` builds it as before, byte for byte past the
MacBinary header. Run as built: a file dropped on the application's
window taken ("Received: 1, room for 3"), and the accessory opened from
the application's Apple menu with Option held, a file dropped on it,
File > Quit, then a Finder drag where the accessory had been moved the
file, the trampoline a pass-through and the accessory gone from the unit
table (`mfgoodbye.py --app-bin`, `dragshellapp.py`, beside
`dragshell.py`).

### An SE/30 under Systems 6.0.8 and 6.0.3: bench Session 4

*Run 2026-11-15 on a real Macintosh SE/30, its built-in screen, the
bench kit rebuilt from fddf78e, and a startup floppy made from Apple's
System Tools 6.0.3 disk with its installer folder taken off and the
diagnostic accessory put into its System file (DRVR 21).* Reported:
"all clear" for each part.

| System | what | result |
|---|---|---|
| 6.0.8, no MultiFinder | the diagnostic control panel: a file out of a window, two files, a file off the desktop, the folder, the disk | each taken |
| 6.0.8, no MultiFinder | a file released on the Control Panel's title bar | the Finder's |
| 6.0.8, MultiFinder | the accessory from the Finder's Apple menu, one drop | taken |
| 6.0.8, MultiFinder | the example application: a drop on its window; the accessory opened from its Apple menu with Option, a drop, File > Quit, a Finder drag where the accessory had been | taken; the file moved |
| 6.0.3, no MultiFinder | the accessory opened, a file dropped, closed, twenty times | no crash |

The SE/30's ROM has Color QuickDraw and its processor an instruction
cache: the first three rows are the first sender runs on a machine with
either, and the last the first under a System below 6.0.5 on that ROM,
where `tramp_flush` has no `_HWPriv` to call. What the rows do not say:
whether the Finder's windows were colour ports (nothing records which
branch of `port_origin` ran), and whether the cache was on (not checked
on the machine). `OPEN_ITEMS.md` says what is left of both.
The 6.0.3 floppy was booted on an emulated Plus first, and the same
System with the accessory took three drops there.

### A file server under System 6: an AppleShare volume's file and the volume itself are taken

*Run 2026-11-15 on the real SE/30 of the entry above, System 6.0.8
without MultiFinder, the AppleShare client installed with the 6.0.8
Installer, the diagnostic accessory; the server an LC475 under System
7.6.1 sharing a folder of its own disk by File Sharing, guest access,
over LocalTalk between the two printer ports.* The question was
`OPEN_ITEMS.md`'s: a volume whose VCB has no drive number is
handed over unchecked, as an ejected disk is, and nobody had seen
whether a server volume is one, nor what the File Manager then does.
Read off photographs:

| dragged onto the accessory | received |
|---|---|
| `READ-FIRST.txt`, out of the server volume's window (its name, which names another program, shown here as `<server>`) | file READ-FIRST.txt on <server> (−2), directory 2 |
| the server volume's own icon | disk <server> on <server> (−2), directory 1 |

No alert either time. The accessory's `what_is()` asked the File
Manager about each and was answered ("file", "disk"), so whichever way
the sender treated the volume, the item it handed over was one the
File Manager knows. The volume's drive number itself was not read.
Putting the file on the desktop first could not be done: the Finder
refuses it from a shared volume ("Items from shared volumes may not be
moved onto the desktop"). The open item on a network volume is closed.

### The SE/30's ROM flushes the cache in `_GetTrapAddress`, as the Macintosh II's does

*Read 2026-11-15 out of a stock SE/30 ROM image.* The open item on a
cache with no `_HWPriv` asked whether the IIx-family ROM -- IIx, IIcx
and SE/30, version `$0178` like the Macintosh II's, checksum `$97221136`
-- has `_HWPriv`, and if not, whether its `_GetTrapAddress` flushes the
instruction cache the way the Macintosh II's does (the entry on a
machine without `_HWPriv`, above).

*The image.* Applefritter's SE/30 board repository carries a stock ROM
as four 64 KB chip files, "bitswapped" for its board's sockets. Matched
bit for bit against the Macintosh II ROM, the wiring came out as the
bits of every byte reversed and the chips in the order U4, U3, U2, U1
for byte lanes 0 to 3; rebuilt that way, the image's own checksum --
the 16-bit words from offset 4 to the end, summed -- is `$97221136`,
the value stored in its first long, and its version word `$0178`.

*What it says*, decoded by `rom_traps.py` from the table offset at ROM
`+$22` (`$26EE2`), 719 Toolbox and 201 OS traps, as on the Macintosh II:

| trap | SE/30 ROM | the same as the Macintosh II's |
|---|---|---|
| `_HWPriv`, `$A098` | unimplemented | yes |
| `_CacheFlush`, `$A0BD` | `+$65F2`: `movec cacr,d1; bset #3,d1; movec d1,cacr; rts` | yes |
| `_GetTrapAddress`, `$A146` | `+$65C2`: `move.l d1,-(sp); jsr ([$06F4]); move.l (sp)+,d1` | yes |
| `_SetTrapAddress`, `$A047` | `+$6568`: the same three instructions | yes |
| `_BlockMove`, `$A02E` | `+$EEA2` | yes |

The bytes from `+$6560` to `+$6620` and the table-building code at
`+$D0A` are identical to the Macintosh II ROM's. So on an SE/30 below
System 6.0.5 `tramp_flush`'s `GetOSTrapAddress(_HWPriv)` flushes the
instruction cache through `jCacheFlush` before it answers that the trap
is unimplemented, as on the Macintosh II -- the reason the twenty
rounds under 6.0.3 on the real machine (the entry on bench Session 4)
ran clean. That no System from 4.1 to 6.0.4 patches `_GetTrapAddress`
on ROM `$0178` was read for the Macintosh II entry and holds here, the
patch resource being chosen by the ROM version. The IIx and IIcx carry
this same ROM and were not run.

*Kept outside the repository*, with the script that rebuilt it, beside
`rom_traps.py`.

### Finders 5.4 to 6.1.8 take a drop from an MFS volume

*Built and run 2026-11-15 on emulated Macintoshes; decided by the
maintainer: MFS must be supported on these Finders, since any old 400K
floppy is MFS and a Plus or SE/30 under System 6.0.8 reads one.* Until
now the route through these Finders' own drag routine refused every
item of an MFS volume, at step 96 out of its window and at 88 off the
desktop; and with MFS disks alone, as on a Macintosh 512K without the
`Hard Disk 20` file, it found no desktop and refused every drag at 90
(the entries on an MFS file on the desktop and on Finders 5.4 and 5.5
on the 64K ROM).

**What the Finder's records hold for MFS**, read out of RAM snapshots
taken with the button held, under Finder 6.1.8 (System 6.0.8, Plus,
1 MB), 5.5 (System 4.1 on a 512Ke and on a 512K, System 3.3 on a 512K)
and 5.4 (System 4.0's Finder under System 4.1 on a 512K). The record is
the one the sender already reads, and it is Finder 5.1's (the route
through the ROM's) with the name four bytes further in, so every field
both routes read is at the same offset into the block's data:

| field | into the data | from the name | an HFS item | an MFS item |
|---|---|---|---|---|
| what it is | +0x00 | −98 | 0 file, 4 disk, 8 folder (the desktop's own record too), 12 the Trash | the same |
| its container's handle | +0x0E | −84 | the window's record, or the desktop's | the same; a file in a Finder folder names the folder's record |
| selected, bit 0x02 | +0x15 | −77 | | the same |
| its volume | +0x22 | −64 | | the same |
| its fdFlags | +0x26 | −60 | fOnDesk on the desktop | the same |
| its number | +0x3C | −38 | the catalog node ID (`DragMe` 16, `DragTwo` 17) | the MFS file number (1 to 5 for the five files written in that order) |
| its directory | +0x56 | −12 | the parent's ID; 1 for a disk | **0, for every record of the volume, the disk's included** |

The byte before the selected one read 0x80 on every HFS file and disk
and 0x40 on every MFS one (0x00 on a folder of either); nothing reads
it. A file moved into a Finder folder of an MFS disk keeps directory 0
and its container becomes the folder's record; a file on the desktop
gets fOnDesk and the desktop's container; a file thrown away gets the
Trash's record as its container, as an HFS file does. The disk's own
record is kind 4 with directory 0 and its window's title as its name.

**The rule chosen.** An MFS volume is told by the signature on its VCB
(0xD2D7), read off the VCB so an ejected one is not touched.

- **The desktop** is the container of every disk's record, as before;
  a disk's record is one with directory 1, as before, or one of kind 4
  with directory 0 on an MFS volume (`disk_record`). So it is found with
  MFS disks alone.
- **A file**, out of the disk's window, a Finder folder's window or off
  the desktop, is believed as the route through the ROM believes one:
  `_GetFileInfo` must find that name on that volume with the file
  number the record holds, visible (`mfs_file`, now shared by both
  routes). It is handed over as volume, directory 0, name. Its record
  must be on the window's volume, and its fOnDesk must say whether it is
  on the desktop, as for an HFS file.
- **A window** of an MFS volume needs no directory: the disk's record
  must carry the volume's name, a folder's record must be a folder of
  that volume, and the items in it are checked one by one
  (`container_dir`, `mfs_item`).
- **A Finder folder**, selected in a window (or on the desktop, read
  from the code and not run), arrives as the files in it and in the folders within it, up to 32 deep, by
  the walk up from each file the route through the ROM already makes
  (`mfs_folder`, `kFolderDepth`); past that the drag is refused (step
  69).
- **The disk's icon** arrives as a disk: volume, directory 1, the
  volume's name off its VCB.
- **The Trash** arrives as before -- the startup disk's Trash folder,
  where there is one -- and the MFS files in it arrive as those files.
  Only MFS files are taken out of it: an HFS item thrown away is in the
  Trash's record too, and is not a file to hand over as directory 0.
- **An ejected MFS disk**: nothing is read. Its files are handed over
  as their records say, unchecked, as for an ejected HFS disk.

Nothing about an HFS item changed: the same tests, in the same order,
reached by the same branches.

*The code.* `mfs_file`, `mfs_take` (was `rom_file`), `mfs_folder` (was
`rom_folder`) and `rec_of` moved from `FDROP_ROM_ROUTE` to
`FDROP_SENDER`, with the record's shared offsets (`kItem...`,
`kKind...`), and `name_ends` is the one test both routes know a record
by. The steps 66, 69 and 70 keep their numbers and are both routes'
now (`kStepMfsFileUnknown`, `kStepMfsTooDeep`, `kStepNameTooLong`); no
step was added. `check_switches.sh` moves those names to the sender's
list and adds `mfs_volume`, `mfs_item` and `disk_record` to the own
route's; it passes.

*The bytes*, the example accessory as a `DRVR`, before and after:

| build | before | after |
|---|---|---|
| everything | 10660 | 11166 (+506) |
| the smallest sender (the 64K ROM, Systems 1.1 and 2.0) | 5590 | 5590 |
| the 64K ROM, Systems 1.1 to 3.2 | 5780 | 5780 |
| the 64K ROM, Systems 3.3 and 4.1 | 5904 | 7048 (+1144) |
| Systems 1.1 to 3.2 on any ROM | 7188 | 7188 |
| Systems 4.0 to 6.0.8 alone | 6336 | 7474 (+1138) |
| System 4.0 and later (a control panel) | 7214 | 8316 (+1102) |
| the Drag Manager alone | 3064 | 3064 |

A build with the route through the ROM already carried the MFS code,
so it adds nothing there, and the builds without the own route are the
same size as before. What each switch adds to the full build: `FDROP_OWN_ROUTE` 2972 (was 2466),
`FDROP_ROM_ROUTE` 2348 (was 2944, the shared code having moved out of
it), `FDROP_FINDER5` 486 (526), `FDROP_RECEIVER` 858 (846),
`FDROP_ROM64K` 518 (518), `FDROP_ROMPLUS` 1202 (1210).

*The runs.* Each is one boot of a fresh disk, the example accessory as
committed with this entry, every gesture in the same boot one after
the other, a second 400K MFS floppy `Floppy` holding `Alpha`, `Beta`,
`Gamma`, `Delta` and `Eps`. The folder is made with New Folder in the
floppy's window and `Delta` dragged into it; `Eps` is dragged to the
desktop by the Finder, and later to the Trash. Every result was read
off the accessory's list in a frame from that boot, with the step and
count words out of a RAM snapshot taken with the button down and after
it (step 4 held, step 5 after, every drop). "Taken" means the list
gained exactly the line or lines named:

| gesture | what the list gained |
|---|---|
| a file out of the floppy's window | file Alpha on Floppy (−2), directory 0 |
| two files | file Beta, file Gamma, directory 0 |
| the folder | file Delta, directory 0 |
| a file out of the folder's window | file Delta, directory 0 |
| a file off the desktop | file Eps, directory 0 |
| the disk's icon | disk Floppy on Floppy (−2), directory 1 |
| the Trash, `Eps` in it | file Eps, directory 0 |
| a file of the floppy ejected with Command-E | the file, directory 0; no alert |

| System, machine | Finder | disks | all eight |
|---|---|---|---|
| 6.0.8, Plus at 1 MB, single Finder | 6.1.8 | HFS startup disk and the floppy | taken, `Alpha` off the ejected floppy's window |
| 6.0.8, Plus at 1 MB, MultiFinder | 6.1.8 | the same | taken, the same |
| 4.1, 512Ke (Plus ROM, 512 KB) | 5.5 | Apple's 800K HFS System Tools and the floppy | taken, `Delta` off the ejected floppy's folder window |
| 3.3, 512Ke | 5.5 | MFS alone: the MFS startup floppy `Early` and the floppy | taken, the same |
| 4.1, 512K (64K ROM) | 5.5 | MFS alone | the first seven taken; the eighth not run (below) |
| 3.3, 512K | 5.5 | MFS alone | the first seven taken; the eighth not run |
| 4.1 with Finder 5.4, 512K | 5.4 | MFS alone | the first seven taken; the eighth not run |

The ejected floppy's VCB read drive number 0 in the snapshot taken
during that drag, in all four of those runs, so the item was handed
over as its record said and nothing asked for the disk.

**On the 64K ROM an MFS floppy cannot be ejected and dragged from.**
Command-E brings the Finder's own "Please insert the disk: Floppy" at
once -- with the floppy's window open or closed, and with no accessory
open at all (three runs) -- so there are no dimmed icons to press. The
same floppy on the Plus ROM ejects quietly. Not the library's, and the
case is `OPEN_ITEMS.md`'s now.

*Mixed*, 6.0.8 single Finder, the HFS startup disk's `DragMe` and the
floppy's `Eps` both on the desktop: `Eps` alone, file Eps on Floppy
(−2), directory 0; `DragMe` alone, file DragMe on Macintosh (−1),
directory 2; both selected together and dragged as one, both lines,
each with its own directory. Both then thrown away and the Trash
dragged: file Eps, directory 0, alone -- `DragMe`'s record had the
Trash as its container too, read in RAM, and is left to the Trash
folder, which System 6 has not got. An HFS file and an MFS file in one
window cannot be, so that is the only way the Finder lets the two be
dragged together.

*The controls*, the library before this change (a25c473), the same
gestures on the same disks, by their steps read from RAM:

| System, machine | out of the window, two, the folder, the folder's window | off the desktop, the disk's icon | the Trash | ejected |
|---|---|---|---|---|
| 6.0.8, Plus, single Finder | 96 each | 88 each | 85 | 96 |
| 4.1, 512K, MFS alone | 90 each | 90 each | 90 | (not run) |

and the list said "Received: 0" at the end of each. So every case in
the tables above could have failed, and did before.

*Regressions*, the same library, each one boot of a fresh disk, read
off that boot's frames; every one received what the same run received
before this change, line for line where the earlier run is on record:

| run | build | received |
|---|---|---|
| 6.0.8, single Finder and MultiFinder: the disk, a folder and `DragMe` in one drag off the desktop, then four of the Finder's own drags | full | disk Macintosh, directory 1; folder DragFolder and file DragMe, directory 2; the Finder's drags as before |
| 4.1, 512K, `Hard Disk 20`: a file and two files out of the HFS disk `Work`'s window, the Finder's drag to the desktop, the file off the desktop, the Finder's drag to the Trash | full | `DragMe`; `DragMe` and `DragTwo`; `DragMe`; all directory 2 |
| 4.1, Plus ROM at 512 KB, the control panel | full | file A Test.img on MMTest (−2), directory 2 |
| 1.1, 128K, through the ROM | the smallest sender, 5590 | file DragMe on Early (−1), directory 0 |
| 3.0, 512Ke, through the ROM | full | file DragMe on Early (−1), directory 0 |
| 7.5.3, Plus at 4 MB, through the Drag Manager | full | "Through the Drag Manager. Received: 1", file DragMe, directory 2 |
| the goodbye runs: an application launched over the accessory (6.0.8), and the accessory opened with Option over one under MultiFinder | full | the trap table, the trampoline and the unit table at each stage as before: pass-through once the accessory had gone, no driver left |
| the title bar, single Finder and MultiFinder: a drop on the content, then a release on the title bar | diagnostic, 12392 | one line; the title-bar release the Finder's |
| 3.2, 512K | the 64K ROM, 1.1 to 3.2 (5780) | file DragMe on Early (−1), directory 0 |
| 4.1, 512K, MFS alone | the 64K ROM, 3.3 and 4.1 (7048) | file Alpha on Floppy (−2), directory 0 |
| 4.1, 512K, `Hard Disk 20` | the same | file DragMe on Work (−2), directory 2 |
| 3.0, 512Ke | 1.1 to 3.2 on any ROM (7188) | file DragMe on Early (−1), directory 0 |
| 6.0.8, single Finder: a folder and a file off the desktop | 4.0 to 6.0.8 alone (7474) | folder DragFolder, file DragMe, directory 2 |
| 6.0.8, single Finder | 4.0 and later (8316) | file DragMe on Macintosh (−1), directory 2 |
| 7.5.3 | the Drag Manager alone (3064) | file DragMe, directory 2 |

Not run: a real Macintosh (`BENCH.md`, Session 7); an MFS
floppy ejected on the 64K ROM (above); the Finder's stack under
`FDROP_STK` on the new path, whose deepest call is two frames
deeper than the HFS one's (`mfs_item`, `mfs_folder`, `mfs_take` and
`mfs_file`, against `takeable` and `cat_entry`); a folder nested more than one deep, and one past 32; a
name longer than 31 on an MFS floppy, on these Finders; System 4.0 itself, which
does not boot; a control panel or the application on an MFS floppy,
though they call the same library.

*How to repeat it:* `mfsown/` beside `dragshell.py`: `run6.sh` (6.0.8,
`single` for the single Finder), `run41.sh` (4.1 on the 512Ke) and
`run64.sh` (3.3 and 4.1 on the 512K or 512Ke, MFS alone), each reading
its steps from one of the `ctl*` files there; `batch1.sh` and
`batch2.sh` are the controls, the mixed run and the regressions.
`dragshell.py` gained `--mfs` (the `--floppy` disk MFS on System 6),
`--stock` (another vmac disk in place of System6.hfv) and a `P:` step
(a drag with RAM held mid-drag); `dragown64.py` gained `--floppy`.
`recdump.py` there prints every byte of each record before its name.

### A quick drag is taken: a release inside the drag routine's first pass, or between two calls of a pass

*Found on a real Plus (6.0.8 single Finder, and 4.0's Finder 5.4), one
drop in three to five; reproduced and fixed 2026-11-17 on an emulated
Plus at a Plus's speed (`mv-1M-1x`).* A quick flick of an icon onto the
accessory went through it to the desktop, or moved the icon within its
own window, with step 1 left in the step word and nothing else written:
the bench photograph read `SEQ*100+STEP` 1601.

**Why.** The drag was recognised at the action procedure's
`_FindWindow`, which comes AFTER that pass's `_GetMouse`, and the outline's
call site was learned afresh in every drag from the pass before the
first answered one. So the first pass of every drag was the Finder's
whatever the pointer was over, and a drag released before a second pass
(the button up before the drag routine's first action procedure, on a
quick flick) ended with the host window taken for the desktop. Read in
RAM after such a drop: `FDROP_GEST` equal to the release's `MBTicks`,
`FDROP_OUTSITE` 0, `FDROP_LASTGM` the final call's site, one recognition.
And the release itself moved `MBTicks`, which `fdrop_look` read as a new
drag and `fdrop_answer` refused in the action procedure: a release between
a pass's calls threw away what that pass had answered (read from the code;
not caught in a run).

**The fix** (`same_gesture`, `drag_arm`). The release of the press being
followed is not a new drag (the button up, later than the press, the final
call not yet seen, the same start point in the frame). A new drag of the
routine already recognised is armed at its FIRST action-procedure
`_GetMouse`: the frame's action procedure must hold the call site
recognised before (`finder_drag_call` on `FDROP_FWRET`) and the call must
return into it; the outline's site learned in that routine is kept. The
action procedure's call is answered held or released. No step added.
The very first drag after the accessory opens is still recognised only
at its `_FindWindow`, as before.

*The runs*, 6.0.8 single Finder, diagnostic accessory, `DragMe` on the
desktop to the accessory, `F:` steps (`dragshell.py`: press, N moves DT
apart, HOLD, release; 2 to 6 ticks press to release): before (d3e30fd),
the first quick drag went to the desktop, step 1, the icon lost under the
window; after, twelve quick drags of nine timings: ten taken (Received
10, `SEQ` 10, step 5 each), two too quick for the Finder to call a drag
(`SEQ` unchanged, the icon not moved). `fastdrag/` beside `dragshell.py`
repeats it (`run.sh`, `st.py` reads the state words).

**TeachText refused three times** at the bench, then taken: nothing in
the code refuses an application (`takeable` asks name, place, visibility).
A snap-back with nothing taken is a refusal whose step was not photographed;
not reproduced.

*The bytes*: the example accessory 11360 (+194), Systems 4.0 to 6.0.8
alone 7674 (+200), a control panel 8516 (+200), the diagnostic build
12616; builds without the own route unchanged (5590, 5780, 7188, 3064).

*Regressions*, the previous entry's set (`regress.sh` in `fastdrag/`), each one
boot: the MFS gesture runs on 6.0.8 single Finder and MultiFinder, 4.1 on
the 512Ke and Finder 5.4 on the 512K, step and count words per drop equal
to the earlier runs' line for line; the HFS 6.0.8 runs both ways, 4.1 with
`Hard Disk 20`, the control panel on 4.1, the ROM route (1.1 smallest,
3.0 full), 7.5.3 through the Drag Manager and the trimmed builds, every
frame pixel-identical to the earlier run's but for the clock; the title
bar both ways, the list identical; both goodbye runs, the same stages.
Not run: a real Macintosh; a quick drag on Finder 5.4 or under
MultiFinder; the release between a pass's calls, alone.

## System 4.0 and an MFS floppy on a real Macintosh Plus

Bench, 2026-11-17, the diagnostic accessory from d3d69d7 on the
`FinderDrop 4.0` floppy (System 4.0, Finder 5.4). Taken: a file, two
files, the 31-character name, a folder, a file from the folder's
window, a file off the desktop, the disk; refused on the title bar;
taken again after a close and reopen. From an MFS floppy: a file, a
file from a folder's window, the disk, several
files, a folder and the Trash each arriving as the files in them, and
ejected (dimmed) files taken with no disk asked for, the accessory
reading none. Quick drops: about twenty after d3d69d7, none reported going through, against
one in three to five before it (System 4.0 and 6.0.8). Two drops in
quick succession on the same icon open it as a double-click, which
the Finder alone does not do after a drag (`OPEN_ITEMS.md`).
A selection of six arrived in the Finder's record order, 5 4 3 2 1 6
for a row of six selected right to left.

## Every build against Systems it was not built for

2026-11-19, emulated, diagnostic accessory from 3e32087: the eight builds
of the README's size table, each on 1.1 (128K), 3.0 (512Ke), 6.0.8 single
Finder and MultiFinder, and 7.5.3. No build took a drop it has no route
for, none crashed, and every control was taken. A build patches on any
ROM it supports whatever the Finder (kDgInstall 1, 3 or 5 with the route
off: the 1.1-3.2 build on 6.0.8, the 4.0-6.0.8 and control-panel builds
on 3.0, the 64K 3.3/4.1 build on 1.1), and those patches let the drag
through. Builds with no route for MultiFinder or the Drag Manager answer
kDgInstall 8. Gaps filled 2026-11-20 with the same accessory: 4.1 on the
64K ROM (dragown64, HFS disk), 1.1 on the Plus ROM (a 1 MB Plus), and the
Drag Manager-only build on every System (the checker now reads
`fdrop_install`, which that build has). Controls all taken; the Drag
Manager-only build took a drop on 7.5.3 only and left kDgInstall 0
elsewhere. Not as expected: on 4.1 on the 64K ROM the smallest sender, the
64K 1.1-3.2 build and the 1.1-3.2 any-ROM build answer kDgInstall 7 and
take nothing (the 64K ROM route patches whatever the System; the trap
table is not readable there). On 1.1 on the Plus ROM the 4.0-6.0.8 and
4.0-and-later builds answer kDgInstall 1 and take nothing, with the trap
table changed, like the earlier rows on 3.0. Not run: 7.1 without the
Drag Manager (the share holds install disks, not a bootable image) and
the comparison of the Finder's drag with a run without the library.
Script and table: kept with the harness, outside the repository.

## The 64K ROM's table is recognised by any trap still in the ROM

2026-11-20, emulated 128K, System 1.1, diagnostic accessory: a counting
pass-through (`addq.l #1`, `jmp` to the ROM's routine) written into the
top of the system heap and `_GetMouse`'s word entry pointed at it before
the accessory opened. `rom64_present` once decoded `_GetMouse`'s entry
alone, so that installed no sender; it now takes `_GetMouse`,
`_FindWindow` or `_Button`, whichever still decodes into the ROM. Full
and smallest builds: the drop taken (`LASTN` 1), the foreign counter
rising throughout, and on close the entry back to the foreign patch's
word, still counting. A second copy now installs too and chains to the
first (the 64K branch of `patch_remove` keeps a covered trampoline as a
pass-through); that is read from the code, not run. Not run: System 2.0,
whose run with these coordinates took no drop with or without the patch.
Cost: 54 bytes (48 in the 3.3/4.1 build).

## Two copies at once on the 64K ROM (run)

System 1.1 on a 512K 64K-ROM machine (a 128K machine says "not enough
memory" to the second accessory), the example accessory twice with
`-DFDROP_DIAG=1`, as DRVR 19 "FinderDrop" and DRVR 20 "FinderDrop 2".
Both report kDgInstall 7 (patched, word table, ROM route) with distinct
trampolines. Open A, open B; a drop on A's own part of the screen was
taken by A only, one on B's by B only; close A, drop on B taken; reopen A,
drop on A taken; close B first, drop on A taken; reopen B, drop on B
taken; close both, then a plain Finder drag moved DragMe: _GetMouse still
works. No crash at any close. _GetMouse's table entry led to the newest
copy's trampoline (read: B's, then A's after A's reopen). Not read: the
chain from there down to the ROM (the harness chain walk stopped at the
first node), and the other copy's count after each drop was read from
the diag words, not the windows' item lists. 128K and the 3.3/4.1 own
route were not run.

### Three checks, 2026-11-20 (4e5e2b7, emulated)

*The own route's stack on MFS*, `-DFDROP_DIAG=1 -DFDROP_STK=1`: room at
`resolve_drag` / low water. 6.0.8 single Finder, Plus 1 MB: file from an
MFS floppy's window 31242 / 30955; files from a Finder folder's window
31242 / 30849. 4.1 at 512 KB (512Ke): 31428 / 31141, folder 31035. HFS
on the same build, 6.0.8: 31202 / 30911 (the older figures above, 31398
and 31155, predate later code). MFS is no deeper than HFS beyond about 60
bytes; 4.1 has 186 more room than 6.0.8. 4.1 HFS not re-run.

*Quick drags under MultiFinder*, 6.0.8, thirteen drags from an MFS
floppy's window at the `fastdrag/` timings: 13 taken (`SEQ` +1 and step 5
each), none through, none counted as a click; room 14858, low 14571. The
desktop-icon variant of those step files never reached the accessory
under MultiFinder (Received 0, even the slow drag); not investigated.

*Two copies, 64K ROM, System 1.1 at 512K*: `_GetMouse` and `_FindWindow`
both run entry, newest copy's trampoline, older copy's trampoline, ROM
(`_GetMouse` enters at the trampoline +6). Closing a copy, in either
order, changes nothing: its trampoline stays in the chain, disabled.
Reopening a copy puts a new trampoline on top; closing both leaves the
two first trampolines and drops the reopened ones.

## A desktop drag under MultiFinder, and a drag of one tick

2026-11-20, emulated 6.0.8: a file dragged slowly off the desktop onto the
diagnostic accessory is taken under MultiFinder as under the single Finder
(step 5, kDgEnd 101); an earlier "Received 0" was the harness
(the harness's quick-drag script always passed --single). A drag of three
moves in 0.02 s, about one tick, is not taken under MultiFinder: step 1,
kDgEnd 1, released before the pointer was ever read over the window. That
is faster than a hand; the quick drags that were taken lasted 2 to 6
ticks.

## _GetTrapAddress names the registers an OS trap may trash

2026-11-20, from an independent review: the one inline trap call,
`_GetTrapAddress` in `trap_addr64` (64K ROM only), declared D0 and A0 and
not D1, D2 and A1, which an OS trap may trash (`fdrop.h`) and the 64K
ROM's routine reaches through a RAM vector. Declared now; +6 bytes in the
full accessory. Run on an emulated 128K under System 1.1, full and
smallest builds: installed (kDgInstall 7) and the drop taken (step 5).
The review found nothing else above low severity.

### An MFS file in two records is refused, and the startup Trash is found off the VCB

`mfs_take` now asks the same question the HFS path does (the same volume, directory 0 and name already queued: `kStepOneItemTwice`), on both routes, since the own route's `mfs_item` and the ROM route's sender both go through it. `startup_trash` reads the startup volume's signature off its VCB instead of calling `PBHGetVInfo`, which the 64K ROM lacks: no Trash on an MFS-only startup disk, by design rather than by an unset field, and the Trash when the Hard Disk 20 HFS patch is loaded.
