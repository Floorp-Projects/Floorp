/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef SOLARIS
#define RF_INLINE_MACROS 1
#endif

static const double TwoTo16 = 65536.0;
static const double TwoToMinus16 = 1.0 / 65536.0;
static const double Zero = 0.0;
static const double TwoTo32 = 65536.0 * 65536.0;
static const double TwoToMinus32 = 1.0 / (65536.0 * 65536.0);

#ifdef RF_INLINE_MACROS

double upper32(double);
double lower32(double, double);
double mod(double, double, double);

void i16_to_d16_and_d32x4(const double * /*1/(2^16)*/,
                          const double * /* 2^16*/,
                          const double * /* 0 */,
                          double * /*result16*/,
                          double * /* result32 */,
                          float * /*source - should be unsigned int* converted to float* */);

#else
#ifdef MP_USE_FLOOR
#include <math.h>
#else
#define floor(d) ((double)((unsigned long long)(d)))
#endif

static double
upper32(double x)
{
    return floor(x * TwoToMinus32);
}

static double
lower32(double x, double y)
{
    return x - TwoTo32 * floor(x * TwoToMinus32);
}

static double
mod(double x, double oneoverm, double m)
{
    return x - m * floor(x * oneoverm);
}

#endif

static void
cleanup(double *dt, int from, int tlen)
{
    int i;
    double tmp, tmp1, x, x1;

    tmp = tmp1 = Zero;
    /* original code **
     for(i=2*from;i<2*tlen-2;i++)
       {
         x=dt[i];
         dt[i]=lower32(x,Zero)+tmp1;
         tmp1=tmp;
         tmp=upper32(x);
       }
     dt[tlen-2]+=tmp1;
     dt[tlen-1]+=tmp;
     **end original code ***/
    /* new code ***/
    for (i = 2 * from; i < 2 * tlen; i += 2) {
        x = dt[i];
        x1 = dt[i + 1];
        dt[i] = lower32(x, Zero) + tmp;
        dt[i + 1] = lower32(x1, Zero) + tmp1;
        tmp = upper32(x);
        tmp1 = upper32(x1);
    }
    /** end new code **/
}

void
conv_d16_to_i32(unsigned int *i32, double *d16, long long *tmp, int ilen)
{
    int i;
    long long t, t1, a, b, c, d;

    t1 = 0;
    a = (long long)d16[0];
    b = (long long)d16[1];
    for (i = 0; i < ilen - 1; i++) {
        c = (long long)d16[2 * i + 2];
        t1 += (unsigned int)a;
        t = (a >> 32);
        d = (long long)d16[2 * i + 3];
        t1 += (b & 0xffff) << 16;
        t += (b >> 16) + (t1 >> 32);
        i32[i] = (unsigned int)t1;
        t1 = t;
        a = c;
        b = d;
    }
    t1 += (unsigned int)a;
    t = (a >> 32);
    t1 += (b & 0xffff) << 16;
    i32[i] = (unsigned int)t1;
}

void
conv_i32_to_d32(double *d32, unsigned int *i32, int len)
{
    int i;

#pragma pipeloop(0)
    for (i = 0; i < len; i++)
        d32[i] = (double)(i32[i]);
}

void
conv_i32_to_d16(double *d16, unsigned int *i32, int len)
{
    int i;
    unsigned int a;

#pragma pipeloop(0)
    for (i = 0; i < len; i++) {
        a = i32[i];
        d16[2 * i] = (double)(a & 0xffff);
        d16[2 * i + 1] = (double)(a >> 16);
    }
}

void
conv_i32_to_d32_and_d16(double *d32, double *d16,
                        unsigned int *i32, int len)
{
    int i = 0;
    unsigned int a;

#pragma pipeloop(0)
#ifdef RF_INLINE_MACROS
    for (; i < len - 3; i += 4) {
        i16_to_d16_and_d32x4(&TwoToMinus16, &TwoTo16, &Zero,
                             &(d16[2 * i]), &(d32[i]), (float *)(&(i32[i])));
    }
#endif
    for (; i < len; i++) {
        a = i32[i];
        d32[i] = (double)(i32[i]);
        d16[2 * i] = (double)(a & 0xffff);
        d16[2 * i + 1] = (double)(a >> 16);
    }
}

void
adjust_montf_result(unsigned int *i32, unsigned int *nint, int len)
{
    long long acc;
    int i;

    if (i32[len] > 0)
        i = -1;
    else {
        for (i = len - 1; i >= 0; i--) {
            if (i32[i] != nint[i])
                break;
        }
    }
    if ((i < 0) || (i32[i] > nint[i])) {
        acc = 0;
        for (i = 0; i < len; i++) {
            acc = acc + (unsigned long long)(i32[i]) - (unsigned long long)(nint[i]);
            i32[i] = (unsigned int)acc;
            acc = acc >> 32;
        }
    }
}

