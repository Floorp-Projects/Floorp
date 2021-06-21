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

SECTION_RODATA 32
pw_1024: times 16 dw 1024
pw_23_22: times 8 dw 23, 22
pb_mask: db 0, 0x80, 0x80, 0, 0x80, 0, 0, 0x80, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0
rnd_next_upperbit_mask: dw 0x100B, 0x2016, 0x402C, 0x8058
pw_seed_xor: times 2 dw 0xb524
             times 2 dw 0x49d8
pd_16: dd 16
pd_m65536: dd ~0xffff
pb_1: times 4 db 1
hmul_bits: dw 32768, 16384, 8192, 4096
round: dw 2048, 1024, 512
mul_bits: dw 256, 128, 64, 32, 16
round_vals: dw 32, 64, 128, 256, 512, 1024
max: dw 256*4-1, 240*4, 235*4, 256*16-1, 240*16, 235*16
min: dw 0, 16*4, 16*16
pw_27_17_17_27: dw 27, 17, 17, 27
; these two should be next to each other
pw_4: times 2 dw 4
pw_16: times 2 dw 16

%macro JMP_TABLE 1-*
    %xdefine %1_table %%table
    %xdefine %%base %1_table
    %xdefine %%prefix mangle(private_prefix %+ _%1)
    %%table:
    %rep %0 - 1
        dd %%prefix %+ .ar%2 - %%base
        %rotate 1
    %endrep
%endmacro

JMP_TABLE generate_grain_y_16bpc_avx2, 0, 1, 2, 3
JMP_TABLE generate_grain_uv_420_16bpc_avx2, 0, 1, 2, 3

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

%define m(x) mangle(private_prefix %+ _ %+ x %+ SUFFIX)

INIT_YMM avx2
cglobal generate_grain_y_16bpc, 3, 9, 16, buf, fg_data, bdmax
    lea              r4, [pb_mask]
%define base r4-pb_mask
    movq            xm1, [base+rnd_next_upperbit_mask]
    movq            xm4, [base+mul_bits]
    movq            xm7, [base+hmul_bits]
    mov             r3d, [fg_dataq+FGData.grain_scale_shift]
    lea             r6d, [bdmaxq+1]
    shr             r6d, 11             ; 0 for 10bpc, 2 for 12bpc
    sub              r3, r6
    vpbroadcastw    xm8, [base+round+r3*2-2]
    mova            xm5, [base+pb_mask]
    vpbroadcastw    xm0, [fg_dataq+FGData.seed]
    vpbroadcastd    xm9, [base+pd_m65536]
    mov              r3, -73*82*2
    sub            bufq, r3
    lea              r6, [gaussian_sequence]
.loop:
    pand            xm2, xm0, xm1
    psrlw           xm3, xm2, 10
    por             xm2, xm3            ; bits 0xf, 0x1e, 0x3c and 0x78 are set
    pmullw          xm2, xm4            ; bits 0x0f00 are set
    pshufb          xm2, xm5, xm2       ; set 15th bit for next 4 seeds
    psllq           xm6, xm2, 30
    por             xm2, xm6
    psllq           xm6, xm2, 15
    por             xm2, xm6            ; aggregate each bit into next seed's high bit
    pmulhuw         xm3, xm0, xm7
    por             xm2, xm3            ; 4 next output seeds
    pshuflw         xm0, xm2, q3333
    psrlw           xm2, 5
    pmovzxwd        xm3, xm2
    mova            xm6, xm9
    vpgatherdd      xm2, [r6+xm3*2], xm6
    pandn           xm2, xm9, xm2
    packusdw        xm2, xm2
    paddw           xm2, xm2            ; otherwise bpc=12 w/ grain_scale_shift=0
                                        ; shifts by 0, which pmulhrsw does not support
    pmulhrsw        xm2, xm8
    movq      [bufq+r3], xm2
    add              r3, 4*2
    jl .loop

    ; auto-regression code
    movsxd           r3, [fg_dataq+FGData.ar_coeff_lag]
    movsxd           r3, [base+generate_grain_y_16bpc_avx2_table+r3*4]
    lea              r3, [r3+base+generate_grain_y_16bpc_avx2_table]
    jmp              r3

.ar1:
    DEFINE_ARGS buf, fg_data, max, shift, val3, min, cf3, x, val0
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    movsx          cf3d, byte [fg_dataq+FGData.ar_coeffs_y+3]
    movd            xm4, [fg_dataq+FGData.ar_coeffs_y]
    DEFINE_ARGS buf, h, max, shift, val3, min, cf3, x, val0
    pinsrb          xm4, [pb_1], 3
    pmovsxbw        xm4, xm4
    pshufd          xm5, xm4, q1111
    pshufd          xm4, xm4, q0000
    vpbroadcastw    xm3, [base+round_vals+shiftq*2-12]    ; rnd
    sub            bufq, 2*(82*73-(82*3+79))
    mov              hd, 70
    sar            maxd, 1
    mov            mind, maxd
    xor            mind, -1
.y_loop_ar1:
    mov              xq, -76
    movsx         val3d, word [bufq+xq*2-2]
.x_loop_ar1:
    movu            xm0, [bufq+xq*2-82*2-2]     ; top/left
    psrldq          xm2, xm0, 2                 ; top
    psrldq          xm1, xm0, 4                 ; top/right
    punpcklwd       xm0, xm2
    punpcklwd       xm1, xm3
    pmaddwd         xm0, xm4
    pmaddwd         xm1, xm5
    paddd           xm0, xm1
.x_loop_ar1_inner:
    movd          val0d, xm0
    psrldq          xm0, 4
    imul          val3d, cf3d
    add           val3d, val0d
    sarx          val3d, val3d, shiftd
    movsx         val0d, word [bufq+xq*2]
    add           val3d, val0d
    cmp           val3d, maxd
    cmovg         val3d, maxd
    cmp           val3d, mind
    cmovl         val3d, mind
    mov word [bufq+xq*2], val3w
    ; keep val3d in-place as left for next x iteration
    inc              xq
    jz .x_loop_ar1_end
    test             xq, 3
    jnz .x_loop_ar1_inner
    jmp .x_loop_ar1

.x_loop_ar1_end:
    add            bufq, 82*2
    dec              hd
    jg .y_loop_ar1
.ar0:
    RET

.ar2:
    DEFINE_ARGS buf, fg_data, bdmax, shift
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    vpbroadcastw   xm14, [base+round_vals-12+shiftq*2]
    movq            xm8, [fg_dataq+FGData.ar_coeffs_y+5]    ; cf5-11
    vinserti128      m8, [fg_dataq+FGData.ar_coeffs_y+0], 1 ; cf0-4
    pxor             m9, m9
    punpcklwd      xm14, xm9
    pcmpgtb          m9, m8
    punpcklbw        m8, m9                                 ; cf5-11,0-4
    vpermq           m9, m8, q3333                          ; cf4
    psrldq         xm10, xm8, 6                             ; cf8-11
    vpblendw        xm9, xm10, 11111110b                    ; cf4,9-11
    pshufd          m12, m8, q0000                          ; cf[5,6], cf[0-1]
    pshufd          m11, m8, q1111                          ; cf[7,8], cf[2-3]
    pshufd         xm13, xm9, q1111                         ; cf[10,11]
    pshufd         xm10, xm9, q0000                         ; cf[4,9]
    sar          bdmaxd, 1
    movd           xm15, bdmaxd
    pcmpeqd         xm7, xm7
    vpbroadcastd   xm15, xm15                               ; max_grain
    pxor            xm7, xm15                               ; min_grain
    sub            bufq, 2*(82*73-(82*3+79))
    DEFINE_ARGS buf, fg_data, h, x
    mov              hd, 70
.y_loop_ar2:
    mov              xq, -76

.x_loop_ar2:
    movu            xm0, [bufq+xq*2-82*2-4]     ; y=-1,x=[-2,+5]
    vinserti128      m0, [bufq+xq*2-82*4-4], 1  ; y=-2,x=[-2,+5]
    psrldq           m1, m0, 2                  ; y=-1/-2,x=[-1,+5]
    psrldq           m2, m0, 4                  ; y=-1/-2,x=[-0,+5]
    psrldq           m3, m0, 6                  ; y=-1/-2,x=[+1,+5]

    vextracti128    xm4, m0, 1                  ; y=-2,x=[-2,+5]
    punpcklwd        m2, m3                     ; y=-1/-2,x=[+0/+1,+1/+2,+2/+3,+3/+4]
    punpckhwd       xm4, xm0                    ; y=-2/-1 interleaved, x=[+2,+5]
    punpcklwd        m0, m1                     ; y=-1/-2,x=[-2/-1,-1/+0,+0/+1,+1/+2]

    pmaddwd          m2, m11
    pmaddwd          m0, m12
    pmaddwd         xm4, xm10

    paddd            m0, m2
    vextracti128    xm2, m0, 1
    paddd           xm4, xm0
    paddd           xm2, xm14
    paddd           xm2, xm4

    movu            xm0, [bufq+xq*2-4]      ; y=0,x=[-2,+5]
    pshufd          xm4, xm0, q3321
    pmovsxwd        xm4, xm4                ; in dwords, y=0,x=[0,3]
