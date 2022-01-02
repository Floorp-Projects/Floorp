/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file contains routines that perform vector multiplication.  */

#include "mpi-priv.h"
#include <unistd.h>

#include <stddef.h>
/* #include <sys/systeminfo.h> */
#include <strings.h>

extern void multacc512(
    int length,                   /* doublewords in multiplicand vector. */
    const mp_digit *scalaraddr,   /* Address of scalar. */
    const mp_digit *multiplicand, /* The multiplicand vector. */
    mp_digit *result);            /* Where to accumulate the result. */

extern void maxpy_little(
    int length,                   /* doublewords in multiplicand vector. */
    const mp_digit *scalaraddr,   /* Address of scalar. */
    const mp_digit *multiplicand, /* The multiplicand vector. */
    mp_digit *result);            /* Where to accumulate the result. */

extern void add_diag_little(
    int length,           /* doublewords in input vector. */
    const mp_digit *root, /* The vector to square. */
    mp_digit *result);    /* Where to accumulate the result. */

void
s_mpv_sqr_add_prop(const mp_digit *pa, mp_size a_len, mp_digit *ps)
{
    add_diag_little(a_len, pa, ps);
}

#define MAX_STACK_DIGITS 258
#define MULTACC512_LEN (512 / MP_DIGIT_BIT)
#define HP_MPY_ADD_FN (a_len == MULTACC512_LEN ? multacc512 : maxpy_little)

/* c = a * b */
void
s_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    mp_digit x[MAX_STACK_DIGITS];
    mp_digit *px = x;
    size_t xSize = 0;

    if (a == c) {
        if (a_len > MAX_STACK_DIGITS) {
            xSize = sizeof(mp_digit) * (a_len + 2);
            px = malloc(xSize);
            if (!px)
                return;
        }
        memcpy(px, a, a_len * sizeof(*a));
        a = px;
    }
    s_mp_setz(c, a_len + 1);
    HP_MPY_ADD_FN(a_len, &b, a, c);
    if (px != x && px) {
        memset(px, 0, xSize);
        free(px);
    }
}

/* c += a * b, where a is a_len words long. */
void
s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
    c[a_len] = 0; /* so carry propagation stops here. */
    HP_MPY_ADD_FN(a_len, &b, a, c);
}

/* c += a * b, where a is y words long. */
void
s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b,
                     mp_digit *c)
{
    HP_MPY_ADD_FN(a_len, &b, a, c);
}
