/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *
 *  This PA-RISC 2.0 function computes the product of two unsigned integers,
 *  and adds the result to a previously computed integer.  The multiplicand
 *  is a 512-bit (64-byte, eight doubleword) unsigned integer, stored in
 *  memory in little-double-wordian order.  The multiplier is an unsigned
 *  64-bit integer.  The previously computed integer to which the product is
 *  added is located in the result ("res") area, and is assumed to be a
 *  576-bit (72-byte, nine doubleword) unsigned integer, stored in memory
 *  in little-double-wordian order.  This value normally will be the result
 *  of a previously computed nine doubleword result.  It is not necessary
 *  to pad the multiplicand with an additional 64-bit zero doubleword.
 *
 *  Multiplicand, multiplier, and addend ideally should be aligned at
 *  16-byte boundaries for best performance.  The code will function
 *  correctly for alignment at eight-byte boundaries which are not 16-byte
 *  boundaries, but the execution may be slightly slower due to even/odd
 *  bank conflicts on PA-RISC 8000 processors.
 *
 *  This function is designed to accept the same calling sequence as Bill
 *  Ackerman's "maxpy_little" function.  The carry from the ninth doubleword
 *  of the result is written to the tenth word of the result, as is done by
 *  Bill Ackerman's function.  The final carry also is returned as an
 *  integer, which may be ignored.  The function prototype may be either
 *  of the following:
 *
 *      void multacc512( int l, chunk* m, const chunk* a, chunk* res );
 *          or
 *      int multacc512( int l, chunk* m, const chunk* a, chunk* res );
 *
 *  where:  "l" originally denoted vector lengths.  This parameter is
 *      ignored.  This function always assumes a multiplicand length of
 *      512 bits (eight doublewords), and addend and result lengths of
 *      576 bits (nine doublewords).
 *
 *      "m" is a pointer to the doubleword multiplier, ideally aligned
 *      on a 16-byte boundary.
 *
 *      "a" is a pointer to the eight-doubleword multiplicand, stored
 *      in little-double-wordian order, and ideally aligned on a 16-byte
 *      boundary.
 *
 *      "res" is a pointer to the nine doubleword addend, and to the
 *      nine-doubleword product computed by this function.  The result
 *      also is stored in little-double-wordian order, and ideally is
 *      aligned on a 16-byte boundary. It is expected that the alignment
 *      of the "res" area may alternate between even/odd doubleword
 *      boundaries for successive calls for 512-bit x 512-bit
 *      multiplications.
 *
 *  The code for this function has been scheduled to use the parallelism
 *  of the PA-RISC 8000 series microprocessors as well as the author was
 *  able.  Comments and/or suggestions for improvement are welcomed.
 *
 *  The code is "64-bit safe".  This means it may be called in either
 *  the 32ILP context or the 64LP context.  All 64-bits of registers are
 *  saved and restored.
 *
 *  This code is self-contained.  It requires no other header files in order
 *  to compile and to be linkable on a PA-RISC 2.0 machine.  Symbolic
 *  definitions for registers and stack offsets are included within this
 *  one source file.
 *
 *  This is a leaf routine.  As such, minimal use is made of the stack area.
 *  Of the 192 bytes allocated, 64 bytes are used for saving/restoring eight
 *  general registers, and 128 bytes are used to move intermediate products
 *  from the floating-point registers to the general registers.  Stack
 *  protocols assure proper alignment of these areas.
 *
 */


/*  ====================================================================*/
/*      symbolic definitions for PA-RISC registers      */
/*      in the MIPS style, avoids lots of case shifts       */
/*      assigments (except t4) preserve register number parity  */
/*  ====================================================================*/

#define zero    %r0         /* permanent zero */
#define t5      %r1         /* temp register, altered by addil */

#define rp      %r2         /* return pointer */

#define s1      %r3         /* callee saves register*/
#define s0      %r4         /* callee saves register*/
#define s3      %r5         /* callee saves register*/
#define s2      %r6         /* callee saves register*/
#define s5      %r7         /* callee saves register*/
#define s4      %r8         /* callee saves register*/
#define s7      %r9         /* callee saves register*/
#define s6      %r10        /* callee saves register*/

