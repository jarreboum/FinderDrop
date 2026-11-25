# History: MacDrag, the probe that came before the library

Before there was a drag library there was a question, and a throwaway
control panel called **WProbe** built to answer it, under the working name
**MacDrag**. This is that probe's notebook, kept as it was written between
the first run and the day the sender went into the control panel it was
built for. It records how the library's mechanisms were found — the
hypotheses that were wrong, the bytes each stage cost, the false
negatives — in the order they happened.

**It is history, not reference.** The code has moved on since: names
here are the probe's (`wp_track`, `patch.S`, `WProbe-min.bin`), sizes are
the probe's builds, and where a fact still holds it is in `NOTES.md`,
which is the index to believe. Where this disagrees with the code or with
`NOTES.md`, this is the stale one. The "run" lines name the probe's own
harness (`tools/probe.py`, `tools/vmac/`), which is not published.

What is changed from the notebook: the host program is not named, and
references into that program's own files and design rules are written
out as what they said. Nothing else.

---

## What has actually been run, and where

This file holds results. Nothing is a claim here unless a run produced it,
and where a run bounds a claim, the bound is written down beside it. What
is *not* here has not been measured.

---

## The question

MacDrag exists to answer one thing before anything is built:

> When the user presses on an icon in a System 6 Finder window, can code
> outside the Finder find out **which file that is**?

If it can, a Drag Manager for System 6 is reachable from public API — the
window says which folder, the folder's catalog says where each file's icon
sits (`fdLocation`), and the mouse says which icon. If it cannot, the only
route left is patching the Finder's own mouse handling, which is a
different project of a different size.

---

## The hypothesis, and the answer

**Hypothesis:** a System 6 Finder window's `WindowRecord.refCon` holds a
working directory reference number for the folder the window shows.
System 6 addresses folders by `wdRefNum` everywhere else, so it is where a
Finder of that period would naturally keep one.

**Answer: no. Falsified, twice over.**

*Run:* Macintosh Plus, System 6.0.8, 1 MB, Mini vMac. Stock `System6.hfv`
with the System Folder stripped to System and Finder, a folder named
`Alpha` at the volume root, and WProbe as the only control panel. Two
Finder windows opened by hand — the volume window `Macintosh` and the
folder window `Alpha` — then the Control Panel from the Apple menu.

*Where:* `tools/probe.py`, photographs in `build/run/`.

```
windows: 4
0 "Control Panel" k-16 vis   rc=0002A470  0003 2F2C 0002 ED86   none
1 "Alpha"         k16  vis   rc=0002A540  0002 E890 0002 E900   none
2 "Macintosh"     k16  vis   rc=0002A578  0002 C198 0002 F946   none
3 "Clipboard"     k17  hid   rc=00000007  FF00 4011 2800 4011   none
```

**The refCon is a pointer, not an identifier.** `0002A540` and `0002A578`
are 56 bytes apart in the application heap, and the two Finder windows'
values differ by exactly one allocation. Read as a `wdRefNum` every one
returns `-35`, `nsvErr`; read as a directory ID on the boot volume every
one returns `-43`, `fnfErr`.

**And there is no identifier one hop behind it either.** The first 64
bytes of the block each refCon points at were searched for anything that
*resolves* — every negative short tried as a `wdRefNum`, every long
between 2 and 65535 tried as a directory ID, and a candidate counted only
if the File Manager turned it into a folder **name**. `none`, on every
window. The first eight bytes printed above are why: they are more
pointers, into the same heap.

**What the run does not say.** It searched one hop and 64 bytes. A
directory ID two allocations away would not have been found. So the
bounded claim is: *the folder a System 6 Finder window shows is not
reachable from the window record, nor from the first 64 bytes of the
block the refCon points at.* That is enough to close the cheap route and
not enough to prove the expensive one impossible.

**Why the search resolves rather than recognises.** A number that looks
like a directory ID proves nothing — that is a plausible wrong answer,
which is worse than none. Every candidate is turned back into a name, and
the name is printed beside the window's own title, so a hit is only a hit
if `Alpha`'s window says `Alpha`.

---

## Incidental: a System 6 cdev needs a Finder bundle to be listed at all

Not the question, and it cost two runs, so it is written down.

A Control Panel document holding `cdev`, `nrct`, `mach`, `DITL` and `ICN#`
— all at ID −4064, all correct, file type `cdev` — was **not listed** by
the System 6.0.8 Control Panel. The alert is *"There are no Control Panel
files in the System Folder."*, which reads as the file being absent rather
than being rejected.

*Bisected:* adding a signature resource matching the file's creator, an
`FREF` for `'cdev'` and a `BNDL` tying them together makes it appear.
Adding only a second `ICN#` at ID 128 does not. So **the signature / FREF
/ BNDL trio is what the Control Panel requires**; whether the `ICN#` at
128 is also needed was not separately tested, and it is cheap to keep.

*How it was found:* the host program's own cdev was installed through this same
harness and listed correctly, which is what proved the harness sound and
moved the search to the resource fork. Bisecting against a known-good file
was worth more than reading about it.

---

## The hard way: watching the Finder through a trap patch. **It works.**

Three probes failed to find the folder by asking (above). This one finds
it by watching, and the result is positive.

*Run:* same machine and disk. The order is the experiment and it is the
opposite of the runs above — the Control Panel is opened **first**, which
is when the patch installs, and only then is the Finder asked to open
windows. `tools/probe.py`, photographs in `build/run/`.

```
windows: 4
0 "Control Panel" k-16 vis rc=0002A508
1 "Alpha"         k16  vis rc=0002A540
2 "Macintosh"     k16  vis rc=0002A578
3 "Clipboard"     k17  hid rc=00000007
old=000087B4 all=17 cat=17
 sel: 48 9 9 9 9 9 9 9 9 9 9 7 7 9 9 7 7
 i=2  d=000000DB Alpha          <- most recent first
 i=1  d=000000DB Alpha
 i=-1 d=000000DB Alpha
 i=7  d=00000002 Macintosh
 i=6  d=00000002 Macintosh
 ...
```

**The Finder announces the folder, on a public trap, with a directory ID.**
Opening a window is `_HFSDispatch` selector 9 — `PBGetCatInfo` — first
with `ioFDirIndex = -1` on the folder itself, then `1, 2, 3 …` walking its
contents, one call per item. That shape is distinctive: it is how "a
window is being opened" is told from other File Manager traffic.

**And it resolves to the right name.** `0x000000DB` is 219, which
`PBGetCatInfo` names **`Alpha`** — and `Alpha` is the title of the window
that appeared. Not directory ID 2, which is every HFS volume's root and
the false positive the earlier scan produced; a distinct ID read from a
known offset in a known parameter block.

**What this proves, exactly.** The fact a drag sender needs — which folder
a Finder window shows — is obtainable from outside the Finder, using only
published traps, with no Finder internals in it. Combined with
`fdLocation`, which puts each file's icon at a known point in that folder,
the press-to-file mapping is reachable.

**What it does not prove.** Two things, and neither is small:

- *The pairing.* This shows a burst of calls and, separately, a window
  that appeared. Nothing here ties the burst to that specific
  `WindowPtr`. A second patch on `_NewWindow` / `_GetNewWindow` would do
  it, and the check is already available — resolve the directory ID to a
  name and compare it with the window's title, which is what these runs
  print side by side.
- *The vehicle.* This is a control panel, and the patch lives exactly as
  long as the panel is open, which is what makes it safe. The real thing
  is an `INIT`: resident code in the System 6 system heap, patching the
  File Manager for the whole session. That is a different set of costs
  and none of them is measured here.

### Two false negatives, both of which photographed as "nothing happened"

Worth more than the positive result, because each one produced a clean
screen with a zero on it and no hint that the run was broken.

1. **`nulDev` drew, and drawing drained the ring.** A desk accessory gets
   periodic time whether or not it is in front, so the panel was taking
   and clearing the log several times a second — including throughout the
   moment the Finder was opening the window. Reported `cat=0`.
2. **Coming to the front draws twice.** An update and an activate arrive
   together, and a read that cleared handed the first one everything and
   the second one an empty ring. The photograph is of the second.
   Reported `cat=0` again, for a different reason.

What separated "no calls" from "no patch" was a second counter that is
incremented on every call and never cleared: `all=12 cat=0` says the patch
is running and the filter is wrong, where `cat=0` alone says nothing at
all. **A zero needs a witness.**

### Platform facts this cost

- **`tst` has no PC-relative form on a 68000.** That addressing mode
  arrived with the 68020. `tst.w WP_ENABLE(%pc)` assembles nowhere; read
  the flag into a register that is already saved.
- **`NGetTrapAddress` and `NSetTrapAddress` are library glue.** A code
  resource that links no library gets `undefined reference to
  NGETTRAPADDRESS`. `GetOSTrapAddress` and `SetOSTrapAddress` are the raw
  traps, are inline, and are one instruction each. `_HFSDispatch` is
  trap word `0xA260`; bit 11 clear makes it an OS trap, addressed by its
  low byte, `0x60`.
- **`ProcPtr` is a pointer to a struct, not a function pointer**, in these
  headers — so `(ProcPtr)&some_function` is a function-to-object
  conversion and `-pedantic` refuses it. Declare the patch as `extern
  unsigned char WP_PATCH[]`; it is never called from C, only installed.

---

## The pairing, and it needs no second trap patch

The run above left one thing open: a burst of calls, and separately a
window that appeared, with nothing tying them together. That is now
closed, and the way it closed is worth the write-up, because the first
attempt at it gave a wrong-looking answer that was actually the clue.

**First attempt.** Record the head of `WindowList` (low memory `0x09D6`)
at each selector-9 call. It came back as the window that was in front
*before* — Alpha's burst reported the Macintosh window, and the Macintosh
window's burst reported the Control Panel. That is not noise: **the Finder
reads the folder first and creates the window afterwards.**

**So record the head on every call, not only the ones carrying a folder.**
Then the change shows up on the calls that follow the burst:

```
windows: 4
0 "Control Panel" k-16 v @0002AF2A
1 "Alpha"         k16  v @0002A768
2 "Macintosh"     k16  v @0002A6BC
3 "Clipboard"     k17  h @0002A610

 s7 w=0002A768                 <- after the burst: the NEW window
 s7 w=0002A768
 s9 w=0002A6BC i=2  Alpha      <- during the burst: still the old front
 s9 w=0002A6BC i=1  Alpha
 s9 w=0002A6BC i=-1 Alpha
 s7 w=0002A6BC                 <- after the Macintosh burst
 s7 w=0002A6BC
 s9 w=0002AF2A i=7  Macintosh  <- during it
```

`0002A768` is the window titled **Alpha**; `0002A6BC` is the one titled
**Macintosh**. Both pairs agree with their titles.

**The rule, stated so it can be implemented:** a burst is `PBGetCatInfo`
with `ioFDirIndex = -1` on a directory followed by `1, 2, 3 …` on the
same one. The head of `WindowList` during the burst is the *old* front
window; the head on the next call after it is the window that burst
belongs to. Resolve the directory ID to a name and compare it with that
window's title — a wrong pairing fails that, and it is free.

**And the Finder hands us the "after" for nothing.** It calls
`PBGetWDInfo` — selector 7 — twice immediately after creating the window,
so the same patch that watches for the burst is called again once the
window exists. **No patch on `_NewWindow` or `_GetNewWindow` is needed**,
which is the expensive one: a Toolbox trap has to be tail-patched to see
its result, and a tail patch has to stash the caller's return address
somewhere, which is not reentrant.

**What this completes.** The chain a drag sender needs is now unbroken and
every link is published API:

1. mouse-down → `FindWindow` gives the `WindowPtr`;
2. the table above gives that window's directory ID;
3. `PBGetCatInfo` over that directory gives each file's `fdLocation`;
4. `fdLocation` plus the mouse position gives the file.

**What is still not done.**

- *The vehicle.* All of this is a control panel, where the patch lives
  exactly as long as the panel is open. The real thing is an `INIT`:
  resident code in the System 6 system heap, patched for the session,
  holding a table that grows with every window opened. None of that cost
  is measured, and on this machine that heap is the scarce thing.
- *Step 4 is arithmetic nobody has done here.* `fdLocation` is in the
  folder's own coordinate space; turning a mouse point into it needs the
  window's scroll position and content origin, which are constants to be
  measured rather than derived.
