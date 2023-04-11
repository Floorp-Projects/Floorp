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

SECTION_RODATA 64

%macro JMP_TABLE 2-*
    %xdefine %%prefix mangle(private_prefix %+ _%1)
    %1_table:
    %xdefine %%base %1_table
    %rep %0 - 1
        dd %%prefix %+ .w%2 - %%base
        %rotate 1
    %endrep
%endmacro

%macro SAVE_TMVS_TABLE 3 ; num_entries, w, suffix
    %rep %1
        db %2*3
        db mangle(private_prefix %+ _save_tmvs_%3).write%2 - \
           mangle(private_prefix %+ _save_tmvs_%3).write1
    %endrep
%endmacro

%if ARCH_X86_64
splat_mv_shuf: db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3
               db  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7
               db  8,  9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11
               db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3
%endif
save_pack0:    db  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0
               db  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1
save_pack1:    db  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2
               db  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3
save_ref_shuf: db  0, -1, -1, -1,  1, -1, -1, -1,  8, -1, -1, -1,  9, -1, -1, -1
cond_shuf512:  db  3,  3,  3,  3,  7,  7,  7,  7,  7,  7,  7,  7,  3,  3,  3,  3
save_cond0:    db  0x80, 0x81, 0x82, 0x83, 0x89, 0x84, 0x00, 0x00
save_cond1:    db  0x84, 0x85, 0x86, 0x87, 0x88, 0x80, 0x00, 0x00
pb_128:        times 16 db 128

save_tmvs_ssse3_table: SAVE_TMVS_TABLE 2, 16, ssse3
                       SAVE_TMVS_TABLE 4,  8, ssse3
                       SAVE_TMVS_TABLE 4,  4, ssse3
                       SAVE_TMVS_TABLE 5,  2, ssse3
                       SAVE_TMVS_TABLE 7,  1, ssse3

%if ARCH_X86_64
save_tmvs_avx2_table: SAVE_TMVS_TABLE 2, 16, avx2
                      SAVE_TMVS_TABLE 4,  8, avx2
                      SAVE_TMVS_TABLE 4,  4, avx2
                      SAVE_TMVS_TABLE 5,  2, avx2
                      SAVE_TMVS_TABLE 7,  1, avx2

save_tmvs_avx512icl_table: SAVE_TMVS_TABLE 2, 16, avx512icl
                           SAVE_TMVS_TABLE 4,  8, avx512icl
                           SAVE_TMVS_TABLE 4,  4, avx512icl
                           SAVE_TMVS_TABLE 5,  2, avx512icl
                           SAVE_TMVS_TABLE 7,  1, avx512icl

JMP_TABLE splat_mv_avx512icl, 1, 2, 4, 8, 16, 32
JMP_TABLE splat_mv_avx2,      1, 2, 4, 8, 16, 32
%endif

JMP_TABLE splat_mv_sse2,      1, 2, 4, 8, 16, 32

SECTION .text

%macro movif32 2
%if ARCH_X86_32
    mov             %1, %2
%endif
%endmacro

INIT_XMM ssse3
; refmvs_temporal_block *rp, ptrdiff_t stride,
; refmvs_block **rr, uint8_t *ref_sign,
; int col_end8, int row_end8, int col_start8, int row_start8
%if ARCH_X86_64
cglobal save_tmvs, 4, 13, 11, rp, stride, rr, ref_sign, \
                             xend, yend, xstart, ystart
%define base_reg r12
%else
cglobal save_tmvs, 6, 7, 8, rp, stride, rr, ref_sign, \
                            xend, yend, xstart, ystart
    movq            m5, [ref_signq]
    lea        strided, [strided*5]
    mov        stridem, strided
    mov             r3, xstartm
    mov             r1, ystartm
 DEFINE_ARGS b, ystart, rr, cand, xend, x
%define stridemp r1m
%define m8  [base+pb_128]
%define m9  [base+save_pack0+ 0]
%define m10 [base+save_pack0+16]
%define base_reg r6
%endif
%define base base_reg-.write1
    LEA       base_reg, .write1
%if ARCH_X86_64
    movifnidn    xendd, xendm
    movifnidn    yendd, yendm
    mov        xstartd, xstartm
    mov        ystartd, ystartm
    movq            m5, [ref_signq]
