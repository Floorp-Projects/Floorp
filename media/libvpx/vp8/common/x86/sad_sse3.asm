;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

%include "vpx_ports/x86_abi_support.asm"

%macro STACK_FRAME_CREATE_X3 0
%if ABI_IS_32BIT
  %define     src_ptr       rsi
  %define     src_stride    rax
  %define     ref_ptr       rdi
  %define     ref_stride    rdx
  %define     end_ptr       rcx
  %define     ret_var       rbx
  %define     result_ptr    arg(4)
  %define     max_sad       arg(4)
  %define     height        dword ptr arg(4)
    push        rbp
    mov         rbp,        rsp
    push        rsi
    push        rdi
    push        rbx

    mov         rsi,        arg(0)              ; src_ptr
    mov         rdi,        arg(2)              ; ref_ptr

    movsxd      rax,        dword ptr arg(1)    ; src_stride
    movsxd      rdx,        dword ptr arg(3)    ; ref_stride
%else
  %if LIBVPX_YASM_WIN64
    SAVE_XMM 7, u
    %define     src_ptr     rcx
    %define     src_stride  rdx
    %define     ref_ptr     r8
    %define     ref_stride  r9
    %define     end_ptr     r10
    %define     ret_var     r11
    %define     result_ptr  [rsp+xmm_stack_space+8+4*8]
    %define     max_sad     [rsp+xmm_stack_space+8+4*8]
    %define     height      dword ptr [rsp+xmm_stack_space+8+4*8]
  %else
    %define     src_ptr     rdi
    %define     src_stride  rsi
    %define     ref_ptr     rdx
    %define     ref_stride  rcx
    %define     end_ptr     r9
    %define     ret_var     r10
    %define     result_ptr  r8
    %define     max_sad     r8
    %define     height      r8
  %endif
%endif

%endmacro

%macro STACK_FRAME_DESTROY_X3 0
  %define     src_ptr
  %define     src_stride
  %define     ref_ptr
  %define     ref_stride
  %define     end_ptr
  %define     ret_var
  %define     result_ptr
  %define     max_sad
  %define     height

%if ABI_IS_32BIT
    pop         rbx
    pop         rdi
    pop         rsi
    pop         rbp
%else
  %if LIBVPX_YASM_WIN64
    RESTORE_XMM
  %endif
%endif
    ret
%endmacro

%macro STACK_FRAME_CREATE_X4 0
%if ABI_IS_32BIT
  %define     src_ptr       rsi
  %define     src_stride    rax
  %define     r0_ptr        rcx
  %define     r1_ptr        rdx
  %define     r2_ptr        rbx
  %define     r3_ptr        rdi
  %define     ref_stride    rbp
  %define     result_ptr    arg(4)
    push        rbp
    mov         rbp,        rsp
    push        rsi
    push        rdi
    push        rbx

    push        rbp
    mov         rdi,        arg(2)              ; ref_ptr_base

    LOAD_X4_ADDRESSES rdi, rcx, rdx, rax, rdi

    mov         rsi,        arg(0)              ; src_ptr

    movsxd      rbx,        dword ptr arg(1)    ; src_stride
    movsxd      rbp,        dword ptr arg(3)    ; ref_stride

    xchg        rbx,        rax
%else
  %if LIBVPX_YASM_WIN64
    SAVE_XMM 7, u
    %define     src_ptr     rcx
    %define     src_stride  rdx
    %define     r0_ptr      rsi
    %define     r1_ptr      r10
    %define     r2_ptr      r11
    %define     r3_ptr      r8
    %define     ref_stride  r9
    %define     result_ptr  [rsp+xmm_stack_space+16+4*8]
    push        rsi

    LOAD_X4_ADDRESSES r8, r0_ptr, r1_ptr, r2_ptr, r3_ptr
  %else
    %define     src_ptr     rdi
    %define     src_stride  rsi
    %define     r0_ptr      r9
    %define     r1_ptr      r10
    %define     r2_ptr      r11
    %define     r3_ptr      rdx
    %define     ref_stride  rcx
    %define     result_ptr  r8

    LOAD_X4_ADDRESSES rdx, r0_ptr, r1_ptr, r2_ptr, r3_ptr

  %endif
