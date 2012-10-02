#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#  $Id: mpi_x86.s,v 1.7 2012/04/25 14:49:50 gerv%gerv.net Exp $
#

.data
.align 4
 #
 # -1 means to call s_mpi_is_sse to determine if we support sse 
 #    instructions.
 #  0 means to use x86 instructions
 #  1 means to use sse2 instructions
.type	is_sse,@object
.size	is_sse,4
is_sse: .long	-1 

#
# sigh, handle the difference between -fPIC and not PIC
# default to pic, since this file seems to be exclusively
# linux right now (solaris uses mpi_i86pc.s and windows uses
# mpi_x86_asm.c)
#
.ifndef NO_PIC
.macro GET   var,reg
    movl   \var@GOTOFF(%ebx),\reg
.endm
.macro PUT   reg,var
    movl   \reg,\var@GOTOFF(%ebx)
.endm
.else
.macro GET   var,reg
    movl   \var,\reg
.endm
.macro PUT   reg,var
    movl   \reg,\var
.endm
.endif

.text


 #  ebp - 36:	caller's esi
 #  ebp - 32:	caller's edi
 #  ebp - 28:	
 #  ebp - 24:	
 #  ebp - 20:	
 #  ebp - 16:	
 #  ebp - 12:	
 #  ebp - 8:	
 #  ebp - 4:	
 #  ebp + 0:	caller's ebp
 #  ebp + 4:	return address
 #  ebp + 8:	a	argument
 #  ebp + 12:	a_len	argument
 #  ebp + 16:	b	argument
 #  ebp + 20:	c	argument
 #  registers:
 # 	eax:
 #	ebx:	carry
 #	ecx:	a_len
 #	edx:
 #	esi:	a ptr
 #	edi:	c ptr
.globl	s_mpv_mul_d
.type	s_mpv_mul_d,@function
s_mpv_mul_d:
    GET    is_sse,%eax
    cmp    $0,%eax
    je     s_mpv_mul_d_x86
    jg     s_mpv_mul_d_sse2
    call   s_mpi_is_sse2
    PUT    %eax,is_sse
    cmp    $0,%eax
    jg     s_mpv_mul_d_sse2
s_mpv_mul_d_x86:
    push   %ebp
    mov    %esp,%ebp
    sub    $28,%esp
    push   %edi
    push   %esi
    push   %ebx
    movl   $0,%ebx		# carry = 0
    mov    12(%ebp),%ecx	# ecx = a_len
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     2f			# jmp if a_len == 0
    mov    8(%ebp),%esi		# esi = a
    cld
1:
    lodsl			# eax = [ds:esi]; esi += 4
    mov    16(%ebp),%edx	# edx = b
    mull   %edx			# edx:eax = Phi:Plo = a_i * b

    add    %ebx,%eax		# add carry (%ebx) to edx:eax
    adc    $0,%edx
    mov    %edx,%ebx		# high half of product becomes next carry

    stosl			# [es:edi] = ax; edi += 4;
    dec    %ecx			# --a_len
    jnz    1b			# jmp if a_len != 0
2:
    mov    %ebx,0(%edi)		# *c = carry
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop
s_mpv_mul_d_sse2:
    push   %ebp
    mov    %esp,%ebp
    push   %edi
    push   %esi
    psubq  %mm2,%mm2		# carry = 0
    mov    12(%ebp),%ecx	# ecx = a_len
    movd   16(%ebp),%mm1	# mm1 = b
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     6f			# jmp if a_len == 0
    mov    8(%ebp),%esi		# esi = a
    cld
5:
    movd   0(%esi),%mm0         # mm0 = *a++
    add    $4,%esi
    pmuludq %mm1,%mm0           # mm0 = b * *a++
    paddq  %mm0,%mm2            # add the carry
    movd   %mm2,0(%edi)         # store the 32bit result
    add    $4,%edi
    psrlq  $32, %mm2		# save the carry
    dec    %ecx			# --a_len
    jnz    5b			# jmp if a_len != 0
