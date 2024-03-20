/*
 * Copyright © 2023, VideoLAN and dav1d authors
 * Copyright © 2023, Loongson Technology Corporation Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "loongson_asm.S"

const min_prob
  .short 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0
endconst

.macro decode_symbol_adapt w
    addi.d          sp,      sp,     -48
    addi.d          a4,      a0,      24
    vldrepl.h       vr0,     a4,      0    //rng
    fst.s           f0,      sp,      0    //val==0
    vld             vr1,     a1,      0    //cdf
.if \w == 16
    li.w            t4,      16
    vldx            vr11,    a1,      t4
.endif
    addi.d          a6,      a0,      16
    vldrepl.d       vr2,     a6,      0    //dif
    addi.d          t0,      a0,      32
    ld.w            t1,      t0,      0    //allow_update_cdf
    la.local        t2,      min_prob
    addi.d          t2,      t2,      32
    addi.w          t3,      a2,      1
    slli.w          t3,      t3,      1
    sub.d           t2,      t2,      t3
    vld             vr3,     t2,      0    //min_prob
.if \w == 16
    vldx            vr13,    t2,      t4
.endif
    vsrli.h         vr4,     vr0,     8    //r = s->rng >> 8
    vslli.h         vr4,     vr4,     8    //r << 8
    vsrli.h         vr5,     vr1,     6
    vslli.h         vr5,     vr5,     7
.if \w == 16
    vsrli.h         vr15,    vr11,    6
    vslli.h         vr15,    vr15,    7
.endif
    vmuh.hu         vr5,     vr4,     vr5
    vadd.h          vr5,     vr5,     vr3  //v
.if \w == 16
    vmuh.hu         vr15,    vr4,     vr15
    vadd.h          vr15,    vr15,    vr13
.endif
    addi.d          t8,      sp,      4
    vst             vr5,     t8,      0    //store v
.if \w == 16
    vstx            vr15,    t8,      t4
.endif
    vreplvei.h      vr20,    vr2,     3    //c
    vssub.hu        vr6,     vr5,     vr20 //c >=v
    vseqi.h         vr6,     vr6,     0
.if \w == 16
    vssub.hu        vr16,    vr15,    vr20 //c >=v
    vseqi.h         vr16,    vr16,    0
    vpickev.b       vr21,    vr16,    vr6
.endif
.if \w <= 8
    vmskltz.h       vr10,    vr6
.else
    vmskltz.b       vr10,    vr21
.endif
    beqz            t1,      .renorm\()\w

    // update_cdf
    alsl.d          t1,      a2,      a1,   1
    ld.h            t2,      t1,      0    //count
    srli.w          t3,      t2,      4    //count >> 4
    addi.w          t3,      t3,      4
    li.w            t5,      2
    sltu            t5,      t5,      a2
    add.w           t3,      t3,      t5   //rate
    sltui           t5,      t2,      32
    add.w           t2,      t2,      t5   //count + (count < 32)
    vreplgr2vr.h    vr9,     t3
    vseq.h          vr7,     vr7,     vr7
    vavgr.hu        vr5,     vr6,     vr7  //i >= val ? -1 : 32768
    vsub.h          vr5,     vr5,     vr1
    vsub.h          vr8,     vr1,     vr6
.if \w == 16
    vavgr.hu        vr15,    vr16,    vr7
    vsub.h          vr15,    vr15,    vr11
    vsub.h          vr18,    vr11,    vr16
.endif
    vsra.h          vr5,     vr5,     vr9
    vadd.h          vr8,     vr8,     vr5
.if \w == 4
    fst.d           f8,      a1,      0
.else
    vst             vr8,     a1,      0
.endif
.if \w == 16
    vsra.h          vr15,    vr15,    vr9
    vadd.h          vr18,    vr18,    vr15
    vstx            vr18,    a1,      t4
.endif
    st.h            t2,      t1,      0

.renorm\()\w:
    vpickve2gr.h    t3,      vr10,    0
    ctz.w           a7,      t3            // ret
    alsl.d          t3,      a7,      t8,      1
    ld.hu           t4,      t3,      0    // v
    addi.d          t3,      t3,      -2
    ld.hu           t5,      t3,      0    // u
    sub.w           t5,      t5,      t4   // rng
    slli.d          t4,      t4,      48
    vpickve2gr.d    t6,      vr2,     0
    sub.d           t6,      t6,      t4   // dif
    addi.d          t6,      t6,      1
    clz.w           t4,      t5            // d
    xori            t4,      t4,      16   // d
    sll.d           t6,      t6,      t4
    addi.d          t6,      t6,      -1   // dif
    addi.d          a5,      a0,      28   // cnt
    ld.w            t7,      a5,      0
    sub.w           t7,      t7,      t4   // cnt-d
    sll.w           t5,      t5,      t4
    st.w            t5,      a4,      0    // store rng
    bge             t7,      zero,    9f

    // refill
    ld.d            t0,      a0,      0    // buf_pos
    addi.d          t1,      a0,      8
    ld.d            t1,      t1,      0    // buf_end
    addi.d          t2,      t0,      8
    blt             t1,      t2,      1f

    ld.d            t0,      t0,      0    // next_bits
    addi.w          t3,      t7,      23   // shift_bits = cnt + 23
    addi.w          t7,      t7,      16   // cnt += 16
    revb.d          t0,      t0            // next_bits = bswap(next_bits)
    srli.w          t4,      t3,      3
    sub.d           t2,      t2,      t4   // buf_pos -= shift_bits >> 3
    st.d            t2,      a0,      0
    andi            t3,      t3,      24   // shift_bits &= 24
    srl.d           t0,      t0,      t3   // next_bits >>= shift_bits
    sub.w           t3,      t3,      t7   // shift_bits -= 16 + cnt
    sll.d           t0,      t0,      t3   // next_bits <<= shift_bits
    li.w            t5,      48
    sub.w           t7,      t5,      t3   // cnt = cnt + 64 - shift_bits
    xor             t6,      t6,      t0   // dif ^= next_bits
    b               9f
1:
    li.w            t4,      40
    sub.w           t5,      t4,      t7   // c = 40 - cnt
2:
    bge             t0,      t1,      3f
    ld.bu           t2,      t0,      0
    addi.d          t0,      t0,      1
    sll.d           t2,      t2,      t5
    xor             t6,      t6,      t2
    addi.w          t5,      t5,      -8
    bge             t5,      zero,    2b
    // refill_eob_end
3:
    st.d            t0,      a0,      0    // s->buf_pos = buf_pos
    sub.w           t7,      t4,      t5   // cnt = 40 - c
9:
    st.w            t7,      a5,      0    // store cnt
    st.d            t6,      a6,      0    // store dif
    move            a0,      a7
    addi.d          sp,      sp,      48
.endm

function msac_decode_symbol_adapt4_lsx
    decode_symbol_adapt 4
endfunc

function msac_decode_symbol_adapt8_lsx
    decode_symbol_adapt 8
endfunc

function msac_decode_symbol_adapt16_lsx
    decode_symbol_adapt 16
endfunc

function msac_decode_bool_lsx
    ld.w            t0,      a0,      24   // rng
    srli.w          a1,      a1,      6
    ld.d            t1,      a0,      16   // dif
    srli.w          t2,      t0,      8    // r >> 8
    mul.w           t2,      t2,      a1
    ld.w            a5,      a0,      28   // cnt
    addi.d          t1,      t1,      1    // dif + 1
    srli.w          t2,      t2,      1
    addi.w          t2,      t2,      4    // v
    slli.d          t3,      t2,      48   // vw
    sltu            t4,      t1,      t3
    move            t8,      t4            // ret
    xori            t4,      t4,      1
    maskeqz         t6,      t3,      t4   // if (ret) vw
    sub.d           t6,      t1,      t6   // dif
    slli.w          t5,      t2,      1
    sub.w           t5,      t0,      t5   // r - 2v
    maskeqz         t7,      t5,      t4   // if (ret) r - 2v
    add.w           t5,      t2,      t7   // v(rng)

    // renorm
    clz.w           t4,      t5            // d
    xori            t4,      t4,      16   // d
    sll.d           t6,      t6,      t4
    addi.d          t6,      t6,      -1   // dif
    sub.w           t7,      a5,      t4   // cnt-d
    sll.w           t5,      t5,      t4
    st.w            t5,      a0,      24   // store rng
    bge             t7,      zero,    9f

    // refill
    ld.d            t0,      a0,      0    // buf_pos
    addi.d          t1,      a0,      8
    ld.d            t1,      t1,      0    // buf_end
    addi.d          t2,      t0,      8
    blt             t1,      t2,      1f

    ld.d            t0,      t0,      0    // next_bits
    addi.w          t3,      t7,      23   // shift_bits = cnt + 23
    addi.w          t7,      t7,      16   // cnt += 16
    revb.d          t0,      t0            // next_bits = bswap(next_bits)
    srli.w          t4,      t3,      3
    sub.d           t2,      t2,      t4   // buf_pos -= shift_bits >> 3
    st.d            t2,      a0,      0
    andi            t3,      t3,      24   // shift_bits &= 24
    srl.d           t0,      t0,      t3   // next_bits >>= shift_bits
    sub.w           t3,      t3,      t7   // shift_bits -= 16 + cnt
    sll.d           t0,      t0,      t3   // next_bits <<= shift_bits
    li.w            t5,      48
    sub.w           t7,      t5,      t3   // cnt = cnt + 64 - shift_bits
    xor             t6,      t6,      t0   // dif ^= next_bits
    b               9f
1:
    li.w            t4,      40
    sub.w           t5,      t4,      t7   // c = 40 - cnt
2:
    bge             t0,      t1,      3f
    ld.bu           t2,      t0,      0
    addi.d          t0,      t0,      1
    sll.d           t2,      t2,      t5
    xor             t6,      t6,      t2
    addi.w          t5,      t5,      -8
    bge             t5,      zero,    2b
    // refill_eob_end
3:
    st.d            t0,      a0,      0    // s->buf_pos = buf_pos
    sub.w           t7,      t4,      t5   // cnt = 40 - c
9:
    st.w            t7,      a0,      28   // store cnt
    st.d            t6,      a0,      16   // store dif
    move            a0,      t8
endfunc

function msac_decode_bool_adapt_lsx
    ld.hu           a3,      a1,      0    // cdf[0] /f
    ld.w            t0,      a0,      24   // rng
    ld.d            t1,      a0,      16   // dif
    srli.w          t2,      t0,      8    // r >> 8
    srli.w          a7,      a3,      6
    mul.w           t2,      t2,      a7
    ld.w            a4,      a0,      32   // allow_update_cdf
    ld.w            a5,      a0,      28   // cnt
    srli.w          t2,      t2,      1
    addi.w          t2,      t2,      4    // v
    slli.d          t3,      t2,      48   // vw
    sltu            t4,      t1,      t3
    move            t8,      t4            // bit
    xori            t4,      t4,      1
    maskeqz         t6,      t3,      t4   // if (ret) vw
    sub.d           t6,      t1,      t6   // dif
    slli.w          t5,      t2,      1
    sub.w           t5,      t0,      t5   // r - 2v
    maskeqz         t7,      t5,      t4   // if (ret) r - 2v
    add.w           t5,      t2,      t7   // v(rng)
    beqz            a4,      .renorm

    // update_cdf
    ld.hu           t0,      a1,      2    // cdf[1]
    srli.w          t1,      t0,      4
    addi.w          t1,      t1,      4    // rate
    sltui           t2,      t0,      32   // count < 32
    add.w           t0,      t0,      t2   // count + (count < 32)
    sub.w           a3,      a3,      t8   // cdf[0] -= bit
    slli.w          t4,      t8,      15
    sub.w           t7,      a3,      t4   // cdf[0] - bit - 32768
    sra.w           t7,      t7,      t1   // (cdf[0] - bit - 32768) >> rate
    sub.w           t7,      a3,      t7   // cdf[0]
    st.h            t7,      a1,      0
    st.h            t0,      a1,      2

.renorm:
    // renorm
    addi.d          t6,      t6,      1
    clz.w           t4,      t5            // d
    xori            t4,      t4,      16   // d
    sll.d           t6,      t6,      t4
    addi.d          t6,      t6,      -1   // dif
    sub.w           t7,      a5,      t4   // cnt-d
    sll.w           t5,      t5,      t4
    st.w            t5,      a0,      24   // store rng
    bge             t7,      zero,    9f

    // refill
    ld.d            t0,      a0,      0    // buf_pos
    addi.d          t1,      a0,      8
    ld.d            t1,      t1,      0    // buf_end
    addi.d          t2,      t0,      8
    blt             t1,      t2,      1f

    ld.d            t0,      t0,      0    // next_bits
    addi.w          t3,      t7,      23   // shift_bits = cnt + 23
    addi.w          t7,      t7,      16   // cnt += 16
    revb.d          t0,      t0            // next_bits = bswap(next_bits)
    srli.w          t4,      t3,      3
    sub.d           t2,      t2,      t4   // buf_pos -= shift_bits >> 3
    st.d            t2,      a0,      0
    andi            t3,      t3,      24   // shift_bits &= 24
    srl.d           t0,      t0,      t3   // next_bits >>= shift_bits
    sub.w           t3,      t3,      t7   // shift_bits -= 16 + cnt
    sll.d           t0,      t0,      t3   // next_bits <<= shift_bits
    li.w            t5,      48
    sub.w           t7,      t5,      t3   // cnt = cnt + 64 - shift_bits
    xor             t6,      t6,      t0   // dif ^= next_bits
    b               9f
1:
    li.w            t4,      40
    sub.w           t5,      t4,      t7   // c = 40 - cnt
2:
    bge             t0,      t1,      3f
    ld.bu           t2,      t0,      0
    addi.d          t0,      t0,      1
    sll.d           t2,      t2,      t5
    xor             t6,      t6,      t2
    addi.w          t5,      t5,      -8
    bge             t5,      zero,    2b
    // refill_eob_end
3:
    st.d            t0,      a0,      0    // s->buf_pos = buf_pos
    sub.w           t7,      t4,      t5   // cnt = 40 - c
9:
    st.w            t7,      a0,      28   // store cnt
    st.d            t6,      a0,      16   // store dif
    move            a0,      t8
endfunc