%endif
    movu            m4, [base+save_ref_shuf]
    movddup         m6, [base+save_cond0]
    movddup         m7, [base+save_cond1]
%if ARCH_X86_64
    mova            m8, [base+pb_128]
    mova            m9, [base+save_pack0+ 0]
    mova           m10, [base+save_pack0+16]
%endif
    psllq           m5, 8
%if ARCH_X86_64
    lea            r9d, [xendq*5]
    lea        xstartd, [xstartq*5]
    sub          yendd, ystartd
    add        ystartd, ystartd
    lea        strideq, [strideq*5]
    sub        xstartq, r9
    add          xendd, r9d
    add            rpq, r9
 DEFINE_ARGS rp, stride, rr, x, xend, h, xstart, ystart, b, cand
%else
    lea             r0, [xendd*5]   ; xend5
    lea             r3, [r3*5]      ; xstart5
    sub             r3, r0          ; -w5
    mov            r6m, r3
%define xstartq r6m
    add          xendd, r0          ; xend6
    add            r0m, r0          ; rp+xend5
    mov          xendm, xendd
    sub             r5, r1          ; h
    add             r1, r1
    mov            r7m, r1
    mov            r5m, r5
%define hd r5mp
    jmp .loop_y_noload
%endif
.loop_y:
    movif32    ystartd, r7m
    movif32      xendd, xendm
.loop_y_noload:
    and        ystartd, 30
    mov             xq, xstartq
    mov             bq, [rrq+ystartq*gprsize]
    add        ystartd, 2
    movif32        r7m, ystartd
    lea             bq, [bq+xendq*4]
.loop_x:
%if ARCH_X86_32
%define rpq  r3
%define r10  r1
%define r10d r1
%define r11  r4
%define r11d r4
%endif
    imul         candq, xq, 0x9999  ; x / 5 * 3
    sar          candq, 16
    movzx         r10d, byte [bq+candq*8+22] ; cand_b->bs
    movu            m0, [bq+candq*8+12]      ; cand_b
    movzx         r11d, byte [base+save_tmvs_ssse3_table+r10*2+0]
    movzx         r10d, byte [base+save_tmvs_ssse3_table+r10*2+1]
    add            r10, base_reg
    add          candq, r11
    jge .calc
    movu            m1, [bq+candq*8+12]
    movzx         r11d, byte [bq+candq*8+22]
    movzx         r11d, byte [base+save_tmvs_ssse3_table+r11*2+1]
    add            r11, base_reg
.calc:
    movif32        rpq, r0m
    ; ref check
    punpckhqdq      m2, m0, m1
    pshufb          m2, m4      ; b0.ref0 b0.ref1 b1.ref0 b1.ref1 | ...
    pshufb          m3, m5, m2  ; ref > 0 && res_sign[ref - 1]
    ; mv check
    punpcklqdq      m2, m0, m1  ; b0.mv0 b0.mv1 b1.mv0 b1.mv1 | ...
    pabsw           m2, m2
    psrlw           m2, 12      ; (abs(mv.x) | abs(mv.y)) < 4096
    ; res
    pcmpgtd         m3, m2
    pshufd          m2, m3, q2301
    pand            m3, m6      ; b0c0 b0c1 b1c0 b1c1 | ...
    pand            m2, m7      ; b0c1 b0c0 b1c1 b1c0 | ...
    por             m3, m2      ; b0.shuf b1.shuf | ...
    pxor            m3, m8      ; if cond0|cond1 == 0 => zero out
    pshufb          m0, m3
    pshufb          m1, m3
    call           r10
    jge .next_line
    pshufd          m0, m1, q3232
    call           r11
    jl .loop_x
.next_line:
    add            rpq, stridemp
    movif32        r0m, rpq
    dec             hd
    jg .loop_y
    RET
.write1:
    movd    [rpq+xq+0], m0
    psrlq           m0, 8
    movd    [rpq+xq+1], m0
    add             xq, 5*1
    ret
.write2:
    movq    [rpq+xq+0], m0
    psrlq           m0, 8
    movd    [rpq+xq+6], m0
    add             xq, 5*2
    ret
