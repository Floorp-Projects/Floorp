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
pw_16: times 2 dw 16
pw_256: times 2 dw 256
pw_2048: times 2 dw 2048
pw_16380: times 2 dw 16380
pw_0_128: dw 0, 128
pw_5_6: dw 5, 6
pd_6: dd 6
pd_1024: dd 1024
pd_0xf0080029: dd 0xf0080029
pd_0xf00801c7: dd 0xf00801c7

cextern sgr_x_by_x

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

INIT_YMM avx2
cglobal sgr_box3_h, 8, 11, 8, sumsq, sum, left, src, stride, w, h, edge, x, xlim
    mov        xlimd, edged
    and        xlimd, 2                             ; have_right
    add           wd, xlimd
    xor        xlimd, 2                             ; 2*!have_right
    jnz .no_right
    add           wd, 15
    and           wd, ~15
.no_right:
    pxor          m1, m1
    lea         srcq, [srcq+wq]
    lea         sumq, [sumq+wq*2-2]
    lea       sumsqq, [sumsqq+wq*4-4]
    neg           wq
    lea          r10, [pb_right_ext_mask+32]
.loop_y:
    mov           xq, wq

    ; load left
    test       edged, 1                             ; have_left
    jz .no_left
    test       leftq, leftq
    jz .load_left_from_main
    pinsrw       xm0, [leftq+2], 7
    add        leftq, 4
    jmp .expand_x
.no_left:
    vpbroadcastb xm0, [srcq+xq]
    jmp .expand_x
.load_left_from_main:
    pinsrw       xm0, [srcq+xq-2], 7
.expand_x:
    punpckhbw    xm0, xm1

    ; when we reach this, xm0 contains left two px in highest words
    cmp           xd, -16
    jle .loop_x
.partial_load_and_extend:
    vpbroadcastb  m3, [srcq-1]
    pmovzxbw      m2, [srcq+xq]
    punpcklbw     m3, m1
    movu          m4, [r10+xq*2]
    pand          m2, m4
    pandn         m4, m3
    por           m2, m4
    jmp .loop_x_noload
.right_extend:
    psrldq       xm2, xm0, 14
    vpbroadcastw  m2, xm2
    jmp .loop_x_noload

.loop_x:
    pmovzxbw      m2, [srcq+xq]
.loop_x_noload:
    vinserti128   m0, xm2, 1
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
    movu [sumsqq+xq*4+ 0], xm5
    movu [sumsqq+xq*4+16], xm6
    vextracti128 [sumsqq+xq*4+32], m5, 1
    vextracti128 [sumsqq+xq*4+48], m6, 1

    vextracti128 xm0, m2, 1
    add           xq, 16

    ; if x <= -16 we can reload more pixels
    ; else if x < 0 we reload and extend (this implies have_right=0)
    ; else if x < xlimd we extend from previous load (this implies have_right=0)
    ; else we are done

    cmp           xd, -16
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

INIT_YMM avx2
cglobal sgr_box3_v, 5, 10, 9, sumsq, sum, w, h, edge, x, y, sumsq_ptr, sum_ptr, ylim
    mov           xq, -2
    mov        ylimd, edged
    and        ylimd, 8                             ; have_bottom
    shr        ylimd, 2
    sub        ylimd, 2                             ; -2 if have_bottom=0, else 0
.loop_x:
    lea           yd, [hq+ylimq+2]
    lea   sumsq_ptrq, [sumsqq+xq*4+4-(384+16)*4]
    lea     sum_ptrq, [sumq+xq*2+2-(384+16)*2]
    test       edged, 4                             ; have_top
    jnz .load_top
    movu          m0, [sumsq_ptrq+(384+16)*4*1]
    movu          m1, [sumsq_ptrq+(384+16)*4*1+32]
    mova          m2, m0
    mova          m3, m1
    mova          m4, m0
    mova          m5, m1
    movu          m6, [sum_ptrq+(384+16)*2*1]
    mova          m7, m6
    mova          m8, m6
    jmp .loop_y_noload
.load_top:
    movu          m0, [sumsq_ptrq-(384+16)*4*1]      ; l2sq [left]
    movu          m1, [sumsq_ptrq-(384+16)*4*1+32]   ; l2sq [right]
    movu          m2, [sumsq_ptrq-(384+16)*4*0]      ; l1sq [left]
    movu          m3, [sumsq_ptrq-(384+16)*4*0+32]   ; l1sq [right]
    movu          m6, [sum_ptrq-(384+16)*2*1]        ; l2
    movu          m7, [sum_ptrq-(384+16)*2*0]        ; l1