- *One Finder.* Everything here is Finder 6.1.x on System 6.0.8. Whether
  7.0.1's Finder reads a folder the same way is untested — and it matters
  less there, since 7.5 has a real Drag Manager.

---

## Step 4: a point in a window, and the file whose icon is under it

The last link. Given the window-to-folder table above, the panel reads the
global mouse, finds the Finder window under it, enumerates that window's
folder, and names the file whose icon contains the point.

*Run:* same machine and disk. The Control Panel is opened first, then the
volume window, and then the pointer is **parked on an icon and
photographed there** — `tools/probe.py --hover 62,105`. The hit line
redraws on null events, so nothing has to be clicked and the panel is
naming a file in somebody else's window with nothing but the pointer's
position to go on.

```
mouse 62,105 loc 32,15 "Macintosh"
  0,0   Alpha
  0,0   Desktop
  273,1 Font/DA Mover 4.1
  28,50 Microsoft Word
  143,1 StuffIt Expander™ 4.0.1 Folder
> 22,-1 System Folder
```

`>` is the hit. The pointer was on the **System Folder** icon, and that is
what came out.

**And the table was applied rather than eyeballed.** `build_pairs` runs
the rule from the section above over the ring — GetCatInfo carries the
head as it was before the window existed, so the window is the head on the
first later call where it changed — and produced the pairing that let this
folder be enumerated at all. The rule is code now, not an observation.

**What was measured, and had been assumed:**

- **`fdLocation` is the icon's top-left, in the window's own local
  coordinates.** `System Folder` is at `22,-1`; the pointer's local
  position is `32,15`, which is inside `22,-1` to `54,31`. No scroll
  correction was needed because the window was not scrolled — that case is
  untested.
- **Global to local needs no port change.** A `GrafPort`'s
  `portBits.bounds` carries the negation of its global origin, so
  `local = global + bounds.topLeft` reads another window's coordinate
  system without calling `SetPort` on somebody else's window.
- **A folder's position is in `ioDrUsrWds.frLocation`, a file's in
  `ioFlFndrInfo.fdLocation`,** and they are at different offsets inside
  the same union. `ioFlAttrib` bit 4 says which.

**Two things the run exposed that a design would have missed:**

- **`0,0` means "not placed", and the Finder does not write it back until
  it has to.** `Alpha` reads `0,0` here — it was made by `hfsutils` and
  has never had a position assigned — and yet the Finder had drawn its
  icon at about x=110 on screen. So for an item the Finder auto-placed,
  `fdLocation` and where the icon actually is do not agree. Files a user
  has ever moved are fine; freshly-created ones are not, until the window
  is saved. **Two unplaced items also both read `0,0`,** so they cannot
  be told apart at all.
- **The enumeration returns invisible items.** `Desktop` is in the list.
  Anything real has to skip `fInvisible`, `fdFlags` bit 14.

Neither is fatal and both are the kind of thing that turns into a bug
report rather than a design note if it is not found on the machine.

## What it costs, as a cdev

Measured, because that is the currency here — a cdev's code is loaded into
the System 6 system heap.

| | bytes |
|---|---|
| `WProbe.cdev`, code resource, before step 4 | 2003 |
| `WProbe.cdev`, code resource, with step 4 | 3152 |
| **step 4** | **1149** |
| of which `patch.o`, the trap patch and its 242-byte ring | 332 |

For scale: the host program's whole control panel — a list, Standard File, a
driver install, an HFS and HFS+ capture — is about 27 KB of code
resource, and its Install scan alone cost 1008 bytes.

**But the cdev number is not the number that matters**, and it is worth
being plain about that. This is all measured in a code resource that is
loaded when the panel opens and thrown away when it closes, which is why
it is safe and why it is cheap. What the real thing costs is an `INIT`:
the same code, resident for the session, plus a table that grows by one
entry for every window opened and is never freed. That has not been
built or measured, and the floor is a machine where the system heap has
room for one mounted volume.

---

## The trigger: seeing the press, and naming the file from it

The chain so far started from wherever the pointer happened to be. What a
drag sender needs is the **mouse-down**, at the moment the Finder is about
to make sense of it.

**`_FindWindow` is the place, and the reason is that its point is an
argument.** The Finder calls it on every mouse-down before deciding what
the click means. Being an argument, the point is on the stack on the way
in, so this is a **head** patch: read it and chain. `_GetNextEvent` was
the obvious alternative and is worse — the event record is filled in by
the trap, so seeing it means tail-patching, which means stashing the
caller's return address somewhere, which is not reentrant.

*Run:* Control Panel opened first, volume window opened, then a real click
on the System Folder icon at global 62,105, then the Control Panel brought
back. `tools/probe.py --press 62,105 --hover 110,105`.

```
over 110,105 "Macintosh" > (nothing)
presses: 8
 60,106 "Macintosh" > System Folder
 60,106 "Macintosh" > System Folder
 60,106 "Macintosh" > System Folder
 60,106 "Macintosh" > System Folder
 60,106 "Macintosh" > System Folder
```

**Press to file, end to end**, with every link published: `_FindWindow`
gives the point, the window list gives the window, the trap-patch table
gives its folder, `PBGetCatInfo` gives the contents, `fdLocation` gives
the icon. (60,106 rather than 62,105 is the emulator's relative pointer
landing a pixel or two off; the harness aims, the machine decides.)

**Two things the run says that were not designed for:**

- **One click produces at least eight `_FindWindow` calls.** The ring
  holds eight and every one of them is the same press — it filled before
  the Apple-menu click that followed could get in. So a sender cannot
  treat a call as an event; it has to debounce, or act on the transition
  rather than the call.
- **`over 110,105 … > (nothing)` is the correct answer and looks like a
  failure.** That is where `Alpha`'s icon is drawn, but `Alpha` has
  `fdLocation` `0,0` — never placed — and unplaced items are now skipped
  rather than reported at the origin. So the limitation found in step 4 is
  visible here as what it actually costs: an item the Finder auto-placed
  cannot be picked up. Files a user has ever moved are fine.

Invisible items are skipped too — `fdFlags` bit 14 — because the
enumeration returns the Desktop file among the rest and a drag that picked
that up would be picking up something the user cannot see.

## Cost, as a cdev, running total

| | code resource |
|---|---|
| window-list probe only | 2003 |
| + step 4, the point-to-file mapping | 3152 |
| + the `_FindWindow` trigger | 3406 |

The trigger is **254 bytes**, most of it the second patch and its 34-byte
press ring; the resolution code was already there and is simply pointed at
a different point. The caveat from the section above is unchanged and is
the important half: this is a code resource that is loaded when the panel
opens and thrown away when it closes. An `INIT` holding a table that grows
per window is a different measurement, and it has not been taken.

---

## Taking the mouse: the first thing here that is not observation

Everything above watches. This step **takes the drag**: the `_FindWindow`
patch, on a press that lands on a file, keeps the mouse until the button
comes up, drawing a mark that follows it. The Finder does not see the
drag at all until it is over.

*Run:* `tools/probe.py --disk-icon 475,45 --drag 62,105,250,318` — press
on the System Folder icon, hold, move in eight steps, **photograph while
still held**, release, then read the panel.

```
drags: 1 mb=5 btn=5 it=-7957 mv=8
 62,105 "Macintosh" > System Folder -> 250,318
```

- **`mv=8`** — the loop saw exactly the eight positions the harness moved
  through. It followed the pointer.
- **`-> 250,318`** — the drop point is exactly where the pointer was taken,
  which it could only know by tracking.
- **`it=-7957`** is 57579 in a signed short, and the overflow is left as it
  came out rather than widened: the number wanted was "did it spin", and
  it did.
- The mid-drag photograph shows an inverted block at the pointer with the
  cursor cut out of it.

**What it deliberately does not do is stop the Finder.** A head patch
returns, so the Finder then handles a click whose button is already up and
treats it as a plain selection. Suppressing that means consuming the
event rather than watching the trap, and that is a different step.

### Four things that went wrong first, all of which photographed as success

1. **No button check.** `_FindWindow` is called by event loops that are
   not handling a press — the panel's own among them — so a press point
   arrives constantly with the button up. Every one resolved to a file and
   was recorded as a drag from a point to itself: **10747 drags**. *The
   trap is not the event; the button is what says this one is.*
2. **An iteration bound is not a time bound.** Two million iterations of
   two Toolbox calls is one length of time on a Macintosh Plus and quite
   another on an emulator running as fast as the host allows. It expired
   part-way through a drag that was still held, rubbed the mark out, and
   the run looked like it had never tracked. It had — the drop point was
   already right. Now bounded in **ticks**, which means the same thing on
   both.
3. **The Window Manager port draws nothing you can photograph.** It was
   the obvious choice — a drag is the one thing not confined to anybody's
   window — and its `visRgn` is the **gray region**: the desktop with
   every window cut out. Correct for the Window Manager, useless here.
   `OpenPort` on a local `GrafPort` gives the screen and a `visRgn` to
   match.
4. **A one-pixel XOR frame over a fifty-percent desktop pattern is
   invisible**, on screen and in a screenshot. `InvertRect` on the whole
   block is unmistakable and rubs itself out the same way. Even so,
   inverting a 50% dither mostly yields the same dither in the opposite
   phase — a real sender would draw the icon's mask, not a rectangle.

Two of these produced a clean screen and a plausible number, which is the
same failure mode as the ring-clearing bugs earlier: **the run reports
something, and the something is not about the question.**

## Cost, as a cdev, running total

| | code resource |
|---|---|
| window-list probe only | 2003 |
| + step 4, the point-to-file mapping | 3152 |
| + the `_FindWindow` trigger | 3406 |
| + taking and tracking the drag | 4650 |

Taking the drag is **1244 bytes**. The whole thing — window-to-folder
table, point-to-file, trigger, and a drag that follows the mouse and knows
what it is carrying — is **4650 bytes** of code resource.

The caveat has not changed and is still the important half: this is loaded
when the panel opens and thrown away when it closes. The `INIT` that would
do it for real is resident, holds a table that grows per window, and has
not been built or measured.

---

## The receiving half, and the thing this project was for

The sender holds the mouse; when it lets go it looks at what is
underneath. Any window, not only the Finder's — which is the whole point.

*Run:* `tools/probe.py --disk-icon 475,45 --drag 62,105,300,200` — the
same press on the System Folder icon, but let go **inside the Control
Panel's own window**.

```
drags: 1 mb=7 btn=7 it=352 mv=8
 62,105 "Macintosh" > System Folder -> 300,200
 dropped System Folder on "Control Panel" k-16 = us
```

**A file dragged out of a Finder window and dropped on a control panel,
on System 6.0.8, which has no Drag Manager** — and the panel knows which
file it is. That is the thing the host program wanted and
cannot have on its floor target, arrived at from the other direction:
instead of a receiver registering with a Drag Manager, the sender that is
already holding the mouse looks at where it let go.

No registration, no flavours, no handlers. `windowKind` is negative for a
desk accessory's window and 16 for the Finder's, so nothing has to be
told which window is whose.

*One cosmetic thing left as it is:* the line runs off the right edge of
the panel at the last two characters. Recorded rather than fixed, because
fixing it means changing code that has been run and not running it again.

*And one number worth not chasing:* `it=352` here against `it=57579` for
the drag that ended on the desktop, over the same wall-clock hold. The
loop spends its time in `InvertRect`, and inverting a block over the
panel's dense text is far slower than over the desktop pattern. The
iteration count measures drawing cost, not time — which is exactly why
the loop's bound was moved to ticks.

## Cost, as a cdev — final for this stage

| | code resource |
|---|---|
| window-list probe only | 2003 |
| + step 4, the point-to-file mapping | 3152 |
| + the `_FindWindow` trigger | 3406 |
| + taking and tracking the drag | 4650 |
| + the receiving half | 5211 |

**5211 bytes of code resource** for the whole chain: the window-to-folder
table built by watching the File Manager, a press caught before the Finder
sees it, the file under it named from `fdLocation`, the mouse held and
followed, and the drop resolved to whatever window it landed on.

For scale, the host program's entire control panel is about 27 KB.

