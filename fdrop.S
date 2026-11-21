/*
 * fdrop.S — the sender's two trap patches, and every byte of the state
 * the C half uses, addressed PC-relative.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * ---- why this is assembly ----
 *
 * A trap patch is entered with the arguments on the stack and must leave
 * every register the real trap will want exactly as it found it.  No C
 * calling convention describes that.
 *
 * ---- why the state lives here ----
 *
 * A code resource (a desk accessory, a control panel) has no A5 world
 * and so no globals, and these patches are called from whatever context
 * the Toolbox was called in, where A5 could hold anything.  So the state
 * is part of the code, addressed PC-relative, and travels with it.
 *
 * The trap table points at a trampoline in the system heap, and the
 * trampoline jumps in here.  So the host keeps this code resource loaded
 * and locked from fdrop_install to fdrop_remove; after that the
 * trampoline is a pass-through and leads nowhere near this code (fdrop.c,
 * patch_remove).
 */
/* fdrop.h's numbers -- the build switches and the diagnostic words'
   count -- so that one number serves both halves.  Its C declarations
   are guarded out of this file by __ASSEMBLER__. */
#include "fdrop.h"

	.section .text.patch,"ax",@progbits

	.globl	FDROP_ENABLE
	.globl	FDROP_FW
	.globl	FDROP_FW_OLD
	.globl	FDROP_GM
	.globl	FDROP_GM_OLD
	.globl	FDROP_TOOLTAB
	.globl	FDROP_TRAMP
	.globl	FDROP_ROM64_FWW
	.globl	FDROP_ROM64_GMW
	.globl	FDROP_ROMDRAG
	.globl	FDROP_ROMWRAP
	.globl	FDROP_ROMMODE
	.globl	FDROP_BUSY
	.globl	FDROP_DRAGFP
	.globl	FDROP_DRAGRET
	.globl	FDROP_START
	.globl	FDROP_GEST
	.globl	FDROP_FWRET
	.globl	FDROP_OUTSITE
	.globl	FDROP_LASTGM
	.globl	FDROP_FAKED
	.globl	FDROP_ACT
	.globl	FDROP_OVER
	.globl	FDROP_FIN
	.globl	FDROP_STATE
	.globl	FDROP_PEND
	.globl	FDROP_PENDAT
	.globl	FDROP_QUEUE
	.globl	FDROP_QCAP
	.globl	FDROP_QN
	.globl	FDROP_ROOM
	.globl	FDROP_REFUSED
	.globl	FDROP_OVERFLOW
	.globl	FDROP_WIN
	.globl	FDROP_LASTN
	.globl	FDROP_STEP
	.globl	FDROP_SEQ

| The two patches are assembled only when one of the sender's two routes
| is built (fdrop.h works FDROP_SENDER out from FDROP_ROM_ROUTE and
| FDROP_OWN_ROUTE).  THE STATE BLOCK is guarded word by word: each word is
| assembled only in a build that has the switch it belongs to, and the
| queue's own words in every build.

#if FDROP_SENDER
/*
 * The two patches, _FindWindow and _GetMouse.  Between them they make a
 * Finder drag that ends on the host window end where it began.
 *
 * Neither takes the mouse.  A drag of a Finder icon stays the Finder's
 * from the press to the release; these only change what it is TOLD
 * while the pointer is over the host window.  fdrop.c ("how the Finder
 * drags an icon") explains the three _GetMouse calls of a drag, and
 * fdrop_answer tells them apart.
 *
 * _FindWindow RECOGNISES the drag: the action procedure's call site is
 * the same twelve bytes in every Finder it is for, and its caller's A6 is
 * the drag routine's frame, which holds the start point.  fdrop_look in
 * fdrop.c makes that check and records the frame.
 *
 * _GetMouse ANSWERS, and only for a caller whose A6 is that frame.  The
 * frame is compared here, before anything else, because every event loop
 * on the machine calls _GetMouse, and this comparison is all most of
 * those calls cost.  fdrop_answer decides the rest, from the caller's
 * return address.
 *
 * Both are HEAD patches.  _FindWindow's point is an argument and is read
 * on the way in; _GetMouse is either answered whole, the real trap never
 * reached, or passed on untouched.  Neither needs to see a result, so
 * neither stashes a return address, which keeps them reentrant.
 *
 * Pascal stack on entry to _FindWindow, from the top:
 *   0(sp)  return address
 *   4(sp)  WindowPtr *theWindow
 *   8(sp)  Point thePoint      (v in the high word, h in the low)
 *  12(sp)  room for the short result
 *
 * and to _GetMouse:
 *   0(sp)  return address
 *   4(sp)  Point *mouseLoc
 *
 * D0-D2 and A0-A1 are scratch across a Toolbox trap, but the C called
 * below treats D2 as callee-saved while the Toolbox does not: so all five
 * are saved.  A6 is never touched: it is the caller's, and it is what is
 * being looked at.
 *
 * FDROP_BUSY keeps the C half from being entered again while it is
 * already running.
 */