6:
    movd   %mm2,0(%edi)		# *c = carry
    emms
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 #  ebp - 36:	caller's esi
 #  ebp - 32:	caller's edi
 #  ebp - 28:	
 #  ebp - 24:	
 #  ebp - 20:	
 #  ebp - 16:	
 #  ebp - 12:	
 #  ebp - 8:	
 #  ebp - 4:	
 #  ebp + 0:	caller's ebp
 #  ebp + 4:	return address
 #  ebp + 8:	a	argument
 #  ebp + 12:	a_len	argument
 #  ebp + 16:	b	argument
 #  ebp + 20:	c	argument
 #  registers:
 # 	eax:
 #	ebx:	carry
 #	ecx:	a_len
 #	edx:
 #	esi:	a ptr
 #	edi:	c ptr
.globl	s_mpv_mul_d_add
.type	s_mpv_mul_d_add,@function
s_mpv_mul_d_add:
    GET    is_sse,%eax
    cmp    $0,%eax
    je     s_mpv_mul_d_add_x86
    jg     s_mpv_mul_d_add_sse2
    call   s_mpi_is_sse2
    PUT    %eax,is_sse
    cmp    $0,%eax
    jg     s_mpv_mul_d_add_sse2
s_mpv_mul_d_add_x86:
    push   %ebp
    mov    %esp,%ebp
    sub    $28,%esp
    push   %edi
    push   %esi
    push   %ebx
    movl   $0,%ebx		# carry = 0
    mov    12(%ebp),%ecx	# ecx = a_len
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     11f			# jmp if a_len == 0
    mov    8(%ebp),%esi		# esi = a
    cld
10:
    lodsl			# eax = [ds:esi]; esi += 4
    mov    16(%ebp),%edx	# edx = b
    mull   %edx			# edx:eax = Phi:Plo = a_i * b

    add    %ebx,%eax		# add carry (%ebx) to edx:eax
    adc    $0,%edx
    mov    0(%edi),%ebx		# add in current word from *c
    add    %ebx,%eax		
    adc    $0,%edx
    mov    %edx,%ebx		# high half of product becomes next carry

    stosl			# [es:edi] = ax; edi += 4;
    dec    %ecx			# --a_len
    jnz    10b			# jmp if a_len != 0
11:
    mov    %ebx,0(%edi)		# *c = carry
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop
s_mpv_mul_d_add_sse2:
    push   %ebp
    mov    %esp,%ebp
    push   %edi
    push   %esi
    psubq  %mm2,%mm2		# carry = 0
    mov    12(%ebp),%ecx	# ecx = a_len
    movd   16(%ebp),%mm1	# mm1 = b
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     16f			# jmp if a_len == 0
    mov    8(%ebp),%esi		# esi = a
    cld
15:
    movd   0(%esi),%mm0         # mm0 = *a++
    add    $4,%esi
    pmuludq %mm1,%mm0           # mm0 = b * *a++
    paddq  %mm0,%mm2            # add the carry
    movd   0(%edi),%mm0
    paddq  %mm0,%mm2            # add the carry
    movd   %mm2,0(%edi)         # store the 32bit result
    add    $4,%edi
    psrlq  $32, %mm2		# save the carry
    dec    %ecx			# --a_len
    jnz    15b			# jmp if a_len != 0
16:
    movd   %mm2,0(%edi)		# *c = carry
    emms
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 #  ebp - 8:	caller's esi
 #  ebp - 4:	caller's edi
 #  ebp + 0:	caller's ebp
 #  ebp + 4:	return address
 #  ebp + 8:	a	argument
 #  ebp + 12:	a_len	argument
 #  ebp + 16:	b	argument
 #  ebp + 20:	c	argument
 #  registers:
 # 	eax:
 #	ebx:	carry
 #	ecx:	a_len
 #	edx:
 #	esi:	a ptr
 #	edi:	c ptr
.globl	s_mpv_mul_d_add_prop
.type	s_mpv_mul_d_add_prop,@function
s_mpv_mul_d_add_prop:
    GET    is_sse,%eax
    cmp    $0,%eax
    je     s_mpv_mul_d_add_prop_x86
    jg     s_mpv_mul_d_add_prop_sse2
    call   s_mpi_is_sse2
    PUT    %eax,is_sse
    cmp    $0,%eax
    jg     s_mpv_mul_d_add_prop_sse2
