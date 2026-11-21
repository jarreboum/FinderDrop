# Archaeology of the drag library

The run that measured a number, the approach that was tried and
abandoned, the failure that forced a line to exist — what used to sit
inside a function, pushed out so the function reads. Keyed by function.
Nothing here is load-bearing: if an entry contradicts the code, the code
is right and the entry is stale.

---

## fdrop.c

### `fdrop_track` is 39% of the file and splitting it up costs bytes

The question that leads here is "can the sender be made smaller", and
`fdrop_track` is what a symbol listing points at: 2282 bytes of a 5828-byte
object, more than three times the next function. It reads as one
enormous routine badly in need of breaking up.

**It is already six routines.** `-Os` inlines every static with one call
site, and `fdrop_track` has six of them. Rebuilt with
`-fno-inline-functions-called-once` it falls to **1684** and `file_at`
(312) and `folder_from_record` (310) reappear as symbols of their own —
so the folder walk and the heap sweep were being read as tracking-loop
bloat.

Measured on the object, `-Os -mpcrel`, text only:

| build | fdrop.o |
|---|---|
| as shipped | 5828 |
| `-fno-inline-functions-called-once` | 5968 (+140) |
| `-fno-inline` | 6496 (+668) |
| `-fconserve-stack` | 6066 (+238) |

So the obvious move is measurably the wrong one, and the compiler was
right. What is actually available inside the function is about 100
bytes: nine of the eleven `FDROP_STEP` writes, which cost 82 across the
whole file and are the only report of where a refused gesture stopped in
a shipping build, and one of the three loops over `nMark`, of which only
the offset pass can fold — the other two are the comment at
"InvertRect rather than a framed rectangle", where every mark must come
off before any goes back on.

**What the exercise did turn up is a stack number, not a size one.**
`fdrop_track` opens `linkw %fp,#-912` and `folder_of` adds 388, so a press
costs about 1332 bytes — of the FINDER's stack, since this runs inside a
trap patch. Two documents said otherwise and were corrected in the same
commit: `fdrop.h` put the drag path's share at "about 290
bytes" of the panel's stack, which is `mark[]` and `carry[]` added up on
the wrong stack, and `PARKED.md` said a rebuild of the System 7
sender spends system heap, which had already been measured not to be
so.

Un-inlining does not rescue the stack either, which is why it was not
done: `fdrop_track` falls to 660 but `folder_from_record` takes a 400-byte
frame of its own, so the peak only moves from about 1332 to about 1092 —
240 bytes for 140 bytes of code and a shape the compiler had got right.

The move that would help is putting `mark[]` and `carry[]` in the static
blob `fdrop.S` already owns, and it costs about 292 bytes of the host
control panel's addressing margin, which stood at 1036.
**That is why `FDROP_STK` exists and why nothing was moved**: buying
down an unmeasured risk with a quarter of a scarce measured resource is
the wrong way round, and a stack question gets answered by measuring a
machine rather than by reasoning.

And the machine answered: about 31 KB under the Finder at a press on System
4.1 and 6.0.8, so the 1332 is four per cent and the 912 was never the
number to be alarmed by. The alarm came from reading it against 3639,
which is a control panel's stack under its own window on 7.0.1 and not
this stack at all — **two figures in the same units, about different
things**, which is the shape of the mistake worth remembering here.