#if FDROP_PATCH_FW    /* not patched at all in a build for the 64K ROM's route alone */
FDROP_FW:
#if FDROP_OWN_ROUTE   /* else: nothing is recognised here, and it passes on */
	movem.l	%d0-%d2/%a0-%a1,-(%sp)   | five longs: everything below +20
#if FDROP_DIAG
	lea	FDROP_DG+102(%pc),%a1      | word 51, kDgFwEntries: every entry
	addq.w	#1,(%a1)
#endif
	move.w	FDROP_ENABLE(%pc),%d0
	beq	3f
	move.w	FDROP_BUSY(%pc),%d0
	bne	3f
	lea	FDROP_BUSY(%pc),%a1
	move.w	#1,(%a1)
	move.l	%a6,-(%sp)            | the caller's frame
	move.l	24(%sp),-(%sp)        | the return address: 0, +20, +4
	move.l	36(%sp),-(%sp)        | thePoint: 8, +20, +8
	jsr	fdrop_look(%pc)
	lea	12(%sp),%sp
	lea	FDROP_BUSY(%pc),%a1
	clr.w	(%a1)
3:
	movem.l	(%sp)+,%d0-%d2/%a0-%a1
#endif
	| The push and the rts cancel, so the arguments are exactly where
	| the real trap expects them.
	move.l	FDROP_FW_OLD(%pc),-(%sp)
	rts
#endif  /* FDROP_PATCH_FW */

FDROP_GM:
	movem.l	%d0-%d2/%a0-%a1,-(%sp)
#if FDROP_DIAG
	lea	FDROP_DG+104(%pc),%a1      | word 52, kDgGmEntries: every _GetMouse
	addq.w	#1,(%a1)
	move.l	20(%sp),-(%sp)        | and its call site (kDgSites)
	jsr	fdrop_dg_site(%pc)
	addq.l	#4,%sp
#endif
	cmpa.l	FDROP_DRAGFP(%pc),%a6   | the Finder's drag being followed
	beq	1f
#if FDROP_ROM_ROUTE
	/*
	 * A drag made by the ROM's own DragTheRgn (the Finders up to 5.3).
	 * Its two _GetMouse calls come from inside the ROM routine, and its
	 * action procedure's from a frame the Finder chose itself, so the
	 * frame cannot be the test: the caller is.  FDROP_ROMDRAG is the
	 * routine, when install found one of that exact shape, and 0
	 * otherwise.
	 */
	move.l	FDROP_ROMDRAG(%pc),%d0
	beq	3f                    | no such routine: the usual case
	move.l	20(%sp),%d1           | the return address
	sub.l	%d0,%d1
	cmpi.l	#0x100,%d1            | within kDragSpan of it
	bcs	1f                    | a _GetMouse from inside DragTheRgn
	move.w	FDROP_ROMMODE(%pc),%d0  | a ROM drag being followed: C decides
	beq	3f                    | whose _GetMouse this is; otherwise not ours
#else
	bra	3f                    | no ROM route built: not the frame, not ours