/*
** the lengths of the input arrays should be at least the following:
** result[nlen+1], dm1[nlen], dm2[2*nlen+1], dt[4*nlen+2], dn[nlen], nint[nlen]
** all of them should be different from one another
**
*/
void
mont_mulf_noconv(unsigned int *result,
                 double *dm1, double *dm2, double *dt,
                 double *dn, unsigned int *nint,
                 int nlen, double dn0)
{
    int i, j, jj;
    int tmp;
    double digit, m2j, nextm2j, a, b;
    double *dptmp, *pdm1, *pdm2, *pdn, *pdtj, pdn_0, pdm1_0;

    pdm1 = &(dm1[0]);
    pdm2 = &(dm2[0]);
    pdn = &(dn[0]);
    pdm2[2 * nlen] = Zero;

    if (nlen != 16) {
        for (i = 0; i < 4 * nlen + 2; i++)
            dt[i] = Zero;

        a = dt[0] = pdm1[0] * pdm2[0];
        digit = mod(lower32(a, Zero) * dn0, TwoToMinus16, TwoTo16);

        pdtj = &(dt[0]);
        for (j = jj = 0; j < 2 * nlen; j++, jj++, pdtj++) {
            m2j = pdm2[j];
            a = pdtj[0] + pdn[0] * digit;
            b = pdtj[1] + pdm1[0] * pdm2[j + 1] + a * TwoToMinus16;
            pdtj[1] = b;

#pragma pipeloop(0)
            for (i = 1; i < nlen; i++) {
                pdtj[2 * i] += pdm1[i] * m2j + pdn[i] * digit;
            }
            if ((jj == 30)) {
                cleanup(dt, j / 2 + 1, 2 * nlen + 1);
                jj = 0;
            }

            digit = mod(lower32(b, Zero) * dn0, TwoToMinus16, TwoTo16);
        }
    } else {
        a = dt[0] = pdm1[0] * pdm2[0];

        dt[65] = dt[64] = dt[63] = dt[62] = dt[61] = dt[60] =
            dt[59] = dt[58] = dt[57] = dt[56] = dt[55] = dt[54] =
                dt[53] = dt[52] = dt[51] = dt[50] = dt[49] = dt[48] =
                    dt[47] = dt[46] = dt[45] = dt[44] = dt[43] = dt[42] =
                        dt[41] = dt[40] = dt[39] = dt[38] = dt[37] = dt[36] =
                            dt[35] = dt[34] = dt[33] = dt[32] = dt[31] = dt[30] =
                                dt[29] = dt[28] = dt[27] = dt[26] = dt[25] = dt[24] =
                                    dt[23] = dt[22] = dt[21] = dt[20] = dt[19] = dt[18] =
                                        dt[17] = dt[16] = dt[15] = dt[14] = dt[13] = dt[12] =
                                            dt[11] = dt[10] = dt[9] = dt[8] = dt[7] = dt[6] =
                                                dt[5] = dt[4] = dt[3] = dt[2] = dt[1] = Zero;

        pdn_0 = pdn[0];
        pdm1_0 = pdm1[0];

        digit = mod(lower32(a, Zero) * dn0, TwoToMinus16, TwoTo16);
        pdtj = &(dt[0]);

        for (j = 0; j < 32; j++, pdtj++) {

            m2j = pdm2[j];
            a = pdtj[0] + pdn_0 * digit;
            b = pdtj[1] + pdm1_0 * pdm2[j + 1] + a * TwoToMinus16;
            pdtj[1] = b;

            /**** this loop will be fully unrolled:
             for(i=1;i<16;i++)
               {
                 pdtj[2*i]+=pdm1[i]*m2j+pdn[i]*digit;
               }
             *************************************/
            pdtj[2] += pdm1[1] * m2j + pdn[1] * digit;
            pdtj[4] += pdm1[2] * m2j + pdn[2] * digit;
            pdtj[6] += pdm1[3] * m2j + pdn[3] * digit;
            pdtj[8] += pdm1[4] * m2j + pdn[4] * digit;
            pdtj[10] += pdm1[5] * m2j + pdn[5] * digit;
            pdtj[12] += pdm1[6] * m2j + pdn[6] * digit;
            pdtj[14] += pdm1[7] * m2j + pdn[7] * digit;
            pdtj[16] += pdm1[8] * m2j + pdn[8] * digit;
            pdtj[18] += pdm1[9] * m2j + pdn[9] * digit;
            pdtj[20] += pdm1[10] * m2j + pdn[10] * digit;
            pdtj[22] += pdm1[11] * m2j + pdn[11] * digit;
            pdtj[24] += pdm1[12] * m2j + pdn[12] * digit;
            pdtj[26] += pdm1[13] * m2j + pdn[13] * digit;
            pdtj[28] += pdm1[14] * m2j + pdn[14] * digit;
            pdtj[30] += pdm1[15] * m2j + pdn[15] * digit;
            /* no need for cleenup, cannot overflow */
            digit = mod(lower32(b, Zero) * dn0, TwoToMinus16, TwoTo16);
        }
    }

    conv_d16_to_i32(result, dt + 2 * nlen, (long long *)dt, nlen + 1);

    adjust_montf_result(result, nint, nlen);
}