.loop_y:
    movu          m4, [sumsq_ptrq+(384+16)*4*1]      ; l0sq [left]
    movu          m5, [sumsq_ptrq+(384+16)*4*1+32]   ; l0sq [right]
    movu          m8, [sum_ptrq+(384+16)*2*1]        ; l0
.loop_y_noload:
    paddd         m0, m2
    paddd         m1, m3
    paddw         m6, m7
    paddd         m0, m4
    paddd         m1, m5
    paddw         m6, m8
    movu [sumsq_ptrq+ 0], m0
    movu [sumsq_ptrq+32], m1
    movu  [sum_ptrq], m6

    ; shift position down by one
    mova          m0, m2
    mova          m1, m3
    mova          m2, m4
    mova          m3, m5
    mova          m6, m7
    mova          m7, m8
    add   sumsq_ptrq, (384+16)*4
    add     sum_ptrq, (384+16)*2
    dec           yd
    jg .loop_y
    cmp           yd, ylimd
    jg .loop_y_noload
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET

INIT_YMM avx2
cglobal sgr_calc_ab1, 4, 6, 11, a, b, w, h, s
    sub           aq, (384+16-1)*4
    sub           bq, (384+16-1)*2
    add           hd, 2
    lea           r5, [sgr_x_by_x-0xf03]
%ifidn sd, sm
    movd         xm6, sd
    vpbroadcastd  m6, xm6
%else
    vpbroadcastd  m6, sm
%endif
    vpbroadcastd  m8, [pd_0xf00801c7]
    vpbroadcastd  m9, [pw_256]
    pcmpeqb       m7, m7
    psrld        m10, m9, 13                        ; pd_2048
    DEFINE_ARGS a, b, w, h, x

.loop_y:
    mov           xq, -2
.loop_x:
    pmovzxwd      m0, [bq+xq*2]
    pmovzxwd      m1, [bq+xq*2+(384+16)*2]
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
    pmulld        m2, m6
    pmulld        m3, m6
    paddusw       m2, m8
    paddusw       m3, m8
    psrld         m2, 20                            ; z
    psrld         m3, 20
    mova          m5, m7
    vpgatherdd    m4, [r5+m2], m5                   ; xx
    mova          m5, m7
    vpgatherdd    m2, [r5+m3], m5
    psrld         m4, 24
    psrld         m2, 24
    pmulld        m0, m4
    pmulld        m1, m2
    packssdw      m4, m2
    psubw         m4, m9, m4
    vpermq        m4, m4, q3120
    paddd         m0, m10
    paddd         m1, m10
    psrld         m0, 12
    psrld         m1, 12
    movu   [bq+xq*2], xm4
    vextracti128 [bq+xq*2+(384+16)*2], m4, 1
    movu   [aq+xq*4], m0
    movu [aq+xq*4+(384+16)*4], m1
    add           xd, 8
    cmp           xd, wd
    jl .loop_x
    add           aq, (384+16)*4*2
    add           bq, (384+16)*2*2
    sub           hd, 2
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_finish_filter1, 7, 13, 16, t, src, stride, a, b, w, h, \
                                       tmp_ptr, src_ptr, a_ptr, b_ptr, x, y
    vpbroadcastd m15, [pw_16]
    xor           xd, xd
