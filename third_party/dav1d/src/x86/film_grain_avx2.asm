; Copyright © 2019-2022, VideoLAN and dav1d authors
; Copyright © 2019-2022, Two Orioles, LLC
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
pb_8x_27_17_8x_17_27: times 8 db 27, 17
                      times 8 db 17, 27
pw_1024: times 16 dw 1024
pb_mask: db 0, 0x80, 0x80, 0, 0x80, 0, 0, 0x80, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0
gen_shufE:     db  0,  1,  8,  9,  2,  3, 10, 11,  4,  5, 12, 13,  6,  7, 14, 15
gen_shufA:     db  0,  1,  2,  3,  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9
gen_shufB:     db  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11
gen_shufC:     db  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13
gen_shufD:     db  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15
rnd_next_upperbit_mask: dw 0x100B, 0x2016, 0x402C, 0x8058
byte_blend: db 0, 0, 0, 0xff, 0, 0, 0, 0
pw_seed_xor: times 2 dw 0xb524
             times 2 dw 0x49d8
pb_23_22: db 23, 22
          times 3 db 0, 32
pb_1: times 4 db 1
hmul_bits: dw 32768, 16384, 8192, 4096
round: dw 2048, 1024, 512
mul_bits: dw 256, 128, 64, 32, 16
round_vals: dw 32, 64, 128, 256, 512
max: dw 255, 240, 235
min: dw 0, 16
pb_27_17_17_27: db 27, 17, 17, 27
                times 2 db 0, 32
pw_1: dw 1

%macro JMP_TABLE 2-*
    %1_8bpc_%2_table:
    %xdefine %%base %1_8bpc_%2_table
    %xdefine %%prefix mangle(private_prefix %+ _%1_8bpc_%2)
    %rep %0 - 2
        dd %%prefix %+ .ar%3 - %%base
        %rotate 1
    %endrep
%endmacro

ALIGN 4
JMP_TABLE generate_grain_y,      avx2, 0, 1, 2, 3
JMP_TABLE generate_grain_uv_420, avx2, 0, 1, 2, 3
JMP_TABLE generate_grain_uv_422, avx2, 0, 1, 2, 3
JMP_TABLE generate_grain_uv_444, avx2, 0, 1, 2, 3

struc FGData
    .seed:                      resd 1
    .num_y_points:              resd 1
    .y_points:                  resb 14 * 2
    .chroma_scaling_from_luma:  resd 1
    .num_uv_points:             resd 2
    .uv_points:                 resb 2 * 10 * 2
    .scaling_shift:             resd 1
    .ar_coeff_lag:              resd 1
    .ar_coeffs_y:               resb 24
    .ar_coeffs_uv:              resb 2 * 28 ; includes padding
    .ar_coeff_shift:            resq 1
    .grain_scale_shift:         resd 1
    .uv_mult:                   resd 2
    .uv_luma_mult:              resd 2
    .uv_offset:                 resd 2
    .overlap_flag:              resd 1
    .clip_to_restricted_range:  resd 1
endstruc

cextern gaussian_sequence

SECTION .text

%macro REPX 2-*
    %xdefine %%f(x) %1
%rep %0 - 1
    %rotate 1
    %%f(%1)
%endrep
%endmacro

INIT_YMM avx2
cglobal generate_grain_y_8bpc, 2, 9, 8, buf, fg_data
%define base r4-generate_grain_y_8bpc_avx2_table
    lea              r4, [generate_grain_y_8bpc_avx2_table]
    vpbroadcastw    xm0, [fg_dataq+FGData.seed]
    mov             r6d, [fg_dataq+FGData.grain_scale_shift]
    movq            xm1, [base+rnd_next_upperbit_mask]
    movsxd           r5, [fg_dataq+FGData.ar_coeff_lag]
    movq            xm4, [base+mul_bits]
    movq            xm5, [base+hmul_bits]
    mov              r7, -73*82
    mova            xm6, [base+pb_mask]
    sub            bufq, r7
    vpbroadcastw    xm7, [base+round+r6*2]
    lea              r6, [gaussian_sequence]
    movsxd           r5, [r4+r5*4]
.loop:
    pand            xm2, xm0, xm1
    psrlw           xm3, xm2, 10
    por             xm2, xm3            ; bits 0xf, 0x1e, 0x3c and 0x78 are set
    pmullw          xm2, xm4            ; bits 0x0f00 are set
    pmulhuw         xm0, xm5
    pshufb          xm3, xm6, xm2       ; set 15th bit for next 4 seeds
    psllq           xm2, xm3, 30
    por             xm2, xm3
    psllq           xm3, xm2, 15
    por             xm2, xm0            ; aggregate each bit into next seed's high bit
    por             xm3, xm2            ; 4 next output seeds
    pshuflw         xm0, xm3, q3333
    psrlw           xm3, 5
    pand            xm2, xm0, xm1
    movq             r2, xm3
    psrlw           xm3, xm2, 10
    por             xm2, xm3
    pmullw          xm2, xm4
    pmulhuw         xm0, xm5
    movzx           r3d, r2w
    pshufb          xm3, xm6, xm2
    psllq           xm2, xm3, 30
    por             xm2, xm3
    psllq           xm3, xm2, 15
    por             xm0, xm2
    movd            xm2, [r6+r3*2]
    rorx             r3, r2, 32
    por             xm3, xm0
    shr             r2d, 16
    pinsrw          xm2, [r6+r2*2], 1
    pshuflw         xm0, xm3, q3333
    movzx           r2d, r3w
    psrlw           xm3, 5
    pinsrw          xm2, [r6+r2*2], 2
    shr             r3d, 16
    movq             r2, xm3
    pinsrw          xm2, [r6+r3*2], 3
    movzx           r3d, r2w
    pinsrw          xm2, [r6+r3*2], 4
    rorx             r3, r2, 32
    shr             r2d, 16
    pinsrw          xm2, [r6+r2*2], 5
    movzx           r2d, r3w
    pinsrw          xm2, [r6+r2*2], 6
    shr             r3d, 16
    pinsrw          xm2, [r6+r3*2], 7
    pmulhrsw        xm2, xm7
    packsswb        xm2, xm2
    movq      [bufq+r7], xm2
    add              r7, 8
    jl .loop

    ; auto-regression code
    add              r5, r4
    jmp              r5

.ar1:
    DEFINE_ARGS buf, fg_data, cf3, shift, val3, min, max, x, val0
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    movsx          cf3d, byte [fg_dataq+FGData.ar_coeffs_y+3]
    movd            xm5, [fg_dataq+FGData.ar_coeffs_y]
    mova            xm2, [base+gen_shufC]
    DEFINE_ARGS buf, h, cf3, shift, val3, min, max, x, val0
    pinsrb          xm5, [base+pb_1], 3
    vpbroadcastw    xm3, [base+round_vals+shiftq*2-12]    ; rnd
    pmovsxbw        xm5, xm5
    pshufd          xm4, xm5, q0000
    pshufd          xm5, xm5, q1111
    sub            bufq, 82*73-(82*3+79)
    mov              hd, 70
    mov            mind, -128
    mov            maxd, 127
.y_loop_ar1:
    mov              xq, -76
    movsx         val3d, byte [bufq+xq-1]
.x_loop_ar1:
    pmovsxbw        xm1, [bufq+xq-82-3]
    pshufb          xm0, xm1, xm2
    punpckhwd       xm1, xm3
    pmaddwd         xm0, xm4
    pmaddwd         xm1, xm5
    paddd           xm0, xm1
.x_loop_ar1_inner:
    movd          val0d, xm0
    psrldq          xm0, 4
    imul          val3d, cf3d
    add           val3d, val0d
    movsx         val0d, byte [bufq+xq]
    sarx          val3d, val3d, shiftd
    add           val3d, val0d
    cmp           val3d, maxd
    cmovns        val3d, maxd
    cmp           val3d, mind
    cmovs         val3d, mind
    mov       [bufq+xq], val3b
    ; keep val3d in-place as left for next x iteration
    inc              xq
    jz .x_loop_ar1_end
    test             xb, 3
    jnz .x_loop_ar1_inner
    jmp .x_loop_ar1
.x_loop_ar1_end:
    add            bufq, 82
    dec              hd
    jg .y_loop_ar1
.ar0:
    RET

.ar2:
%if WIN64
    ; xmm6 and xmm7 already saved
    %assign xmm_regs_used 16
    %assign stack_size_padded 168
    SUB             rsp, stack_size_padded
    movaps   [rsp+16*2], xmm8
    movaps   [rsp+16*3], xmm9
    movaps   [rsp+16*4], xmm10
    movaps   [rsp+16*5], xmm11
    movaps   [rsp+16*6], xmm12
    movaps   [rsp+16*7], xmm13
    movaps   [rsp+16*8], xmm14
    movaps   [rsp+16*9], xmm15