.write4:
    pshufb          m0, m9
    movu   [rpq+xq+ 0], m0
    psrlq           m0, 8
    movd   [rpq+xq+16], m0
    add             xq, 5*4
    ret
.write8:
    pshufb          m2, m0, m9
    movu   [rpq+xq+ 0], m2
    pshufb          m0, m10
    movu   [rpq+xq+16], m0
    psrldq          m2, 2
    movq   [rpq+xq+32], m2
    add             xq, 5*8
    ret
.write16:
    pshufb          m2, m0, m9
    movu   [rpq+xq+ 0], m2
    pshufb          m0, m10
    movu   [rpq+xq+16], m0
    shufps          m2, m0, q1032
    movu   [rpq+xq+48], m2
    shufps          m2, m0, q2121
    movu   [rpq+xq+32], m2
    shufps          m0, m2, q1032
    movu   [rpq+xq+64], m0
    add             xq, 5*16
    ret

INIT_XMM sse2
; refmvs_block **rr, refmvs_block *a, int bx4, int bw4, int bh4
cglobal splat_mv, 4, 5, 3, rr, a, bx4, bw4, bh4
    add           bx4d, bw4d
    tzcnt         bw4d, bw4d
    mova            m2, [aq]
    LEA             aq, splat_mv_sse2_table
    lea           bx4q, [bx4q*3-32]
    movsxd        bw4q, [aq+bw4q*4]
    movifnidn     bh4d, bh4m
    pshufd          m0, m2, q0210
    pshufd          m1, m2, q1021
    pshufd          m2, m2, q2102
    add           bw4q, aq
.loop:
    mov             aq, [rrq]
    add            rrq, gprsize
    lea             aq, [aq+bx4q*4]
    jmp           bw4q
.w32:
    mova    [aq-16*16], m0
    mova    [aq-16*15], m1
    mova    [aq-16*14], m2
    mova    [aq-16*13], m0
    mova    [aq-16*12], m1
    mova    [aq-16*11], m2
    mova    [aq-16*10], m0
    mova    [aq-16* 9], m1
    mova    [aq-16* 8], m2
    mova    [aq-16* 7], m0
    mova    [aq-16* 6], m1
    mova    [aq-16* 5], m2
.w16:
    mova    [aq-16* 4], m0
    mova    [aq-16* 3], m1
    mova    [aq-16* 2], m2
    mova    [aq-16* 1], m0
    mova    [aq+16* 0], m1
    mova    [aq+16* 1], m2
.w8:
    mova    [aq+16* 2], m0
    mova    [aq+16* 3], m1
    mova    [aq+16* 4], m2
.w4:
    mova    [aq+16* 5], m0
    mova    [aq+16* 6], m1
    mova    [aq+16* 7], m2
    dec           bh4d
    jg .loop
    RET
.w2:
    movu      [aq+104], m0
    movq      [aq+120], m1
    dec           bh4d
    jg .loop
    RET
.w1:
    movq      [aq+116], m0
    movd      [aq+124], m2
    dec           bh4d
    jg .loop
    RET

%if ARCH_X86_64
INIT_YMM avx2
; refmvs_temporal_block *rp, ptrdiff_t stride,
; refmvs_block **rr, uint8_t *ref_sign,
; int col_end8, int row_end8, int col_start8, int row_start8
cglobal save_tmvs, 4, 13, 10, rp, stride, rr, ref_sign, \
                              xend, yend, xstart, ystart
%define base r12-.write1
    lea            r12, [.write1]
    movifnidn    xendd, xendm
    movifnidn    yendd, yendm
    mov        xstartd, xstartm
    mov        ystartd, ystartm
    vpbroadcastq    m4, [ref_signq]
    vpbroadcastq    m3, [base+save_ref_shuf+8]
    vpbroadcastq    m5, [base+save_cond0]
    vpbroadcastq    m6, [base+save_cond1]
    vpbroadcastd    m7, [base+pb_128]
    mova            m8, [base+save_pack0]
    mova            m9, [base+save_pack1]
    psllq           m4, 8
    lea            r9d, [xendq*5]
    lea        xstartd, [xstartq*5]
    sub          yendd, ystartd
    add        ystartd, ystartd
    lea        strideq, [strideq*5]
    sub        xstartq, r9
    add          xendd, r9d
    add            rpq, r9
 DEFINE_ARGS rp, stride, rr, x, xend, h, xstart, ystart, b, cand
