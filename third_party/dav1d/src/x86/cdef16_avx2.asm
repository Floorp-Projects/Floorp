; Copyright © 2021, VideoLAN and dav1d authors
; Copyright © 2021, Two Orioles, LLC
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

SECTION_RODATA

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

dir_shift: times 2 dw 0x4000
           times 2 dw 0x1000

pw_2048:   times 2 dw 2048

cextern cdef_dir_8bpc_avx2.main

SECTION .text

%macro REPX 2-*
    %xdefine %%f(x) %1
%rep %0 - 1
    %rotate 1
    %%f(%1)
%endrep
%endmacro

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
    ; out of bounds values are set to a value that is a both a large unsigned
    ; value and a negative signed value.
    ; use signed max and unsigned min to remove them
    pmaxsw          m7, m5                      ; max after p0
    pminuw          m8, m5                      ; min after p0
    pmaxsw          m7, m6                      ; max after p1
    pminuw          m8, m6                      ; min after p1

    ; accumulate sum[m15] over p0/p1
    psubw           m5, m4                      ; diff_p0(p0 - px)
    psubw           m6, m4                      ; diff_p1(p1 - px)
    pabsw           m9, m5
    pabsw          m10, m6
    psignw         m11, %4, m5
    psignw         m12, %4, m6
    psrlw           m5, m9, %2
    psrlw           m6, m10, %2
    psubusw         m5, %3, m5
    psubusw         m6, %3, m6
    pminuw          m5, m9                      ; constrain(diff_p0)
    pminuw          m6, m10                     ; constrain(diff_p1)
    pmullw          m5, m11                     ; constrain(diff_p0) * taps
    pmullw          m6, m12                     ; constrain(diff_p1) * taps
    paddw          m15, m5
    paddw          m15, m6
%endmacro

%macro cdef_filter_fn 3 ; w, h, stride
INIT_YMM avx2
%if %1 != 4 || %2 != 8
cglobal cdef_filter_%1x%2_16bpc, 4, 9, 16, 2 * 16 + (%2+4)*%3, \
                                 dst, stride, left, top, pri, sec, \
                                 stride3, dst4, edge
%else
cglobal cdef_filter_%1x%2_16bpc, 4, 10, 16, 2 * 16 + (%2+4)*%3, \
                                 dst, stride, left, top, pri, sec, \
                                 stride3, dst4, edge
%endif
%define px rsp+2*16+2*%3
    pcmpeqw        m14, m14
    psllw          m14, 15                  ; 0x8000
    mov          edged, r8m

    ; prepare pixel buffers - body/right
%if %1 == 4
    INIT_XMM avx2
%endif
%if %2 == 8
    lea          dst4q, [dstq+strideq*4]
%endif
    lea       stride3q, [strideq*3]
    test         edgeb, 2                   ; have_right
    jz .no_right
    movu            m1, [dstq+strideq*0]
    movu            m2, [dstq+strideq*1]
    movu            m3, [dstq+strideq*2]
    movu            m4, [dstq+stride3q]
    mova     [px+0*%3], m1
    mova     [px+1*%3], m2
    mova     [px+2*%3], m3
    mova     [px+3*%3], m4
%if %2 == 8
    movu            m1, [dst4q+strideq*0]
    movu            m2, [dst4q+strideq*1]
    movu            m3, [dst4q+strideq*2]
    movu            m4, [dst4q+stride3q]
    mova     [px+4*%3], m1
    mova     [px+5*%3], m2
    mova     [px+6*%3], m3
    mova     [px+7*%3], m4
%endif
    jmp .body_done
.no_right:
%if %1 == 4
    movq           xm1, [dstq+strideq*0]
    movq           xm2, [dstq+strideq*1]
    movq           xm3, [dstq+strideq*2]
    movq           xm4, [dstq+stride3q]
    movq     [px+0*%3], xm1
    movq     [px+1*%3], xm2
    movq     [px+2*%3], xm3
    movq     [px+3*%3], xm4