%endif
    DEFINE_ARGS buf, fg_data, h, x
    mov             r6d, [fg_dataq+FGData.ar_coeff_shift]
    pmovsxbw        xm7, [fg_dataq+FGData.ar_coeffs_y+0]    ; cf0-7
    movd            xm9, [fg_dataq+FGData.ar_coeffs_y+8]    ; cf8-11
    vpbroadcastd   xm10, [base+round_vals-14+r6*2]
    movq           xm11, [base+byte_blend+1]
    pmovsxbw        xm9, xm9
    pshufd          xm4, xm7, q0000
    mova           xm12, [base+gen_shufA]
    pshufd          xm5, xm7, q3333
    mova           xm13, [base+gen_shufB]
    pshufd          xm6, xm7, q1111
    mova           xm14, [base+gen_shufC]
    pshufd          xm7, xm7, q2222
    mova           xm15, [base+gen_shufD]
    pshufd          xm8, xm9, q0000
    psrld          xm10, 16
    pshufd          xm9, xm9, q1111
    sub            bufq, 82*73-(82*3+79)
    mov              hd, 70
.y_loop_ar2:
    mov              xq, -76
.x_loop_ar2:
    pmovsxbw        xm0, [bufq+xq-82*2-2]   ; y=-2,x=[-2,+5]
    pmovsxbw        xm1, [bufq+xq-82*1-2]   ; y=-1,x=[-2,+5]
    pshufb          xm2, xm0, xm12
    pmaddwd         xm2, xm4
    pshufb          xm3, xm1, xm13
    pmaddwd         xm3, xm5
    paddd           xm2, xm3
    pshufb          xm3, xm0, xm14
    pmaddwd         xm3, xm6
    punpckhqdq      xm0, xm0
    punpcklwd       xm0, xm1
    pmaddwd         xm0, xm7
    pshufb          xm1, xm15
    pmaddwd         xm1, xm8
    paddd           xm2, xm10
    paddd           xm2, xm3
    paddd           xm0, xm1
    paddd           xm2, xm0
    movq            xm0, [bufq+xq-2]        ; y=0,x=[-2,+5]
.x_loop_ar2_inner:
    pmovsxbw        xm1, xm0
    pmaddwd         xm3, xm9, xm1
    psrldq          xm1, 4                  ; y=0,x=0
    paddd           xm3, xm2
    psrldq          xm2, 4                  ; shift top to next pixel
    psrad           xm3, [fg_dataq+FGData.ar_coeff_shift]
    ; don't packssdw since we only care about one value
    paddw           xm3, xm1
    packsswb        xm3, xm3
    pextrb    [bufq+xq], xm3, 0
    pslldq          xm3, 2
    pandn           xm0, xm11, xm0
    pand            xm3, xm11
    por             xm0, xm3
    psrldq          xm0, 1
    inc              xq
    jz .x_loop_ar2_end
    test             xb, 3
    jnz .x_loop_ar2_inner
    jmp .x_loop_ar2
.x_loop_ar2_end:
    add            bufq, 82
    dec              hd
    jg .y_loop_ar2
    RET

INIT_YMM avx2
.ar3:
%if WIN64
    ; xmm6 and xmm7 already saved
    %assign stack_offset 16
    ALLOC_STACK   16*14
    %assign stack_size stack_size - 16*4
    %assign xmm_regs_used 12
    movaps  [rsp+16*12], xmm8
    movaps  [rsp+16*13], xmm9
    movaps  [rsp+16*14], xmm10
    movaps  [rsp+16*15], xmm11
%else
    ALLOC_STACK   16*12
%endif
    mov             r6d, [fg_dataq+FGData.ar_coeff_shift]
    movq           xm11, [base+byte_blend]
    pmovsxbw         m1, [fg_dataq+FGData.ar_coeffs_y+ 0]   ; cf0-15
    pmovsxbw        xm2, [fg_dataq+FGData.ar_coeffs_y+16]   ; cf16-23
    pshufd           m0, m1, q0000
    mova    [rsp+16* 0], m0
    pshufd           m0, m1, q1111
    mova    [rsp+16* 2], m0
    pshufd           m0, m1, q2222
    mova    [rsp+16* 4], m0
    pshufd           m1, m1, q3333
    mova    [rsp+16* 6], m1
    pshufd          xm0, xm2, q0000
    mova    [rsp+16* 8], xm0
    pshufd          xm0, xm2, q1111
    mova    [rsp+16* 9], xm0
    psrldq          xm7, xm2, 10
    mova             m8, [base+gen_shufA]
    pinsrw          xm2, [base+pw_1], 5
    mova             m9, [base+gen_shufC]
    pshufd          xm2, xm2, q2222
    movu            m10, [base+gen_shufE]
    vpbroadcastw    xm6, [base+round_vals-12+r6*2]
    pinsrw          xm7, [base+round_vals+r6*2-10], 3
    mova    [rsp+16*10], xm2
    DEFINE_ARGS buf, fg_data, h, x
    sub            bufq, 82*73-(82*3+79)
    mov              hd, 70
.y_loop_ar3:
    mov              xq, -76
.x_loop_ar3:
    movu            xm5, [bufq+xq-82*3-3]    ; y=-3,x=[-3,+12]
    vinserti128      m5, [bufq+xq-82*2-3], 1 ; y=-2,x=[-3,+12]
    movu            xm4, [bufq+xq-82*1-3]    ; y=-1,x=[-3,+12]
    punpcklbw        m3, m5, m5
    punpckhwd        m5, m4
    psraw            m3, 8
    punpcklbw        m5, m5
    psraw            m5, 8
    punpcklbw       xm4, xm4
    psraw           xm4, 8
    pshufb           m0, m3, m8
    pmaddwd          m0, [rsp+16*0]
    pshufb           m1, m3, m9
    pmaddwd          m1, [rsp+16*2]
    shufps           m2, m3, m5, q1032
    paddd            m0, m1
    pshufb           m1, m2, m8
    vperm2i128       m3, m4, 0x21
    pmaddwd          m1, [rsp+16*4]
    shufps          xm2, xm3, q1021
    vpblendd         m2, m3, 0xf0
    pshufb           m2, m10
    paddd            m0, m1
    pmaddwd          m2, [rsp+16*6]
    pshufb          xm1, xm4, xm9
    pmaddwd         xm1, [rsp+16*8]
    shufps          xm4, xm5, q1132
    paddd            m0, m2
    pshufb          xm2, xm4, xm8
    pshufd          xm4, xm4, q2121
    pmaddwd         xm2, [rsp+16*9]
    punpcklwd       xm4, xm6
    pmaddwd         xm4, [rsp+16*10]
    vextracti128    xm3, m0, 1
    paddd           xm0, xm1
    movq            xm1, [bufq+xq-3]        ; y=0,x=[-3,+4]
    paddd           xm2, xm4
    paddd           xm0, xm2
    paddd           xm0, xm3
.x_loop_ar3_inner:
    pmovsxbw        xm2, xm1
    pmaddwd         xm2, xm7
    pshufd          xm3, xm2, q1111
    paddd           xm2, xm0                ; add top
    paddd           xm2, xm3                ; left+cur
    psrldq          xm0, 4
    psrad           xm2, [fg_dataq+FGData.ar_coeff_shift]
    ; don't packssdw since we only care about one value
    packsswb        xm2, xm2
    pextrb    [bufq+xq], xm2, 0
    pslldq          xm2, 3
    pand            xm2, xm11
    pandn           xm1, xm11, xm1
    por             xm1, xm2
    psrldq          xm1, 1
    inc              xq
    jz .x_loop_ar3_end
    test             xb, 3
    jnz .x_loop_ar3_inner
    jmp .x_loop_ar3
.x_loop_ar3_end:
    add            bufq, 82
    dec              hd
    jg .y_loop_ar3
    RET

%macro generate_grain_uv_fn 3 ; ss_name, ss_x, ss_y
INIT_XMM avx2
cglobal generate_grain_uv_%1_8bpc, 4, 10, 16, buf, bufy, fg_data, uv
%define base r4-generate_grain_uv_%1_8bpc_avx2_table
    lea              r4, [generate_grain_uv_%1_8bpc_avx2_table]
    vpbroadcastw    xm0, [fg_dataq+FGData.seed]
    mov             r6d, [fg_dataq+FGData.grain_scale_shift]
    movq            xm1, [base+rnd_next_upperbit_mask]
    movq            xm4, [base+mul_bits]
    movq            xm5, [base+hmul_bits]
    mova            xm6, [base+pb_mask]
    vpbroadcastw    xm7, [base+round+r6*2]
    vpbroadcastd    xm2, [base+pw_seed_xor+uvq*4]
    pxor            xm0, xm2
    lea              r6, [gaussian_sequence]
%if %2
    mov             r7d, 73-35*%3
    add            bufq, 44
.loop_y:
    mov              r5, -44
%else
    mov              r5, -73*82
    sub            bufq, r5
