; Copyright © 2018, VideoLAN and dav1d authors
; Copyright © 2018, Two Orioles, LLC
; Copyright © 2018, VideoLabs
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

%include "ext/x86/x86inc.asm"

SECTION_RODATA 16

pb_right_ext_mask: times 16 db 0xff
                   times 16 db 0
pb_14x0_1_2: times 14 db 0
             db 1, 2
pb_0_to_15_min_n: db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13
                  db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 14
pb_unpcklwdw: db 0, 1, 0, 1, 4, 5, 4, 5, 8, 9, 8, 9, 12, 13, 12, 13
pb_0: times 16 db 0
pb_2: times 16 db 2
pb_3: times 16 db 3
pb_4: times 16 db 4
pb_15: times 16 db 15
pb_0_1: times 8 db 0, 1
pb_6_7: times 8 db 6, 7
pb_14_15: times 8 db 14, 15
pw_1: times 8 dw 1
pw_16: times 8 dw 16
pw_128: times 8 dw 128
pw_255: times 8 dw 255
pw_256: times 8 dw 256
pw_2048: times 8 dw 2048
pw_16380: times 8 dw 16380
pw_5_6: times 4 dw 5, 6
pw_0_128: times 4 dw 0, 128
pd_1024: times 4 dd 1024
%if ARCH_X86_32
pd_256: times 4 dd 256
pd_512: times 4 dd 512
pd_2048: times 4 dd 2048
%endif
pd_0xF0080029: times 4 dd 0xF0080029
pd_0xF00801C7: times 4 dd 0XF00801C7

cextern sgr_x_by_x

SECTION .text

%if ARCH_X86_32
 %define PIC_base_offset $$

 %macro SETUP_PIC 1-3 1,0 ; PIC_reg, save_PIC_reg, restore_PIC_reg
  %assign pic_reg_stk_off 4
  %xdefine PIC_reg %1
  %if %2 == 1
    mov        [esp], %1
  %endif
    LEA      PIC_reg, PIC_base_offset
  %if %3 == 1
    XCHG_PIC_REG
  %endif
 %endmacro

 %macro XCHG_PIC_REG 0
    mov [esp+pic_reg_stk_off], PIC_reg
    %assign pic_reg_stk_off (pic_reg_stk_off+4) % 8
    mov PIC_reg, [esp+pic_reg_stk_off]
 %endmacro

 %define PIC_sym(sym)   (PIC_reg+(sym)-PIC_base_offset)

%else
 %macro XCHG_PIC_REG 0
 %endmacro

 %define PIC_sym(sym)   (sym)
%endif

%macro PALIGNR 4 ; dst, src1, src2, shift
 %if cpuflag(ssse3)
    palignr       %1, %2, %3, %4
 %else
  %assign %%i regnumof%+%1 + 1
  %define %%tmp m %+ %%i
    psrldq        %1, %3, %4
    pslldq     %%tmp, %2, 16-%4
    por           %1, %%tmp
 %endif
%endmacro

%macro PMADDUBSW 5 ; dst, src, zero, tmp, reset_zero
 %if cpuflag(ssse3)
    pmaddubsw     %1, %2
 %else
  %if %5 == 1
    pxor          %3, %3
  %endif
    punpckhbw     %4, %1, %3
    punpcklbw     %1, %3
    pmaddwd       %4, %2
    pmaddwd       %1, %2
    packssdw      %1, %4
 %endif
%endmacro

;;;;;;;;;;;;;;;;;;;;;;
;;      wiener      ;;
;;;;;;;;;;;;;;;;;;;;;;

%macro WIENER_H 0
%if ARCH_X86_64
cglobal wiener_filter_h, 5, 15, 16, dst, left, src, stride, fh, w, h, edge
    mov        edged, edgem
    movifnidn     wd, wm
    mov           hd, hm
%else
cglobal wiener_filter_h, 5, 7, 8, -84, dst, left, src, stride, fh, w, h, edge
    mov           r5, edgem
    mov     [esp+12], r5
    mov           wd, wm
    mov           hd, hm
    SETUP_PIC hd
 %define m15 m0
 %define m14 m1
 %define m13 m2
 %define m12 m3
%endif

    movq         m15, [fhq]
%if cpuflag(ssse3)
    pshufb       m12, m15, [PIC_sym(pb_6_7)]
    pshufb       m13, m15, [PIC_sym(pb_4)]
    pshufb       m14, m15, [PIC_sym(pb_2)]
    pshufb       m15, m15, [PIC_sym(pb_0)]
%else
    pshuflw      m12, m15, q3333
    punpcklbw    m15, m15
    pshufhw      m13, m15, q0000
    pshuflw      m14, m15, q2222
    pshuflw      m15, m15, q0000
    punpcklqdq   m12, m12
    punpckhqdq   m13, m13
    punpcklqdq   m14, m14
    punpcklqdq   m15, m15
    psraw        m13, 8
    psraw        m14, 8
    psraw        m15, 8
%endif

%if ARCH_X86_64
    mova         m11, [pw_2048]
    mova         m10, [pw_16380]
    lea          r11, [pb_right_ext_mask]

    DEFINE_ARGS dst, left, src, stride, x, w, h, edge, srcptr, dstptr, xlim
%else
 %define m10    [PIC_sym(pw_16380)]
 %define m11    [PIC_sym(pw_2048)]
 %define m12    [esp+0x14]
 %define m13    [esp+0x24]
 %define m14    [esp+0x34]
 %define m15    [esp+0x44]
    mova         m12, m3
    mova         m13, m2
    mova         m14, m1
    mova         m15, m0

    DEFINE_ARGS dst, left, src, stride, x, w, h, edge
 %define srcptrq    srcq
 %define dstptrq    dstq
 %define hd         dword [esp+ 0]
 %define edged      dword [esp+12]
 %define xlimd      dword [esp+16]
%endif

    ; if (edge & has_right) align_w_to_16
    ; else w -= 3, and use that as limit in x loop
    test       edged, 2 ; has_right
    jnz .align
    mov        xlimd, -3
    jmp .loop
.align:
    add           wd, 15
    and           wd, ~15
%if ARCH_X86_64
    xor        xlimd, xlimd
%else
    mov        xlimd, 0
%endif

    ; main y loop for vertical filter
.loop:
%if ARCH_X86_64
    mov      srcptrq, srcq
    mov      dstptrq, dstq
    lea           xd, [wq+xlimq]
%else
    mov      [esp+8], srcq
    mov      [esp+4], dstq
    mov           xd, xlimd
    add           xd, wd
%endif

    ; load left edge pixels
    test       edged, 1 ; have_left
    jz .emu_left
    test       leftq, leftq ; left == NULL for the edge-extended bottom/top
    jz .load_left_combined
    movd          m0, [leftq]
    movd          m1, [srcq]
    punpckldq     m0, m1
    pslldq        m0, 9
    add        leftq, 4
    jmp .left_load_done
.load_left_combined:
    movq          m0, [srcq-3]
    pslldq        m0, 10
    jmp .left_load_done
.emu_left:
    movd          m0, [srcq]
%if cpuflag(ssse3)
    pshufb        m0, [PIC_sym(pb_14x0_1_2)]
%else
    pslldq        m1, m0, 13
    punpcklbw     m0, m0
    pshuflw       m0, m0, q0000
    punpcklqdq    m0, m0
    psrldq        m0, 2
    por           m0, m1
%endif

    ; load right edge pixels
.left_load_done:
    cmp           xd, 16
    jg .main_load
    test          xd, xd
    jg .load_and_splat
    je .splat_right

    ; for very small images (w=[1-2]), edge-extend the original cache,
    ; ugly, but only runs in very odd cases
%if cpuflag(ssse3)
    add           wd, wd
 %if ARCH_X86_64
    pshufb        m0, [r11-pb_right_ext_mask+pb_0_to_15_min_n+wq*8-16]
 %else
    pshufb        m0, [PIC_sym(pb_0_to_15_min_n)+wq*8-16]
 %endif
    shr           wd, 1
%else
    shl           wd, 4
    pcmpeqd       m2, m2
    movd          m3, wd
    psrldq        m2, 2
    punpckhbw     m1, m0, m0
    pshufhw       m1, m1, q1122
    psllq         m1, m3
    pand          m0, m2
    pandn         m2, m1
    por           m0, m2
    shr           wd, 4
%endif

    ; main x loop, mostly this starts in .main_load
