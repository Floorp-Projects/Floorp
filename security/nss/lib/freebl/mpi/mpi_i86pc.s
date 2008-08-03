/
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
/ The Original Code is the Netscape security libraries.
/
/ The Initial Developer of the Original Code is
/ Netscape Communications Corporation.
/ Portions created by the Initial Developer are Copyright (C) 2001
/ the Initial Developer. All Rights Reserved.
/
/ Contributor(s):
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
/ ***** END LICENSE BLOCK *****

/  $Id: mpi_i86pc.s,v 1.2 2006/12/11 09:45:32 gerv%gerv.net Exp $
/

.text

 /  ebp - 36:	caller's esi
 /  ebp - 32:	caller's edi
 /  ebp - 28:	
 /  ebp - 24:	
 /  ebp - 20:	
 /  ebp - 16:	
 /  ebp - 12:	
 /  ebp - 8:	
 /  ebp - 4:	
 /  ebp + 0:	caller's ebp
 /  ebp + 4:	return address
 /  ebp + 8:	a	argument
 /  ebp + 12:	a_len	argument
 /  ebp + 16:	b	argument
 /  ebp + 20:	c	argument
 /  registers:
 / 	eax:
 /	ebx:	carry
 /	ecx:	a_len
 /	edx:
 /	esi:	a ptr
 /	edi:	c ptr
.globl	s_mpv_mul_d
.type	s_mpv_mul_d,@function
s_mpv_mul_d:
    push   %ebp
    mov    %esp,%ebp
    sub    $28,%esp
    push   %edi
    push   %esi
    push   %ebx
    movl   $0,%ebx		/ carry = 0
    mov    12(%ebp),%ecx	/ ecx = a_len
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     L2			/ jmp if a_len == 0
    mov    8(%ebp),%esi		/ esi = a
    cld
L1:
    lodsl			/ eax = [ds:esi]; esi += 4
    mov    16(%ebp),%edx	/ edx = b
    mull   %edx			/ edx:eax = Phi:Plo = a_i * b

    add    %ebx,%eax		/ add carry (%ebx) to edx:eax
    adc    $0,%edx
    mov    %edx,%ebx		/ high half of product becomes next carry

    stosl			/ [es:edi] = ax; edi += 4;
    dec    %ecx			/ --a_len
    jnz    L1			/ jmp if a_len != 0
L2:
    mov    %ebx,0(%edi)		/ *c = carry
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 /  ebp - 36:	caller's esi
 /  ebp - 32:	caller's edi
 /  ebp - 28:	
 /  ebp - 24:	
 /  ebp - 20:	
 /  ebp - 16:	
 /  ebp - 12:	
 /  ebp - 8:	
 /  ebp - 4:	
 /  ebp + 0:	caller's ebp
 /  ebp + 4:	return address
 /  ebp + 8:	a	argument
 /  ebp + 12:	a_len	argument
 /  ebp + 16:	b	argument
 /  ebp + 20:	c	argument
 /  registers:
 / 	eax:
 /	ebx:	carry
 /	ecx:	a_len
 /	edx:
 /	esi:	a ptr
 /	edi:	c ptr
.globl	s_mpv_mul_d_add
.type	s_mpv_mul_d_add,@function
s_mpv_mul_d_add:
    push   %ebp
    mov    %esp,%ebp
    sub    $28,%esp
    push   %edi
    push   %esi
    push   %ebx
    movl   $0,%ebx		/ carry = 0
    mov    12(%ebp),%ecx	/ ecx = a_len
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     L4			/ jmp if a_len == 0
    mov    8(%ebp),%esi		/ esi = a
    cld
L3:
    lodsl			/ eax = [ds:esi]; esi += 4
    mov    16(%ebp),%edx	/ edx = b
    mull   %edx			/ edx:eax = Phi:Plo = a_i * b

    add    %ebx,%eax		/ add carry (%ebx) to edx:eax
    adc    $0,%edx
    mov    0(%edi),%ebx		/ add in current word from *c
    add    %ebx,%eax		
    adc    $0,%edx
    mov    %edx,%ebx		/ high half of product becomes next carry

    stosl			/ [es:edi] = ax; edi += 4;
    dec    %ecx			/ --a_len
    jnz    L3			/ jmp if a_len != 0
L4:
    mov    %ebx,0(%edi)		/ *c = carry
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 /  ebp - 36:	caller's esi
 /  ebp - 32:	caller's edi
 /  ebp - 28:	
 /  ebp - 24:	
 /  ebp - 20:	
 /  ebp - 16:	
 /  ebp - 12:	
 /  ebp - 8:	
 /  ebp - 4:	
 /  ebp + 0:	caller's ebp
 /  ebp + 4:	return address
 /  ebp + 8:	a	argument
 /  ebp + 12:	a_len	argument
 /  ebp + 16:	b	argument
 /  ebp + 20:	c	argument
 /  registers:
 / 	eax:
 /	ebx:	carry
 /	ecx:	a_len
 /	edx:
 /	esi:	a ptr
 /	edi:	c ptr
