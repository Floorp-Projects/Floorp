/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Makoto Kato <m_kato@ga2.so-net.ne.jp> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    "adc     r4, #0\n"

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
    "adc     r4, #0\n"
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
    "adc     r4, #0\n"
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
    "adc     r4, #0\n"
    "umlal   r3, r4, r6, r6\n" /* w = r3:r4 */
    "str     r3, [%2], #4\n"

    "ldr     r5, [%2]\n"
    "adds    r3, r4, r5\n"
    "mov     r4, #0\n"
    "adc     r4, #0\n"
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
    "adc     r4, #0\n"
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