%endif
.loop:
    pand            xm2, xm0, xm1
    psrlw           xm3, xm2, 10
    por             xm2, xm3            ; bits 0xf, 0x1e, 0x3c and 0x78 are set
    pmullw          xm2, xm4            ; bits 0x0f00 are set
    pmulhuw         xm0, xm5
    pshufb          xm3, xm6, xm2       ; set 15th bit for next 4 seeds
    psllq           xm2, xm3, 30
    por             xm2, xm3
    psllq           xm3, xm2, 15
    por             xm2, xm0            ; aggregate each bit into next seed's high bit
    por             xm2, xm3            ; 4 next output seeds
    pshuflw         xm0, xm2, q3333
    psrlw           xm2, 5
    movq             r8, xm2
    movzx           r9d, r8w
    movd            xm2, [r6+r9*2]
    rorx             r9, r8, 32
    shr             r8d, 16
    pinsrw          xm2, [r6+r8*2], 1
    movzx           r8d, r9w
    pinsrw          xm2, [r6+r8*2], 2
    shr             r9d, 16
    pinsrw          xm2, [r6+r9*2], 3
    pmulhrsw        xm2, xm7
    packsswb        xm2, xm2
    movd      [bufq+r5], xm2
    add              r5, 4
    jl .loop
%if %2
    add            bufq, 82
    dec             r7d
    jg .loop_y
%endif

    ; auto-regression code
    movsxd           r6, [fg_dataq+FGData.ar_coeff_lag]
    movsxd           r6, [base+generate_grain_uv_%1_8bpc_avx2_table+r6*4]
    add              r6, r4
    jmp              r6

INIT_YMM avx2
.ar0:
    DEFINE_ARGS buf, bufy, fg_data, uv, unused, shift
    imul            uvd, 28
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    movd            xm2, [fg_dataq+FGData.ar_coeffs_uv+uvq]
    movd            xm3, [base+hmul_bits+shiftq*2]
    DEFINE_ARGS buf, bufy, h
    pmovsxbw        xm2, xm2
%if %2
    vpbroadcastd     m7, [base+pb_1]
    vpbroadcastw     m6, [base+hmul_bits+2+%3*2]
%endif
    vpbroadcastw     m2, xm2
    vpbroadcastw     m3, xm3
    pxor            m12, m12
%if %2
    sub            bufq, 82*(73-35*%3)+82-(82*3+41)
%else
    sub            bufq, 82*70-3
%endif
    add           bufyq, 3+82*3
    mov              hd, 70-35*%3
.y_loop_ar0:
%if %2
    ; first 32 pixels
    movu            xm4, [bufyq]
    vinserti128      m4, [bufyq+32], 1
%if %3
    movu            xm0, [bufyq+82]
    vinserti128      m0, [bufyq+82+32], 1
%endif
    movu            xm5, [bufyq+16]
    vinserti128      m5, [bufyq+48], 1
%if %3
    movu            xm1, [bufyq+82+16]
    vinserti128      m1, [bufyq+82+48], 1
%endif
    pmaddubsw        m4, m7, m4
%if %3
    pmaddubsw        m0, m7, m0
%endif
    pmaddubsw        m5, m7, m5
%if %3
    pmaddubsw        m1, m7, m1
    paddw            m4, m0
    paddw            m5, m1
%endif
    pmulhrsw         m4, m6
    pmulhrsw         m5, m6
%else
    xor             r3d, r3d
    ; first 32x2 pixels
.x_loop_ar0:
    movu             m4, [bufyq+r3]
    pcmpgtb          m0, m12, m4
    punpckhbw        m5, m4, m0
    punpcklbw        m4, m0
%endif
    pmullw           m4, m2
    pmullw           m5, m2
    pmulhrsw         m4, m3
    pmulhrsw         m5, m3
%if %2
    movu             m1, [bufq]
%else
    movu             m1, [bufq+r3]
%endif
    pcmpgtb          m8, m12, m1
    punpcklbw        m0, m1, m8
    punpckhbw        m1, m8
    paddw            m0, m4
    paddw            m1, m5
    packsswb         m0, m1
%if %2
    movu         [bufq], m0
%else
    movu      [bufq+r3], m0
    add             r3d, 32
    cmp             r3d, 64
    jl .x_loop_ar0
%endif

    ; last 6/12 pixels
    movu            xm4, [bufyq+32*2]
%if %2
%if %3
    movu            xm5, [bufyq+32*2+82]
%endif
    pmaddubsw       xm4, xm7, xm4
%if %3
    pmaddubsw       xm5, xm7, xm5
    paddw           xm4, xm5
%endif
    movq            xm0, [bufq+32]
    pmulhrsw        xm4, xm6
    pmullw          xm4, xm2
    pmulhrsw        xm4, xm3
    pcmpgtb         xm5, xm12, xm0
    punpcklbw       xm5, xm0, xm5
    paddw           xm4, xm5
    packsswb        xm4, xm4
    pblendw         xm0, xm4, xm0, 1000b
    movq      [bufq+32], xm0
%else
    movu            xm0, [bufq+64]
    pcmpgtb         xm1, xm12, xm4
    punpckhbw       xm5, xm4, xm1
    punpcklbw       xm4, xm1
    pmullw          xm5, xm2
    pmullw          xm4, xm2
    vpblendd        xm1, xm3, xm12, 0x0c
    pmulhrsw        xm5, xm1
    pmulhrsw        xm4, xm3
    pcmpgtb         xm1, xm12, xm0
    punpckhbw       xm8, xm0, xm1
    punpcklbw       xm0, xm1
    paddw           xm5, xm8
    paddw           xm0, xm4
    packsswb        xm0, xm5
    movu      [bufq+64], xm0
%endif
    add            bufq, 82
    add           bufyq, 82<<%3
    dec              hd
    jg .y_loop_ar0
    RET

INIT_XMM avx2
.ar1:
    DEFINE_ARGS buf, bufy, fg_data, uv, val3, cf3, min, max, x, shift
    imul            uvd, 28
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    movsx          cf3d, byte [fg_dataq+FGData.ar_coeffs_uv+uvq+3]
    movd            xm4, [fg_dataq+FGData.ar_coeffs_uv+uvq]
    pinsrb          xm4, [fg_dataq+FGData.ar_coeffs_uv+uvq+4], 3
    DEFINE_ARGS buf, bufy, h, val0, val3, cf3, min, max, x, shift
    pmovsxbw        xm4, xm4
    pshufd          xm5, xm4, q1111
    pshufd          xm4, xm4, q0000
    pmovsxwd        xm3, [base+round_vals+shiftq*2-12]    ; rnd
%if %2
    vpbroadcastd    xm7, [base+pb_1]
    vpbroadcastw    xm6, [base+hmul_bits+2+%3*2]
%endif
    vpbroadcastd    xm3, xm3
%if %2
    sub            bufq, 82*(73-35*%3)+44-(82*3+41)
%else
    sub            bufq, 82*70-(82-3)
%endif
    add           bufyq, 79+82*3
    mov              hd, 70-35*%3
    mov            mind, -128
    mov            maxd, 127
.y_loop_ar1:
    mov              xq, -(76>>%2)
    movsx         val3d, byte [bufq+xq-1]
.x_loop_ar1:
    pmovsxbw        xm0, [bufq+xq-82-1]     ; top/left
%if %2
    movq            xm8, [bufyq+xq*2]
%if %3
    movq            xm9, [bufyq+xq*2+82]
%endif
%endif
    psrldq          xm2, xm0, 2             ; top
    psrldq          xm1, xm0, 4             ; top/right
%if %2
    pmaddubsw       xm8, xm7, xm8
%if %3
    pmaddubsw       xm9, xm7, xm9
    paddw           xm8, xm9
%endif
    pmulhrsw        xm8, xm6
%else
    pmovsxbw        xm8, [bufyq+xq]
%endif
    punpcklwd       xm0, xm2
    punpcklwd       xm1, xm8
    pmaddwd         xm0, xm4
    pmaddwd         xm1, xm5
    paddd           xm0, xm1
    paddd           xm0, xm3
.x_loop_ar1_inner:
    movd          val0d, xm0
    psrldq          xm0, 4
    imul          val3d, cf3d
    add           val3d, val0d
    sarx          val3d, val3d, shiftd
    movsx         val0d, byte [bufq+xq]
    add           val3d, val0d
    cmp           val3d, maxd
    cmovns        val3d, maxd
    cmp           val3d, mind
    cmovs         val3d, mind
    mov  byte [bufq+xq], val3b
    ; keep val3d in-place as left for next x iteration
    inc              xq
    jz .x_loop_ar1_end
    test             xq, 3
    jnz .x_loop_ar1_inner
    jmp .x_loop_ar1

.x_loop_ar1_end:
    add            bufq, 82
    add           bufyq, 82<<%3
    dec              hd
    jg .y_loop_ar1
    RET

.ar2:
    DEFINE_ARGS buf, bufy, fg_data, uv, unused, shift
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    imul            uvd, 28
    vpbroadcastw   xm13, [base+round_vals-12+shiftq*2]
    pmovsxbw        xm7, [fg_dataq+FGData.ar_coeffs_uv+uvq+0]   ; cf0-7
    pmovsxbw        xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+8]   ; cf8-12
    pinsrw          xm0, [base+pw_1], 5