%else
    mova           xm1, [dstq+strideq*0]
    mova           xm2, [dstq+strideq*1]
    mova           xm3, [dstq+strideq*2]
    mova           xm4, [dstq+stride3q]
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
 %if %1 == 4
    movq           xm1, [dst4q+strideq*0]
    movq           xm2, [dst4q+strideq*1]
    movq           xm3, [dst4q+strideq*2]
    movq           xm4, [dst4q+stride3q]
    movq     [px+4*%3], xm1
    movq     [px+5*%3], xm2
    movq     [px+6*%3], xm3
    movq     [px+7*%3], xm4
 %else
    mova           xm1, [dst4q+strideq*0]
    mova           xm2, [dst4q+strideq*1]
    mova           xm3, [dst4q+strideq*2]
    mova           xm4, [dst4q+stride3q]
    mova     [px+4*%3], xm1
    mova     [px+5*%3], xm2
    mova     [px+6*%3], xm3
    mova     [px+7*%3], xm4
 %endif
    movd [px+4*%3+%1*2], xm14
    movd [px+5*%3+%1*2], xm14
    movd [px+6*%3+%1*2], xm14
    movd [px+7*%3+%1*2], xm14
%endif
.body_done:

    ; top
    test         edgeb, 4                    ; have_top
    jz .no_top
    test         edgeb, 1                    ; have_left
    jz .top_no_left
    test         edgeb, 2                    ; have_right
    jz .top_no_right
    movu            m1, [topq+strideq*0-%1]
    movu            m2, [topq+strideq*1-%1]
    movu  [px-2*%3-%1], m1
    movu  [px-1*%3-%1], m2
    jmp .top_done
.top_no_right:
    movu            m1, [topq+strideq*0-%1*2]
    movu            m2, [topq+strideq*1-%1*2]
    movu [px-2*%3-%1*2], m1
    movu [px-1*%3-%1*2], m2
    movd [px-2*%3+%1*2], xm14
    movd [px-1*%3+%1*2], xm14
    jmp .top_done
.top_no_left:
    test         edgeb, 2                   ; have_right
    jz .top_no_left_right
    movu            m1, [topq+strideq*0]
    movu            m2, [topq+strideq*1]
    mova   [px-2*%3+0], m1
    mova   [px-1*%3+0], m2
    movd   [px-2*%3-4], xm14
    movd   [px-1*%3-4], xm14
    jmp .top_done
.top_no_left_right:
%if %1 == 4
    movq           xm1, [topq+strideq*0]
    movq           xm2, [topq+strideq*1]
    movq   [px-2*%3+0], xm1
    movq   [px-1*%3+0], xm2
%else
    mova           xm1, [topq+strideq*0]
    mova           xm2, [topq+strideq*1]
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
    test         edgeb, 1                   ; have_left
    jz .no_left
    mova           xm1, [leftq+ 0]
%if %2 == 8
    mova           xm2, [leftq+16]
%endif
    movd   [px+0*%3-4], xm1
    pextrd [px+1*%3-4], xm1, 1
    pextrd [px+2*%3-4], xm1, 2
    pextrd [px+3*%3-4], xm1, 3
%if %2 == 8
    movd   [px+4*%3-4], xm2
    pextrd [px+5*%3-4], xm2, 1
    pextrd [px+6*%3-4], xm2, 2
    pextrd [px+7*%3-4], xm2, 3
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
    test         edgeb, 8                   ; have_bottom
    jz .no_bottom
    lea          dst8q, [dstq+%2*strideq]
    test         edgeb, 1                   ; have_left
    jz .bottom_no_left
    test         edgeb, 2                   ; have_right
    jz .bottom_no_right
    movu            m1, [dst8q-%1]
    movu            m2, [dst8q+strideq-%1]
    movu   [px+(%2+0)*%3-%1], m1
    movu   [px+(%2+1)*%3-%1], m2
    jmp .bottom_done
.bottom_no_right:
    movu            m1, [dst8q-%1*2]
    movu            m2, [dst8q+strideq-%1*2]
    movu  [px+(%2+0)*%3-%1*2], m1
    movu  [px+(%2+1)*%3-%1*2], m2
%if %1 == 8
    movd  [px+(%2-1)*%3+%1*2], xm14                ; overwritten by previous movu
%endif
    movd  [px+(%2+0)*%3+%1*2], xm14
    movd  [px+(%2+1)*%3+%1*2], xm14
    jmp .bottom_done
