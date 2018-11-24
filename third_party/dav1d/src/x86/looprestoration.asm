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
pb_right_ext_mask: times 32 db 0xff
                   times 32 db 0
pb_14x0_1_2: times 14 db 0
             db 1, 2
pb_0_to_15_min_n: db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13
                  db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 14
pb_15: times 16 db 15
pw_2048: times 2 dw 2048
pw_16380: times 2 dw 16380
pw_0_128: dw 0, 128
pd_1024: dd 1024

SECTION .text

INIT_YMM avx2
cglobal wiener_filter_h, 8, 12, 16, dst, left, src, stride, fh, w, h, edge
    vpbroadcastb m15, [fhq+0]
    vpbroadcastb m14, [fhq+2]
    vpbroadcastb m13, [fhq+4]
    vpbroadcastw m12, [fhq+6]
    vpbroadcastd m11, [pw_2048]
    vpbroadcastd m10, [pw_16380]
    lea          r11, [pb_right_ext_mask]

    DEFINE_ARGS dst, left, src, stride, x, w, h, edge, srcptr, dstptr, xlim

    ; if (edge & has_right) align_w_to_32
    ; else w -= 32, and use that as limit in x loop
    test       edged, 2 ; has_right
    jnz .align
    mov        xlimq, -3
    jmp .loop
.align:
    add           wd, 31
    and           wd, ~31
    xor        xlimd, xlimd

    ; main y loop for vertical filter
.loop:
    mov      srcptrq, srcq
    mov      dstptrq, dstq
    lea           xq, [wq+xlimq]

    ; load left edge pixels
    test       edged, 1 ; have_left
    jz .emu_left
    test       leftq, leftq ; left == NULL for the edge-extended bottom/top
    jz .load_left_combined
    movd         xm0, [leftq]
    add        leftq, 4
    pinsrd       xm0, [srcq], 1
    pslldq       xm0, 9
    jmp .left_load_done
.load_left_combined:
    movq         xm0, [srcq-3]
    pslldq       xm0, 10
    jmp .left_load_done
.emu_left:
    movd         xm0, [srcq]
    pshufb       xm0, [pb_14x0_1_2]

    ; load right edge pixels
.left_load_done:
    cmp           xd, 32
    jg .main_load
    test          xd, xd
    jg .load_and_splat
    je .splat_right

    ; for very small images (w=[1-2]), edge-extend the original cache,
    ; ugly, but only runs in very odd cases
    add           wd, wd
    pshufb       xm0, [r11-pb_right_ext_mask+pb_0_to_15_min_n+wq*8-16]
    shr           wd, 1

    ; main x loop, mostly this starts in .main_load
.splat_right:
    ; no need to load new pixels, just extend them from the (possibly previously
    ; extended) previous load into m0
    pshufb       xm1, xm0, [pb_15]
    jmp .main_loop
.load_and_splat:
    ; load new pixels and extend edge for right-most
    movu          m1, [srcptrq+3]
    sub          r11, xq
    movu          m2, [r11-pb_right_ext_mask+pb_right_ext_mask+32]
    add          r11, xq
    vpbroadcastb  m3, [srcptrq+2+xq]
    pand          m1, m2
    pandn         m3, m2, m3
    por           m1, m3
    jmp .main_loop
.main_load:
    ; load subsequent line
    movu          m1, [srcptrq+3]
.main_loop:
    vinserti128   m0, xm1, 1

    palignr       m2, m1, m0, 10
    palignr       m3, m1, m0, 11
    palignr       m4, m1, m0, 12
    palignr       m5, m1, m0, 13
    palignr       m6, m1, m0, 14
    palignr       m7, m1, m0, 15

    punpcklbw     m0, m2, m1
    punpckhbw     m2, m1
    punpcklbw     m8, m3, m7
    punpckhbw     m3, m7
    punpcklbw     m7, m4, m6
    punpckhbw     m4, m6
    pxor          m9, m9
    punpcklbw     m6, m5, m9
    punpckhbw     m5, m9

    pmaddubsw     m0, m15
    pmaddubsw     m2, m15
    pmaddubsw     m8, m14
    pmaddubsw     m3, m14
    pmaddubsw     m7, m13
    pmaddubsw     m4, m13
    paddw         m0, m8
    paddw         m2, m3
    psllw         m8, m6, 7
    psllw         m3, m5, 7
    psubw         m8, m10
    psubw         m3, m10
    pmullw        m6, m12
    pmullw        m5, m12
    paddw         m0, m7
    paddw         m2, m4
    paddw         m0, m6
    paddw         m2, m5
    paddsw        m0, m8
    paddsw        m2, m3
    psraw         m0, 3
    psraw         m2, 3
    paddw         m0, m11
    paddw         m2, m11
    mova   [dstptrq], xm0
    mova [dstptrq+16], xm2
    vextracti128 [dstptrq+32], m0, 1
    vextracti128 [dstptrq+48], m2, 1
    vextracti128 xm0, m1, 1
    add      srcptrq, 32
    add      dstptrq, 64
    sub           xq, 32
    cmp           xd, 32
    jg .main_load
    test          xd, xd
    jg .load_and_splat
    cmp           xd, xlimd
    jg .splat_right

    add         srcq, strideq
    add         dstq, 384*2
    dec           hd
    jg .loop
    RET

cglobal wiener_filter_v, 7, 10, 16, dst, stride, mid, w, h, fv, edge
    vpbroadcastd m14, [fvq+4]
    vpbroadcastd m15, [fvq]
    vpbroadcastd m13, [pw_0_128]
    paddw        m14, m13
    vpbroadcastd m12, [pd_1024]

    DEFINE_ARGS dst, stride, mid, w, h, ylim, edge, y, mptr, dstptr
    mov        ylimd, edged
    and        ylimd, 8 ; have_bottom
    shr        ylimd, 2
    sub        ylimd, 3

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
    mov        mptrq, midq
    mov      dstptrq, dstq
    add           yd, ylimd
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
    paddd        m10, m11
    paddd         m7, m9
    paddd        m10, m12
    paddd         m7, m12
    psrad        m10, 11
    psrad         m7, 11
    packssdw     m10, m7
    packuswb     m10, m10
    vpermq       m10, m10, q3120
    mova   [dstptrq], xm10
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

    add         dstq, 16
    add         midq, 32
    sub           wd, 16
    jg .loop_x
    RET
%endif ; ARCH_X86_64