%if %2
    vpbroadcastw   xm12, [base+hmul_bits+2+%3*2]
    vpbroadcastd   xm11, [base+pb_1]
%endif
    DEFINE_ARGS buf, bufy, fg_data, h, unused, x
    pshufd          xm4, xm7, q0000
    pshufd          xm5, xm7, q3333
    pshufd          xm6, xm7, q1111
    pshufd          xm7, xm7, q2222
    pshufd          xm8, xm0, q0000
    pshufd          xm9, xm0, q1111
    pshufd         xm10, xm0, q2222
%if %2
    sub            bufq, 82*(73-35*%3)+44-(82*3+41)
%else
    sub            bufq, 82*70-(82-3)
%endif
    add           bufyq, 79+82*3
    mov              hd, 70-35*%3
.y_loop_ar2:
    mov              xq, -(76>>%2)

.x_loop_ar2:
    pmovsxbw        xm0, [bufq+xq-82*2-2]   ; y=-2,x=[-2,+5]
    pmovsxbw        xm1, [bufq+xq-82*1-2]   ; y=-1,x=[-2,+5]
    pshufb          xm2, xm0, [base+gen_shufA]
    pmaddwd         xm2, xm4
    pshufb          xm3, xm1, [base+gen_shufB]
    pmaddwd         xm3, xm5
    paddd           xm2, xm3
    pshufb          xm3, xm0, [base+gen_shufC]
    pmaddwd         xm3, xm6
    punpckhqdq      xm0, xm0                 ; y=-2,x=[+2,+5]
    punpcklwd       xm0, xm1
    pmaddwd         xm0, xm7
    pshufb          xm1, [gen_shufD]
    pmaddwd         xm1, xm8
    paddd           xm2, xm3
    paddd           xm0, xm1
    paddd           xm2, xm0

%if %2
    movq            xm0, [bufyq+xq*2]
%if %3
    movq            xm3, [bufyq+xq*2+82]
%endif
    pmaddubsw       xm0, xm11, xm0
%if %3
    pmaddubsw       xm3, xm11, xm3
    paddw           xm0, xm3
%endif
    pmulhrsw        xm0, xm12
%else
    pmovsxbw        xm0, [bufyq+xq]
%endif
    punpcklwd       xm0, xm13
    pmaddwd         xm0, xm10
    paddd           xm2, xm0

    movq            xm0, [bufq+xq-2]        ; y=0,x=[-2,+5]
.x_loop_ar2_inner:
    pmovsxbw        xm0, xm0
    pmaddwd         xm3, xm0, xm9
    psrldq          xm0, 2
    paddd           xm3, xm2
    psrldq          xm2, 4                  ; shift top to next pixel
    psrad           xm3, [fg_dataq+FGData.ar_coeff_shift]
    pslldq          xm3, 2
    paddw           xm3, xm0
    pblendw         xm0, xm3, 00000010b
    packsswb        xm0, xm0
    pextrb    [bufq+xq], xm0, 1
    inc              xq
    jz .x_loop_ar2_end
    test             xb, 3
    jnz .x_loop_ar2_inner
    jmp .x_loop_ar2

.x_loop_ar2_end:
    add            bufq, 82
    add           bufyq, 82<<%3
    dec              hd
    jg .y_loop_ar2
    RET

INIT_YMM avx2
.ar3:
    DEFINE_ARGS buf, bufy, fg_data, uv, unused, shift
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    imul            uvd, 28
    pmovsxbw         m0, [fg_dataq+FGData.ar_coeffs_uv+uvq+ 0] ; cf0-15
    pmovsxbw        xm1, [fg_dataq+FGData.ar_coeffs_uv+uvq+16] ; cf16-23
    vpbroadcastb    xm2, [fg_dataq+FGData.ar_coeffs_uv+uvq+24] ; cf24 [luma]
    movd           xm13, [base+round_vals-10+shiftq*2]
    vpbroadcastd   xm14, [base+round_vals-14+shiftq*2]
    pshufd           m6, m0, q0000
    pshufd           m7, m0, q1111
    pshufd           m8, m0, q2222
    pshufd           m9, m0, q3333
    pshufd         xm10, xm1, q0000
    pshufd         xm11, xm1, q1111
    pshufhw        xm12, xm1, q0000
    psraw           xm2, 8
    palignr        xm13, xm1, 10
    punpckhwd      xm12, xm2                     ; interleave luma cf
    psrld          xm14, 16
    DEFINE_ARGS buf, bufy, fg_data, h, unused, x
%if %2
    vpbroadcastw   xm15, [base+hmul_bits+2+%3*2]
    sub            bufq, 82*(73-35*%3)+44-(82*3+41)
%else
    sub            bufq, 82*70-(82-3)
%endif
    add           bufyq, 79+82*3
    mov              hd, 70-35*%3
.y_loop_ar3:
    mov              xq, -(76>>%2)