The caveat is unchanged and is still the half that matters: all of this is
loaded when the panel opens and thrown away when it closes, which is why
the trap table never holds a pointer into a resource that is not there.
The `INIT` that would do it for a whole session — resident, with a table
that grows per window — has not been built and has not been measured.

---

## What it costs in the heap — asked because the host program might take it

The sizes above are the code resource, which is not the same question.
This is the heap.

### Nothing is allocated in the system heap at all

*Measured.* The panel records `FreeMemSys` and `MaxBlockSys` when the
patches go in and prints them beside the current values. After a complete
drag — press caught, folder walked, mouse held, mark drawn, drop resolved:

```
sysfree=00003C92 max=00003BB0
 at install=00003C92/00003BB0
```

**Byte-identical.** 15506 free, largest block 15280, before and after.
Everything the sender needs is either inside the code resource or on the
caller's stack, so the heap cost is the code and nothing else.

The one allocation anywhere is `OpenPort`, which makes two regions — and
that is in the **front application's** heap, not the system heap, and
`ClosePort` gives them back within the same call.

### The code, measured by two links rather than apportioned

Per-function sizes cannot answer this: the compiler inlines across exactly
the boundary in question. So the same sources are linked twice, once with
the diagnostic report compiled out — `make minimal` — and the report-free
build was **run and confirmed to drag** before its size was quoted.

| | bytes |
|---|---|
| **the sender alone** — patches, table, point-to-file, drag, drop | **2392** |
| with the probe's report on top | 5555 |

Of the 2392, `patch.S` is 564: both trap patches and all their storage.
The resident buffers inside that are about 400 bytes — the GetCatInfo ring
242, the press ring 34, the drag record 18, the drop record 84, and the
saved trap addresses and flags.

### The transient cost, which is stack and not heap

`wp_track`'s frame is about **750 bytes**: the ring snapshot 240, a
`GrafPort` 108, a `Str255` 256, a `CInfoPBRec` 108, the pair table 32. It
runs on whatever stack the Finder's event loop is on, inside a trap patch.
That is the number to look at twice before this goes anywhere near the
floor — it is not heap, so `FreeMemSys` will never show it.

Two of those are avoidable if it matters: the snapshot exists so the ring
can be read without the patch writing underneath it, and could be read in
place with the patch switched off; the `GrafPort` is only needed while
drawing.

### What this means for the host program specifically

**The `INIT` caveat does not apply to the host program's use case**, and that is
the finding worth having. Everything above lives exactly as long as the
control panel is open — which is why the trap table never holds a pointer
into a resource that is not loaded. For that program it is not a limitation
at all: the user opens the panel and drags images onto it, so the panel is
open by definition. No resident code, no session-long table, no `INIT`.

So the bill is **about 2.3 KB of System 6 system heap while the panel is
open** — 2392 less the hundred-odd bytes of formatting helpers the host
already has — plus 750 bytes of stack during a drag, plus
nothing else.

For scale, in the host program: parking its write path freed 3280 bytes,
of which its `DRVR` code was 2798; its Install scan cost 1008. This is
between them, and larger than either.

**It has not been run at the 1 MB floor as part of the host program**,
and the host's own rule says that is the run that decides it: anything that
grows a resident structure gets a run at the floor before it is believed,
and a cdev's code is loaded into that heap too. WProbe's own runs are at
the floor, but WProbe is not carrying a driver, a list, Standard File and
a capture.

---

## Going lower

Asked for, and the answer is yes: **2392 → 1872 bytes**, and the stack
frame with it. Both builds were re-run and both still drag.

### The big one: apply the pairing rule where the calls are

The window-to-folder rule used to be *logged* — twenty File Manager calls
into a ring, snapshotted onto the panel's stack, and walked by
`build_pairs` to find the head change. The rule is small enough to apply
in the patch itself: on `ioFDirIndex == -1`, remember the folder and the
front window; on any later call where the front window has changed,
record the pair. Four slots written round, read newest-first, and no
duplicate detection anywhere — the search order does that.

That deleted, together:

| | bytes |
|---|---|
| the 20-entry ring | 242 |
| `build_pairs` | 252 |
| `ring_take` | 76 |
| the stack snapshot it needed | 240 (stack) |
| against new assembly | +40 |

It is also simply righter. The rule is now in one place, stated once, and
there is no window between logging a call and interpreting it in which the
patch can write underneath the reader.

### Two smaller ones

- **The button is read once, not twice.** Both readings existed because
  the guard first reported no drags and there was no way to tell a button
  that was up from a trap answering wrongly. They agreed, 25 and 25, so
  the check has done its job and the cheaper reading stays. The four drag
  counters — `mb`, `it`, `mv` — are guarded into the diagnostic build.
- **`Str255` is 224 bytes of stack nobody uses.** An HFS name is at most
  31 characters, so 40 bytes is the buffer. This runs on the Finder's
  event-loop stack inside a trap patch, which is why it was worth doing.

### Where it stands

| | bytes |
|---|---|
| **the sender alone** | **1872** |
| with the diagnostic report | 4632 |

and inside the 1872:

| | bytes |
|---|---|
| `wp_track` — press, folder, file, hold, track, drop | 920 |
| `patch.S` — both trap patches, and ~156 of storage | 396 |
| `WP_CDEV_MAIN` — install, remove, message dispatch | 236 |
| the one-line report, which the host would not take | 254 |
| the entry branch | 4 |

**So what the host would actually add is about 1.4 KB**: the sender and
the patches, without this probe's own dispatch and report, since the host
has both already.

**And the stack is now about 290 bytes**, down from 750 — the snapshot is
gone and the name buffer is a quarter of its old size. That was the number
worth worrying about, because it is spent inside a trap patch on somebody
else's stack and `FreeMemSys` will never show it.

The heap claim is unchanged and was re-measured on both runs: `sysfree`
and `max` identical at install and after a complete drag.

---

## The last cuts, and a correctness bug they paid for

**1872 → 1777**, and the sender is now right on a volume that is not the
boot disk, which it was not before.

### What came out

- **The window's title, forty bytes of it.** The window POINTER says the
  same thing and more: a panel asking "was that dropped on ME" wants a
  pointer comparison, not a string, and anyone who wants a title has the
  window list. `dropped System Folder on us` is that comparison against
  the panel's own dialog.
- **Where the drag began and ended.** Two points, recorded for the report
  and used by nothing: the drop record already says which window it landed
  on, which is what a caller acts on. Guarded into the diagnostic build.
- **Two bytes of slot padding.** A slot is window(4) + folder(4) +
  volume(2) = 10, which is even, so the pad was never needed.

### What went in, and why it was not optional

**The folder was being looked up with `ioVRefNum = 0` — the default
volume.** A Finder window showing a folder on a second disk would have
resolved that directory ID against the boot disk and named the wrong file
or none. It would never have shown up here, because this test disk has one
volume.

So the patch records `ioVRefNum` from the parameter block alongside the
directory ID — **whatever the Finder itself passed**, which is the thing
that works when it is passed back, whether it is a volume reference number
or a working directory's. The drop record carries it too, with the folder,
because a name on its own can be looked at and not opened:

```
drop System Folder v=-1 d=00000002 on us
```

That is volume, folder, name and target — everything a caller needs to
open the file rather than merely say what it was.

**The cut paid for the fix**: −90 bytes of title and coordinates against
+120 of volume threading, and then the slot and padding trims took it
below where it started.

### And a display bug worth keeping

The pair-table display kept the old **eight**-byte stride after the slot
grew to ten, and printed a row claiming `@FFFF0000` was showing the System
Folder. `FFFF` was slot zero's `vRefNum`, −1, read four bytes into the
wrong place; the trailing zeros made a directory ID of 0, and
`PBGetCatInfo` takes that as "the default directory" and cheerfully
answers **System Folder**. A completely plausible row, assembled out of a
stride error and a helpful File Manager.

Two things came out of it. The guard is now `win == 0 || dir == 0`,
because directory ID zero is not a folder. And the stray row was, by
accident, the first evidence that the volume was being recorded at all —
`FFFF` had to come from somewhere.

### Where it ends

| | bytes |
|---|---|
| **the sender alone** | **1777** |
| with the diagnostic report | 4760 |

The host's share of that is still about **1.4 KB**, since it has its own
dispatch and its own interface. Heap allocation during a drag: none,
re-measured on this build. Stack during a drag: about 290 bytes.

---

## What was left: one optimisation that costs bytes, and one that is not worth it

Asked whether anything remained. Three things did. Two were done and the
size went **up**, which is the right answer to a question about
optimisation rather than about size.

### A press is not a drag until it moves — 112 bytes, and worth every one

`wp_track` walked the folder on the **press**: up to forty
`PBGetCatInfo` calls, before the Finder was allowed to see an ordinary
click, on a Macintosh Plus, possibly off a floppy. Every click in a Finder
window paid for a drag that mostly was not happening.

So nothing is looked up until the pointer has left the icon by more than
three pixels — which is also what a drag *is*.

*Measured, on the case the change is about:* a click that does not move.

```
drags: 0 mb=6 it=0 mv=0
```

Six presses saw the button down, none became a drag, and no folder was
walked. Before, every one of those six would have enumerated the volume's
root. A click now costs one comparison.

*And the drag still works*, re-run on both builds.

### The merge that cost fourteen bytes and was kept anyway

The gate was written as a second loop first — one to wait, one to track —
which is 1875 bytes against **1889** for the single loop that does both.
So merging *cost* fourteen bytes. It is kept for the shape rather than
the size: one loop, one mouse read, one button check, one place where the
decision is made. Fourteen bytes is noise; two loops drifting apart is
not.

Written down because the comment in the source first claimed the merge
saved 98 bytes, which was an expectation typed before the link ran.

### Twenty-eight bytes of storage the sender never touches

`WP_DRAG`, `WP_HEAP` and `WP_TOTAL` are diagnostics and their `.space` is
now guarded out of the sender build too, not just the code that writes
them.

### What is left, and why it is not being taken

**`blob_long` and `set_blob_long` read and write the patch's storage a
byte at a time** — 42 bytes of helper plus a call at each of five sites,
perhaps 60 bytes in all. Every field in that storage is at an even offset,
so direct long access would work and would be smaller.

**It is not worth it.** The saving is three percent, and what it buys in
exchange is an alignment assumption that a future edit breaks *silently*:
add one byte field near the top and every long after it is odd, and a
68000 answers that with an address error rather than a warning. The
byte-wise version cannot be broken that way. This is the same trade
the host program makes for file data — byte-wise parsing only — arriving
at our own structures.

### Where it ends

| | bytes |
|---|---|
| **the sender alone** | **1889** |
| with the diagnostic report | 4852 |

Up 112 from the smallest it has been, and faster on every click that is
not a drag. Heap during a drag: none. Stack: about 290 bytes.

---

## Does the drag make the machine hiccup? Measured: no.

The worry is right and the answer is not a reading of the code. So the
`_HFSDispatch` counter was moved **before** the enable check, which makes
it a count of File Manager *work* rather than of work we chose to log, and
the sender takes a reading either side of the commit and either side of
the tracking loop.

*Run:* the usual drag, from an icon, across the Control Panel's window, to
a drop.

```
drags: 1 mb=6 mv=10 fm=6/0
```

**`fm=6/0`: six File Manager calls to identify the file, and ZERO during
the drag itself.**

Wall clock cannot say this — Mini vMac runs the emulated CPU as fast as
the host allows, and `tools/vmac/run.py` says so in its own header — but a
count of calls is the same number on a Macintosh Plus as it is here.

### What the tracking loop actually does per pass

Two bytes of low memory for the button, two for the mouse, one
`TickCount`, and — only when the position has changed — two `InvertRect`s.
No File Manager, no Memory Manager, no allocation. **Dragging across
another folder's window costs nothing extra**: nothing inspects what is
underneath while the mouse is moving. What the drop landed on is resolved
once, at the release, by walking the window list, which does not touch the
disk either.

### The one pause there is, and where it was moved to

Identifying the file is a folder walk — one `PBGetCatInfo` per item until
the icon under the press is found, six here, bounded by the item count.
That is the only work with a disk behind it, it happens **once**, and it
used to happen *before* the mark was drawn: between the user moving the
mouse and anything appearing, which is exactly where a pause is felt.

It now happens after. The mark is already following the pointer while the
walk runs. The price is a mark that flashes and vanishes when the press
was not on a file at all, which is the rare case and the cheap one to be
wrong in.