.splat_right:
    ; no need to load new pixels, just extend them from the (possibly previously
    ; extended) previous load into m0
%if cpuflag(ssse3)
    pshufb        m1, m0, [PIC_sym(pb_15)]
%else
    punpckhbw     m1, m0, m0
    pshufhw       m1, m1, q3333
    punpckhqdq    m1, m1
%endif
    jmp .main_loop
.load_and_splat:
    ; load new pixels and extend edge for right-most
    movu          m1, [srcptrq+3]
%if ARCH_X86_64
    sub          r11, xq
    movu          m2, [r11+16]
    add          r11, xq
%else
    sub      PIC_reg, xd
    movu          m2, [PIC_sym(pb_right_ext_mask)+16]
    add      PIC_reg, xd
%endif
    movd          m3, [srcptrq+2+xq]
%if cpuflag(ssse3)
    pshufb        m3, [PIC_sym(pb_0)]
%else
    punpcklbw     m3, m3
    pshuflw       m3, m3, q0000
    punpcklqdq    m3, m3
%endif
    pand          m1, m2
    pxor          m2, [PIC_sym(pb_right_ext_mask)]
    pand          m3, m2
    pxor          m2, [PIC_sym(pb_right_ext_mask)]
    por           m1, m3
    jmp .main_loop
.main_load:
    ; load subsequent line
    movu          m1, [srcptrq+3]
.main_loop:
%if ARCH_X86_64
    PALIGNR       m2, m1, m0, 10
    PALIGNR       m3, m1, m0, 11
    PALIGNR       m4, m1, m0, 12
    PALIGNR       m5, m1, m0, 13
    PALIGNR       m6, m1, m0, 14
    PALIGNR       m7, m1, m0, 15

    punpcklbw     m0, m2, m1
    punpckhbw     m2, m1
    punpcklbw     m8, m3, m7
    punpckhbw     m3, m7
    punpcklbw     m7, m4, m6
    punpckhbw     m4, m6
    PMADDUBSW     m0, m15, m6, m9, 1
    PMADDUBSW     m2, m15, m6, m9, 0
    PMADDUBSW     m8, m14, m6, m9, 0
    PMADDUBSW     m3, m14, m6, m9, 0
    PMADDUBSW     m7, m13, m6, m9, 0
    PMADDUBSW     m4, m13, m6, m9, 0
    paddw         m0, m8
    paddw         m2, m3
 %if cpuflag(ssse3)
    pxor          m6, m6
 %endif
    punpcklbw     m3, m5, m6
    punpckhbw     m5, m6
    psllw         m8, m3, 7
    psllw         m6, m5, 7
    psubw         m8, m10
    psubw         m6, m10
    pmullw        m3, m12
    pmullw        m5, m12
    paddw         m0, m7
    paddw         m2, m4
    paddw         m0, m3
    paddw         m2, m5
    paddsw        m0, m8
    paddsw        m2, m6
    psraw         m0, 3
    psraw         m2, 3
    paddw         m0, m11
    paddw         m2, m11
    mova [dstptrq+ 0], m0
    mova [dstptrq+16], m2
%else
    PALIGNR       m2, m1, m0, 10
    punpcklbw     m3, m2, m1
    punpckhbw     m2, m1
    PMADDUBSW     m3, m15, m4, m5, 1
    PMADDUBSW     m2, m15, m4, m5, 0
    PALIGNR       m4, m1, m0, 11
    PALIGNR       m5, m1, m0, 15
    punpcklbw     m6, m4, m5
    punpckhbw     m4, m5
    PMADDUBSW     m6, m14, m5, m7, 1
    PMADDUBSW     m4, m14, m5, m7, 0
    paddw         m3, m6
    paddw         m2, m4
    PALIGNR       m4, m1, m0, 12
    PALIGNR       m5, m1, m0, 14
    punpcklbw     m6, m4, m5
    punpckhbw     m4, m5
    PMADDUBSW     m6, m13, m5, m7, 1
    PMADDUBSW     m4, m13, m5, m7, 0
    paddw         m3, m6
    paddw         m2, m4
    PALIGNR       m6, m1, m0, 13
 %if cpuflag(ssse3)
    pxor          m5, m5
 %endif
    punpcklbw     m4, m6, m5
    punpckhbw     m6, m5
    psllw         m5, m4, 7
    psllw         m7, m6, 7
    psubw         m5, m10
    psubw         m7, m10
    pmullw        m4, m12
    pmullw        m6, m12
    paddw         m3, m4
    paddw         m2, m6
    paddsw        m3, m5
    paddsw        m2, m7
    psraw         m3, 3
    psraw         m2, 3
    paddw         m3, m11
    paddw         m2, m11
    mova [dstptrq+ 0], m3
    mova [dstptrq+16], m2
%endif

    mova          m0, m1
    add      srcptrq, 16
    add      dstptrq, 32
    sub           xd, 16
    cmp           xd, 16
    jg .main_load
    test          xd, xd
    jg .load_and_splat
    cmp           xd, xlimd
    jg .splat_right

%if ARCH_X86_32
    mov         srcq, [esp+8]
    mov         dstq, [esp+4]
%endif
    add         srcq, strideq
    add         dstq, 384*2
    dec           hd
    jg .loop
    RET
%endmacro

%macro WIENER_V 0
%if ARCH_X86_64
cglobal wiener_filter_v, 4, 10, 16, dst, stride, mid, w, h, fv, edge
    mov        edged, edgem
    movifnidn    fvq, fvmp
    movifnidn     hd, hm
    movq         m15, [fvq]
    pshufd       m14, m15, q1111
    pshufd       m15, m15, q0000
    paddw        m14, [pw_0_128]
    mova         m12, [pd_1024]

    DEFINE_ARGS dst, stride, mid, w, h, y, edge, ylim, mptr, dstptr

    mov        ylimd, edged
    and        ylimd, 8 ; have_bottom
    shr        ylimd, 2
    sub        ylimd, 3
%else
cglobal wiener_filter_v, 5, 7, 8, -96, dst, stride, mid, w, h, fv, edge
 %define ylimd [esp+12]

    mov          r5d, edgem
    and          r5d, 8
    shr          r5d, 2
    sub          r5d, 3
    mov        ylimd, r5d
    mov          fvq, fvmp
    mov        edged, edgem

    SETUP_PIC edged

    movq          m0, [fvq]
    pshufd        m1, m0, q1111
    pshufd        m0, m0, q0000
    paddw         m1, [PIC_sym(pw_0_128)]
    mova  [esp+0x50], m0
    mova  [esp+0x40], m1

    DEFINE_ARGS dst, stride, mid, w, h, y, edge
 %define mptrq      midq
 %define dstptrq    dstq
 %define edged      dword [esp]
%endif

    ; main x loop for vertical filter, does one column of 16 pixels
.loop_x:
    mova          m3, [midq] ; middle line

    ; load top pixels
    test       edged, 4 ; have_top
    jz .emu_top
    mova          m0, [midq-384*4]
    mova          m2, [midq-384*2]
    mova          m1, m0
    jmp .load_bottom_pixels
.emu_top:
    mova          m0, m3
    mova          m1, m3
    mova          m2, m3

    ; load bottom pixels
.load_bottom_pixels:
    mov           yd, hd
%if ARCH_X86_64
    mov        mptrq, midq
    mov      dstptrq, dstq
    add           yd, ylimd
%else
    mov      [esp+8], midq
    mov      [esp+4], dstq
    add           yd, ylimd
%endif
    jg .load_threelines

    ; the remainder here is somewhat messy but only runs in very weird
    ; circumstances at the bottom of the image in very small blocks (h=[1-3]),
    ; so performance is not terribly important here...
    je .load_twolines
    cmp           yd, -1
    je .load_oneline
    ; h == 1 case
    mova          m5, m3
    mova          m4, m3
    mova          m6, m3
    jmp .loop
.load_oneline:
    ; h == 2 case
    mova          m4, [midq+384*2]
    mova          m5, m4
    mova          m6, m4
    jmp .loop
.load_twolines:
    ; h == 3 case
    mova          m4, [midq+384*2]
    mova          m5, [midq+384*4]
    mova          m6, m5
    jmp .loop
.load_threelines:
    ; h > 3 case
    mova          m4, [midq+384*2]
    mova          m5, [midq+384*4]
    ; third line loaded in main loop below

    ; main y loop for vertical filter
