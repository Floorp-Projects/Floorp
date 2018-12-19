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

%include "config.asm"
%include "ext/x86/x86inc.asm"

SECTION_RODATA 16

; dav1d_obmc_masks[] with 64-x interleaved
obmc_masks: db  0,  0,  0,  0
            ; 2 @4
            db 45, 19, 64,  0
            ; 4 @8
            db 39, 25, 50, 14, 59,  5, 64,  0
            ; 8 @16
            db 36, 28, 42, 22, 48, 16, 53, 11, 57,  7, 61,  3, 64,  0, 64,  0
            ; 16 @32
            db 34, 30, 37, 27, 40, 24, 43, 21, 46, 18, 49, 15, 52, 12, 54, 10
            db 56,  8, 58,  6, 60,  4, 61,  3, 64,  0, 64,  0, 64,  0, 64,  0
            ; 32 @64
            db 33, 31, 35, 29, 36, 28, 38, 26, 40, 24, 41, 23, 43, 21, 44, 20
            db 45, 19, 47, 17, 48, 16, 50, 14, 51, 13, 52, 12, 53, 11, 55,  9
            db 56,  8, 57,  7, 58,  6, 59,  5, 60,  4, 60,  4, 61,  3, 62,  2
            db 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0, 64,  0
blend_shuf: db  0,  1,  0,  1,  0,  1,  0,  1,  2,  3,  2,  3,  2,  3,  2,  3

pb_64:   times 16 db 64
pw_8:    times 8 dw 8
pw_26:   times 8 dw 26
pw_258:  times 8 dw 258
pw_512:  times 8 dw 512
pw_1024: times 8 dw 1024
pw_2048: times 8 dw 2048

%macro BIDIR_JMP_TABLE 1-*
    ;evaluated at definition time (in loop below)
    %xdefine %1_table (%%table - 2*%2)
    %xdefine %%base %1_table
    %xdefine %%prefix mangle(private_prefix %+ _%1)
    ; dynamically generated label
    %%table:
    %rep %0 - 1 ; repeat for num args
        dd %%prefix %+ .w%2 - %%base
        %rotate 1
    %endrep
%endmacro

BIDIR_JMP_TABLE avg_ssse3,        4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_avg_ssse3,      4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE mask_ssse3,       4, 8, 16, 32, 64, 128
BIDIR_JMP_TABLE w_mask_420_ssse3, 4, 8, 16, 16, 16, 16
BIDIR_JMP_TABLE blend_ssse3,      4, 8, 16, 32
BIDIR_JMP_TABLE blend_v_ssse3, 2, 4, 8, 16, 32
BIDIR_JMP_TABLE blend_h_ssse3, 2, 4, 8, 16, 16, 16, 16

SECTION .text

INIT_XMM ssse3

%if WIN64
DECLARE_REG_TMP 6, 4
%else
DECLARE_REG_TMP 6, 7
%endif

%macro BIDIR_FN 1 ; op
    %1                    0
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4_loop:
    %1_INC_PTR            2
    %1                    0
    lea                dstq, [dstq+strideq*4]
.w4: ; tile 4x
    movd   [dstq          ], m0      ; copy dw[0]
    pshuflw              m1, m0, q1032 ; swap dw[1] and dw[0]
    movd   [dstq+strideq*1], m1      ; copy dw[1]
    punpckhqdq           m0, m0      ; swap dw[3,2] with dw[1,0]
    movd   [dstq+strideq*2], m0      ; dw[2]
    psrlq                m0, 32      ; shift right in dw[3]
    movd   [dstq+stride3q ], m0      ; copy
    sub                  hd, 4
    jg .w4_loop
    RET
.w8_loop:
    %1_INC_PTR            2
    %1                    0
    lea                dstq, [dstq+strideq*2]
.w8:
    movq   [dstq          ], m0
    movhps [dstq+strideq*1], m0
    sub                  hd, 2
    jg .w8_loop
    RET
.w16_loop:
    %1_INC_PTR            2
    %1                    0
    lea                dstq, [dstq+strideq]
.w16:
    mova   [dstq          ], m0
    dec                  hd
    jg .w16_loop
    RET
