/*
 *  mpi_x86_asm.c - MSVC inline assembly implementation of s_mpv_ functions.
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mpi-priv.h"

static int is_sse = -1;
extern unsigned long s_mpi_is_sse2();

/*
 *   ebp - 36:	caller's esi
 *   ebp - 32:	caller's edi
 *   ebp - 28:	
 *   ebp - 24:	
 *   ebp - 20:	
 *   ebp - 16:	
 *   ebp - 12:	
 *   ebp - 8:	
 *   ebp - 4:	
 *   ebp + 0:	caller's ebp
 *   ebp + 4:	return address
 *   ebp + 8:	a	argument
 *   ebp + 12:	a_len	argument
 *   ebp + 16:	b	argument
 *   ebp + 20:	c	argument
 *   registers:
 *  	eax:
 * 	ebx:	carry
 * 	ecx:	a_len
 * 	edx:
 * 	esi:	a ptr
 * 	edi:	c ptr
 */
__declspec(naked) void
s_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
  __asm {
    mov    eax, is_sse
    cmp    eax, 0
    je     s_mpv_mul_d_x86
    jg     s_mpv_mul_d_sse2
    call   s_mpi_is_sse2
    mov    is_sse, eax
    cmp    eax, 0
    jg     s_mpv_mul_d_sse2
s_mpv_mul_d_x86:
    push   ebp
    mov    ebp,esp
    sub    esp,28
    push   edi
    push   esi
    push   ebx
    mov    ebx,0		; carry = 0
    mov    ecx,[ebp+12]		; ecx = a_len
    mov    edi,[ebp+20]
    cmp    ecx,0
    je     L_2			; jmp if a_len == 0
    mov    esi,[ebp+8]		; esi = a
    cld
L_1:
    lodsd			; eax = [ds:esi]; esi += 4
    mov    edx,[ebp+16]		; edx = b
    mul    edx			; edx:eax = Phi:Plo = a_i * b

    add    eax,ebx		; add carry (ebx) to edx:eax
    adc    edx,0
    mov    ebx,edx		; high half of product becomes next carry

    stosd			; [es:edi] = ax; edi += 4;
    dec    ecx			; --a_len
    jnz    L_1			; jmp if a_len != 0
L_2:
    mov    [edi],ebx		; *c = carry
    pop    ebx
    pop    esi
    pop    edi
    leave  
    ret    
    nop
s_mpv_mul_d_sse2:
    push   ebp
    mov    ebp, esp
    push   edi
    push   esi
    psubq  mm2, mm2		; carry = 0
    mov    ecx, [ebp+12]	; ecx = a_len
    movd   mm1, [ebp+16]	; mm1 = b
    mov    edi, [ebp+20]
    cmp    ecx, 0
    je     L_6			; jmp if a_len == 0
    mov    esi, [ebp+8]		; esi = a
    cld
L_5:
    movd   mm0, [esi]		; mm0 = *a++
    add    esi, 4
    pmuludq mm0, mm1		; mm0 = b * *a++
    paddq  mm2, mm0		; add the carry
    movd   [edi], mm2		; store the 32bit result
    add    edi, 4
    psrlq  mm2, 32		; save the carry
    dec    ecx			; --a_len
    jnz    L_5			; jmp if a_len != 0
L_6:
    movd   [edi], mm2		; *c = carry
    emms
    pop    esi
    pop    edi
    leave  
    ret    
    nop
  }
}

/*
 *   ebp - 36:	caller's esi
 *   ebp - 32:	caller's edi
 *   ebp - 28:	
 *   ebp - 24:	
 *   ebp - 20:	
 *   ebp - 16:	
 *   ebp - 12:	
 *   ebp - 8:	
 *   ebp - 4:	
 *   ebp + 0:	caller's ebp
 *   ebp + 4:	return address
 *   ebp + 8:	a	argument
 *   ebp + 12:	a_len	argument
 *   ebp + 16:	b	argument
 *   ebp + 20:	c	argument
 *   registers:
 *  	eax:
 * 	ebx:	carry
 * 	ecx:	a_len
 * 	edx:
 * 	esi:	a ptr
 * 	edi:	c ptr
 */