.loop_y:
    and        ystartd, 30
    mov             xq, xstartq
    mov             bq, [rrq+ystartq*8]
    add        ystartd, 2
    lea             bq, [bq+xendq*4]
.loop_x:
    imul         candq, xq, 0x9999
    sar          candq, 16                   ; x / 5 * 3
    movzx         r10d, byte [bq+candq*8+22] ; cand_b->bs
    movu           xm0, [bq+candq*8+12]      ; cand_b
    movzx         r11d, byte [base+save_tmvs_avx2_table+r10*2+0]
    movzx         r10d, byte [base+save_tmvs_avx2_table+r10*2+1]
    add            r10, r12
    add          candq, r11
    jge .calc
    vinserti128     m0, [bq+candq*8+12], 1
    movzx         r11d, byte [bq+candq*8+22]
    movzx         r11d, byte [base+save_tmvs_avx2_table+r11*2+1]
    add            r11, r12
.calc:
    pshufb          m1, m0, m3
    pabsw           m2, m0
    pshufb          m1, m4, m1  ; ref > 0 && res_sign[ref - 1]
    psrlw           m2, 12      ; (abs(mv.x) | abs(mv.y)) < 4096
    pcmpgtd         m1, m2
    pshufd          m2, m1, q2301
    pand            m1, m5      ; b0.cond0 b1.cond0
    pand            m2, m6      ; b0.cond1 b1.cond1
    por             m1, m2      ; b0.shuf b1.shuf
    pxor            m1, m7      ; if cond0|cond1 == 0 => zero out
    pshufb          m0, m1
    call           r10
    jge .next_line
    vextracti128   xm0, m0, 1
    call           r11
    jl .loop_x
.next_line:
    add            rpq, strideq
    dec             hd
    jg .loop_y
    RET
.write1:
    movd   [rpq+xq+ 0], xm0
    pextrb [rpq+xq+ 4], xm0, 4
    add             xq, 5*1
    ret
.write2:
    movq    [rpq+xq+0], xm0
    psrlq          xm1, xm0, 8
    movd    [rpq+xq+6], xm1
    add             xq, 5*2
    ret
.write4:
    pshufb         xm1, xm0, xm8
    movu   [rpq+xq+ 0], xm1
    psrlq          xm1, 8
    movd   [rpq+xq+16], xm1
    add             xq, 5*4
    ret
.write8:
    vinserti128     m1, m0, xm0, 1
    pshufb          m1, m8
    movu   [rpq+xq+ 0], m1
    psrldq         xm1, 2
    movq   [rpq+xq+32], xm1
    add             xq, 5*8
    ret
.write16:
    vinserti128     m1, m0, xm0, 1
    pshufb          m2, m1, m8
    movu   [rpq+xq+ 0], m2
    pshufb          m1, m9
    movu   [rpq+xq+32], m1
    shufps         xm2, xm1, q1021
    movu   [rpq+xq+64], xm2
    add             xq, 5*16
    ret

cglobal splat_mv, 4, 5, 3, rr, a, bx4, bw4, bh4
    add           bx4d, bw4d
    tzcnt         bw4d, bw4d
    vbroadcasti128  m0, [aq]
    lea             aq, [splat_mv_avx2_table]
    lea           bx4q, [bx4q*3-32]
    movsxd        bw4q, [aq+bw4q*4]
    pshufb          m0, [splat_mv_shuf]
    movifnidn     bh4d, bh4m
    pshufd          m1, m0, q2102
    pshufd          m2, m0, q1021
    add           bw4q, aq
.loop:
    mov             aq, [rrq]
    add            rrq, gprsize
    lea             aq, [aq+bx4q*4]
    jmp           bw4q
.w32:
    mova     [aq-32*8], m0
    mova     [aq-32*7], m1
    mova     [aq-32*6], m2
    mova     [aq-32*5], m0
    mova     [aq-32*4], m1
    mova     [aq-32*3], m2