.loop_x:
    lea     tmp_ptrq, [tq+xq*2]
    lea     src_ptrq, [srcq+xq*1]
    lea       a_ptrq, [aq+xq*4+(384+16)*4]
    lea       b_ptrq, [bq+xq*2+(384+16)*2]
    movu          m0, [aq+xq*4-(384+16)*4-4]
    movu          m2, [aq+xq*4-(384+16)*4+4]
    mova          m1, [aq+xq*4-(384+16)*4]           ; a:top [first half]
    paddd         m0, m2                            ; a:tl+tr [first half]
    movu          m2, [aq+xq*4-(384+16)*4-4+32]
    movu          m4, [aq+xq*4-(384+16)*4+4+32]
    mova          m3, [aq+xq*4-(384+16)*4+32]        ; a:top [second half]
    paddd         m2, m4                            ; a:tl+tr [second half]
    movu          m4, [aq+xq*4-4]
    movu          m5, [aq+xq*4+4]
    paddd         m1, [aq+xq*4]                     ; a:top+ctr [first half]
    paddd         m4, m5                            ; a:l+r [first half]
    movu          m5, [aq+xq*4+32-4]
    movu          m6, [aq+xq*4+32+4]
    paddd         m3, [aq+xq*4+32]                  ; a:top+ctr [second half]
    paddd         m5, m6                            ; a:l+r [second half]

    movu          m6, [bq+xq*2-(384+16)*2-2]
    movu          m8, [bq+xq*2-(384+16)*2+2]
    mova          m7, [bq+xq*2-(384+16)*2]          ; b:top
    paddw         m6, m8                            ; b:tl+tr
    movu          m8, [bq+xq*2-2]
    movu          m9, [bq+xq*2+2]
    paddw         m7, [bq+xq*2]                     ; b:top+ctr
    paddw         m8, m9                            ; b:l+r
    mov           yd, hd
.loop_y:
    movu          m9, [b_ptrq-2]
    movu         m10, [b_ptrq+2]
    paddw         m7, [b_ptrq]                      ; b:top+ctr+bottom
    paddw         m9, m10                           ; b:bl+br
    paddw        m10, m7, m8                        ; b:top+ctr+bottom+l+r
    paddw         m6, m9                            ; b:tl+tr+bl+br
    psubw         m7, [b_ptrq-(384+16)*2*2]         ; b:ctr+bottom
    paddw        m10, m6
    psllw        m10, 2
    psubw        m10, m6                            ; aa
    pmovzxbw     m12, [src_ptrq]
    punpcklwd     m6, m10, m15
    punpckhwd    m10, m15
    punpcklwd    m13, m12, m15
    punpckhwd    m12, m15
    pmaddwd       m6, m13                           ; aa*src[x]+256 [first half]
    pmaddwd      m10, m12                           ; aa*src[x]+256 [second half]

    movu         m11, [a_ptrq-4]
    movu         m12, [a_ptrq+4]
    paddd         m1, [a_ptrq]                      ; a:top+ctr+bottom [first half]
    paddd        m11, m12                           ; a:bl+br [first half]
    movu         m12, [a_ptrq+32-4]
    movu         m13, [a_ptrq+32+4]
    paddd         m3, [a_ptrq+32]                   ; a:top+ctr+bottom [second half]
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
    vperm2i128    m0, m13, m14, 0x31
    vinserti128  m13, xm14, 1
    psubd         m1, [a_ptrq-(384+16)*4*2]          ; a:ctr+bottom [first half]
    psubd         m3, [a_ptrq-(384+16)*4*2+32]       ; a:ctr+bottom [second half]

    paddd         m6, m13
    paddd        m10, m0
    psrad         m6, 9
    psrad        m10, 9
    packssdw      m6, m10
    mova  [tmp_ptrq], m6

    ; shift to next row
    mova          m0, m4
    mova          m2, m5
    mova          m4, m11
    mova          m5, m12
    mova          m6, m8
    mova          m8, m9

    add       a_ptrq, (384+16)*4
    add       b_ptrq, (384+16)*2
    add     tmp_ptrq, 384*2
    add     src_ptrq, strideq
    dec           yd
    jg .loop_y
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET

INIT_YMM avx2
cglobal sgr_weighted1, 6, 6, 7, dst, stride, t, w, h, wt
    movd         xm0, wtd
    vpbroadcastw  m0, xm0
    psllw         m0, 4
    DEFINE_ARGS dst, stride, t, w, h, idx
.loop_y:
    xor         idxd, idxd
.loop_x:
    mova          m1, [tq+idxq*2+ 0]
    mova          m4, [tq+idxq*2+32]
    pmovzxbw      m2, [dstq+idxq+ 0]
    pmovzxbw      m5, [dstq+idxq+16]
    psllw         m3, m2, 4
    psllw         m6, m5, 4
    psubw         m1, m3
    psubw         m4, m6
    pmulhrsw      m1, m0
    pmulhrsw      m4, m0
    paddw         m1, m2
    paddw         m4, m5
    packuswb      m1, m4
    vpermq        m1, m1, q3120
    mova [dstq+idxq], m1
    add         idxd, 32
    cmp         idxd, wd
    jl .loop_x
    add         dstq, strideq
    add           tq, 384 * 2
    dec           hd
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_box5_h, 8, 11, 10, sumsq, sum, left, src, stride, w, h, edge, x, xlim
    test       edged, 2                             ; have_right
    jz .no_right
    xor        xlimd, xlimd
    add           wd, 2
    add           wd, 15
    and           wd, ~15
    jmp .right_done
