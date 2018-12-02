/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
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

#include "config.h"

#include <stddef.h>
#include <stdint.h>

#include "common/attributes.h"

#define CLIP(a) iclip(a, min, max)

static void NOINLINE
inv_dct4_1d(const coef *const in, const ptrdiff_t in_s,
            coef *const out, const ptrdiff_t out_s, const int max)
{
    const int min = -max - 1;
    const int in0 = in[0 * in_s], in1 = in[1 * in_s];
    const int in2 = in[2 * in_s], in3 = in[3 * in_s];

    int t0 = ((in0 + in2) * 2896 + 2048) >> 12;
    int t1 = ((in0 - in2) * 2896 + 2048) >> 12;
    int t2 = (in1 * 1567 - in3 * 3784 + 2048) >> 12;
    int t3 = (in1 * 3784 + in3 * 1567 + 2048) >> 12;

    out[0 * out_s] = CLIP(t0 + t3);
    out[1 * out_s] = CLIP(t1 + t2);
    out[2 * out_s] = CLIP(t1 - t2);
    out[3 * out_s] = CLIP(t0 - t3);
}

static void NOINLINE
inv_dct8_1d(const coef *const in, const ptrdiff_t in_s,
            coef *const out, const ptrdiff_t out_s, const int max)
{
    const int min = -max - 1;
    coef tmp[4];

    inv_dct4_1d(in, in_s * 2, tmp, 1, max);

    const int in1 = in[1 * in_s], in3 = in[3 * in_s];
    const int in5 = in[5 * in_s], in7 = in[7 * in_s];

    int t4a = (in1 *  799 - in7 * 4017 + 2048) >> 12;
    int t5a = (in5 * 3406 - in3 * 2276 + 2048) >> 12;
    int t6a = (in5 * 2276 + in3 * 3406 + 2048) >> 12;
    int t7a = (in1 * 4017 + in7 *  799 + 2048) >> 12;

    int t4  = CLIP(t4a + t5a);
        t5a = CLIP(t4a - t5a);
    int t7  = CLIP(t7a + t6a);
        t6a = CLIP(t7a - t6a);

    int t5  = ((t6a - t5a) * 2896 + 2048) >> 12;
    int t6  = ((t6a + t5a) * 2896 + 2048) >> 12;

    out[0 * out_s] = CLIP(tmp[0] + t7);
    out[1 * out_s] = CLIP(tmp[1] + t6);
    out[2 * out_s] = CLIP(tmp[2] + t5);
    out[3 * out_s] = CLIP(tmp[3] + t4);
    out[4 * out_s] = CLIP(tmp[3] - t4);
    out[5 * out_s] = CLIP(tmp[2] - t5);
    out[6 * out_s] = CLIP(tmp[1] - t6);
    out[7 * out_s] = CLIP(tmp[0] - t7);
}

static void NOINLINE
inv_dct16_1d(const coef *const in, const ptrdiff_t in_s,
             coef *const out, const ptrdiff_t out_s, const int max)
{
    const int min = -max - 1;
    coef tmp[8];

    inv_dct8_1d(in, in_s * 2, tmp, 1, max);

    const int in1  = in[ 1 * in_s], in3  = in[ 3 * in_s];
    const int in5  = in[ 5 * in_s], in7  = in[ 7 * in_s];
    const int in9  = in[ 9 * in_s], in11 = in[11 * in_s];
    const int in13 = in[13 * in_s], in15 = in[15 * in_s];

    int t8a  = (in1  *  401 - in15 * 4076 + 2048) >> 12;
    int t15a = (in1  * 4076 + in15 *  401 + 2048) >> 12;
    int t9a  = (in9  * 3166 - in7  * 2598 + 2048) >> 12;
    int t14a = (in9  * 2598 + in7  * 3166 + 2048) >> 12;
    int t10a = (in5  * 1931 - in11 * 3612 + 2048) >> 12;
    int t13a = (in5  * 3612 + in11 * 1931 + 2048) >> 12;
    int t11a = (in13 * 3920 - in3  * 1189 + 2048) >> 12;
    int t12a = (in13 * 1189 + in3  * 3920 + 2048) >> 12;

    int t8  = CLIP(t8a  + t9a);
    int t9  = CLIP(t8a  - t9a);
    int t10 = CLIP(t11a - t10a);
    int t11 = CLIP(t11a + t10a);
    int t12 = CLIP(t12a + t13a);
    int t13 = CLIP(t12a - t13a);
    int t14 = CLIP(t15a - t14a);
    int t15 = CLIP(t15a + t14a);

    t9a  = (  t14 * 1567 - t9  * 3784  + 2048) >> 12;
    t14a = (  t14 * 3784 + t9  * 1567  + 2048) >> 12;
    t10a = (-(t13 * 3784 + t10 * 1567) + 2048) >> 12;
    t13a = (  t13 * 1567 - t10 * 3784  + 2048) >> 12;

    t8a  = CLIP(t8   + t11);
    t9   = CLIP(t9a  + t10a);
    t10  = CLIP(t9a  - t10a);
    t11a = CLIP(t8   - t11);
    t12a = CLIP(t15  - t12);
    t13  = CLIP(t14a - t13a);
    t14  = CLIP(t14a + t13a);
    t15a = CLIP(t15  + t12);

    t10a = ((t13  - t10)  * 2896 + 2048) >> 12;
    t13a = ((t13  + t10)  * 2896 + 2048) >> 12;
    t11  = ((t12a - t11a) * 2896 + 2048) >> 12;
    t12  = ((t12a + t11a) * 2896 + 2048) >> 12;

    out[ 0 * out_s] = CLIP(tmp[0] + t15a);
    out[ 1 * out_s] = CLIP(tmp[1] + t14);
    out[ 2 * out_s] = CLIP(tmp[2] + t13a);
    out[ 3 * out_s] = CLIP(tmp[3] + t12);
    out[ 4 * out_s] = CLIP(tmp[4] + t11);
    out[ 5 * out_s] = CLIP(tmp[5] + t10a);
    out[ 6 * out_s] = CLIP(tmp[6] + t9);
    out[ 7 * out_s] = CLIP(tmp[7] + t8a);
    out[ 8 * out_s] = CLIP(tmp[7] - t8a);
    out[ 9 * out_s] = CLIP(tmp[6] - t9);
    out[10 * out_s] = CLIP(tmp[5] - t10a);
    out[11 * out_s] = CLIP(tmp[4] - t11);
    out[12 * out_s] = CLIP(tmp[3] - t12);
    out[13 * out_s] = CLIP(tmp[2] - t13a);
    out[14 * out_s] = CLIP(tmp[1] - t14);
    out[15 * out_s] = CLIP(tmp[0] - t15a);
}