.w32_loop:
    %1_INC_PTR            4
    %1                    0
    lea                dstq, [dstq+strideq]
.w32:
    mova   [dstq          ], m0
    %1                    2
    mova   [dstq + 16     ], m0
    dec                  hd
    jg .w32_loop
    RET
.w64_loop:
    %1_INC_PTR            8
    %1                    0
    add                dstq, strideq
.w64:
    %assign i 0
    %rep 4
    mova   [dstq + i*16   ], m0
    %assign i i+1
    %if i < 4
    %1                    2*i
    %endif
    %endrep
    dec                  hd
    jg .w64_loop
    RET
.w128_loop:
    %1_INC_PTR            16
    %1                    0
    add                dstq, strideq
.w128:
    %assign i 0
    %rep 8
    mova   [dstq + i*16   ], m0
    %assign i i+1
    %if i < 8
    %1                    2*i
    %endif
    %endrep
    dec                  hd
    jg .w128_loop
    RET
%endmacro

%macro AVG 1 ; src_offset
    ; writes AVG of tmp1 tmp2 uint16 coeffs into uint8 pixel
    mova                 m0, [tmp1q+(%1+0)*mmsize] ; load 8 coef(2bytes) from tmp1
    paddw                m0, [tmp2q+(%1+0)*mmsize] ; load/add 8 coef(2bytes) tmp2
    mova                 m1, [tmp1q+(%1+1)*mmsize]
    paddw                m1, [tmp2q+(%1+1)*mmsize]
    pmulhrsw             m0, m2
    pmulhrsw             m1, m2
    packuswb             m0, m1 ; pack/trunc 16 bits from m0 & m1 to 8 bit
%endmacro

%macro AVG_INC_PTR 1
    add               tmp1q, %1*mmsize
    add               tmp2q, %1*mmsize
%endmacro

cglobal avg, 4, 7, 3, dst, stride, tmp1, tmp2, w, h, stride3
    lea                  r6, [avg_ssse3_table]
    tzcnt                wd, wm ; leading zeros
    movifnidn            hd, hm ; move h(stack) to h(register) if not already that register
    movsxd               wq, dword [r6+wq*4] ; push table entry matching the tile width (tzcnt) in widen reg
    mova                 m2, [pw_1024+r6-avg_ssse3_table] ; fill m2 with shift/align
    add                  wq, r6
    BIDIR_FN            AVG

%macro W_AVG 1 ; src_offset
    ; (a * weight + b * (16 - weight) + 128) >> 8
    ; = ((a - b) * weight + (b << 4) + 128) >> 8
    ; = ((((b - a) * (-weight << 12)) >> 16) + b + 8) >> 4
    mova                 m0,     [tmp2q+(%1+0)*mmsize]
    psubw                m2, m0, [tmp1q+(%1+0)*mmsize]
    mova                 m1,     [tmp2q+(%1+1)*mmsize]
    psubw                m3, m1, [tmp1q+(%1+1)*mmsize]
    paddw                m2, m2 ; compensate for the weight only being half
    paddw                m3, m3 ; of what it should be
    pmulhw               m2, m4 ; (b-a) * (-weight << 12)
    pmulhw               m3, m4 ; (b-a) * (-weight << 12)
    paddw                m0, m2 ; ((b-a) * -weight) + b
    paddw                m1, m3
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    packuswb             m0, m1
%endmacro

%define W_AVG_INC_PTR AVG_INC_PTR