%endif
%endmacro

%macro STACK_FRAME_DESTROY_X4 0
  %define     src_ptr
  %define     src_stride
  %define     r0_ptr
  %define     r1_ptr
  %define     r2_ptr
  %define     r3_ptr
  %define     ref_stride
  %define     result_ptr

%if ABI_IS_32BIT
    pop         rbx
    pop         rdi
    pop         rsi
    pop         rbp
%else
  %if LIBVPX_YASM_WIN64
    pop         rsi
    RESTORE_XMM
  %endif
%endif
    ret
%endmacro

%macro PROCESS_16X2X3 5
%if %1==0
        movdqa          xmm0,       XMMWORD PTR [%2]
        lddqu           xmm5,       XMMWORD PTR [%3]
        lddqu           xmm6,       XMMWORD PTR [%3+1]
        lddqu           xmm7,       XMMWORD PTR [%3+2]

        psadbw          xmm5,       xmm0
        psadbw          xmm6,       xmm0
        psadbw          xmm7,       xmm0
%else
        movdqa          xmm0,       XMMWORD PTR [%2]
        lddqu           xmm1,       XMMWORD PTR [%3]
        lddqu           xmm2,       XMMWORD PTR [%3+1]
        lddqu           xmm3,       XMMWORD PTR [%3+2]

        psadbw          xmm1,       xmm0
        psadbw          xmm2,       xmm0
        psadbw          xmm3,       xmm0

        paddw           xmm5,       xmm1
        paddw           xmm6,       xmm2
        paddw           xmm7,       xmm3
%endif
        movdqa          xmm0,       XMMWORD PTR [%2+%4]
        lddqu           xmm1,       XMMWORD PTR [%3+%5]
        lddqu           xmm2,       XMMWORD PTR [%3+%5+1]
        lddqu           xmm3,       XMMWORD PTR [%3+%5+2]

%if %1==0 || %1==1
        lea             %2,         [%2+%4*2]
        lea             %3,         [%3+%5*2]
%endif

        psadbw          xmm1,       xmm0
        psadbw          xmm2,       xmm0
        psadbw          xmm3,       xmm0

        paddw           xmm5,       xmm1
        paddw           xmm6,       xmm2
        paddw           xmm7,       xmm3
%endmacro

%macro PROCESS_8X2X3 5
%if %1==0
        movq            mm0,       QWORD PTR [%2]
        movq            mm5,       QWORD PTR [%3]
        movq            mm6,       QWORD PTR [%3+1]
        movq            mm7,       QWORD PTR [%3+2]

        psadbw          mm5,       mm0
        psadbw          mm6,       mm0
        psadbw          mm7,       mm0
%else
        movq            mm0,       QWORD PTR [%2]
        movq            mm1,       QWORD PTR [%3]
        movq            mm2,       QWORD PTR [%3+1]
        movq            mm3,       QWORD PTR [%3+2]

        psadbw          mm1,       mm0
        psadbw          mm2,       mm0
        psadbw          mm3,       mm0

        paddw           mm5,       mm1
        paddw           mm6,       mm2
        paddw           mm7,       mm3
%endif
        movq            mm0,       QWORD PTR [%2+%4]
        movq            mm1,       QWORD PTR [%3+%5]
        movq            mm2,       QWORD PTR [%3+%5+1]
        movq            mm3,       QWORD PTR [%3+%5+2]

%if %1==0 || %1==1
        lea             %2,        [%2+%4*2]
        lea             %3,        [%3+%5*2]
%endif

        psadbw          mm1,       mm0
        psadbw          mm2,       mm0
        psadbw          mm3,       mm0

        paddw           mm5,       mm1
        paddw           mm6,       mm2
        paddw           mm7,       mm3
%endmacro

%macro LOAD_X4_ADDRESSES 5
        mov             %2,         [%1+REG_SZ_BYTES*0]
        mov             %3,         [%1+REG_SZ_BYTES*1]

        mov             %4,         [%1+REG_SZ_BYTES*2]
        mov             %5,         [%1+REG_SZ_BYTES*3]