.w16:
    mova     [aq-32*2], m0
    mova     [aq-32*1], m1
    mova     [aq+32*0], m2
.w8:
    mova     [aq+32*1], m0
    mova     [aq+32*2], m1
    mova     [aq+32*3], m2
    dec           bh4d
    jg .loop
    RET
.w4:
    movu      [aq+ 80], m0
    mova      [aq+112], xm1
    dec           bh4d
    jg .loop
    RET
.w2:
    movu      [aq+104], xm0
    movq      [aq+120], xm2
    dec           bh4d
    jg .loop
    RET
.w1:
    movq      [aq+116], xm0
    movd      [aq+124], xm1
    dec           bh4d
    jg .loop
    RET

INIT_ZMM avx512icl
; refmvs_temporal_block *rp, ptrdiff_t stride,
; refmvs_block **rr, uint8_t *ref_sign,
; int col_end8, int row_end8, int col_start8, int row_start8
cglobal save_tmvs, 4, 15, 10, rp, stride, rr, ref_sign, \
                              xend, yend, xstart, ystart
%define base r14-.write1
    lea            r14, [.write1]
    movifnidn    xendd, xendm
    movifnidn    yendd, yendm
    mov        xstartd, xstartm
    mov        ystartd, ystartm
    psllq           m4, [ref_signq]{bcstq}, 8
    vpbroadcastq    m3, [base+save_ref_shuf+8]
    vbroadcasti32x4 m5, [base+cond_shuf512]
    vbroadcasti32x4 m6, [base+save_cond0]
    vpbroadcastd    m7, [base+pb_128]
    mova            m8, [base+save_pack0]
    movu           xm9, [base+save_pack0+4]
    lea            r9d, [xendq*5]
    lea        xstartd, [xstartq*5]
    sub          yendd, ystartd
    add        ystartd, ystartd
    lea        strideq, [strideq*5]
    sub        xstartq, r9
    add          xendd, r9d
    add            rpq, r9
    mov           r10d, 0x1f
    kmovb           k2, r10d
 DEFINE_ARGS rp, stride, rr, x, xend, h, xstart, ystart, b, cand
.loop_y:
    and        ystartd, 30
    mov             xq, xstartq
    mov             bq, [rrq+ystartq*8]
    add        ystartd, 2
    lea             bq, [bq+xendq*4]
.loop_x:
    imul         candq, xq, 0x9999
    sar          candq, 16                   ; x / 5 * 3
    movzx         r10d, byte [bq+candq*8+22] ; cand_b->bs
    movu           xm0, [bq+candq*8+12]      ; cand_b
    movzx         r11d, byte [base+save_tmvs_avx512icl_table+r10*2+0]
    movzx         r10d, byte [base+save_tmvs_avx512icl_table+r10*2+1]
    add            r10, r14
    add          candq, r11
    jge .calc
    movzx         r11d, byte [bq+candq*8+22]
    vinserti32x4   ym0, [bq+candq*8+12], 1
    movzx         r12d, byte [base+save_tmvs_avx512icl_table+r11*2+0]
    movzx         r11d, byte [base+save_tmvs_avx512icl_table+r11*2+1]
    add            r11, r14
    add          candq, r12
    jge .calc
    movzx         r12d, byte [bq+candq*8+22]
    vinserti32x4    m0, [bq+candq*8+12], 2
    movzx         r13d, byte [base+save_tmvs_avx512icl_table+r12*2+0]
    movzx         r12d, byte [base+save_tmvs_avx512icl_table+r12*2+1]
    add            r12, r14
    add          candq, r13
    jge .calc
    vinserti32x4    m0, [bq+candq*8+12], 3
    movzx         r13d, byte [bq+candq*8+22]
    movzx         r13d, byte [base+save_tmvs_avx512icl_table+r13*2+1]
    add            r13, r14