.loop_load:
    ; load one line into m6. if that pixel is no longer available, do
    ; nothing, since m6 still has the data from the previous line in it. We
    ; try to structure the loop so that the common case is evaluated fastest
    mova          m6, [mptrq+384*6]
.loop:
%if ARCH_X86_64
    paddw         m7, m0, m6
    paddw         m8, m1, m5
    paddw         m9, m2, m4
    punpcklwd    m10, m7, m8
    punpckhwd     m7, m8
    punpcklwd    m11, m9, m3
    punpckhwd     m9, m3
    pmaddwd      m10, m15
    pmaddwd       m7, m15
    pmaddwd      m11, m14
    pmaddwd       m9, m14
    paddd        m10, m12
    paddd         m7, m12
    paddd        m10, m11
    paddd         m7, m9
    psrad        m10, 11
    psrad         m7, 11
    packssdw     m10, m7
    packuswb     m10, m10
    movq   [dstptrq], m10
%else
    mova  [esp+0x30], m1
    mova  [esp+0x20], m2
    mova  [esp+0x10], m3
    paddw         m0, m6
    paddw         m1, m5
    paddw         m2, m4
    punpcklwd     m7, m2, m3
    punpckhwd     m2, m3
    punpcklwd     m3, m0, m1
    punpckhwd     m0, m1
    mova          m1, [esp+0x50]
    pmaddwd       m3, m1
    pmaddwd       m0, m1
    mova          m1, [esp+0x40]
    pmaddwd       m7, m1
    pmaddwd       m2, m1
    paddd         m3, [PIC_sym(pd_1024)]
    paddd         m0, [PIC_sym(pd_1024)]
    paddd         m3, m7
    paddd         m0, m2
    psrad         m3, 11
    psrad         m0, 11
    packssdw      m3, m0
    packuswb      m3, m3
    movq      [dstq], m3
    mova          m1, [esp+0x30]
    mova          m2, [esp+0x20]
    mova          m3, [esp+0x10]
%endif
    ; shift pixels one position
    mova          m0, m1
    mova          m1, m2
    mova          m2, m3
    mova          m3, m4
    mova          m4, m5
    mova          m5, m6
    add      dstptrq, strideq
    add        mptrq, 384*2
    dec           yd
    jg .loop_load
    ; for the bottom pixels, continue using m6 (as extended edge)
    cmp           yd, ylimd
    jg .loop

%if ARCH_X86_32
    mov         midq, [esp+8]
    mov         dstq, [esp+4]
%endif
    add         dstq, 8
    add         midq, 16
    sub           wd, 8
    jg .loop_x
    RET
%endmacro

INIT_XMM sse2
WIENER_H
WIENER_V

INIT_XMM ssse3
WIENER_H
WIENER_V

;;;;;;;;;;;;;;;;;;;;;;;;;;
;;      self-guided     ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;

%macro MULLD 2
    pmulhuw       m5, %1, %2
    pmullw        %1, %2
    pslld         m5, 16
    paddd         %1, m5
%endmacro

%macro GATHERDD 2
    mova          m5, m7
    movd         r6d, %2
 %if ARCH_X86_64
    movd          %1, [r5+r6]
    pextrw       r6d, %2, 2
    pinsrw        m5, [r5+r6+2], 3
    pextrw       r6d, %2, 4
    pinsrw        %1, [r5+r6+2], 5
    pextrw       r6d, %2, 6
    pinsrw        m5, [r5+r6+2], 7
 %else
    movd          %1, [PIC_sym(sgr_x_by_x-0xF03)+r6]
    pextrw       r6d, %2, 2
    pinsrw        m5, [PIC_sym(sgr_x_by_x-0xF03)+r6+2], 3
    pextrw       r6d, %2, 4
    pinsrw        %1, [PIC_sym(sgr_x_by_x-0xF03)+r6+2], 5
    pextrw       r6d, %2, 6
    pinsrw        m5, [PIC_sym(sgr_x_by_x-0xF03)+r6+2], 7
 %endif
    por           %1, m5
%endmacro

%if ARCH_X86_64
cglobal sgr_box3_h, 5, 11, 8, sumsq, sum, left, src, stride, x, h, edge, w, xlim
    mov        xlimd, edgem
    movifnidn     xd, xm
    mov           hd, hm
    mov        edged, xlimd
    and        xlimd, 2                             ; have_right
    add           xd, xlimd
    xor        xlimd, 2                             ; 2*!have_right
%else
cglobal sgr_box3_h, 6, 7, 8, sumsq, sum, left, src, stride, x, h, edge, w, xlim
 %define wq     r0m
 %define xlimd  r1m
 %define hd     hmp
 %define edged  edgemp

    mov           r6, edgem
    and           r6, 2                             ; have_right
    add           xd, r6
    xor           r6, 2                             ; 2*!have_right
    mov        xlimd, r6
    SETUP_PIC     r6, 0
%endif

    jnz .no_right
    add           xd, 7
    and           xd, ~7
.no_right:
    pxor          m1, m1
    lea         srcq, [srcq+xq]
    lea         sumq, [sumq+xq*2-2]
    lea       sumsqq, [sumsqq+xq*4-4]
    neg           xq
    mov           wq, xq
%if ARCH_X86_64
    lea          r10, [pb_right_ext_mask+16]
%endif
.loop_y:
    mov           xq, wq

    ; load left
    test       edged, 1                             ; have_left
    jz .no_left
    test       leftq, leftq
    jz .load_left_from_main
    movd          m0, [leftq]
    pslldq        m0, 12
    add        leftq, 4
    jmp .expand_x
.no_left:
    movd          m0, [srcq+xq]
    pshufb        m0, [PIC_sym(pb_0)]
    jmp .expand_x
.load_left_from_main:
    movd          m0, [srcq+xq-2]
    pslldq        m0, 14
.expand_x:
    punpckhbw    xm0, xm1

    ; when we reach this, m0 contains left two px in highest words
    cmp           xd, -8
    jle .loop_x
.partial_load_and_extend:
    movd          m3, [srcq-4]
    pshufb        m3, [PIC_sym(pb_3)]
    movq          m2, [srcq+xq]
    punpcklbw     m2, m1
    punpcklbw     m3, m1
%if ARCH_X86_64
    movu          m4, [r10+xq*2]
%else
    movu          m4, [PIC_sym(pb_right_ext_mask+16)+xd*2]
%endif
    pand          m2, m4
    pandn         m4, m3
    por           m2, m4
    jmp .loop_x_noload
.right_extend:
    pshufb        m2, m0, [PIC_sym(pb_14_15)]
    jmp .loop_x_noload

.loop_x:
    movq          m2, [srcq+xq]
    punpcklbw     m2, m1
.loop_x_noload:
    palignr       m3, m2, m0, 12
    palignr       m4, m2, m0, 14

    punpcklwd     m5, m3, m2
    punpckhwd     m6, m3, m2
    paddw         m3, m4
    punpcklwd     m7, m4, m1
    punpckhwd     m4, m1
    pmaddwd       m5, m5
    pmaddwd       m6, m6
    pmaddwd       m7, m7
    pmaddwd       m4, m4
    paddd         m5, m7
    paddd         m6, m4
    paddw         m3, m2
    movu [sumq+xq*2], m3
    movu [sumsqq+xq*4+ 0], m5
    movu [sumsqq+xq*4+16], m6

    mova          m0, m2
    add           xq, 8

    ; if x <= -8 we can reload more pixels
    ; else if x < 0 we reload and extend (this implies have_right=0)
    ; else if x < xlimd we extend from previous load (this implies have_right=0)
    ; else we are done

    cmp           xd, -8
    jle .loop_x
    test          xd, xd
    jl .partial_load_and_extend
    cmp           xd, xlimd
    jl .right_extend

    add       sumsqq, (384+16)*4
    add         sumq, (384+16)*2
    add         srcq, strideq
    dec           hd
    jg .loop_y
    RET

%if ARCH_X86_64
cglobal sgr_box3_v, 4, 10, 9, sumsq, sum, w, h, edge, x, y, sumsq_base, sum_base, ylim
    movifnidn  edged, edgem
%else
cglobal sgr_box3_v, 5, 7, 8, -28, sumsq, sum, w, h, edge, x, y
 %define sumsq_baseq dword [esp+0]
 %define sum_baseq   dword [esp+4]
 %define ylimd       dword [esp+8]
 %define m8          [esp+12]
%endif
    mov           xq, -2
