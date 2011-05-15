/ ***** BEGIN LICENSE BLOCK *****
/ Version: MPL 1.1/GPL 2.0/LGPL 2.1
/ 
/ The contents of this file are subject to the Mozilla Public License Version
/ 1.1 (the "License"); you may not use this file except in compliance with
/ the License. You may obtain a copy of the License at
/ http://www.mozilla.org/MPL/
/ 
/ Software distributed under the License is distributed on an "AS IS" basis,
/ WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
/ for the specific language governing rights and limitations under the
/ License.
/ 
/ The Original Code is the Solaris software cryptographic token.
/ 
/ The Initial Developer of the Original Code is
/ Sun Microsystems, Inc.
/ Portions created by the Initial Developer are Copyright (C) 2005
/ the Initial Developer. All Rights Reserved.
/ 
/ Contributor(s):
/   Sun Microsystems, Inc.
/ 
/ Alternatively, the contents of this file may be used under the terms of
/ either the GNU General Public License Version 2 or later (the "GPL"), or
/ the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
/ in which case the provisions of the GPL or the LGPL are applicable instead
/ of those above. If you wish to allow use of your version of this file only
/ under the terms of either the GPL or the LGPL, and not to allow others to
/ use your version of this file under the terms of the MPL, indicate your
/ decision by deleting the provisions above and replace them with the notice
/ and other provisions required by the GPL or the LGPL. If you do not delete
/ the provisions above, a recipient may use your version of this file under
/ the terms of any one of the MPL, the GPL or the LGPL.
/ 
/ ***** END LICENSE BLOCK ***** */


/ ------------------------------------------------------------------------
/
/  Implementation of s_mpv_mul_set_vec which exploits
/  the 64X64->128 bit  unsigned multiply instruction.
/
/ ------------------------------------------------------------------------

/ r = a * digit, r and a are vectors of length len
/ returns the carry digit
/ r and a are 64 bit aligned.
/
/ uint64_t
/ s_mpv_mul_set_vec64(uint64_t *r, uint64_t *a, int len, uint64_t digit)
/

.text; .align 16; .globl s_mpv_mul_set_vec64; .type s_mpv_mul_set_vec64, @function; s_mpv_mul_set_vec64:

	xorq	%rax, %rax		/ if (len == 0) return (0)
	testq	%rdx, %rdx
	jz	.L17

	movq	%rdx, %r8		/ Use r8 for len; %rdx is used by mul
	xorq	%r9, %r9		/ cy = 0

.L15:
	cmpq	$8, %r8			/ 8 - len
	jb	.L16
	movq	0(%rsi), %rax		/ rax = a[0]
	movq	8(%rsi), %r11		/ prefetch a[1]
	mulq	%rcx			/ p = a[0] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 0(%rdi)		/ r[0] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	16(%rsi), %r11		/ prefetch a[2]
	mulq	%rcx			/ p = a[1] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 8(%rdi)		/ r[1] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	24(%rsi), %r11		/ prefetch a[3]
	mulq	%rcx			/ p = a[2] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 16(%rdi)		/ r[2] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	32(%rsi), %r11		/ prefetch a[4]
	mulq	%rcx			/ p = a[3] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 24(%rdi)		/ r[3] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	40(%rsi), %r11		/ prefetch a[5]
	mulq	%rcx			/ p = a[4] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 32(%rdi)		/ r[4] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	48(%rsi), %r11		/ prefetch a[6]
	mulq	%rcx			/ p = a[5] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 40(%rdi)		/ r[5] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	56(%rsi), %r11		/ prefetch a[7]
	mulq	%rcx			/ p = a[6] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 48(%rdi)		/ r[6] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	mulq	%rcx			/ p = a[7] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 56(%rdi)		/ r[7] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	addq	$64, %rsi
	addq	$64, %rdi
	subq	$8, %r8

	jz	.L17
	jmp	.L15

.L16:
	movq	0(%rsi), %rax
	mulq	%rcx			/ p = a[0] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 0(%rdi)		/ r[0] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L17

	movq	8(%rsi), %rax
	mulq	%rcx			/ p = a[1] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 8(%rdi)		/ r[1] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L17

	movq	16(%rsi), %rax
	mulq	%rcx			/ p = a[2] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 16(%rdi)		/ r[2] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L17

	movq	24(%rsi), %rax
	mulq	%rcx			/ p = a[3] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 24(%rdi)		/ r[3] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L17

	movq	32(%rsi), %rax
	mulq	%rcx			/ p = a[4] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 32(%rdi)		/ r[4] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L17

	movq	40(%rsi), %rax
	mulq	%rcx			/ p = a[5] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 40(%rdi)		/ r[5] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L17

	movq	48(%rsi), %rax
	mulq	%rcx			/ p = a[6] * digit
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 48(%rdi)		/ r[6] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L17


