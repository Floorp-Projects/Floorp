
// Select C numeric constant
        .radix  C
// for 64 bit mode, use .psr abi64
        .psr    abi32
// big endian
        .psr    msb
// Section has executable code
        .section .text, "ax","progbits"
// procedure named 'XPTC_InvokeByIndex'
        .proc   XPTC_InvokeByIndex
// manual bundling
        .explicit

// extern "C" PRUint32
// invoke_copy_to_stack(uint64_t* d,
//   const PRUint32 paramCount, nsXPTCVariant* s)
        .global invoke_copy_to_stack
//      .exclass  invoke_copy_to_stack, @fullyvisible
        .type   invoke_copy_to_stack,@function

//      .exclass  XPTC_InvokeByIndex, @fullyvisible
        .type   XPTC_InvokeByIndex,@function

// XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
//   PRUint32 paramCount, nsXPTCVariant* params);
XPTC_InvokeByIndex::
        .prologue
        .save ar.pfs, r37
// allocate 4 input args, 6 local args, and 8 output args
        alloc           r37 = ar.pfs, 4, 6, 8, 0   // M
        nop.i           0                       ;; // I

// unwind table already knows gp, no need to specify anything
        add             r39 = 0, gp                // A
        .save rp, r36
        mov             r36 = rp                   // I
        .vframe r38
        add             r38 = 0, sp             ;; // A

// We first calculate the amount of extra memory stack space required
// for the arguments, and register storage.
// We then call invoke_copy_to_stack() to write the argument contents
// to the specified memory regions.
// We then copy the integer arguments to integer registers, and floating
// arguments to float registers.
// Lastly we load the virtual table jump pointer, and call the target
// subroutine.

// in0 : that
// in1 : methodIndex
// in2 : paramCount
// in3 : params

// stack frame size is 16 + (8 * even(paramCount)) + 64 + 64
// 16 byte frame header
// 8 * even(paramCount) memory argument area
// 64 bytes integer register area
// 64 bytes float register area
// This scheme makes sure stack fram size is a multiple of 16

        .body
        add             r10 = 8, r0          // A
// r41 points to float register area
        add             r41 = -64, sp        // A
// r40 points to int register area
        add             r40 = -128, sp    ;; // A

        add             out1 = 0, r40        // A
        add             out2 = 0, r41        // A
        tbit.z          p14,p15 = in2,0   ;; // I

// compute 8 * even(paramCount)
(p14)   shladd          r11 = in2, 3, r0  ;; // A
(p15)   shladd          r11 = in2, 3, r10 ;; // A
        sub             out0 = r40, r11   ;; // A

// advance the stack frame
        add             sp = -16, out0       // A
        add             out3 = 0, in2        // A
        add             out4 = 0, in3        // A

// branch to invoke_copy_to_stack
        br.call.sptk.few rp = invoke_copy_to_stack ;; // B

// restore gp
        add             gp = 0, r39          // A
        add             out0 = 0, in0        // A

// load the integer and float registers
        ld8             out1 = [r40], 8      // M
        ldfd            f8 = [r41], 8       ;; // M

        ld8             out2 = [r40], 8      // M
        ldfd            f9 = [r41], 8       ;; // M

        ld8             out3 = [r40], 8      // M
        ldfd            f10 = [r41], 8      ;; // M

        ld8             out4 = [r40], 8      // M
        ldfd            f11 = [r41], 8      ;; // M

        ld8             out5 = [r40], 8      // M
        ldfd            f12 = [r41], 8      ;; // M
// 16 * methodIndex
        shladd          r11 = in1, 4, r0     // A

        ld8             out6 = [r40], 8      // M
        ldfd            f13 = [r41], 8      ;; // M

        ld8             out7 = [r40], 8      // M
        ldfd            f14 = [r41], 8       // M
        addp4           r8 = 0, in0       ;; // A

// look up virtual base table and dispatch to target subroutine
// This section assumes 32 bit pointer mode, and virtual base table
// layout from the ABI http://www.codesourcery.com/cxx-abi/abi.html

// load base table
        ld4             r8 = [r8]         ;; // M
        addp4           r8 = r11, r8      ;; // A

 // first entry is jump pointer, second entry is gp
        addp4           r9 = 8, r8       ;; // A
// load jump pointer
        ld8             r8 = [r8]

// load gp
        ld8             gp = [r9]        ;; // M
        mov             b6 = r8          ;; // I

// branch to target virtual function
        br.call.sptk.few rp = b6          ;; // B

// epilog
        mov             ar.pfs = r37         // I
        mov             rp = r36          ;; // I

        add             sp = 0, r38          // A
        add             gp = 0, r39          // A
        br.ret.sptk.few rp                ;; // B

        .endp