%endmacro

%macro PROCESS_16X2X4 8
%if %1==0
        movdqa          xmm0,       XMMWORD PTR [%2]
        lddqu           xmm4,       XMMWORD PTR [%3]
        lddqu           xmm5,       XMMWORD PTR [%4]
        lddqu           xmm6,       XMMWORD PTR [%5]
        lddqu           xmm7,       XMMWORD PTR [%6]

        psadbw          xmm4,       xmm0
        psadbw          xmm5,       xmm0
        psadbw          xmm6,       xmm0
        psadbw          xmm7,       xmm0
%else
        movdqa          xmm0,       XMMWORD PTR [%2]
        lddqu           xmm1,       XMMWORD PTR [%3]
        lddqu           xmm2,       XMMWORD PTR [%4]
        lddqu           xmm3,       XMMWORD PTR [%5]

        psadbw          xmm1,       xmm0
        psadbw          xmm2,       xmm0
        psadbw          xmm3,       xmm0

        paddw           xmm4,       xmm1
        lddqu           xmm1,       XMMWORD PTR [%6]
        paddw           xmm5,       xmm2
        paddw           xmm6,       xmm3

        psadbw          xmm1,       xmm0
        paddw           xmm7,       xmm1
%endif
        movdqa          xmm0,       XMMWORD PTR [%2+%7]
        lddqu           xmm1,       XMMWORD PTR [%3+%8]
        lddqu           xmm2,       XMMWORD PTR [%4+%8]
        lddqu           xmm3,       XMMWORD PTR [%5+%8]

        psadbw          xmm1,       xmm0
        psadbw          xmm2,       xmm0
        psadbw          xmm3,       xmm0

        paddw           xmm4,       xmm1
        lddqu           xmm1,       XMMWORD PTR [%6+%8]
        paddw           xmm5,       xmm2
        paddw           xmm6,       xmm3

%if %1==0 || %1==1
        lea             %2,         [%2+%7*2]
        lea             %3,         [%3+%8*2]

        lea             %4,         [%4+%8*2]
        lea             %5,         [%5+%8*2]

        lea             %6,         [%6+%8*2]
%endif
        psadbw          xmm1,       xmm0
        paddw           xmm7,       xmm1

%endmacro

%macro PROCESS_8X2X4 8
%if %1==0
        movq            mm0,        QWORD PTR [%2]
        movq            mm4,        QWORD PTR [%3]
        movq            mm5,        QWORD PTR [%4]
        movq            mm6,        QWORD PTR [%5]
        movq            mm7,        QWORD PTR [%6]

        psadbw          mm4,        mm0
        psadbw          mm5,        mm0
        psadbw          mm6,        mm0
        psadbw          mm7,        mm0
%else
        movq            mm0,        QWORD PTR [%2]
        movq            mm1,        QWORD PTR [%3]
        movq            mm2,        QWORD PTR [%4]
        movq            mm3,        QWORD PTR [%5]

        psadbw          mm1,        mm0
        psadbw          mm2,        mm0
        psadbw          mm3,        mm0

        paddw           mm4,        mm1
        movq            mm1,        QWORD PTR [%6]
        paddw           mm5,        mm2
        paddw           mm6,        mm3

        psadbw          mm1,        mm0
        paddw           mm7,        mm1
%endif
        movq            mm0,        QWORD PTR [%2+%7]
        movq            mm1,        QWORD PTR [%3+%8]
        movq            mm2,        QWORD PTR [%4+%8]
        movq            mm3,        QWORD PTR [%5+%8]

        psadbw          mm1,        mm0
        psadbw          mm2,        mm0
        psadbw          mm3,        mm0

        paddw           mm4,        mm1
        movq            mm1,        QWORD PTR [%6+%8]
        paddw           mm5,        mm2
        paddw           mm6,        mm3

%if %1==0 || %1==1
        lea             %2,         [%2+%7*2]
        lea             %3,         [%3+%8*2]

        lea             %4,         [%4+%8*2]
        lea             %5,         [%5+%8*2]

        lea             %6,         [%6+%8*2]