#define t1      %r19        /* caller saves register*/
#define t0      %r20        /* caller saves register*/
#define t3      %r21        /* caller saves register*/
#define t2      %r22        /* caller saves register*/

#define a3      %r23        /* fourth argument register, high word */
#define a2      %r24        /* third argument register, low word*/
#define a1      %r25        /* second argument register, high word*/
#define a0      %r26        /* first argument register, low word*/

#define v0      %r28        /* high order return value*/
#define v1      %r29        /* low order return value*/

#define sp      %r30        /* stack pointer*/
#define t4      %r31        /* temporary register   */

#define fa0     %fr4        /* first argument register*/
#define fa1     %fr5        /* second argument register*/
#define fa2     %fr6        /* third argument register*/
#define fa3     %fr7        /* fourth argument register*/

#define fa0r    %fr4R       /* first argument register*/
#define fa1r    %fr5R       /* second argument register*/
#define fa2r    %fr6R       /* third argument register*/
#define fa3r    %fr7R       /* fourth argument register*/

#define ft0     %fr8        /* caller saves register*/
#define ft1     %fr9        /* caller saves register*/
#define ft2     %fr10       /* caller saves register*/
#define ft3     %fr11       /* caller saves register*/

#define ft0r    %fr8R       /* caller saves register*/
#define ft1r    %fr9R       /* caller saves register*/
#define ft2r    %fr10R      /* caller saves register*/
#define ft3r    %fr11R      /* caller saves register*/

#define ft4     %fr22       /* caller saves register*/
#define ft5     %fr23       /* caller saves register*/
#define ft6     %fr24       /* caller saves register*/
#define ft7     %fr25       /* caller saves register*/
#define ft8     %fr26       /* caller saves register*/
#define ft9     %fr27       /* caller saves register*/
#define ft10    %fr28       /* caller saves register*/
#define ft11    %fr29       /* caller saves register*/
#define ft12    %fr30       /* caller saves register*/
#define ft13    %fr31       /* caller saves register*/

#define ft4r    %fr22R      /* caller saves register*/
#define ft5r    %fr23R      /* caller saves register*/
#define ft6r    %fr24R      /* caller saves register*/
#define ft7r    %fr25R      /* caller saves register*/
#define ft8r    %fr26R      /* caller saves register*/
#define ft9r    %fr27R      /* caller saves register*/
#define ft10r   %fr28R      /* caller saves register*/
#define ft11r   %fr29R      /* caller saves register*/
#define ft12r   %fr30R      /* caller saves register*/
#define ft13r   %fr31R      /* caller saves register*/



/*  ================================================================== */
/*      functional definitions for PA-RISC registers           */
/*  ================================================================== */

/*              general registers           */

#define T1      a0          /* temp, (length parameter ignored)             */

#define pM      a1          /* -> 64-bit multiplier                         */
#define T2      a1          /* temp, (after fetching multiplier)            */

#define pA      a2          /* -> multiplicand vector (8 64-bit words)      */
#define T3      a2          /* temp, (after fetching multiplicand)          */

#define pR      a3          /* -> addend vector (8 64-bit doublewords,
                                  result vector (9 64-bit words)            */

#define S0      s0          /* callee saves summand registers               */
#define S1      s1
#define S2      s2
#define S3      s3
#define S4      s4
#define S5      s5
#define S6      s6
#define S7      s7

#define S8      v0          /* caller saves summand registers               */
#define S9      v1
#define S10     t0
#define S11     t1
#define S12     t2
#define S13     t3
#define S14     t4
#define S15     t5



/*              floating-point registers                                    */

#define M       fa0         /* multiplier double word                       */
#define MR      fa0r        /* low order half of multiplier double word     */
#define ML      fa0         /* high order half of multiplier double word    */

#define A0      fa2         /* multiplicand double word 0                   */
#define A0R     fa2r        /* low order half of multiplicand double word   */
#define A0L     fa2         /* high order half of multiplicand double word  */

#define A1      fa3         /* multiplicand double word 1                   */
#define A1R     fa3r        /* low order half of multiplicand double word   */
#define A1L     fa3         /* high order half of multiplicand double word  */

#define A2      ft0         /* multiplicand double word 2                   */
#define A2R     ft0r        /* low order half of multiplicand double word   */
#define A2L     ft0         /* high order half of multiplicand double word  */

