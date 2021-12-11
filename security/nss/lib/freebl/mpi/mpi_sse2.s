# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef DARWIN
#define s_mpv_mul_d          _s_mpv_mul_d
#define s_mpv_mul_d_add      _s_mpv_mul_d_add
#define s_mpv_mul_d_add_prop _s_mpv_mul_d_add_prop
#define s_mpv_sqr_add_prop   _s_mpv_sqr_add_prop
#define s_mpv_div_2dx1d      _s_mpv_div_2dx1d
#define TYPE_FUNCTION(x)
#else
#define TYPE_FUNCTION(x) .type x, @function
#endif

.text

 #  ebp - 8:    caller's esi
 #  ebp - 4:    caller's edi
 #  ebp + 0:    caller's ebp
 #  ebp + 4:    return address
 #  ebp + 8:    a       argument
 #  ebp + 12:   a_len   argument
 #  ebp + 16:   b       argument
 #  ebp + 20:   c       argument
 #  registers:
 #      ebx:
 #      ecx:    a_len
 #      esi:    a ptr
 #      edi:    c ptr
.globl s_mpv_mul_d
.private_extern s_mpv_mul_d
TYPE_FUNCTION(s_mpv_mul_d)
s_mpv_mul_d:
    push   %ebp
    mov    %esp, %ebp
    push   %edi
    push   %esi
    psubq  %mm2, %mm2           # carry = 0
    mov    12(%ebp), %ecx       # ecx = a_len
    movd   16(%ebp), %mm1       # mm1 = b
    mov    20(%ebp), %edi
    cmp    $0, %ecx
    je     2f                   # jmp if a_len == 0
    mov    8(%ebp), %esi        # esi = a
    cld
1:
    movd   0(%esi), %mm0        # mm0 = *a++
    add    $4, %esi
    pmuludq %mm1, %mm0          # mm0 = b * *a++
    paddq  %mm0, %mm2           # add the carry
    movd   %mm2, 0(%edi)        # store the 32bit result
    add    $4, %edi
    psrlq  $32, %mm2            # save the carry
    dec    %ecx                 # --a_len
    jnz    1b                   # jmp if a_len != 0
2:
    movd   %mm2, 0(%edi)        # *c = carry
    emms
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 #  ebp - 8:    caller's esi
 #  ebp - 4:    caller's edi
 #  ebp + 0:    caller's ebp
 #  ebp + 4:    return address
 #  ebp + 8:    a       argument
 #  ebp + 12:   a_len   argument
 #  ebp + 16:   b       argument
 #  ebp + 20:   c       argument
 #  registers:
 #      ebx:
 #      ecx:    a_len
 #      esi:    a ptr
 #      edi:    c ptr
.globl s_mpv_mul_d_add
.private_extern s_mpv_mul_d_add
TYPE_FUNCTION(s_mpv_mul_d_add)
s_mpv_mul_d_add:
    push   %ebp
    mov    %esp, %ebp
    push   %edi
    push   %esi
    psubq  %mm2, %mm2           # carry = 0
    mov    12(%ebp), %ecx       # ecx = a_len
    movd   16(%ebp), %mm1       # mm1 = b
    mov    20(%ebp), %edi
    cmp    $0, %ecx
    je     2f                   # jmp if a_len == 0
    mov    8(%ebp), %esi        # esi = a
    cld
1:
    movd   0(%esi), %mm0        # mm0 = *a++
    add    $4, %esi
    pmuludq %mm1, %mm0          # mm0 = b * *a++
    paddq  %mm0, %mm2           # add the carry
    movd   0(%edi), %mm0
    paddq  %mm0, %mm2           # add the carry
    movd   %mm2, 0(%edi)        # store the 32bit result
    add    $4, %edi
    psrlq  $32, %mm2            # save the carry
    dec    %ecx                 # --a_len
    jnz    1b                   # jmp if a_len != 0
2:
    movd   %mm2, 0(%edi)        # *c = carry
    emms
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 #  ebp - 12:   caller's ebx
 #  ebp - 8:    caller's esi
 #  ebp - 4:    caller's edi
 #  ebp + 0:    caller's ebp
 #  ebp + 4:    return address
 #  ebp + 8:    a       argument
 #  ebp + 12:   a_len   argument
 #  ebp + 16:   b       argument
 #  ebp + 20:   c       argument
 #  registers:
 #      eax:
 #      ebx:    carry
 #      ecx:    a_len
 #      esi:    a ptr
 #      edi:    c ptr
.globl s_mpv_mul_d_add_prop
.private_extern s_mpv_mul_d_add_prop
TYPE_FUNCTION(s_mpv_mul_d_add_prop)
s_mpv_mul_d_add_prop:
    push   %ebp
    mov    %esp, %ebp
    push   %edi
    push   %esi
    push   %ebx
    psubq  %mm2, %mm2           # carry = 0
    mov    12(%ebp), %ecx       # ecx = a_len
    movd   16(%ebp), %mm1       # mm1 = b
    mov    20(%ebp), %edi
    cmp    $0, %ecx
    je     2f                   # jmp if a_len == 0
    mov    8(%ebp), %esi        # esi = a
    cld
