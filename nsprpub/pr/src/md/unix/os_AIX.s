# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

.set r0,0; .set SP,1; .set RTOC,2; .set r3,3; .set r4,4
.set r5,5; .set r6,6; .set r7,7; .set r8,8; .set r9,9
.set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
.set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
.set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
.set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
.set r30,30; .set r31,31


        .rename H.10.NO_SYMBOL{PR},""
        .rename H.18.longjmp{TC},"longjmp"

        .lglobl H.10.NO_SYMBOL{PR}
        .globl  .longjmp
        .globl  longjmp{DS}
        .extern .sigcleanup
        .extern .jmprestfpr

# .text section

        .csect  H.10.NO_SYMBOL{PR}
.longjmp:
         mr   r13,r3
         mr   r14,r4
        stu   SP,-56(SP)
         bl   .sigcleanup
          l   RTOC,0x14(SP)
        cal   SP,0x38(SP)
         mr   r3,r13
         mr   r4,r14
          l   r5,0x8(r3)
          l   SP,0xc(r3)
          l   r7,0xf8(r3)
         st   r7,0x0(SP)
          l   RTOC,0x10(r3)
         bl   .jmprestfpr
# 1 == cr0 in disassembly
       cmpi   1,r4,0x0  
       mtlr   r5
         lm   r13,0x14(r3)
          l   r5,0x60(r3)
      mtcrf   0x38,r5
         mr   r3,r4
        bne   __L1
        lil   r3,0x1
__L1:
         br

# traceback table
        .long   0x00000000
        .byte   0x00                    # VERSION=0
        .byte   0x00                    # LANG=TB_C
        .byte   0x20                    # IS_GL=0,IS_EPROL=0,HAS_TBOFF=1
                                        # INT_PROC=0,HAS_CTL=0,TOCLESS=0
                                        # FP_PRESENT=0,LOG_ABORT=0
        .byte   0x40                    # INT_HNDL=0,NAME_PRESENT=1
                                        # USES_ALLOCA=0,CL_DIS_INV=WALK_ONCOND
                                        # SAVES_CR=0,SAVES_LR=0
        .byte   0x80                    # STORES_BC=1,FPR_SAVED=0
        .byte   0x00                    # GPR_SAVED=0
        .byte   0x02                    # FIXEDPARMS=2
        .byte   0x01                    # FLOATPARMS=0,PARMSONSTK=1
        .long   0x00000000              #
        .long   0x00000014              # TB_OFFSET
        .short  7                       # NAME_LEN
        .byte   "longjmp"
        .byte   0                       # padding
        .byte   0                       # padding
        .byte   0                       # padding
# End of traceback table
        .long   0x00000000              # "\0\0\0\0"

# .data section

        .toc                            # 0x00000038
T.18.longjmp:
        .tc     H.18.longjmp{TC},longjmp{DS}

        .csect  longjmp{DS}
        .long   .longjmp                # "\0\0\0\0"
        .long   TOC{TC0}                # "\0\0\0008"
        .long   0x00000000              # "\0\0\0\0"
# End   csect   longjmp{DS}

# .bss section