.calc:
    pshufb          m1, m0, m3
    pabsw           m2, m0
    pshufb          m1, m4, m1      ; ref > 0 && res_sign[ref - 1]
    psrlw           m2, 12          ; (abs(mv.x) | abs(mv.y)) < 4096
    psubd           m2, m1
    pshufb          m2, m5           ; c0 c1 c1 c0
    pand            m2, m6
    punpckhqdq      m1, m2, m2
    vpternlogd      m1, m2, m7, 0x56 ; (c0shuf | c1shuf) ^ 0x80
    pshufb          m2, m0, m1
    mova           xm0, xm2
    call           r10
    jge .next_line
    vextracti32x4  xm0, m2, 1
    call           r11
    jge .next_line
    vextracti32x4  xm0, m2, 2
    call           r12
    jge .next_line
    vextracti32x4  xm0, m2, 3
    call           r13
    jl .loop_x
.next_line:
    add            rpq, strideq
    dec             hd
    jg .loop_y
    RET
.write1:
    vmovdqu8 [rpq+xq]{k2}, xm0
    add             xq, 5*1
    ret
.write2:
    pshufb         xm0, xm8
    vmovdqu16 [rpq+xq]{k2}, xm0
    add             xq, 5*2
    ret
.write4:
    vpermb         ym0, ym8, ym0
    vmovdqu32 [rpq+xq]{k2}, ym0
    add             xq, 5*4
    ret
.write8:
    vpermb          m0, m8, m0
    vmovdqu64 [rpq+xq]{k2}, m0
    add             xq, 5*8
    ret
.write16:
    vpermb          m1, m8, m0
    movu   [rpq+xq+ 0], m1
    pshufb         xm0, xm9
    movu   [rpq+xq+64], xm0
    add             xq, 5*16
    ret

INIT_ZMM avx512icl
cglobal splat_mv, 4, 7, 3, rr, a, bx4, bw4, bh4
    vbroadcasti32x4    m0, [aq]
    lea                r1, [splat_mv_avx512icl_table]
    tzcnt            bw4d, bw4d
    lea              bx4d, [bx4q*3]
    pshufb             m0, [splat_mv_shuf]
    movsxd           bw4q, [r1+bw4q*4]
    mov               r6d, bh4m
    add              bw4q, r1
    lea               rrq, [rrq+r6*8]
    mov               r1d, 0x3f
    neg                r6
    kmovb              k1, r1d
    jmp              bw4q
.w1:
    mov                r1, [rrq+r6*8]
    vmovdqu16 [r1+bx4q*4]{k1}, xm0
    inc                r6
    jl .w1
    RET
.w2:
    mov                r1, [rrq+r6*8]
    vmovdqu32 [r1+bx4q*4]{k1}, ym0
    inc                r6
    jl .w2
    RET
.w4:
    mov                r1, [rrq+r6*8]
    vmovdqu64 [r1+bx4q*4]{k1}, m0
    inc                r6
    jl .w4
    RET
.w8:
    pshufd            ym1, ym0, q1021
.w8_loop:
    mov                r1, [rrq+r6*8+0]
    mov                r3, [rrq+r6*8+8]
    movu   [r1+bx4q*4+ 0], m0
    mova   [r1+bx4q*4+64], ym1
    movu   [r3+bx4q*4+ 0], m0
    mova   [r3+bx4q*4+64], ym1
    add                r6, 2
    jl .w8_loop
    RET
.w16:
    pshufd             m1, m0, q1021
    pshufd             m2, m0, q2102
.w16_loop:
    mov                r1, [rrq+r6*8+0]
    mov                r3, [rrq+r6*8+8]
    mova [r1+bx4q*4+64*0], m0
    mova [r1+bx4q*4+64*1], m1
    mova [r1+bx4q*4+64*2], m2
    mova [r3+bx4q*4+64*0], m0
    mova [r3+bx4q*4+64*1], m1
    mova [r3+bx4q*4+64*2], m2
    add                r6, 2
    jl .w16_loop
    RET
.w32:
    pshufd             m1, m0, q1021
    pshufd             m2, m0, q2102
.w32_loop:
    mov                r1, [rrq+r6*8]
    lea                r1, [r1+bx4q*4]
    mova        [r1+64*0], m0
    mova        [r1+64*1], m1
    mova        [r1+64*2], m2
    mova        [r1+64*3], m0
    mova        [r1+64*4], m1
    mova        [r1+64*5], m2
    inc                r6
    jl .w32_loop
    RET
%endif ; ARCH_X86_64