static void NOINLINE
inv_dct32_1d(const coef *const in, const ptrdiff_t in_s,
             coef *const out, const ptrdiff_t out_s, const int max)
{
    const int min = -max - 1;
    coef tmp[16];

    inv_dct16_1d(in, in_s * 2, tmp, 1, max);

    const int in1  = in[ 1 * in_s], in3  = in[ 3 * in_s];
    const int in5  = in[ 5 * in_s], in7  = in[ 7 * in_s];
    const int in9  = in[ 9 * in_s], in11 = in[11 * in_s];
    const int in13 = in[13 * in_s], in15 = in[15 * in_s];
    const int in17 = in[17 * in_s], in19 = in[19 * in_s];
    const int in21 = in[21 * in_s], in23 = in[23 * in_s];
    const int in25 = in[25 * in_s], in27 = in[27 * in_s];
    const int in29 = in[29 * in_s], in31 = in[31 * in_s];

    int t16a = (in1  *  201 - in31 * 4091 + 2048) >> 12;
    int t31a = (in1  * 4091 + in31 *  201 + 2048) >> 12;
    int t17a = (in17 * 3035 - in15 * 2751 + 2048) >> 12;
    int t30a = (in17 * 2751 + in15 * 3035 + 2048) >> 12;
    int t18a = (in9  * 1751 - in23 * 3703 + 2048) >> 12;
    int t29a = (in9  * 3703 + in23 * 1751 + 2048) >> 12;
    int t19a = (in25 * 3857 - in7  * 1380 + 2048) >> 12;
    int t28a = (in25 * 1380 + in7  * 3857 + 2048) >> 12;
    int t20a = (in5  *  995 - in27 * 3973 + 2048) >> 12;
    int t27a = (in5  * 3973 + in27 *  995 + 2048) >> 12;
    int t21a = (in21 * 3513 - in11 * 2106 + 2048) >> 12;
    int t26a = (in21 * 2106 + in11 * 3513 + 2048) >> 12;
    int t22a = (in13 * 2440 - in19 * 3290 + 2048) >> 12;
    int t25a = (in13 * 3290 + in19 * 2440 + 2048) >> 12;
    int t23a = (in29 * 4052 - in3  *  601 + 2048) >> 12;
    int t24a = (in29 *  601 + in3  * 4052 + 2048) >> 12;

    int t16 = CLIP(t16a + t17a);
    int t17 = CLIP(t16a - t17a);
    int t18 = CLIP(t19a - t18a);
    int t19 = CLIP(t19a + t18a);
    int t20 = CLIP(t20a + t21a);
    int t21 = CLIP(t20a - t21a);
    int t22 = CLIP(t23a - t22a);
    int t23 = CLIP(t23a + t22a);
    int t24 = CLIP(t24a + t25a);
    int t25 = CLIP(t24a - t25a);
    int t26 = CLIP(t27a - t26a);
    int t27 = CLIP(t27a + t26a);
    int t28 = CLIP(t28a + t29a);
    int t29 = CLIP(t28a - t29a);
    int t30 = CLIP(t31a - t30a);
    int t31 = CLIP(t31a + t30a);

    t17a = (  t30 *  799 - t17 * 4017  + 2048) >> 12;
    t30a = (  t30 * 4017 + t17 *  799  + 2048) >> 12;
    t18a = (-(t29 * 4017 + t18 *  799) + 2048) >> 12;
    t29a = (  t29 *  799 - t18 * 4017  + 2048) >> 12;
    t21a = (  t26 * 3406 - t21 * 2276  + 2048) >> 12;
    t26a = (  t26 * 2276 + t21 * 3406  + 2048) >> 12;
    t22a = (-(t25 * 2276 + t22 * 3406) + 2048) >> 12;
    t25a = (  t25 * 3406 - t22 * 2276  + 2048) >> 12;

    t16a = CLIP(t16  + t19);
    t17  = CLIP(t17a + t18a);
    t18  = CLIP(t17a - t18a);
    t19a = CLIP(t16  - t19);
    t20a = CLIP(t23  - t20);
    t21  = CLIP(t22a - t21a);
    t22  = CLIP(t22a + t21a);
    t23a = CLIP(t23  + t20);
    t24a = CLIP(t24  + t27);
    t25  = CLIP(t25a + t26a);
    t26  = CLIP(t25a - t26a);
    t27a = CLIP(t24  - t27);
    t28a = CLIP(t31  - t28);
    t29  = CLIP(t30a - t29a);
    t30  = CLIP(t30a + t29a);
    t31a = CLIP(t31  + t28);

    t18a = (  t29  * 1567 - t18  * 3784  + 2048) >> 12;
    t29a = (  t29  * 3784 + t18  * 1567  + 2048) >> 12;
    t19  = (  t28a * 1567 - t19a * 3784  + 2048) >> 12;
    t28  = (  t28a * 3784 + t19a * 1567  + 2048) >> 12;
    t20  = (-(t27a * 3784 + t20a * 1567) + 2048) >> 12;
    t27  = (  t27a * 1567 - t20a * 3784  + 2048) >> 12;
    t21a = (-(t26  * 3784 + t21  * 1567) + 2048) >> 12;
    t26a = (  t26  * 1567 - t21  * 3784  + 2048) >> 12;

    t16  = CLIP(t16a + t23a);
    t17a = CLIP(t17  + t22);
    t18  = CLIP(t18a + t21a);
    t19a = CLIP(t19  + t20);
    t20a = CLIP(t19  - t20);
    t21  = CLIP(t18a - t21a);
    t22a = CLIP(t17  - t22);
    t23  = CLIP(t16a - t23a);
    t24  = CLIP(t31a - t24a);
    t25a = CLIP(t30  - t25);
    t26  = CLIP(t29a - t26a);
    t27a = CLIP(t28  - t27);
    t28a = CLIP(t28  + t27);
    t29  = CLIP(t29a + t26a);
    t30a = CLIP(t30  + t25);
    t31  = CLIP(t31a + t24a);

    t20  = ((t27a - t20a) * 2896 + 2048) >> 12;
    t27  = ((t27a + t20a) * 2896 + 2048) >> 12;
    t21a = ((t26  - t21 ) * 2896 + 2048) >> 12;
    t26a = ((t26  + t21 ) * 2896 + 2048) >> 12;
    t22  = ((t25a - t22a) * 2896 + 2048) >> 12;
    t25  = ((t25a + t22a) * 2896 + 2048) >> 12;
    t23a = ((t24  - t23 ) * 2896 + 2048) >> 12;
    t24a = ((t24  + t23 ) * 2896 + 2048) >> 12;

    out[ 0 * out_s] = CLIP(tmp[ 0] + t31);
    out[ 1 * out_s] = CLIP(tmp[ 1] + t30a);
    out[ 2 * out_s] = CLIP(tmp[ 2] + t29);
    out[ 3 * out_s] = CLIP(tmp[ 3] + t28a);
    out[ 4 * out_s] = CLIP(tmp[ 4] + t27);
    out[ 5 * out_s] = CLIP(tmp[ 5] + t26a);
    out[ 6 * out_s] = CLIP(tmp[ 6] + t25);
    out[ 7 * out_s] = CLIP(tmp[ 7] + t24a);
    out[ 8 * out_s] = CLIP(tmp[ 8] + t23a);
    out[ 9 * out_s] = CLIP(tmp[ 9] + t22);
    out[10 * out_s] = CLIP(tmp[10] + t21a);
    out[11 * out_s] = CLIP(tmp[11] + t20);
    out[12 * out_s] = CLIP(tmp[12] + t19a);
    out[13 * out_s] = CLIP(tmp[13] + t18);
    out[14 * out_s] = CLIP(tmp[14] + t17a);
    out[15 * out_s] = CLIP(tmp[15] + t16);
    out[16 * out_s] = CLIP(tmp[15] - t16);
    out[17 * out_s] = CLIP(tmp[14] - t17a);
    out[18 * out_s] = CLIP(tmp[13] - t18);
    out[19 * out_s] = CLIP(tmp[12] - t19a);
    out[20 * out_s] = CLIP(tmp[11] - t20);
    out[21 * out_s] = CLIP(tmp[10] - t21a);
    out[22 * out_s] = CLIP(tmp[ 9] - t22);
    out[23 * out_s] = CLIP(tmp[ 8] - t23a);
    out[24 * out_s] = CLIP(tmp[ 7] - t24a);
    out[25 * out_s] = CLIP(tmp[ 6] - t25);
    out[26 * out_s] = CLIP(tmp[ 5] - t26a);
    out[27 * out_s] = CLIP(tmp[ 4] - t27);
    out[28 * out_s] = CLIP(tmp[ 3] - t28a);
    out[29 * out_s] = CLIP(tmp[ 2] - t29);
    out[30 * out_s] = CLIP(tmp[ 1] - t30a);
    out[31 * out_s] = CLIP(tmp[ 0] - t31);
}