#endif
1:
	move.w	FDROP_ENABLE(%pc),%d0
	beq	3f
	move.w	FDROP_BUSY(%pc),%d0
	bne	3f
	lea	FDROP_BUSY(%pc),%a1
	move.w	#1,(%a1)
	move.l	%a6,-(%sp)
	move.l	24(%sp),-(%sp)        | the return address: 0, +20, +4
	move.l	32(%sp),-(%sp)        | mouseLoc: 4, +20, +8
	jsr	fdrop_answer(%pc)
	lea	12(%sp),%sp
	lea	FDROP_BUSY(%pc),%a1
	clr.w	(%a1)
	tst.l	%d0
	beq	3f
	| Answered: the point is written, and the real trap is not called.
	movem.l	(%sp)+,%d0-%d2/%a0-%a1
	move.l	(%sp)+,%a0            | the return address
	addq.l	#4,%sp                | and the one argument, Pascal-style
	jmp	(%a0)
3:
	movem.l	(%sp)+,%d0-%d2/%a0-%a1
	move.l	FDROP_GM_OLD(%pc),-(%sp)
	rts
#endif  /* FDROP_SENDER */

/*
 * ---- the state ----
 *
 * Each word is aligned, so the C half may read it as a short or a long.
 */
	.align	2
#if FDROP_PATCH_FW
FDROP_FW_OLD:
	.long	0                     | what _FindWindow's patch chains to
#endif
	.align	2
#if FDROP_SENDER
FDROP_GM_OLD:
	.long	0                     | what _GetMouse's patch chains to
#endif
	.align	2
#if FDROP_SENDER && FDROP_ROMPLUS
FDROP_TOOLTAB:
	.long	0                     | the tool trap table the two were written into
#endif
	.align	2
#if FDROP_SENDER
FDROP_TRAMP:
	.long	0                     | the trampoline block (fdrop.c, patch_install), or 0
#endif
	.align	2
#if FDROP_PATCH_FW && FDROP_ROM64K
FDROP_ROM64_FWW:
	.word	0                     | 64K ROM: _FindWindow's saved table word
#endif
	.align	2
#if FDROP_SENDER && FDROP_ROM64K
FDROP_ROM64_GMW:
	.word	0                     | 64K ROM: _GetMouse's saved table word
#endif
	.align	2
#if FDROP_ROM_ROUTE
FDROP_ROMDRAG:
	.long	0                     | the ROM's DragTheRgn, if install found it whole
#endif
	.align	2
#if FDROP_ROM_ROUTE && FDROP_ROMPLUS && FDROP_FINDER5
FDROP_ROMWRAP:
	.long	0                     | System 3's action wrapper in front of it, or 0
#endif
	.align	2
#if FDROP_ROM_ROUTE
FDROP_ROMMODE:
	.word	0                     | a drag through it is being followed
#endif

#if FDROP_SENDER
FDROP_BUSY:
	.word	0                     | inside fdrop_look or fdrop_answer already
#endif

/*
 * The Finder drag being followed: written by fdrop_look (or rom_arm) when
 * it recognises one, and read by both patches.
 *
 * The FRAME is what makes a _GetMouse call the drag's: the drag routine's
 * own two calls and its action procedure's (which does not LINK) all run
 * with A6 on it.  The frame's RETURN SLOT and the gesture's MBTicks make
 * sure it is still the same drag: a frame address is reused by the next
 * call made from the same depth, but its return slot is not.
 *
 * The CALL SITES that tell the drag's _GetMouse calls apart are not
 * written down anywhere: the action procedure's _FindWindow is recognised
 * by its bytes, the outline's _GetMouse is learned as the one made in the
 * frame just before each action procedure's, and the final one is any
 * other the drag routine makes once that is known.
 */
	.align	2
#if FDROP_SENDER
FDROP_DRAGFP:
	.long	0                     | A6 of the Finder's drag routine
#endif
	.align	2
#if FDROP_SENDER
FDROP_DRAGRET:
	.long	0                     | ...that frame's return address
