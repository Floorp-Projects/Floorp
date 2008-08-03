! Inner multiply loop functions for pure 32-bit Sparc v8 CPUs.
! ***** BEGIN LICENSE BLOCK *****
! Version: MPL 1.1/GPL 2.0/LGPL 2.1
! 
! The contents of this file are subject to the Mozilla Public License Version 
! 1.1 (the "License"); you may not use this file except in compliance with 
! the License. You may obtain a copy of the License at 
! http://www.mozilla.org/MPL/
! 
! Software distributed under the License is distributed on an "AS IS" basis,
! WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
! for the specific language governing rights and limitations under the
! License.
!
! The Original Code is a SPARC v8 optimized multiply and add function
! 
! The Initial Developer of the Original Code is Sun Microsystems Inc.
! Portions created by Sun Microsystems Inc. are 
! Copyright (C) 2000-2005 Sun Microsystems Inc.  All Rights Reserved.
! 
! Contributor(s):
! 
! Alternatively, the contents of this file may be used under the terms of
! either the GNU General Public License Version 2 or later (the "GPL"), or
! the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
! in which case the provisions of the GPL or the LGPL are applicable instead
! of those above. If you wish to allow use of your version of this file only
! under the terms of either the GPL or the LGPL, and not to allow others to
! use your version of this file under the terms of the MPL, indicate your
! decision by deleting the provisions above and replace them with the notice
! and other provisions required by the GPL or the LGPL. If you do not delete
! the provisions above, a recipient may use your version of this file under
! the terms of any one of the MPL, the GPL or the LGPL.
! 
! ***** END LICENSE BLOCK *****

! $Id: mpv_sparcv8x.s,v 1.2 2005/08/06 11:08:41 nelsonb%netscape.com Exp $

	.file	"mpv_sparcv8x.s"
	.align	8

	.section     ".text",#alloc,#execinstr
	.global      s_mpv_mul_d
 s_mpv_mul_d:
	save         %sp, -0x60, %sp
	mov          %i0, %o0
	clr          %g4
	cmp          %i1, 0x0
	be           .L103
	sub          %i1, 0x1, %o5
	ld           [%o0], %g1
.L101:
	umul         %g1, %i2, %g2
	rd           %y, %g1
	add          %g2, %g4, %g3
	mov          %g1, %o4
	add          %o0, 0x4, %o0
	cmp          %g3, %g4
	blu,a        .L102
	add          %g1, 0x1, %o4
.L102:
	st           %g3, [%i3]
	mov          %o5, %g1
	add          %i3, 0x4, %i3
	cmp          %g1, 0x0
	mov          %o4, %g4
	sub          %o5, 0x1, %o5
	bne,a        .L101
	ld           [%o0], %g1
.L103:
	st           %g4, [%i3]
	ret
	restore

	.type	s_mpv_mul_d,2
	.size	s_mpv_mul_d,(.-s_mpv_mul_d)

	.align	16
	.global      s_mpv_mul_d_add
 s_mpv_mul_d_add:

	save         %sp, -0x60, %sp
	mov          %i0, %o0
	clr          %g4
	cmp          %i1, 0x0
	be           .L204
	sub          %i1, 0x1, %o5
	ld           [%o0], %g1
.L201:
	umul         %g1, %i2, %g2
	rd           %y, %g1
	add          %g2, %g4, %g3
	mov          %g1, %o4
	add          %o0, 0x4, %o0
	cmp          %g3, %g4
	blu,a        .L202
	add          %g1, 0x1, %o4
.L202:
	ld           [%i3], %g2
	add          %g3, %g2, %g1
	cmp          %g1, %g2
	blu,a        .L203
	add          %o4, 0x1, %o4
.L203:
	st           %g1, [%i3]
	mov          %o5, %g1
	add          %i3, 0x4, %i3
	cmp          %g1, 0x0
	mov          %o4, %g4
	sub          %o5, 0x1, %o5
	bne,a        .L201
	ld           [%o0], %g1
.L204:
	st           %g4, [%i3]
	ret
	restore

	.type	s_mpv_mul_d_add,2
	.size	s_mpv_mul_d_add,(.-s_mpv_mul_d_add)

	.align	16
	.global      s_mpv_mul_d_add_prop
 s_mpv_mul_d_add_prop:

	save         %sp, -0x60, %sp
	mov          %i0, %o0
	clr          %o5
	cmp          %i1, 0x0
	be           .L30x70
	sub          %i1, 0x1, %g4
	ld           [%o0], %g1
.L30x1c:
	umul         %g1, %i2, %g2
	rd           %y, %g1
	add          %g2, %o5, %g3
	mov          %g1, %o4
	add          %o0, 0x4, %o0
	cmp          %g3, %o5
	blu,a        .L30x3c
	add          %g1, 0x1, %o4
.L30x3c:
	ld           [%i3], %g2
	add          %g3, %g2, %g1
	cmp          %g1, %g2
	blu,a        .L30x50
	add          %o4, 0x1, %o4
.L30x50:
	st           %g1, [%i3]
	mov          %g4, %g1
	add          %i3, 0x4, %i3
	cmp          %g1, 0x0
	mov          %o4, %o5
	sub          %g4, 0x1, %g4
	bne,a        .L30x1c
	ld           [%o0], %g1
.L30x70:
	cmp          %o5, 0x0
	be           .L30xa0
	nop
	ld           [%i3], %g1
.L30x80:
	add          %o5, %g1, %g2
	st           %g2, [%i3]
	add          %i3, 0x4, %i3
	cmp          %g2, %g1
	addx         %g0, 0x0, %o5
	cmp          %o5, 0x0
	bne,a        .L30x80
	ld           [%i3], %g1
.L30xa0:
	ret
	restore

	.type	s_mpv_mul_d_add_prop,2
	.size	s_mpv_mul_d_add_prop,(.-s_mpv_mul_d_add_prop)