.x_loop_ar2_inner:
    pmaddwd         xm3, xm0, xm13
    paddd           xm3, xm2
    psrldq          xm2, 4                  ; shift top to next pixel
    psrad           xm3, [fg_dataq+FGData.ar_coeff_shift]
    ; skip packssdw because we only care about one value
    paddd           xm3, xm4
    pminsd          xm3, xm15
    pmaxsd          xm3, xm7
    pextrw  [bufq+xq*2], xm3, 0
    psrldq          xm4, 4
    pslldq          xm3, 2
    psrldq          xm0, 2
    vpblendw        xm0, xm3, 0010b
    inc              xq
    jz .x_loop_ar2_end
    test             xq, 3
    jnz .x_loop_ar2_inner
    jmp .x_loop_ar2

.x_loop_ar2_end:
    add            bufq, 82*2
    dec              hd
    jg .y_loop_ar2
    RET

.ar3:
    DEFINE_ARGS buf, fg_data, bdmax, shift
%if WIN64
    mov              r6, rsp
    and             rsp, ~31
    sub             rsp, 64
    %define         tmp  rsp
%elif STACK_ALIGNMENT < 32
    mov              r6, rsp
    and              r6, ~31
    %define         tmp  r6-64
%else
    %define         tmp  rsp+stack_offset-88
%endif
    sar          bdmaxd, 1
    movd           xm15, bdmaxd
    pcmpeqd        xm13, xm13
    vpbroadcastd   xm15, xm15                                   ; max_grain
    pxor           xm13, xm15                                   ; min_grain
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    vpbroadcastw    m14, [base+round_vals+shiftq*2-12]
    movq            xm0, [fg_dataq+FGData.ar_coeffs_y+ 0]       ; cf0-6
    movd            xm1, [fg_dataq+FGData.ar_coeffs_y+14]       ; cf14-16
    pinsrb          xm0, [fg_dataq+FGData.ar_coeffs_y+13], 7    ; cf0-6,13
    pinsrb          xm1, [pb_1], 3                              ; cf14-16,pb_1
    movd            xm2, [fg_dataq+FGData.ar_coeffs_y+21]       ; cf21-23
    vinserti128      m0, [fg_dataq+FGData.ar_coeffs_y+ 7], 1    ; cf7-13
    vinserti128      m1, [fg_dataq+FGData.ar_coeffs_y+17], 1    ; cf17-20
    punpcklbw        m0, m0                                     ; sign-extension
    punpcklbw        m1, m1                                     ; sign-extension
    punpcklbw       xm2, xm2
    REPX   {psraw x, 8}, m0, m1, xm2

    pshufd           m8, m0, q0000              ; cf[0,1] | cf[7,8]
    pshufd           m9, m0, q1111              ; cf[2,3] | cf[9,10]
    pshufd          m10, m0, q2222              ; cf[4,5] | cf[11,12]
    pshufd         xm11, xm0, q3333             ; cf[6,13]

    pshufd           m3, m1, q0000              ; cf[14,15] | cf[17,18]
    pshufd           m4, m1, q1111              ; cf[16],pw_1 | cf[19,20]
    mova     [tmp+0*32], m3
    mova     [tmp+1*32], m4

    paddw           xm5, xm14, xm14
    vpblendw       xm12, xm2, xm5, 00001000b

    DEFINE_ARGS buf, fg_data, h, x
    sub            bufq, 2*(82*73-(82*3+79))
    mov              hd, 70
.y_loop_ar3:
    mov              xq, -76

.x_loop_ar3:
    movu            xm0, [bufq+xq*2-82*6-6+ 0]      ; y=-3,x=[-3,+4]
    movq            xm1, [bufq+xq*2-82*6-6+16]      ; y=-3,x=[+5,+8]
    movu            xm2, [bufq+xq*2-82*2-6+ 0]      ; y=-1,x=[-3,+4]
    vinserti128      m0, [bufq+xq*2-82*4-6+ 0], 1   ; y=-3/-2,x=[-3,+4]
    vinserti128      m1, [bufq+xq*2-82*4-6+16], 1   ; y=-3/-2,x=[+5,+12]
    vinserti128      m2, [bufq+xq*2-82*2-6+ 6], 1   ; y=-1,x=[+1,+8]

    palignr         m4, m1, m0, 2                   ; y=-3/-2,x=[-2,+5]
    palignr         m1, m0, 12                      ; y=-3/-2,x=[+3,+6]
    punpckhwd       m5, m0, m4                      ; y=-3/-2,x=[+1/+2,+2/+3,+3/+4,+4/+5]
    punpcklwd       m0, m4                          ; y=-3/-2,x=[-3/-2,-2/-1,-1/+0,+0/+1]
    palignr         m6, m5, m0, 8                   ; y=-3/-2,x=[-1/+0,+0/+1,+1/+2,+2/+3]
    vextracti128   xm7, m1, 1
    punpcklwd      xm1, xm7                         ; y=-3/-2 interleaved,x=[+3,+4,+5,+6]

    psrldq          m3, m2, 2
    psrldq          m4, m2, 4
    psrldq          m7, m2, 6
    vpblendd        m7, m14, 00001111b              ; rounding constant
    punpcklwd       m2, m3                          ; y=-1,x=[-3/-2,-2/-1,-1/+0,+0/+1]
                                                    ;      x=[+0/+1,+1/+2,+2/+3,+3/+4]
    punpcklwd       m4, m7                          ; y=-1,x=[-1/rnd,+0/rnd,+1/rnd,+2/rnd]
                                                    ;      x=[+2/+3,+3/+4,+4/+5,+5,+6]

    pmaddwd          m0, m8
    pmaddwd          m6, m9
    pmaddwd          m5, m10
    pmaddwd         xm1, xm11
    pmaddwd          m2, [tmp+0*32]
    pmaddwd          m4, [tmp+1*32]

    paddd            m0, m6
    paddd            m5, m2
    paddd            m0, m4
    paddd            m0, m5
    vextracti128    xm4, m0, 1
    paddd           xm0, xm1
    paddd           xm0, xm4

    movu            xm1, [bufq+xq*2-6]        ; y=0,x=[-3,+4]
.x_loop_ar3_inner:
    pmaddwd         xm2, xm1, xm12
    pshufd          xm3, xm2, q1111
    paddd           xm2, xm3                ; left+cur
    paddd           xm2, xm0                ; add top
    psrldq          xm0, 4
    psrad           xm2, [fg_dataq+FGData.ar_coeff_shift]
    ; skip packssdw because we only care about one value
    pminsd          xm2, xm15
    pmaxsd          xm2, xm13
    pextrw  [bufq+xq*2], xm2, 0
    pslldq          xm2, 4
    psrldq          xm1, 2
    vpblendw        xm1, xm2, 0100b
    inc              xq
    jz .x_loop_ar3_end
    test             xq, 3
    jnz .x_loop_ar3_inner
    jmp .x_loop_ar3

.x_loop_ar3_end:
    add            bufq, 82*2
    dec              hd
    jg .y_loop_ar3
%if WIN64
    mov             rsp, r6
%endif
    RET

INIT_XMM avx2
cglobal generate_grain_uv_420_16bpc, 4, 10, 16, buf, bufy, fg_data, uv, bdmax
%define base r8-pb_mask
    lea              r8, [pb_mask]
    movifnidn    bdmaxd, bdmaxm
    movq            xm1, [base+rnd_next_upperbit_mask]
    movq            xm4, [base+mul_bits]
    movq            xm7, [base+hmul_bits]
    mov             r5d, [fg_dataq+FGData.grain_scale_shift]
    lea             r6d, [bdmaxq+1]
    shr             r6d, 11             ; 0 for 10bpc, 2 for 12bpc
    sub              r5, r6
    vpbroadcastw    xm8, [base+round+r5*2-2]
    mova            xm5, [base+pb_mask]
    vpbroadcastw    xm0, [fg_dataq+FGData.seed]
    vpbroadcastw    xm9, [base+pw_seed_xor+uvq*4]
    pxor            xm0, xm9
    vpbroadcastd    xm9, [base+pd_m65536]
    lea              r6, [gaussian_sequence]
    mov             r7d, 38
    add            bufq, 44*2
.loop_y:
    mov              r5, -44