static void NOINLINE
inv_dct64_1d(const coef *const in, const ptrdiff_t in_s,
             coef *const out, const ptrdiff_t out_s, const int max)
{
    const int min = -max - 1;
    coef tmp[32];

    inv_dct32_1d(in, in_s * 2, tmp, 1, max);

    const int in1  = in[ 1 * in_s], in3  = in[ 3 * in_s];
    const int in5  = in[ 5 * in_s], in7  = in[ 7 * in_s];
    const int in9  = in[ 9 * in_s], in11 = in[11 * in_s];
    const int in13 = in[13 * in_s], in15 = in[15 * in_s];
    const int in17 = in[17 * in_s], in19 = in[19 * in_s];
    const int in21 = in[21 * in_s], in23 = in[23 * in_s];
    const int in25 = in[25 * in_s], in27 = in[27 * in_s];
    const int in29 = in[29 * in_s], in31 = in[31 * in_s];
    const int in33 = in[33 * in_s], in35 = in[35 * in_s];
    const int in37 = in[37 * in_s], in39 = in[39 * in_s];
    const int in41 = in[41 * in_s], in43 = in[43 * in_s];
    const int in45 = in[45 * in_s], in47 = in[47 * in_s];
    const int in49 = in[49 * in_s], in51 = in[51 * in_s];
    const int in53 = in[53 * in_s], in55 = in[55 * in_s];
    const int in57 = in[57 * in_s], in59 = in[59 * in_s];
    const int in61 = in[61 * in_s], in63 = in[63 * in_s];

    int t32a = (in1  *  101 - in63 * 4095 + 2048) >> 12;
    int t33a = (in33 * 2967 - in31 * 2824 + 2048) >> 12;
    int t34a = (in17 * 1660 - in47 * 3745 + 2048) >> 12;
    int t35a = (in49 * 3822 - in15 * 1474 + 2048) >> 12;
    int t36a = (in9  *  897 - in55 * 3996 + 2048) >> 12;
    int t37a = (in41 * 3461 - in23 * 2191 + 2048) >> 12;
    int t38a = (in25 * 2359 - in39 * 3349 + 2048) >> 12;
    int t39a = (in57 * 4036 - in7  *  700 + 2048) >> 12;
    int t40a = (in5  *  501 - in59 * 4065 + 2048) >> 12;
    int t41a = (in37 * 3229 - in27 * 2520 + 2048) >> 12;
    int t42a = (in21 * 2019 - in43 * 3564 + 2048) >> 12;
    int t43a = (in53 * 3948 - in11 * 1092 + 2048) >> 12;
    int t44a = (in13 * 1285 - in51 * 3889 + 2048) >> 12;
    int t45a = (in45 * 3659 - in19 * 1842 + 2048) >> 12;
    int t46a = (in29 * 2675 - in35 * 3102 + 2048) >> 12;
    int t47a = (in61 * 4085 - in3  *  301 + 2048) >> 12;
    int t48a = (in61 *  301 + in3  * 4085 + 2048) >> 12;
    int t49a = (in29 * 3102 + in35 * 2675 + 2048) >> 12;
    int t50a = (in45 * 1842 + in19 * 3659 + 2048) >> 12;
    int t51a = (in13 * 3889 + in51 * 1285 + 2048) >> 12;
    int t52a = (in53 * 1092 + in11 * 3948 + 2048) >> 12;
    int t53a = (in21 * 3564 + in43 * 2019 + 2048) >> 12;
    int t54a = (in37 * 2520 + in27 * 3229 + 2048) >> 12;
    int t55a = (in5  * 4065 + in59 *  501 + 2048) >> 12;
    int t56a = (in57 *  700 + in7  * 4036 + 2048) >> 12;
    int t57a = (in25 * 3349 + in39 * 2359 + 2048) >> 12;
    int t58a = (in41 * 2191 + in23 * 3461 + 2048) >> 12;
    int t59a = (in9  * 3996 + in55 *  897 + 2048) >> 12;
    int t60a = (in49 * 1474 + in15 * 3822 + 2048) >> 12;
    int t61a = (in17 * 3745 + in47 * 1660 + 2048) >> 12;
    int t62a = (in33 * 2824 + in31 * 2967 + 2048) >> 12;
    int t63a = (in1  * 4095 + in63 *  101 + 2048) >> 12;

    int t32 = CLIP(t32a + t33a);
    int t33 = CLIP(t32a - t33a);
    int t34 = CLIP(t35a - t34a);
    int t35 = CLIP(t35a + t34a);
    int t36 = CLIP(t36a + t37a);
    int t37 = CLIP(t36a - t37a);
    int t38 = CLIP(t39a - t38a);
    int t39 = CLIP(t39a + t38a);
    int t40 = CLIP(t40a + t41a);
    int t41 = CLIP(t40a - t41a);
    int t42 = CLIP(t43a - t42a);
    int t43 = CLIP(t43a + t42a);
    int t44 = CLIP(t44a + t45a);
    int t45 = CLIP(t44a - t45a);
    int t46 = CLIP(t47a - t46a);
    int t47 = CLIP(t47a + t46a);
    int t48 = CLIP(t48a + t49a);
    int t49 = CLIP(t48a - t49a);
    int t50 = CLIP(t51a - t50a);
    int t51 = CLIP(t51a + t50a);
    int t52 = CLIP(t52a + t53a);
    int t53 = CLIP(t52a - t53a);
    int t54 = CLIP(t55a - t54a);
    int t55 = CLIP(t55a + t54a);
    int t56 = CLIP(t56a + t57a);
    int t57 = CLIP(t56a - t57a);
    int t58 = CLIP(t59a - t58a);
    int t59 = CLIP(t59a + t58a);
    int t60 = CLIP(t60a + t61a);
    int t61 = CLIP(t60a - t61a);
    int t62 = CLIP(t63a - t62a);
    int t63 = CLIP(t63a + t62a);

    t33a = (t33 * -4076 + t62 *   401 + 2048) >> 12;
    t34a = (t34 * - 401 + t61 * -4076 + 2048) >> 12;
    t37a = (t37 * -2598 + t58 *  3166 + 2048) >> 12;
    t38a = (t38 * -3166 + t57 * -2598 + 2048) >> 12;
    t41a = (t41 * -3612 + t54 *  1931 + 2048) >> 12;
    t42a = (t42 * -1931 + t53 * -3612 + 2048) >> 12;
    t45a = (t45 * -1189 + t50 *  3920 + 2048) >> 12;
    t46a = (t46 * -3920 + t49 * -1189 + 2048) >> 12;
    t49a = (t46 * -1189 + t49 *  3920 + 2048) >> 12;
    t50a = (t45 *  3920 + t50 *  1189 + 2048) >> 12;
    t53a = (t42 * -3612 + t53 *  1931 + 2048) >> 12;
    t54a = (t41 *  1931 + t54 *  3612 + 2048) >> 12;
    t57a = (t38 * -2598 + t57 *  3166 + 2048) >> 12;
    t58a = (t37 *  3166 + t58 *  2598 + 2048) >> 12;
    t61a = (t34 * -4076 + t61 *   401 + 2048) >> 12;
    t62a = (t33 *   401 + t62 *  4076 + 2048) >> 12;

    t32a = CLIP(t32  + t35);
    t33  = CLIP(t33a + t34a);
    t34  = CLIP(t33a - t34a);
    t35a = CLIP(t32  - t35);
    t36a = CLIP(t39  - t36);
    t37  = CLIP(t38a - t37a);
    t38  = CLIP(t38a + t37a);
    t39a = CLIP(t39  + t36);
    t40a = CLIP(t40  + t43);
    t41  = CLIP(t41a + t42a);
    t42  = CLIP(t41a - t42a);
    t43a = CLIP(t40  - t43);
    t44a = CLIP(t47  - t44);
    t45  = CLIP(t46a - t45a);
    t46  = CLIP(t46a + t45a);
    t47a = CLIP(t47  + t44);
    t48a = CLIP(t48  + t51);
    t49  = CLIP(t49a + t50a);
    t50  = CLIP(t49a - t50a);
    t51a = CLIP(t48  - t51);
    t52a = CLIP(t55  - t52);
    t53  = CLIP(t54a - t53a);
    t54  = CLIP(t54a + t53a);
    t55a = CLIP(t55  + t52);
    t56a = CLIP(t56  + t59);
    t57  = CLIP(t57a + t58a);
    t58  = CLIP(t57a - t58a);
    t59a = CLIP(t56  - t59);
    t60a = CLIP(t63  - t60);
    t61  = CLIP(t62a - t61a);
    t62  = CLIP(t62a + t61a);
    t63a = CLIP(t63  + t60);

    t34a = (t34  * -4017 + t61  *   799 + 2048) >> 12;
    t35  = (t35a * -4017 + t60a *   799 + 2048) >> 12;
    t36  = (t36a * - 799 + t59a * -4017 + 2048) >> 12;
    t37a = (t37  * - 799 + t58  * -4017 + 2048) >> 12;
    t42a = (t42  * -2276 + t53  *  3406 + 2048) >> 12;
    t43  = (t43a * -2276 + t52a *  3406 + 2048) >> 12;
    t44  = (t44a * -3406 + t51a * -2276 + 2048) >> 12;
    t45a = (t45  * -3406 + t50  * -2276 + 2048) >> 12;
    t50a = (t45  * -2276 + t50  *  3406 + 2048) >> 12;
    t51  = (t44a * -2276 + t51a *  3406 + 2048) >> 12;
    t52  = (t43a *  3406 + t52a *  2276 + 2048) >> 12;
    t53a = (t42  *  3406 + t53  *  2276 + 2048) >> 12;
    t58a = (t37  * -4017 + t58  *   799 + 2048) >> 12;
    t59  = (t36a * -4017 + t59a *   799 + 2048) >> 12;
    t60  = (t35a *   799 + t60a *  4017 + 2048) >> 12;
    t61a = (t34  *   799 + t61  *  4017 + 2048) >> 12;

    t32  = CLIP(t32a + t39a);
    t33a = CLIP(t33  + t38);
    t34  = CLIP(t34a + t37a);
    t35a = CLIP(t35  + t36);
    t36a = CLIP(t35  - t36);
    t37  = CLIP(t34a - t37a);
    t38a = CLIP(t33  - t38);
    t39  = CLIP(t32a - t39a);
    t40  = CLIP(t47a - t40a);
    t41a = CLIP(t46  - t41);
    t42  = CLIP(t45a - t42a);
    t43a = CLIP(t44  - t43);
    t44a = CLIP(t44  + t43);
    t45  = CLIP(t45a + t42a);
    t46a = CLIP(t46  + t41);
    t47  = CLIP(t47a + t40a);
    t48  = CLIP(t48a + t55a);
    t49a = CLIP(t49  + t54);
    t50  = CLIP(t50a + t53a);
    t51a = CLIP(t51  + t52);
    t52a = CLIP(t51  - t52);
    t53  = CLIP(t50a - t53a);
    t54a = CLIP(t49  - t54);
    t55  = CLIP(t48a - t55a);
    t56  = CLIP(t63a - t56a);
    t57a = CLIP(t62  - t57);
    t58  = CLIP(t61a - t58a);
    t59a = CLIP(t60  - t59);
    t60a = CLIP(t60  + t59);
    t61  = CLIP(t61a + t58a);
    t62a = CLIP(t62  + t57);
    t63  = CLIP(t63a + t56a);

    t36  = (t36a * -3784 + t59a *  1567 + 2048) >> 12;
    t37a = (t37  * -3784 + t58  *  1567 + 2048) >> 12;
    t38  = (t38a * -3784 + t57a *  1567 + 2048) >> 12;
    t39a = (t39  * -3784 + t56  *  1567 + 2048) >> 12;
    t40a = (t40  * -1567 + t55  * -3784 + 2048) >> 12;
    t41  = (t41a * -1567 + t54a * -3784 + 2048) >> 12;
    t42a = (t42  * -1567 + t53  * -3784 + 2048) >> 12;
    t43  = (t43a * -1567 + t52a * -3784 + 2048) >> 12;
    t52  = (t43a * -3784 + t52a *  1567 + 2048) >> 12;
    t53a = (t42  * -3784 + t53  *  1567 + 2048) >> 12;
    t54  = (t41a * -3784 + t54a *  1567 + 2048) >> 12;
    t55a = (t40  * -3784 + t55  *  1567 + 2048) >> 12;
    t56a = (t39  *  1567 + t56  *  3784 + 2048) >> 12;
    t57  = (t38a *  1567 + t57a *  3784 + 2048) >> 12;
    t58a = (t37  *  1567 + t58  *  3784 + 2048) >> 12;
    t59  = (t36a *  1567 + t59a *  3784 + 2048) >> 12;

    t32a = CLIP(t32  + t47);
    t33  = CLIP(t33a + t46a);
    t34a = CLIP(t34  + t45);
    t35  = CLIP(t35a + t44a);
    t36a = CLIP(t36  + t43);
    t37  = CLIP(t37a + t42a);
    t38a = CLIP(t38  + t41);
    t39  = CLIP(t39a + t40a);
    t40  = CLIP(t39a - t40a);
    t41a = CLIP(t38  - t41);
    t42  = CLIP(t37a - t42a);
    t43a = CLIP(t36  - t43);
    t44  = CLIP(t35a - t44a);
    t45a = CLIP(t34  - t45);
    t46  = CLIP(t33a - t46a);
    t47a = CLIP(t32  - t47);
    t48a = CLIP(t63  - t48);
    t49  = CLIP(t62a - t49a);
    t50a = CLIP(t61  - t50);
    t51  = CLIP(t60a - t51a);
    t52a = CLIP(t59  - t52);
    t53  = CLIP(t58a - t53a);
    t54a = CLIP(t57  - t54);
    t55  = CLIP(t56a - t55a);
    t56  = CLIP(t56a + t55a);
    t57a = CLIP(t57  + t54);
    t58  = CLIP(t58a + t53a);
    t59a = CLIP(t59  + t52);
    t60  = CLIP(t60a + t51a);
    t61a = CLIP(t61  + t50);
    t62  = CLIP(t62a + t49a);
    t63a = CLIP(t63  + t48);

    t40a = (t40  * -2896 + t55  * 2896 + 2048) >> 12;
    t41  = (t41a * -2896 + t54a * 2896 + 2048) >> 12;
    t42a = (t42  * -2896 + t53  * 2896 + 2048) >> 12;
    t43  = (t43a * -2896 + t52a * 2896 + 2048) >> 12;
    t44a = (t44  * -2896 + t51  * 2896 + 2048) >> 12;
    t45  = (t45a * -2896 + t50a * 2896 + 2048) >> 12;
    t46a = (t46  * -2896 + t49  * 2896 + 2048) >> 12;
    t47  = (t47a * -2896 + t48a * 2896 + 2048) >> 12;
    t48  = (t47a *  2896 + t48a * 2896 + 2048) >> 12;
    t49a = (t46  *  2896 + t49  * 2896 + 2048) >> 12;
    t50  = (t45a *  2896 + t50a * 2896 + 2048) >> 12;
    t51a = (t44  *  2896 + t51  * 2896 + 2048) >> 12;
    t52  = (t43a *  2896 + t52a * 2896 + 2048) >> 12;
    t53a = (t42  *  2896 + t53  * 2896 + 2048) >> 12;
    t54  = (t41a *  2896 + t54a * 2896 + 2048) >> 12;
    t55a = (t40  *  2896 + t55  * 2896 + 2048) >> 12;

    out[ 0 * out_s] = CLIP(tmp[ 0] + t63a);
    out[ 1 * out_s] = CLIP(tmp[ 1] + t62);
    out[ 2 * out_s] = CLIP(tmp[ 2] + t61a);
    out[ 3 * out_s] = CLIP(tmp[ 3] + t60);
    out[ 4 * out_s] = CLIP(tmp[ 4] + t59a);
    out[ 5 * out_s] = CLIP(tmp[ 5] + t58);
    out[ 6 * out_s] = CLIP(tmp[ 6] + t57a);
    out[ 7 * out_s] = CLIP(tmp[ 7] + t56);
    out[ 8 * out_s] = CLIP(tmp[ 8] + t55a);
    out[ 9 * out_s] = CLIP(tmp[ 9] + t54);
    out[10 * out_s] = CLIP(tmp[10] + t53a);
    out[11 * out_s] = CLIP(tmp[11] + t52);
    out[12 * out_s] = CLIP(tmp[12] + t51a);
    out[13 * out_s] = CLIP(tmp[13] + t50);
    out[14 * out_s] = CLIP(tmp[14] + t49a);
    out[15 * out_s] = CLIP(tmp[15] + t48);
    out[16 * out_s] = CLIP(tmp[16] + t47);
    out[17 * out_s] = CLIP(tmp[17] + t46a);
    out[18 * out_s] = CLIP(tmp[18] + t45);
    out[19 * out_s] = CLIP(tmp[19] + t44a);
    out[20 * out_s] = CLIP(tmp[20] + t43);
    out[21 * out_s] = CLIP(tmp[21] + t42a);
    out[22 * out_s] = CLIP(tmp[22] + t41);
    out[23 * out_s] = CLIP(tmp[23] + t40a);
    out[24 * out_s] = CLIP(tmp[24] + t39);
    out[25 * out_s] = CLIP(tmp[25] + t38a);
    out[26 * out_s] = CLIP(tmp[26] + t37);
    out[27 * out_s] = CLIP(tmp[27] + t36a);
    out[28 * out_s] = CLIP(tmp[28] + t35);
    out[29 * out_s] = CLIP(tmp[29] + t34a);
    out[30 * out_s] = CLIP(tmp[30] + t33);
    out[31 * out_s] = CLIP(tmp[31] + t32a);
    out[32 * out_s] = CLIP(tmp[31] - t32a);
    out[33 * out_s] = CLIP(tmp[30] - t33);
    out[34 * out_s] = CLIP(tmp[29] - t34a);
    out[35 * out_s] = CLIP(tmp[28] - t35);
    out[36 * out_s] = CLIP(tmp[27] - t36a);
    out[37 * out_s] = CLIP(tmp[26] - t37);
    out[38 * out_s] = CLIP(tmp[25] - t38a);
    out[39 * out_s] = CLIP(tmp[24] - t39);
    out[40 * out_s] = CLIP(tmp[23] - t40a);
    out[41 * out_s] = CLIP(tmp[22] - t41);
    out[42 * out_s] = CLIP(tmp[21] - t42a);
    out[43 * out_s] = CLIP(tmp[20] - t43);
    out[44 * out_s] = CLIP(tmp[19] - t44a);
    out[45 * out_s] = CLIP(tmp[18] - t45);
    out[46 * out_s] = CLIP(tmp[17] - t46a);
    out[47 * out_s] = CLIP(tmp[16] - t47);
    out[48 * out_s] = CLIP(tmp[15] - t48);
    out[49 * out_s] = CLIP(tmp[14] - t49a);
    out[50 * out_s] = CLIP(tmp[13] - t50);
    out[51 * out_s] = CLIP(tmp[12] - t51a);
    out[52 * out_s] = CLIP(tmp[11] - t52);
    out[53 * out_s] = CLIP(tmp[10] - t53a);
    out[54 * out_s] = CLIP(tmp[ 9] - t54);
    out[55 * out_s] = CLIP(tmp[ 8] - t55a);
    out[56 * out_s] = CLIP(tmp[ 7] - t56);
    out[57 * out_s] = CLIP(tmp[ 6] - t57a);
    out[58 * out_s] = CLIP(tmp[ 5] - t58);
    out[59 * out_s] = CLIP(tmp[ 4] - t59a);
    out[60 * out_s] = CLIP(tmp[ 3] - t60);
    out[61 * out_s] = CLIP(tmp[ 2] - t61a);
    out[62 * out_s] = CLIP(tmp[ 1] - t62);
    out[63 * out_s] = CLIP(tmp[ 0] - t63a);
}