#endif
	.align	2
#if FDROP_SENDER
FDROP_START:
	.long	0                     | ...its startPt, global, v then h
#endif
	.align	2
#if FDROP_SENDER
FDROP_GEST:
	.long	0                     | MBTicks of the press it was made of
#endif
	.align	2
#if FDROP_OWN_ROUTE
FDROP_FWRET:
	.long	0                     | where the action proc's _FindWindow returns
#endif
	.align	2
#if FDROP_OWN_ROUTE
FDROP_OUTSITE:
	.long	0                     | where the outline's _GetMouse returns, once learned
#endif
	.align	2
#if FDROP_OWN_ROUTE
FDROP_LASTGM:
	.long	0                     | the drag routine's last _GetMouse since the action proc's
#endif
	.align	2
#if FDROP_OWN_ROUTE
FDROP_FAKED:
	.word	0                     | the action procedure's last _GetMouse was answered
#endif
	.align	2
#if FDROP_SENDER
FDROP_ACT:
	.word	0                     | ...and its _FindWindow was asked about that point
#endif
	.align	2
#if FDROP_SENDER
FDROP_OVER:
	.word	0                     | the release was over the host window
#endif
	.align	2
#if FDROP_SENDER
FDROP_FIN:
	.word	0                     | the final _GetMouse: fdrop.c, kFinNotYet...
#endif
	.align	2
#if FDROP_SENDER
FDROP_STATE:
	.word	0                     | what the drag carries: fdrop.c, kDragUnread...
#endif
	.align	2
#if FDROP_SENDER
FDROP_PEND:
	.word	0                     | files written past the queue's depth
#endif
	.align	2
#if FDROP_SENDER
FDROP_PENDAT:
	.word	0                     | ...and the depth they were written at
#endif

/*
 * What the last drag carried and how far it got, for fdrop_diag.
 */
	.align	2
#if FDROP_SENDER
FDROP_LASTN:
	.word	0                     | files the last gesture carried
#endif
	.align	2
#if FDROP_SENDER
FDROP_STEP:
	.word	0                     | how far the last drag got: fdrop.c, kStep...
#endif
	.align	2
#if FDROP_SENDER
FDROP_SEQ:
	.word	0                     | Finder drags recognised
#endif


/*
 * The host window, written by fdrop_install and cleared by fdrop_remove.
 * A Finder drag has to be over it for the sender to answer, and every
 * queued drop names it.
 */
	.align	2
FDROP_WIN:
	.long	0                     | the window drops are being kept FOR

/*
 * The drop queue is the HOST'S: fdrop_install is handed where it is and
 * how many entries it holds.  What lives here is where to find it, so
 * that the sender can reach it from inside the Finder's drag,
 * PC-relative.  Entries are FDROP_ENTRY_BYTES each (fdrop.h); fdrop.c
 * appends at the depth, and the host takes from the front, one per
 * fdrop_take_drop.
 */
	.align	2
FDROP_QUEUE:
	.long	0                     | the host's queue, or 0
#if FDROP_RECEIVER
FDROP_ROOM:
	.long	0                     | the host's room hook, or 0
#endif
FDROP_QCAP:
	.word	0                     | how many entries it holds
FDROP_QN:
	.word	0                     | drops QUEUED, not drops seen
FDROP_REFUSED:
	.word	0                     | a drag refused for room, not yet asked about
#if FDROP_SENDER
FDROP_OVERFLOW:
	.word	0                     | the sender found more than fits, this drag
#endif


	.align	2
#if FDROP_SENDER
FDROP_ENABLE:
	.word	0                     | 0 until fdrop_install turns the patches on
#endif

/*
 * The diagnostic words, in an FDROP_DIAG build only: what a drag saw on
 * its way through, read by the host through fdrop_diag.  fdrop.c names
 * each one (kDg...).  FDROP_DIAG must be given to both halves.
 */
#if FDROP_DIAG
	.globl	FDROP_DG
	.align	2
FDROP_DG:
	.space	2 * FDROP_DG_WORDS
#endif