#define A3      ft1         /* multiplicand double word 3                   */
#define A3R     ft1r        /* low order half of multiplicand double word   */
#define A3L     ft1         /* high order half of multiplicand double word  */

#define A4      ft2         /* multiplicand double word 4                   */
#define A4R     ft2r        /* low order half of multiplicand double word   */
#define A4L     ft2         /* high order half of multiplicand double word  */

#define A5      ft3         /* multiplicand double word 5                   */
#define A5R     ft3r        /* low order half of multiplicand double word   */
#define A5L     ft3         /* high order half of multiplicand double word  */

#define A6      ft4         /* multiplicand double word 6                   */
#define A6R     ft4r        /* low order half of multiplicand double word   */
#define A6L     ft4         /* high order half of multiplicand double word  */

#define A7      ft5         /* multiplicand double word 7                   */
#define A7R     ft5r        /* low order half of multiplicand double word   */
#define A7L     ft5         /* high order half of multiplicand double word  */

#define P0      ft6         /* product word 0                               */
#define P1      ft7         /* product word 0                               */
#define P2      ft8         /* product word 0                               */
#define P3      ft9         /* product word 0                               */
#define P4      ft10        /* product word 0                               */
#define P5      ft11        /* product word 0                               */
#define P6      ft12        /* product word 0                               */
#define P7      ft13        /* product word 0                               */




/*  ======================================================================  */
/*      symbolic definitions for HP-UX stack offsets                        */
/*      symbolic definitions for memory NOPs                                */
/*  ======================================================================  */

#define ST_SZ       192         /* stack area total size                    */

#define SV0         -192(sp)    /* general register save area               */
#define SV1         -184(sp)
#define SV2         -176(sp)
#define SV3         -168(sp)
#define SV4         -160(sp)
#define SV5         -152(sp)
#define SV6         -144(sp)
#define SV7         -136(sp)

#define XF0         -128(sp)    /* data transfer area                       */
#define XF1         -120(sp)    /* for floating-pt to integer regs          */
#define XF2         -112(sp)
#define XF3         -104(sp)
#define XF4         -96(sp)
#define XF5         -88(sp)
#define XF6         -80(sp)
#define XF7         -72(sp)
#define XF8         -64(sp)
#define XF9         -56(sp)
#define XF10        -48(sp)
#define XF11        -40(sp)
#define XF12        -32(sp)
#define XF13        -24(sp)
#define XF14        -16(sp)
#define XF15        -8(sp)

#define mnop    proberi (sp),3,zero     /* memory NOP                       */




/*  ======================================================================  */
/*      assembler formalities                                               */
/*  ======================================================================  */

#ifdef __LP64__
                .level  2.0W
#else
                .level  2.0
#endif
                .space    $TEXT$
                .subspa   $CODE$
                .align    16

/*  ======================================================================  */
/*      here to compute 64-bit x 512-bit product + 512-bit addend           */
/*  ======================================================================  */