__declspec(naked) void
s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
  __asm {
    mov    eax, is_sse
    cmp    eax, 0
    je     s_mpv_mul_d_add_x86
    jg     s_mpv_mul_d_add_sse2
    call   s_mpi_is_sse2
    mov    is_sse, eax
    cmp    eax, 0
    jg     s_mpv_mul_d_add_sse2
s_mpv_mul_d_add_x86:
    push   ebp
    mov    ebp,esp
    sub    esp,28
    push   edi
    push   esi
    push   ebx
    mov    ebx,0		; carry = 0
    mov    ecx,[ebp+12]		; ecx = a_len
    mov    edi,[ebp+20]
    cmp    ecx,0
    je     L_11			; jmp if a_len == 0
    mov    esi,[ebp+8]		; esi = a
    cld
L_10:
    lodsd			; eax = [ds:esi]; esi += 4
    mov    edx,[ebp+16]		; edx = b
    mul    edx			; edx:eax = Phi:Plo = a_i * b

    add    eax,ebx		; add carry (ebx) to edx:eax
    adc    edx,0
    mov    ebx,[edi]		; add in current word from *c
    add    eax,ebx		
    adc    edx,0
    mov    ebx,edx		; high half of product becomes next carry

    stosd			; [es:edi] = ax; edi += 4;
    dec    ecx			; --a_len
    jnz    L_10			; jmp if a_len != 0
L_11:
    mov    [edi],ebx		; *c = carry
    pop    ebx
    pop    esi
    pop    edi
    leave  
    ret    
    nop
s_mpv_mul_d_add_sse2:
    push   ebp
    mov    ebp, esp
    push   edi
    push   esi
    psubq  mm2, mm2		; carry = 0
    mov    ecx, [ebp+12]	; ecx = a_len
    movd   mm1, [ebp+16]	; mm1 = b
    mov    edi, [ebp+20]
    cmp    ecx, 0
    je     L_16			; jmp if a_len == 0
    mov    esi, [ebp+8]		; esi = a
    cld
L_15:
    movd   mm0, [esi]		; mm0 = *a++
    add    esi, 4
    pmuludq mm0, mm1		; mm0 = b * *a++
    paddq  mm2, mm0		; add the carry
    movd   mm0, [edi]
    paddq  mm2, mm0		; add the carry
    movd   [edi], mm2		; store the 32bit result
    add    edi, 4
    psrlq  mm2, 32		; save the carry
    dec    ecx			; --a_len
    jnz    L_15			; jmp if a_len != 0
L_16:
    movd   [edi], mm2		; *c = carry
    emms
    pop    esi
    pop    edi
    leave  
    ret    
    nop
  }
}

/*
 *   ebp - 36:	caller's esi
 *   ebp - 32:	caller's edi
 *   ebp - 28:	
 *   ebp - 24:	
 *   ebp - 20:	
 *   ebp - 16:	
 *   ebp - 12:	
 *   ebp - 8:	
 *   ebp - 4:	
 *   ebp + 0:	caller's ebp
 *   ebp + 4:	return address
 *   ebp + 8:	a	argument
 *   ebp + 12:	a_len	argument
 *   ebp + 16:	b	argument
 *   ebp + 20:	c	argument
 *   registers:
 *  	eax:
 * 	ebx:	carry
 * 	ecx:	a_len
 * 	edx:
 * 	esi:	a ptr
 * 	edi:	c ptr
 */