*Not measured, and said as reasoning rather than as a result:* those
lookups should be cache hits, because the Finder has just enumerated the
same folder to draw the window they are in. That is an argument, not a
number, and the number would need the 1x emulator and a run this harness
is not shaped for.

### A limit found while looking, which was a bug

`file_at` walked **at most 40 items**. The loop ends by itself when
`PBGetCatInfo` runs off the end of the folder, so 40 was meant as a
runaway guard — but it was a limit: a System Folder routinely holds more
than forty files, and the forty-first could not be picked up. Silently,
because a miss looks exactly like a press on empty space. It is 200 now,
which is a guard again rather than a limit, and it costs nothing in the
average case because the walk stops at the icon it is looking for.

### And one stale field removed

`it=`, the tracking-loop iteration count, went on reading zero after the
gate and the tracker became one loop and the counter went with them. A
field that always reads zero is worse than no field — the same rule that
retired `btn=`.

| | bytes |
|---|---|
| **the sender alone** | **1911** |
| with the diagnostic report | 4932 |

---

## The order a user actually works in — a hole, and the fix

### The hole

Every run so far opened the Control Panel **first** and the Finder window
second, because that is the order the patch needs: it learns a window's
folder by watching the Finder read it. A user does the opposite — find
your files, then open the panel you want to drop them on.

*Run:* `tools/probe.py --pre-open …`

```
pairs (0 seen), all=2
drags: 0 mb=13 mv=0 fm=0/0
 60,106 "Macintosh" unpaired
```

**Every window unpaired, every drag refused.** And `all=2` says the
obvious workaround is not one: clicking a window to activate it costs two
File Manager calls and does not make the Finder re-read the folder.

### The fix, and what makes it safe

For those windows the folder is dug out of the Finder's own record. The
earlier searches found nothing (above) — and what changed is not the
search but the **check** available to it: for a window on screen, the
folder's name must equal the window's **title**. Find a candidate any way
you like, then make it survive a test that has nothing to do with how it
was found.

```
deep "Alpha"     1/72 d=000000DB
deep "Macintosh" 1/12 d=00000002
```

`0xDB` is 219, which is `Alpha`'s real directory ID, one hop past the
refCon and 72 bytes in — while `Macintosh` is 12 bytes in. **Different
offsets, so it is not a field and a fixed offset would be a fiction.**

`Macintosh` on its own would have proved nothing: it is the volume's root,
directory ID 2, named after the volume — so every stray long equal to 2
passes the title check for that one window. `Alpha` is the run that
counts, because 219 is arbitrary and a stray 219 naming a folder called
exactly `Alpha` is not a thing that happens.

**A candidate that matches on more than one mounted volume is refused
rather than picked.** Two disks can hold folders with the same name at the
same directory number, and a wrong one is a drag that lifts a file out of
the wrong disk. Refusing costs a drag; being wrong costs the user's
confidence in every other drag.

### What it costs, measured twice because the first number was wrong

It runs **once**, when the panel opens, and its results go into the same
table the patch writes — so nothing on the drag path calls it and a drag
is still zero File Manager calls.

The first measurement said **3458 calls** to pair two windows, which on a
Plus is seconds of stall when the panel opens. Every long between 2 and
65535 was a candidate and each cost a `PBGetCatInfo`.

`ioVNxtCNID` is the fix: the next catalog node a volume will hand out, so
every directory that exists is below it — a few hundred on a disk this
age. One number from the `PBHGetVInfo` already being made, and almost
every candidate is then rejected by a comparison instead of a disk call.

**13 calls to pair two windows.** Measured on its own, because the running
total could not answer it: the diagnostic report resolves points on every
null event and swamps everything — 8149 calls in a few seconds of the
panel merely being open, almost none of them the pairing.

### And it works

```
pairs (2 seen), pairing cost 13
 @0002A6BC d=00000002 -1 Macintosh
 @0002A768 d=000000DB -1 Alpha

drags: 1 mb=4 mv=9 fm=6/0
 62,105 "Macintosh" > System Folder -> 300,200
 drop System Folder v=-1 d=00000002 on us
```

A drag out of a window that was open before the panel existed.

### What it cost in code

| | bytes |
|---|---|
| sender, before | 1911 |
| **sender, with already-open windows paired** | **2619** |

708 bytes, and not optional: without them the feature works only if the
user opens the control panel before the folder, which nobody does.

*Still not fixed, and unrelated:* an item the Finder auto-placed has
`fdLocation` `0,0` and cannot be picked up at all. That is the remaining
functional hole.

---

## The auto-placement hole, looked at: narrower than it read

An item whose `fdLocation` is `0,0` has never been given a position, and
cannot be picked up — the sender skips it, because `0,0` is not a location
but the absence of one, and two unplaced items are indistinguishable.

The question is how long an item stays that way.

*Run:* open the volume window, look, close it, look again.
`tools/probe.py --close-window 28,52`

```
before                          after
 0,0   Alpha                     64,0  Alpha
 0,0   Desktop                   0,0   Desktop
 273,1 Font/DA Mover 4.1         273,1 Font/DA Mover 4.1
 28,50 Microsoft Word            28,50 Microsoft Word
 143,1 StuffIt Expander™ …       143,1 StuffIt Expander™ …
 22,-1 System Folder             22,-1 System Folder
```

**The Finder writes the auto-placed position back when the window is
closed.** Nothing else moved. `Desktop` stays at `0,0` because it is
invisible and was never drawn.

So the hole is not "an auto-placed item can never be dragged". It is:

> An item put on the disk by something that is **not** a Finder cannot be
> dragged until its window has been opened and closed once.

Which is a much smaller thing, and it has a user-facing answer that is
true rather than a workaround: **close the window and open it again.**

*Why `Alpha` was the case at all:* it was made by `hmkdir`, so no Finder
has ever placed it. Every other item on this disk has a position because a
Finder put it there. A file a user copies in the Finder gets one at the
moment of the copy — which is how the stock items got theirs — so the
common route onto a Macintosh does not hit this.

*The route that does hit it* is the one the host program cares about: an image
written onto a disk by a tool that is not a Finder, which is exactly what
`tools/vmac/` does with `hfsutils`. Worth knowing rather than worth
fixing.

### Why nothing is being built for it

Two things could be, and both are worse than the limitation.

- **Reproduce the Finder's placement.** It is deterministic — `Alpha`
  landed at `64,0`, in the gap after `System Folder` at `22,-1` — so it
  could probably be worked out. But there is **no check available**: no
  independent oracle says where an icon is, so a computed position could
  only be believed, and this file is a long argument against believing
  computed answers. Every other thing here is checked against something
  the wrong answer cannot pass. This one cannot be.
- **Write positions ourselves.** That modifies the user's disk to make our
  feature work, and it would be fighting the Finder's own layout for
  icons it has already drawn somewhere else.

The current behaviour is the safe failure: a press on an unplaced item
finds nothing, the mark flashes and vanishes, and the Finder handles the
click as the ordinary click it was.

### Two harness notes, both of which cost a run

- **A click on an inactive window's close box brings it forward** rather
  than closing it. The run photographed a window that was now in front and
  still open, which reads as a failed close and is a successful activate.
  It takes two clicks.
- **The close box is at (28, 52), not (27, 46).** Six pixels high is the
  title bar, and clicking the title bar of an inactive window also just
  activates it — so the same photograph explains both mistakes and neither
  is distinguishable from the other without looking at where the box
  actually is.

---

## What happens if the user drags an unplaced item anyway

Asked, and worth asking, because the answer had been asserted from
reading the code rather than measured.

*Run:* open the volume window, bring it in front of the panel, and drag
`Alpha` — the folder with `fdLocation` `0,0` — out of it.
`tools/probe.py --raise-window 60,46 --drag 110,105,300,250`

**Nothing of ours happens, and the Finder's own behaviour is untouched.**

```
drags: 0 mb=6 mv=0 fail=1 fm=7/0
```

We take no drag. A mark is drawn and rubbed out. The Finder then receives
the press and does what it would have done with none of this installed:
it selected the folder and moved it — the window went from **5 items to
4** in the photograph. No residue on screen, no stuck state, nothing to
recover from.

That is the whole answer. What follows is what looking at it exposed.

### Two things it exposed, both now fixed

**The draw-first order was flashing a mark for every press that moved,
anywhere.** Drawing before identifying was right for the *folder walk*,
which reaches the disk — but the question "is this press even in a Finder
window?" is a walk of the window list and costs nothing, so it belongs
before the drawing and not after. Without that, a drag on the desktop, in
another application's window, or on this panel's own window drew and
erased a mark for no reason. Now the free check comes first.

**Refusing a gesture cost 56 File Manager calls.** `_FindWindow` is called
about eight times for one press — and the Finder's *own* drag, which takes
over the moment we decline, calls it again on every pass with the
pointer's current position. Each of those walked the folder afresh looking
for an icon that is not there: measured at **eight refusals of seven calls
each, while the Finder was trying to drag the icon itself.**

Remembering the failed *point* was the first fix and it does not work,
because those calls carry different points — the pointer is moving. What
is the same across them is the **gesture**, so that is what is remembered,
and it is cleared by the button coming up.

```
before   fail=8  fm=7   (56 calls to refuse one drag)
after    fail=1  fm=7   (7)
```

And the successful path is unchanged: `drags: 1 mv=8 fail=0 fm=6/0`,
`drop System Folder v=-1 d=00000002 on us`.

**Also removed:** the per-item `fdLocation` listing added for the
auto-placement question. It had answered it, and it was six lines long,
which pushed the pair table down into the band `draw_hit` erases — a
report that overwrites its own most important line.

| | bytes |
|---|---|
| **the sender alone** | **2651** |
| with the diagnostic report | 5780 |

---

## Two claims about the unplaced case, both tested

### "It cannot happen in practice, because nothing else is running"

The reasoning: on System 6 without MultiFinder one application runs at a
time, so while the user is in the Finder nothing can create a file behind
its back, and every file the user sees was therefore placed by the Finder.

True of a pure Finder session, and it is not the route. The condition is
not "a file appeared spontaneously" but **"a file whose Finder info was
never set by a Finder"**, and the commonest way to get one is entirely
ordinary: an application saves a document, the user quits to the Finder,
opens the folder, and tries to move it.

`Create` leaves the Finder info zeroed. So the panel made a file the way
an application makes one — `PBHCreate` at the volume root — and asked the
catalog what position it has:

```
app-made 0,0
```

**A file created by a program has no position.** Not a program running
alongside the Finder; a program that ran before it. The other two routes
are a disk prepared on another machine, and — the one that matters here —
**a volume the host program itself mounts**, whose contents were written by
whatever made the image.

*(`PBHCreate` rather than `Create`: the high-level call is library glue,
`undefined reference to CREATE`, and this resource links no library. The
same shape as `NGetTrapAddress` having to become `GetOSTrapAddress`.)*

### "Dragging it a few pixels and letting go would fix it"

It would, if the Finder wrote the position when it moved the icon. It does
not.

*Run:* drag `Alpha` from 110,105 to 150,135 — a short move inside its own
window — release, and read the catalog. The photograph shows the icon
**has moved**, to the second row, selected.

```
Alpha 0,0
```

**Still no position.** The Finder holds the new one in memory and writes
it at the same moment it writes an auto-placed one: when the window
closes. Moving the icon does not change *when*, only *what*.

So the instinct is nearly right and lands one step short. What fixes it is
closing the window — with or without the nudge.

### Which leaves the failure looking like this

A user tries to drag a file an application just saved. Our sender declines
(`drags: 0`), the Finder takes the press and moves the icon as it normally
would, and the file still cannot be picked up by us. Close the window and
reopen it and it can, permanently.

Worth stating plainly rather than filed as an edge case: **the case is
ordinary, the workaround is real, and neither is discoverable.** Anything
built on this wants to say so.

---

## Is there a programming workaround for the unplaced case? Tried, and no.

Four routes. Three were rejected with reasons, one was actually searched
for and not found — and the search is written down so nobody repeats it.

### 1. Reproduce the Finder's placement algorithm — rejected on principle

It is deterministic and could probably be worked out. But there is **no
oracle**: nothing independent says where an icon is, so a computed
position could only be believed. Every other answer in this project is
checked against something a wrong answer cannot pass — the folder against
the window title, the drop against the window pointer, a candidate
directory ID against more than one volume. This one has nothing.

