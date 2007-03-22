#  ***** BEGIN LICENSE BLOCK *****
#
# Version: MPL 1.1/LGPL 2.1/GPL 2.0
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is IBM Corporation.
# Portions created by IBM are
#   Copyright (C) 2002, International Business Machines Corporation.
#   All Rights Reserved.
#
# Contributor(s):
#
#  Alternatively, the contents of this file may be used under the terms of
#  either of the GNU General Public License Version 2 or later (the "GPL"),
#  or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
#  in which case the provisions of the GPL or the LGPL are applicable instead
#  of those above. If you wish to allow use of your version of this file only
#  under the terms of either the GPL or the LGPL, and not to allow others to
#  use your version of this file under the terms of the MPL, indicate your
#  decision by deleting the provisions above and replace them with the notice
#  and other provisions required by the LGPL or the GPL. If you do not delete
#  the provisions above, a recipient may use your version of this file under
#  the terms of any one of the MPL, the GPL or the LGPL.
#
#  ***** END LICENSE BLOCK *****

.set r0,0; .set sp,1; .set RTOC,2; .set r3,3; .set r4,4
.set r5,5; .set r6,6; .set r7,7; .set r8,8; .set r9,9
.set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
.set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
.set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
.set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
.set r30,30; .set r31,31
.set f0,0; .set f1,1; .set f2,2; .set f3,3; .set f4,4
.set f5,5; .set f6,6; .set f7,7; .set f8,8; .set f9,9
.set f10,10; .set f11,11; .set f12,12; .set f13,13; .set f14,14
.set f15,15; .set f16,16; .set f17,17; .set f18,18; .set f19,19
.set f20,20; .set f21,21; .set f22,22; .set f23,23; .set f24,24
.set f25,25; .set f26,26; .set f27,27; .set f28,28; .set f29,29
.set f30,30; .set f31,31


        .rename     H.10.NO_SYMBOL{PR},""
        .rename     H.18.SharedStub{TC},"SharedStub"


# .text section

        .csect      H.10.NO_SYMBOL{PR}
        .globl      .SharedStub
        .globl      SharedStub{DS}
        .extern     .PrepareAndDispatch


#jimbo        csect   CODE{PR}
#
# on entry SharedStub has the method selector in r12, the rest of the original
# parameters are in r3 thru r10 and f1 thru f13
#
#jimbo	import	.PrepareAndDispatch

.SharedStub:
        mflr    r0
        std     r0,16(sp)

        mr      r12,r3      # Move methodIndex into r12 for LATER
        ld      r3,176(sp)  # Get the 'original' r3 (load 'this' into r3)

        stdu    sp,-248(sp) # room for linkage (24*2), fprData (104),
                            # gprData(28*2), outgoing params to 
                            # PrepareAndDispatch (40)

        std     r4,88(sp)   # link area (48) + PrepareAndDispatch parms (40)
        std     r5,96(sp)
        std     r6,104(sp)
        std     r7,112(sp)
        std     r8,120(sp)
        std     r9,128(sp)
        std     r10,136(sp)
        stfd    f1,144(sp)
        stfd    f2,152(sp)
        stfd    f3,160(sp)
        stfd    f4,168(sp)
        stfd    f5,176(sp)
        stfd    f6,184(sp)
        stfd    f7,192(sp)
        stfd    f8,200(sp)
        stfd    f9,208(sp)
        stfd    f10,216(sp)
        stfd    f11,224(sp)
        stfd    f12,232(sp)
        stfd    f13,240(sp)

        addi    r6,sp,88    # gprData
        addi    r7,sp,144   # fprData
                            # r3 has the 'self' pointer already
        mr      r4,r12      # methodIndex selector (it is now LATER)
        addi    r5,sp,488   # pointer to callers args area, beyond r3-r10
                            # mapped range
        # 32bit:    176 (stack-distance)    64bit:  248 (stack-distance)
        #           104 (this ptr offset)           176
        #            32 (8*4 for r3-r10)             64 (8*8)
        #           ---                             ---
        #           312                             488

        bl      .PrepareAndDispatch
        nop

        ld      r0,264(sp)  # 248+16
        addi    sp,sp,248
        mtlr    r0
        blr

# .data section

        .toc                            # 0x00000038
T.18.SharedStub:
        .tc     H.18.SharedStub{TC},SharedStub{DS}

        .csect  SharedStub{DS}
        .llong  .SharedStub             # "\0\0\0\0"
        .llong  TOC{TC0}                # "\0\0\0008"
        .llong  0x00000000              # "\0\0\0\0"
# End   csect   SharedStub{DS}

# .bss section