cglobal w_avg, 4, 7, 6, dst, stride, tmp1, tmp2, w, h, stride3
    lea                  r6, [w_avg_ssse3_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movd                 m0, r6m
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
    movsxd               wq, dword [r6+wq*4]
    pxor                 m4, m4
    psllw                m0, 11 ; can't shift by 12, sign bit must be preserved
    psubw                m4, m0
    mova                 m5, [pw_2048+r6-w_avg_ssse3_table]
    add                  wq, r6
    BIDIR_FN          W_AVG

%macro MASK 1 ; src_offset
    ; (a * m + b * (64 - m) + 512) >> 10
    ; = ((a - b) * m + (b << 6) + 512) >> 10
    ; = ((((b - a) * (-m << 10)) >> 16) + b + 8) >> 4
    mova                 m3,     [maskq+(%1+0)*(mmsize/2)]
    mova                 m0,     [tmp2q+(%1+0)*mmsize] ; b
    psubw                m1, m0, [tmp1q+(%1+0)*mmsize] ; b - a
    mova                 m6, m3      ; m
    psubb                m3, m4, m6  ; -m
    paddw                m1, m1     ; (b - a) << 1
    paddb                m3, m3     ; -m << 1
    punpcklbw            m2, m4, m3 ; -m << 9 (<< 8 when ext as uint16)
    pmulhw               m1, m2     ; (-m * (b - a)) << 10
    paddw                m0, m1     ; + b
    mova                 m1,     [tmp2q+(%1+1)*mmsize] ; b
    psubw                m2, m1, [tmp1q+(%1+1)*mmsize] ; b - a
    paddw                m2, m2  ; (b - a) << 1
    mova                 m6, m3  ; (-m << 1)
    punpckhbw            m3, m4, m6 ; (-m << 9)
    pmulhw               m2, m3 ; (-m << 9)
    paddw                m1, m2 ; (-m * (b - a)) << 10
    pmulhrsw             m0, m5 ; round
    pmulhrsw             m1, m5 ; round
    packuswb             m0, m1 ; interleave 16 -> 8
%endmacro

%macro MASK_INC_PTR 1
    add               maskq, %1*mmsize/2
    add               tmp1q, %1*mmsize
    add               tmp2q, %1*mmsize
%endmacro

%if ARCH_X86_64
cglobal mask, 4, 8, 7, dst, stride, tmp1, tmp2, w, h, mask, stride3
    movifnidn            hd, hm
%else
cglobal mask, 4, 7, 7, dst, stride, tmp1, tmp2, w, mask, stride3
%define hd dword r5m
%endif
    lea                  r6, [mask_ssse3_table]
    tzcnt                wd, wm
    movsxd               wq, dword [r6+wq*4]
    pxor                 m4, m4
    mova                 m5, [pw_2048+r6-mask_ssse3_table]
    add                  wq, r6
    mov               maskq, r6m
    BIDIR_FN           MASK
%undef hd

%if ARCH_X86_64
 %define reg_pw_8         m8
 %define reg_pw_27        m9
 %define reg_pw_2048      m10
%else
 %define reg_pw_8         [pw_8]
 %define reg_pw_27        [pw_26] ; 64 - 38
 %define reg_pw_2048      [pw_2048]
%endif

%macro W_MASK_420_B 2 ; src_offset in bytes, mask_out
    ;**** do m0 = u16.dst[7..0], m%2 = u16.m[7..0] ****
    mova                 m0, [tmp1q+(%1)]
    mova                 m1, [tmp2q+(%1)]
    psubw                m1, m0 ; tmp1 - tmp2
    pabsw                m3, m1 ; abs(tmp1 - tmp2)
    paddw                m3, reg_pw_8 ; abs(tmp1 - tmp2) + 8
    psrlw                m3, 8  ; (abs(tmp1 - tmp2) + 8) >> 8
    psubusw             m%2, reg_pw_27, m3 ; 64 - min(m, 64)
    psllw                m2, m%2, 10
    pmulhw               m1, m2 ; tmp2 * ()
    paddw                m0, m1 ; tmp1 + ()
    ;**** do m1 = u16.dst[7..0], m%2 = u16.m[7..0] ****
    mova                 m1, [tmp1q+(%1)+mmsize]
    mova                 m2, [tmp2q+(%1)+mmsize]
    psubw                m2, m1 ; tmp1 - tmp2
    pabsw                m7, m2 ; abs(tmp1 - tmp2)
    paddw                m7, reg_pw_8 ; abs(tmp1 - tmp2) + 8
    psrlw                m7, 8  ; (abs(tmp1 - tmp2) + 8) >> 8
    psubusw              m3, reg_pw_27, m7 ; 64 - min(m, 64)
    phaddw              m%2, m3 ; pack both u16.m[8..0]runs as u8.m [15..0]
    psllw                m3, 10
    pmulhw               m2, m3
    paddw                m1, m2
    ;********
    pmulhrsw             m0, reg_pw_2048 ; round/scale 2048
    pmulhrsw             m1, reg_pw_2048 ; round/scale 2048
    packuswb             m0, m1 ; concat m0 = u8.dst[15..0]
%endmacro

%macro W_MASK_420 2
    W_MASK_420_B (%1*16), %2
%endmacro

%if ARCH_X86_64
; args: dst, stride, tmp1, tmp2, w, h, mask, sign
cglobal w_mask_420, 4, 9, 11, dst, stride, tmp1, tmp2, w, h, mask, stride3
    lea                  r7, [w_mask_420_ssse3_table]
    mov                  wd, wm
    tzcnt               r8d, wd
    movifnidn            hd, hm
    mov               maskq, maskmp
    movd                 m0, r7m
    pshuflw              m0, m0, q0000 ; sign
    punpcklqdq           m0, m0
    movsxd               r8, dword [r7+r8*4]
    mova           reg_pw_8, [pw_8]
    mova          reg_pw_27, [pw_26] ; 64 - 38
    mova        reg_pw_2048, [pw_2048]
    mova                 m6, [pw_258] ; 64 * 4 + 2
    psubw                m6, m0
    add                  r8, r7
    W_MASK_420            0, 4
    lea            stride3q, [strideq*3]
    jmp                  r8
    %define dst_bak      r8
    %define loop_w       r7
    %define orig_w       wq
%else
cglobal w_mask_420, 4, 7, 8, dst, stride, tmp1, tmp2, w, mask, stride3
    tzcnt               r6d, r4m
    mov                  wd, w_mask_420_ssse3_table
    add                  wd, [wq+r6*4]
    mov               maskq, r6mp
    movd                 m0, r7m
    pshuflw              m0, m0, q0000 ; sign
    punpcklqdq           m0, m0
    mova                 m6, [pw_258] ; 64 * 4 + 2
    psubw                m6, m0
    W_MASK_420            0, 4
    lea            stride3q, [strideq*3]
    jmp                  wd
    %define dst_bak     r0m
    %define loop_w      r6q
    %define orig_w      r4m
    %define hd    dword r5m
%endif
.w4_loop:
    add               tmp1q, 2*16
    add               tmp2q, 2*16
    W_MASK_420            0, 4
    lea                dstq, [dstq+strideq*4]
    add               maskq, 4
.w4:
    movd   [dstq          ], m0 ; copy m0[0]
    pshuflw              m1, m0, q1032
    movd   [dstq+strideq*1], m1 ; copy m0[1]
    punpckhqdq           m0, m0
    movd   [dstq+strideq*2], m0 ; copy m0[2]
    psrlq                m0, 32
    movd   [dstq+stride3q ], m0 ; copy m0[3]
    pshufd               m5, m4, q3131; DBDB even lines repeated
    pshufd               m4, m4, q2020; CACA odd lines repeated
    psubw                m1, m6, m4   ; m9 == 64 * 4 + 2
    psubw                m1, m5       ; C-D A-B C-D A-B
    psrlw                m1, 2        ; >> 2
    packuswb             m1, m1
    movd            [maskq], m1
    sub                  hd, 4
    jg .w4_loop
    RET
.w8_loop:
    add               tmp1q, 2*16
    add               tmp2q, 2*16
    W_MASK_420            0, 4
    lea                dstq, [dstq+strideq*2]
    add               maskq, 4
.w8:
    movq   [dstq          ], m0
    movhps [dstq+strideq*1], m0
    pshufd               m1, m4, q3232
    psubw                m0, m6, m4
    psubw                m0, m1
    psrlw                m0, 2
    packuswb             m0, m0
    movd            [maskq], m0
    sub                  hd, 2
    jg .w8_loop
    RET
.w16: ; w32/64/128
    mov             dst_bak, dstq
    mov              loop_w, orig_w ; use width as counter
%if ARCH_X86_32
    mov                  wq, orig_w ; because we altered it in 32bit setup
%endif
    jmp .w16ge_inner_loop_first
.w16ge_loop:
    lea               tmp1q, [tmp1q+wq*2] ; skip even line pixels
    lea               tmp2q, [tmp2q+wq*2] ; skip even line pixels
    lea                dstq, [dstq+strideq*2]
    mov             dst_bak, dstq
    mov              loop_w, orig_w
.w16ge_inner_loop:
    W_MASK_420_B           0, 4
.w16ge_inner_loop_first:
    mova   [dstq          ], m0
    W_MASK_420_B       wq*2, 5  ; load matching even line (offset = widthpx * (16+16))
    mova   [dstq+strideq*1], m0
    psubw                m1, m6, m4 ; m9 == 64 * 4 + 2
    psubw                m1, m5     ; - odd line mask
    psrlw                m1, 2      ; >> 2
    packuswb             m1, m1
    movq            [maskq], m1
    add               tmp1q, 2*16
    add               tmp2q, 2*16
    add               maskq, 8
    add                dstq, 16
    sub              loop_w, 16
    jg .w16ge_inner_loop
    mov                dstq, dst_bak
    sub                  hd, 2
    jg .w16ge_loop
    RET

%undef reg_pw_8
%undef reg_pw_27
%undef reg_pw_2048
%undef dst_bak
%undef loop_w
%undef orig_w
%undef hd

%macro BLEND_64M 4; a, b, mask1, mask2
    punpcklbw            m0, %1, %2; {b;a}[7..0]
    punpckhbw            %1, %2    ; {b;a}[15..8]
    pmaddubsw            m0, %3    ; {b*m[0] + (64-m[0])*a}[7..0] u16
    pmaddubsw            %1, %4    ; {b*m[1] + (64-m[1])*a}[15..8] u16
    pmulhrsw             m0, m5    ; {((b*m[0] + (64-m[0])*a) + 1) / 32}[7..0] u16
    pmulhrsw             %1, m5    ; {((b*m[1] + (64-m[0])*a) + 1) / 32}[15..8] u16
    packuswb             m0, %1    ; {blendpx}[15..0] u8
%endmacro

%macro BLEND 2; a, b
    psubb                m3, m4, m0 ; m3 = (64 - m)
    punpcklbw            m2, m3, m0 ; {m;(64-m)}[7..0]
    punpckhbw            m3, m0     ; {m;(64-m)}[15..8]
    BLEND_64M            %1, %2, m2, m3
%endmacro

cglobal blend, 3, 7, 7, dst, ds, tmp, w, h, mask
%define base r6-blend_ssse3_table
    lea                  r6, [blend_ssse3_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movifnidn         maskq, maskmp
    movsxd               wq, dword [r6+wq*4]
    mova                 m4, [base+pb_64]
    mova                 m5, [base+pw_512]
    add                  wq, r6
    lea                  r6, [dsq*3]
    jmp                  wq
.w4:
    movq                 m0, [maskq]; m
    movd                 m1, [dstq+dsq*0] ; a
    movd                 m6, [dstq+dsq*1]
    punpckldq            m1, m6
    movq                 m6, [tmpq] ; b
    psubb                m3, m4, m0 ; m3 = (64 - m)
    punpcklbw            m2, m3, m0 ; {m;(64-m)}[7..0]
    punpcklbw            m1, m6    ; {b;a}[7..0]
    pmaddubsw            m1, m2    ; {b*m[0] + (64-m[0])*a}[7..0] u16
    pmulhrsw             m1, m5    ; {((b*m[0] + (64-m[0])*a) + 1) / 32}[7..0] u16
    packuswb             m1, m0    ; {blendpx}[15..0] u8
    movd       [dstq+dsq*0], m1
    psrlq                m1, 32
    movd       [dstq+dsq*1], m1
    add               maskq, 8
    add                tmpq, 8
    lea                dstq, [dstq+dsq*2] ; dst_stride * 2
    sub                  hd, 2
    jg .w4
    RET
.w8:
    mova                 m0, [maskq]; m
    movq                 m1, [dstq+dsq*0] ; a
    movhps               m1, [dstq+dsq*1]
    mova                 m6, [tmpq] ; b
    BLEND                m1, m6
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    add               maskq, 16
    add                tmpq, 16
    lea                dstq, [dstq+dsq*2] ; dst_stride * 2
    sub                  hd, 2
    jg .w8
    RET
.w16:
    mova                 m0, [maskq]; m
    mova                 m1, [dstq] ; a
    mova                 m6, [tmpq] ; b
    BLEND                m1, m6
    mova             [dstq], m0
    add               maskq, 16
    add                tmpq, 16
    add                dstq, dsq ; dst_stride
    dec                  hd
    jg .w16
    RET
.w32:
    %assign i 0
    %rep 2
    mova                 m0, [maskq+16*i]; m
    mova                 m1, [dstq+16*i] ; a
    mova                 m6, [tmpq+16*i] ; b
    BLEND                m1, m6
    mova        [dstq+i*16], m0
    %assign i i+1
    %endrep
    add               maskq, 32
    add                tmpq, 32
    add                dstq, dsq ; dst_stride
    dec                  hd
    jg .w32
    RET

cglobal blend_v, 3, 6, 8, dst, ds, tmp, w, h, mask
%define base r5-blend_v_ssse3_table
    lea                  r5, [blend_v_ssse3_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, dword [r5+wq*4]
    mova                 m5, [base+pw_512]
    add                  wq, r5
    add               maskq, obmc_masks-blend_v_ssse3_table
    jmp                  wq
.w2:
    movd                 m3, [maskq+4]
    punpckldq            m3, m3
    ; 2 mask blend is provided for 4 pixels / 2 lines
.w2_loop:
    movd                 m1, [dstq+dsq*0] ; a {..;a;a}
    pinsrw               m1, [dstq+dsq*1], 1
    movd                 m2, [tmpq] ; b
    punpcklbw            m0, m1, m2; {b;a}[7..0]
    pmaddubsw            m0, m3    ; {b*m + (64-m)*a}[7..0] u16
    pmulhrsw             m0, m5    ; {((b*m + (64-m)*a) + 1) / 32}[7..0] u16
    packuswb             m0, m1    ; {blendpx}[8..0] u8
    movd                r3d, m0
    mov        [dstq+dsq*0], r3w
    shr                 r3d, 16
    mov        [dstq+dsq*1], r3w
    add                tmpq, 2*2
    lea                dstq, [dstq + dsq * 2]
    sub                  hd, 2
    jg .w2_loop
    RET
.w4:
    movddup              m3, [maskq+8]
    ; 4 mask blend is provided for 8 pixels / 2 lines
.w4_loop:
    movd                 m1, [dstq+dsq*0] ; a
    movd                 m2, [dstq+dsq*1] ;
    punpckldq            m1, m2
    movq                 m2, [tmpq] ; b
    punpcklbw            m1, m2    ; {b;a}[7..0]
    pmaddubsw            m1, m3    ; {b*m + (64-m)*a}[7..0] u16
    pmulhrsw             m1, m5    ; {((b*m + (64-m)*a) + 1) / 32}[7..0] u16
    packuswb             m1, m1    ; {blendpx}[8..0] u8
    movd             [dstq], m1
    psrlq                m1, 32
    movd       [dstq+dsq*1], m1
    add                tmpq, 2*4
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w4_loop
    RET
.w8:
    mova                 m3, [maskq+16]
    ; 8 mask blend is provided for 16 pixels
.w8_loop:
    movq                 m1, [dstq+dsq*0] ; a
    movhps               m1, [dstq+dsq*1]
    mova                 m2, [tmpq]; b
    BLEND_64M            m1, m2, m3, m3
    movq       [dstq+dsq*0], m0
    punpckhqdq           m0, m0
    movq       [dstq+dsq*1], m0
    add                tmpq, 16
    lea                dstq, [dstq+dsq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
.w16:
    ; 16 mask blend is provided for 32 pixels
    mova                  m3, [maskq+32] ; obmc_masks_16[0] (64-m[0])
    mova                  m4, [maskq+48] ; obmc_masks_16[1] (64-m[1])
.w16_loop:
    mova                 m1, [dstq] ; a
    mova                 m2, [tmpq] ; b
    BLEND_64M            m1, m2, m3, m4
    mova             [dstq], m0
    add                tmpq, 16
    add                dstq, dsq
    dec                  hd
    jg .w16_loop
    RET
.w32:
    mova                 m3, [maskq+64 ] ; obmc_masks_32[0] (64-m[0])
    mova                 m4, [maskq+80 ] ; obmc_masks_32[1] (64-m[1])
    mova                 m6, [maskq+96 ] ; obmc_masks_32[2] (64-m[2])
    mova                 m7, [maskq+112] ; obmc_masks_32[3] (64-m[3])
    ; 16 mask blend is provided for 64 pixels
.w32_loop:
    mova                 m1, [dstq+16*0] ; a
    mova                 m2, [tmpq+16*0] ; b
    BLEND_64M            m1, m2, m3, m4
    mova        [dstq+16*0], m0
    mova                 m1, [dstq+16*1] ; a
    mova                 m2, [tmpq+16*1] ; b
    BLEND_64M            m1, m2, m6, m7
    mova        [dstq+16*1], m0
    add                tmpq, 32
    add                dstq, dsq
    dec                  hd
    jg .w32_loop
    RET

cglobal blend_h, 4, 7, 6, dst, ds, tmp, w, h, mask
%define base r5-blend_h_ssse3_table
    lea                  r5, [blend_h_ssse3_table]
    mov                 r6d, wd
    tzcnt                wd, wd
    mov                  hd, hm
    movsxd               wq, dword [r5+wq*4]
    mova                 m5, [base+pw_512]
    add                  wq, r5
    lea               maskq, [base+obmc_masks+hq*4]
    neg                  hq
    jmp                  wq
.w2:
    movd                 m0, [dstq+dsq*0]
    pinsrw               m0, [dstq+dsq*1], 1
    movd                 m2, [maskq+hq*2]
    movd                 m1, [tmpq]
    punpcklwd            m2, m2
    punpcklbw            m0, m1
    pmaddubsw            m0, m2
    pmulhrsw             m0, m5
    packuswb             m0, m0
    movd                r3d, m0
    mov        [dstq+dsq*0], r3w
    shr                 r3d, 16
    mov        [dstq+dsq*1], r3w
    lea                dstq, [dstq+dsq*2]
    add                tmpq, 2*2
    add                  hq, 2
    jl .w2
    RET
.w4:
    mova                 m3, [blend_shuf]
.w4_loop:
    movd                 m0, [dstq+dsq*0]
    movd                 m2, [dstq+dsq*1]
    punpckldq            m0, m2 ; a
    movq                 m1, [tmpq] ; b
    movq                 m2, [maskq+hq*2] ; m
    pshufb               m2, m3
    punpcklbw            m0, m1
    pmaddubsw            m0, m2
    pmulhrsw             m0, m5
    packuswb             m0, m0
    movd       [dstq+dsq*0], m0
    psrlq                m0, 32
    movd       [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    add                tmpq, 4*2
    add                  hq, 2
    jl .w4_loop
    RET
.w8:
    movd                 m4, [maskq+hq*2]
    punpcklwd            m4, m4
    pshufd               m3, m4, q0000
    pshufd               m4, m4, q1111
    movq                 m1, [dstq+dsq*0] ; a
    movhps               m1, [dstq+dsq*1]
    mova                 m2, [tmpq]
    BLEND_64M            m1, m2, m3, m4
    movq       [dstq+dsq*0], m0
    movhps     [dstq+dsq*1], m0
    lea                dstq, [dstq+dsq*2]
    add                tmpq, 8*2
    add                  hq, 2
    jl .w8
    RET
; w16/w32/w64/w128
.w16:
    sub                 dsq, r6
.w16_loop0:
    movd                 m3, [maskq+hq*2]
    pshuflw              m3, m3, q0000
    punpcklqdq           m3, m3
    mov                  wd, r6d
.w16_loop:
    mova                 m1, [dstq] ; a
    mova                 m2, [tmpq] ; b
    BLEND_64M            m1, m2, m3, m3
    mova             [dstq], m0
    add                dstq, 16
    add                tmpq, 16
    sub                  wd, 16
    jg .w16_loop
    add                dstq, dsq
    inc                  hq
    jl .w16_loop0
    RET