%endif
        psadbw          mm1,        mm0
        paddw           mm7,        mm1

%endmacro

;void int vp8_sad16x16x3_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad16x16x3_sse3) PRIVATE
sym(vp8_sad16x16x3_sse3):

    STACK_FRAME_CREATE_X3

        PROCESS_16X2X3 0, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 2, src_ptr, ref_ptr, src_stride, ref_stride

        mov             rcx,        result_ptr

        movq            xmm0,       xmm5
        psrldq          xmm5,       8

        paddw           xmm0,       xmm5
        movd            [rcx],      xmm0
;-
        movq            xmm0,       xmm6
        psrldq          xmm6,       8

        paddw           xmm0,       xmm6
        movd            [rcx+4],    xmm0
;-
        movq            xmm0,       xmm7
        psrldq          xmm7,       8

        paddw           xmm0,       xmm7
        movd            [rcx+8],    xmm0

    STACK_FRAME_DESTROY_X3

;void int vp8_sad16x8x3_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad16x8x3_sse3) PRIVATE
sym(vp8_sad16x8x3_sse3):

    STACK_FRAME_CREATE_X3

        PROCESS_16X2X3 0, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_16X2X3 2, src_ptr, ref_ptr, src_stride, ref_stride

        mov             rcx,        result_ptr

        movq            xmm0,       xmm5
        psrldq          xmm5,       8

        paddw           xmm0,       xmm5
        movd            [rcx],      xmm0
;-
        movq            xmm0,       xmm6
        psrldq          xmm6,       8

        paddw           xmm0,       xmm6
        movd            [rcx+4],    xmm0
;-
        movq            xmm0,       xmm7
        psrldq          xmm7,       8

        paddw           xmm0,       xmm7
        movd            [rcx+8],    xmm0

    STACK_FRAME_DESTROY_X3

;void int vp8_sad8x16x3_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad8x16x3_sse3) PRIVATE
sym(vp8_sad8x16x3_sse3):

    STACK_FRAME_CREATE_X3

        PROCESS_8X2X3 0, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 2, src_ptr, ref_ptr, src_stride, ref_stride

        mov             rcx,        result_ptr

        punpckldq       mm5,        mm6

        movq            [rcx],      mm5
        movd            [rcx+8],    mm7

    STACK_FRAME_DESTROY_X3

;void int vp8_sad8x8x3_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad8x8x3_sse3) PRIVATE
sym(vp8_sad8x8x3_sse3):

    STACK_FRAME_CREATE_X3

        PROCESS_8X2X3 0, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 1, src_ptr, ref_ptr, src_stride, ref_stride
        PROCESS_8X2X3 2, src_ptr, ref_ptr, src_stride, ref_stride

        mov             rcx,        result_ptr

        punpckldq       mm5,        mm6

        movq            [rcx],      mm5
        movd            [rcx+8],    mm7

    STACK_FRAME_DESTROY_X3

;void int vp8_sad4x4x3_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad4x4x3_sse3) PRIVATE
sym(vp8_sad4x4x3_sse3):

    STACK_FRAME_CREATE_X3

        movd            mm0,        DWORD PTR [src_ptr]
        movd            mm1,        DWORD PTR [ref_ptr]

        movd            mm2,        DWORD PTR [src_ptr+src_stride]
        movd            mm3,        DWORD PTR [ref_ptr+ref_stride]

        punpcklbw       mm0,        mm2
        punpcklbw       mm1,        mm3

        movd            mm4,        DWORD PTR [ref_ptr+1]
        movd            mm5,        DWORD PTR [ref_ptr+2]

        movd            mm2,        DWORD PTR [ref_ptr+ref_stride+1]
        movd            mm3,        DWORD PTR [ref_ptr+ref_stride+2]

        psadbw          mm1,        mm0

        punpcklbw       mm4,        mm2
        punpcklbw       mm5,        mm3

        psadbw          mm4,        mm0
        psadbw          mm5,        mm0

        lea             src_ptr,    [src_ptr+src_stride*2]
        lea             ref_ptr,    [ref_ptr+ref_stride*2]

        movd            mm0,        DWORD PTR [src_ptr]
        movd            mm2,        DWORD PTR [ref_ptr]

        movd            mm3,        DWORD PTR [src_ptr+src_stride]
        movd            mm6,        DWORD PTR [ref_ptr+ref_stride]

        punpcklbw       mm0,        mm3
        punpcklbw       mm2,        mm6

        movd            mm3,        DWORD PTR [ref_ptr+1]
        movd            mm7,        DWORD PTR [ref_ptr+2]

        psadbw          mm2,        mm0

        paddw           mm1,        mm2

        movd            mm2,        DWORD PTR [ref_ptr+ref_stride+1]
        movd            mm6,        DWORD PTR [ref_ptr+ref_stride+2]

        punpcklbw       mm3,        mm2
        punpcklbw       mm7,        mm6

        psadbw          mm3,        mm0
        psadbw          mm7,        mm0

        paddw           mm3,        mm4
        paddw           mm7,        mm5

        mov             rcx,        result_ptr

        punpckldq       mm1,        mm3

        movq            [rcx],      mm1
        movd            [rcx+8],    mm7

    STACK_FRAME_DESTROY_X3