.x_loop_ar3:
    vbroadcasti128   m3, [bufq+xq-82*2-3]         ; y=-2,x=[-3,+12
    palignr         xm1, xm3, [bufq+xq-82*3-9], 6 ; y=-3,x=[-3,+12]
    vbroadcasti128   m4, [bufq+xq-82*1-3]    ; y=-1,x=[-3,+12]
    vpblendd         m3, m1, 0x0f
    pxor             m0, m0
    pcmpgtb          m2, m0, m3
    pcmpgtb          m0, m4
    punpcklbw        m1, m3, m2
    punpckhbw        m3, m2
    punpcklbw        m2, m4, m0
    punpckhbw       xm4, xm0
    pshufb           m0, m1, [base+gen_shufA]
    pmaddwd          m0, m6
    pshufb           m5, m1, [base+gen_shufC]
    pmaddwd          m5, m7
    shufps           m1, m3, q1032
    paddd            m0, m5
    pshufb           m5, m1, [base+gen_shufA]
    pmaddwd          m5, m8
    shufps          xm1, xm3, q2121
    vpblendd         m1, m2, 0xf0
    pshufb           m1, [base+gen_shufE]
    pmaddwd          m1, m9
    paddd            m0, m5
    pshufb          xm3, xm2, [base+gen_shufC]
    paddd            m0, m1
    pmaddwd         xm3, xm10
    palignr         xm1, xm4, xm2, 2
    punpckhwd       xm1, xm2, xm1
    pmaddwd         xm1, xm11
    palignr         xm4, xm2, 12
    paddd           xm3, xm1
%if %2
    vpbroadcastd    xm5, [base+pb_1]
    movq            xm1, [bufyq+xq*2]
    pmaddubsw       xm1, xm5, xm1
%if %3
    movq            xm2, [bufyq+xq*2+82]
    pmaddubsw       xm5, xm2
    paddw           xm1, xm5
%endif
    pmulhrsw        xm1, xm15
%else
    pmovsxbw        xm1, [bufyq+xq]
%endif
    punpcklwd       xm4, xm1
    pmaddwd         xm4, xm12
    movq            xm1, [bufq+xq-3]        ; y=0,x=[-3,+4]
    vextracti128    xm2, m0, 1
    paddd           xm0, xm14
    paddd           xm3, xm4
    paddd           xm0, xm3
    paddd           xm0, xm2
.x_loop_ar3_inner:
    pmovsxbw        xm1, xm1
    pmaddwd         xm2, xm13, xm1
    pshuflw         xm3, xm2, q1032
    paddd           xm2, xm0                ; add top
    paddd           xm2, xm3                ; left+cur
    psrldq          xm0, 4
    psrad           xm2, [fg_dataq+FGData.ar_coeff_shift]
    psrldq          xm1, 2
    ; don't packssdw, we only care about one value
    punpckldq       xm2, xm2
    pblendw         xm1, xm2, 0100b
    packsswb        xm1, xm1
    pextrb    [bufq+xq], xm1, 2
    inc              xq
    jz .x_loop_ar3_end
    test             xb, 3
    jnz .x_loop_ar3_inner
    jmp .x_loop_ar3
.x_loop_ar3_end:
    add            bufq, 82
    add           bufyq, 82<<%3
    dec              hd
    jg .y_loop_ar3
    RET
%endmacro

generate_grain_uv_fn 420, 1, 1
generate_grain_uv_fn 422, 1, 0
generate_grain_uv_fn 444, 0, 0

INIT_YMM avx2
cglobal fgy_32x32xn_8bpc, 6, 13, 16, dst, src, stride, fg_data, w, scaling, grain_lut
    pcmpeqw         m10, m10
    psrld           m10, 24
    mov             r7d, [fg_dataq+FGData.scaling_shift]
    lea              r8, [pb_mask]
%define base r8-pb_mask
    vpbroadcastw    m11, [base+mul_bits+r7*2-14]
    mov             r7d, [fg_dataq+FGData.clip_to_restricted_range]
    vpbroadcastw    m12, [base+max+r7*4]
    vpbroadcastw    m13, [base+min+r7*2]

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused, sby, see, overlap

    mov        overlapd, [fg_dataq+FGData.overlap_flag]
    movifnidn      sbyd, sbym
    test           sbyd, sbyd
    setnz           r7b
    test            r7b, overlapb
    jnz .vertical_overlap

    imul           seed, sbyd, (173 << 24) | 37
    add            seed, (105 << 24) | 178
    rol            seed, 8
    movzx          seed, seew
    xor            seed, [fg_dataq+FGData.seed]

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                unused1, unused2, see, overlap

    lea        src_bakq, [srcq+wq]
    neg              wq
    sub            dstq, srcq

.loop_x:
    mov             r6d, seed
    or             seed, 0xEFF4
    shr             r6d, 1
    test           seeb, seeh
    lea            seed, [r6+0x8000]
    cmovp          seed, r6d                ; updated seed

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                offx, offy, see, overlap

    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 164
    lea           offyq, [offyq+offxq*2+747] ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                h, offxy, see, overlap

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y:
    ; src
    mova             m0, [srcq]
    pxor             m2, m2
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2
    punpckhwd        m7, m1, m2
    punpcklwd        m6, m1, m2             ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m8, [scalingq+m4], m3
    vpgatherdd       m4, [scalingq+m5], m9
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m5, [scalingq+m6], m3
    vpgatherdd       m6, [scalingq+m7], m9
    pand             m8, m10
    pand             m4, m10
    pand             m5, m10
    pand             m6, m10
    packusdw         m8, m4
    packusdw         m5, m6

    ; grain = grain_lut[offy+y][offx+x]
    movu             m3, [grain_lutq+offxyq]
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m2, m8
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
    mova    [dstq+srcq], m0

    add            srcq, strideq
    add      grain_lutq, 82
    dec              hd
    jg .loop_y

    add              wq, 32
    jge .end
    lea            srcq, [src_bakq+wq]
    test       overlapd, overlapd
    jz .loop_x

    ; r8m = sbym
    movq           xm15, [pb_27_17_17_27]
    cmp       dword r8m, 0
    jne .loop_x_hv_overlap

    ; horizontal overlap (without vertical overlap)
    movq           xm14, [pw_1024]
.loop_x_h_overlap:
    mov             r6d, seed
    or             seed, 0xEFF4
    shr             r6d, 1
    test           seeb, seeh
    lea            seed, [r6+0x8000]
    cmovp          seed, r6d                ; updated seed

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                offx, offy, see, left_offxy

    lea     left_offxyd, [offyd+32]         ; previous column's offy*stride+offx
    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 164
    lea           offyq, [offyq+offxq*2+747] ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                h, offxy, see, left_offxy

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y_h_overlap:
    ; src
    mova             m0, [srcq]
    pxor             m2, m2
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2
    punpckhwd        m7, m1, m2
    punpcklwd        m6, m1, m2             ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m8, [scalingq+m4], m3
    vpgatherdd       m4, [scalingq+m5], m9
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m5, [scalingq+m6], m3
    vpgatherdd       m6, [scalingq+m7], m9
    pand             m8, m10
    pand             m4, m10
    pand             m5, m10
    pand             m6, m10
    packusdw         m8, m4
    packusdw         m5, m6

    ; grain = grain_lut[offy+y][offx+x]
    movu             m3, [grain_lutq+offxyq]
    movd            xm4, [grain_lutq+left_offxyq]
    punpcklbw       xm4, xm3
    pmaddubsw       xm4, xm15, xm4
    pmulhrsw        xm4, xm14
    packsswb        xm4, xm4
    vpblendd         m3, m3, m4, 00000001b
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m2, m8
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
    mova    [dstq+srcq], m0

    add            srcq, strideq
    add      grain_lutq, 82
    dec              hd
    jg .loop_y_h_overlap

    add              wq, 32
    jge .end
    lea            srcq, [src_bakq+wq]

    ; r8m = sbym
    cmp       dword r8m, 0
    jne .loop_x_hv_overlap
    jmp .loop_x_h_overlap

.end:
    RET

.vertical_overlap:
    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused, sby, see, overlap

    movzx          sbyd, sbyb
    imul           seed, [fg_dataq+FGData.seed], 0x00010001
    imul            r7d, sbyd, 173 * 0x00010001
    imul           sbyd, 37 * 0x01000100
    add             r7d, (105 << 16) | 188
    add            sbyd, (178 << 24) | (141 << 8)
    and             r7d, 0x00ff00ff
    and            sbyd, 0xff00ff00
    xor            seed, r7d
    xor            seed, sbyd               ; (cur_seed << 16) | top_seed

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                unused1, unused2, see, overlap

    lea        src_bakq, [srcq+wq]
    neg              wq
    sub            dstq, srcq

    vpbroadcastd    m14, [pw_1024]
.loop_x_v_overlap:
    vpbroadcastw    m15, [pb_27_17_17_27]

    ; we assume from the block above that bits 8-15 of r7d are zero'ed
    mov             r6d, seed
    or             seed, 0xeff4eff4
    test           seeb, seeh
    setp            r7b                     ; parity of top_seed
    shr            seed, 16
    shl             r7d, 16
    test           seeb, seeh
    setp            r7b                     ; parity of cur_seed
    or              r6d, 0x00010001
    xor             r7d, r6d
    rorx           seed, r7d, 1             ; updated (cur_seed << 16) | top_seed

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                offx, offy, see, overlap, top_offxy

    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 164
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq*2+0x10001*747+32*82]

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                h, offxy, see, overlap, top_offxy

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y_v_overlap:
    ; src
    mova             m0, [srcq]
    pxor             m2, m2
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2
    punpckhwd        m7, m1, m2
    punpcklwd        m6, m1, m2             ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m8, [scalingq+m4], m3
    vpgatherdd       m4, [scalingq+m5], m9
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m5, [scalingq+m6], m3
    vpgatherdd       m6, [scalingq+m7], m9
    pand             m8, m10
    pand             m4, m10
    pand             m5, m10
    pand             m6, m10
    packusdw         m8, m4
    packusdw         m5, m6

    ; grain = grain_lut[offy+y][offx+x]
    movu             m3, [grain_lutq+offxyq]
    movu             m4, [grain_lutq+top_offxyq]
    punpckhbw        m6, m4, m3
    punpcklbw        m4, m3
    pmaddubsw        m6, m15, m6
    pmaddubsw        m4, m15, m4
    pmulhrsw         m6, m14
    pmulhrsw         m4, m14
    packsswb         m3, m4, m6
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m2, m8
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
    mova    [dstq+srcq], m0

    vpbroadcastw    m15, [pb_27_17_17_27+2] ; swap weights for second v-overlap line
    add            srcq, strideq
    add      grain_lutq, 82
    dec              hw
    jz .end_y_v_overlap
    ; 2 lines get vertical overlap, then fall back to non-overlap code for
    ; remaining (up to) 30 lines
    btc              hd, 16
    jnc .loop_y_v_overlap
    jmp .loop_y

.end_y_v_overlap:
    add              wq, 32
    jge .end_hv
    lea            srcq, [src_bakq+wq]

    ; since fg_dataq.overlap is guaranteed to be set, we never jump
    ; back to .loop_x_v_overlap, and instead always fall-through to
    ; h+v overlap

    movq           xm15, [pb_27_17_17_27]
.loop_x_hv_overlap:
    vpbroadcastw     m8, [pb_27_17_17_27]

    ; we assume from the block above that bits 8-15 of r7d are zero'ed
    mov             r6d, seed
    or             seed, 0xeff4eff4
    test           seeb, seeh
    setp            r7b                     ; parity of top_seed
    shr            seed, 16
    shl             r7d, 16
    test           seeb, seeh
    setp            r7b                     ; parity of cur_seed
    or              r6d, 0x00010001
    xor             r7d, r6d
    rorx           seed, r7d, 1             ; updated (cur_seed << 16) | top_seed

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                offx, offy, see, left_offxy, top_offxy, topleft_offxy

    lea  topleft_offxyq, [top_offxyq+32]
    lea     left_offxyq, [offyq+32]
    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 164
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq*2+0x10001*747+32*82]

    DEFINE_ARGS dst, src, stride, src_bak, w, scaling, grain_lut, \
                h, offxy, see, left_offxy, top_offxy, topleft_offxy

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y_hv_overlap:
    ; src
    mova             m0, [srcq]
    pxor             m2, m2
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2
    punpckhwd        m7, m1, m2
    punpcklwd        m6, m1, m2             ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m3, m3
    ; FIXME it would be nice to have another register here to do 2 vpgatherdd's in parallel
    vpgatherdd       m9, [scalingq+m4], m3
    pcmpeqw          m3, m3
    vpgatherdd       m4, [scalingq+m5], m3
    pcmpeqw          m3, m3
    vpgatherdd       m5, [scalingq+m6], m3
    pcmpeqw          m3, m3
    vpgatherdd       m6, [scalingq+m7], m3
    pand             m9, m10
    pand             m4, m10
    pand             m5, m10
    pand             m6, m10
    packusdw         m9, m4
    packusdw         m5, m6

    ; grain = grain_lut[offy+y][offx+x]
    movu             m3, [grain_lutq+offxyq]
    movu             m6, [grain_lutq+top_offxyq]
    movd            xm4, [grain_lutq+left_offxyq]
    movd            xm7, [grain_lutq+topleft_offxyq]
    ; do h interpolation first (so top | top/left -> top, left | cur -> cur)
    punpcklbw       xm4, xm3
    punpcklbw       xm7, xm6
    pmaddubsw       xm4, xm15, xm4
    pmaddubsw       xm7, xm15, xm7
    pmulhrsw        xm4, xm14
    pmulhrsw        xm7, xm14
    packsswb        xm4, xm4
    packsswb        xm7, xm7
    vpblendd         m3, m4, 00000001b
    vpblendd         m6, m7, 00000001b
    ; followed by v interpolation (top | cur -> cur)
    punpckhbw        m7, m6, m3
    punpcklbw        m6, m3
    pmaddubsw        m7, m8, m7
    pmaddubsw        m6, m8, m6
    pmulhrsw         m7, m14
    pmulhrsw         m6, m14
    packsswb         m3, m6, m7
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m2, m9
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
    mova    [dstq+srcq], m0

    vpbroadcastw     m8, [pb_27_17_17_27+2] ; swap weights for second v-overlap line
    add            srcq, strideq
    add      grain_lutq, 82
    dec              hw
    jz .end_y_hv_overlap
    ; 2 lines get vertical overlap, then fall back to non-overlap code for
    ; remaining (up to) 30 lines
    btc              hd, 16
    jnc .loop_y_hv_overlap
    jmp .loop_y_h_overlap