.loop_x:
    pand            xm2, xm0, xm1
    psrlw           xm3, xm2, 10
    por             xm2, xm3            ; bits 0xf, 0x1e, 0x3c and 0x78 are set
    pmullw          xm2, xm4            ; bits 0x0f00 are set
    pshufb          xm2, xm5, xm2       ; set 15th bit for next 4 seeds
    psllq           xm6, xm2, 30
    por             xm2, xm6
    psllq           xm6, xm2, 15
    por             xm2, xm6            ; aggregate each bit into next seed's high bit
    pmulhuw         xm3, xm0, xm7
    por             xm2, xm3            ; 4 next output seeds
    pshuflw         xm0, xm2, q3333
    psrlw           xm2, 5
    pmovzxwd        xm3, xm2
    mova            xm6, xm9
    vpgatherdd      xm2, [r6+xm3*2], xm6
    pandn           xm2, xm9, xm2
    packusdw        xm2, xm2
    paddw           xm2, xm2            ; otherwise bpc=12 w/ grain_scale_shift=0
                                        ; shifts by 0, which pmulhrsw does not support
    pmulhrsw        xm2, xm8
    movq    [bufq+r5*2], xm2
    add              r5, 4
    jl .loop_x
    add            bufq, 82*2
    dec             r7d
    jg .loop_y

    ; auto-regression code
    movsxd           r5, [fg_dataq+FGData.ar_coeff_lag]
    movsxd           r5, [base+generate_grain_uv_420_16bpc_avx2_table+r5*4]
    lea              r5, [r5+base+generate_grain_uv_420_16bpc_avx2_table]
    jmp              r5

.ar0:
    INIT_YMM avx2
    DEFINE_ARGS buf, bufy, fg_data, uv, bdmax, shift
    imul            uvd, 28
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    movd            xm4, [fg_dataq+FGData.ar_coeffs_uv+uvq]
    vpbroadcastw     m3, [base+hmul_bits+shiftq*2-10]
    sar          bdmaxd, 1
    movd           xm14, bdmaxd
    pcmpeqw          m7, m7
    vpbroadcastw    m14, xm14                       ; max_gain
    pxor             m7, m14                        ; min_grain
    DEFINE_ARGS buf, bufy, h
    pmovsxbw        xm4, xm4
    vpbroadcastw     m6, [hmul_bits+4]
    vpbroadcastw     m4, xm4
    pxor             m5, m5
    sub            bufq, 2*(82*38+82-(82*3+41))
    add           bufyq, 2*(3+82*3)
    mov              hd, 35
.y_loop_ar0:
    ; first 32 pixels
    movu            xm8, [bufyq]
    movu            xm9, [bufyq+82*2]
    movu           xm10, [bufyq+     16]
    movu           xm11, [bufyq+82*2+16]
    vinserti128      m8, [bufyq+     32], 1
    vinserti128      m9, [bufyq+82*2+32], 1
    vinserti128     m10, [bufyq+     48], 1
    vinserti128     m11, [bufyq+82*2+48], 1
    paddw            m8, m9
    paddw           m10, m11
    phaddw           m8, m10
    movu           xm10, [bufyq+     64]
    movu           xm11, [bufyq+82*2+64]
    movu           xm12, [bufyq+     80]
    movu           xm13, [bufyq+82*2+80]
    vinserti128     m10, [bufyq+     96], 1
    vinserti128     m11, [bufyq+82*2+96], 1
    vinserti128     m12, [bufyq+     112], 1
    vinserti128     m13, [bufyq+82*2+112], 1
    paddw           m10, m11
    paddw           m12, m13
    phaddw          m10, m12
    pmulhrsw         m8, m6
    pmulhrsw        m10, m6
    punpckhwd        m9, m8, m5
    punpcklwd        m8, m5
    punpckhwd       m11, m10, m5
    punpcklwd       m10, m5
    REPX {pmaddwd x, m4}, m8, m9, m10, m11
    REPX {psrad x, 5}, m8, m9, m10, m11
    packssdw         m8, m9
    packssdw        m10, m11
    REPX {pmulhrsw x, m3}, m8, m10
    paddw            m8, [bufq+ 0]
    paddw           m10, [bufq+32]
    pminsw           m8, m14
    pminsw          m10, m14
    pmaxsw           m8, m7
    pmaxsw          m10, m7
    movu      [bufq+ 0], m8
    movu      [bufq+32], m10

    ; last 6 pixels
    movu            xm8, [bufyq+32*4]
    movu           xm10, [bufyq+32*4+16]
    paddw           xm8, [bufyq+32*4+82*2]
    paddw          xm10, [bufyq+32*4+82*2+16]
    phaddw          xm8, xm10
    pmulhrsw        xm8, xm6
    punpckhwd       xm9, xm8, xm5
    punpcklwd       xm8, xm5
    REPX {pmaddwd x, xm4}, xm8, xm9
    REPX {psrad   x, 5}, xm8, xm9
    packssdw        xm8, xm9
    pmulhrsw        xm8, xm3
    movu            xm0, [bufq+32*2]
    paddw           xm8, xm0
    pminsw          xm8, xm14
    pmaxsw          xm8, xm7
    vpblendw        xm0, xm8, xm0, 11000000b
    movu    [bufq+32*2], xm0

    add            bufq, 82*2
    add           bufyq, 82*4
    dec              hd
    jg .y_loop_ar0
    RET

.ar1:
    INIT_XMM avx2
    DEFINE_ARGS buf, bufy, fg_data, uv, max, cf3, min, val3, x, shift
    imul            uvd, 28
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    movsx          cf3d, byte [fg_dataq+FGData.ar_coeffs_uv+uvq+3]
    movd            xm4, [fg_dataq+FGData.ar_coeffs_uv+uvq]
    pinsrb          xm4, [fg_dataq+FGData.ar_coeffs_uv+uvq+4], 3
    DEFINE_ARGS buf, bufy, h, val0, max, cf3, min, val3, x, shift
    pmovsxbw        xm4, xm4
    pshufd          xm5, xm4, q1111
    pshufd          xm4, xm4, q0000
    pmovsxwd        xm3, [base+round_vals+shiftq*2-12]    ; rnd
    vpbroadcastw    xm6, [hmul_bits+4]
    vpbroadcastd    xm3, xm3
    sub            bufq, 2*(82*38+44-(82*3+41))
    add           bufyq, 2*(79+82*3)
    mov              hd, 35
    sar            maxd, 1
    mov            mind, maxd
    xor            mind, -1
.y_loop_ar1:
    mov              xq, -38
    movsx         val3d, word [bufq+xq*2-2]
.x_loop_ar1:
    movu            xm0, [bufq+xq*2-82*2-2] ; top/left
    movu            xm8, [bufyq+xq*4]
    psrldq          xm2, xm0, 2             ; top
    psrldq          xm1, xm0, 4             ; top/right
    phaddw          xm8, [bufyq+xq*4+82*2]
    pshufd          xm9, xm8, q3232
    paddw           xm8, xm9
    pmulhrsw        xm8, xm6
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
    movsx         val0d, word [bufq+xq*2]
    add           val3d, val0d
    cmp           val3d, maxd
    cmovg         val3d, maxd
    cmp           val3d, mind
    cmovl         val3d, mind
    mov word [bufq+xq*2], val3w
    ; keep val3d in-place as left for next x iteration
    inc              xq
    jz .x_loop_ar1_end
    test             xq, 3
    jnz .x_loop_ar1_inner
    jmp .x_loop_ar1

.x_loop_ar1_end:
    add            bufq, 82*2
    add           bufyq, 82*4
    dec              hd
    jg .y_loop_ar1
    RET

    INIT_YMM avx2
.ar2:
    DEFINE_ARGS buf, bufy, fg_data, uv, bdmax, shift
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    imul            uvd, 28
    sar          bdmaxd, 1
    movd            xm6, bdmaxd
    pcmpeqd         xm5, xm5
    vpbroadcastd    xm6, xm6                ; max_grain
    pxor            xm5, xm6                ; min_grain
    vpbroadcastw    xm7, [base+hmul_bits+4]
    vpbroadcastw   xm15, [base+round_vals-12+shiftq*2]

    movd            xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+5]
    pinsrb          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+12], 4
    pinsrb          xm0, [pb_1], 5
    pinsrw          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+10], 3
    movhps          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+0]
    pinsrb          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+9], 13
    pmovsxbw         m0, xm0

    pshufd         xm13, xm0, q3333
    pshufd          m12, m0, q0000
    pshufd          m11, m0, q1111
    pshufd          m10, m0, q2222

    DEFINE_ARGS buf, bufy, fg_data, h, x
    sub            bufq, 2*(82*38+44-(82*3+41))
    add           bufyq, 2*(79+82*3)
    mov              hd, 35
