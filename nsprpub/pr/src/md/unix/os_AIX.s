# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
