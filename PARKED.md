# What the drag library tried and could not make work

Each entry is something that is **not in the running code**, kept in
enough detail that nobody has to find it out again. It says what ran
and what did not, because the two are worth different amounts to
whoever picks it up.

---

## The sender on System 7 — why it does not work there, and what it would need

**There is no sender for System 7.0, 7.0.1 or 7.1, because one could not
be made to work properly.** One was written. It took a drop on 7.0.1 in
the runs that were made, and it was never reliable: it had to work out
the Finder's record layout on the machine while the user dragged, each
rule in that working-out was there because a wrong answer had already
got through, and one part of it — which folder a window shows — was
never solved. The maintainer's decision of 2026-10-20 is that nothing
here is to be worked on for those Systems. What follows is the record of
what was learned, not a plan.

Dragging a file onto a program's window works on System 7.5.3, Mac OS
8.1 and 9.2.2 because those systems have a Drag Manager and
`fdrop.c` registers a receiver with it. On **System 7.0.1 and
7.1 without Apple's Drag and Drop extension** nothing is taken: those
Finders do not carry the call site the sender recognises
(`NOTES.md`, the drag routine across Finders), so the patches
stand aside there on their own. Installing Apple's extension brings the
drag through the Drag Manager — the sender asks `Gestalt('drag')` and
stands aside where there is one, so the two can never both be in play.

`NOTES.md` is the archive; this is the index to it.

**The one fact underneath all of it:** three Finders keep an item's
record differently, and System 4.1 and 6.0.8 agree with each other on
every number while 7.0.1 differs on every one.

| | 4.1 | 6.0.8 | 7.0.1 |
|---|---|---|---|
| `fdLocation`, from the name | −96 | −96 | −82 |
| selected, from the name | −77 | −77 | −63 |
| what "selected" reads as | `0x02` | `0x02` | bit `0x08` of `0x80` |
| one record to the next | 118 | 118 | 120 |
| the catalog's frame, from the drawn one | same | same | v +28, h +16 |

So a sender for System 7 cannot write any of those down: it has to
**measure** them on the machine, and measuring is what most of that
sender's code was.

*What it needed, in the order it was learned:*

- **A search for the position offset, not a constant.** Candidate offsets
  across ±512 bytes of each item's name, tested against every item in the
  window at once. It needs every defence it grew, because each one is a
  measured false positive: an offset where every item reads the SAME
  position is not reading positions (name − 104 on 4.1 holds the stride
  and a zero); an unplaced item's value must at least be a position
  inside the window and not 0,0 (name − 98 on 4.1); and something must be
  under the pointer, or a click on empty space settles on an impostor and
  locks (name − 370 and name − 212 on 6.0.8, and every drag afterwards
  refused). A press that has MOVED is what makes an answer final; a click
  is provisional and the next drag asks again.

- **The catalog's frame is not the drawing frame, and the vector must be
  measured too.** A System 7 Finder keeps both positions in the record
  four bytes apart and writes the one it does not draw from into the
  catalog. So a placed item's value must differ from the catalog by a
  vector that every placed item agrees on — zero being what 4.1 and 6.0.8
  give — with the candidates taken from the first placed item's own
  record, a non-zero one needing two items to agree, and a non-zero one
  preferred over zero where both are accepted. `file_at` then adds it
  before comparing a catalog position with the pointer, and says nothing
  at all until some calibration has related the two frames. Without this
  the two rules are unsatisfiable together and **every drag on a machine
  that has been shut down once is refused**.

- **The selected byte and the record stride, learned from what the user
  does.** A plain click leaves one item selected and a shift-click adds a
  second, which is two moments in which the selected SET is known; the
  byte that splits the same way both times is the flag, and the distance
  between the two records is the stride. That is what makes a drag of one
  icon carry the other selected ones, and there is no list anywhere else
  to read it from.

- **`StillDown()` rather than the `MBState` byte.** On System 7 that byte
  does not refresh while a patch holds the machine: the gate spun 62675
  times, never saw the button come up, and held the whole tick bound on
  the first press anywhere. **This one is in the sender as it is now**,
  because asking the right question costs one trap and the cheaper
  reading is what fails.