.y_loop_ar2:
    mov              xq, -38

.x_loop_ar2:
    movu            xm0, [bufq+xq*2-82*2-4]     ; y=-1,x=[-2,+5]
    vinserti128      m0, [bufq+xq*2-82*4-4], 1  ; y=-2,x=[-2,+5]
    psrldq           m1, m0, 2                  ; y=-1/-2,x=[-1,+5]
    psrldq           m2, m0, 4                  ; y=-1/-2,x=[-0,+5]
    psrldq           m3, m0, 6                  ; y=-1/-2,x=[+1,+5]

    movu            xm8, [bufyq+xq*4]
    paddw           xm8, [bufyq+xq*4+82*2]
    phaddw          xm8, xm8

    vinserti128      m4, xm0, 1                 ; y=-1,x=[-2,+5]
    punpcklwd        m2, m3                     ; y=-1/-2,x=[+0/+1,+1/+2,+2/+3,+3/+4]
    punpckhwd        m4, m0, m4                 ; y=-2/-1 interleaved, x=[+2,+5]
    punpcklwd        m0, m1                     ; y=-1/-2,x=[-2/-1,-1/+0,+0/+1,+1/+2]

    pmulhrsw        xm1, xm8, xm7
    punpcklwd       xm1, xm15                   ; luma, round interleaved
    vpblendd         m1, m1, m4, 11110000b

    pmaddwd          m2, m11
    pmaddwd          m0, m12
    pmaddwd          m1, m10
    paddd            m2, m0
    paddd            m2, m1
    vextracti128    xm0, m2, 1
    paddd           xm2, xm0

    movu            xm0, [bufq+xq*2-4]      ; y=0,x=[-2,+5]
    pshufd          xm4, xm0, q3321
    pmovsxwd        xm4, xm4                ; y=0,x=[0,3] in dword
.x_loop_ar2_inner:
    pmaddwd         xm3, xm0, xm13
    paddd           xm3, xm2
    psrldq          xm2, 4                  ; shift top to next pixel
    psrad           xm3, [fg_dataq+FGData.ar_coeff_shift]
    ; we do not need to packssdw since we only care about one value
    paddd           xm3, xm4
    pminsd          xm3, xm6
    pmaxsd          xm3, xm5
    pextrw  [bufq+xq*2], xm3, 0
    psrldq          xm0, 2
    pslldq          xm3, 2
    psrldq          xm4, 4
    vpblendw        xm0, xm3, 00000010b
    inc              xq
    jz .x_loop_ar2_end
    test             xq, 3
    jnz .x_loop_ar2_inner
    jmp .x_loop_ar2

.x_loop_ar2_end:
    add            bufq, 82*2
    add           bufyq, 82*4
    dec              hd
    jg .y_loop_ar2
    RET

.ar3:
    DEFINE_ARGS buf, bufy, fg_data, uv, bdmax, shift
%if WIN64
    mov              r6, rsp
    and             rsp, ~31
    sub             rsp, 96
    %define         tmp  rsp
%elif STACK_ALIGNMENT < 32
    mov              r6, rsp
    and              r6, ~31
    %define         tmp  r6-96
%else
    %define         tmp  rsp+stack_offset-120
%endif
    mov          shiftd, [fg_dataq+FGData.ar_coeff_shift]
    imul            uvd, 28
    vpbroadcastw   xm14, [base+round_vals-12+shiftq*2]
    sar          bdmaxd, 1
    movd           xm15, bdmaxd
    pcmpeqd        xm13, xm13
    vpbroadcastd   xm15, xm15                   ; max_grain
    pxor           xm13, xm15                   ; min_grain
    vpbroadcastw   xm12, [base+hmul_bits+4]

    movq            xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+ 0]
    pinsrb          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+24], 7   ; luma
    movhps          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+ 7]
    pmovsxbw         m0, xm0

    pshufd          m11, m0, q3333
    pshufd          m10, m0, q2222
    pshufd           m9, m0, q1111
    pshufd           m8, m0, q0000

    movd            xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+14]
    pinsrb          xm0, [pb_1], 3
    pinsrd          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+21], 1
    pinsrd          xm0, [fg_dataq+FGData.ar_coeffs_uv+uvq+17], 2
    pmovsxbw         m0, xm0

    pshufd           m1, m0, q0000
    pshufd           m2, m0, q1111
    mova     [tmp+32*2], m11
    pshufd         xm11, xm0, q3232
    mova     [tmp+32*0], m1
    mova     [tmp+32*1], m2
    pinsrw         xm11, [base+round_vals-10+shiftq*2], 3

    DEFINE_ARGS buf, bufy, fg_data, h, unused, x
    sub            bufq, 2*(82*38+44-(82*3+41))
    add           bufyq, 2*(79+82*3)
    mov              hd, 35
.y_loop_ar3:
    mov              xq, -38

.x_loop_ar3:
    movu            xm0, [bufq+xq*2-82*6-6+ 0]      ; y=-3,x=[-3,+4]
    movq            xm1, [bufq+xq*2-82*6-6+16]      ; y=-3,x=[+5,+8]
    movu            xm2, [bufq+xq*2-82*2-6+ 0]      ; y=-1,x=[-3,+4]
    vinserti128      m0, [bufq+xq*2-82*4-6+ 0], 1   ; y=-3/-2,x=[-3,+4]
    vinserti128      m1, [bufq+xq*2-82*4-6+16], 1   ; y=-3/-2,x=[+5,+12]
    vinserti128      m2, [bufq+xq*2-82*2-6+ 6], 1   ; y=-1,x=[+1,+8]

    movu           xm7, [bufyq+xq*4]
    paddw          xm7, [bufyq+xq*4+82*2]
    phaddw         xm7, xm7

    palignr         m4, m1, m0, 2                   ; y=-3/-2,x=[-2,+5]
    palignr         m1, m0, 12                      ; y=-3/-2,x=[+3,+6]
    punpckhwd       m5, m0, m4                      ; y=-3/-2,x=[+1/+2,+2/+3,+3/+4,+4/+5]
    punpcklwd       m0, m4                          ; y=-3/-2,x=[-3/-2,-2/-1,-1/+0,+0/+1]
    palignr         m6, m5, m0, 8                   ; y=-3/-2,x=[-1/+0,+0/+1,+1/+2,+2/+3]
    pmulhrsw       xm7, xm12
    punpcklwd       m1, m7

    psrldq          m3, m2, 2
    psrldq          m4, m2, 4
    psrldq          m7, m2, 6
    vpblendd        m7, m14, 00001111b              ; rounding constant
    punpcklwd       m2, m3                          ; y=-1,x=[-3/-2,-2/-1,-1/+0,+0/+1]
                                                    ;      x=[+0/+1,+1/+2,+2/+3,+3/+4]
    punpcklwd       m4, m7                          ; y=-1,x=[-1/rnd,+0/rnd,+1/rnd,+2/rnd]
                                                    ;      x=[+2/+3,+3/+4,+4/+5,+5,+6]

    pmaddwd          m0, m8
    pmaddwd          m6, m9
    pmaddwd          m5, m10
    pmaddwd          m1, [tmp+32*2]
    pmaddwd          m2, [tmp+32*0]
    pmaddwd          m4, [tmp+32*1]

    paddd            m0, m6
    paddd            m5, m2
    paddd            m4, m1
    paddd            m0, m4
    paddd            m0, m5
    vextracti128    xm4, m0, 1
    paddd           xm0, xm4

    movu            xm1, [bufq+xq*2-6]        ; y=0,x=[-3,+4]
.x_loop_ar3_inner:
    pmaddwd         xm2, xm1, xm11
    pshufd          xm3, xm2, q1111
    paddd           xm2, xm3                ; left+cur
    paddd           xm2, xm0                ; add top
    psrldq          xm0, 4
    psrad           xm2, [fg_dataq+FGData.ar_coeff_shift]
    ; no need to packssdw since we only care about one value
    pminsd          xm2, xm15
    pmaxsd          xm2, xm13
    pextrw  [bufq+xq*2], xm2, 0
    pslldq          xm2, 4
    psrldq          xm1, 2
    vpblendw        xm1, xm2, 00000100b
    inc              xq
    jz .x_loop_ar3_end
    test             xq, 3
    jnz .x_loop_ar3_inner
    jmp .x_loop_ar3

.x_loop_ar3_end:
    add            bufq, 82*2
    add           bufyq, 82*4
    dec              hd
    jg .y_loop_ar3
%if WIN64
    mov             rsp, r6
%endif
    RET

INIT_YMM avx2
cglobal fgy_32x32xn_16bpc, 6, 14, 16, dst, src, stride, fg_data, w, scaling, grain_lut
    mov             r7d, [fg_dataq+FGData.scaling_shift]
    lea              r8, [pb_mask]