.L17:
	movq	%r9, %rax
	ret

.size s_mpv_mul_set_vec64, .-s_mpv_mul_set_vec64

/ ------------------------------------------------------------------------
/
/  Implementation of s_mpv_mul_add_vec which exploits
/  the 64X64->128 bit  unsigned multiply instruction.
/
/ ------------------------------------------------------------------------

/ r += a * digit, r and a are vectors of length len
/ returns the carry digit
/ r and a are 64 bit aligned.
/
/ uint64_t
/ s_mpv_mul_add_vec64(uint64_t *r, uint64_t *a, int len, uint64_t digit)
/

.text; .align 16; .globl s_mpv_mul_add_vec64; .type s_mpv_mul_add_vec64, @function; s_mpv_mul_add_vec64:

	xorq	%rax, %rax		/ if (len == 0) return (0)
	testq	%rdx, %rdx
	jz	.L27

	movq	%rdx, %r8		/ Use r8 for len; %rdx is used by mul
	xorq	%r9, %r9		/ cy = 0

.L25:
	cmpq	$8, %r8			/ 8 - len
	jb	.L26
	movq	0(%rsi), %rax		/ rax = a[0]
	movq	0(%rdi), %r10		/ r10 = r[0]
	movq	8(%rsi), %r11		/ prefetch a[1]
	mulq	%rcx			/ p = a[0] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[0]
	movq	8(%rdi), %r10		/ prefetch r[1]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 0(%rdi)		/ r[0] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	16(%rsi), %r11		/ prefetch a[2]
	mulq	%rcx			/ p = a[1] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[1]
	movq	16(%rdi), %r10		/ prefetch r[2]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 8(%rdi)		/ r[1] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	24(%rsi), %r11		/ prefetch a[3]
	mulq	%rcx			/ p = a[2] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[2]
	movq	24(%rdi), %r10		/ prefetch r[3]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 16(%rdi)		/ r[2] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	32(%rsi), %r11		/ prefetch a[4]
	mulq	%rcx			/ p = a[3] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[3]
	movq	32(%rdi), %r10		/ prefetch r[4]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 24(%rdi)		/ r[3] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	40(%rsi), %r11		/ prefetch a[5]
	mulq	%rcx			/ p = a[4] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[4]
	movq	40(%rdi), %r10		/ prefetch r[5]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 32(%rdi)		/ r[4] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	48(%rsi), %r11		/ prefetch a[6]
	mulq	%rcx			/ p = a[5] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[5]
	movq	48(%rdi), %r10		/ prefetch r[6]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 40(%rdi)		/ r[5] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	movq	56(%rsi), %r11		/ prefetch a[7]
	mulq	%rcx			/ p = a[6] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[6]
	movq	56(%rdi), %r10		/ prefetch r[7]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 48(%rdi)		/ r[6] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	movq	%r11, %rax
	mulq	%rcx			/ p = a[7] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[7]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 56(%rdi)		/ r[7] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)

	addq	$64, %rsi
	addq	$64, %rdi
	subq	$8, %r8

	jz	.L27
	jmp	.L25

.L26:
	movq	0(%rsi), %rax
	movq	0(%rdi), %r10
	mulq	%rcx			/ p = a[0] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[0]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 0(%rdi)		/ r[0] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L27

	movq	8(%rsi), %rax
	movq	8(%rdi), %r10
	mulq	%rcx			/ p = a[1] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[1]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 8(%rdi)		/ r[1] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L27

	movq	16(%rsi), %rax
	movq	16(%rdi), %r10
	mulq	%rcx			/ p = a[2] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[2]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 16(%rdi)		/ r[2] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L27

	movq	24(%rsi), %rax
	movq	24(%rdi), %r10
	mulq	%rcx			/ p = a[3] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[3]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 24(%rdi)		/ r[3] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L27

	movq	32(%rsi), %rax
	movq	32(%rdi), %r10
	mulq	%rcx			/ p = a[4] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[4]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 32(%rdi)		/ r[4] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L27

	movq	40(%rsi), %rax
	movq	40(%rdi), %r10
	mulq	%rcx			/ p = a[5] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[5]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 40(%rdi)		/ r[5] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L27

	movq	48(%rsi), %rax
	movq	48(%rdi), %r10
	mulq	%rcx			/ p = a[6] * digit
	addq	%r10, %rax
	adcq	$0, %rdx		/ p += r[6]
	addq	%r9, %rax
	adcq	$0, %rdx		/ p += cy
	movq	%rax, 48(%rdi)		/ r[6] = lo(p)
	movq	%rdx, %r9		/ cy = hi(p)
	decq	%r8
	jz	.L27


.L27:
	movq	%r9, %rax
	ret
        
.size s_mpv_mul_add_vec64, .-s_mpv_mul_add_vec64