s_mpv_mul_d_add_prop_x86:
    push   %ebp
    mov    %esp,%ebp
    sub    $28,%esp
    push   %edi
    push   %esi
    push   %ebx
    movl   $0,%ebx		# carry = 0
    mov    12(%ebp),%ecx	# ecx = a_len
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     21f			# jmp if a_len == 0
    cld
    mov    8(%ebp),%esi		# esi = a
20:
    lodsl			# eax = [ds:esi]; esi += 4
    mov    16(%ebp),%edx	# edx = b
    mull   %edx			# edx:eax = Phi:Plo = a_i * b

    add    %ebx,%eax		# add carry (%ebx) to edx:eax
    adc    $0,%edx
    mov    0(%edi),%ebx		# add in current word from *c
    add    %ebx,%eax		
    adc    $0,%edx
    mov    %edx,%ebx		# high half of product becomes next carry

    stosl			# [es:edi] = ax; edi += 4;
    dec    %ecx			# --a_len
    jnz    20b			# jmp if a_len != 0
21:
    cmp    $0,%ebx		# is carry zero?
    jz     23f
    mov    0(%edi),%eax		# add in current word from *c
    add	   %ebx,%eax
    stosl			# [es:edi] = ax; edi += 4;
    jnc    23f
22:
    mov    0(%edi),%eax		# add in current word from *c
    adc	   $0,%eax
    stosl			# [es:edi] = ax; edi += 4;
    jc     22b
23:
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop
s_mpv_mul_d_add_prop_sse2:
    push   %ebp
    mov    %esp,%ebp
    push   %edi
    push   %esi
    push   %ebx
    psubq  %mm2,%mm2		# carry = 0
    mov    12(%ebp),%ecx	# ecx = a_len
    movd   16(%ebp),%mm1	# mm1 = b
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     26f			# jmp if a_len == 0
    mov    8(%ebp),%esi		# esi = a
    cld
25:
    movd   0(%esi),%mm0         # mm0 = *a++
    movd   0(%edi),%mm3		# fetch the sum
    add    $4,%esi
    pmuludq %mm1,%mm0           # mm0 = b * *a++
    paddq  %mm0,%mm2            # add the carry
    paddq  %mm3,%mm2            # add *c++
    movd   %mm2,0(%edi)         # store the 32bit result
    add    $4,%edi
    psrlq  $32, %mm2		# save the carry
    dec    %ecx			# --a_len
    jnz    25b			# jmp if a_len != 0
26:
    movd   %mm2,%ebx
    cmp    $0,%ebx		# is carry zero?
    jz     28f
    mov    0(%edi),%eax
    add    %ebx, %eax
    stosl
    jnc    28f
27:
    mov    0(%edi),%eax		# add in current word from *c
    adc	   $0,%eax
    stosl			# [es:edi] = ax; edi += 4;
    jc     27b
28:
    emms
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop


 #  ebp - 20:	caller's esi
 #  ebp - 16:	caller's edi
 #  ebp - 12:	
 #  ebp - 8:	carry
 #  ebp - 4:	a_len	local
 #  ebp + 0:	caller's ebp
 #  ebp + 4:	return address
 #  ebp + 8:	pa	argument
 #  ebp + 12:	a_len	argument
 #  ebp + 16:	ps	argument
 #  ebp + 20:	
 #  registers:
 # 	eax:
 #	ebx:	carry
 #	ecx:	a_len
 #	edx:
 #	esi:	a ptr
 #	edi:	c ptr

.globl	s_mpv_sqr_add_prop
.type	s_mpv_sqr_add_prop,@function
s_mpv_sqr_add_prop:
     GET   is_sse,%eax
     cmp    $0,%eax
     je     s_mpv_sqr_add_prop_x86
     jg     s_mpv_sqr_add_prop_sse2
     call   s_mpi_is_sse2
     PUT    %eax,is_sse
     cmp    $0,%eax
     jg     s_mpv_sqr_add_prop_sse2
s_mpv_sqr_add_prop_x86:
     push   %ebp
     mov    %esp,%ebp
     sub    $12,%esp
     push   %edi
     push   %esi
     push   %ebx
     movl   $0,%ebx		# carry = 0
     mov    12(%ebp),%ecx	# a_len
     mov    16(%ebp),%edi	# edi = ps
     cmp    $0,%ecx
     je     31f			# jump if a_len == 0
     cld
     mov    8(%ebp),%esi	# esi = pa