.end_y_hv_overlap:
    add              wq, 32
    lea            srcq, [src_bakq+wq]
    jl .loop_x_hv_overlap

.end_hv:
    RET

%macro FGUV_FN 3 ; name, ss_hor, ss_ver
cglobal fguv_32x32xn_i%1_8bpc, 6, 15, 16, dst, src, stride, fg_data, w, scaling, \
                                     grain_lut, h, sby, luma, lstride, uv_pl, is_id
    mov             r7d, [fg_dataq+FGData.scaling_shift]
    lea              r8, [pb_mask]
%define base r8-pb_mask
    vpbroadcastw    m11, [base+mul_bits+r7*2-14]
    mov             r7d, [fg_dataq+FGData.clip_to_restricted_range]
    mov             r9d, dword is_idm
    vpbroadcastw    m13, [base+min+r7*2]
    shlx            r7d, r7d, r9d
    vpbroadcastw    m12, [base+max+r7*2]

    cmp byte [fg_dataq+FGData.chroma_scaling_from_luma], 0
    jne .csfl

%macro %%FGUV_32x32xN_LOOP 3 ; not-csfl, ss_hor, ss_ver
    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused, sby, see, overlap

%if %1
    mov             r7d, dword r11m
    vpbroadcastb     m0, [fg_dataq+FGData.uv_mult+r7*4]
    vpbroadcastb     m1, [fg_dataq+FGData.uv_luma_mult+r7*4]
    punpcklbw       m14, m1, m0
    vpbroadcastw    m15, [fg_dataq+FGData.uv_offset+r7*4]
%else
    vpbroadcastd    m14, [pw_1024]
%if %2
    vpbroadcastq    m15, [pb_23_22]
%else
    vpbroadcastq   xm15, [pb_27_17_17_27]
%endif
%endif
%if %3
    vpbroadcastw    m10, [pb_23_22]
%elif %2
    mova            m10, [pb_8x_27_17_8x_17_27]
%endif

    mov        overlapd, [fg_dataq+FGData.overlap_flag]
    movifnidn      sbyd, sbym
    test           sbyd, sbyd
    setnz           r7b
    test            r7b, overlapb
    jnz %%vertical_overlap

    imul           seed, sbyd, (173 << 24) | 37
    add            seed, (105 << 24) | 178
    rol            seed, 8
    movzx          seed, seew
    xor            seed, [fg_dataq+FGData.seed]

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                unused2, unused3, see, overlap, unused4, unused5, lstride

    mov           lumaq, r9mp
    lea             r12, [srcq+wq]
    lea             r13, [dstq+wq]
    lea             r14, [lumaq+wq*(1+%2)]
    mov           r11mp, r12
    mov           r12mp, r13
    mov        lstrideq, r10mp
    neg              wq

%%loop_x:
    mov             r6d, seed
    or             seed, 0xEFF4
    shr             r6d, 1
    test           seeb, seeh
    lea            seed, [r6+0x8000]
    cmovp          seed, r6d               ; updated seed

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                offx, offy, see, overlap, unused1, unused2, lstride

    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 164>>%3
    lea           offyq, [offyq+offxq*(2-%2)+(3+(6>>%3))*82+3+(6>>%2)]  ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                h, offxy, see, overlap, unused1, unused2, lstride

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%%loop_y:
    ; src
%if %2
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm6, [lumaq+lstrideq*0+16]
    mova            xm0, [srcq]
    vpbroadcastd     m7, [pb_1]
    vinserti128      m4, [lumaq+lstrideq*(1+%3) +0], 1
    vinserti128      m6, [lumaq+lstrideq*(1+%3)+16], 1
    vinserti128      m0, [srcq+strideq], 1
    pxor             m2, m2
    pmaddubsw        m4, m7
    pmaddubsw        m6, m7
    pavgw            m4, m2
    pavgw            m6, m2
%else
    pxor             m2, m2
    mova             m4, [lumaq]
    mova             m0, [srcq]
%endif

%if %1
%if %2
    packuswb         m4, m6                 ; luma
%endif
    punpckhbw        m6, m4, m0
    punpcklbw        m4, m0                 ; { luma, chroma }
    pmaddubsw        m6, m14
    pmaddubsw        m4, m14
    psraw            m6, 6
    psraw            m4, 6
    paddw            m6, m15
    paddw            m4, m15
    packuswb         m4, m6                 ; pack+unpack = clip
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%elif %2 == 0
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%endif

    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: luma_src as dword

    ; scaling[luma_src]
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m8, [scalingq-3+m4], m3
    vpgatherdd       m4, [scalingq-3+m5], m9
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m5, [scalingq-3+m6], m3
    vpgatherdd       m6, [scalingq-3+m7], m9
    REPX {psrld x, 24}, m8, m4, m5, m6
    packusdw         m8, m4
    packusdw         m5, m6

    ; unpack chroma_source
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word

    ; grain = grain_lut[offy+y][offx+x]
%if %2
    movu            xm3, [grain_lutq+offxyq+ 0]
    vinserti128      m3, [grain_lutq+offxyq+82], 1
%else
    movu             m3, [grain_lutq+offxyq]
%endif
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m2, m8
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
%if %2
    mova         [dstq], xm0
    vextracti128 [dstq+strideq], m0, 1
%else
    mova         [dstq], m0
%endif

%if %2
    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*(2<<%3)]
%else
    add            srcq, strideq
    add            dstq, strideq
    add           lumaq, lstrideq
%endif
    add      grain_lutq, 82<<%2
    sub              hb, 1+%2
    jg %%loop_y

    add              wq, 32>>%2
    jge %%end
    mov            srcq, r11mp
    mov            dstq, r12mp
    lea           lumaq, [r14+wq*(1+%2)]
    add            srcq, wq
    add            dstq, wq
    test       overlapd, overlapd
    jz %%loop_x

    ; r8m = sbym
    cmp       dword r8m, 0
    jne %%loop_x_hv_overlap

    ; horizontal overlap (without vertical overlap)
%%loop_x_h_overlap:
    mov             r6d, seed
    or             seed, 0xEFF4
    shr             r6d, 1
    test           seeb, seeh
    lea            seed, [r6+0x8000]
    cmovp          seed, r6d               ; updated seed

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                offx, offy, see, left_offxy, unused1, unused2, lstride

    lea     left_offxyd, [offyd+(32>>%2)]         ; previous column's offy*stride+offx
    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 164>>%3
    lea           offyq, [offyq+offxq*(2-%2)+(3+(6>>%3))*82+3+(6>>%2)]  ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                h, offxy, see, left_offxy, unused1, unused2, lstride

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%%loop_y_h_overlap:
    ; src
%if %2
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm6, [lumaq+lstrideq*0+16]
    mova            xm0, [srcq]
    vpbroadcastd     m7, [pb_1]
    vinserti128      m4, [lumaq+lstrideq*(1+%3) +0], 1
    vinserti128      m6, [lumaq+lstrideq*(1+%3)+16], 1
    vinserti128      m0, [srcq+strideq], 1
    pxor             m2, m2
    pmaddubsw        m4, m7
    pmaddubsw        m6, m7
    pavgw            m4, m2
    pavgw            m6, m2
%else
    mova             m4, [lumaq]
    mova             m0, [srcq]
    pxor             m2, m2
%endif

%if %1
%if %2
    packuswb         m4, m6                 ; luma
