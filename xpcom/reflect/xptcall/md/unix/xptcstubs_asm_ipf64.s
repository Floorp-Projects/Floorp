
// Select C numeric constant
        .radix  C
        .psr    abi64
        .psr    lsb
// Section has executable code
        .section .text, "ax","progbits"
// procedure named 'SharedStub'
        .proc   SharedStub
// manual bundling
        .explicit

        .global PrepareAndDispatch
//      .exclass  PrepareAndDispatch, @fullyvisible
        .type   PrepareAndDispatch,@function

SharedStub::
// 10 arguments, first 8 are the input arguments of previous
// function call.  The 9th one is methodIndex and the 10th is the
// pointer to the remaining input arguments.  The last two arguments
// are passed in memory.
        .prologue
        .save ar.pfs , r41
// allocate 8 input args, 4 local args, and 5 output args
        alloc           r41 = ar.pfs, 8, 4, 5, 0   // M
        .save rp, r40
        mov             r40 = rp                   // I
        add             out4 = 24, sp           ;; // I

        .save ar.unat, r42
        mov             r42 = ar.unat              // M
        .fframe 144
        add             sp = -144, sp              // A
// unwind table already knows gp, don't need to specify anything
        add             r43 = 0, gp             ;; // A

// We have possible 8 integer registers and 8 float registers that could
// be arguments.  We also have a stack region from the previous
// stack frame that may hold some stack arguments.
// We need to write the integer registers to a memory region, write
// the float registers to a memory region (making sure we don't step
// on NAT while touching the registers).  We also mark the memory
// address of the stack arguments.
// We then call PrepareAndDispatch() specifying the three memory
// region pointers.


        .body
        add             out0 = 0, in0        // A  move self ptr
// 144 bytes = 16 byte stack header + 64 byte int space + 64 byte float space
// methodIndex is at 144 + 16 bytes away from current sp
// (current frame + previous frame header)
        ld8             out4 = [out4]        // M  restarg address
        add             r11  = 160, sp    ;; // A  address of methodIndex

        ld8             out1 = [r11]         // M  load methodIndex
// sp + 16 is the start of intargs
        add             out2 = 16, sp        // A  address of intargs
// the intargs take up 64 bytes, so sp + 16 + 64 is the start of floatargs
        add             out3 = 80, sp     ;; // A  address of floatargs

        add             r11 = 0, out2     ;; // A
        st8.spill       [r11] = in1, 8       // M
        add             r10 = 0, out3     ;; // A

        st8.spill       [r11] = in2, 8    ;; // M
        st8.spill       [r11] = in3, 8       // M
        nop.i           0                 ;; // I

        st8.spill       [r11] = in4, 8    ;; // M
        st8.spill       [r11] = in5, 8       // M
        nop.i           0                 ;; // I

        st8.spill       [r11] = in6, 8    ;; // M
        st8.spill       [r11] = in7          // M
        fclass.nm       p14,p15 = f8,@nat ;; // F

(p14)   stfd            [r10] = f8, 8        // M
(p15)   add             r10 = 8, r10         // A
        fclass.nm       p12,p13 = f9,@nat ;; // F

(p12)   stfd            [r10] = f9, 8        // M
(p13)   add             r10 = 8, r10         // A
        fclass.nm       p14,p15 =f10,@nat ;; // F

(p14)   stfd            [r10] = f10, 8       // M
(p15)   add             r10 = 8, r10         // A
        fclass.nm       p12,p13 =f11,@nat ;; // F

(p12)   stfd            [r10] = f11, 8       // M
(p13)   add             r10 = 8, r10         // A
        fclass.nm       p14,p15 =f12,@nat ;; // F

(p14)   stfd            [r10] = f12, 8       // M
(p15)   add             r10 = 8, r10         // A
        fclass.nm       p12,p13 =f13,@nat ;; // F

(p12)   stfd            [r10] = f13, 8       // M
(p13)   add             r10 = 8, r10         // A
        fclass.nm       p14,p15 =f14,@nat ;; // F

(p14)   stfd            [r10] = f14, 8       // M
(p15)   add             r10 = 8, r10         // A
        fclass.nm       p12,p13 =f15,@nat ;; // F

(p12)   stfd            [r10] = f15, 8       // M
(p13)   add             r10 = 8, r10         // A

// branch to PrepareAndDispatch
        br.call.dptk.few rp = PrepareAndDispatch ;; // B

// epilog
        mov             ar.unat = r42        // M
        mov             ar.pfs = r41         // I
        mov             rp = r40          ;; // I

        add             gp = 0, r43          // A
        add             sp = 144, sp         // A
        br.ret.dptk.few rp                ;; // B

        .endp

/* Magic indicating no need for an executable stack */
.section .note.GNU-stack, "", @progbits ; .previous