;unsigned int vp8_sad16x16_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  max_sad)
;%define lddqu movdqu
global sym(vp8_sad16x16_sse3) PRIVATE
sym(vp8_sad16x16_sse3):

    STACK_FRAME_CREATE_X3

        mov             end_ptr,    4
        pxor            xmm7,        xmm7

.vp8_sad16x16_sse3_loop:
        movdqa          xmm0,       XMMWORD PTR [src_ptr]
        movdqu          xmm1,       XMMWORD PTR [ref_ptr]
        movdqa          xmm2,       XMMWORD PTR [src_ptr+src_stride]
        movdqu          xmm3,       XMMWORD PTR [ref_ptr+ref_stride]

        lea             src_ptr,    [src_ptr+src_stride*2]
        lea             ref_ptr,    [ref_ptr+ref_stride*2]

        movdqa          xmm4,       XMMWORD PTR [src_ptr]
        movdqu          xmm5,       XMMWORD PTR [ref_ptr]
        movdqa          xmm6,       XMMWORD PTR [src_ptr+src_stride]

        psadbw          xmm0,       xmm1

        movdqu          xmm1,       XMMWORD PTR [ref_ptr+ref_stride]

        psadbw          xmm2,       xmm3
        psadbw          xmm4,       xmm5
        psadbw          xmm6,       xmm1

        lea             src_ptr,    [src_ptr+src_stride*2]
        lea             ref_ptr,    [ref_ptr+ref_stride*2]

        paddw           xmm7,        xmm0
        paddw           xmm7,        xmm2
        paddw           xmm7,        xmm4
        paddw           xmm7,        xmm6

        sub             end_ptr,     1
        jne             .vp8_sad16x16_sse3_loop

        movq            xmm0,       xmm7
        psrldq          xmm7,       8
        paddw           xmm0,       xmm7
        movq            rax,        xmm0

    STACK_FRAME_DESTROY_X3

;void vp8_copy32xn_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *dst_ptr,
;    int  dst_stride,
;    int height);
global sym(vp8_copy32xn_sse3) PRIVATE
sym(vp8_copy32xn_sse3):

    STACK_FRAME_CREATE_X3