%if ARCH_X86_64
    mov        ylimd, edged
    and        ylimd, 8                             ; have_bottom
    shr        ylimd, 2
    sub        ylimd, 2                             ; -2 if have_bottom=0, else 0
    mov  sumsq_baseq, sumsqq
    mov    sum_baseq, sumq
.loop_x:
    mov       sumsqq, sumsq_baseq
    mov         sumq, sum_baseq
    lea           yd, [hd+ylimd+2]
%else
    mov           yd, edged
    and           yd, 8                             ; have_bottom
    shr           yd, 2
    sub           yd, 2                             ; -2 if have_bottom=0, else 0
    mov  sumsq_baseq, sumsqq
    mov    sum_baseq, sumq
    mov        ylimd, yd
.loop_x:
    mov       sumsqd, sumsq_baseq
    mov         sumd, sum_baseq
    lea           yd, [hd+2]
    add           yd, ylimd
%endif
    lea       sumsqq, [sumsqq+xq*4+4-(384+16)*4]
    lea         sumq, [sumq+xq*2+2-(384+16)*2]
    test       edged, 4                             ; have_top
    jnz .load_top
    movu          m0, [sumsqq+(384+16)*4*1]
    movu          m1, [sumsqq+(384+16)*4*1+16]
    mova          m2, m0
    mova          m3, m1
    mova          m4, m0
    mova          m5, m1
    movu          m6, [sumq+(384+16)*2*1]
    mova          m7, m6
    mova          m8, m6
    jmp .loop_y_noload
.load_top:
    movu          m0, [sumsqq-(384+16)*4*1]      ; l2sq [left]
    movu          m1, [sumsqq-(384+16)*4*1+16]   ; l2sq [right]
    movu          m2, [sumsqq-(384+16)*4*0]      ; l1sq [left]
    movu          m3, [sumsqq-(384+16)*4*0+16]   ; l1sq [right]
    movu          m6, [sumq-(384+16)*2*1]        ; l2
    movu          m7, [sumq-(384+16)*2*0]        ; l1
.loop_y:
%if ARCH_X86_64
    movu          m8, [sumq+(384+16)*2*1]        ; l0
%else
    movu          m4, [sumq+(384+16)*2*1]        ; l0
    mova          m8, m4
%endif
    movu          m4, [sumsqq+(384+16)*4*1]      ; l0sq [left]
    movu          m5, [sumsqq+(384+16)*4*1+16]   ; l0sq [right]
.loop_y_noload:
    paddd         m0, m2
    paddd         m1, m3
    paddw         m6, m7
    paddd         m0, m4
    paddd         m1, m5
    paddw         m6, m8
    movu [sumsqq+ 0], m0
    movu [sumsqq+16], m1
    movu      [sumq], m6

    ; shift position down by one
    mova          m0, m2
    mova          m1, m3
    mova          m2, m4
    mova          m3, m5
    mova          m6, m7
    mova          m7, m8
    add       sumsqq, (384+16)*4
    add         sumq, (384+16)*2
    dec           yd
    jg .loop_y
    cmp           yd, ylimd
    jg .loop_y_noload
    add           xd, 8
    cmp           xd, wd
    jl .loop_x
    RET

cglobal sgr_calc_ab1, 4, 7, 12, a, b, w, h, s
    movifnidn     sd, sm
    sub           aq, (384+16-1)*4
    sub           bq, (384+16-1)*2
    add           hd, 2
%if ARCH_X86_64
    LEA           r5, sgr_x_by_x-0xF03
%else
    SETUP_PIC r5, 0
%endif
    movd          m6, sd
    pshuflw       m6, m6, q0000
    punpcklqdq    m6, m6
    pxor          m7, m7
    DEFINE_ARGS a, b, w, h, x
%if ARCH_X86_64
    mova          m8, [pd_0xF00801C7]
    mova          m9, [pw_256]
    psrld        m10, m9, 13                        ; pd_2048
    mova         m11, [pb_unpcklwdw]
%else
 %define m8     [PIC_sym(pd_0xF00801C7)]
 %define m9     [PIC_sym(pw_256)]
 %define m10    [PIC_sym(pd_2048)]
 %define m11    [PIC_sym(pb_unpcklwdw)]
%endif
.loop_y:
    mov           xq, -2
.loop_x:
    movq          m0, [bq+xq*2]
    movq          m1, [bq+xq*2+(384+16)*2]
    punpcklwd     m0, m7
    punpcklwd     m1, m7
    movu          m2, [aq+xq*4]
    movu          m3, [aq+xq*4+(384+16)*4]
    pslld         m4, m2, 3
    pslld         m5, m3, 3
    paddd         m2, m4                            ; aa * 9
    paddd         m3, m5
    pmaddwd       m4, m0, m0
    pmaddwd       m5, m1, m1
    pmaddwd       m0, m8
    pmaddwd       m1, m8
    psubd         m2, m4                            ; p = aa * 9 - bb * bb
    psubd         m3, m5
    MULLD         m2, m6
    MULLD         m3, m6
    paddusw       m2, m8
    paddusw       m3, m8
    psrld         m2, 20                            ; z
    psrld         m3, 20
    GATHERDD      m4, m2                            ; xx
    GATHERDD      m2, m3
    psrld         m4, 24
    psrld         m2, 24
    packssdw      m3, m4, m2
    pshufb        m4, m11
    MULLD         m0, m4
    pshufb        m2, m11
    MULLD         m1, m2
    psubw         m5, m9, m3
    paddd         m0, m10
    paddd         m1, m10
    psrld         m0, 12
    psrld         m1, 12
    movq   [bq+xq*2], m5
    psrldq        m5, 8
    movq [bq+xq*2+(384+16)*2], m5
    movu   [aq+xq*4], m0
    movu [aq+xq*4+(384+16)*4], m1
    add           xd, 4
    cmp           xd, wd
    jl .loop_x
    add           aq, (384+16)*4*2
    add           bq, (384+16)*2*2
    sub           hd, 2
    jg .loop_y
    RET

%if ARCH_X86_64
cglobal sgr_finish_filter1, 5, 13, 16, t, src, stride, a, b, w, h, \
                                       tmp_base, src_base, a_base, b_base, x, y
    movifnidn     wd, wm
    mov           hd, hm
    mova         m15, [pw_16]
    mov    tmp_baseq, tq
    mov    src_baseq, srcq
    mov      a_baseq, aq
    mov      b_baseq, bq
    xor           xd, xd
%else
cglobal sgr_finish_filter1, 7, 7, 8, -144, t, src, stride, a, b, x, y
 %define tmp_baseq  [esp+8]
 %define src_baseq  [esp+12]
 %define a_baseq    [esp+16]
 %define b_baseq    [esp+20]
 %define wd         [esp+24]
 %define hd         [esp+28]
    mov    tmp_baseq, tq
    mov    src_baseq, srcq
    mov      a_baseq, aq
    mov      b_baseq, bq
    mov           wd, xd
    mov           hd, yd
    xor           xd, xd
    SETUP_PIC yd, 1, 1
    jmp .loop_start
%endif

.loop_x:
    mov           tq, tmp_baseq
    mov         srcq, src_baseq
    mov           aq, a_baseq
    mov           bq, b_baseq
%if ARCH_X86_32
.loop_start:
    movu          m0, [bq+xq*2-(384+16)*2-2]
    movu          m2, [bq+xq*2-(384+16)*2+2]
    mova          m1, [bq+xq*2-(384+16)*2]          ; b:top
    paddw         m0, m2                            ; b:tl+tr
    movu          m2, [bq+xq*2-2]
    movu          m3, [bq+xq*2+2]
    paddw         m1, [bq+xq*2]                     ; b:top+ctr
    paddw         m2, m3                            ; b:l+r
    mova  [esp+0x80], m0
    mova  [esp+0x70], m1
    mova  [esp+0x60], m2
