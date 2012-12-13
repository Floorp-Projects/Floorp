/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This inlined version is for 32-bit ARM platform only */

#if !defined(__arm__)
#error "This is for ARM only"
#endif

/* 16-bit thumb doesn't work inlined assember version */
#if (!defined(__thumb__) || defined(__thumb2__)) && !defined(__ARM_ARCH_3__)

#include "mpi-priv.h"

#ifdef MP_ASSEMBLY_MULTIPLY
void s_mpv_mul_d(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
  __asm__ __volatile__(
    "mov     r5, #0\n"
#ifdef __thumb2__
    "cbz     %1, 2f\n"
#else
    "cmp     %1, r5\n" /* r5 is 0 now */
    "beq     2f\n"
#endif

    "1:\n"
    "mov     r4, #0\n"
    "ldr     r6, [%0], #4\n"
    "umlal   r5, r4, r6, %2\n"
    "str     r5, [%3], #4\n"
    "mov     r5, r4\n"

    "subs    %1, #1\n"
    "bne     1b\n"

    "2:\n"
    "str     r5, [%3]\n"
  :
  : "r"(a), "r"(a_len), "r"(b), "r"(c)
  : "memory", "cc", "%r4", "%r5", "%r6");
}

void s_mpv_mul_d_add(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
  __asm__ __volatile__(
    "mov     r5, #0\n"
#ifdef __thumb2__
    "cbz     %1, 2f\n"
#else
    "cmp     %1, r5\n" /* r5 is 0 now */
    "beq     2f\n"
#endif

    "1:\n"
    "mov     r4, #0\n"
    "ldr     r6, [%3]\n"
    "adds    r5, r6\n"
    "adc     r4, r4, #0\n"

    "ldr     r6, [%0], #4\n"
    "umlal   r5, r4, r6, %2\n"
    "str     r5, [%3], #4\n"
    "mov     r5, r4\n"

    "subs    %1, #1\n"
    "bne     1b\n"

    "2:\n"
    "str     r5, [%3]\n"
    :
    : "r"(a), "r"(a_len), "r"(b), "r"(c)
    : "memory", "cc", "%r4", "%r5", "%r6");
}

void s_mpv_mul_d_add_prop(const mp_digit *a, mp_size a_len, mp_digit b, mp_digit *c)
{
  if (!a_len)
    return;

  __asm__ __volatile__(
    "mov     r5, #0\n"

    "1:\n"
    "mov     r4, #0\n"
    "ldr     r6, [%3]\n"
    "adds    r5, r6\n"
    "adc     r4, r4, #0\n"
    "ldr     r6, [%0], #4\n"
    "umlal   r5, r4, r6, %2\n"
    "str     r5, [%3], #4\n"
    "mov     r5, r4\n"

    "subs    %1, #1\n"
    "bne     1b\n"

#ifdef __thumb2__
    "cbz     r4, 3f\n"
#else
    "cmp     r4, #0\n"
    "beq     3f\n"
#endif

    "2:\n"
    "mov     r4, #0\n"
    "ldr     r6, [%3]\n"
    "adds    r5, r6\n"
    "adc     r4, r4, #0\n"
    "str     r5, [%3], #4\n"
    "movs    r5, r4\n"
    "bne     2b\n"

    "3:\n"
    :
    : "r"(a), "r"(a_len), "r"(b), "r"(c)
    : "memory", "cc", "%r4", "%r5", "%r6");
}
#endif

#ifdef MP_ASSEMBLY_SQUARE
void s_mpv_sqr_add_prop(const mp_digit *pa, mp_size a_len, mp_digit *ps)
{
  if (!a_len)
    return;

  __asm__ __volatile__(
    "mov     r3, #0\n"

    "1:\n"
    "mov     r4, #0\n"
    "ldr     r6, [%0], #4\n"
    "ldr     r5, [%2]\n"
    "adds    r3, r5\n"
    "adc     r4, r4, #0\n"
    "umlal   r3, r4, r6, r6\n" /* w = r3:r4 */
    "str     r3, [%2], #4\n"

    "ldr     r5, [%2]\n"
    "adds    r3, r4, r5\n"
    "mov     r4, #0\n"
    "adc     r4, r4, #0\n"
    "str     r3, [%2], #4\n"
    "mov     r3, r4\n"

    "subs    %1, #1\n"
    "bne     1b\n"

#ifdef __thumb2__
    "cbz     r3, 3f\n"
#else
    "cmp     r3, #0\n"
    "beq     3f\n"
#endif

    "2:\n"
    "mov     r4, #0\n"
    "ldr     r5, [%2]\n"
    "adds    r3, r5\n"
    "adc     r4, r4, #0\n"
    "str     r3, [%2], #4\n"
    "movs    r3, r4\n"
    "bne     2b\n"

    "3:"
    :
    : "r"(pa), "r"(a_len), "r"(ps)
    : "memory", "cc", "%r3", "%r4", "%r5", "%r6");
}
#endif
#endif