.block_copy_sse3_loopx4:
        lea             end_ptr,    [src_ptr+src_stride*2]

        movdqu          xmm0,       XMMWORD PTR [src_ptr]
        movdqu          xmm1,       XMMWORD PTR [src_ptr + 16]
        movdqu          xmm2,       XMMWORD PTR [src_ptr + src_stride]
        movdqu          xmm3,       XMMWORD PTR [src_ptr + src_stride + 16]
        movdqu          xmm4,       XMMWORD PTR [end_ptr]
        movdqu          xmm5,       XMMWORD PTR [end_ptr + 16]
        movdqu          xmm6,       XMMWORD PTR [end_ptr + src_stride]
        movdqu          xmm7,       XMMWORD PTR [end_ptr + src_stride + 16]

        lea             src_ptr,    [src_ptr+src_stride*4]

        lea             end_ptr,    [ref_ptr+ref_stride*2]

        movdqa          XMMWORD PTR [ref_ptr], xmm0
        movdqa          XMMWORD PTR [ref_ptr + 16], xmm1
        movdqa          XMMWORD PTR [ref_ptr + ref_stride], xmm2
        movdqa          XMMWORD PTR [ref_ptr + ref_stride + 16], xmm3
        movdqa          XMMWORD PTR [end_ptr], xmm4
        movdqa          XMMWORD PTR [end_ptr + 16], xmm5
        movdqa          XMMWORD PTR [end_ptr + ref_stride], xmm6
        movdqa          XMMWORD PTR [end_ptr + ref_stride + 16], xmm7

        lea             ref_ptr,    [ref_ptr+ref_stride*4]

        sub             height,     4
        cmp             height,     4
        jge             .block_copy_sse3_loopx4

        ;Check to see if there is more rows need to be copied.
        cmp             height, 0
        je              .copy_is_done

.block_copy_sse3_loop:
        movdqu          xmm0,       XMMWORD PTR [src_ptr]
        movdqu          xmm1,       XMMWORD PTR [src_ptr + 16]
        lea             src_ptr,    [src_ptr+src_stride]

        movdqa          XMMWORD PTR [ref_ptr], xmm0
        movdqa          XMMWORD PTR [ref_ptr + 16], xmm1
        lea             ref_ptr,    [ref_ptr+ref_stride]

        sub             height,     1
        jne             .block_copy_sse3_loop

.copy_is_done:
    STACK_FRAME_DESTROY_X3

;void vp8_sad16x16x4d_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr_base,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad16x16x4d_sse3) PRIVATE
sym(vp8_sad16x16x4d_sse3):

    STACK_FRAME_CREATE_X4

        PROCESS_16X2X4 0, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 2, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride

%if ABI_IS_32BIT
        pop             rbp
%endif
        mov             rcx,        result_ptr

        movq            xmm0,       xmm4
        psrldq          xmm4,       8

        paddw           xmm0,       xmm4
        movd            [rcx],      xmm0
;-
        movq            xmm0,       xmm5
        psrldq          xmm5,       8

        paddw           xmm0,       xmm5
        movd            [rcx+4],    xmm0
;-
        movq            xmm0,       xmm6
        psrldq          xmm6,       8

        paddw           xmm0,       xmm6
        movd            [rcx+8],    xmm0
;-
        movq            xmm0,       xmm7
        psrldq          xmm7,       8

        paddw           xmm0,       xmm7
        movd            [rcx+12],   xmm0

    STACK_FRAME_DESTROY_X4

;void vp8_sad16x8x4d_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr_base,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad16x8x4d_sse3) PRIVATE
sym(vp8_sad16x8x4d_sse3):

    STACK_FRAME_CREATE_X4

        PROCESS_16X2X4 0, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_16X2X4 2, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride

%if ABI_IS_32BIT
        pop             rbp
%endif
        mov             rcx,        result_ptr

        movq            xmm0,       xmm4
        psrldq          xmm4,       8

        paddw           xmm0,       xmm4
        movd            [rcx],      xmm0
;-
        movq            xmm0,       xmm5
        psrldq          xmm5,       8

        paddw           xmm0,       xmm5
        movd            [rcx+4],    xmm0
;-
        movq            xmm0,       xmm6
        psrldq          xmm6,       8

        paddw           xmm0,       xmm6
        movd            [rcx+8],    xmm0
;-
        movq            xmm0,       xmm7
        psrldq          xmm7,       8

        paddw           xmm0,       xmm7
        movd            [rcx+12],   xmm0

    STACK_FRAME_DESTROY_X4

;void int vp8_sad8x16x4d_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad8x16x4d_sse3) PRIVATE
sym(vp8_sad8x16x4d_sse3):

    STACK_FRAME_CREATE_X4

        PROCESS_8X2X4 0, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 2, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride

%if ABI_IS_32BIT
        pop             rbp