static void NOINLINE
inv_adst4_1d(const coef *const in, const ptrdiff_t in_s,
             coef *const out, const ptrdiff_t out_s, const int range)
{
    const int in0 = in[0 * in_s], in1 = in[1 * in_s];
    const int in2 = in[2 * in_s], in3 = in[3 * in_s];

    int t0 = 1321 * in0 + 3803 * in2 + 2482 * in3;
    int t1 = 2482 * in0 - 1321 * in2 - 3803 * in3;
    int t2 = 3344 * (in0 - in2 + in3);
    int t3 = 3344 * in1;

    out[0 * out_s] = (t0 + t3      + 2048) >> 12;
    out[1 * out_s] = (t1 + t3      + 2048) >> 12;
    out[2 * out_s] = (t2           + 2048) >> 12;
    out[3 * out_s] = (t0 + t1 - t3 + 2048) >> 12;
}

static void NOINLINE
inv_adst8_1d(const coef *const in, const ptrdiff_t in_s,
             coef *const out, const ptrdiff_t out_s, const int max)
{
    const int min = -max - 1;
    const int in0 = in[0 * in_s], in1 = in[1 * in_s];
    const int in2 = in[2 * in_s], in3 = in[3 * in_s];
    const int in4 = in[4 * in_s], in5 = in[5 * in_s];
    const int in6 = in[6 * in_s], in7 = in[7 * in_s];

    int t0a = (4076 * in7 +  401 * in0 + 2048) >> 12;
    int t1a = ( 401 * in7 - 4076 * in0 + 2048) >> 12;
    int t2a = (3612 * in5 + 1931 * in2 + 2048) >> 12;
    int t3a = (1931 * in5 - 3612 * in2 + 2048) >> 12;
    int t4a = (2598 * in3 + 3166 * in4 + 2048) >> 12;
    int t5a = (3166 * in3 - 2598 * in4 + 2048) >> 12;
    int t6a = (1189 * in1 + 3920 * in6 + 2048) >> 12;
    int t7a = (3920 * in1 - 1189 * in6 + 2048) >> 12;

    int t0 = CLIP(t0a + t4a);
    int t1 = CLIP(t1a + t5a);
    int t2 = CLIP(t2a + t6a);
    int t3 = CLIP(t3a + t7a);
    int t4 = CLIP(t0a - t4a);
    int t5 = CLIP(t1a - t5a);
    int t6 = CLIP(t2a - t6a);
    int t7 = CLIP(t3a - t7a);

    t4a = (3784 * t4 + 1567 * t5 + 2048) >> 12;
    t5a = (1567 * t4 - 3784 * t5 + 2048) >> 12;
    t6a = (3784 * t7 - 1567 * t6 + 2048) >> 12;
    t7a = (1567 * t7 + 3784 * t6 + 2048) >> 12;

    out[0 * out_s] = CLIP(  t0 + t2);
    out[7 * out_s] = CLIP(-(t1 + t3));
    t2             = CLIP(  t0 - t2);
    t3             = CLIP(  t1 - t3);

    out[1 * out_s] = CLIP(-(t4a + t6a));
    out[6 * out_s] = CLIP(  t5a + t7a );
    t6             = CLIP(  t4a - t6a );
    t7             = CLIP(  t5a - t7a );

    out[3 * out_s] = -(((t2 + t3) * 2896 + 2048) >> 12);
    out[4 * out_s] =   ((t2 - t3) * 2896 + 2048) >> 12;
    out[2 * out_s] =   ((t6 + t7) * 2896 + 2048) >> 12;
    out[5 * out_s] = -(((t6 - t7) * 2896 + 2048) >> 12);
}