30:
     lodsl			# %eax = [ds:si]; si += 4;
     mull   %eax

     add    %ebx,%eax		# add "carry"
     adc    $0,%edx
     mov    0(%edi),%ebx
     add    %ebx,%eax		# add low word from result
     mov    4(%edi),%ebx
     stosl			# [es:di] = %eax; di += 4;
     adc    %ebx,%edx		# add high word from result
     movl   $0,%ebx
     mov    %edx,%eax
     adc    $0,%ebx
     stosl			# [es:di] = %eax; di += 4;
     dec    %ecx		# --a_len
     jnz    30b			# jmp if a_len != 0
31:
    cmp    $0,%ebx		# is carry zero?
    jz     34f
    mov    0(%edi),%eax		# add in current word from *c
    add	   %ebx,%eax
    stosl			# [es:edi] = ax; edi += 4;
    jnc    34f
32:
    mov    0(%edi),%eax		# add in current word from *c
    adc	   $0,%eax
    stosl			# [es:edi] = ax; edi += 4;
    jc     32b
34:
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop
s_mpv_sqr_add_prop_sse2:
    push   %ebp
    mov    %esp,%ebp
    push   %edi
    push   %esi
    push   %ebx
    psubq  %mm2,%mm2		# carry = 0
    mov    12(%ebp),%ecx	# ecx = a_len
    mov    16(%ebp),%edi
    cmp    $0,%ecx
    je     36f			# jmp if a_len == 0
    mov    8(%ebp),%esi		# esi = a
    cld
35:
    movd   0(%esi),%mm0        # mm0 = *a
    movd   0(%edi),%mm3	       # fetch the sum
    add	   $4,%esi
    pmuludq %mm0,%mm0          # mm0 = sqr(a)
    paddq  %mm0,%mm2           # add the carry
    paddq  %mm3,%mm2           # add the low word
    movd   4(%edi),%mm3
    movd   %mm2,0(%edi)        # store the 32bit result
    psrlq  $32, %mm2	
    paddq  %mm3,%mm2           # add the high word
    movd   %mm2,4(%edi)        # store the 32bit result
    psrlq  $32, %mm2	       # save the carry.
    add    $8,%edi
    dec    %ecx			# --a_len
    jnz    35b			# jmp if a_len != 0
36:
    movd   %mm2,%ebx
    cmp    $0,%ebx		# is carry zero?
    jz     38f
    mov    0(%edi),%eax
    add    %ebx, %eax
    stosl
    jnc    38f
37:
    mov    0(%edi),%eax		# add in current word from *c
    adc	   $0,%eax
    stosl			# [es:edi] = ax; edi += 4;
    jc     37b
38:
    emms
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 #
 # Divide 64-bit (Nhi,Nlo) by 32-bit divisor, which must be normalized
 # so its high bit is 1.   This code is from NSPR.
 #
 # mp_err s_mpv_div_2dx1d(mp_digit Nhi, mp_digit Nlo, mp_digit divisor,
 # 		          mp_digit *qp, mp_digit *rp)

 #  esp +  0:   Caller's ebx
 #  esp +  4:	return address
 #  esp +  8:	Nhi	argument
 #  esp + 12:	Nlo	argument
 #  esp + 16:	divisor	argument
 #  esp + 20:	qp	argument
 #  esp + 24:   rp	argument
 #  registers:
 # 	eax:
 #	ebx:	carry
 #	ecx:	a_len
 #	edx:
 #	esi:	a ptr
 #	edi:	c ptr
 # 

.globl	s_mpv_div_2dx1d
.type	s_mpv_div_2dx1d,@function
s_mpv_div_2dx1d:
       push   %ebx
       mov    8(%esp),%edx
       mov    12(%esp),%eax
       mov    16(%esp),%ebx
       div    %ebx
       mov    20(%esp),%ebx
       mov    %eax,0(%ebx)
       mov    24(%esp),%ebx
       mov    %edx,0(%ebx)
       xor    %eax,%eax		# return zero
       pop    %ebx
       ret    
       nop
  
 # Magic indicating no need for an executable stack
.section .note.GNU-stack, "", @progbits
.previous