%endif
        mov             rcx,        result_ptr

        punpckldq       mm4,        mm5
        punpckldq       mm6,        mm7

        movq            [rcx],      mm4
        movq            [rcx+8],    mm6

    STACK_FRAME_DESTROY_X4

;void int vp8_sad8x8x4d_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad8x8x4d_sse3) PRIVATE
sym(vp8_sad8x8x4d_sse3):

    STACK_FRAME_CREATE_X4

        PROCESS_8X2X4 0, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 1, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride
        PROCESS_8X2X4 2, src_ptr, r0_ptr, r1_ptr, r2_ptr, r3_ptr, src_stride, ref_stride

%if ABI_IS_32BIT
        pop             rbp
%endif
        mov             rcx,        result_ptr

        punpckldq       mm4,        mm5
        punpckldq       mm6,        mm7

        movq            [rcx],      mm4
        movq            [rcx+8],    mm6

    STACK_FRAME_DESTROY_X4

;void int vp8_sad4x4x4d_sse3(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride,
;    int  *results)
global sym(vp8_sad4x4x4d_sse3) PRIVATE
sym(vp8_sad4x4x4d_sse3):

    STACK_FRAME_CREATE_X4

        movd            mm0,        DWORD PTR [src_ptr]
        movd            mm1,        DWORD PTR [r0_ptr]

        movd            mm2,        DWORD PTR [src_ptr+src_stride]
        movd            mm3,        DWORD PTR [r0_ptr+ref_stride]

        punpcklbw       mm0,        mm2
        punpcklbw       mm1,        mm3

        movd            mm4,        DWORD PTR [r1_ptr]
        movd            mm5,        DWORD PTR [r2_ptr]

        movd            mm6,        DWORD PTR [r3_ptr]
        movd            mm2,        DWORD PTR [r1_ptr+ref_stride]

        movd            mm3,        DWORD PTR [r2_ptr+ref_stride]
        movd            mm7,        DWORD PTR [r3_ptr+ref_stride]

        psadbw          mm1,        mm0

        punpcklbw       mm4,        mm2
        punpcklbw       mm5,        mm3

        punpcklbw       mm6,        mm7
        psadbw          mm4,        mm0

        psadbw          mm5,        mm0
        psadbw          mm6,        mm0



        lea             src_ptr,    [src_ptr+src_stride*2]
        lea             r0_ptr,     [r0_ptr+ref_stride*2]

        lea             r1_ptr,     [r1_ptr+ref_stride*2]
        lea             r2_ptr,     [r2_ptr+ref_stride*2]

        lea             r3_ptr,     [r3_ptr+ref_stride*2]

        movd            mm0,        DWORD PTR [src_ptr]
        movd            mm2,        DWORD PTR [r0_ptr]

        movd            mm3,        DWORD PTR [src_ptr+src_stride]
        movd            mm7,        DWORD PTR [r0_ptr+ref_stride]

        punpcklbw       mm0,        mm3
        punpcklbw       mm2,        mm7

        movd            mm3,        DWORD PTR [r1_ptr]
        movd            mm7,        DWORD PTR [r2_ptr]

        psadbw          mm2,        mm0
%if ABI_IS_32BIT
        mov             rax,        rbp

        pop             rbp
%define     ref_stride    rax
%endif
        mov             rsi,        result_ptr

        paddw           mm1,        mm2
        movd            [rsi],      mm1

        movd            mm2,        DWORD PTR [r1_ptr+ref_stride]
        movd            mm1,        DWORD PTR [r2_ptr+ref_stride]

        punpcklbw       mm3,        mm2
        punpcklbw       mm7,        mm1

        psadbw          mm3,        mm0
        psadbw          mm7,        mm0

        movd            mm2,        DWORD PTR [r3_ptr]
        movd            mm1,        DWORD PTR [r3_ptr+ref_stride]

        paddw           mm3,        mm4
        paddw           mm7,        mm5

        movd            [rsi+4],    mm3
        punpcklbw       mm2,        mm1

        movd            [rsi+8],    mm7
        psadbw          mm2,        mm0

        paddw           mm2,        mm6
        movd            [rsi+12],   mm2


    STACK_FRAME_DESTROY_X4

