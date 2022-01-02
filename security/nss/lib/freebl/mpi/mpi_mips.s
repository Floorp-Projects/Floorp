/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <regdef.h>
        .set    noreorder
        .set    noat

        .section        .text, 1, 0x00000006, 4, 4
.text:
        .section        .text

        .ent    s_mpv_mul_d_add
        .globl  s_mpv_mul_d_add

s_mpv_mul_d_add: 
 #/* c += a * b */
 #void s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, 
 #			      mp_digit *c)
 #{
 #  mp_digit   a0, a1;	regs a4, a5
 #  mp_digit   c0, c1;  regs a6, a7
 #  mp_digit   cy = 0;  reg t2
 #  mp_word    w0, w1;  regs t0, t1
 #
 #  if (a_len) {
	beq	a1,zero,.L.1
	move	t2,zero		# cy = 0
	dsll32	a2,a2,0		# "b" is sometimes negative (?!?!)
	dsrl32	a2,a2,0		# This clears the upper 32 bits.
 #    a0 = a[0];
	lwu	a4,0(a0)
 #    w0 = ((mp_word)b * a0);
	dmultu	a2,a4
 #    if (--a_len) {
	addiu	a1,a1,-1
	beq	a1,zero,.L.2
 #      while (a_len >= 2) {
	sltiu	t3,a1,2
	bne	t3,zero,.L.3
 #	  a1     = a[1];
	lwu	a5,4(a0)
.L.4:
 #	  a_len -= 2;
        addiu	a1,a1,-2
 #	  c0     = c[0];
	lwu	a6,0(a3)
 #	  w0    += cy;
	mflo	t0
	daddu	t0,t0,t2
 #	  w0    += c0;
	daddu	t0,t0,a6
 #	  w1     = (mp_word)b * a1; 
	dmultu	a2,a5			#
 #	  cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  a0     = a[2];
	lwu	a4,8(a0)
 #	  a     += 2;
	addiu	a0,a0,8
 #	  c1     = c[1];
	lwu	a7,4(a3)
 #	  w1    += cy;
	mflo	t1
	daddu	t1,t1,t2
 #	  w1    += c1;
	daddu	t1,t1,a7
 #	  w0     = (mp_word)b * a0;
	dmultu	a2,a4			#
 #	  cy     = CARRYOUT(w1);
	dsrl32	t2,t1,0
 #	  c[1]   = ACCUM(w1);
	sw	t1,4(a3)
 #	  c     += 2;
	addiu	a3,a3,8
	sltiu	t3,a1,2
	beq	t3,zero,.L.4
 #	  a1     = a[1];
	lwu	a5,4(a0)
 #      }
.L.3:
 #      c0       = c[0];
	lwu	a6,0(a3)
 #      w0      += cy;
 #      if (a_len) {
	mflo	t0
	beq	a1,zero,.L.5
	daddu	t0,t0,t2
 #	  w1     = (mp_word)b * a1; 
	dmultu	a2,a5
 #	  w0    += c0;
	daddu	t0,t0,a6		#
 #	  cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  c1     = c[1];
	lwu	a7,4(a3)
 #	  w1    += cy;
	mflo	t1
	daddu	t1,t1,t2
 #	  w1    += c1;
	daddu	t1,t1,a7
 #	  c[1]   = ACCUM(w1);
	sw	t1,4(a3)
 #	  cy     = CARRYOUT(w1);
	dsrl32	t2,t1,0
 #	  c     += 1;
	b	.L.6
	addiu	a3,a3,4
 #      } else {
.L.5:
 #	  w0    += c0;
	daddu	t0,t0,a6
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  cy     = CARRYOUT(w0);
	b	.L.6
	dsrl32	t2,t0,0
 #      }
 #    } else {
.L.2:
 #      c0     = c[0];
	lwu	a6,0(a3)
 #      w0    += c0;
	mflo	t0
	daddu	t0,t0,a6
 #      c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #      cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #    }
.L.6:
 #    c[1] = cy;
	jr	ra
	sw	t2,4(a3)
 #  }
.L.1:
	jr	ra
	nop
 #}
 #
        .end    s_mpv_mul_d_add

        .ent    s_mpv_mul_d_add_prop
        .globl  s_mpv_mul_d_add_prop