%define base r8-pb_mask
    vpbroadcastw    m11, [base+mul_bits+r7*2-14]
    mov             r6d, [fg_dataq+FGData.clip_to_restricted_range]
    mov             r9d, r9m        ; bdmax
    sar             r9d, 11         ; is_12bpc
    shlx           r10d, r6d, r9d
    vpbroadcastw    m13, [base+min+r10*2]
    lea             r9d, [r9d*3]
    lea             r9d, [r6d*2+r9d]
    vpbroadcastw    m12, [base+max+r9*2]
    vpbroadcastw    m10, r9m
    pxor             m2, m2

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused1, \
                sby, see

    movifnidn      sbyd, sbym
    test           sbyd, sbyd
    setnz           r7b
    test            r7b, byte [fg_dataq+FGData.overlap_flag]
    jnz .vertical_overlap

    imul           seed, sbyd, (173 << 24) | 37
    add            seed, (105 << 24) | 178
    rol            seed, 8
    movzx          seed, seew
    xor            seed, [fg_dataq+FGData.seed]

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                unused1, unused2, see, src_bak

    lea        src_bakq, [srcq+wq*2]
    neg              wq
    sub            dstq, srcq

.loop_x:
    mov             r6d, seed
    or             seed, 0xEFF4
    shr             r6d, 1
    test           seeb, seeh
    lea            seed, [r6+0x8000]
    cmovp          seed, r6d                ; updated seed

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, src_bak

    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 164
    lea           offyq, [offyq+offxq*2+747] ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, src_bak

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y:
    ; src
    pminuw           m0, m10, [srcq+ 0]
    pminuw           m1, m10, [srcq+32]          ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2
    punpckhwd        m7, m1, m2
    punpcklwd        m6, m1, m2             ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m3, m3
    mova             m9, m3
    vpgatherdd       m8, [scalingq+m4-3], m3
    vpgatherdd       m4, [scalingq+m5-3], m9
    pcmpeqw          m3, m3
    mova             m9, m3
    vpgatherdd       m5, [scalingq+m6-3], m3
    vpgatherdd       m6, [scalingq+m7-3], m9
    REPX  {psrld x, 24}, m8, m4, m5, m6
    packssdw         m8, m4
    packssdw         m5, m6

    ; grain = grain_lut[offy+y][offx+x]
    movu             m9, [grain_lutq+offxyq*2]
    movu             m3, [grain_lutq+offxyq*2+32]

    ; noise = round2(scaling[src] * grain, scaling_shift)
    REPX {pmullw x, m11}, m8, m5
    pmulhrsw         m9, m8
    pmulhrsw         m3, m5

    ; dst = clip_pixel(src, noise)
    paddw            m0, m9
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova [dstq+srcq+ 0], m0
    mova [dstq+srcq+32], m1

    add            srcq, strideq
    add      grain_lutq, 82*2
    dec              hd
    jg .loop_y

    add              wq, 32
    jge .end
    lea            srcq, [src_bakq+wq*2]
    cmp byte [fg_dataq+FGData.overlap_flag], 0
    je .loop_x

    ; r8m = sbym
    movq           xm15, [pw_27_17_17_27]
    cmp       dword r8m, 0
    jne .loop_x_hv_overlap

    ; horizontal overlap (without vertical overlap)
    vpbroadcastd   xm14, [pd_16]
.loop_x_h_overlap:
    mov             r6d, seed
    or             seed, 0xEFF4
    shr             r6d, 1
    test           seeb, seeh
    lea            seed, [r6+0x8000]
    cmovp          seed, r6d                ; updated seed

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, src_bak, left_offxy

    lea     left_offxyd, [offyd+32]         ; previous column's offy*stride+offx
    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 164
    lea           offyq, [offyq+offxq*2+747] ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, src_bak, left_offxy

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y_h_overlap:
    ; src
    pminuw           m0, m10, [srcq+ 0]
    pminuw           m1, m10, [srcq+32]          ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2
    punpckhwd        m7, m1, m2
    punpcklwd        m6, m1, m2             ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m3, m3
    mova             m9, m3
    vpgatherdd       m8, [scalingq+m4-3], m3
    vpgatherdd       m4, [scalingq+m5-3], m9
    pcmpeqw          m3, m3
    mova             m9, m3
    vpgatherdd       m5, [scalingq+m6-3], m3
    vpgatherdd       m6, [scalingq+m7-3], m9
    REPX  {psrld x, 24}, m8, m4, m5, m6
    packssdw         m8, m4
    packssdw         m5, m6

    ; grain = grain_lut[offy+y][offx+x]
    movu             m9, [grain_lutq+offxyq*2]
    movd            xm7, [grain_lutq+left_offxyq*2]
    punpcklwd       xm7, xm9
    pmaddwd         xm7, xm15
    paddd           xm7, xm14
    psrad           xm7, 5
    packssdw        xm7, xm7
    vpblendd         m9, m7, 00000001b
    pcmpeqw          m3, m3
    psraw            m7, m10, 1             ; max_grain
    pxor             m3, m7                 ; min_grain
    pminsw           m9, m7
    pmaxsw           m9, m3
    movu             m3, [grain_lutq+offxyq*2+32]

    ; noise = round2(scaling[src] * grain, scaling_shift)
    REPX {pmullw x, m11}, m8, m5
    pmulhrsw         m9, m8
    pmulhrsw         m3, m5

    ; dst = clip_pixel(src, noise)
    paddw            m0, m9
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova [dstq+srcq+ 0], m0
    mova [dstq+srcq+32], m1

    add            srcq, strideq
    add      grain_lutq, 82*2
    dec              hd
    jg .loop_y_h_overlap

    add              wq, 32
    jge .end
    lea            srcq, [src_bakq+wq*2]

    ; r8m = sbym
    cmp       dword r8m, 0
    jne .loop_x_hv_overlap
    jmp .loop_x_h_overlap

.end:
    RET

.vertical_overlap:
    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused1, \
                sby, see

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

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                unused1, unused2, see, src_bak

    lea        src_bakq, [srcq+wq*2]
    neg              wq
    sub            dstq, srcq

    vpbroadcastd    m14, [pd_16]
.loop_x_v_overlap:
    vpbroadcastd    m15, [pw_27_17_17_27]

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

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, src_bak, unused, top_offxy

    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 164
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq*2+0x10001*747+32*82]

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, src_bak, unused, top_offxy

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y_v_overlap:
    ; grain = grain_lut[offy+y][offx+x]
    movu             m3, [grain_lutq+offxyq*2]
    movu             m7, [grain_lutq+top_offxyq*2]
    punpckhwd        m9, m7, m3
    punpcklwd        m7, m3
    REPX {pmaddwd x, m15}, m9, m7
    REPX {paddd   x, m14}, m9, m7
    REPX {psrad   x, 5}, m9, m7
    packssdw         m7, m9
    pcmpeqw          m0, m0
    psraw            m1, m10, 1             ; max_grain
    pxor             m0, m1                 ; min_grain
    pminsw           m7, m1
    pmaxsw           m7, m0
    movu             m3, [grain_lutq+offxyq*2+32]
    movu             m8, [grain_lutq+top_offxyq*2+32]
    punpckhwd        m9, m8, m3
    punpcklwd        m8, m3
    REPX {pmaddwd x, m15}, m9, m8
    REPX {paddd   x, m14}, m9, m8
    REPX {psrad   x, 5}, m9, m8
    packssdw         m8, m9
    pminsw           m8, m1
    pmaxsw           m8, m0

    ; src
    pminuw           m0, m10, [srcq+ 0]          ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2

    ; scaling[src]
    pcmpeqw          m3, m3
    mova             m9, m3
    vpgatherdd       m6, [scalingq+m4-3], m3
    vpgatherdd       m4, [scalingq+m5-3], m9
    REPX  {psrld x, 24}, m6, m4
    packssdw         m6, m4

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m6, m11
    pmulhrsw         m6, m7

    ; same for the other half
    pminuw           m1, m10, [srcq+32]          ; m0-1: src as word
    punpckhwd        m9, m1, m2
    punpcklwd        m4, m1, m2             ; m4-7: src as dword
    pcmpeqw          m3, m3
    mova             m7, m3
    vpgatherdd       m5, [scalingq+m4-3], m3
    vpgatherdd       m4, [scalingq+m9-3], m7
    REPX  {psrld x, 24}, m5, m4
    packssdw         m5, m4

    pmullw           m5, m11
    pmulhrsw         m5, m8

    ; dst = clip_pixel(src, noise)
    paddw            m0, m6
    paddw            m1, m5
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova [dstq+srcq+ 0], m0
    mova [dstq+srcq+32], m1

    vpbroadcastd    m15, [pw_27_17_17_27+4] ; swap weights for second v-overlap line
    add            srcq, strideq
    add      grain_lutq, 82*2
    dec              hw
    jz .end_y_v_overlap
    ; 2 lines get vertical overlap, then fall back to non-overlap code for
    ; remaining (up to) 30 lines
    xor              hd, 0x10000
    test             hd, 0x10000
    jnz .loop_y_v_overlap
    jmp .loop_y