.globl	s_mpv_mul_d_add_prop
.type	s_mpv_mul_d_add_prop,@function
s_mpv_mul_d_add_prop:
    push   %ebp
    mov    %esp,%ebp
    sub    $28,%esp
    push   %edi
    push   %esi
    push   %ebx
    movl   $0,%ebx		/ carry = 0
    mov    12(%ebp),%ecx	/ ecx = a_len
    mov    20(%ebp),%edi
    cmp    $0,%ecx
    je     L6			/ jmp if a_len == 0
    cld
    mov    8(%ebp),%esi		/ esi = a
L5:
    lodsl			/ eax = [ds:esi]; esi += 4
    mov    16(%ebp),%edx	/ edx = b
    mull   %edx			/ edx:eax = Phi:Plo = a_i * b

    add    %ebx,%eax		/ add carry (%ebx) to edx:eax
    adc    $0,%edx
    mov    0(%edi),%ebx		/ add in current word from *c
    add    %ebx,%eax		
    adc    $0,%edx
    mov    %edx,%ebx		/ high half of product becomes next carry

    stosl			/ [es:edi] = ax; edi += 4;
    dec    %ecx			/ --a_len
    jnz    L5			/ jmp if a_len != 0
L6:
    cmp    $0,%ebx		/ is carry zero?
    jz     L8
    mov    0(%edi),%eax		/ add in current word from *c
    add	   %ebx,%eax
    stosl			/ [es:edi] = ax; edi += 4;
    jnc    L8
L7:
    mov    0(%edi),%eax		/ add in current word from *c
    adc	   $0,%eax
    stosl			/ [es:edi] = ax; edi += 4;
    jc     L7
L8:
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 /  ebp - 20:	caller's esi
 /  ebp - 16:	caller's edi
 /  ebp - 12:	
 /  ebp - 8:	carry
 /  ebp - 4:	a_len	local
 /  ebp + 0:	caller's ebp
 /  ebp + 4:	return address
 /  ebp + 8:	pa	argument
 /  ebp + 12:	a_len	argument
 /  ebp + 16:	ps	argument
 /  ebp + 20:	
 /  registers:
 / 	eax:
 /	ebx:	carry
 /	ecx:	a_len
 /	edx:
 /	esi:	a ptr
 /	edi:	c ptr

.globl	s_mpv_sqr_add_prop
.type	s_mpv_sqr_add_prop,@function
s_mpv_sqr_add_prop:
     push   %ebp
     mov    %esp,%ebp
     sub    $12,%esp
     push   %edi
     push   %esi
     push   %ebx
     movl   $0,%ebx		/ carry = 0
     mov    12(%ebp),%ecx	/ a_len
     mov    16(%ebp),%edi	/ edi = ps
     cmp    $0,%ecx
     je     L11			/ jump if a_len == 0
     cld
     mov    8(%ebp),%esi	/ esi = pa
L10:
     lodsl			/ %eax = [ds:si]; si += 4;
     mull   %eax

     add    %ebx,%eax		/ add "carry"
     adc    $0,%edx
     mov    0(%edi),%ebx
     add    %ebx,%eax		/ add low word from result
     mov    4(%edi),%ebx
     stosl			/ [es:di] = %eax; di += 4;
     adc    %ebx,%edx		/ add high word from result
     movl   $0,%ebx
     mov    %edx,%eax
     adc    $0,%ebx
     stosl			/ [es:di] = %eax; di += 4;
     dec    %ecx		/ --a_len
     jnz    L10			/ jmp if a_len != 0
L11:
    cmp    $0,%ebx		/ is carry zero?
    jz     L14
    mov    0(%edi),%eax		/ add in current word from *c
    add	   %ebx,%eax
    stosl			/ [es:edi] = ax; edi += 4;
    jnc    L14
L12:
    mov    0(%edi),%eax		/ add in current word from *c
    adc	   $0,%eax
    stosl			/ [es:edi] = ax; edi += 4;
    jc     L12
L14:
    pop    %ebx
    pop    %esi
    pop    %edi
    leave  
    ret    
    nop

 /
 / Divide 64-bit (Nhi,Nlo) by 32-bit divisor, which must be normalized
 / so its high bit is 1.   This code is from NSPR.
 /
 / mp_err s_mpv_div_2dx1d(mp_digit Nhi, mp_digit Nlo, mp_digit divisor,
 / 		          mp_digit *qp, mp_digit *rp)

 /  esp +  0:   Caller's ebx
 /  esp +  4:	return address
 /  esp +  8:	Nhi	argument
 /  esp + 12:	Nlo	argument
 /  esp + 16:	divisor	argument
 /  esp + 20:	qp	argument
 /  esp + 24:   rp	argument
 /  registers:
 / 	eax:
 /	ebx:	carry
 /	ecx:	a_len
 /	edx:
 /	esi:	a ptr
 /	edi:	c ptr
 / 

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
       xor    %eax,%eax		/ return zero
       pop    %ebx
       ret    
       nop
  