static void NOINLINE
inv_adst16_1d(const coef *const in, const ptrdiff_t in_s,
              coef *const out, const ptrdiff_t out_s, const int max)
{
    const int min = -max - 1;
    const int in0  = in[ 0 * in_s], in1  = in[ 1 * in_s];
    const int in2  = in[ 2 * in_s], in3  = in[ 3 * in_s];
    const int in4  = in[ 4 * in_s], in5  = in[ 5 * in_s];
    const int in6  = in[ 6 * in_s], in7  = in[ 7 * in_s];
    const int in8  = in[ 8 * in_s], in9  = in[ 9 * in_s];
    const int in10 = in[10 * in_s], in11 = in[11 * in_s];
    const int in12 = in[12 * in_s], in13 = in[13 * in_s];
    const int in14 = in[14 * in_s], in15 = in[15 * in_s];

    int t0  = (in15 * 4091 + in0  *  201 + 2048) >> 12;
    int t1  = (in15 *  201 - in0  * 4091 + 2048) >> 12;
    int t2  = (in13 * 3973 + in2  *  995 + 2048) >> 12;
    int t3  = (in13 *  995 - in2  * 3973 + 2048) >> 12;
    int t4  = (in11 * 3703 + in4  * 1751 + 2048) >> 12;
    int t5  = (in11 * 1751 - in4  * 3703 + 2048) >> 12;
    int t6  = (in9  * 3290 + in6  * 2440 + 2048) >> 12;
    int t7  = (in9  * 2440 - in6  * 3290 + 2048) >> 12;
    int t8  = (in7  * 2751 + in8  * 3035 + 2048) >> 12;
    int t9  = (in7  * 3035 - in8  * 2751 + 2048) >> 12;
    int t10 = (in5  * 2106 + in10 * 3513 + 2048) >> 12;
    int t11 = (in5  * 3513 - in10 * 2106 + 2048) >> 12;
    int t12 = (in3  * 1380 + in12 * 3857 + 2048) >> 12;
    int t13 = (in3  * 3857 - in12 * 1380 + 2048) >> 12;
    int t14 = (in1  *  601 + in14 * 4052 + 2048) >> 12;
    int t15 = (in1  * 4052 - in14 *  601 + 2048) >> 12;

    int t0a  = CLIP(t0 + t8 );
    int t1a  = CLIP(t1 + t9 );
    int t2a  = CLIP(t2 + t10);
    int t3a  = CLIP(t3 + t11);
    int t4a  = CLIP(t4 + t12);
    int t5a  = CLIP(t5 + t13);
    int t6a  = CLIP(t6 + t14);
    int t7a  = CLIP(t7 + t15);
    int t8a  = CLIP(t0 - t8 );
    int t9a  = CLIP(t1 - t9 );
    int t10a = CLIP(t2 - t10);
    int t11a = CLIP(t3 - t11);
    int t12a = CLIP(t4 - t12);
    int t13a = CLIP(t5 - t13);
    int t14a = CLIP(t6 - t14);
    int t15a = CLIP(t7 - t15);

    t8   = (t8a  * 4017 + t9a  *  799 + 2048) >> 12;
    t9   = (t8a  *  799 - t9a  * 4017 + 2048) >> 12;
    t10  = (t10a * 2276 + t11a * 3406 + 2048) >> 12;
    t11  = (t10a * 3406 - t11a * 2276 + 2048) >> 12;
    t12  = (t13a * 4017 - t12a *  799 + 2048) >> 12;
    t13  = (t13a *  799 + t12a * 4017 + 2048) >> 12;
    t14  = (t15a * 2276 - t14a * 3406 + 2048) >> 12;
    t15  = (t15a * 3406 + t14a * 2276 + 2048) >> 12;

    t0   = CLIP(t0a + t4a);
    t1   = CLIP(t1a + t5a);
    t2   = CLIP(t2a + t6a);
    t3   = CLIP(t3a + t7a);
    t4   = CLIP(t0a - t4a);
    t5   = CLIP(t1a - t5a);
    t6   = CLIP(t2a - t6a);
    t7   = CLIP(t3a - t7a);
    t8a  = CLIP(t8  + t12);
    t9a  = CLIP(t9  + t13);
    t10a = CLIP(t10 + t14);
    t11a = CLIP(t11 + t15);
    t12a = CLIP(t8  - t12);
    t13a = CLIP(t9  - t13);
    t14a = CLIP(t10 - t14);
    t15a = CLIP(t11 - t15);

    t4a  = (t4   * 3784 + t5   * 1567 + 2048) >> 12;
    t5a  = (t4   * 1567 - t5   * 3784 + 2048) >> 12;
    t6a  = (t7   * 3784 - t6   * 1567 + 2048) >> 12;
    t7a  = (t7   * 1567 + t6   * 3784 + 2048) >> 12;
    t12  = (t12a * 3784 + t13a * 1567 + 2048) >> 12;
    t13  = (t12a * 1567 - t13a * 3784 + 2048) >> 12;
    t14  = (t15a * 3784 - t14a * 1567 + 2048) >> 12;
    t15  = (t15a * 1567 + t14a * 3784 + 2048) >> 12;

    out[ 0 * out_s] = CLIP(  t0  + t2   );
    out[15 * out_s] = CLIP(-(t1  + t3)  );
    t2a             = CLIP(  t0  - t2   );
    t3a             = CLIP(  t1  - t3   );
    out[ 3 * out_s] = CLIP(-(t4a + t6a) );
    out[12 * out_s] = CLIP(  t5a + t7a  );
    t6              = CLIP(  t4a - t6a  );
    t7              = CLIP(  t5a - t7a  );
    out[ 1 * out_s] = CLIP(-(t8a + t10a));
    out[14 * out_s] = CLIP(  t9a + t11a );
    t10             = CLIP(  t8a - t10a );
    t11             = CLIP(  t9a - t11a );
    out[ 2 * out_s] = CLIP(  t12 + t14  );
    out[13 * out_s] = CLIP(-(t13 + t15) );
    t14a            = CLIP(  t12 - t14  );
    t15a            = CLIP(  t13 - t15  );

    out[ 7 * out_s] = -(((t2a  + t3a)  * 2896 + 2048) >> 12);
    out[ 8 * out_s] =   ((t2a  - t3a)  * 2896 + 2048) >> 12;
    out[ 4 * out_s] =   ((t6   + t7)   * 2896 + 2048) >> 12;
    out[11 * out_s] = -(((t6   - t7)   * 2896 + 2048) >> 12);
    out[ 6 * out_s] =   ((t10  + t11)  * 2896 + 2048) >> 12;
    out[ 9 * out_s] = -(((t10  - t11)  * 2896 + 2048) >> 12);
    out[ 5 * out_s] = -(((t14a + t15a) * 2896 + 2048) >> 12);
    out[10 * out_s] =   ((t14a - t15a) * 2896 + 2048) >> 12;
}