s_mpv_mul_d_add_prop: 
 #/* c += a * b */
 #void s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, 
 #			      mp_digit *c)
 #{
 #  mp_digit   a0, a1;	regs a4, a5
 #  mp_digit   c0, c1;  regs a6, a7
 #  mp_digit   cy = 0;  reg t2
 #  mp_word    w0, w1;  regs t0, t1
 #
 #  if (a_len) {
	beq	a1,zero,.M.1
	move	t2,zero		# cy = 0
	dsll32	a2,a2,0		# "b" is sometimes negative (?!?!)
	dsrl32	a2,a2,0		# This clears the upper 32 bits.
 #    a0 = a[0];
	lwu	a4,0(a0)
 #    w0 = ((mp_word)b * a0);
	dmultu	a2,a4
 #    if (--a_len) {
	addiu	a1,a1,-1
	beq	a1,zero,.M.2
 #      while (a_len >= 2) {
	sltiu	t3,a1,2
	bne	t3,zero,.M.3
 #	  a1     = a[1];
	lwu	a5,4(a0)
.M.4:
 #	  a_len -= 2;
        addiu	a1,a1,-2
 #	  c0     = c[0];
	lwu	a6,0(a3)
 #	  w0    += cy;
	mflo	t0
	daddu	t0,t0,t2
 #	  w0    += c0;
	daddu	t0,t0,a6
 #	  w1     = (mp_word)b * a1; 
	dmultu	a2,a5			#
 #	  cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  a0     = a[2];
	lwu	a4,8(a0)
 #	  a     += 2;
	addiu	a0,a0,8
 #	  c1     = c[1];
	lwu	a7,4(a3)
 #	  w1    += cy;
	mflo	t1
	daddu	t1,t1,t2
 #	  w1    += c1;
	daddu	t1,t1,a7
 #	  w0     = (mp_word)b * a0;
	dmultu	a2,a4			#
 #	  cy     = CARRYOUT(w1);
	dsrl32	t2,t1,0
 #	  c[1]   = ACCUM(w1);
	sw	t1,4(a3)
 #	  c     += 2;
	addiu	a3,a3,8
	sltiu	t3,a1,2
	beq	t3,zero,.M.4
 #	  a1     = a[1];
	lwu	a5,4(a0)
 #      }
.M.3:
 #      c0       = c[0];
	lwu	a6,0(a3)
 #      w0      += cy;
 #      if (a_len) {
	mflo	t0
	beq	a1,zero,.M.5
	daddu	t0,t0,t2
 #	  w1     = (mp_word)b * a1; 
	dmultu	a2,a5
 #	  w0    += c0;
	daddu	t0,t0,a6		#
 #	  cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  c1     = c[1];
	lwu	a7,4(a3)
 #	  w1    += cy;
	mflo	t1
	daddu	t1,t1,t2
 #	  w1    += c1;
	daddu	t1,t1,a7
 #	  c[1]   = ACCUM(w1);
	sw	t1,4(a3)
 #	  cy     = CARRYOUT(w1);
	dsrl32	t2,t1,0
 #	  c     += 1;
	b	.M.6
	addiu	a3,a3,8
 #      } else {
.M.5:
 #	  w0    += c0;
	daddu	t0,t0,a6
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
	b	.M.6
	addiu	a3,a3,4
 #      }
 #    } else {
.M.2:
 #      c0     = c[0];
	lwu	a6,0(a3)
 #      w0    += c0;
	mflo	t0
	daddu	t0,t0,a6
 #      c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #      cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
	addiu	a3,a3,4
 #    }
.M.6:

 #    while (cy) {
	beq	t2,zero,.M.1
	nop
.M.7:
 #      mp_word w = (mp_word)*c + cy;
	lwu	a6,0(a3)
	daddu	t2,t2,a6
 #      *c++ = ACCUM(w);
	sw	t2,0(a3)
 #      cy = CARRYOUT(w);
	dsrl32	t2,t2,0
	bne	t2,zero,.M.7
	addiu	a3,a3,4

 #  }
.M.1:
	jr	ra
	nop
 #}
 #
        .end    s_mpv_mul_d_add_prop

        .ent    s_mpv_mul_d
        .globl  s_mpv_mul_d

s_mpv_mul_d: 
 #/* c = a * b */
 #void s_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, 
 #			      mp_digit *c)
 #{
 #  mp_digit   a0, a1;	regs a4, a5
 #  mp_digit   cy = 0;  reg t2
 #  mp_word    w0, w1;  regs t0, t1
 #
 #  if (a_len) {
	beq	a1,zero,.N.1
	move	t2,zero		# cy = 0
	dsll32	a2,a2,0		# "b" is sometimes negative (?!?!)
	dsrl32	a2,a2,0		# This clears the upper 32 bits.
 #    a0 = a[0];
	lwu	a4,0(a0)
 #    w0 = ((mp_word)b * a0);
	dmultu	a2,a4
 #    if (--a_len) {
	addiu	a1,a1,-1
	beq	a1,zero,.N.2
 #      while (a_len >= 2) {
	sltiu	t3,a1,2
	bne	t3,zero,.N.3
 #	  a1     = a[1];
	lwu	a5,4(a0)