.end_y_v_overlap:
    add              wq, 32
    jge .end_hv
    lea            srcq, [src_bakq+wq*2]

    ; since fg_dataq.overlap is guaranteed to be set, we never jump
    ; back to .loop_x_v_overlap, and instead always fall-through to
    ; h+v overlap

    movq           xm15, [pw_27_17_17_27]
.loop_x_hv_overlap:
    vpbroadcastd     m8, [pw_27_17_17_27]

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

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, src_bak, left_offxy, top_offxy, topleft_offxy

    lea  topleft_offxyq, [top_offxyq+32]
    lea     left_offxyq, [offyq+32]
    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 164
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq*2+0x10001*747+32*82]

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, src_bak, left_offxy, top_offxy, topleft_offxy

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
.loop_y_hv_overlap:
    ; grain = grain_lut[offy+y][offx+x]
    movu             m3, [grain_lutq+offxyq*2]
    movu             m0, [grain_lutq+offxyq*2+32]
    movu             m6, [grain_lutq+top_offxyq*2]
    movu             m1, [grain_lutq+top_offxyq*2+32]
    movd            xm4, [grain_lutq+left_offxyq*2]
    movd            xm7, [grain_lutq+topleft_offxyq*2]
    ; do h interpolation first (so top | top/left -> top, left | cur -> cur)
    punpcklwd       xm4, xm3
    punpcklwd       xm7, xm6
    REPX {pmaddwd x, xm15}, xm4, xm7
    REPX {paddd   x, xm14}, xm4, xm7
    REPX {psrad   x, 5}, xm4, xm7
    REPX {packssdw x, x}, xm4, xm7
    pcmpeqw          m5, m5
    psraw            m9, m10, 1             ; max_grain
    pxor             m5, m9                 ; min_grain
    REPX {pminsw x, xm9}, xm4, xm7
    REPX {pmaxsw x, xm5}, xm4, xm7
    vpblendd         m3, m4, 00000001b
    vpblendd         m6, m7, 00000001b
    ; followed by v interpolation (top | cur -> cur)
    punpckhwd        m7, m6, m3
    punpcklwd        m6, m3
    punpckhwd        m3, m1, m0
    punpcklwd        m1, m0
    REPX {pmaddwd x, m8}, m7, m6, m3, m1
    REPX {paddd   x, m14}, m7, m6, m3, m1
    REPX {psrad   x, 5}, m7, m6, m3, m1
    packssdw         m7, m6, m7
    packssdw         m3, m1, m3
    REPX {pminsw x, m9}, m7, m3
    REPX {pmaxsw x, m5}, m7, m3

    ; src
    pminuw           m0, m10, [srcq+ 0]
    pminuw           m1, m10, [srcq+32]          ; m0-1: src as word
    punpckhwd        m5, m0, m2
    punpcklwd        m4, m0, m2

    ; scaling[src]
    pcmpeqw          m9, m9
    vpgatherdd       m6, [scalingq+m4-3], m9
    pcmpeqw          m9, m9
    vpgatherdd       m4, [scalingq+m5-3], m9
    REPX  {psrld x, 24}, m6, m4
    packssdw         m6, m4

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m6, m11
    pmulhrsw         m7, m6

    ; other half
    punpckhwd        m5, m1, m2
    punpcklwd        m4, m1, m2             ; m4-7: src as dword

    ; scaling[src]
    pcmpeqw          m6, m6
    vpgatherdd       m9, [scalingq+m4-3], m6
    pcmpeqw          m6, m6
    vpgatherdd       m4, [scalingq+m5-3], m6
    REPX  {psrld x, 24}, m9, m4
    packssdw         m9, m4

    ; noise = round2(scaling[src] * grain, scaling_shift)
    pmullw           m9, m11
    pmulhrsw         m3, m9

    ; dst = clip_pixel(src, noise)
    paddw            m0, m7
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova [dstq+srcq+ 0], m0
    mova [dstq+srcq+32], m1

    vpbroadcastd     m8, [pw_27_17_17_27+4] ; swap weights for second v-overlap line
    add            srcq, strideq
    add      grain_lutq, 82*2
    dec              hw
    jz .end_y_hv_overlap
    ; 2 lines get vertical overlap, then fall back to non-overlap code for
    ; remaining (up to) 30 lines
    xor              hd, 0x10000
    test             hd, 0x10000
    jnz .loop_y_hv_overlap
    jmp .loop_y_h_overlap

.end_y_hv_overlap:
    add              wq, 32
    lea            srcq, [src_bakq+wq*2]
    jl .loop_x_hv_overlap

.end_hv:
    RET

cglobal fguv_32x32xn_i420_16bpc, 6, 15, 16, dst, src, stride, fg_data, w, scaling, \
                                      grain_lut, h, sby, luma, lstride, uv_pl, is_id
%define base r8-pb_mask
    lea              r8, [pb_mask]
    mov             r7d, [fg_dataq+FGData.scaling_shift]
    vpbroadcastw    m11, [base+mul_bits+r7*2-14]
    mov             r6d, [fg_dataq+FGData.clip_to_restricted_range]
    mov             r9d, r13m               ; bdmax
    sar             r9d, 11                 ; is_12bpc
    shlx           r10d, r6d, r9d
    vpbroadcastw    m13, [base+min+r10*2]
    lea            r10d, [r9d*3]
    mov            r11d, is_idm
    shlx            r6d, r6d, r11d
    add            r10d, r6d
    vpbroadcastw    m12, [base+max+r10*2]
    vpbroadcastw    m10, r13m
    pxor             m2, m2

    cmp byte [fg_dataq+FGData.chroma_scaling_from_luma], 0
    jne .csfl

%macro FGUV_32x32xN_LOOP 1 ; not-csfl
    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused, sby, see, overlap

%if %1
    mov             r7d, r11m
    vpbroadcastw     m0, [fg_dataq+FGData.uv_mult+r7*4]
    vpbroadcastw     m1, [fg_dataq+FGData.uv_luma_mult+r7*4]
    punpcklwd       m14, m1, m0
    vpbroadcastw    m15, [fg_dataq+FGData.uv_offset+r7*4]
    vpbroadcastd     m9, [base+pw_4+r9*4]
    pmullw          m15, m9
%else
    vpbroadcastd    m14, [pw_1024]
    vpbroadcastd    m15, [pw_23_22]
%endif

    movifnidn      sbyd, sbym
    test           sbyd, sbyd
    setnz           r7b
    test            r7b, byte [fg_dataq+FGData.overlap_flag]
    jnz %%vertical_overlap

    imul           seed, sbyd, (173 << 24) | 37
    add            seed, (105 << 24) | 178
    rol            seed, 8
    movzx          seed, seew
    xor            seed, [fg_dataq+FGData.seed]

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                unused2, unused3, see, unused4, unused5, unused6, luma, lstride

    mov           lumaq, r9mp
    mov        lstrideq, r10mp
    lea             r10, [srcq+wq*2]
    lea             r11, [dstq+wq*2]
    lea             r12, [lumaq+wq*4]
    mov           r10mp, r10
    mov           r11mp, r11
    mov           r12mp, r12
    neg              wq

%%loop_x:
    mov             r6d, seed
    or             seed, 0xEFF4
    shr             r6d, 1
    test           seeb, seeh
    lea            seed, [r6+0x8000]
    cmovp          seed, r6d               ; updated seed

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, unused1, unused2, unused3, luma, lstride

    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 82
    lea           offyq, [offyq+offxq+498]  ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, unused1, unused2, unused3, luma, lstride

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%%loop_y:
    ; src
    mova             m0, [srcq]
    mova             m1, [srcq+strideq]     ; m0-1: src as word

    ; luma_src
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm7, [lumaq+lstrideq*0+16]
    vinserti128      m4, [lumaq+lstrideq*0+32], 1
    vinserti128      m7, [lumaq+lstrideq*0+48], 1
    mova            xm6, [lumaq+lstrideq*2+ 0]
    mova            xm8, [lumaq+lstrideq*2+16]
    vinserti128      m6, [lumaq+lstrideq*2+32], 1
    vinserti128      m8, [lumaq+lstrideq*2+48], 1
    phaddw           m4, m7
    phaddw           m6, m8
    pavgw            m4, m2
    pavgw            m6, m2

