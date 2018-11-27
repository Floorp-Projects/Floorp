; Copyright © 2018, VideoLAN and dav1d authors
; Copyright © 2018, Two Orioles, LLC
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice,
;    this list of conditions and the following disclaimer in the documentation
;    and/or other materials provided with the distribution.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
; ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
; (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
; ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%include "config.asm"
%include "ext/x86/x86inc.asm"

%if ARCH_X86_64

SECTION_RODATA 32
pd_04512763: dd 0, 4, 5, 1, 2, 7, 6, 3
div_table: dd 840, 420, 280, 210, 168, 140, 120, 105
           dd 420, 210, 140, 105
pd_04261537: dd 0, 4, 2, 6, 1, 5, 3, 7
shufw_6543210x: db 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1, 14, 15
shufw_210xxxxx: db 4, 5, 2, 3, 0, 1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
pw_128: times 2 dw 128
pw_2048: times 2 dw 2048
tap_table: dw 4, 2, 3, 3, 2, 1
           db -1 * 16 + 1, -2 * 16 + 2
           db  0 * 16 + 1, -1 * 16 + 2
           db  0 * 16 + 1,  0 * 16 + 2
           db  0 * 16 + 1,  1 * 16 + 2
           db  1 * 16 + 1,  2 * 16 + 2
           db  1 * 16 + 0,  2 * 16 + 1
           db  1 * 16 + 0,  2 * 16 + 0
           db  1 * 16 + 0,  2 * 16 - 1
           ; the last 6 are repeats of the first 6 so we don't need to & 7
           db -1 * 16 + 1, -2 * 16 + 2
           db  0 * 16 + 1, -1 * 16 + 2
           db  0 * 16 + 1,  0 * 16 + 2
           db  0 * 16 + 1,  1 * 16 + 2
           db  1 * 16 + 1,  2 * 16 + 2
           db  1 * 16 + 0,  2 * 16 + 1

SECTION .text

%macro ACCUMULATE_TAP 6 ; tap_offset, shift, strength, mul_tap, w, stride
    ; load p0/p1
    movsx         offq, byte [dirq+kq+%1]       ; off1
%if %5 == 4
    movq           xm5, [stkq+offq*2+%6*0]      ; p0
    movq           xm6, [stkq+offq*2+%6*2]
    movhps         xm5, [stkq+offq*2+%6*1]
    movhps         xm6, [stkq+offq*2+%6*3]
    vinserti128     m5, xm6, 1
%else
    movu           xm5, [stkq+offq*2+%6*0]      ; p0
    vinserti128     m5, [stkq+offq*2+%6*1], 1
%endif
    neg           offq                          ; -off1
%if %5 == 4
    movq           xm6, [stkq+offq*2+%6*0]      ; p1
    movq           xm9, [stkq+offq*2+%6*2]
    movhps         xm6, [stkq+offq*2+%6*1]
    movhps         xm9, [stkq+offq*2+%6*3]
    vinserti128     m6, xm9, 1
%else
    movu           xm6, [stkq+offq*2+%6*0]      ; p1
    vinserti128     m6, [stkq+offq*2+%6*1], 1
%endif
    pcmpeqw         m9, m14, m5
    pcmpeqw        m10, m14, m6
    pandn           m9, m5
    pandn          m10, m6
    pmaxsw          m7, m9                      ; max after p0
    pminsw          m8, m5                      ; min after p0
    pmaxsw          m7, m10                     ; max after p1
    pminsw          m8, m6                      ; min after p1

    ; accumulate sum[m15] over p0/p1
    psubw           m5, m4                      ; diff_p0(p0 - px)
    psubw           m6, m4                      ; diff_p1(p1 - px)
    pabsw           m9, m5
    pabsw          m10, m6
    psraw          m11, m9,  %2
    psraw          m12, m10, %2
    psubw          m11, %3, m11
    psubw          m12, %3, m12
    pmaxsw         m11, m13
    pmaxsw         m12, m13
    pminsw         m11, m9
    pminsw         m12, m10
    psignw         m11, m5                      ; constrain(diff_p0)
    psignw         m12, m6                      ; constrain(diff_p1)
    pmullw         m11, %4                      ; constrain(diff_p0) * pri_taps
    pmullw         m12, %4                      ; constrain(diff_p1) * pri_taps
    paddw          m15, m11
    paddw          m15, m12
%endmacro

%macro cdef_filter_fn 3 ; w, h, stride
INIT_YMM avx2
cglobal cdef_filter_%1x%2, 4, 9, 16, 2 * 16 + (%2+4)*%3, \
                           dst, stride, left, top, pri, sec, stride3, dst4, edge
%define px rsp+2*16+2*%3
    pcmpeqw        m14, m14
    psrlw          m14, 1                   ; 0x7fff
    mov          edged, r8m

    ; prepare pixel buffers - body/right
%if %1 == 4
    INIT_XMM avx2
%endif
%if %2 == 8
    lea          dst4q, [dstq+strideq*4]
%endif
    lea       stride3q, [strideq*3]
    test         edged, 2                   ; have_right
    jz .no_right
    pmovzxbw        m1, [dstq+strideq*0]
    pmovzxbw        m2, [dstq+strideq*1]
    pmovzxbw        m3, [dstq+strideq*2]
    pmovzxbw        m4, [dstq+stride3q]
    mova     [px+0*%3], m1
    mova     [px+1*%3], m2
    mova     [px+2*%3], m3
    mova     [px+3*%3], m4
%if %2 == 8
    pmovzxbw        m1, [dst4q+strideq*0]
    pmovzxbw        m2, [dst4q+strideq*1]
    pmovzxbw        m3, [dst4q+strideq*2]
    pmovzxbw        m4, [dst4q+stride3q]
    mova     [px+4*%3], m1
    mova     [px+5*%3], m2
    mova     [px+6*%3], m3
    mova     [px+7*%3], m4
%endif
    jmp .body_done
.no_right:
%if %1 == 4
    movd           xm1, [dstq+strideq*0]
    movd           xm2, [dstq+strideq*2]
    pinsrd         xm1, [dstq+strideq*1], 1
    pinsrd         xm2, [dstq+stride3q], 1
    pmovzxbw       xm1, xm1
    pmovzxbw       xm2, xm2
    movq     [px+0*%3], xm1
    movhps   [px+1*%3], xm1
    movq     [px+2*%3], xm2
    movhps   [px+3*%3], xm2
%else
    pmovzxbw       xm1, [dstq+strideq*0]
    pmovzxbw       xm2, [dstq+strideq*1]
    pmovzxbw       xm3, [dstq+strideq*2]
    pmovzxbw       xm4, [dstq+stride3q]
    mova     [px+0*%3], xm1
    mova     [px+1*%3], xm2
    mova     [px+2*%3], xm3
    mova     [px+3*%3], xm4
%endif
    movd [px+0*%3+%1*2], xm14
    movd [px+1*%3+%1*2], xm14
    movd [px+2*%3+%1*2], xm14
    movd [px+3*%3+%1*2], xm14
%if %2 == 8
    ; FIXME w == 4
    movd [px+0*%3+%1*2], xm14
    movd [px+1*%3+%1*2], xm14
    movd [px+2*%3+%1*2], xm14
    movd [px+3*%3+%1*2], xm14
    pmovzxbw       xm1, [dst4q+strideq*0]
    pmovzxbw       xm2, [dst4q+strideq*1]
    pmovzxbw       xm3, [dst4q+strideq*2]
    pmovzxbw       xm4, [dst4q+stride3q]
    mova     [px+4*%3], xm1
    mova     [px+5*%3], xm2
    mova     [px+6*%3], xm3
    mova     [px+7*%3], xm4
    movd [px+4*%3+%1*2], xm14
    movd [px+5*%3+%1*2], xm14
    movd [px+6*%3+%1*2], xm14
    movd [px+7*%3+%1*2], xm14
%endif
.body_done:

    ; top
    DEFINE_ARGS dst, stride, left, top2, pri, sec, stride3, top1, edge
    test         edged, 4                    ; have_top
    jz .no_top
    mov          top1q, [top2q+0*gprsize]
    mov          top2q, [top2q+1*gprsize]
    test         edged, 1                    ; have_left
    jz .top_no_left
    test         edged, 2                    ; have_right
    jz .top_no_right
    pmovzxbw        m1, [top1q-(%1/2)]
    pmovzxbw        m2, [top2q-(%1/2)]
    movu  [px-2*%3-%1], m1
    movu  [px-1*%3-%1], m2
    jmp .top_done
.top_no_right:
    pmovzxbw        m1, [top1q-%1]
    pmovzxbw        m2, [top2q-%1]
    movu [px-2*%3-%1*2], m1
    movu [px-1*%3-%1*2], m2
    movd [px-2*%3+%1*2], xm14
    movd [px-1*%3+%1*2], xm14
    jmp .top_done
.top_no_left:
    test         edged, 2                   ; have_right
    jz .top_no_left_right
    pmovzxbw        m1, [top1q]
    pmovzxbw        m2, [top2q]
    mova   [px-2*%3+0], m1
    mova   [px-1*%3+0], m2
    movd   [px-2*%3-4], xm14
    movd   [px-1*%3-4], xm14
    jmp .top_done
.top_no_left_right:
%if %1 == 4
    movd           xm1, [top1q]
    pinsrd         xm1, [top2q], 1
    pmovzxbw       xm1, xm1
    movq   [px-2*%3+0], xm1
    movhps [px-1*%3+0], xm1
%else
    pmovzxbw       xm1, [top1q]
    pmovzxbw       xm2, [top2q]
    mova   [px-2*%3+0], xm1
    mova   [px-1*%3+0], xm2
%endif
    movd   [px-2*%3-4], xm14
    movd   [px-1*%3-4], xm14
    movd [px-2*%3+%1*2], xm14
    movd [px-1*%3+%1*2], xm14
    jmp .top_done
.no_top:
    movu   [px-2*%3-%1], m14
    movu   [px-1*%3-%1], m14
.top_done:

    ; left
    test         edged, 1                   ; have_left
    jz .no_left
    pmovzxbw       xm1, [leftq+ 0]
%if %2 == 8
    pmovzxbw       xm2, [leftq+ 8]
%endif
    movd   [px+0*32-4], xm1
    pextrd [px+1*32-4], xm1, 1
    pextrd [px+2*32-4], xm1, 2
    pextrd [px+3*32-4], xm1, 3
%if %2 == 8
    movd   [px+4*32-4], xm2
    pextrd [px+5*32-4], xm2, 1
    pextrd [px+6*32-4], xm2, 2
    pextrd [px+7*32-4], xm2, 3
%endif
    jmp .left_done
.no_left:
    movd   [px+0*%3-4], xm14
    movd   [px+1*%3-4], xm14
    movd   [px+2*%3-4], xm14
    movd   [px+3*%3-4], xm14
%if %2 == 8
    movd   [px+4*%3-4], xm14
    movd   [px+5*%3-4], xm14
    movd   [px+6*%3-4], xm14
    movd   [px+7*%3-4], xm14
%endif
.left_done:

    ; bottom
    DEFINE_ARGS dst, stride, dst8, dummy1, pri, sec, stride3, dummy3, edge
    test         edged, 8                   ; have_bottom
    jz .no_bottom
    lea          dst8q, [dstq+%2*strideq]
    test         edged, 1                   ; have_left
    jz .bottom_no_left
    test         edged, 2                   ; have_right
    jz .bottom_no_right
    pmovzxbw        m1, [dst8q-(%1/2)]
    pmovzxbw        m2, [dst8q+strideq-(%1/2)]
    movu   [px+(%2+0)*%3-%1], m1
    movu   [px+(%2+1)*%3-%1], m2
    jmp .bottom_done
.bottom_no_right:
    pmovzxbw        m1, [dst8q-%1]
    pmovzxbw        m2, [dst8q+strideq-%1]
    movu  [px+(%2+0)*%3-%1*2], m1
    movu  [px+(%2+1)*%3-%1*2], m2
%if %1 == 8
    movd  [px+(%2-1)*%3+%1*2], xm14                ; overwritten by previous movu
%endif
    movd  [px+(%2+0)*%3+%1*2], xm14
    movd  [px+(%2+1)*%3+%1*2], xm14
    jmp .bottom_done
.bottom_no_left:
    test          edged, 2                  ; have_right
    jz .bottom_no_left_right
    pmovzxbw        m1, [dst8q]
    pmovzxbw        m2, [dst8q+strideq]
    mova   [px+(%2+0)*%3+0], m1
    mova   [px+(%2+1)*%3+0], m2
    movd   [px+(%2+0)*%3-4], xm14
    movd   [px+(%2+1)*%3-4], xm14
    jmp .bottom_done
.bottom_no_left_right:
%if %1 == 4
    movd           xm1, [dst8q]
    pinsrd         xm1, [dst8q+strideq], 1
    pmovzxbw       xm1, xm1
    movq   [px+(%2+0)*%3+0], xm1
    movhps [px+(%2+1)*%3+0], xm1
%else
    pmovzxbw       xm1, [dst8q]
    pmovzxbw       xm2, [dst8q+strideq]
    mova   [px+(%2+0)*%3+0], xm1
    mova   [px+(%2+1)*%3+0], xm2
%endif
    movd   [px+(%2+0)*%3-4], xm14
    movd   [px+(%2+1)*%3-4], xm14
    movd  [px+(%2+0)*%3+%1*2], xm14
    movd  [px+(%2+1)*%3+%1*2], xm14
    jmp .bottom_done
.no_bottom:
    movu   [px+(%2+0)*%3-%1], m14
    movu   [px+(%2+1)*%3-%1], m14
.bottom_done:

    ; actual filter
    INIT_YMM avx2
    DEFINE_ARGS dst, stride, pridmp, damping, pri, sec, stride3, secdmp
%undef edged
    movifnidn     prid, prim
    movifnidn     secd, secm
    mov       dampingd, r7m

    mov        pridmpd, prid
    mov        secdmpd, secd
    or         pridmpd, 1
    or         secdmpd, 1
    lzcnt      pridmpd, pridmpd
    lzcnt      secdmpd, secdmpd
    lea        pridmpd, [pridmpd+dampingd-31]
    lea        secdmpd, [secdmpd+dampingd-31]
    xor       dampingd, dampingd
    test       pridmpd, pridmpd
    cmovl      pridmpd, dampingd
    test       secdmpd, secdmpd
    cmovl      secdmpd, dampingd
    mov        [rsp+0], pridmpq                 ; pri_shift
    mov        [rsp+8], secdmpq                 ; sec_shift

    ; pri/sec_taps[k] [4 total]
    DEFINE_ARGS dst, stride, tap, dummy, pri, sec, stride3
    movd           xm0, prid
    movd           xm1, secd
    vpbroadcastw    m0, xm0                     ; pri_strength
    vpbroadcastw    m1, xm1                     ; sec_strength
    and           prid, 1
    lea           tapq, [tap_table]
    lea           priq, [tapq+priq*4]           ; pri_taps
    lea           secq, [tapq+8]                ; sec_taps

    ; off1/2/3[k] [6 total] from [tapq+12+(dir+0/2/6)*2+k]
    DEFINE_ARGS dst, stride, tap, dir, pri, sec, stride3
    mov           dird, r6m
    lea           tapq, [tapq+dirq*2+12]
%if %1*%2*2/mmsize > 1
    DEFINE_ARGS dst, stride, dir, stk, pri, sec, h, off, k
    mov             hd, %1*%2*2/mmsize
%else
    DEFINE_ARGS dst, stride, dir, stk, pri, sec, stride3, off, k
%endif
    lea           stkq, [px]
    pxor           m13, m13
%if %1*%2*2/mmsize > 1
.v_loop:
%endif
    mov             kd, 1
%if %1 == 4
    movq           xm4, [stkq+%3*0]
    movhps         xm4, [stkq+%3*1]
    movq           xm5, [stkq+%3*2]
    movhps         xm5, [stkq+%3*3]
    vinserti128     m4, xm5, 1
%else
    mova           xm4, [stkq+%3*0]             ; px
    vinserti128     m4, [stkq+%3*1], 1
%endif
    pxor           m15, m15                     ; sum
    mova            m7, m4                      ; max
    mova            m8, m4                      ; min
.k_loop:
    vpbroadcastw    m2, [priq+kq*2]             ; pri_taps
    vpbroadcastw    m3, [secq+kq*2]             ; sec_taps

    ACCUMULATE_TAP 0*2, [rsp+0], m0, m2, %1, %3
    ACCUMULATE_TAP 2*2, [rsp+8], m1, m3, %1, %3
    ACCUMULATE_TAP 6*2, [rsp+8], m1, m3, %1, %3

    dec             kq
    jge .k_loop

    vpbroadcastd   m12, [pw_2048]
    pcmpgtw        m11, m13, m15
    paddw          m15, m11
    pmulhrsw       m15, m12
    paddw           m4, m15
    pminsw          m4, m7
    pmaxsw          m4, m8
    packuswb        m4, m4
    vextracti128   xm5, m4, 1
%if %1 == 4
    movd [dstq+strideq*0], xm4
    pextrd [dstq+strideq*1], xm4, 1
    movd [dstq+strideq*2], xm5
    pextrd [dstq+stride3q], xm5, 1
%else
    movq [dstq+strideq*0], xm4
    movq [dstq+strideq*1], xm5
%endif

%if %1*%2*2/mmsize > 1
    lea           dstq, [dstq+strideq*2]
    add           stkq, %3*2
    dec             hd
    jg .v_loop
%endif

    RET
%endmacro

cdef_filter_fn 8, 8, 32
cdef_filter_fn 4, 4, 32

INIT_YMM avx2
cglobal cdef_dir, 3, 4, 15, src, stride, var, stride3
    lea       stride3q, [strideq*3]
    movq           xm0, [srcq+strideq*0]
    movq           xm1, [srcq+strideq*1]
    movq           xm2, [srcq+strideq*2]
    movq           xm3, [srcq+stride3q]
    lea           srcq, [srcq+strideq*4]
    vpbroadcastq    m4, [srcq+strideq*0]
    vpbroadcastq    m5, [srcq+strideq*1]
    vpbroadcastq    m6, [srcq+strideq*2]
    vpbroadcastq    m7, [srcq+stride3q]
    vpbroadcastd    m8, [pw_128]
    pxor            m9, m9

    vpblendd        m0, m0, m7, 0xf0
    vpblendd        m1, m1, m6, 0xf0
    vpblendd        m2, m2, m5, 0xf0
    vpblendd        m3, m3, m4, 0xf0

    punpcklbw       m0, m9
    punpcklbw       m1, m9
    punpcklbw       m2, m9
    punpcklbw       m3, m9

    psubw           m0, m8
    psubw           m1, m8
    psubw           m2, m8
    psubw           m3, m8

    ; shuffle registers to generate partial_sum_diag[0-1] together
    vpermq          m7, m0, q1032
    vpermq          m6, m1, q1032
    vpermq          m5, m2, q1032
    vpermq          m4, m3, q1032

    ; start with partial_sum_hv[0-1]
    paddw           m8, m0, m1
    paddw           m9, m2, m3
    phaddw         m10, m0, m1
    phaddw         m11, m2, m3
    paddw           m8, m9
    phaddw         m10, m11
    vextracti128   xm9, m8, 1
    vextracti128  xm11, m10, 1
    paddw          xm8, xm9                 ; partial_sum_hv[1]
    phaddw        xm10, xm11                ; partial_sum_hv[0]
    vinserti128     m8, xm10, 1
    vpbroadcastd    m9, [div_table+44]
    pmaddwd         m8, m8
    pmulld          m8, m9                  ; cost6[2a-d] | cost2[a-d]

    ; create aggregates [lower half]:
    ; m9 = m0:01234567+m1:x0123456+m2:xx012345+m3:xxx01234+
    ;      m4:xxxx0123+m5:xxxxx012+m6:xxxxxx01+m7:xxxxxxx0
    ; m10=             m1:7xxxxxxx+m2:67xxxxxx+m3:567xxxxx+
    ;      m4:4567xxxx+m5:34567xxx+m6:234567xx+m7:1234567x
    ; and [upper half]:
    ; m9 = m0:xxxxxxx0+m1:xxxxxx01+m2:xxxxx012+m3:xxxx0123+
    ;      m4:xxx01234+m5:xx012345+m6:x0123456+m7:01234567
    ; m10= m0:1234567x+m1:234567xx+m2:34567xxx+m3:4567xxxx+
    ;      m4:567xxxxx+m5:67xxxxxx+m6:7xxxxxxx
    ; and then shuffle m11 [shufw_6543210x], unpcklwd, pmaddwd, pmulld, paddd

    pslldq          m9, m1, 2
    psrldq         m10, m1, 14
    pslldq         m11, m2, 4
    psrldq         m12, m2, 12
    pslldq         m13, m3, 6
    psrldq         m14, m3, 10
    paddw           m9, m11
    paddw          m10, m12
    paddw           m9, m13
    paddw          m10, m14
    pslldq         m11, m4, 8
    psrldq         m12, m4, 8
    pslldq         m13, m5, 10
    psrldq         m14, m5, 6
    paddw           m9, m11
    paddw          m10, m12
    paddw           m9, m13
    paddw          m10, m14
    pslldq         m11, m6, 12
    psrldq         m12, m6, 4
    pslldq         m13, m7, 14
    psrldq         m14, m7, 2
    paddw           m9, m11
    paddw          m10, m12
    paddw           m9, m13
    paddw          m10, m14                 ; partial_sum_diag[0/1][8-14,zero]
    vbroadcasti128 m14, [shufw_6543210x]
    vbroadcasti128 m13, [div_table+16]
    vbroadcasti128 m12, [div_table+0]
    paddw           m9, m0                  ; partial_sum_diag[0/1][0-7]
    pshufb         m10, m14
    punpckhwd      m11, m9, m10
    punpcklwd       m9, m10
    pmaddwd        m11, m11
    pmaddwd         m9, m9
    pmulld         m11, m13
    pmulld          m9, m12
    paddd           m9, m11                 ; cost0[a-d] | cost4[a-d]

    ; merge horizontally and vertically for partial_sum_alt[0-3]
    paddw          m10, m0, m1
    paddw          m11, m2, m3
    paddw          m12, m4, m5
    paddw          m13, m6, m7
    phaddw          m0, m4
    phaddw          m1, m5
    phaddw          m2, m6
    phaddw          m3, m7

    ; create aggregates [lower half]:
    ; m4 = m10:01234567+m11:x0123456+m12:xx012345+m13:xxx01234
    ; m11=              m11:7xxxxxxx+m12:67xxxxxx+m13:567xxxxx
    ; and [upper half]:
    ; m4 = m10:xxx01234+m11:xx012345+m12:x0123456+m13:01234567
    ; m11= m10:567xxxxx+m11:67xxxxxx+m12:7xxxxxxx
    ; and then shuffle m11 [shufw_210xxxxx], unpcklwd, pmaddwd, pmulld, paddd

    vbroadcasti128 m14, [shufw_210xxxxx]
    pslldq          m4, m11, 2
    psrldq         m11, 14
    pslldq          m5, m12, 4
    psrldq         m12, 12
    pslldq          m6, m13, 6
    psrldq         m13, 10
    paddw           m4, m10
    paddw          m11, m12
    vpbroadcastd   m12, [div_table+44]
    paddw           m5, m6
    paddw          m11, m13                 ; partial_sum_alt[3/2] right
    vbroadcasti128 m13, [div_table+32]
    paddw           m4, m5                  ; partial_sum_alt[3/2] left
    pshufb         m11, m14
    punpckhwd       m6, m4, m11
    punpcklwd       m4, m11
    pmaddwd         m6, m6
    pmaddwd         m4, m4
    pmulld          m6, m12
    pmulld          m4, m13
    paddd           m4, m6                  ; cost7[a-d] | cost5[a-d]

    ; create aggregates [lower half]:
    ; m5 = m0:01234567+m1:x0123456+m2:xx012345+m3:xxx01234
    ; m1 =             m1:7xxxxxxx+m2:67xxxxxx+m3:567xxxxx
    ; and [upper half]:
    ; m5 = m0:xxx01234+m1:xx012345+m2:x0123456+m3:01234567
    ; m1 = m0:567xxxxx+m1:67xxxxxx+m2:7xxxxxxx
    ; and then shuffle m11 [shufw_210xxxxx], unpcklwd, pmaddwd, pmulld, paddd

    pslldq          m5, m1, 2
    psrldq          m1, 14
    pslldq          m6, m2, 4
    psrldq          m2, 12
    pslldq          m7, m3, 6
    psrldq          m3, 10
    paddw           m5, m0
    paddw           m1, m2
    paddw           m6, m7
    paddw           m1, m3                  ; partial_sum_alt[0/1] right
    paddw           m5, m6                  ; partial_sum_alt[0/1] left
    pshufb          m1, m14
    punpckhwd       m6, m5, m1
    punpcklwd       m5, m1
    pmaddwd         m6, m6
    pmaddwd         m5, m5
    pmulld          m6, m12
    pmulld          m5, m13
    paddd           m5, m6                  ; cost1[a-d] | cost3[a-d]

    mova           xm0, [pd_04512763+ 0]
    mova           xm1, [pd_04512763+ 16]
    phaddd          m9, m8
    phaddd          m5, m4
    phaddd          m9, m5
    vpermd          m0, m9                  ; cost[0/4/2/6]
    vpermd          m1, m9                  ; cost[1/5/3/7]

    ; now find the best cost, its idx^4 complement, and its idx
    pcmpgtd        xm2, xm1, xm0            ; [1/5/3/7] > [0/4/2/6]
    pand           xm3, xm2, xm1
    pandn          xm4, xm2, xm0
    por            xm3, xm4                 ; higher 4 values
    pshufd         xm1, xm1, q2301
    pshufd         xm0, xm0, q2301
    pand           xm1, xm2, xm1
    pandn          xm0, xm2, xm0
    por            xm0, xm1                 ; complementary 4 values at idx^4 offset
    pand          xm13, xm2, [pd_04261537+16]
    pandn         xm14, xm2, [pd_04261537+ 0]
    por           xm14, xm13                ; indices

    punpckhqdq     xm4, xm3, xm0
    punpcklqdq     xm3, xm0
    pcmpgtd        xm5, xm4, xm3            ; [2or3-6or7] > [0or1/4or5]
    punpcklqdq     xm5, xm5
    pand           xm6, xm5, xm4
    pandn          xm7, xm5, xm3
    por            xm6, xm7                 ; { highest 2 values, complements at idx^4 }
    movhlps       xm13, xm14
    pand          xm13, xm5, xm13
    pandn         xm14, xm5, xm14
    por           xm14, xm13

    pshufd         xm7, xm6, q3311
    pcmpgtd        xm8, xm7, xm6            ; [4or5or6or7] > [0or1or2or3]
    punpcklqdq     xm8, xm8
    pand           xm9, xm8, xm7
    pandn         xm10, xm8, xm6
    por            xm9, xm10                ; max
    movhlps       xm10, xm9                 ; complement at idx^4
    psubd          xm9, xm10
    psrld          xm9, 10
    movd        [varq], xm9
    pshufd        xm13, xm14, q1111
    pand          xm13, xm8, xm13
    pandn         xm14, xm8, xm14
    por           xm14, xm13
    movd           eax, xm14
    RET
%endif ; ARCH_X86_64
