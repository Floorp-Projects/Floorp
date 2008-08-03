/*
 *  mpi_x86.c - MSVC inline assembly implementation of s_mpv_ functions.
 * 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mpi-priv.h"

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
    je     L_4			; jmp if a_len == 0
    mov    esi,[ebp+8]		; esi = a
    cld
L_3:
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
    jnz    L_3			; jmp if a_len != 0
L_4:
    mov    [edi],ebx		; *c = carry
    pop    ebx
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
    je     L_6			; jmp if a_len == 0
    cld
    mov    esi,[ebp+8]		; esi = a
L_5:
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
    jnz    L_5			; jmp if a_len != 0
L_6:
    cmp    ebx,0		; is carry zero?
    jz     L_8
    mov    eax,[edi]		; add in current word from *c
    add    eax,ebx
    stosd			; [es:edi] = ax; edi += 4;
    jnc    L_8
L_7:
    mov    eax,[edi]		; add in current word from *c
    adc    eax,0
    stosd			; [es:edi] = ax; edi += 4;
    jc     L_7
L_8:
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
     je     L_11		; jump if a_len == 0
     cld
     mov    esi,[ebp+8]		; esi = pa
L_10:
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
     jnz    L_10		; jmp if a_len != 0
L_11:
    cmp    ebx,0		; is carry zero?
    jz     L_14
    mov    eax,[edi]		; add in current word from *c
    add    eax,ebx
    stosd			; [es:edi] = ax; edi += 4;
    jnc    L_14
L_12:
    mov    eax,[edi]		; add in current word from *c
    adc    eax,0
    stosd			; [es:edi] = ax; edi += 4;
    jc     L_12
L_14:
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