%endif
    punpckhbw        m6, m4, m0
    punpcklbw        m4, m0                 ; { luma, chroma }
    pmaddubsw        m6, m14
    pmaddubsw        m4, m14
    psraw            m6, 6
    psraw            m4, 6
    paddw            m6, m15
    paddw            m4, m15
    packuswb         m4, m6                 ; pack+unpack = clip
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%elif %2 == 0
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%endif

    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: luma_src as dword

    ; scaling[luma_src]
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m8, [scalingq-3+m4], m3
    vpgatherdd       m4, [scalingq-3+m5], m9
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m5, [scalingq-3+m6], m3
    vpgatherdd       m6, [scalingq-3+m7], m9
    REPX {psrld x, 24}, m8, m4, m5, m6
    packusdw         m8, m4
    packusdw         m5, m6

    ; unpack chroma_source
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word

    ; grain = grain_lut[offy+y][offx+x]
%if %2
%if %1
    vpbroadcastq     m6, [pb_23_22]
%endif
    movu            xm3, [grain_lutq+offxyq+ 0]
    movd            xm4, [grain_lutq+left_offxyq+ 0]
    vinserti128      m3, [grain_lutq+offxyq+82], 1
    vinserti128      m4, [grain_lutq+left_offxyq+82], 1
    punpcklbw        m4, m3
%if %1
    pmaddubsw        m4, m6, m4
    pmulhrsw         m4, [pw_1024]
%else
    pmaddubsw        m4, m15, m4
    pmulhrsw         m4, m14
%endif
    packsswb         m4, m4
    vpblendd         m3, m3, m4, 00010001b
%else
%if %1
    movq            xm6, [pb_27_17_17_27]
%endif
    movu             m3, [grain_lutq+offxyq]
    movd            xm4, [grain_lutq+left_offxyq]
    punpcklbw       xm4, xm3
%if %1
    pmaddubsw       xm4, xm6, xm4
    pmulhrsw        xm4, [pw_1024]
%else
    pmaddubsw       xm4, xm15, xm4
    pmulhrsw        xm4, xm14
%endif
    packsswb        xm4, xm4
    vpblendd         m3, m3, m4, 00000001b
%endif
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m2, m8
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
%if %2
    mova         [dstq], xm0
    vextracti128 [dstq+strideq], m0, 1
%else
    mova         [dstq], m0
%endif

%if %2
    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*(2<<%3)]
%else
    add            srcq, strideq
    add            dstq, strideq
    add           lumaq, lstrideq
%endif
    add      grain_lutq, 82*(1+%2)
    sub              hb, 1+%2
    jg %%loop_y_h_overlap

    add              wq, 32>>%2
    jge %%end
    mov            srcq, r11mp
    mov            dstq, r12mp
    lea           lumaq, [r14+wq*(1+%2)]
    add            srcq, wq
    add            dstq, wq

    ; r8m = sbym
    cmp       dword r8m, 0
    jne %%loop_x_hv_overlap
    jmp %%loop_x_h_overlap

%%end:
    RET

%%vertical_overlap:
    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused, \
                sby, see, overlap, unused1, unused2, lstride

    movzx          sbyd, sbyb
    imul           seed, [fg_dataq+FGData.seed], 0x00010001
    imul            r7d, sbyd, 173 * 0x00010001
    imul           sbyd, 37 * 0x01000100
    add             r7d, (105 << 16) | 188
    add            sbyd, (178 << 24) | (141 << 8)
    and             r7d, 0x00ff00ff
    and            sbyd, 0xff00ff00
    xor            seed, r7d
    xor            seed, sbyd               ; (cur_seed << 16) | top_seed

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                unused1, unused2, see, overlap, unused3, unused4, lstride

    mov           lumaq, r9mp
    lea             r12, [srcq+wq]
    lea             r13, [dstq+wq]
    lea             r14, [lumaq+wq*(1+%2)]
    mov           r11mp, r12
    mov           r12mp, r13
    mov        lstrideq, r10mp
    neg              wq

%%loop_x_v_overlap:
    ; we assume from the block above that bits 8-15 of r7d are zero'ed
    mov             r6d, seed
    or             seed, 0xeff4eff4
    test           seeb, seeh
    setp            r7b                     ; parity of top_seed
    shr            seed, 16
    shl             r7d, 16
    test           seeb, seeh
    setp            r7b                     ; parity of cur_seed
    or              r6d, 0x00010001
    xor             r7d, r6d
    rorx           seed, r7d, 1             ; updated (cur_seed << 16) | top_seed

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                offx, offy, see, overlap, top_offxy, unused, lstride

    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 164>>%3
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq*(2-%2)+0x10001*((3+(6>>%3))*82+3+(6>>%2))+(32>>%3)*82]

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                h, offxy, see, overlap, top_offxy, unused, lstride

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%if %2 == 0
    vbroadcasti128  m10, [pb_8x_27_17_8x_17_27]
%endif
%%loop_y_v_overlap:
    ; src
%if %2
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm6, [lumaq+lstrideq*0+16]
    mova            xm0, [srcq]
    vpbroadcastd     m7, [pb_1]
    vinserti128      m4, [lumaq+lstrideq*(1+%3) +0], 1
    vinserti128      m6, [lumaq+lstrideq*(1+%3)+16], 1
    vinserti128      m0, [srcq+strideq], 1
    pxor             m2, m2
    pmaddubsw        m4, m7
    pmaddubsw        m6, m7
    pavgw            m4, m2
    pavgw            m6, m2
%else
    mova             m4, [lumaq]
    mova             m0, [srcq]
    pxor             m2, m2
%endif

%if %1
%if %2
    packuswb         m4, m6                 ; luma
%endif
    punpckhbw        m6, m4, m0
    punpcklbw        m4, m0                 ; { luma, chroma }
    pmaddubsw        m6, m14
    pmaddubsw        m4, m14
    psraw            m6, 6
    psraw            m4, 6
    paddw            m6, m15
    paddw            m4, m15
    packuswb         m4, m6                 ; pack+unpack = clip
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%elif %2 == 0
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%endif

    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: luma_src as dword

    ; scaling[luma_src]
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m8, [scalingq-3+m4], m3
    vpgatherdd       m4, [scalingq-3+m5], m9
    pcmpeqw          m3, m3
    pcmpeqw          m9, m9
    vpgatherdd       m5, [scalingq-3+m6], m3
    vpgatherdd       m6, [scalingq-3+m7], m9
    REPX {psrld x, 24}, m8, m4, m5, m6
    packusdw         m8, m4
    packusdw         m5, m6

%if %2
    ; unpack chroma_source
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word
%endif

    ; grain = grain_lut[offy+y][offx+x]
%if %3 == 0
%if %2
    movu            xm3, [grain_lutq+offxyq]
    movu            xm4, [grain_lutq+top_offxyq]
    vinserti128      m3, [grain_lutq+offxyq+82], 1
    vinserti128      m4, [grain_lutq+top_offxyq+82], 1
%else
    movu             m3, [grain_lutq+offxyq]
    movu             m4, [grain_lutq+top_offxyq]
%endif
    punpckhbw        m9, m4, m3
    punpcklbw        m4, m3
    pmaddubsw        m9, m10, m9
    pmaddubsw        m4, m10, m4
%if %1
    pmulhrsw         m9, [pw_1024]
    pmulhrsw         m4, [pw_1024]
%else
    pmulhrsw         m9, m14
    pmulhrsw         m4, m14
%endif
    packsswb         m3, m4, m9
%else
    movq            xm3, [grain_lutq+offxyq]
    movq            xm4, [grain_lutq+top_offxyq]
    vinserti128      m3, [grain_lutq+offxyq+8], 1
    vinserti128      m4, [grain_lutq+top_offxyq+8], 1
    punpcklbw        m4, m3
    pmaddubsw        m4, m10, m4
%if %1
    pmulhrsw         m4, [pw_1024]
%else
    pmulhrsw         m4, m14
%endif
    packsswb         m4, m4
    vpermq           m4, m4, q3120
    ; only interpolate first line, insert second line unmodified
    vinserti128      m3, m4, [grain_lutq+offxyq+82], 1
%endif
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m2, m8
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
%if %2
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
    mova         [dstq], xm0
    vextracti128 [dstq+strideq], m0, 1
%else
    pxor             m6, m6
    punpckhbw        m9, m0, m6
    punpcklbw        m0, m6                 ; m0-1: src as word

    paddw            m0, m2
    paddw            m9, m3
    pmaxsw           m0, m13
    pmaxsw           m9, m13
    pminsw           m0, m12
    pminsw           m9, m12
    packuswb         m0, m9
    mova         [dstq], m0
%endif

    sub              hb, 1+%2
    jle %%end_y_v_overlap
%if %2
    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*(2<<%3)]
%else
    add            srcq, strideq
    add            dstq, strideq
    add           lumaq, lstrideq
%endif
    add      grain_lutq, 82<<%2