multacc512
        .PROC
        .CALLINFO
        .ENTRY
    fldd    0(pM),M                 ; multiplier double word
    ldo     ST_SZ(sp),sp            ; push stack

    fldd    0(pA),A0                ; multiplicand double word 0
    std     S1,SV1                  ; save s1

    fldd    16(pA),A2               ; multiplicand double word 2
    std     S3,SV3                  ; save s3

    fldd    32(pA),A4               ; multiplicand double word 4
    std     S5,SV5                  ; save s5

    fldd    48(pA),A6               ; multiplicand double word 6
    std     S7,SV7                  ; save s7


    std     S0,SV0                  ; save s0
    fldd    8(pA),A1                ; multiplicand double word 1
    xmpyu   MR,A0L,P0               ; A0 cross 32-bit word products
    xmpyu   ML,A0R,P2

    std     S2,SV2                  ; save s2
    fldd    24(pA),A3               ; multiplicand double word 3
    xmpyu   MR,A2L,P4               ; A2 cross 32-bit word products
    xmpyu   ML,A2R,P6

    std     S4,SV4                  ; save s4
    fldd    40(pA),A5               ; multiplicand double word 5

    std     S6,SV6                  ; save s6
    fldd    56(pA),A7               ; multiplicand double word 7


    fstd    P0,XF0                  ; MR * A0L
    xmpyu   MR,A0R,P0               ; A0 right 32-bit word product
    xmpyu   MR,A1L,P1               ; A1 cross 32-bit word product

    fstd    P2,XF2                  ; ML * A0R
    xmpyu   ML,A0L,P2               ; A0 left 32-bit word product
    xmpyu   ML,A1R,P3               ; A1 cross 32-bit word product

    fstd    P4,XF4                  ; MR * A2L
    xmpyu   MR,A2R,P4               ; A2 right 32-bit word product
    xmpyu   MR,A3L,P5               ; A3 cross 32-bit word product

    fstd    P6,XF6                  ; ML * A2R
    xmpyu   ML,A2L,P6               ; A2 parallel 32-bit word product
    xmpyu   ML,A3R,P7               ; A3 cross 32-bit word product


    ldd     XF0,S0                  ; MR * A0L
    fstd    P1,XF1                  ; MR * A1L

    ldd     XF2,S2                  ; ML * A0R
    fstd    P3,XF3                  ; ML * A1R

    ldd     XF4,S4                  ; MR * A2L
    fstd    P5,XF5                  ; MR * A3L
    xmpyu   MR,A1R,P1               ; A1 parallel 32-bit word products
    xmpyu   ML,A1L,P3

    ldd     XF6,S6                  ; ML * A2R
    fstd    P7,XF7                  ; ML * A3R
    xmpyu   MR,A3R,P5               ; A3 parallel 32-bit word products
    xmpyu   ML,A3L,P7


    fstd    P0,XF0                  ; MR * A0R
    ldd     XF1,S1                  ; MR * A1L
    nop
    add     S0,S2,T1                ; A0 cross product sum

    fstd    P2,XF2                  ; ML * A0L
    ldd     XF3,S3                  ; ML * A1R
    add,dc  zero,zero,S0            ; A0 cross product sum carry
    depd,z  T1,31,32,S2             ; A0 cross product sum << 32

    fstd    P4,XF4                  ; MR * A2R
    ldd     XF5,S5                  ; MR * A3L
    shrpd   S0,T1,32,S0             ; A0 carry | cross product sum >> 32
    add     S4,S6,T3                ; A2 cross product sum

    fstd    P6,XF6                  ; ML * A2L
    ldd     XF7,S7                  ; ML * A3R
    add,dc  zero,zero,S4            ; A2 cross product sum carry
    depd,z  T3,31,32,S6             ; A2 cross product sum << 32


    ldd     XF0,S8                  ; MR * A0R
    fstd    P1,XF1                  ; MR * A1R
    xmpyu   MR,A4L,P0               ; A4 cross 32-bit word product
    xmpyu   MR,A5L,P1               ; A5 cross 32-bit word product

    ldd     XF2,S10                 ; ML * A0L
    fstd    P3,XF3                  ; ML * A1L
    xmpyu   ML,A4R,P2               ; A4 cross 32-bit word product
    xmpyu   ML,A5R,P3               ; A5 cross 32-bit word product

    ldd     XF4,S12                 ; MR * A2R
    fstd    P5,XF5                  ; MR * A3L
    xmpyu   MR,A6L,P4               ; A6 cross 32-bit word product
    xmpyu   MR,A7L,P5               ; A7 cross 32-bit word product

    ldd     XF6,S14                 ; ML * A2L
    fstd    P7,XF7                  ; ML * A3L
    xmpyu   ML,A6R,P6               ; A6 cross 32-bit word product
    xmpyu   ML,A7R,P7               ; A7 cross 32-bit word product


    fstd    P0,XF0                  ; MR * A4L
    ldd     XF1,S9                  ; MR * A1R
    shrpd   S4,T3,32,S4             ; A2 carry | cross product sum >> 32
    add     S1,S3,T1                ; A1 cross product sum

    fstd    P2,XF2                  ; ML * A4R
    ldd     XF3,S11                 ; ML * A1L
    add,dc  zero,zero,S1            ; A1 cross product sum carry
    depd,z  T1,31,32,S3             ; A1 cross product sum << 32

    fstd    P4,XF4                  ; MR * A6L
    ldd     XF5,S13                 ; MR * A3R
    shrpd   S1,T1,32,S1             ; A1 carry | cross product sum >> 32
    add     S5,S7,T3                ; A3 cross product sum

    fstd    P6,XF6                  ; ML * A6R
    ldd     XF7,S15                 ; ML * A3L
    add,dc  zero,zero,S5            ; A3 cross product sum carry
    depd,z  T3,31,32,S7             ; A3 cross product sum << 32


    shrpd   S5,T3,32,S5             ; A3 carry | cross product sum >> 32
    add     S2,S8,S8                ; M * A0 right doubleword, P0 doubleword

    add,dc  S0,S10,S10              ; M * A0 left doubleword
    add     S3,S9,S9                ; M * A1 right doubleword

    add,dc  S1,S11,S11              ; M * A1 left doubleword
    add     S6,S12,S12              ; M * A2 right doubleword


    ldd     24(pR),S3               ; Addend word 3
    fstd    P1,XF1                  ; MR * A5L
    add,dc  S4,S14,S14              ; M * A2 left doubleword
    xmpyu   MR,A5R,P1               ; A5 right 32-bit word product

    ldd     8(pR),S1                ; Addend word 1
    fstd    P3,XF3                  ; ML * A5R
    add     S7,S13,S13              ; M * A3 right doubleword
    xmpyu   ML,A5L,P3               ; A5 left 32-bit word product

    ldd     0(pR),S7                ; Addend word 0
    fstd    P5,XF5                  ; MR * A7L
    add,dc  S5,S15,S15              ; M * A3 left doubleword
    xmpyu   MR,A7R,P5               ; A7 right 32-bit word product

    ldd     16(pR),S5               ; Addend word 2
    fstd    P7,XF7                  ; ML * A7R
    add     S10,S9,S9               ; P1 doubleword
    xmpyu   ML,A7L,P7               ; A7 left 32-bit word products


    ldd     XF0,S0                  ; MR * A4L
    fstd    P1,XF9                  ; MR * A5R
    add,dc  S11,S12,S12             ; P2 doubleword
    xmpyu   MR,A4R,P0               ; A4 right 32-bit word product

    ldd     XF2,S2                  ; ML * A4R
    fstd    P3,XF11                 ; ML * A5L
    add,dc  S14,S13,S13             ; P3 doubleword
    xmpyu   ML,A4L,P2               ; A4 left 32-bit word product

    ldd     XF6,S6                  ; ML * A6R
    fstd    P5,XF13                 ; MR * A7R
    add,dc  zero,S15,T2             ; P4 partial doubleword
    xmpyu   MR,A6R,P4               ; A6 right 32-bit word product

    ldd     XF4,S4                  ; MR * A6L
    fstd    P7,XF15                 ; ML * A7L
    add     S7,S8,S8                ; R0 + P0, new R0 doubleword
    xmpyu   ML,A6L,P6               ; A6 left 32-bit word product


    fstd    P0,XF0                  ; MR * A4R
    ldd     XF7,S7                  ; ML * A7R
    add,dc  S1,S9,S9                ; c + R1 + P1, new R1 doubleword

    fstd    P2,XF2                  ; ML * A4L
    ldd     XF1,S1                  ; MR * A5L
    add,dc  S5,S12,S12              ; c + R2 + P2, new R2 doubleword

    fstd    P4,XF4                  ; MR * A6R
    ldd     XF5,S5                  ; MR * A7L
    add,dc  S3,S13,S13              ; c + R3 + P3, new R3 doubleword

    fstd    P6,XF6                  ; ML * A6L
    ldd     XF3,S3                  ; ML * A5R
    add,dc  zero,T2,T2              ; c + partial P4
    add     S0,S2,T1                ; A4 cross product sum


    std     S8,0(pR)                ; save R0
    add,dc  zero,zero,S0            ; A4 cross product sum carry
    depd,z  T1,31,32,S2             ; A4 cross product sum << 32

    std     S9,8(pR)                ; save R1
    shrpd   S0,T1,32,S0             ; A4 carry | cross product sum >> 32
    add     S4,S6,T3                ; A6 cross product sum

    std     S12,16(pR)              ; save R2
    add,dc  zero,zero,S4            ; A6 cross product sum carry
    depd,z  T3,31,32,S6             ; A6 cross product sum << 32


    std     S13,24(pR)              ; save R3
    shrpd   S4,T3,32,S4             ; A6 carry | cross product sum >> 32
    add     S1,S3,T1                ; A5 cross product sum

    ldd     XF0,S8                  ; MR * A4R
    add,dc  zero,zero,S1            ; A5 cross product sum carry
    depd,z  T1,31,32,S3             ; A5 cross product sum << 32

    ldd     XF2,S10                 ; ML * A4L
    ldd     XF9,S9                  ; MR * A5R
    shrpd   S1,T1,32,S1             ; A5 carry | cross product sum >> 32
    add     S5,S7,T3                ; A7 cross product sum

    ldd     XF4,S12                 ; MR * A6R
    ldd     XF11,S11                ; ML * A5L
    add,dc  zero,zero,S5            ; A7 cross product sum carry
    depd,z  T3,31,32,S7             ; A7 cross product sum << 32

    ldd     XF6,S14                 ; ML * A6L
    ldd     XF13,S13                ; MR * A7R
    shrpd   S5,T3,32,S5             ; A7 carry | cross product sum >> 32
    add     S2,S8,S8                ; M * A4 right doubleword


    ldd     XF15,S15                ; ML * A7L
    add,dc  S0,S10,S10              ; M * A4 left doubleword
    add     S3,S9,S9                ; M * A5 right doubleword

    add,dc  S1,S11,S11              ; M * A5 left doubleword
    add     S6,S12,S12              ; M * A6 right doubleword

    ldd     32(pR),S0               ; Addend word 4
    ldd     40(pR),S1               ; Addend word 5
    add,dc  S4,S14,S14              ; M * A6 left doubleword
    add     S7,S13,S13              ; M * A7 right doubleword

    ldd     48(pR),S2               ; Addend word 6
    ldd     56(pR),S3               ; Addend word 7
    add,dc  S5,S15,S15              ; M * A7 left doubleword
    add     S8,T2,S8                ; P4 doubleword

    ldd     64(pR),S4               ; Addend word 8
    ldd     SV5,s5                  ; restore s5
    add,dc  S10,S9,S9               ; P5 doubleword
    add,dc  S11,S12,S12             ; P6 doubleword


    ldd     SV6,s6                  ; restore s6
    ldd     SV7,s7                  ; restore s7
    add,dc  S14,S13,S13             ; P7 doubleword
    add,dc  zero,S15,S15            ; P8 doubleword

    add     S0,S8,S8                ; new R4 doubleword

    ldd     SV0,s0                  ; restore s0
    std     S8,32(pR)               ; save R4
    add,dc  S1,S9,S9                ; new R5 doubleword

    ldd     SV1,s1                  ; restore s1
    std     S9,40(pR)               ; save R5
    add,dc  S2,S12,S12              ; new R6 doubleword

    ldd     SV2,s2                  ; restore s2
    std     S12,48(pR)              ; save R6
    add,dc  S3,S13,S13              ; new R7 doubleword

    ldd     SV3,s3                  ; restore s3
    std     S13,56(pR)              ; save R7
    add,dc  S4,S15,S15              ; new R8 doubleword

    ldd     SV4,s4                  ; restore s4
    std     S15,64(pR)              ; save result[8]
    add,dc  zero,zero,v0            ; return carry from R8

    CMPIB,*= 0,v0,$L0               ; if no overflow, exit
    LDO     8(pR),pR

$FINAL1                             ; Final carry propagation
    LDD     64(pR),v0
    LDO     8(pR),pR
    ADDI    1,v0,v0
    CMPIB,*= 0,v0,$FINAL1           ; Keep looping if there is a carry.
    STD     v0,56(pR)
$L0
    bv      zero(rp)                ; -> caller
    ldo     -ST_SZ(sp),sp           ; pop stack

/*  ======================================================================  */
/*      end of module                                                       */
/*  ======================================================================  */


        bve (rp)
        .EXIT
        nop
                .PROCEND
                .SPACE         $TEXT$
                .SUBSPA        $CODE$
                .EXPORT        multacc512,ENTRY

        .end