%endif
    movu          m0, [aq+xq*4-(384+16)*4-4]
    movu          m2, [aq+xq*4-(384+16)*4+4]
    mova          m1, [aq+xq*4-(384+16)*4]          ; a:top [first half]
    paddd         m0, m2                            ; a:tl+tr [first half]
    movu          m2, [aq+xq*4-(384+16)*4-4+16]
    movu          m4, [aq+xq*4-(384+16)*4+4+16]
    mova          m3, [aq+xq*4-(384+16)*4+16]       ; a:top [second half]
    paddd         m2, m4                            ; a:tl+tr [second half]
    movu          m4, [aq+xq*4-4]
    movu          m5, [aq+xq*4+4]
    paddd         m1, [aq+xq*4]                     ; a:top+ctr [first half]
    paddd         m4, m5                            ; a:l+r [first half]
    movu          m5, [aq+xq*4+16-4]
    movu          m6, [aq+xq*4+16+4]
    paddd         m3, [aq+xq*4+16]                  ; a:top+ctr [second half]
    paddd         m5, m6                            ; a:l+r [second half]
%if ARCH_X86_64
    movu          m6, [bq+xq*2-(384+16)*2-2]
    movu          m8, [bq+xq*2-(384+16)*2+2]
    mova          m7, [bq+xq*2-(384+16)*2]          ; b:top
    paddw         m6, m8                            ; b:tl+tr
    movu          m8, [bq+xq*2-2]
    movu          m9, [bq+xq*2+2]
    paddw         m7, [bq+xq*2]                     ; b:top+ctr
    paddw         m8, m9                            ; b:l+r
%endif

    lea           tq, [tq+xq*2]
    lea         srcq, [srcq+xq*1]
    lea           aq, [aq+xq*4+(384+16)*4]
    lea           bq, [bq+xq*2+(384+16)*2]
    mov           yd, hd
.loop_y:
%if ARCH_X86_64
    movu          m9, [bq-2]
    movu         m10, [bq+2]
    paddw         m7, [bq]                          ; b:top+ctr+bottom
    paddw         m9, m10                           ; b:bl+br
    paddw        m10, m7, m8                        ; b:top+ctr+bottom+l+r
    paddw         m6, m9                            ; b:tl+tr+bl+br
    psubw         m7, [bq-(384+16)*2*2]             ; b:ctr+bottom
    paddw        m10, m6
    psllw        m10, 2
    psubw        m10, m6                            ; aa
    pxor         m14, m14
    movq         m12, [srcq]
    punpcklbw    m12, m14
    punpcklwd     m6, m10, m15
    punpckhwd    m10, m15
    punpcklwd    m13, m12, m15
    punpckhwd    m12, m15
    pmaddwd       m6, m13                           ; aa*src[x]+256 [first half]
    pmaddwd      m10, m12                           ; aa*src[x]+256 [second half]
%else
    paddd         m1, [aq]                          ; a:top+ctr+bottom [first half]
    paddd         m3, [aq+16]                       ; a:top+ctr+bottom [second half]
    mova  [esp+0x50], m1
    mova  [esp+0x40], m3
    mova  [esp+0x30], m4
    movu          m6, [aq-4]
    movu          m7, [aq+4]
    paddd         m1, m4                            ; a:top+ctr+bottom+l+r [first half]
    paddd         m3, m5                            ; a:top+ctr+bottom+l+r [second half]
    paddd         m6, m7                            ; a:bl+br [first half]
    movu          m7, [aq+16-4]
    movu          m4, [aq+16+4]
    paddd         m7, m4                            ; a:bl+br [second half]
    paddd         m0, m6                            ; a:tl+tr+bl+br [first half]
    paddd         m2, m7                            ; a:tl+tr+bl+br [second half]
    paddd         m1, m0
    paddd         m3, m2
    pslld         m1, 2
    pslld         m3, 2
    psubd         m1, m0                            ; bb [first half]
    psubd         m3, m2                            ; bb [second half]
%endif

%if ARCH_X86_64
    movu         m11, [aq-4]
    movu         m12, [aq+4]
    paddd         m1, [aq]                          ; a:top+ctr+bottom [first half]
    paddd        m11, m12                           ; a:bl+br [first half]
    movu         m12, [aq+16-4]
    movu         m13, [aq+16+4]
    paddd         m3, [aq+16]                       ; a:top+ctr+bottom [second half]
    paddd        m12, m13                           ; a:bl+br [second half]
    paddd        m13, m1, m4                        ; a:top+ctr+bottom+l+r [first half]
    paddd        m14, m3, m5                        ; a:top+ctr+bottom+l+r [second half]
    paddd         m0, m11                           ; a:tl+tr+bl+br [first half]
    paddd         m2, m12                           ; a:tl+tr+bl+br [second half]
    paddd        m13, m0
    paddd        m14, m2
    pslld        m13, 2
    pslld        m14, 2
    psubd        m13, m0                            ; bb [first half]
    psubd        m14, m2                            ; bb [second half]
    psubd         m1, [aq-(384+16)*4*2]             ; a:ctr+bottom [first half]
    psubd         m3, [aq-(384+16)*4*2+16]          ; a:ctr+bottom [second half]
%else
    mova          m4, [esp+0x80]
    mova  [esp+0x80], m5
    mova          m5, [esp+0x70]
    mova  [esp+0x70], m6
    mova          m6, [esp+0x60]
    mova  [esp+0x60], m7
    mova  [esp+0x20], m1
    movu          m7, [bq-2]
    movu          m1, [bq+2]
    paddw         m5, [bq]                          ; b:top+ctr+bottom
    paddw         m7, m1
    paddw         m1, m5, m6                        ; b:top+ctr+bottom+l+r
    paddw         m4, m7                            ; b:tl+tr+bl+br
    psubw         m5, [bq-(384+16)*2*2]             ; b:ctr+bottom
    paddw         m1, m4
    psllw         m1, 2
    psubw         m1, m4                            ; aa
    movq          m0, [srcq]
    XCHG_PIC_REG
    punpcklbw     m0, [PIC_sym(pb_right_ext_mask)+16]
    punpcklwd     m4, m1, [PIC_sym(pw_16)]
    punpckhwd     m1, [PIC_sym(pw_16)]
    punpcklwd     m2, m0, [PIC_sym(pw_16)]
    punpckhwd     m0, [PIC_sym(pw_16)]
    XCHG_PIC_REG
    pmaddwd       m4, m2                            ; aa*src[x]+256 [first half]
    pmaddwd       m1, m0                            ; aa*src[x]+256 [second half]
%endif

%if ARCH_X86_64
    paddd         m6, m13
    paddd        m10, m14
    psrad         m6, 9
    psrad        m10, 9
    packssdw      m6, m10
    mova        [tq], m6
%else
    paddd         m4, [esp+0x20]
    paddd         m1, m3
    psrad         m4, 9
    psrad         m1, 9
    packssdw      m4, m1
    mova        [tq], m4
%endif

    ; shift to next row
%if ARCH_X86_64
    mova          m0, m4
    mova          m2, m5
    mova          m4, m11
    mova          m5, m12
    mova          m6, m8
    mova          m8, m9
%else
    mova          m1, [esp+0x50]
    mova          m3, [esp+0x40]
    mova          m0, [esp+0x30]
    mova          m2, [esp+0x80]
    mova          m4, [esp+0x70]
    mova  [esp+0x70], m5
    mova          m5, [esp+0x60]
    mova  [esp+0x80], m6
    mova  [esp+0x60], m7
    psubd         m1, [aq-(384+16)*4*2]             ; a:ctr+bottom [first half]
    psubd         m3, [aq-(384+16)*4*2+16]          ; a:ctr+bottom [second half]
%endif

    add           aq, (384+16)*4
    add           bq, (384+16)*2
    add           tq, 384*2
    add         srcq, strideq
    dec           yd
    jg .loop_y
    add           xd, 8
    cmp           xd, wd
    jl .loop_x
    RET

cglobal sgr_weighted1, 4, 7, 8, dst, stride, t, w, h, wt
    movifnidn     hd, hm
%if ARCH_X86_32
    SETUP_PIC r6, 0
%endif
    movd          m0, wtm
    pshufb        m0, [PIC_sym(pb_0_1)]
    psllw         m0, 4
    pxor          m7, m7
    DEFINE_ARGS dst, stride, t, w, h, idx
.loop_y:
    xor         idxd, idxd
.loop_x:
    mova          m1, [tq+idxq*2+ 0]
    mova          m4, [tq+idxq*2+16]
    mova          m5, [dstq+idxq]
    punpcklbw     m2, m5, m7
    punpckhbw     m5, m7
    psllw         m3, m2, 4
    psllw         m6, m5, 4
    psubw         m1, m3
    psubw         m4, m6
    pmulhrsw      m1, m0
    pmulhrsw      m4, m0
    paddw         m1, m2
    paddw         m4, m5
    packuswb      m1, m4
    mova [dstq+idxq], m1
    add         idxd, 16
    cmp         idxd, wd
    jl .loop_x
    add         dstq, strideq
    add           tq, 384 * 2
    dec           hd
    jg .loop_y
    RET