%if %2 == 0
    vbroadcasti128  m10, [pb_8x_27_17_8x_17_27+16]
    btc              hd, 16
    jnc %%loop_y_v_overlap
%endif
    jmp %%loop_y

%%end_y_v_overlap:
    add              wq, 32>>%2
    jge %%end_hv
    mov            srcq, r11mp
    mov            dstq, r12mp
    lea           lumaq, [r14+wq*(1+%2)]
    add            srcq, wq
    add            dstq, wq

    ; since fg_dataq.overlap is guaranteed to be set, we never jump
    ; back to .loop_x_v_overlap, and instead always fall-through to
    ; h+v overlap

%%loop_x_hv_overlap:
    ; we assume from the block above that bits 8-15 of r7d are zero'ed
    mov             r6d, seed
    or             seed, 0xeff4eff4
    test           seeb, seeh
    setp            r7b                     ; parity of top_seed
    shr            seed, 16
    shl             r7d, 16
    test           seeb, seeh
    setp            r7b                     ; parity of cur_seed
    or              r6d, 0x00010001
    xor             r7d, r6d
    rorx           seed, r7d, 1             ; updated (cur_seed << 16) | top_seed

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                offx, offy, see, left_offxy, top_offxy, topleft_offxy, lstride

    lea  topleft_offxyq, [top_offxyq+(32>>%2)]
    lea     left_offxyq, [offyq+(32>>%2)]
    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 164>>%3
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq*(2-%2)+0x10001*((3+(6>>%3))*82+3+(6>>%2))+(32>>%3)*82]

    DEFINE_ARGS dst, src, stride, luma, w, scaling, grain_lut, \
                h, offxy, see, left_offxy, top_offxy, topleft_offxy, lstride

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%if %2 == 0
    vbroadcasti128  m10, [pb_8x_27_17_8x_17_27]
%endif
%%loop_y_hv_overlap:
    ; src
%if %2
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm6, [lumaq+lstrideq*0+16]
    mova            xm0, [srcq]
    vpbroadcastd     m7, [pb_1]
    vinserti128      m4, [lumaq+lstrideq*(1+%3) +0], 1
    vinserti128      m6, [lumaq+lstrideq*(1+%3)+16], 1
    vinserti128      m0, [srcq+strideq], 1
    pxor             m2, m2
    pmaddubsw        m4, m7
    pmaddubsw        m6, m7
    pavgw            m4, m2
    pavgw            m6, m2
%else
    mova             m4, [lumaq]
    mova             m0, [srcq]
    pxor             m2, m2
%endif

%if %1
%if %2
    packuswb         m4, m6                 ; luma
%endif
    punpckhbw        m6, m4, m0
    punpcklbw        m4, m0                 ; { luma, chroma }
    pmaddubsw        m6, m14
    pmaddubsw        m4, m14
    psraw            m6, 6
    psraw            m4, 6
    paddw            m6, m15
    paddw            m4, m15
    packuswb         m4, m6                 ; pack+unpack = clip
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%elif %2 == 0
    punpckhbw        m6, m4, m2
    punpcklbw        m4, m2
%endif

    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m9, m9
    pcmpeqw          m3, m3
    vpgatherdd       m8, [scalingq-3+m4], m9
    vpgatherdd       m4, [scalingq-3+m5], m3
    pcmpeqw          m9, m9
    pcmpeqw          m3, m3
    vpgatherdd       m5, [scalingq-3+m6], m9
    vpgatherdd       m6, [scalingq-3+m7], m3
    REPX {psrld x, 24}, m8, m4, m5, m6
    packusdw         m8, m4
    packusdw         m5, m6

%if %2
    ; unpack chroma source
    punpckhbw        m1, m0, m2
    punpcklbw        m0, m2                 ; m0-1: src as word
%endif

    ; grain = grain_lut[offy+y][offx+x]
%if %1
%if %2
    vpbroadcastq     m9, [pb_23_22]
%else
    vpbroadcastq    xm9, [pb_27_17_17_27]
%endif
%endif

%if %2
    movu            xm3, [grain_lutq+offxyq]
%if %3
    movq            xm6, [grain_lutq+top_offxyq]
%else
    movu            xm6, [grain_lutq+top_offxyq]
%endif
    vinserti128      m3, [grain_lutq+offxyq+82], 1
%if %3
    vinserti128      m6, [grain_lutq+top_offxyq+8], 1
%else
    vinserti128      m6, [grain_lutq+top_offxyq+82], 1
%endif
%else
    movu             m3, [grain_lutq+offxyq]
    movu             m6, [grain_lutq+top_offxyq]
%endif
    movd            xm4, [grain_lutq+left_offxyq]
    movd            xm7, [grain_lutq+topleft_offxyq]
%if %2
    vinserti128      m4, [grain_lutq+left_offxyq+82], 1
%if %3 == 0
    vinserti128      m7, [grain_lutq+topleft_offxyq+82], 1
%endif
%endif

    ; do h interpolation first (so top | top/left -> top, left | cur -> cur)
%if %2
    punpcklbw        m4, m3
%if %3
    punpcklbw       xm7, xm6
%else
    punpcklbw        m7, m6
%endif
    punpcklqdq       m4, m7
%if %1
    pmaddubsw        m4, m9, m4
    pmulhrsw         m4, [pw_1024]
%else
    pmaddubsw        m4, m15, m4
    pmulhrsw         m4, m14
%endif
    packsswb         m4, m4
    vpblendd         m3, m4, 00010001b
    psrldq           m4, 4
%if %3
    vpblendd         m6, m6, m4, 00000001b
%else
    vpblendd         m6, m6, m4, 00010001b
%endif
%else
    punpcklbw       xm4, xm3
    punpcklbw       xm7, xm6
    punpcklqdq      xm4, xm7
%if %1
    pmaddubsw       xm4, xm9, xm4
    pmulhrsw        xm4, [pw_1024]
%else
    pmaddubsw       xm4, xm15, xm4
    pmulhrsw        xm4, xm14
%endif
    packsswb        xm4, xm4
    vpblendd         m3, m3, m4, 00000001b
    psrldq          xm4, 4
    vpblendd         m6, m6, m4, 00000001b
%endif

    ; followed by v interpolation (top | cur -> cur)
%if %3
    vpermq           m9, m3, q3120
    punpcklbw        m6, m9
    pmaddubsw        m6, m10, m6
%if %1
    pmulhrsw         m6, [pw_1024]
%else
    pmulhrsw         m6, m14
%endif
    packsswb         m6, m6
    vpermq           m6, m6, q3120
    vpblendd         m3, m3, m6, 00001111b
%else
    punpckhbw        m9, m6, m3
    punpcklbw        m6, m3
    pmaddubsw        m9, m10, m9
    pmaddubsw        m6, m10, m6
%if %1
    pmulhrsw         m9, [pw_1024]
    pmulhrsw         m6, [pw_1024]
%else
    pmulhrsw         m9, m14
    pmulhrsw         m6, m14
%endif
    packsswb         m3, m6, m9
%endif
    pcmpgtb          m7, m2, m3
    punpcklbw        m2, m3, m7
    punpckhbw        m3, m7

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m2, m8
    pmullw           m3, m5
    pmulhrsw         m2, m11
    pmulhrsw         m3, m11

    ; dst = clip_pixel(src, noise)
%if %2
    paddw            m0, m2
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    packuswb         m0, m1
    mova         [dstq], xm0
    vextracti128 [dstq+strideq], m0, 1
%else
    pxor             m6, m6
    punpckhbw        m9, m0, m6
    punpcklbw        m0, m6                 ; m0-1: src as word
    paddw            m0, m2
    paddw            m9, m3
    pmaxsw           m0, m13
    pmaxsw           m9, m13
    pminsw           m0, m12
    pminsw           m9, m12
    packuswb         m0, m9
    mova         [dstq], m0
%endif

%if %2
    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*(2<<%3)]
%else
    add            srcq, strideq
    add            dstq, strideq
    add           lumaq, lstrideq
%endif
    add      grain_lutq, 82<<%2
    sub              hb, 1+%2
%if %2
    jg %%loop_y_h_overlap
%else
    je %%end_y_hv_overlap
    vbroadcasti128  m10, [pb_8x_27_17_8x_17_27+16]
    btc              hd, 16
    jnc %%loop_y_hv_overlap
    jmp %%loop_y_h_overlap
%endif

%%end_y_hv_overlap:
    add              wq, 32>>%2
    jge %%end_hv
    mov            srcq, r11mp
    mov            dstq, r12mp
    lea           lumaq, [r14+wq*(1+%2)]
    add            srcq, wq
    add            dstq, wq
    jmp %%loop_x_hv_overlap

%%end_hv:
    RET
%endmacro

    %%FGUV_32x32xN_LOOP 1, %2, %3
.csfl:
    %%FGUV_32x32xN_LOOP 0, %2, %3
%endmacro

FGUV_FN 420, 1, 1
FGUV_FN 422, 1, 0
FGUV_FN 444, 0, 0

%endif ; ARCH_X86_64