.bottom_no_left:
    test         edgeb, 2                  ; have_right
    jz .bottom_no_left_right
    movu            m1, [dst8q]
    movu            m2, [dst8q+strideq]
    mova   [px+(%2+0)*%3+0], m1
    mova   [px+(%2+1)*%3+0], m2
    movd   [px+(%2+0)*%3-4], xm14
    movd   [px+(%2+1)*%3-4], xm14
    jmp .bottom_done
.bottom_no_left_right:
%if %1 == 4
    movq           xm1, [dst8q]
    movq           xm2, [dst8q+strideq]
    movq   [px+(%2+0)*%3+0], xm1
    movq   [px+(%2+1)*%3+0], xm2
%else
    mova           xm1, [dst8q]
    mova           xm2, [dst8q+strideq]
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
    DEFINE_ARGS dst, stride, pridmp, damping, pri, secdmp, stride3, zero
 %undef edged
    movifnidn     prid, prim
    mov       dampingd, r7m
    lzcnt      pridmpd, prid
%if UNIX64
    movd           xm0, prid
    movd           xm1, secdmpd
%endif
    lzcnt      secdmpd, secdmpm
    sub       dampingd, 31
    xor          zerod, zerod
    add        pridmpd, dampingd
    cmovs      pridmpd, zerod
    add        secdmpd, dampingd
    cmovs      secdmpd, zerod
    mov        [rsp+0], pridmpq                 ; pri_shift
    mov        [rsp+8], secdmpq                 ; sec_shift

    ; pri/sec_taps[k] [4 total]
    DEFINE_ARGS dst, stride, table, dir, pri, sec, stride3
%if UNIX64
    vpbroadcastw    m0, xm0                     ; pri_strength
    vpbroadcastw    m1, xm1                     ; sec_strength
%else
    vpbroadcastw    m0, prim                    ; pri_strength
    vpbroadcastw    m1, secm                    ; sec_strength
%endif
    rorx           r2d, prid, 2
    cmp      dword r9m, 0xfff
    cmove         prid, r2d
    and           prid, 4
    lea         tableq, [tap_table]
    lea           priq, [tableq+priq]           ; pri_taps
    lea           secq, [tableq+8]              ; sec_taps

    ; off1/2/3[k] [6 total] from [tableq+12+(dir+0/2/6)*2+k]
    mov           dird, r6m
    lea         tableq, [tableq+dirq*2+12]
%if %1*%2*2/mmsize > 1
 %if %1 == 4
    DEFINE_ARGS dst, stride, dir, stk, pri, sec, stride3, h, off, k
 %else
    DEFINE_ARGS dst, stride, dir, stk, pri, sec, h, off, k
 %endif
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
%if %1 == 4
    vextracti128   xm5, m4, 1
    movq [dstq+strideq*0], xm4
    movhps [dstq+strideq*1], xm4
    movq [dstq+strideq*2], xm5
    movhps [dstq+stride3q], xm5
%else
    mova [dstq+strideq*0], xm4
    vextracti128 [dstq+strideq*1], m4, 1
%endif

%if %1*%2*2/mmsize > 1
 %define vloop_lines (mmsize/(%1*2))
    lea           dstq, [dstq+strideq*vloop_lines]
    add           stkq, %3*vloop_lines
    dec             hd
    jg .v_loop
%endif

    RET
%endmacro

cdef_filter_fn 8, 8, 32
cdef_filter_fn 4, 4, 32

INIT_YMM avx2
cglobal cdef_dir_16bpc, 4, 7, 6, src, stride, var, bdmax
    lea             r6, [dir_shift]
    shr         bdmaxd, 11 ; 0 for 10bpc, 1 for 12bpc
    vpbroadcastd    m4, [r6+bdmaxq*4]
    lea             r6, [strideq*3]
    mova           xm0, [srcq+strideq*0]
    mova           xm1, [srcq+strideq*1]
    mova           xm2, [srcq+strideq*2]
    mova           xm3, [srcq+r6       ]
    lea           srcq, [srcq+strideq*4]
    vinserti128     m0, [srcq+r6       ], 1
    vinserti128     m1, [srcq+strideq*2], 1
    vinserti128     m2, [srcq+strideq*1], 1
    vinserti128     m3, [srcq+strideq*0], 1
    REPX {pmulhuw x, m4}, m0, m1, m2, m3
    jmp mangle(private_prefix %+ _cdef_dir_8bpc %+ SUFFIX).main

%endif ; ARCH_X86_64