%if ARCH_X86_64
cglobal sgr_box5_h, 5, 11, 12, sumsq, sum, left, src, stride, w, h, edge, x, xlim
    mov        edged, edgem
    movifnidn     wd, wm
    mov           hd, hm
    mova         m10, [pb_0]
    mova         m11, [pb_0_1]
%else
cglobal sgr_box5_h, 7, 7, 8, sumsq, sum, left, src, xlim, x, h, edge
 %define edged      edgemp
 %define wd         xd
 %define wq         wd
 %define wm         r5m
 %define strideq    r4m
    SUB          esp, 8
    SETUP_PIC sumsqd, 1, 1

 %define m10    [PIC_sym(pb_0)]
 %define m11    [PIC_sym(pb_0_1)]
%endif

    test       edged, 2                             ; have_right
    jz .no_right
    xor        xlimd, xlimd
    add           wd, 2
    add           wd, 15
    and           wd, ~15
    jmp .right_done
.no_right:
    mov        xlimd, 3
    dec           wd
.right_done:
    pxor          m1, m1
    lea         srcq, [srcq+wq+1]
    lea         sumq, [sumq+wq*2-2]
    lea       sumsqq, [sumsqq+wq*4-4]
    neg           wq
%if ARCH_X86_64
    lea          r10, [pb_right_ext_mask+16]
%else
    mov           wm, xd
 %define wq wm
%endif

.loop_y:
    mov           xq, wq
    ; load left
    test       edged, 1                             ; have_left
    jz .no_left
    test       leftq, leftq
    jz .load_left_from_main
    movd          m0, [leftq]
    movd          m2, [srcq+xq-1]
    pslldq        m2, 4
    por           m0, m2
    pslldq        m0, 11
    add        leftq, 4
    jmp .expand_x
.no_left:
    movd          m0, [srcq+xq-1]
    XCHG_PIC_REG
    pshufb        m0, m10
    XCHG_PIC_REG
    jmp .expand_x
.load_left_from_main:
    movd          m0, [srcq+xq-4]
    pslldq        m0, 12
.expand_x:
    punpckhbw     m0, m1

    ; when we reach this, m0 contains left two px in highest words
    cmp           xd, -8
    jle .loop_x
    test          xd, xd
    jge .right_extend
.partial_load_and_extend:
    XCHG_PIC_REG
    movd          m3, [srcq-1]
    movq          m2, [srcq+xq]
    pshufb        m3, m10
    punpcklbw     m3, m1
    punpcklbw     m2, m1
%if ARCH_X86_64
    movu          m4, [r10+xq*2]
%else
    movu          m4, [PIC_sym(pb_right_ext_mask+16)+xd*2]
    XCHG_PIC_REG
%endif
    pand          m2, m4
    pandn         m4, m3
    por           m2, m4
    jmp .loop_x_noload
.right_extend:
    psrldq        m2, m0, 14
    XCHG_PIC_REG
    pshufb        m2, m11
    XCHG_PIC_REG
    jmp .loop_x_noload

.loop_x:
    movq          m2, [srcq+xq]
    punpcklbw     m2, m1
.loop_x_noload:
    palignr       m3, m2, m0, 8
    palignr       m4, m2, m0, 10
    palignr       m5, m2, m0, 12
    palignr       m6, m2, m0, 14

%if ARCH_X86_64
    paddw         m0, m3, m2
    punpcklwd     m7, m3, m2
    punpckhwd     m3, m2
    paddw         m0, m4
    punpcklwd     m8, m4, m5
    punpckhwd     m4, m5
    paddw         m0, m5
    punpcklwd     m9, m6, m1
    punpckhwd     m5, m6, m1
    paddw         m0, m6
    pmaddwd       m7, m7
    pmaddwd       m3, m3
    pmaddwd       m8, m8
    pmaddwd       m4, m4
    pmaddwd       m9, m9
    pmaddwd       m5, m5
    paddd         m7, m8
    paddd         m3, m4
    paddd         m7, m9
    paddd         m3, m5
    movu [sumq+xq*2], m0
    movu [sumsqq+xq*4+ 0], m7
    movu [sumsqq+xq*4+16], m3
%else
    paddw         m0, m3, m2
    paddw         m0, m4
    paddw         m0, m5
    paddw         m0, m6
    movu [sumq+xq*2], m0
    punpcklwd     m7, m3, m2
    punpckhwd     m3, m2
    punpcklwd     m0, m4, m5
    punpckhwd     m4, m5
    punpckhwd     m5, m6, m1
    pmaddwd       m7, m7
    pmaddwd       m3, m3
    pmaddwd       m0, m0
    pmaddwd       m4, m4
    pmaddwd       m5, m5
    paddd         m7, m0
    paddd         m3, m4
    paddd         m3, m5
    punpcklwd     m0, m6, m1
    pmaddwd       m0, m0
    paddd         m7, m0
    movu [sumsqq+xq*4+ 0], m7
    movu [sumsqq+xq*4+16], m3
%endif

    mova          m0, m2
    add           xq, 8

    ; if x <= -8 we can reload more pixels
    ; else if x < 0 we reload and extend (this implies have_right=0)
    ; else if x < xlimd we extend from previous load (this implies have_right=0)
    ; else we are done

    cmp           xd, -8
    jle .loop_x
    test          xd, xd
    jl .partial_load_and_extend
    cmp           xd, xlimd
    jl .right_extend

    add       sumsqq, (384+16)*4
    add         sumq, (384+16)*2
    add         srcq, strideq
    dec           hd
    jg .loop_y
%if ARCH_X86_32
    ADD          esp, 8
%endif
    RET

%if ARCH_X86_64
cglobal sgr_box5_v, 4, 10, 15, sumsq, sum, w, h, edge, x, y, sumsq_ptr, sum_ptr, ylim
    movifnidn  edged, edgem
    mov        ylimd, edged
%else
cglobal sgr_box5_v, 5, 7, 8, -44, sumsq, sum, x, y, ylim, sumsq_ptr, sum_ptr
 %define wm     [esp+0]
 %define hm     [esp+4]
 %define edgem  [esp+8]
    mov           wm, xd
    mov           hm, yd
    mov        edgem, ylimd
%endif

    and        ylimd, 8                             ; have_bottom
    shr        ylimd, 2
    sub        ylimd, 3                             ; -3 if have_bottom=0, else -1
    mov           xq, -2
%if ARCH_X86_64
.loop_x:
    lea           yd, [hd+ylimd+2]
    lea   sumsq_ptrq, [sumsqq+xq*4+4-(384+16)*4]
    lea     sum_ptrq, [  sumq+xq*2+2-(384+16)*2]
    test       edged, 4                             ; have_top
    jnz .load_top
    movu          m0, [sumsq_ptrq+(384+16)*4*1]
    movu          m1, [sumsq_ptrq+(384+16)*4*1+16]
    mova          m2, m0
    mova          m3, m1
    mova          m4, m0
    mova          m5, m1
    mova          m6, m0
    mova          m7, m1
    movu         m10, [sum_ptrq+(384+16)*2*1]
    mova         m11, m10
    mova         m12, m10
    mova         m13, m10
    jmp .loop_y_second_load
.load_top:
    movu          m0, [sumsq_ptrq-(384+16)*4*1]      ; l3/4sq [left]
    movu          m1, [sumsq_ptrq-(384+16)*4*1+16]   ; l3/4sq [right]
    movu          m4, [sumsq_ptrq-(384+16)*4*0]      ; l2sq [left]
    movu          m5, [sumsq_ptrq-(384+16)*4*0+16]   ; l2sq [right]
    mova          m2, m0
    mova          m3, m1
    movu         m10, [sum_ptrq-(384+16)*2*1]        ; l3/4
    movu         m12, [sum_ptrq-(384+16)*2*0]        ; l2
    mova         m11, m10
.loop_y:
    movu          m6, [sumsq_ptrq+(384+16)*4*1]      ; l1sq [left]
    movu          m7, [sumsq_ptrq+(384+16)*4*1+16]   ; l1sq [right]
    movu         m13, [sum_ptrq+(384+16)*2*1]        ; l1
