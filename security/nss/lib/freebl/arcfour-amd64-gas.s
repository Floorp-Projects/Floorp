# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is "Marc Bevand's fast AMD64 ARCFOUR source"
#
# The Initial Developer of the Original Code is
# Marc Bevand <bevand_m@epita.fr> .
# Portions created by the Initial Developer are 
# Copyright (C) 2004 the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# ** ARCFOUR implementation optimized for AMD64.
# **
# ** The throughput achieved by this code is about 320 MBytes/sec, on
# ** a 1.8 GHz AMD Opteron (rev C0) processor.

.text
.align 16
.globl ARCFOUR
.type ARCFOUR,@function
ARCFOUR:
	pushq	%rbp
	pushq	%rbx
	movq	%rdi,		%rbp	# key = ARG(key)
	movq	%rsi,		%rbx	# rbx = ARG(len)
	movq	%rdx,		%rsi	# in = ARG(in)
	movq	%rcx,		%rdi	# out = ARG(out)
	movq	(%rbp),		%rcx	# x = key->x
	movq	8(%rbp),	%rdx	# y = key->y
	addq	$16,		%rbp	# d = key->data
	incq	%rcx			# x++
	andq	$255,		%rcx	# x &= 0xff
	leaq	-8(%rbx,%rsi),	%rbx	# rbx = in+len-8
	movq	%rbx,		%r9	# tmp = in+len-8
	movq	0(%rbp,%rcx,8),	%rax	# tx = d[x]
	cmpq	%rsi,		%rbx	# cmp in with in+len-8
	jl	.Lend			# jump if (in+len-8 < in)

.Lstart:
	addq	$8,		%rsi		# increment in
	addq	$8,		%rdi		# increment out

	# generate the next 8 bytes of the rc4 stream into %r8
	movq	$8,		%r11		# byte counter
1:	addb	%al,		%dl		# y += tx
	movl	0(%rbp,%rdx,8),	%ebx		# ty = d[y]
	movl	%ebx,		0(%rbp,%rcx,8)	# d[x] = ty
	addb	%al,		%bl		# val = ty + tx
	movl	%eax,		0(%rbp,%rdx,8)	# d[y] = tx
	incb	%cl				# x++		(NEXT ROUND)
	movl	0(%rbp,%rcx,8),	%eax		# tx = d[x]	(NEXT ROUND)
	movb	0(%rbp,%rbx,8),	%r8b		# val = d[val]
	decb	%r11b
	rorq	$8,		%r8		# (ror does not change ZF)
	jnz 	1b

	# xor 8 bytes
	xorq	-8(%rsi),	%r8
	cmpq	%r9,		%rsi		# cmp in+len-8 with in
	movq	%r8,		-8(%rdi)
	jle	.Lstart				# jump if (in <= in+len-8)

.Lend:
	addq	$8,		%r9		# tmp = in+len

	# handle the last bytes, one by one
1:	cmpq	%rsi,		%r9		# cmp in with in+len
	jle	.Lfinished			# jump if (in+len <= in)
	addb	%al,		%dl		# y += tx
	movl	0(%rbp,%rdx,8),	%ebx		# ty = d[y]
	movl	%ebx,		0(%rbp,%rcx,8)	# d[x] = ty
	addb	%al,		%bl		# val = ty + tx
	movl	%eax,		0(%rbp,%rdx,8)	# d[y] = tx
	incb	%cl				# x++		(NEXT ROUND)
	movl	0(%rbp,%rcx,8),	%eax		# tx = d[x]	(NEXT ROUND)
	movb	0(%rbp,%rbx,8),	%r8b		# val = d[val]
	xorb	(%rsi),		%r8b		# xor 1 byte
	movb	%r8b,		(%rdi)
	incq	%rsi				# in++
	incq	%rdi				# out++
	jmp 1b

.Lfinished:
	decq	%rcx				# x--
	movb	%dl,		-8(%rbp)	# key->y = y
	movb	%cl,		-16(%rbp)	# key->x = x
	popq	%rbx
	popq	%rbp
	ret
.L_ARCFOUR_end:
.size ARCFOUR,.L_ARCFOUR_end-ARCFOUR

# Magic indicating no need for an executable stack
.section .note.GNU-stack,"",@progbits
.previous