### 2. Find the Finder's in-memory layout — searched, not found

This one is not a guess, because it **can** be checked: a layout holds
positions for every item in the window, including the ones whose
`fdLocation` the catalog already knows. Take a placed item's position from
the catalog, look for those four bytes in the Finder's structures, and if
they turn up, the thing containing them is the layout. Several known pairs
agreeing is not a coincidence worth worrying about.

*Searched:* the window record's own block, then every plausible pointer in
its first 64 bytes, then every plausible pointer in the first 64 bytes of
each of those — two dereferences, up to 16 × 16 blocks, **512 bytes each**,
for `273,1` in both `(h,v)` and `(v,h)` order (the second of which is also
how a `Rect`'s top-left would read).

```
hunt 273,1 : none
```

**Not found.** Which does not prove it is not there — the Finder may hold
positions in some other form, or further away — but it does say the search
that found the folder ID does not find this, and that is the bound worth
recording. The folder ID turned up one hop out and 72 bytes in; this is
not in the same neighbourhood.

### 3. Write the positions ourselves — rejected

It modifies the user's disk to suit our feature, and worse, it would not
work: for a window already open the Finder has computed its layout in
memory and keeps drawing there, so our positions would disagree with what
is on screen until the window was reopened. That is worse than the
limitation.

### 4. The one that works is a sentence, not code

Closing the window and opening it again fixes it permanently, because that
is when the Finder writes. And a user's own instinct helps rather than
hinders: a failed drag leaves the Finder moving the icon, so **nudge it
and close the window** and it is placed for good.

That is not a workaround anybody can find on their own, which is the point
worth carrying forward: **the case is ordinary, the fix is one action, and
neither is discoverable.** Whatever is built on this should say so where
the user is, not here.

---

## Found it: the Finder does keep the position, and it is reachable

The earlier "no programming workaround" stands corrected. The reasoning
that got here was not mine — a position has to be held somewhere or a
redraw would put a moved icon back, and the nudge run showed it stays
moved — and the mistake was searching the wrong place. Following pointers
out from the window record found nothing. **Scanning the Finder's whole
application heap found it at once.**

`ApplZone` at `0x02AA` is the zone and its `bkLim` the end; every byte
between is RAM this machine owns.

### One occurrence, then a table, then an anchor

Searching the heap for a known item's position — `273,1`, Font/DA Mover —
gave exactly **one** hit in ~130K. Then all four placed items:

```
273,1  0x00033808   28,50  0x000338FA   143,1  0x0002E8FA   22,-1  0x000339FE
```

Three clustered 242 and 260 bytes apart: records of varying size, and the
thing that varies in a Finder record is the name. So the search was
re-anchored on the **name** instead, and asked where the position sits
relative to it:

```
0x00033868 @-96  Font/DA Mover 4.1
0x0003395A @-96  Microsoft Word
0x000339D4 @-96  StuffIt Expander™ 4.0.1 Folder
0x00033A5E @-96  System Folder
```

**Minus 96, for all four** — different name lengths, files and folders
alike.

### And it gives the right answer for an unplaced item

Reading that offset for every item in the folder, with the catalog's value
beside it:

```
 0,64        cat 0,0    Alpha
 0,0         cat 0,0    Desktop
 1,273       cat 273,1  Font/DA Mover
 30092,22352 cat 0,0    MadeByProbe
 50,28       cat 28,50  Microsoft Word
 1,143       cat 143,1  StuffIt Expander
```

The order is `v,h` — a `Point` — and every placed item's memory value
equals its catalog value.

**`Alpha` reads `v=0, h=64`.** The catalog says `0,0`. And the earlier
close-the-window run recorded what the Finder eventually wrote for
`Alpha`: **`64,0`**. Exact match, from a different run, before any of this
was looked for. That is ground truth arriving independently, which is the
best kind.

### What makes it safe to use, and what does not

**The check is already in hand at runtime.** The sender enumerates the
folder anyway. So: read the memory position for every item, require that
it equals the catalog for every item the catalog has a position for, and
only then trust it for the ones it does not. A wrong offset, a moved
structure, a different Finder — all fail that immediately, on the same
folder, in the same breath.

**What is not safe is the name search on its own.** `MadeByProbe` read
`30092,22352`, which is nonsense: the first occurrence of that name in the
heap is not its Finder record. A name has to be found *and* corroborated —
by the position being inside the window's content rectangle, or by taking
the occurrence nearest the records already confirmed.

**And this is the most fragile thing in the project.** A magic offset into
an undocumented record in one version of one Finder, against a folder ID
found by a name check and a drag built on published traps. It is worth
having written down; whether it is worth shipping is a different question,
and the check above is what the answer turns on.

---

## The version sweep: System 6.0.8 works, System 7.0.1 runs but cannot drag

The first other Finder. Three portability bugs found — all of which also
improved System 6 — and one boundary that has not been crossed.

### Three bugs the sweep found

**1. `windowKind == 16` is a fact about one Finder.** System 7.0.1 uses
**20**, and its **desktop is a window** of that kind too. The failure was
silent: no window matched, nothing paired, every drag refused, and the
report said `pairs (0 seen)` with no hint why. It is `>= 8` now —
`userKind`, documented, true of both — which is deliberately loose,
because a window the Finder never opened a folder for has no pair and is
refused a step later anyway. *Do not identify a thing by a magic number
when a check can settle it.*

**2. The cheap check ran too late.** "Is this press in a Finder window?"
costs no File Manager calls, and it was being asked *after* the movement
gate. So the gate was entered for every press anywhere: a click on the
Apple menu at `14,10` entered it and held the machine for the whole tick
bound. Measured — `for 14,10 gate=62675` — with `WP_BUSY` blocking
everything that followed, so the drag that came next was never seen.

**3. One refusal disabled the sender for the session.** `WP_REFUSED` is
cleared only by *seeing* the button up. Where the button state does not
advance, that never happens: `mb=2`, two presses ever considered, and
every one after silently declined.

System 6 after all three: `drags: 1 mb=1 mv=8 fail=0` — and `mb=1` rather
than the old 5, because only the real press is now considered at all.

### The boundary: the drag cannot be taken on System 7

Everything up to the drag works there. The patches install, folders pair,
and a point resolves to a window and a file — `over 500,330
"System_7_0_1" > (nothing)` is the whole chain answering.

**What does not work is holding the mouse.** While the tracking loop has
the CPU inside `_FindWindow`, System 7 does not advance the mouse position
or the button state, so the gate never sees movement and never sees a
release. Measured: `gate=62675` passes against a frozen `20,40`.

Four ways of asking were tried and none of them moved it:

| | |
|---|---|
| `Mouse`, low memory `0x0830` | frozen |
| `RawMouse`, `0x082C` — the interrupt-updated one | frozen |
| `EventAvail` once per pass through the loop | frozen |
| `EventAvail` before the button read as well | frozen |
| `StillDown()` — the canonical tracking-loop question | frozen |

On System 6 all of this works without any of it, which is what hid the
problem: **the technique of holding the mouse inside a trap patch is, so
far, System 6 only.**

That is the boundary the sweep was for, and it is the thing to attack
next: not "does the cdev run" — it does — but "what makes a System 7
machine advance its mouse state while somebody else has the CPU".

---

## Harness or cdev? The harness is exonerated, decisively

Worth settling before more theorising, and it takes one run with **no cdev
installed at all**: press on the bare desktop before the Control Panel is
ever opened, drag, and photograph while still held.

`tools/probe.py --system 7 --rubber 150,150,380,260`

**The Finder's own rubber-band rectangle follows the pointer.** So motion
during a button hold reaches the emulated machine perfectly well, and
anything that cannot see it is ours.

That also kills the tempting explanation. The Finder's rubber-band loop is
itself a tight tracking loop calling `StillDown` and `GetMouse` without
returning to its event loop — exactly the shape of ours — and it works.

### So what is actually different

Ours runs **inside a head patch on `_FindWindow`**, before the real trap
has been reached. In that context, on System 7:

| asked by | answer |
|---|---|
| `Mouse`, low memory `0x0830` | frozen |
| `RawMouse`, `0x082C` | frozen |
| `EventAvail` each pass, then the low-memory read | frozen |
| `EventAvail` before the button read as well | frozen |
| `StillDown()` | stays true after the button is long released |
| `GetMouse()` — which is what the loop used when it first worked | frozen |

The last of those is worth its own line, because it was a **regression I
introduced**: the loop used `GetMouse` when this first ran, and the raw
low-memory read arrived with the size reduction — cheaper, and it made no
difference on System 6. Putting `GetMouse` back does not fix System 7, so
the raw read was not the cause, but it should never have been changed on
the grounds it was.

Meanwhile `TickCount` clearly advances: the loop leaves by its tick bound.
So interrupts are running and the machine is not stalled — it is
specifically the mouse and button state that do not move for us.

### What that suggests doing instead

Stop taking the mouse, and **ride along with the Finder's own tracking
loop**. The Finder calls `StillDown` on every pass of the drag it is
already doing, and that loop demonstrably sees the mouse. Patching
`_StillDown` would put our code inside a context that works, called
repeatedly for the duration of a real drag, with the press point already
known from `_FindWindow`.

The Finder would still be doing its own drag underneath, which for a drop
onto a control panel means it refuses the drop and snaps the icon back —
and we would still learn what was dragged and where it was let go. That is
a different design from the one that works on System 6, and whether it can
be one design rather than two is the question it raises.

---

## System 7.0.1 works. The previous section was wrong, twice over.

`drags: 1 mb=12 mv=6 fail=1` on System 7.0.1 — six movements tracked, a
drag taken. And System 6 unchanged beside it: `drags: 1 mb=1 mv=7 fail=0`.

The claim that the mouse "does not advance inside a trap patch on System
7" was **wrong**, and it is worth setting out how it was reached, because
every step of it was a measurement.

### The measurement that broke the chain of reasoning

`why=1/8t`. The tracking loop was leaving because **`StillDown` went
false after eight ticks** — a correct, prompt exit for a click. It was
never hanging, never frozen, never spinning to its bound. Everything built
on top of "it spins against a frozen mouse" was built on a number nobody
had measured: I inferred the exit reason from the iteration count instead
of recording it, and the inference was wrong.

*The lesson is one this file already contains and I did not apply: a zero
needs a witness. So does an exit.*

### The two real faults

**1. `WP_REFUSED` was sticky across presses.** It is set by any press
outside a Finder window — a click on the Apple menu will do — and was
cleared only by *seeing* the button up. System 6 calls `_FindWindow` about
eight times per press, so a button-up is always observed in between and
the flag never survives. System 7 calls it far fewer times, so the flag
lived on into the **next** press and refused it. Measured as `mb=2`: two
presses ever considered in a whole session.

Now the refused **point** is remembered too, and a press somewhere else is
reconsidered whatever the flag says. The two together are what was wanted
all along: the same press refused once and cheaply, a different press
always a fresh question. `mb` went 2 → 15.

**2. Nothing on the System 7 disk had ever been placed.** Every
`fdLocation` on it reads `0,0`, because the image was built by a tool and
no Finder has ever laid an icon out on it. So every lookup correctly found
nothing, and 100% of a disk hit the limitation that is an edge case
elsewhere.

Open the volume window, close it — which is when the Finder writes the
positions it invented — and open it again, and the drag works.

### What the sweep therefore says

| | |
|---|---|
| System 6.0.8 | works |
| System 7.0.1 | works |

with three portability fixes that were real (`windowKind >= 8`, the
window check before the gate, the refusal remembering its point) and one
diagnosis that was not.

**And a warning for the rest of the sweep**: a stock disk image is likely
to have no placed icons at all, so a new system will look like a total
failure for a reason that has nothing to do with it. Open a window and
close it before believing anything.

---

## The harness now opens and closes a window before anything else

`prime_icons()` is a default step, not a flag. Every run double-clicks the
volume's icon, photographs the window, clicks its close box, and
photographs it shut — before the Control Panel is ever opened.

**It is not tidying up.** A disk image built by a tool has never had a
Finder lay its icons out, so every `fdLocation` on it reads `0,0` and
nothing on it can be picked up at all. A run against such a disk measures
nothing: it looks like total failure for a reason that has nothing to do
with the system under test, which is exactly how System 7 was wrongly
written off. Opening the window makes the Finder invent positions;
**closing it is when it writes them.**

`--no-prime` skips it, for meeting the unplaced case deliberately.

### The close box is a per-system constant, and six pixels matter

```
CLOSE_BOX = {"6": (28, 52), "7": (141, 260), "75": (141, 260)}
```

Both were measured by enlarging a photograph, and both were got wrong
first. On System 6 the box is at `28,52` and the title bar at `28,46`; on
System 7 the box starts at x=136 and `133` is the frame beside it. **A
click on the title bar, or on an inactive window's close box, merely
activates the window** — which photographs as a window that is now in
front and still open, and reads as a failed close. Two runs were spent on
that before the box was measured rather than guessed.

### And the panel now says whether the disk has placed icons

The first few items of a paired folder, with their `fdLocation`, in the
report. That is the one question that must be answered before any result
from a new system means anything, and it is now impossible to miss:

```
 -1,-1 Alarm Clock
```

Non-zero, so that folder has been laid out and results from it are about
the system.

### Where the sweep actually stands

| | |
|---|---|
| System 6.0.8 | **works**, verified with priming: `drags: 1 mv=7 fail=0`, `62,105 "Macintosh" > System Folder -> 300,200` |
| System 7.0.1 | **a drag was taken once** — `drags: 1 mv=6` — and has not reproduced since |

The System 7 result is real but not yet repeatable on demand, and the
outstanding suspects are all on the harness side rather than the cdev's:
the panel window covers the icon being pressed once it returns to the
front, so the drag has to be aimed at a window that is actually on top,
and which window that is changes with the step order. Nothing measured
says the cdev behaves differently there.

*That distinction is worth keeping sharp, because the last time it was
blurred it produced a confident and wrong conclusion about System 7.*

---

## System 7.0.1 works, reproducibly — and the last blocker was the harness

```
System 7.0.1   drags: 1 mb=9 mv=6 fail=1
               271,290 "System_7_0_1" > System Folder -> 250,150
               drop System Folder v=-1 d=00000002
System 6.0.8   drags: 1 mb=1 mv=7 fail=0
               62,105 "Macintosh" > System Folder -> 300,200
               drop System Folder v=-1 d=00000002 on us
```

Both systems, the whole chain, with priming as a default step.

### The real bug in the cdev: a pair was never checked

The burst rule — a folder read, then the front window changing — is a
**heuristic**, and it is System 6 shaped. On System 7 the Finder reads
folders for its own reasons and moves windows for others, and the two
coincide. Measured: the window titled **"Control Panels"** came back
paired to the folder **"Apple Menu Items"**. Every lookup then searched
the wrong directory, found nothing, and read as the drag not working.

So a pair is now checked before it is believed: the folder's name must
equal the window's **title**, which is the same test the deep search for
already-open windows has always had to pass. One `PBGetCatInfo`, once, at
the moment a drag commits — not on the drag path, which still makes no
File Manager calls at all. The bad pair is now refused by name:
`"Control Panels" unpaired`.

*It had no check only because on System 6 it was never seen to be wrong.
A heuristic right on one system and wrong on another is not one to leave
unchecked.*

### And the bug that wasted the most runs was mine, in the harness

An edit that removed the `--close-window` block took the neighbouring
`--raise-window` **implementation** with it and left the option parsing
perfectly. Several System 7 runs then passed `--raise-window`, had it do
nothing, pressed on the panel's own window — which covers the icon once
the panel returns to the front — and reported that no drag happened. That
was true, and it was not about System 7.

**A flag that parses and does nothing is the worst kind of failure**, and
the tell was there in every run: no `window-raised` photograph among the
screenshots. The step is restored, and it takes its own photograph so its
absence is visible.

### Also fixed: "on us" had lost its reference

Moving the drop summary out of `draw_hit` and into `draw_log` left the
comparison without the dialog to compare against, so every drop read
`elsewhere` — including the ones that landed on the panel. `draw_log`
takes the dialog now.

| | |
|---|---|
| System 6.0.8 | works |
| System 7.0.1 | works |

---

## System 7.5.3: the cdev runs; the drag is declined at the file lookup

*Where it got to:* the panel opens and draws, `windowKind` is **20** as on
7.0.1, presses are seen (19 of them), the deep search runs, and the
window-to-folder pairing is **correct and title-checked**:

```
pairs (3 seen), pairing cost 415
 @00398DB6 d=00000013 -1 Apple Menu Items     <- rejected, title mismatch
 @00398FB0 d=00000002 -1 System 7.5           <- the volume window, good
 137,9 About System                            <- the disk HAS placed icons
drags: 0 mb=5 mv=0 fail=1
```

The mid-drag photograph settles what the press did: the volume window is
frontmost, **System Folder is selected**, and the Finder's own dashed
outline is following the pointer. So the press landed on the icon, our
sender declined, and the Finder took the drag — the safe failure, working
as designed.

`fail=1` says exactly where: the commit ran and **`file_at` found no icon
under the point**. Not the window, not the pairing, not the button, not
the mouse. That is the one step left to instrument — print the local point
and the folder's fdLocations side by side — and it is where the next run
should start.

### Harness facts for 7.5.3, each of which cost a run

- **The Apple menu's `Control Panels` is at y=131 and is hierarchical.**
  Releasing on the parent opens the folder, which is what is wanted;
  y=115 — which works on 7.0.1 — selects **Chooser** here and opens
  something else entirely.
- **WProbe's icon in that folder is at (357, 95)**, in a folder of 43
  items rather than 13.
- **The volume window's close box is at (22, 37)**, not the 7.0.1 value.
- **The volume window and the Control Panels window overlap almost
  exactly**, so no click can say which one it means. That is why the
  sequence now **drags while the Finder's window is still frontmost**
  instead of re-opening the panel first and trying to reach back
  underneath it — removing the question rather than answering it.
- **The system heap is far tighter**: `sysfree` 0x43DE, about 17 KB,
  against roughly 196 KB on 7.0.1.

### The sweep so far

| | |
|---|---|
| System 6.0.8 | works |
| System 7.0.1 | works |
| System 7.5.3 | runs; drag declined at the file lookup, cause not yet found |

---

## Where the Drag Manager actually is — measured, and it changes the plan

The app should not use this machinery where the system already has a Drag
Manager: there, a control panel registers a **receive handler** and the
Finder — which is a drag sender on those systems — does the sending. That
is smaller, supported, and somebody else's to maintain.

So the switch is `Gestalt('drag')`, probed rather than inferred from a
version number, and here is what it answers:

| system | `Gestalt('drag')` |
|---|---|
| System 6.0.8 | **absent** (−5551, `gestaltUndefSelectorErr`) |
| System 7.0.1 | **absent** (−5551) |
| System 7.5.3 | **PRESENT**, `0x80000003` |

`_Gestalt` itself is implemented on 6.0.8 — the answer is a selector
error, not a missing trap, which is worth knowing because Gestalt only
arrived in System 6.0.4 and the probe checks the trap against
`_Unimplemented` before calling it.

*Gestalt is library glue in these headers and this resource links no
library, so the trap is called directly — the same shape as
`NGetTrapAddress` becoming `GetOSTrapAddress` and `Create` becoming
`PBHCreate`. That is three times now; in this project the high-level call
is the exception.*

### What that settles

**System 7.5.3 does not need chasing.** Its `file_at` refusal is real and
unexplained, and it is also on the far side of the switch: an app there
uses the Drag Manager and never reaches this code. The one step left to
instrument is written down above if it is ever wanted.

**The two systems that need this machinery both work.** 6.0.8 and 7.0.1
are exactly the systems with no Drag Manager, and both take a drag.

**And the sweep's remaining question changes.** It is no longer "how far
up does the sender go" — up is the Drag Manager's job from 7.5 — but
**how far DOWN**: which is the oldest system that can run a cdev at all
and still do this. That is the 4.1/5.0 direction, and it is bounded by the
plug-in Control Panel existing rather than by anything in the code.

### The design tension, stated rather than buried

This is two implementations of one feature, which the host program's
design rules warn against. The defence is that the alternative is not supporting the
floor at all, and that the second implementation is *small* — registering
a receive handler against an API somebody else maintains, versus the
sender machinery here. The seam is one Gestalt call, and it is the kind
the host's rules ask for: a capability probed, not a version tested.

---

## The floor: System 4.1 (April 1987) runs the cdev; the pairing does not

*Image:* `System Tools 2.0.1`, System 4.1 / Finder 5.5, an 800K floppy
booted directly. Added to the harness as `--system 41`.

**The vehicle reaches at least this far back.** That System Folder holds
`General`, `Keyboard`, `Monitors`, `Mouse`, `Sound` and `Startup Device`,
all of type `cdev` — so the plug-in Control Panel already exists in April
1987, and the question of how far down this can go is not bounded by the
Control Panel after all.

**And the sender runs there.** The report-free build opens, is selected in
the Control Panel's list, and draws.

```
sys 00000410   dragmgr: no Gestalt
sysfree=00003E12 max=00003BCA
1 "System Tools" k16 v @000F94EA
drags: 0 mb=7 mv=0 fail=7
pairs (0 seen), pairing cost 0
presses: 31790
```

- **`windowKind` is 16**, the same as System 6 — it is System 7 that is
  the odd one at 20. The `>= 8` test covers all three.
- **`_FindWindow` is called 31790 times.** System 4.1's Finder calls it
  continuously, not only on mouse-downs, which is a far heavier load on
  the patch than System 6's eight-per-press.
- **`pairs (0 seen)`.** Nothing is ever paired, so every press fails —
  `fail=7` out of `mb=7`.

**The likely reason, and the next thing to test:** the burst rule watches
`_HFSDispatch` selector 9, `PBGetCatInfo`. A 1987 Finder has no reason to
use it — `PBGetFInfo`, the older flat call on trap `_GetFileInfo`
(`0xA00C`), reads a directory by index perfectly well and is what software
of that age uses. If so, the folder identity there would arrive as a
**working directory reference number** in `ioVRefNum`, which is the
original hypothesis this whole project started by falsifying on System 6
— and might simply be true on System 4.1.

### And the crash that had to be fixed first: ID = 12

The diagnostic build died on System 4.1 with *"Sorry, a system error
occurred. ID = 12"* — an unimplemented trap — which takes the Control
Panel down with it.

It was the **Gestalt guard**, and the way it was wrong is worth keeping.
It compared the `_Gestalt` trap against `_Unimplemented`, which is the
textbook check. On a 128K ROM the toolbox trap table has 512 slots, `0x1AD`
is inside it, and **the slot need not hold `_Unimplemented` for the trap to
be absent**. So the guard passed and the call died.

The version answers it without asking the trap table anything:
`SysVersion`, low memory `0x015A`, in BCD — `0x0410` here — and Gestalt
arrived in 6.0.4. No trap, no table, no assumption.

*Both Memory Manager calls were guarded the same way in the same pass and
that guard was NOT the problem — they work on 4.1. Which is the useful
half of the lesson: the trap-table check is fine for OS traps and was
wrong for this one.*

### The sweep

| | |
|---|---|
| System 4.1 | cdev runs, sender runs; **no pairing**, so no drag |
| System 6.0.8 | works |
| System 7.0.1 | works |
| System 7.5.3 | has a Drag Manager; the app should use that instead |

---

## The heap trick works on System 7.0.1 too — at a different offset

The icon position is at a fixed distance from the item's name in the
Finder's application heap. Measured per Finder rather than assumed,
because nothing about it is portable by construction:

| system | `SysVersion` | offset from the name |
|---|---|---|
| System 6.0.8 | `0x0607` | **−96** |
| System 7.0.1 | `0x0701` | **−136** |

Same mechanism, different constant. Which is the important half of the
result: **the offset must be discovered at runtime, not written down.**

### And the discovery is self-checking, which is what makes it usable

The procedure is the one that found −96 in the first place, and it needs
nothing known in advance:

1. take an item the catalog **does** have a position for;
2. find its name in the application heap;
3. measure back from the name to those four bytes.

Every input comes from the machine, and the answer is checked by
construction — an offset that does not reproduce a position the catalog
already states is simply not the offset. Require agreement across several
placed items and a wrong one cannot survive.

Then apply it to the items the catalog says nothing about, which are the
ones that cannot otherwise be picked up at all.

**So this stops being a magic constant and becomes a calibration.** That
is a much better thing to ship: it is the difference between "this Finder,
this version" and "any Finder that keeps the position near the name",
and the second is checkable on the machine in front of the user.

### The wrinkle still in the way

Each run calibrated from **one** item and read `nooff` on the second — the
same fault `MadeByProbe` showed earlier: **the first occurrence of a name
in the heap is not necessarily its record.** A calibration that wants
agreement across several items has to scan *every* occurrence of a name
and keep the offsets that recur, rather than the first that turns up.

That is a bounded piece of work and it is the next thing this needs.

---

## System 4.1 uses the same offset as System 6, and two corrections

```
sys 00000410  hunt -96      128,0 Alpha      pairs (1 seen)
```

| system | `SysVersion` | offset from the name |
|---|---|---|
| System 4.1 | `0x0410` | **−96** |
| System 6.0.8 | `0x0607` | **−96** |
| System 7.0.1 | `0x0701` | **−136** |

**The Finder's record layout held from April 1987 through System 6 and
moved at System 7.** Which is a good shape for a calibration to meet: two
values across four years, discovered rather than known, and the procedure
does not care which it finds.

### Correction: the pairing is not absent on 4.1, it is intermittent

The earlier note said System 4.1 never pairs. That was one run. This one
reports `pairs (1 seen)` on the same system with the same build — the
difference being that this run opened the volume window while the panel
was already up, giving the burst rule its chance, and did not go on to
drag. **So the burst rule does fire on 4.1 sometimes**, and "never" was a
conclusion drawn from a single observation, which is the mistake this file
keeps recording.

What is still true: it did not fire in the drag run, and every press there
failed for want of a pair.

### And the answer to "is the heap better than the catalog"

Better for one thing, and it does not replace the catalog — it changes
what the catalog is for.

| | positions | names | calibration | check |
|---|---|---|---|---|
| catalog `fdLocation` | placed items only | yes | — | ground truth |
| Finder's heap | **every** item | no | needs the catalog | needs the catalog |

The heap is the better source of **positions**, because it has them for
items the catalog says nothing about — which is the whole failing case.
The catalog is still what supplies the names, what calibrates the offset,
and what proves the offset right. Neither is sufficient alone, and the
pair of them is stronger than either: a position that comes from the heap
via an offset the catalog validated is better evidence than a position
read straight out of the catalog, because it had to survive a comparison.

---

## Calibration by agreement, and the 4.1 hypothesis was wrong

### The calibration now scans every occurrence and requires agreement

The first version took the first occurrence of a name and the first
offset that matched, and read `nooff` on the second item every time. Now
every occurrence of every name is scanned, every offset that reproduces
that item's **known** position is collected, and the offsets are tallied
across items. A count of *n* means *n* placed items each have some
occurrence where that offset is right.

```
System 4.1     cal 4it  -96 x3   -94 x2   of 4
System 6.0.8   cal 4it  -96 x3            of 4
System 7.0.1   cal 4it  -86 x2  -136 x1   of 5
```

**Decisive on 4.1 and 6.0.8: −96, three items of four.** And the
runner-up on 4.1 is `−94` — the true offset's two-byte neighbour, trailing
it. That is the shape a working discriminator should have, and it is
better evidence than the winner alone.

**Not decisive on 7.0.1.** No offset explains all four items; −86 covers
two and −136 — which is what the single-item version reported — covers
one. So the earlier −136 was the minority answer, and neither value is
established. Either that Finder's records are not uniform, or some names
are not being matched. **The System 7 offset should be treated as unknown
rather than as −136**, which is a correction to the previous section.

*What the numbers do establish is the method*: on the two systems where a
clear winner exists, it is the same value that was found independently
and, on System 6, verified against a position the Finder later wrote.

### And `_GetFileInfo` is not how System 4.1 reads a folder

The guess was that a 1987 Finder would use `PBGetFInfo` on `_GetFileInfo`
rather than `PBGetCatInfo`, which would explain why nothing pairs there.
Counted:

```
sys 00000410   gfi 7
```

**Seven calls in an entire session**, including opening a window. That is
not an enumeration; it is not the mechanism. The hypothesis is wrong.

*The next lead, unpursued:* that volume's root holds a file called
`DeskTop`, type `FNDR`/`ERIK`. Finders of that era kept their own database
of what a window contains, and reading it would need no per-item File
Manager call at all — which would fit a count of seven.

---

## System 4.1 takes a drag. The answer was a census, not a fourth guess.

```
System 4.1     drags: 1 mb=1 mv=7 fail=0
               105,105 "System Tools" > System Folder -> 250,265
               drop System Folder v=-1 d=00000002 on us
System 6.0.8   drags: 1 mb=1 mv=7 fail=0
System 7.0.1   drags: 1 mb=7 mv=6 fail=1
```

**All three systems without a Drag Manager now take a drag**, from April
1987 to 1991.

### Three guesses, then one measurement

How System 4.1 reads a folder was guessed three times and wrong three
times: that it used `PBGetCatInfo` in some other way, then `PBGetFInfo`
on `_GetFileInfo` (counted: **7 calls in a session**, not an
enumeration), then the `DeskTop` file at the volume root.

A census by selector settled it in one run:

```
sys 00000410   gfi 7   hfs 129
sel 9:127  10:1  11:1
```

**It uses `PBGetCatInfo` — selector 9 — 127 times out of 129.** The call
was never the problem.

### What was wrong were two System 6 assumptions in the rule

**It required a leading `ioFDirIndex` of −1 to start a burst.** That is how
System 6 and 7 begin reading a folder. System 4.1 simply does not make
that call, so the burst never started.

**And it recorded the pair only on a call that was NOT GetCatInfo.** That
works where a folder read is bracketed by other traffic; on 4.1 there are
three such calls in a whole session, so the check ran three times and
never at the right moment.

Both are gone. The rule is now two steps in this order, on **every** call:
record a pending folder if the front window has changed, then remember
this call's folder. The order is the fix — it catches a run of GetCatInfo
calls with the window changing part way through.

**Being looser costs nothing, because a pair is checked against the
window's title before it is believed.** That check was added for System 7,
where the burst rule mispaired; here it is what makes relaxing the rule
safe. A guard built for one system paid for a fix on another.

### Two bugs of my own, both silent

- **The census scaled `D0`** — which is the selector the real trap still
  has to receive, and which the pairing compares against 9. It was
  restored before the trap ran, so nothing crashed; everything in between
  saw a doubled selector. Caught by reading the code rather than by a
  symptom.
- **Two edits did nothing at all.** Anchor text that no longer matched, a
  `replace` that silently changed nothing, and a run that measured the old
  build — the same class as the `--raise-window` implementation deleted
  earlier. Both times the tell was in the output and both times I read the
  output as a result. **An edit that does not apply should be as loud as
  one that fails to compile.**

---

## One method, three systems — and asking for that exposed a real bug

The question was whether the System 4.1 fix could be used on 6.0.8 and
7.0.1 as well. It already was: the fix was not a special case but the
removal of two System 6 assumptions, so there is one rule. But *checking*
that claim rather than asserting it is what found the following.

### The instrumented build was changing what it measured

`4.1 takes a drag` was measured with the **diagnostic** build. Run the
**sender-only** build on the same machine and it takes no drag at all.

The report walks folders on every redraw. That traffic is `PBGetCatInfo`,
it goes through the patch, and **it supplied the very call the pairing
rule was waiting for.** The sender makes no such calls, so on 4.1 it
paired nothing.

*A diagnostic that changes the thing it measures* — and the only reason it
came to light is that "is this generic?" was treated as a question to test
rather than a claim to defend.

### What the rule was actually missing

It needs a call **after** the window has been made, to notice the front
window changed. System 6 and 7 provide one — the Finder calls
`PBGetWDInfo` twice immediately after creating a window. System 4.1 does
not: its whole session is `sel 9:127 10:1 11:1`, and neither of those two
falls in the right place.

So the finalising step no longer waits for anybody's File Manager call.
**The panel is running anyway**: on its idle time it looks to see whether
the front window has changed since the patch remembered a folder, and
records the pair itself. No trap, no version test, nothing to depend on
the Finder doing.

### The result: one binary, three systems

The same `WProbe-min.bin`, unmodified between runs:

| | |
|---|---|
| System 4.1 | `dropped System Folder on us` |
| System 6.0.8 | `dropped System Folder elsewhere` |
| System 7.0.1 | `dropped System Folder on us` |

*(`elsewhere` on 6.0.8 is correct — the drop there lands on the Finder's
window, because the harness now drags while it is frontmost.)*

**And there is no version test anywhere in the sender.** Checked rather
than claimed: the only system-shaped constants left are `windowKind >= 8`
— `userKind`, documented, and true from 1987 to 1991 — and `_HFSDispatch`
selector 9. `SysVersion` appears once, inside the diagnostic build, to
decide whether Gestalt may be called at all.

**3181 bytes**, one code path, four years of Finders.

---

## The drop lands on the panel's own window, on all three

Asked directly, because it is the thing the host would actually use — a
file dragged out of the Finder and let go on the control panel.

Same `WProbe-min.bin`, the sender only:

| | drop point | result |
|---|---|---|
| System 4.1 | 250,265 | `dropped System Folder on us` |
| System 6.0.8 | 300,275 | `dropped System Folder on us` |
| System 7.0.1 | 250,150 | `dropped System Folder on us` |

The System 6 case needed a drop point of `300,275` rather than the
`300,200` used before, and the reason is worth keeping: the harness now
drags while the **Finder's** window is frontmost, so `300,200` is over
that window and resolves to it — correctly. The panel extends below it,
and a drop at `300,275` is over a part of the panel nothing covers.

**Which also shows the resolution has the right semantic.** The panel is
*behind* the Finder's window at that moment, and the drop still resolves
to the panel, because what is being asked is "the frontmost window
containing this point" rather than "the frontmost window". Dropping on a
visible part of a background window means that window, which is what a
user would expect and what the Drag Manager does on the systems that
have one.

---

## Dragging a file the catalog places nowhere — wired in, and it works on two of three

The last functional hole. An item with `fdLocation` `0,0` has never been
placed by a Finder and the catalog cannot say where its icon is, so it
could not be picked up at all. The calibrated heap lookup is now in the
**sender**, not only in the report:

- calibrate once, from items the catalog *does* place, requiring **two to
  agree** before an offset is used;
- fall back to the volume root if the folder being dragged from has no
  placed items, because the offset is a property of the **Finder** and any
  laid-out folder will calibrate it;
- and in `file_at`, an item at `0,0` gets its position from the Finder's
  heap instead.

*Measured*, same sender-only binary, dragging an item whose catalog entry
reads `0,0`:

| | |
|---|---|
| System 4.1 | `dropped Alpha on us` |
| System 6.0.8 | `dropped Alpha on us` |
| System 7.0.1 | **no drag** |

**System 7.0.1 fails for the reason already recorded**: its calibration is
not decisive. Four placed items give `−86 ×2` and `−136 ×1` — no offset
explains them all — so whichever is taken is unreliable, and the lookup
does not find the icon.

**And the failure is the safe one.** A wrong or missing offset means the
item is simply not found, which is where it was before any of this
existed. It does not pick up a different file. The guard that makes that
true is the same one throughout: an offset has to reproduce positions the
catalog already states, and two items have to agree, before it is used at
all.

`cal 0it none` in the diagnostic build is that guard working — the folder
being dragged from held one unplaced file and nothing to calibrate from,
so the calibration declined rather than guessed.

**Sender: 3899 bytes**, up 718 for the calibration and the lookup.

*What would close System 7.0.1* is understanding why no single offset
covers all four of its placed items — whether its records are not uniform
or some names are being matched in the wrong place. That is the same
loose end the calibration section already names.

---

## Why the offsets disagreed: it was never the Finder, it was the layout

The per-item detail on System 7.0.1, rather than the tally:

```
d 256,0   -136 -86 -84   Alpha
f 300,20   -             Placed
f  64,0   -86            Read Me
```

**Every icon on that primed disk sits at `v = 0`.** The four bytes searched
for are the `Point` itself, so an item at `v=0` is searched for as
`00 00 00 hh` — a pattern that occurs all over a heap. `Alpha` alone
matched at −136, −86 and −84, and no tally can choose between three
coincidences and a fact.

System 6's disk has its items at `v = 1`, `50` and `−1`. Those are rare
patterns, they agree, and it calibrates cleanly:

```
d  64,0   -96 -94   Alpha
f 273,1   -96       Font/DA Mover
d  28,50  -96       Microsoft Word
```

Note `Alpha` there: at `64,0` it yields the right answer **and** a spurious
neighbour, exactly as the weak-signature theory predicts, while the two
strongly-placed items yield only `−96`.

**So the difference between the systems was not the Finder's record
layout.** It was where the icons happened to sit — and System 7.0.1's disk
had been primed into a single row, which is the worst possible case.
Several sections above attribute this to System 7 and are wrong.

### The fix, and it is the same discipline as everything else

Calibrate only from a position that can **discriminate**: both components
non-zero. That costs the items that cannot tell an answer from a
coincidence and keeps the ones that can. If none remain, the calibration
declines — the safe failure — rather than choosing between three offsets
that all "fit".

*Re-measured with the filter in, sender-only build, dragging an item whose
catalog entry reads `0,0`:*

| | |
|---|---|
| System 4.1 | `dropped Alpha on us` |
| System 6.0.8 | `dropped Alpha on us` |
| System 7.0.1 | declines — that disk has no discriminating item |

**And `Placed` is worth one line of its own.** Its catalog says `300,20`,
which is strong, and it yields nothing at all — because the panel wrote
that position itself and the Finder never drew the icon there. An item
whose catalog position the Finder has not honoured is exactly an item the
heap will not corroborate, and the search correctly says so.

*What would let 7.0.1 calibrate* is one folder laid out in more than one
row, since a second row has `v != 0`. Its Control Panels folder has
thirteen items and is certainly multi-row; the calibration currently tries
only the dragged-from folder and the volume root.

---

## Working around a disk with nothing well-placed on it

Requiring a strongly-placed item was safe and often useless: a single row
of icons is a perfectly ordinary way for a folder to look, and on such a
disk the calibration simply declines. Two workarounds, both now in.

### 1. A candidate must explain every item it can be checked against

Not just one. For a candidate offset, every item whose name is found in
the heap must fit: exactly, for the ones the catalog places; and as a
position inside the window that is showing it, for the ones it does not.
That second clause uses the **unplaced** items as evidence — the very ones
a well-placed item cannot help with.

An item whose name is not in the heap is passed over rather than treated
as a contradiction. Requiring every item to be found made every offset
fail on a disk where some names never appear.

### 2. The click is the constraint that was being thrown away

The decisive one. When this runs, **the user has just pressed on the
icon** — so its position is known to within 32 pixels out of a screen of
512 by 342. That is a far stronger constraint than any placed item's, and
it is available exactly when it is needed.

So the click picks the candidate and the placed items only have to
**corroborate** it: an offset that puts the pressed icon under the pointer
*and* reproduces a position the catalog states is not a coincidence worth
worrying about. One corroboration is then enough, where three agreeing
items are needed without a click.

And because the folder being dragged from may hold nothing else at all —
a new folder with one new file in it is exactly the case this is for — the
calibration reads a **second folder** purely to corroborate. The offset is
a property of the Finder, so any laid-out folder will do.

### Where that leaves it

*Sender-only build, dragging an item whose catalog entry reads `0,0`:*

| | |
|---|---|
| System 4.1 | `dropped Alpha on us` |
| System 6.0.8 | `dropped Alpha on us` |
| System 7.0.1 | still no drag |

**System 7.0.1 still does not calibrate**, and I have not found why. The
disk is the hard case — every icon in one row at `v=0`, and the file being
dragged is alone in its folder — but the click constraint was supposed to
be enough for exactly that, and it is not. What is known: `off 0/0`, so no
candidate survives; the paired folder and the root are both being read;
and the same code calibrates on the two older systems.

**The failure remains the safe one** — the item is not found, which is
where it was before any of this — and the two systems that need this
machinery most are the two it works on. But this is an unfinished
investigation and not a solved problem, and the next step is to print the
candidate offsets the click admits on 7.0.1 rather than only the verdict.

---

## Does the Finder mark what is selected? Not where the name is.

The idea: a user who has pressed an icon has **selected** it, and a
selection is a fact the Finder must hold — where a position is one it may
never have written. If the selection were marked near the name, the
dragged file could be identified with no icon position at all and the
whole `0,0` problem would go away.

*Tested* by checksumming the 128 bytes before each item's name and running
twice with **different icons clicked**:

```
clicked System Folder        clicked Alpha
 0280 Alpha                   0280 Alpha
 0004 Desktop                 0004 Desktop
 EDC2 Font/DA M               EDC2 Font/DA M
```

**Byte for byte identical.** Selecting `Alpha` changed nothing in the 128
bytes before its name.

### Two readings, and the second is a limitation of the test

**Either the selection is not marked there** — it may be elsewhere in the
record, or in a structure of its own.

**Or it was already cleared by the time the panel could look.** This is the
honest caveat and it is a real one: to read the report the panel has to be
opened, which means going to the Apple menu, which deactivates the
Finder's window. A highlight bit that clears on deactivation would be gone
before anything could see it. The test cannot currently distinguish those
two, because the panel has no way to look while the Finder is still
active.

### And one thing it does say

The bytes around the name are **stable across selection**, which is
consistent with what is found by name being a directory or name cache
rather than a live per-icon record. If so, that would also bear on the
offset trouble: a cache would hold the position the catalog last wrote and
not necessarily the one the Finder is drawing — which is exactly the
distinction that made `Placed` yield nothing.

*What would settle it* is reading the bytes from inside the `_FindWindow`
patch, while the Finder is active and the click is being handled, rather
than from the panel afterwards. That is a few lines — the patch already
runs at exactly the right moment — and it is where this should go next.

---

## Reading the selection from inside the patch: still not there

The previous test had a real flaw — the panel can only look after the
Finder's window has been deactivated, so a highlight that clears on
deactivation would already be gone. Fixed, and the answer did not change.

*How*: the name addresses are cached once at idle, so the checksum itself
is cheap enough to take **inside the `_FindWindow` patch** — while the
Finder is frontmost and a click is being handled. And since the Finder
selects *after* `_FindWindow` returns, a press can never see its own
selection, only the one left by the press before it. So the harness
presses twice: the second press reads the first one's selection.

```
selected Alpha, then pressed     snap 0280 6682 EDC2 96D6
selected StuffIt, then pressed   snap 0280 6682 EDC2 96D6
```

**Identical.** Read at the right moment, in the right context, with the
Finder active and different icons selected: the 128 bytes before each
item's name do not carry which item is selected.

### What this leaves, and it is a coherent picture

The structure found by name holds **positions** and not **UI state**:

- unplaced items' positions *are* there, which is why the drag works on
  4.1 and 6.0.8 at all;
- `Placed` yielded nothing, because its `fdLocation` was changed in the
  catalog behind the Finder's back and what is in the heap did not follow;
- and selection is not there at all.

That is consistent with a per-item record the Finder keeps in order to
lay a window out, with selection tracked somewhere else entirely.

**So this line is closed for that neighbourhood.** Finding the selection
would mean searching the heap for something that *changes* between two
presses rather than looking where the name is — which is a different and
much wider search, and one whose result would still be a Finder-private
structure. The position route already works on the two systems that need
it, and it is checked; a selection route would have to earn the same.

---

## The switch: probe for a Drag Manager, and stand aside where there is one

The design has always been "our machinery only where there is no Drag
Manager". That is now enforced in the code: `patch_install` asks, and
where the answer is yes it installs nothing at all.

```
System 7.5.3   Drag Manager present: standing aside
System 7.0.1   dropped System Folder on us
System 6.0.8   dropped Alpha on us
System 4.1     dropped Alpha on us
```

### Why it is a probe and not a version test

Tempting to say "System 7 and later use the official one". **It would be
wrong on the case that motivates it.** System 7.5 ships a Drag Manager;
7.0 and 7.1 have one *only if the user installed Apple's extension*. A
stock 7.0.1 measures as `absent (-5551)`, and a version rule would hand
the work to something that is not there.

`Gestalt('drag')` answers the real question. The version is consulted for
exactly one thing: `_Gestalt` did not exist before System 6.0.4, so on an
older machine it must not be called — read from `SysVersion` at `0x015A`,
which needs no trap, because the trap-table check that would normally
guard such a call is what took System 4.1 down with an ID = 12.

### Two bugs this shook out, and the second is the interesting one

**A missing clobber list.** The inline `Gestalt` declared only the two
registers carrying its arguments. A Toolbox trap may use all of D0–D2 and
A0–A1, so the compiler was free to keep something live across it. *A
missing clobber does not fail where it is written*: 7.5.3, which returns
before reaching anything else, was fine.

**Calibrating inside the drag.** The bigger one. The calibration sweeps
the whole application heap — a megabyte on a 1 MB machine, several on a
4 MB one — and it was running inside the trap patch, holding the mouse,
with the Finder stopped behind it. System 6 was small enough to get away
with it. **System 7.0.1 was not: the drag stopped working entirely**,
while the instrumented build, which had already calibrated, carried on
fine and hid it.

It happens at idle now, where a slow sweep costs nothing and holds
nobody. That is the same rule as moving the folder walk to after the mark
is drawn, arriving a second time: **do not do expensive work while you
have the machine.** Twice now the symptom has been "the drag silently
stops working on the larger system", and twice the cause has been work
done in the wrong place rather than work done wrongly.

---

## How reliable is stock System 7.0.1 actually? Counted, not asserted.

A fair challenge: after the calibration fix there was exactly **one**
successful sender run on 7.0.1, which is an observation and not a claim
about reliability. So it was repeated.

| case | result |
|---|---|
| a file the catalog places | **4 runs, 4 successes** — `dropped System Folder on us` |
| a file the catalog places nowhere | still fails |

**So the placed case is reliable there now**, against a clear pattern of
failures before, and the earlier unreliability was real with two causes,
both found: the harness aiming at a window the panel covered, and the
calibration sweeping a 4 MB heap from inside the drag. Neither was
mysterious once measured; both looked like "System 7 does not work".

**The unplaced case still does not work on 7.0.1**, and moving the
calibration to idle did not change it — so it is not a time problem. It
is the one recorded above: that disk's icons give the calibration nothing
it can discriminate with.

### What that means in practice

For the case this is actually for — a user dragging a disk image they
keep on their Mac — the file is almost certainly placed, because it got
there through a Finder and has been in a window that was closed. The
unplaced case is the freshly-arrived-from-a-tool one, and it has a true
one-action answer: close the window and open it again.

So on stock 7.0.1 the honest summary is **works, with one documented
exception that the user can clear themselves** — rather than either
"works" or "does not work reliably".

---

## Putting this into the host program: the plan, and what it costs

*Summarised: the original named the host's own source files and
documents line by line.*

Not started at the time of writing, and deliberately: the files it would
have to edit had uncommitted work from another session in them, and
editing alongside that is the one thing likely to lose somebody's work.
It waited for a clean tree.

**What it costs, which is a decision rather than a detail.** The host's
cdev was **38740 bytes** of code resource. The sender is **4676**, so the
panel becomes about **43.4 KB** — a twelve percent increase in something
that lives in the System 6 system heap, on a machine where that panel had
already once failed to install with `memFullErr`. So a run at the 1 MB
floor before any of it is believed, because the failure is intermittent —
heap fragmentation as much as exhaustion — and one successful install
proves less than it looks like.

**Stage one — the sender.** Two new files that nothing else references,
and three calls in the panel: install at `initDev`, remove at `closeDev`,
finish a pairing at `nulDev`. A drop on the panel's own window arrives as
vRefNum, dirID and name — exactly what the panel's Load button already
hands its loading path — so this adds a second door to that path and
changes nothing after it.

**Stage two — the Drag Manager, for 7.5 and later.** The sender stands
aside where `Gestalt('drag')` says one exists, so without stage two those
systems have no drag. A receive handler is small, supported, and the seam
is the same Gestalt call: **a capability probed, never a version tested**,
and the measurement that forces it is that stock System 7.0.1 has no Drag
Manager. A separate piece of work, and a separate commit.