.loop_y_second_load:
    test          yd, yd
    jle .emulate_second_load
    movu          m8, [sumsq_ptrq+(384+16)*4*2]      ; l0sq [left]
    movu          m9, [sumsq_ptrq+(384+16)*4*2+16]   ; l0sq [right]
    movu         m14, [sum_ptrq+(384+16)*2*2]        ; l0
.loop_y_noload:
    paddd         m0, m2
    paddd         m1, m3
    paddw        m10, m11
    paddd         m0, m4
    paddd         m1, m5
    paddw        m10, m12
    paddd         m0, m6
    paddd         m1, m7
    paddw        m10, m13
    paddd         m0, m8
    paddd         m1, m9
    paddw        m10, m14
    movu [sumsq_ptrq+ 0], m0
    movu [sumsq_ptrq+16], m1
    movu  [sum_ptrq], m10

    ; shift position down by one
    mova          m0, m4
    mova          m1, m5
    mova          m2, m6
    mova          m3, m7
    mova          m4, m8
    mova          m5, m9
    mova         m10, m12
    mova         m11, m13
    mova         m12, m14
    add   sumsq_ptrq, (384+16)*4*2
    add     sum_ptrq, (384+16)*2*2
    sub           yd, 2
    jge .loop_y
    ; l1 = l0
    mova          m6, m8
    mova          m7, m9
    mova         m13, m14
    cmp           yd, ylimd
    jg .loop_y_noload
    add           xd, 8
    cmp           xd, wd
    jl .loop_x
    RET
.emulate_second_load:
    mova          m8, m6
    mova          m9, m7
    mova         m14, m13
    jmp .loop_y_noload
%else
.sumsq_loop_x:
    lea           yd, [ylimd+2]
    add           yd, hm
    lea   sumsq_ptrq, [sumsqq+xq*4+4-(384+16)*4]
    test dword edgem, 4                             ; have_top
    jnz .sumsq_load_top
    movu          m0, [sumsq_ptrq+(384+16)*4*1]
    movu          m1, [sumsq_ptrq+(384+16)*4*1+16]
    mova          m4, m0
    mova          m5, m1
    mova          m6, m0
    mova          m7, m1
    mova  [esp+0x1c], m0
    mova  [esp+0x0c], m1
    jmp .sumsq_loop_y_second_load
.sumsq_load_top:
    movu          m0, [sumsq_ptrq-(384+16)*4*1]      ; l3/4sq [left]
    movu          m1, [sumsq_ptrq-(384+16)*4*1+16]   ; l3/4sq [right]
    movu          m4, [sumsq_ptrq-(384+16)*4*0]      ; l2sq [left]
    movu          m5, [sumsq_ptrq-(384+16)*4*0+16]   ; l2sq [right]
    mova  [esp+0x1c], m0
    mova  [esp+0x0c], m1
.sumsq_loop_y:
    movu          m6, [sumsq_ptrq+(384+16)*4*1]      ; l1sq [left]
    movu          m7, [sumsq_ptrq+(384+16)*4*1+16]   ; l1sq [right]
.sumsq_loop_y_second_load:
    test          yd, yd
    jle .sumsq_emulate_second_load
    movu          m2, [sumsq_ptrq+(384+16)*4*2]      ; l0sq [left]
    movu          m3, [sumsq_ptrq+(384+16)*4*2+16]   ; l0sq [right]
.sumsq_loop_y_noload:
    paddd         m0, [esp+0x1c]
    paddd         m1, [esp+0x0c]
    paddd         m0, m4
    paddd         m1, m5
    paddd         m0, m6
    paddd         m1, m7
    paddd         m0, m2
    paddd         m1, m3
    movu [sumsq_ptrq+ 0], m0
    movu [sumsq_ptrq+16], m1

    ; shift position down by one
    mova          m0, m4
    mova          m1, m5
    mova          m4, m2
    mova          m5, m3
    mova  [esp+0x1c], m6
    mova  [esp+0x0c], m7
    add   sumsq_ptrq, (384+16)*4*2
    sub           yd, 2
    jge .sumsq_loop_y
    ; l1 = l0
    mova          m6, m2
    mova          m7, m3
    cmp           yd, ylimd
    jg .sumsq_loop_y_noload
    add           xd, 8
    cmp           xd, wm
    jl .sumsq_loop_x

    mov           xd, -2
.sum_loop_x:
    lea           yd, [ylimd+2]
    add           yd, hm
    lea     sum_ptrq, [sumq+xq*2+2-(384+16)*2]
    test dword edgem, 4                             ; have_top
    jnz .sum_load_top
    movu          m0, [sum_ptrq+(384+16)*2*1]
    mova          m1, m0
    mova          m2, m0
    mova          m3, m0
    jmp .sum_loop_y_second_load
.sum_load_top:
    movu          m0, [sum_ptrq-(384+16)*2*1]        ; l3/4
    movu          m2, [sum_ptrq-(384+16)*2*0]        ; l2
    mova          m1, m0
.sum_loop_y:
    movu          m3, [sum_ptrq+(384+16)*2*1]        ; l1
.sum_loop_y_second_load:
    test          yd, yd
    jle .sum_emulate_second_load
    movu          m4, [sum_ptrq+(384+16)*2*2]        ; l0
.sum_loop_y_noload:
    paddw         m0, m1
    paddw         m0, m2
    paddw         m0, m3
    paddw         m0, m4
    movu  [sum_ptrq], m0

    ; shift position down by one
    mova          m0, m2
    mova          m1, m3
    mova          m2, m4
    add     sum_ptrq, (384+16)*2*2
    sub           yd, 2
    jge .sum_loop_y
    ; l1 = l0
    mova          m3, m4
    cmp           yd, ylimd
    jg .sum_loop_y_noload
    add           xd, 8
    cmp           xd, wm
    jl .sum_loop_x
    RET
.sumsq_emulate_second_load:
    mova          m2, m6
    mova          m3, m7
    jmp .sumsq_loop_y_noload
.sum_emulate_second_load:
    mova          m4, m3
    jmp .sum_loop_y_noload
%endif

cglobal sgr_calc_ab2, 4, 7, 11, a, b, w, h, s
    movifnidn     sd, sm
    sub           aq, (384+16-1)*4
    sub           bq, (384+16-1)*2
    add           hd, 2
%if ARCH_X86_64
    LEA           r5, sgr_x_by_x-0xF03
%else
    SETUP_PIC r5, 0
%endif
    movd          m6, sd
    pshuflw       m6, m6, q0000
    punpcklqdq    m6, m6
    pxor          m7, m7
    DEFINE_ARGS a, b, w, h, x
%if ARCH_X86_64
    mova          m8, [pd_0xF0080029]
    mova          m9, [pw_256]
    psrld        m10, m9, 15                        ; pd_512
%else
 %define m8     [PIC_sym(pd_0xF0080029)]
 %define m9     [PIC_sym(pw_256)]
 %define m10    [PIC_sym(pd_512)]
%endif
.loop_y:
    mov           xq, -2
.loop_x:
    movq          m0, [bq+xq*2+0]
    movq          m1, [bq+xq*2+8]
    punpcklwd     m0, m7
    punpcklwd     m1, m7
    movu          m2, [aq+xq*4+ 0]
    movu          m3, [aq+xq*4+16]
    pslld         m4, m2, 3                         ; aa * 8
    pslld         m5, m3, 3
    paddd         m2, m4                            ; aa * 9
    paddd         m3, m5
    paddd         m4, m4                            ; aa * 16
    paddd         m5, m5
    paddd         m2, m4                            ; aa * 25
    paddd         m3, m5
    pmaddwd       m4, m0, m0
    pmaddwd       m5, m1, m1
    psubd         m2, m4                            ; p = aa * 25 - bb * bb
    psubd         m3, m5
    MULLD         m2, m6
    MULLD         m3, m6
    paddusw       m2, m8
    paddusw       m3, m8
    psrld         m2, 20                            ; z
    psrld         m3, 20
    GATHERDD      m4, m2                            ; xx
    GATHERDD      m2, m3
    psrld         m4, 24
    psrld         m2, 24
    packssdw      m3, m4, m2
    pmullw        m4, m8
    pmullw        m2, m8
    psubw         m5, m9, m3
    pmaddwd       m0, m4
    pmaddwd       m1, m2
    paddd         m0, m10
    paddd         m1, m10
    psrld         m0, 10
    psrld         m1, 10
    movu   [bq+xq*2], m5
    movu [aq+xq*4+ 0], m0
    movu [aq+xq*4+16], m1
    add           xd, 8
    cmp           xd, wd
    jl .loop_x
    add           aq, (384+16)*4*2
    add           bq, (384+16)*2*2
    sub           hd, 2
    jg .loop_y
    RET

