/*
 * da_header.S — the DRVR header of the FinderDrop example desk accessory,
 * and its entry stubs.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * A desk accessory is a driver: this header is what the Device Manager
 * and the Desk Manager read.  Its flags say the DA wants to stay locked
 * while open (its code holds FinderDrop's trap patches), periodic time
 * (accRun, which is when FinderDrop runs), Control calls for its
 * window's events, and the goodbye kiss: the Control call that comes
 * instead of Close when the heap it lives in goes from under it.  Its
 * name, which the Apple menu shows, begins with a NUL byte, as every
 * desk accessory's does.
 *
 * Each stub hands the parameter block and the DCE to the C function.
 * Control and Prime finish through jIODone unless the call was immediate.
 * bsr and bra, never jsr and jmp: the resource is loaded wherever the
 * heap had room and is never relocated, so nothing may be absolute.
 */
	.section .text.drvrheader,"ax",@progbits
	.globl	DAStart

DAStart:
	.word	0x7400			/* drvrFlags: dNeedLock, dNeedTime, dNeedGoodbye,
					   dCtlEnable                       */
	.word	6			/* drvrDelay: accRun every 6 ticks  */
	.word	0x0142			/* drvrEMask: mouseDown, update,
					   activate                         */
	.word	0			/* drvrMenu: none                   */

	.word	DAOpen-DAStart
	.word	DANothing-DAStart	/* Prime  */
	.word	DACtl-DAStart		/* Control */
	.word	DANothing-DAStart	/* Status */
	.word	DAClose-DAStart

	.byte	11			/* the name, as the Apple menu shows it */
	.byte	0
	.ascii	"FinderDrop"
	.even

JIODONE = 0x08FC

	/* Open and Close: call, return. */
	.macro	entry	name, target
\name:
	move.l	%a1,-(%sp)		/* the DCE           */
	move.l	%a0,-(%sp)		/* the param block   */
	bsr.w	\target
	addq.l	#8,%sp
	rts
	.endm

	/* Prime, Control, Status: call, then finish through jIODone unless
	   the caller asked for an immediate call (noQueueBit in ioTrap). */
	.macro	qentry	name, target
\name:
	move.l	%a1,-(%sp)
	move.l	%a0,-(%sp)
	move.l	%a1,-(%sp)
	move.l	%a0,-(%sp)
	bsr.w	\target
	addq.l	#8,%sp
	move.l	(%sp)+,%a0
	move.l	(%sp)+,%a1
	move.w	6(%a0),%d1		/* ioTrap     */
	btst	#9,%d1			/* noQueueBit */
	bne.s	9f
	move.l	JIODONE,-(%sp)
	rts
9:	rts
	.endm

	entry	DAOpen,    da_open
	qentry	DANothing, da_nothing
	qentry	DACtl,     da_control
	entry	DAClose,   da_close
