# FinderDrop examples

Three small programs, one for each kind of program FinderDrop is for. Each
does nothing but be dropped on: drag files, folders or disks from the
Finder onto its window and it lists each one — what it is, its name, the
volume it is on and the directory it is in. Each is the smallest complete
use of the library in its form, and is meant to be read: the comment at
the top of each source file explains the form and the calls it makes.

| example | form | runs on | build |
|---|---|---|---|
| `da/` | desk accessory | System 1.1 to Mac OS 9 | `sh examples/da/build.sh` |
| `cdev/` | control panel | System 4.1 to Mac OS 9 | `sh examples/cdev/build.sh` |
| `app/` | application | System 4.2 to 6.0.8 under MultiFinder, System 7.5 and later | `sh examples/app/build.sh` |

On System 7.0 and 7.1 none of them receives anything unless Apple's
Macintosh Drag and Drop extension is installed: FinderDrop does not read
those Finders' own drag (`PARKED.md` is why).

The results go under `build/examples/`, each program as a MacBinary
file: `da/FinderDrop.bin`, the desk accessory in a Font/DA Mover
suitcase (made by `da/suitcase.py` out of `da/FinderDrop.drvr`, the bare
`DRVR` resource); `cdev/FinderDrop.bin`, the control panel;
`app/FinderDropApp.bin`, the application.

## Installing them

- **The desk accessory** comes in a suitcase: open it with Font/DA
  Mover (double-clicking it does), copy `FinderDrop` into the System
  file, and choose it from the Apple menu. Installing it on System 7
  and later, where an accessory is a file of its own, has not been
  tried from this suitcase.
- **The control panel** goes in the System Folder (on System 7 and later,
  the Control Panels folder) and appears in the Control Panel.
- **The application** goes anywhere. On System 6, run it under
  MultiFinder: without it, launching an application quits the Finder and
  there is nothing to drag from — the window says so. It has an Apple
  menu, from which a desk accessory opens (under MultiFinder, into the
  application's own layer with Option held), and Quit in a File menu.

## Building for some machines and not others

Every `build.sh` passes its arguments to both halves of the library and
to the example, so the switches in `README.md` (Build switches)
work on any of them:

| for | build with |
|---|---|
| a Macintosh 128K or 512K on Systems 1.1 and 2.0: the smallest sender | `sh examples/da/build.sh -DFDROP_FINDER5=0 -DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0 -DFDROP_ROMPLUS=0` |
| the same machines up to System 3.2 | `sh examples/da/build.sh -DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0 -DFDROP_ROMPLUS=0` |
| Systems 1.1 to 3.2 on any Macintosh | `sh examples/da/build.sh -DFDROP_OWN_ROUTE=0 -DFDROP_RECEIVER=0` |
| a Macintosh 512K on Systems 3.3 and 4.1, which take a drop only from an HFS volume: on that machine, with Apple's `Hard Disk 20` file | `sh examples/da/build.sh -DFDROP_ROM_ROUTE=0 -DFDROP_FINDER5=0 -DFDROP_RECEIVER=0 -DFDROP_ROMPLUS=0` |
| a control panel, which cannot exist before System 4.0 | `sh examples/cdev/build.sh -DFDROP_ROM_ROUTE=0` (and `-DFDROP_ROM64K=0` if it is never to run on a 64K ROM) |
| the Drag Manager alone, patching nothing | `sh examples/app/build.sh -DFDROP_ROM_ROUTE=0 -DFDROP_OWN_ROUTE=0` |

An example built without the door a machine needs opens and receives
nothing. The accessory leaves out what its own build cannot need as the
library does: without a door that can hand it a folder it makes no
catalog call to ask what an item is.

The application is built one section per function so that the linker
can drop the name Retro68's compiler writes after every function for
MacsBug, about 2.4 KB of it; `MACSBUG=1 sh examples/app/build.sh`
keeps them, for stepping through it. The accessory and the control panel
drop them in their linker scripts.

`sh examples/check_switches.sh` compiles the library with each
switch off and fails if a function or a state word of that switch is
still in it.

## What a drop is

Every item arrives as three things: a volume reference number, a
directory ID and a name — an FSSpec in all but type, the same whether it
came through the Drag Manager or through FinderDrop's own patches.

| the item | volume | directory | name |
|---|---|---|---|
| a file or a folder on an HFS volume | its volume | the directory it is in | its name |
| a file on an MFS volume (a 400K floppy) | its volume | 0 | its name |
| a disk | its volume | 1, the parent of every root | the volume's name |
| a folder on an MFS volume | — | — | arrives as the files in it, each as above |
| the Trash | the startup disk | 2 | "Trash", a folder — where the system keeps one (System 7) |
| the Trash, holding files of an MFS volume | — | — | those files, each as above, beside the Trash folder where there is one |

What an item is, the program asks the File Manager; each example's
`what_is()` shows how. An item dragged off an EJECTED disk arrives the
same way, unchecked, and that call is what puts up the system's
"Please insert the disk"; a name no longer on the disk when it is back
shows as "gone". A Trash with nothing behind it (System 6 keeps no
Trash folder) is left out of the drag, and the rest still arrives. A
drag of more items than the queue holds is refused whole, and the
Finder puts the icons back; the desk accessory counts those refusals
on its first line.

## Debugging with them

Build any of them with `-DFDROP_DIAG=1` and the window also shows the
library's diagnostic words, in hex: `fdrop.c` says what each one holds.
The row of black squares along the bottom — one per item received, in
every build — is for a test harness, which can count them off a
screenshot without reading text.

Each example hands FinderDrop a queue of its own: the desk accessory
and the control panel eight entries inside their state, the
application one sized from its heap and grown by `room()` when the Drag
Manager brings a bigger drag.
`runtime.c` is shared by the desk accessory and the control panel, which
are code resources and link no C library: it is what the compiler expects
to find for memory copies and 32-bit arithmetic on a 68000.

## Where they have run

On emulated Macintoshes only, as they stand here (`NOTES.md`, the
entry on the examples):

| example | System | what it received |
|---|---|---|
| desk accessory | 1.1, Macintosh 128K (64K ROM) | a file from the disk's window; the disk |
| desk accessory | 3.0, 512Ke (Plus ROM) | a file from the desktop and the disk, in one drag |
| desk accessory | 6.0.8, Plus at 1 MB | the disk, a folder and a file, in one drag |
| control panel | 4.1 at 512 KB, and 6.0.8 | an image file, through FinderDrop's patches |
| control panel | 7.5.3 | the same file, through the Drag Manager |
| application | 6.0.8 under MultiFinder | a file from the desktop, taken in the background |
| application | 7.5.3 | a file; the disk; the System Folder, through the Drag Manager |

The library itself, in the hosts these examples grew out of, has run on
every System from 1.1 to 3.4 but 3.3 (`NOTES.md`).