- **A press refused is remembered by its GESTURE, not its point.**
  System 6 calls `_FindWindow` about eight times per press so a
  button-up is always seen in between; System 7 calls it far fewer times,
  so a flag alone lived into the next press and refused it — two presses
  considered in a whole session. **This one is in the sender as it is now** too: it is
  cheap and it is right on both.

- **The panel's own window has to be skipped explicitly.** Under System 6
  it is a desk accessory's, window kind negative, and the kind test skips
  it already; under System 7 the Finder hosts it and the kind is the
  Finder's business, so `FDROP_WIN` is compared instead.

- **The window must be paired with the folder it shows, and on System 7
  that is unsolved.** The ring of folders the Finder has asked the File
  Manager about never holds a folder whose volume was mounted before the
  panel opened, and `NOTES.md` has the diagnosis. It bites on 6.0.8 too; what
  hides it there is the volume-name fallback, and a folder's window has
  no fallback on either system.

*And what is NOT needed, so that another attempt does not carry it:* the frame
vector and the offset search are **only** for System 7. The drop queue,
the `_FindWindow` and `_GetMouse` patches and the whole receiver path are
shared and are still here.

**But the sender another attempt would join no longer takes the mouse, and no
longer searches for anything**, and everything above was built for one
that did both. Since 2026-10-19 it lets the Finder drag and answers the
Finder's own `_GetMouse` while the pointer is over the panel, and it
finds the Finder's records by walking its heap block by block: on 4.1
and 6.0.8 every icon is a block whose size identifies it, and the block
names the window it is drawn in (`NOTES.md`, the Finder's drag
routine and the Finder's records). The window-to-folder pairing and the
`_HFSDispatch` patch went with that. Two things are now KNOWN about
System 7 rather than unknown: its Finder (7.0, 7.0.1 and 7.1, read
statically) drags an icon with a loop of its own taking C arguments and
one `_GetMouse` a pass, returning last point minus start point rather
than a sum, and the action-procedure call site the sender recognises is
not in it (`NOTES.md`, the survey) -- so another attempt needs a
recognition of its own and an answer of a different shape. Whether a
System 7 Finder keeps its icons as blocks the same way has not been
looked at; the record offsets above say its layout differs. Nor has the
`Mouse` low-memory global, which the sender reads and which that
work found stale on 7.0.1 while a patch held the machine.

*Cost, measured:* the sender with the System 7 work in it was 9,694 bytes of a
50,486-byte control panel; without it, it was **4,438** of
71,212, the same panel built with and without it, and the whole of
`fdrop.c` plus its assembly half was 7,308 bytes of text.

**WHAT ANOTHER ATTEMPT SPENDS IS NOT SYSTEM HEAP, and this entry said it was.**
It read "under System 6 a cdev is a desk accessory's code in the system
heap, so what a rebuild spends is spent at the 1 MB floor". Measured, it
is the opposite: a 49394-byte cdev against a free system heap of 15180
on a 1 MB Plus, so the code resource is in the Control Panel desk
accessory's own space and not the system heap. The budget another attempt is
measured against is the code resource's addressing margin — how far a
16-bit PC-relative displacement reaches — not the system heap.

*What it DOES spend is somebody else's stack.* `fdrop_track` runs inside a
trap patch, so its 912-byte frame plus `folder_of`'s 388 — about 1332
bytes a press — come off the Finder's stack mid-press, not the program's.
`FDROP_STK` in `fdrop.c` is the instrument and it has run: about
**31 KB** under the Finder at a press on System 4.1 and 6.0.8 alike, so
the sender costs four per cent of what is there.

**That is a reason to try again rather than a cost of trying — but it
is not this tier's answer.** 7.0.1 is exactly where a control panel was
measured with 3639 bytes of its own stack under it, and it is the tier there is
no sender for, so it is the one number this measurement does not have.
Another attempt takes its own run before anything is believed.