.no_right:
    mov        xlimd, 3
    sub           wd, 1
.right_done:
    pxor          m1, m1
    lea         srcq, [srcq+wq+1]
    lea         sumq, [sumq+wq*2-2]
    lea       sumsqq, [sumsqq+wq*4-4]
    neg           wq
    lea          r10, [pb_right_ext_mask+32]
.loop_y:
    mov           xq, wq

    ; load left
    test       edged, 1                             ; have_left
    jz .no_left
    test       leftq, leftq
    jz .load_left_from_main
    movd         xm0, [leftq]
    pinsrd       xm0, [srcq+xq-1], 1
    pslldq       xm0, 11
    add        leftq, 4
    jmp .expand_x
.no_left:
    vpbroadcastb xm0, [srcq+xq-1]
    jmp .expand_x
.load_left_from_main:
    pinsrd       xm0, [srcq+xq-4], 3
.expand_x:
    punpckhbw    xm0, xm1

    ; when we reach this, xm0 contains left two px in highest words
    cmp           xd, -16
    jle .loop_x
    test          xd, xd
    jge .right_extend
.partial_load_and_extend:
    vpbroadcastb  m3, [srcq-1]
    pmovzxbw      m2, [srcq+xq]
    punpcklbw     m3, m1
    movu          m4, [r10+xq*2]
    pand          m2, m4
    pandn         m4, m3
    por           m2, m4
    jmp .loop_x_noload
.right_extend:
    psrldq       xm2, xm0, 14
    vpbroadcastw  m2, xm2
    jmp .loop_x_noload

.loop_x:
    pmovzxbw      m2, [srcq+xq]
.loop_x_noload:
    vinserti128   m0, xm2, 1
    palignr       m3, m2, m0, 8
    palignr       m4, m2, m0, 10
    palignr       m5, m2, m0, 12
    palignr       m6, m2, m0, 14

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
    movu [sumsqq+xq*4+ 0], xm7
    movu [sumsqq+xq*4+16], xm3
    vextracti128 [sumsqq+xq*4+32], m7, 1
    vextracti128 [sumsqq+xq*4+48], m3, 1

    vextracti128 xm0, m2, 1
    add           xq, 16

    ; if x <= -16 we can reload more pixels
    ; else if x < 0 we reload and extend (this implies have_right=0)
    ; else if x < xlimd we extend from previous load (this implies have_right=0)
    ; else we are done

    cmp           xd, -16
    jle .loop_x
    test          xd, xd
    jl .partial_load_and_extend
    cmp           xd, xlimd
    jl .right_extend

    add       sumsqq, (384+16)*4
    add         sumq, (384+16)*2
    add         srcq, strideq
    dec hd
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_box5_v, 5, 10, 15, sumsq, sum, w, h, edge, x, y, sumsq_ptr, sum_ptr, ylim
    mov           xq, -2
    mov        ylimd, edged
    and        ylimd, 8                             ; have_bottom
    shr        ylimd, 2
    sub        ylimd, 3                             ; -3 if have_bottom=0, else -1
.loop_x:
    lea           yd, [hq+ylimq+2]
    lea   sumsq_ptrq, [sumsqq+xq*4+4-(384+16)*4]
    lea     sum_ptrq, [sumq+xq*2+2-(384+16)*2]
    test       edged, 4                             ; have_top
    jnz .load_top
    movu          m0, [sumsq_ptrq+(384+16)*4*1]
    movu          m1, [sumsq_ptrq+(384+16)*4*1+32]
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
    movu          m1, [sumsq_ptrq-(384+16)*4*1+32]   ; l3/4sq [right]
    movu          m4, [sumsq_ptrq-(384+16)*4*0]      ; l2sq [left]
    movu          m5, [sumsq_ptrq-(384+16)*4*0+32]   ; l2sq [right]
    mova          m2, m0
    mova          m3, m1
    movu         m10, [sum_ptrq-(384+16)*2*1]        ; l3/4
    movu         m12, [sum_ptrq-(384+16)*2*0]        ; l2
    mova         m11, m10