%if ARCH_X86_64
cglobal sgr_finish_filter2, 5, 13, 14, t, src, stride, a, b, w, h, \
                                       tmp_base, src_base, a_base, b_base, x, y
    movifnidn     wd, wm
    mov           hd, hm
    mov    tmp_baseq, tq
    mov    src_baseq, srcq
    mov      a_baseq, aq
    mov      b_baseq, bq
    mova          m9, [pw_5_6]
    mova         m12, [pw_256]
    psrlw        m10, m12, 8                    ; pw_1
    psrlw        m11, m12, 1                    ; pw_128
    pxor         m13, m13
%else
cglobal sgr_finish_filter2, 6, 7, 8, t, src, stride, a, b, x, y
 %define tmp_baseq  r0m
 %define src_baseq  r1m
 %define a_baseq    r3m
 %define b_baseq    r4m
 %define wd         r5m
 %define hd         r6m

    SUB          esp, 8
    SETUP_PIC yd

 %define m8     m5
 %define m9     [PIC_sym(pw_5_6)]
 %define m10    [PIC_sym(pw_1)]
 %define m11    [PIC_sym(pw_128)]
 %define m12    [PIC_sym(pw_256)]
 %define m13    m0
%endif
    xor           xd, xd
.loop_x:
    mov           tq, tmp_baseq
    mov         srcq, src_baseq
    mov           aq, a_baseq
    mov           bq, b_baseq
    movu          m0, [aq+xq*4-(384+16)*4-4]
    mova          m1, [aq+xq*4-(384+16)*4]
    movu          m2, [aq+xq*4-(384+16)*4+4]
    movu          m3, [aq+xq*4-(384+16)*4-4+16]
    mova          m4, [aq+xq*4-(384+16)*4+16]
    movu          m5, [aq+xq*4-(384+16)*4+4+16]
    paddd         m0, m2
    paddd         m3, m5
    paddd         m0, m1
    paddd         m3, m4
    pslld         m2, m0, 2
    pslld         m5, m3, 2
    paddd         m2, m0
    paddd         m5, m3
    paddd         m0, m2, m1                    ; prev_odd_b [first half]
    paddd         m1, m5, m4                    ; prev_odd_b [second half]
    movu          m3, [bq+xq*2-(384+16)*2-2]
    mova          m4, [bq+xq*2-(384+16)*2]
    movu          m5, [bq+xq*2-(384+16)*2+2]
    paddw         m3, m5
    punpcklwd     m5, m3, m4
    punpckhwd     m3, m4
    pmaddwd       m5, m9
    pmaddwd       m3, m9
    mova          m2, m5
    packssdw      m2, m3                        ; prev_odd_a
    lea           tq, [tq+xq*2]
    lea         srcq, [srcq+xq*1]
    lea           aq, [aq+xq*4+(384+16)*4]
    lea           bq, [bq+xq*2+(384+16)*2]
%if ARCH_X86_32
    mov        [esp], PIC_reg
%endif
    mov           yd, hd
    XCHG_PIC_REG
.loop_y:
    movu          m3, [aq-4]
    mova          m4, [aq]
    movu          m5, [aq+4]
    paddd         m3, m5
    paddd         m3, m4
    pslld         m5, m3, 2
    paddd         m5, m3
    paddd         m5, m4                        ; cur_odd_b [first half]
    movu          m3, [aq+16-4]
    mova          m6, [aq+16]
    movu          m7, [aq+16+4]
    paddd         m3, m7
    paddd         m3, m6
    pslld         m7, m3, 2
    paddd         m7, m3
    paddd         m4, m7, m6                    ; cur_odd_b [second half]
    movu          m3, [bq-2]
    mova          m6, [bq]
    movu          m7, [bq+2]
    paddw         m3, m7
    punpcklwd     m7, m3, m6
    punpckhwd     m3, m6
    pmaddwd       m7, m9
    pmaddwd       m3, m9
    packssdw      m6, m7, m3                    ; cur_odd_a

    paddd         m0, m5                        ; cur_even_b [first half]
    paddd         m1, m4                        ; cur_even_b [second half]
    paddw         m2, m6                        ; cur_even_a

    movq          m3, [srcq]
%if ARCH_X86_64
    punpcklbw     m3, m13
%else
    mova        [td], m5
    pxor          m7, m7
    punpcklbw     m3, m7
%endif
    punpcklwd     m7, m3, m10
    punpckhwd     m3, m10
    punpcklwd     m8, m2, m12
    punpckhwd     m2, m12
    pmaddwd       m7, m8
    pmaddwd       m3, m2
    paddd         m7, m0
    paddd         m3, m1
    psrad         m7, 9
    psrad         m3, 9

%if ARCH_X86_32
    pxor         m13, m13
%endif
    movq          m8, [srcq+strideq]
    punpcklbw     m8, m13
    punpcklwd     m0, m8, m10
    punpckhwd     m8, m10
    punpcklwd     m1, m6, m11
    punpckhwd     m2, m6, m11
    pmaddwd       m0, m1
    pmaddwd       m8, m2
%if ARCH_X86_64
    paddd         m0, m5
%else
    paddd         m0, [td]
%endif
    paddd         m8, m4
    psrad         m0, 8
    psrad         m8, 8

    packssdw      m7, m3
    packssdw      m0, m8
%if ARCH_X86_32
    mova          m5, [td]
%endif
    mova [tq+384*2*0], m7
    mova [tq+384*2*1], m0

    mova          m0, m5
    mova          m1, m4
    mova          m2, m6
    add           aq, (384+16)*4*2
    add           bq, (384+16)*2*2
    add           tq, 384*2*2
    lea         srcq, [srcq+strideq*2]
%if ARCH_X86_64
    sub           yd, 2
%else
    sub dword [esp+4], 2
%endif
    jg .loop_y
    add           xd, 8
    cmp           xd, wd
    jl .loop_x
%if ARCH_X86_32
    ADD          esp, 8
%endif
    RET

cglobal sgr_weighted2, 4, 7, 12, dst, stride, t1, t2, w, h, wt
    movifnidn     wd, wm
    mov          wtq, wtmp
%if ARCH_X86_64
    movifnidn     hd, hm
    mova         m10, [pd_1024]
    pxor         m11, m11
%else
    SETUP_PIC     hd, 0
 %define m10    [PIC_sym(pd_1024)]
 %define m11    m7
%endif
    movd          m0, [wtq]
    pshufd        m0, m0, 0
    DEFINE_ARGS dst, stride, t1, t2, w, h, idx
%if ARCH_X86_32
 %define hd     hmp
%endif

.loop_y:
    xor         idxd, idxd
.loop_x:
    mova          m1, [t1q+idxq*2+ 0]
    mova          m2, [t1q+idxq*2+16]
    mova          m3, [t2q+idxq*2+ 0]
    mova          m4, [t2q+idxq*2+16]
    mova          m6, [dstq+idxq]
%if ARCH_X86_32
    pxor          m11, m11
%endif
    punpcklbw     m5, m6, m11
    punpckhbw     m6, m11
    psllw         m7, m5, 4
    psubw         m1, m7
    psubw         m3, m7
    psllw         m7, m6, 4
    psubw         m2, m7
    psubw         m4, m7
    punpcklwd     m7, m1, m3
    punpckhwd     m1, m3
    punpcklwd     m3, m2, m4
    punpckhwd     m2, m4
    pmaddwd       m7, m0
    pmaddwd       m1, m0
    pmaddwd       m3, m0
    pmaddwd       m2, m0
    paddd         m7, m10
    paddd         m1, m10
    paddd         m3, m10
    paddd         m2, m10
    psrad         m7, 11
    psrad         m1, 11
    psrad         m3, 11
    psrad         m2, 11
    packssdw      m7, m1
    packssdw      m3, m2
    paddw         m7, m5
    paddw         m3, m6
    packuswb      m7, m3
    mova [dstq+idxq], m7
    add         idxd, 16
    cmp         idxd, wd
    jl .loop_x
    add         dstq, strideq
    add          t1q, 384 * 2
    add          t2q, 384 * 2
    dec           hd
    jg .loop_y
    RET