1:
    movd   0(%esi), %mm0        # mm0 = *a++
    movd   0(%edi), %mm3        # fetch the sum
    add    $4, %esi
    pmuludq %mm1, %mm0          # mm0 = b * *a++
    paddq  %mm0, %mm2           # add the carry
    paddq  %mm3, %mm2           # add *c++
    movd   %mm2, 0(%edi)        # store the 32bit result
    add    $4, %edi
    psrlq  $32, %mm2            # save the carry
    dec    %ecx                 # --a_len
    jnz    1b                   # jmp if a_len != 0
2:
    movd   %mm2, %ebx
    cmp    $0, %ebx             # is carry zero?
    jz     4f
    mov    0(%edi), %eax
    add    %ebx, %eax
    stosl
    jnc    4f
3:
    mov    0(%edi), %eax        # add in current word from *c
    adc    $0, %eax
    stosl                       # [es:edi] = ax; edi += 4;
    jc     3b
4:
    emms
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 #  ebp - 12:   caller's ebx
 #  ebp - 8:    caller's esi
 #  ebp - 4:    caller's edi
 #  ebp + 0:    caller's ebp
 #  ebp + 4:    return address
 #  ebp + 8:    pa      argument
 #  ebp + 12:   a_len   argument
 #  ebp + 16:   ps      argument
 #  registers:
 #      eax:
 #      ebx:    carry
 #      ecx:    a_len
 #      esi:    a ptr
 #      edi:    c ptr
.globl s_mpv_sqr_add_prop
.private_extern s_mpv_sqr_add_prop
TYPE_FUNCTION(s_mpv_sqr_add_prop)
s_mpv_sqr_add_prop:
    push   %ebp
    mov    %esp, %ebp
    push   %edi
    push   %esi
    push   %ebx
    psubq  %mm2, %mm2           # carry = 0
    mov    12(%ebp), %ecx       # ecx = a_len
    mov    16(%ebp), %edi
    cmp    $0, %ecx
    je     2f                   # jmp if a_len == 0
    mov    8(%ebp), %esi        # esi = a
    cld
1:
    movd   0(%esi), %mm0        # mm0 = *a
    movd   0(%edi), %mm3        # fetch the sum
    add    $4, %esi
    pmuludq %mm0, %mm0          # mm0 = sqr(a)
    paddq  %mm0, %mm2           # add the carry
    paddq  %mm3, %mm2           # add the low word
    movd   4(%edi), %mm3
    movd   %mm2, 0(%edi)        # store the 32bit result
    psrlq  $32, %mm2
    paddq  %mm3, %mm2           # add the high word
    movd   %mm2, 4(%edi)        # store the 32bit result
    psrlq  $32, %mm2            # save the carry.
    add    $8, %edi
    dec    %ecx                 # --a_len
    jnz    1b                   # jmp if a_len != 0
2:
    movd   %mm2, %ebx
    cmp    $0, %ebx             # is carry zero?
    jz     4f
    mov    0(%edi), %eax
    add    %ebx, %eax
    stosl
    jnc    4f
3:
    mov    0(%edi), %eax        # add in current word from *c
    adc    $0, %eax
    stosl                       #  [es:edi] = ax; edi += 4;
    jc     3b
4:
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
 #                        mp_digit *qp, mp_digit *rp)

 #  esp +  0:   Caller's ebx
 #  esp +  4:   return address
 #  esp +  8:   Nhi     argument
 #  esp + 12:   Nlo     argument
 #  esp + 16:   divisor argument
 #  esp + 20:   qp      argument
 #  esp + 24:   rp      argument
 #  registers:
 #      eax:
 #      ebx:    carry
 #      ecx:    a_len
 #      edx:
 #      esi:    a ptr
 #      edi:    c ptr
 # 
.globl s_mpv_div_2dx1d
.private_extern s_mpv_div_2dx1d
TYPE_FUNCTION(s_mpv_div_2dx1d)
s_mpv_div_2dx1d:
       push   %ebx
       mov    8(%esp), %edx
       mov    12(%esp), %eax
       mov    16(%esp), %ebx
       div    %ebx
       mov    20(%esp), %ebx
       mov    %eax, 0(%ebx)
       mov    24(%esp), %ebx
       mov    %edx, 0(%ebx)
       xor    %eax, %eax        # return zero
       pop    %ebx
       ret    
       nop

#ifndef DARWIN
 # Magic indicating no need for an executable stack
.section .note.GNU-stack, "", @progbits
.previous
#endif