.loop_y:
    movu          m6, [sumsq_ptrq+(384+16)*4*1]      ; l1sq [left]
    movu          m7, [sumsq_ptrq+(384+16)*4*1+32]   ; l1sq [right]
    movu         m13, [sum_ptrq+(384+16)*2*1]        ; l1
.loop_y_second_load:
    test          yd, yd
    jle .emulate_second_load
    movu          m8, [sumsq_ptrq+(384+16)*4*2]      ; l0sq [left]
    movu          m9, [sumsq_ptrq+(384+16)*4*2+32]   ; l0sq [right]
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
    movu [sumsq_ptrq+32], m1
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
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET
.emulate_second_load:
    mova          m8, m6
    mova          m9, m7
    mova         m14, m13
    jmp .loop_y_noload

INIT_YMM avx2
cglobal sgr_calc_ab2, 4, 6, 11, a, b, w, h, s
    sub           aq, (384+16-1)*4
    sub           bq, (384+16-1)*2
    add           hd, 2
    lea           r5, [sgr_x_by_x-0xf03]
%ifidn sd, sm
    movd         xm6, sd
    vpbroadcastd  m6, xm6
%else
    vpbroadcastd  m6, sm
%endif
    vpbroadcastd  m8, [pd_0xf0080029]
    vpbroadcastd  m9, [pw_256]
    pcmpeqb       m7, m7
    psrld        m10, m9, 15                        ; pd_512
    DEFINE_ARGS a, b, w, h, x
.loop_y:
    mov           xq, -2
.loop_x:
    pmovzxwd      m0, [bq+xq*2+ 0]
    pmovzxwd      m1, [bq+xq*2+16]
    movu          m2, [aq+xq*4+ 0]
    movu          m3, [aq+xq*4+32]
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
    pmulld        m2, m6
    pmulld        m3, m6
    paddusw       m2, m8
    paddusw       m3, m8
    psrld         m2, 20                            ; z
    psrld         m3, 20
    mova          m5, m7
    vpgatherdd    m4, [r5+m2], m5                   ; xx
    mova          m5, m7
    vpgatherdd    m2, [r5+m3], m5
    psrld         m4, 24
    psrld         m2, 24
    packssdw      m3, m4, m2
    pmullw        m4, m8
    pmullw        m2, m8
    psubw         m3, m9, m3
    vpermq        m3, m3, q3120
    pmaddwd       m0, m4
    pmaddwd       m1, m2
    paddd         m0, m10
    paddd         m1, m10
    psrld         m0, 10
    psrld         m1, 10
    movu   [bq+xq*2], m3
    movu [aq+xq*4+ 0], m0
    movu [aq+xq*4+32], m1
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    add           aq, (384+16)*4*2
    add           bq, (384+16)*2*2
    sub           hd, 2
    jg .loop_y
    RET

INIT_YMM avx2
cglobal sgr_finish_filter2, 7, 13, 13, t, src, stride, a, b, w, h, \
                                       tmp_ptr, src_ptr, a_ptr, b_ptr, x, y
    vpbroadcastd  m9, [pw_5_6]
    vpbroadcastd m12, [pw_256]
    psrlw        m11, m12, 1                    ; pw_128
    psrlw        m10, m12, 8                    ; pw_1
    xor           xd, xd
.loop_x:
    lea     tmp_ptrq, [tq+xq*2]
    lea     src_ptrq, [srcq+xq*1]
    lea       a_ptrq, [aq+xq*4+(384+16)*4]
    lea       b_ptrq, [bq+xq*2+(384+16)*2]
    movu          m0, [aq+xq*4-(384+16)*4-4]
    mova          m1, [aq+xq*4-(384+16)*4]
    movu          m2, [aq+xq*4-(384+16)*4+4]
    movu          m3, [aq+xq*4-(384+16)*4-4+32]
    mova          m4, [aq+xq*4-(384+16)*4+32]
    movu          m5, [aq+xq*4-(384+16)*4+4+32]
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
    packssdw      m2, m5, m3                    ; prev_odd_a
    mov           yd, hd
