/*
 * cdev_entry.S — the first bytes of the FinderDrop example control panel.
 *
 * SPDX-License-Identifier: 0BSD
 * Copyright (C) 2026 by the FinderDrop authors (LICENSE).
 *
 * The Control Panel calls a cdev at byte zero of its resource, so the
 * branch to the C goes in a section cdev.ld places first by name, rather
 * than relying on link order.  bra, not jmp: a code resource is loaded
 * wherever the heap had room and never relocated.  The C function is
 * declared pascal, and Retro68 gives a pascal function's symbol in upper
 * case.  The arguments are already on the stack in Pascal order.
 */
	.section .text.cdeventry,"ax",@progbits
	.globl	CdevStart

CdevStart:
	bra.w	FINDERDROP_CDEV