__declspec(naked) void
s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
  __asm {
    mov    eax, is_sse
    cmp    eax, 0
    je     s_mpv_mul_d_add_prop_x86
    jg     s_mpv_mul_d_add_prop_sse2
    call   s_mpi_is_sse2
    mov    is_sse, eax
    cmp    eax, 0
    jg     s_mpv_mul_d_add_prop_sse2
s_mpv_mul_d_add_prop_x86:
    push   ebp
    mov    ebp,esp
    sub    esp,28
    push   edi
    push   esi
    push   ebx
    mov    ebx,0		; carry = 0
    mov    ecx,[ebp+12]		; ecx = a_len
    mov    edi,[ebp+20]
    cmp    ecx,0
    je     L_21			; jmp if a_len == 0
    cld
    mov    esi,[ebp+8]		; esi = a
L_20:
    lodsd			; eax = [ds:esi]; esi += 4
    mov    edx,[ebp+16]		; edx = b
    mul    edx			; edx:eax = Phi:Plo = a_i * b

    add    eax,ebx		; add carry (ebx) to edx:eax
    adc    edx,0
    mov    ebx,[edi]		; add in current word from *c
    add    eax,ebx		
    adc    edx,0
    mov    ebx,edx		; high half of product becomes next carry

    stosd			; [es:edi] = ax; edi += 4;
    dec    ecx			; --a_len
    jnz    L_20			; jmp if a_len != 0
L_21:
    cmp    ebx,0		; is carry zero?
    jz     L_23
    mov    eax,[edi]		; add in current word from *c
    add    eax,ebx
    stosd			; [es:edi] = ax; edi += 4;
    jnc    L_23
L_22:
    mov    eax,[edi]		; add in current word from *c
    adc    eax,0
    stosd			; [es:edi] = ax; edi += 4;
    jc     L_22
L_23:
    pop    ebx
    pop    esi
    pop    edi
    leave  
    ret    
    nop
s_mpv_mul_d_add_prop_sse2:
    push   ebp
    mov    ebp, esp
    push   edi
    push   esi
    push   ebx
    psubq  mm2, mm2		; carry = 0
    mov    ecx, [ebp+12]	; ecx = a_len
    movd   mm1, [ebp+16]	; mm1 = b
    mov    edi, [ebp+20]
    cmp    ecx, 0
    je     L_26			; jmp if a_len == 0
    mov    esi, [ebp+8]		; esi = a
    cld
L_25:
    movd   mm0, [esi]		; mm0 = *a++
    movd   mm3, [edi]		; fetch the sum
    add    esi, 4
    pmuludq mm0, mm1		; mm0 = b * *a++
    paddq  mm2, mm0		; add the carry
    paddq  mm2, mm3		; add *c++
    movd   [edi], mm2		; store the 32bit result
    add    edi, 4
    psrlq  mm2, 32		; save the carry
    dec    ecx			; --a_len
    jnz    L_25			; jmp if a_len != 0
L_26:
    movd   ebx, mm2
    cmp    ebx, 0		; is carry zero?
    jz     L_28
    mov    eax, [edi]
    add    eax, ebx
    stosd
    jnc    L_28
L_27:
    mov    eax, [edi]		; add in current word from *c
    adc	   eax, 0
    stosd			; [es:edi] = ax; edi += 4;
    jc     L_27
L_28:
    emms
    pop    ebx
    pop    esi
    pop    edi
    leave  
    ret    
    nop
  }
}

/*
 *   ebp - 20:	caller's esi
 *   ebp - 16:	caller's edi
 *   ebp - 12:	
 *   ebp - 8:	carry
 *   ebp - 4:	a_len	local
 *   ebp + 0:	caller's ebp
 *   ebp + 4:	return address
 *   ebp + 8:	pa	argument
 *   ebp + 12:	a_len	argument
 *   ebp + 16:	ps	argument
 *   ebp + 20:	
 *   registers:
 *  	eax:
 * 	ebx:	carry
 * 	ecx:	a_len
 * 	edx:
 * 	esi:	a ptr
 * 	edi:	c ptr
 */