.loop_y:
    movu          m3, [a_ptrq-4]
    mova          m4, [a_ptrq]
    movu          m5, [a_ptrq+4]
    movu          m6, [a_ptrq+32-4]
    mova          m7, [a_ptrq+32]
    movu          m8, [a_ptrq+32+4]
    paddd         m3, m5
    paddd         m6, m8
    paddd         m3, m4
    paddd         m6, m7
    pslld         m5, m3, 2
    pslld         m8, m6, 2
    paddd         m5, m3
    paddd         m8, m6
    paddd         m3, m5, m4                    ; cur_odd_b [first half]
    paddd         m4, m8, m7                    ; cur_odd_b [second half]
    movu          m5, [b_ptrq-2]
    mova          m6, [b_ptrq]
    movu          m7, [b_ptrq+2]
    paddw         m5, m7
    punpcklwd     m7, m5, m6
    punpckhwd     m5, m6
    pmaddwd       m7, m9
    pmaddwd       m5, m9
    packssdw      m5, m7, m5                    ; cur_odd_a

    paddd         m0, m3                        ; cur_even_b [first half]
    paddd         m1, m4                        ; cur_even_b [second half]
    paddw         m2, m5                        ; cur_even_a

    pmovzxbw      m6, [src_ptrq]
    vperm2i128    m8, m0, m1, 0x31
    vinserti128   m0, xm1, 1
    punpcklwd     m7, m6, m10
    punpckhwd     m6, m10
    punpcklwd     m1, m2, m12
    punpckhwd     m2, m12
    pmaddwd       m7, m1
    pmaddwd       m6, m2
    paddd         m7, m0
    paddd         m6, m8
    psrad         m7, 9
    psrad         m6, 9

    pmovzxbw      m8, [src_ptrq+strideq]
    punpcklwd     m0, m8, m10
    punpckhwd     m8, m10
    punpcklwd     m1, m5, m11
    punpckhwd     m2, m5, m11
    pmaddwd       m0, m1
    pmaddwd       m8, m2
    vinserti128   m2, m3, xm4, 1
    vperm2i128    m1, m3, m4, 0x31
    paddd         m0, m2
    paddd         m8, m1
    psrad         m0, 8
    psrad         m8, 8

    packssdw      m7, m6
    packssdw      m0, m8
    mova [tmp_ptrq+384*2*0], m7
    mova [tmp_ptrq+384*2*1], m0

    mova          m0, m3
    mova          m1, m4
    mova          m2, m5
    add       a_ptrq, (384+16)*4*2
    add       b_ptrq, (384+16)*2*2
    add     tmp_ptrq, 384*2*2
    lea     src_ptrq, [src_ptrq+strideq*2]
    sub           yd, 2
    jg .loop_y
    add           xd, 16
    cmp           xd, wd
    jl .loop_x
    RET

INIT_YMM avx2
cglobal sgr_weighted2, 7, 7, 11, dst, stride, t1, t2, w, h, wt
    vpbroadcastd  m0, [wtq]
    vpbroadcastd m10, [pd_1024]
    DEFINE_ARGS dst, stride, t1, t2, w, h, idx
.loop_y:
    xor         idxd, idxd
.loop_x:
    mova          m1, [t1q+idxq*2+ 0]
    mova          m2, [t1q+idxq*2+32]
    mova          m3, [t2q+idxq*2+ 0]
    mova          m4, [t2q+idxq*2+32]
    pmovzxbw      m5, [dstq+idxq+ 0]
    pmovzxbw      m6, [dstq+idxq+16]
    psllw         m7, m5, 4
    psllw         m8, m6, 4
    psubw         m1, m7
    psubw         m2, m8
    psubw         m3, m7
    psubw         m4, m8
    punpcklwd     m9, m1, m3
    punpckhwd     m1, m3
    punpcklwd     m3, m2, m4
    punpckhwd     m2, m4
    pmaddwd       m9, m0
    pmaddwd       m1, m0
    pmaddwd       m3, m0
    pmaddwd       m2, m0
    paddd         m9, m10
    paddd         m1, m10
    paddd         m3, m10
    paddd         m2, m10
    psrad         m9, 11
    psrad         m1, 11
    psrad         m3, 11
    psrad         m2, 11
    packssdw      m1, m9, m1
    packssdw      m2, m3, m2
    paddw         m1, m5
    paddw         m2, m6
    packuswb      m1, m2
    vpermq        m1, m1, q3120
    mova [dstq+idxq], m1
    add         idxd, 32
    cmp         idxd, wd
    jl .loop_x
    add         dstq, strideq
    add          t1q, 384 * 2
    add          t2q, 384 * 2
    dec           hd
    jg .loop_y
    RET
%endif ; ARCH_X86_64
