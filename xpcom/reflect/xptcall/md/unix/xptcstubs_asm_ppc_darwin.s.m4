/* -*- Mode: asm -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

        .text
        .globl _SharedStub
dnl
define(STUB_MANGLED_ENTRY,
`        .globl  '$2`
        .align  2
'$2`:
        addi    r12,   0,'$1`
        b       _SharedStub')
dnl
define(STUB_ENTRY,
`       .if     '$1` < 10
STUB_MANGLED_ENTRY('$1`, `__ZN14nsXPTCStubBase5Stub'$1`Ev')
        .elseif '$1` < 100
STUB_MANGLED_ENTRY('$1`, `__ZN14nsXPTCStubBase6Stub'$1`Ev')
        .elseif '$1` < 1000
STUB_MANGLED_ENTRY('$1`, `__ZN14nsXPTCStubBase7Stub'$1`Ev')
        .else
        .err    "Stub'$1` >= 1000 not yet supported."
        .endif
')
dnl
define(SENTINEL_ENTRY, `')
dnl
include(xptcstubsdef.inc)
dnl
// See also xptcstubs_ppc_rhapsody.cpp:PrepareAndDispatch.
_SharedStub:
                                // Prolog(ue)
        mflr     r0             // Save the link register in the caller's
        stw      r0,   8(r1)    //   stack frame
        stwu     r1,-176(r1)    // Allocate stack space for our own frame and
                                //   adjust stack pointer

                                // Linkage area, 0(r1) to 24(r1)
                                // Original sp saved at 0(r1)

                                // Parameter area, 20 bytes from 24(r1) to
                                //   44(r1) to accomodate 5 arguments passed
                                //   to PrepareAndDispatch

                                // Local variables, 132 bytes from 44(r1)
                                //   to 176(r1), to accomodate 5 words and
                                //   13 doubles

        stw      r4,  44(r1)    // Save parameters passed in GPRs r4-r10;
        stw      r5,  48(r1)    //   a pointer to here will be passed to
        stw      r6,  52(r1)    //   PrepareAndDispatch for access to
        stw      r7,  56(r1)    //   arguments passed in registers.  r3,
        stw      r8,  60(r1)    //   the self pointer, is used for the
        stw      r9,  64(r1)    //   call but isn't otherwise needed in
        stw     r10,  68(r1)    //   PrepareAndDispatch, so it is not saved.

        stfd     f1,  72(r1)    // Do the same for floating-point parameters
        stfd     f2,  80(r1)    //   passed in FPRs f1-f13
        stfd     f3,  88(r1)
        stfd     f4,  96(r1)
        stfd     f5, 104(r1)
        stfd     f6, 112(r1)
        stfd     f7, 120(r1)
        stfd     f8, 128(r1)
        stfd     f9, 136(r1)
        stfd    f10, 144(r1)
        stfd    f11, 152(r1)
        stfd    f12, 160(r1)
        stfd    f13, 168(r1)

                                // Set up parameters for call to
                                //   PrepareAndDispatch.  argument=
                                // 0, pointer to self, already in r3
        mr       r4,r12         // 1, stub number
        addi     r5, r1, 204    // 2, pointer to the parameter area in our
                                //   caller's stack, for access to
                                //   parameters beyond those passed in
                                //   registers.  Skip past the first parameter
                                //   (corresponding to r3) for the same reason
                                //   as above.  176 (size of our frame) + 24
                                //   (size of caller's linkage) + 4 (skipped
                                //   parameter)
        addi     r6, r1,  44    // 3, pointer to saved GPRs
        addi     r7, r1,  72    // 4, pointer to saved FPRs

        bl      L_PrepareAndDispatch$stub
                                // Do it
        nop                     // Leave room for linker magic

                                // Epilog(ue)
        lwz      r0, 184(r1)    // Retrieve old link register value
        addi     r1, r1, 176    // Restore stack pointer
        mtlr     r0             // Restore link register
        blr                     // Return

.picsymbol_stub
L_PrepareAndDispatch$stub:      // Standard PIC symbol stub
        .indirect_symbol _PrepareAndDispatch
        mflr    r0
        bcl     20,31,L1$pb
L1$pb:
        mflr    r11
        addis   r11,r11,ha16(L1$lz-L1$pb)
        mtlr    r0
        lwz     r12,lo16(L1$lz-L1$pb)(r11)
        mtctr   r12
        addi    r11,r11,lo16(L1$lz-L1$pb)
        bctr
.lazy_symbol_pointer
L1$lz:
        .indirect_symbol _PrepareAndDispatch
        .long   dyld_stub_binding_helper