%if %1
    punpckhwd        m3, m4, m0
    punpcklwd        m4, m0
    punpckhwd        m5, m6, m1
    punpcklwd        m6, m1                 ; { luma, chroma }
    REPX {pmaddwd x, m14}, m3, m4, m5, m6
    REPX {psrad   x, 6}, m3, m4, m5, m6
    packssdw         m4, m3
    packssdw         m6, m5
    REPX {paddw x, m15}, m4, m6
    REPX {pmaxsw x, m2}, m4, m6
    REPX {pminsw x, m10}, m4, m6             ; clip_pixel()
%else
    REPX {pminuw x, m10}, m4, m6
%endif

    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: luma_src as dword

    ; scaling[luma_src]
    pcmpeqw          m3, m3
    mova             m9, m3
    vpgatherdd       m8, [scalingq+m4-3], m3
    vpgatherdd       m4, [scalingq+m5-3], m9
    pcmpeqw          m3, m3
    mova             m9, m3
    vpgatherdd       m5, [scalingq+m6-3], m3
    vpgatherdd       m6, [scalingq+m7-3], m9
    REPX  {psrld x, 24}, m8, m4, m5, m6
    packssdw         m8, m4
    packssdw         m5, m6

    ; grain = grain_lut[offy+y][offx+x]
    movu             m9, [grain_lutq+offxyq*2]
    movu             m3, [grain_lutq+offxyq*2+82*2]

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    REPX {pmullw x, m11}, m8, m5
    pmulhrsw         m9, m8
    pmulhrsw         m3, m5

    ; dst = clip_pixel(src, noise)
    paddw            m0, m9
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova         [dstq], m0
    mova [dstq+strideq], m1

    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*4]
    add      grain_lutq, 82*4
    sub              hb, 2
    jg %%loop_y

    add              wq, 16
    jge %%end
    mov            srcq, r10mp
    mov            dstq, r11mp
    mov           lumaq, r12mp
    lea            srcq, [srcq+wq*2]
    lea            dstq, [dstq+wq*2]
    lea           lumaq, [lumaq+wq*4]
    cmp byte [fg_dataq+FGData.overlap_flag], 0
    je %%loop_x

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

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, left_offxy, unused1, unused2, luma, lstride

    lea     left_offxyd, [offyd+16]         ; previous column's offy*stride+offx
    mov           offxd, seed
    rorx          offyd, seed, 8
    shr           offxd, 12
    and           offyd, 0xf
    imul          offyd, 82
    lea           offyq, [offyq+offxq+498]  ; offy*stride+offx

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, left_offxy, unused1, unused2, luma, lstride

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%%loop_y_h_overlap:
    mova             m0, [srcq]
    mova             m1, [srcq+strideq]

    ; luma_src
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm7, [lumaq+lstrideq*0+16]
    vinserti128      m4, [lumaq+lstrideq*0+32], 1
    vinserti128      m7, [lumaq+lstrideq*0+48], 1
    mova            xm6, [lumaq+lstrideq*2+ 0]
    mova            xm8, [lumaq+lstrideq*2+16]
    vinserti128      m6, [lumaq+lstrideq*2+32], 1
    vinserti128      m8, [lumaq+lstrideq*2+48], 1
    phaddw           m4, m7
    phaddw           m6, m8
    pavgw            m4, m2
    pavgw            m6, m2

%if %1
    punpckhwd        m3, m4, m0
    punpcklwd        m4, m0
    punpckhwd        m5, m6, m1
    punpcklwd        m6, m1                 ; { luma, chroma }
    REPX {pmaddwd x, m14}, m3, m4, m5, m6
    REPX {psrad   x, 6}, m3, m4, m5, m6
    packssdw         m4, m3
    packssdw         m6, m5
    REPX {paddw x, m15}, m4, m6
    REPX {pmaxsw x, m2}, m4, m6
    REPX {pminsw x, m10}, m4, m6             ; clip_pixel()
%else
    REPX {pminuw x, m10}, m4, m6
%endif

    ; grain = grain_lut[offy+y][offx+x]
    movu             m9, [grain_lutq+offxyq*2]
    movu             m3, [grain_lutq+offxyq*2+82*2]
    movd            xm5, [grain_lutq+left_offxyq*2+ 0]
    pinsrw          xm5, [grain_lutq+left_offxyq*2+82*2], 1 ; {left0, left1}
    punpcklwd       xm7, xm9, xm3           ; {cur0, cur1}
    punpcklwd       xm5, xm7                ; {left0, cur0, left1, cur1}
%if %1
    pmaddwd         xm5, [pw_23_22]
%else
    pmaddwd         xm5, xm15
%endif
    vpbroadcastd    xm8, [pd_16]
    paddd           xm5, xm8
    psrad           xm5, 5
    packssdw        xm5, xm5
    pcmpeqw         xm8, xm8
    psraw           xm7, xm10, 1
    pxor            xm8, xm7
    pmaxsw          xm5, xm8
    pminsw          xm5, xm7
    vpblendw        xm7, xm5, xm9, 11111110b
    psrldq          xm5, 2
    vpblendw        xm5, xm3, 11111110b
    vpblendd         m9, m7, 00001111b
    vpblendd         m3, m5, 00001111b

    ; scaling[luma_src]
    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    pcmpeqw          m7, m7
    vpgatherdd       m8, [scalingq+m4-3], m7
    pcmpeqw          m7, m7
    vpgatherdd       m4, [scalingq+m5-3], m7
    REPX  {psrld x, 24}, m8, m4
    packssdw         m8, m4

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m8, m11
    pmulhrsw         m9, m8

    ; same for the other half
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: luma_src as dword
    pcmpeqw          m8, m8
    mova             m4, m8
    vpgatherdd       m5, [scalingq+m6-3], m8
    vpgatherdd       m6, [scalingq+m7-3], m4
    REPX  {psrld x, 24}, m5, m6
    packssdw         m5, m6

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m5, m11
    pmulhrsw         m3, m5

    ; dst = clip_pixel(src, noise)
    paddw            m0, m9
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova         [dstq], m0
    mova [dstq+strideq], m1

    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*4]
    add      grain_lutq, 82*4
    sub              hb, 2
    jg %%loop_y_h_overlap

    add              wq, 16
    jge %%end
    mov            srcq, r10mp
    mov            dstq, r11mp
    mov           lumaq, r12mp
    lea            srcq, [srcq+wq*2]
    lea            dstq, [dstq+wq*2]
    lea           lumaq, [lumaq+wq*4]

    ; r8m = sbym
    cmp       dword r8m, 0
    jne %%loop_x_hv_overlap
    jmp %%loop_x_h_overlap

%%end:
    RET

%%vertical_overlap:
    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, unused, \
                sby, see, unused1, unused2, unused3, lstride

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

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                unused1, unused2, see, unused3, unused4, unused5, luma, lstride

    mov           lumaq, r9mp
    mov        lstrideq, r10mp
    lea             r10, [srcq+wq*2]
    lea             r11, [dstq+wq*2]
    lea             r12, [lumaq+wq*4]
    mov           r10mp, r10
    mov           r11mp, r11
    mov           r12mp, r12
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

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, unused1, top_offxy, unused2, luma, lstride

    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 82
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq+0x10001*498+16*82]

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, unused1, top_offxy, unused2, luma, lstride

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%%loop_y_v_overlap:
    ; src
    mova             m0, [srcq]
    mova             m1, [srcq+strideq]

    ; luma_src
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm7, [lumaq+lstrideq*0+16]
    vinserti128      m4, [lumaq+lstrideq*0+32], 1
    vinserti128      m7, [lumaq+lstrideq*0+48], 1
    mova            xm6, [lumaq+lstrideq*2+ 0]
    mova            xm8, [lumaq+lstrideq*2+16]
    vinserti128      m6, [lumaq+lstrideq*2+32], 1
    vinserti128      m8, [lumaq+lstrideq*2+48], 1
    phaddw           m4, m7
    phaddw           m6, m8
    pavgw            m4, m2
    pavgw            m6, m2

%if %1
    punpckhwd        m3, m4, m0
    punpcklwd        m4, m0
    punpckhwd        m5, m6, m1
    punpcklwd        m6, m1                 ; { luma, chroma }
    REPX {pmaddwd x, m14}, m3, m4, m5, m6
    REPX {psrad   x, 6}, m3, m4, m5, m6
    packssdw         m4, m3
    packssdw         m6, m5
    REPX {paddw x, m15}, m4, m6
    REPX {pmaxsw x, m2}, m4, m6
    REPX {pminsw x, m10}, m4, m6             ; clip_pixel()
%else
    REPX {pminuw x, m10}, m4, m6