.N.4:
 #	  a_len -= 2;
        addiu	a1,a1,-2
 #	  w0    += cy;
	mflo	t0
	daddu	t0,t0,t2
 #	  cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #	  w1     = (mp_word)b * a1; 
	dmultu	a2,a5	
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  a0     = a[2];
	lwu	a4,8(a0)
 #	  a     += 2;
	addiu	a0,a0,8
 #	  w1    += cy;
	mflo	t1
	daddu	t1,t1,t2
 #	  cy     = CARRYOUT(w1);
	dsrl32	t2,t1,0
 #	  w0     = (mp_word)b * a0;
	dmultu	a2,a4	
 #	  c[1]   = ACCUM(w1);
	sw	t1,4(a3)
 #	  c     += 2;
	addiu	a3,a3,8
	sltiu	t3,a1,2
	beq	t3,zero,.N.4
 #	  a1     = a[1];
	lwu	a5,4(a0)
 #      }
.N.3:
 #      w0      += cy;
 #      if (a_len) {
	mflo	t0
	beq	a1,zero,.N.5
	daddu	t0,t0,t2
 #	  w1     = (mp_word)b * a1; 
	dmultu	a2,a5			#
 #	  cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  w1    += cy;
	mflo	t1
	daddu	t1,t1,t2
 #	  c[1]   = ACCUM(w1);
	sw	t1,4(a3)
 #	  cy     = CARRYOUT(w1);
	dsrl32	t2,t1,0
 #	  c     += 1;
	b	.N.6
	addiu	a3,a3,4
 #      } else {
.N.5:
 #	  c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #	  cy     = CARRYOUT(w0);
	b	.N.6
	dsrl32	t2,t0,0
 #      }
 #    } else {
.N.2:
	mflo	t0
 #      c[0]   = ACCUM(w0);
	sw	t0,0(a3)
 #      cy     = CARRYOUT(w0);
	dsrl32	t2,t0,0
 #    }
.N.6:
 #    c[1] = cy;
	jr	ra
	sw	t2,4(a3)
 #  }
.N.1:
	jr	ra
	nop
 #}
 #
        .end    s_mpv_mul_d


        .ent    s_mpv_sqr_add_prop
        .globl  s_mpv_sqr_add_prop
 #void   s_mpv_sqr_add_prop(const mp_digit *a, mp_size a_len, mp_digit *sqrs);
 #	registers
 #	a0		*a
 #	a1		a_len
 #	a2		*sqr
 #	a3		digit from *a, a_i
 #	a4		square of digit from a
 #	a5,a6		next 2 digits in sqr
 #	a7,t0		carry 
s_mpv_sqr_add_prop:
	move	a7,zero
	move	t0,zero
	lwu	a3,0(a0)
	addiu	a1,a1,-1	# --a_len
	dmultu	a3,a3
	beq	a1,zero,.P.3	# jump if we've already done the only sqr
	addiu	a0,a0,4		# ++a
.P.2:
        lwu	a5,0(a2)
        lwu	a6,4(a2)
	addiu	a2,a2,8		# sqrs += 2;
	dsll32	a6,a6,0
	daddu	a5,a5,a6
	lwu	a3,0(a0)
	addiu	a0,a0,4		# ++a
	mflo	a4
	daddu	a6,a5,a4
	sltu	a7,a6,a5	# a7 = a6 < a5	detect overflow
	dmultu	a3,a3
	daddu	a4,a6,t0
	sltu	t0,a4,a6
	add	t0,t0,a7
	sw	a4,-8(a2)
	addiu	a1,a1,-1	# --a_len
	dsrl32	a4,a4,0
	bne	a1,zero,.P.2	# loop if a_len > 0
	sw	a4,-4(a2)
.P.3:
        lwu	a5,0(a2)
        lwu	a6,4(a2)
	addiu	a2,a2,8		# sqrs += 2;
	dsll32	a6,a6,0
	daddu	a5,a5,a6
	mflo	a4
	daddu	a6,a5,a4
	sltu	a7,a6,a5	# a7 = a6 < a5	detect overflow
	daddu	a4,a6,t0
	sltu	t0,a4,a6
	add	t0,t0,a7
	sw	a4,-8(a2)
	beq	t0,zero,.P.9	# jump if no carry
	dsrl32	a4,a4,0
.P.8:
	sw	a4,-4(a2)
	/* propagate final carry */
	lwu	a5,0(a2)
	daddu	a6,a5,t0
	sltu	t0,a6,a5
	bne	t0,zero,.P.8	# loop if carry persists
	addiu	a2,a2,4		# sqrs++
.P.9:
	jr	ra
	sw	a4,-4(a2)

        .end    s_mpv_sqr_add_prop