#define flip_inv_adst(sz) \
static void inv_flipadst##sz##_1d(const coef *const in, const ptrdiff_t in_s, \
                                  coef *const out, const ptrdiff_t out_s, const int range) \
{ \
    inv_adst##sz##_1d(in, in_s, &out[(sz - 1) * out_s], -out_s, range); \
}

flip_inv_adst(4)
flip_inv_adst(8)
flip_inv_adst(16)

#undef flip_inv_adst

static void NOINLINE
inv_identity4_1d(const coef *const in, const ptrdiff_t in_s,
                 coef *const out, const ptrdiff_t out_s, const int range)
{
    for (int i = 0; i < 4; i++)
        out[out_s * i] = (in[in_s * i] * 5793 + 2048) >> 12;
}

static void NOINLINE
inv_identity8_1d(const coef *const in, const ptrdiff_t in_s,
                 coef *const out, const ptrdiff_t out_s, const int range)
{
    for (int i = 0; i < 8; i++)
        out[out_s * i] = in[in_s * i] * 2;
}

static void NOINLINE
inv_identity16_1d(const coef *const in, const ptrdiff_t in_s,
                  coef *const out, const ptrdiff_t out_s, const int range)
{
    for (int i = 0; i < 16; i++)
        out[out_s * i] = (in[in_s * i] * 2 * 5793 + 2048) >> 12;
}

static void NOINLINE
inv_identity32_1d(const coef *const in, const ptrdiff_t in_s,
                  coef *const out, const ptrdiff_t out_s, const int range)
{
    for (int i = 0; i < 32; i++)
        out[out_s * i] = in[in_s * i] * 4;
}

static void NOINLINE
inv_wht4_1d(const coef *const in, const ptrdiff_t in_s,
            coef *const out, const ptrdiff_t out_s,
            const int pass)
{
    const int sh = 2 * !pass;
    const int in0 = in[0 * in_s] >> sh, in1 = in[1 * in_s] >> sh;
    const int in2 = in[2 * in_s] >> sh, in3 = in[3 * in_s] >> sh;
    const int t0 = in0 + in1;
    const int t2 = in2 - in3;
    const int t4 = (t0 - t2) >> 1;
    const int t3 = t4 - in3;
    const int t1 = t4 - in1;

    out[0 * out_s] = t0 - t3;
    out[1 * out_s] = t3;
    out[2 * out_s] = t1;
    out[3 * out_s] = t2 + t1;
}