__declspec(naked) void
s_mpv_sqr_add_prop(const mp_digit *a, mp_size a_len, mp_digit *sqrs)
{
  __asm {
     mov    eax, is_sse
     cmp    eax, 0
     je     s_mpv_sqr_add_prop_x86
     jg     s_mpv_sqr_add_prop_sse2
     call   s_mpi_is_sse2
     mov    is_sse, eax
     cmp    eax, 0
     jg     s_mpv_sqr_add_prop_sse2
s_mpv_sqr_add_prop_x86:
     push   ebp
     mov    ebp,esp
     sub    esp,12
     push   edi
     push   esi
     push   ebx
     mov    ebx,0		; carry = 0
     mov    ecx,[ebp+12]	; a_len
     mov    edi,[ebp+16]	; edi = ps
     cmp    ecx,0
     je     L_31		; jump if a_len == 0
     cld
     mov    esi,[ebp+8]		; esi = pa
L_30:
     lodsd			; eax = [ds:si]; si += 4;
     mul    eax

     add    eax,ebx		; add "carry"
     adc    edx,0
     mov    ebx,[edi]
     add    eax,ebx		; add low word from result
     mov    ebx,[edi+4]
     stosd			; [es:di] = eax; di += 4;
     adc    edx,ebx		; add high word from result
     mov    ebx,0
     mov    eax,edx
     adc    ebx,0
     stosd			; [es:di] = eax; di += 4;
     dec    ecx			; --a_len
     jnz    L_30		; jmp if a_len != 0
L_31:
    cmp    ebx,0		; is carry zero?
    jz     L_34
    mov    eax,[edi]		; add in current word from *c
    add    eax,ebx
    stosd			; [es:edi] = ax; edi += 4;
    jnc    L_34
L_32:
    mov    eax,[edi]		; add in current word from *c
    adc    eax,0
    stosd			; [es:edi] = ax; edi += 4;
    jc     L_32
L_34:
    pop    ebx
    pop    esi
    pop    edi
    leave  
    ret    
    nop
s_mpv_sqr_add_prop_sse2:
    push   ebp
    mov    ebp, esp
    push   edi
    push   esi
    push   ebx
    psubq  mm2, mm2		; carry = 0
    mov    ecx, [ebp+12]	; ecx = a_len
    mov    edi, [ebp+16]
    cmp    ecx, 0
    je     L_36		; jmp if a_len == 0
    mov    esi, [ebp+8]		; esi = a
    cld
L_35:
    movd   mm0, [esi]		; mm0 = *a
    movd   mm3, [edi]		; fetch the sum
    add	   esi, 4
    pmuludq mm0, mm0		; mm0 = sqr(a)
    paddq  mm2, mm0		; add the carry
    paddq  mm2, mm3		; add the low word
    movd   mm3, [edi+4]
    movd   [edi], mm2		; store the 32bit result
    psrlq  mm2, 32	
    paddq  mm2, mm3		; add the high word
    movd   [edi+4], mm2		; store the 32bit result
    psrlq  mm2, 32		; save the carry.
    add    edi, 8
    dec    ecx			; --a_len
    jnz    L_35			; jmp if a_len != 0
L_36:
    movd   ebx, mm2
    cmp    ebx, 0		; is carry zero?
    jz     L_38
    mov    eax, [edi]
    add    eax, ebx
    stosd
    jnc    L_38
L_37:
    mov    eax, [edi]		; add in current word from *c
    adc	   eax, 0
    stosd			; [es:edi] = ax; edi += 4;
    jc     L_37
L_38:
    emms
    pop    ebx
    pop    esi
    pop    edi
    leave  
    ret    
    nop
  }
}

/* 
 *  Divide 64-bit (Nhi,Nlo) by 32-bit divisor, which must be normalized
 *  so its high bit is 1.   This code is from NSPR.
 *
 *  Dump of assembler code for function s_mpv_div_2dx1d:
 *  
 *   esp +  0:   Caller's ebx
 *   esp +  4:	return address
 *   esp +  8:	Nhi	argument
 *   esp + 12:	Nlo	argument
 *   esp + 16:	divisor	argument
 *   esp + 20:	qp	argument
 *   esp + 24:   rp	argument
 *   registers:
 *  	eax:
 * 	ebx:	carry
 * 	ecx:	a_len
 * 	edx:
 * 	esi:	a ptr
 * 	edi:	c ptr
 */  
__declspec(naked) mp_err
s_mpv_div_2dx1d(mp_digit Nhi, mp_digit Nlo, mp_digit divisor,
		mp_digit *qp, mp_digit *rp)
{
  __asm {
       push   ebx
       mov    edx,[esp+8]
       mov    eax,[esp+12]
       mov    ebx,[esp+16]
       div    ebx
       mov    ebx,[esp+20]
       mov    [ebx],eax
       mov    ebx,[esp+24]
       mov    [ebx],edx
       xor    eax,eax		; return zero
       pop    ebx
       ret    
       nop
  }
}