%endif

    ; grain = grain_lut[offy+y][offx+x]
    movu             m9, [grain_lutq+offxyq*2]
    movu             m5, [grain_lutq+top_offxyq*2]
    punpckhwd        m7, m5, m9
    punpcklwd        m5, m9                 ; {top/cur interleaved}
%if %1
    REPX {pmaddwd x, [pw_23_22]}, m7, m5
%else
    REPX {pmaddwd x, m15}, m7, m5
%endif
    vpbroadcastd     m3, [pd_16]
    REPX  {paddd x, m3}, m7, m5
    REPX   {psrad x, 5}, m7, m5
    packssdw         m9, m5, m7
    pcmpeqw          m7, m7
    psraw            m5, m10, 1
    pxor             m7, m5
    pmaxsw           m9, m7
    pminsw           m9, m5

    ; scaling[luma_src]
    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    pcmpeqw          m7, m7
    vpgatherdd       m8, [scalingq+m4-3], m7
    pcmpeqw          m7, m7
    vpgatherdd       m4, [scalingq+m5-3], m7
    REPX  {psrld x, 24}, m8, m4
    packssdw         m8, m4

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m8, m11
    pmulhrsw         m9, m8

    ; same for the other half
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: luma_src as dword
    pcmpeqw          m8, m8
    mova             m4, m8
    vpgatherdd       m5, [scalingq+m6-3], m8
    vpgatherdd       m6, [scalingq+m7-3], m4
    REPX  {psrld x, 24}, m5, m6
    packssdw         m5, m6

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    movu             m3, [grain_lutq+offxyq*2+82*2]
    pmullw           m5, m11
    pmulhrsw         m3, m5

    ; dst = clip_pixel(src, noise)
    paddw            m0, m9
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova         [dstq], m0
    mova [dstq+strideq], m1

    sub              hb, 2
    jle %%end_y_v_overlap
    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*4]
    add      grain_lutq, 82*4
    jmp %%loop_y

%%end_y_v_overlap:
    add              wq, 16
    jge %%end_hv
    mov            srcq, r10mp
    mov            dstq, r11mp
    mov           lumaq, r12mp
    lea            srcq, [srcq+wq*2]
    lea            dstq, [dstq+wq*2]
    lea           lumaq, [lumaq+wq*4]

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

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                offx, offy, see, left_offxy, top_offxy, topleft_offxy, luma, lstride

    lea  topleft_offxyq, [top_offxyq+16]
    lea     left_offxyq, [offyq+16]
    rorx          offyd, seed, 8
    rorx          offxd, seed, 12
    and           offyd, 0xf000f
    and           offxd, 0xf000f
    imul          offyd, 82
    ; offxy=offy*stride+offx, (cur_offxy << 16) | top_offxy
    lea           offyq, [offyq+offxq+0x10001*498+16*82]

    DEFINE_ARGS dst, src, stride, fg_data, w, scaling, grain_lut, \
                h, offxy, see, left_offxy, top_offxy, topleft_offxy, luma, lstride

    movzx    top_offxyd, offxyw
    shr          offxyd, 16

    mov              hd, hm
    mov      grain_lutq, grain_lutmp
%%loop_y_hv_overlap:
    ; grain = grain_lut[offy+y][offx+x]
    movd            xm5, [grain_lutq+left_offxyq*2]
    pinsrw          xm5, [grain_lutq+left_offxyq*2+82*2], 1
    pinsrw          xm5, [grain_lutq+topleft_offxyq*2], 2   ; { left0, left1, top/left }
    movu             m9, [grain_lutq+offxyq*2]
    movu             m3, [grain_lutq+offxyq*2+82*2]
    movu             m8, [grain_lutq+top_offxyq*2]
    punpcklwd       xm7, xm9, xm3           ; { cur0, cur1 }
    punpckldq       xm7, xm8                ; { cur0, cur1, top0 }
    punpcklwd       xm5, xm7                ; { cur/left } interleaved
    pmaddwd         xm5, [pw_23_22]
    vpbroadcastd    xm0, [pd_16]
    paddd           xm5, xm0
    psrad           xm5, 5
    packssdw        xm5, xm5
    pcmpeqw         xm0, xm0
    psraw           xm7, xm10, 1
    pxor            xm0, xm7
    pminsw          xm5, xm7
    pmaxsw          xm5, xm0
    pcmpeqw         xm7, xm7
    psrldq          xm7, 14                 ; 0xffff, 0.....
    vpblendvb        m9, m5, m7             ; line 0
    psrldq          xm5, 2
    vpblendvb        m3, m5, m7             ; line 1
    psrldq          xm5, 2
    vpblendvb        m5, m8, m5, m7         ; top line

    punpckhwd        m7, m5, m9
    punpcklwd        m5, m9                 ; {top/cur interleaved}
%if %1
    REPX {pmaddwd x, [pw_23_22]}, m7, m5
%else
    REPX {pmaddwd x, m15}, m7, m5
%endif
    vpbroadcastd     m9, [pd_16]
    REPX  {paddd x, m9}, m5, m7
    REPX   {psrad x, 5}, m5, m7
    packssdw         m9, m5, m7
    pcmpeqw          m5, m5
    psraw            m7, m10, 1
    pxor             m5, m7
    pmaxsw           m9, m5
    pminsw           m9, m7

    ; src
    mova             m0, [srcq]
    mova             m1, [srcq+strideq]

    ; luma_src
    mova            xm4, [lumaq+lstrideq*0+ 0]
    mova            xm7, [lumaq+lstrideq*0+16]
    vinserti128      m4, [lumaq+lstrideq*0+32], 1
    vinserti128      m7, [lumaq+lstrideq*0+48], 1
    mova            xm6, [lumaq+lstrideq*2+ 0]
    mova            xm8, [lumaq+lstrideq*2+16]
    vinserti128      m6, [lumaq+lstrideq*2+32], 1
    vinserti128      m8, [lumaq+lstrideq*2+48], 1
    phaddw           m4, m7
    phaddw           m6, m8
    pavgw            m4, m2
    pavgw            m6, m2

%if %1
    punpckhwd        m8, m4, m0
    punpcklwd        m4, m0
    punpckhwd        m5, m6, m1
    punpcklwd        m6, m1                 ; { luma, chroma }
    REPX {pmaddwd x, m14}, m8, m4, m5, m6
    REPX {psrad   x, 6}, m8, m4, m5, m6
    packssdw         m4, m8
    packssdw         m6, m5
    REPX {paddw x, m15}, m4, m6
    REPX {pmaxsw x, m2}, m4, m6
    REPX {pminsw x, m10}, m4, m6             ; clip_pixel()
%else
    REPX {pminuw x, m10}, m4, m6
%endif

    ; scaling[luma_src]
    punpckhwd        m5, m4, m2
    punpcklwd        m4, m2
    pcmpeqw          m7, m7
    vpgatherdd       m8, [scalingq+m4-3], m7
    pcmpeqw          m7, m7
    vpgatherdd       m4, [scalingq+m5-3], m7
    REPX  {psrld x, 24}, m8, m4
    packssdw         m8, m4

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m8, m11
    pmulhrsw         m9, m8

    ; same for the other half
    punpckhwd        m7, m6, m2
    punpcklwd        m6, m2                 ; m4-7: luma_src as dword
    pcmpeqw          m8, m8
    mova             m4, m8
    vpgatherdd       m5, [scalingq+m6-3], m8
    vpgatherdd       m6, [scalingq+m7-3], m4
    REPX  {psrld x, 24}, m5, m6
    packssdw         m5, m6

    ; noise = round2(scaling[luma_src] * grain, scaling_shift)
    pmullw           m5, m11
    pmulhrsw         m3, m5

    ; dst = clip_pixel(src, noise)
    paddw            m0, m9
    paddw            m1, m3
    pmaxsw           m0, m13
    pmaxsw           m1, m13
    pminsw           m0, m12
    pminsw           m1, m12
    mova         [dstq], m0
    mova [dstq+strideq], m1

    lea            srcq, [srcq+strideq*2]
    lea            dstq, [dstq+strideq*2]
    lea           lumaq, [lumaq+lstrideq*4]
    add      grain_lutq, 82*4
    sub              hb, 2
    jg %%loop_y_h_overlap

%%end_y_hv_overlap:
    add              wq, 16
    jge %%end_hv
    mov            srcq, r10mp
    mov            dstq, r11mp
    mov           lumaq, r12mp
    lea            srcq, [srcq+wq*2]
    lea            dstq, [dstq+wq*2]
    lea           lumaq, [lumaq+wq*4]
    jmp %%loop_x_hv_overlap

%%end_hv:
    RET
%endmacro

    FGUV_32x32xN_LOOP 1
.csfl:
    FGUV_32x32xN_LOOP 0

%endif ; ARCH_X86_64
