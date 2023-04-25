/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2021      Kunwar Maheep Singh <kunwar.maheep@students.iiit.ac.in>
 *   2021      Christopher Moore <moore@free.fr>
 */

/* The ternarylogic implementation is based on Wojciech Muła's work at
 * https://github.com/WojciechMula/ternary-logic */

#if !defined(SIMDE_X86_AVX512_TERNARYLOGIC_H)
#define SIMDE_X86_AVX512_TERNARYLOGIC_H

#include "types.h"
#include "movm.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x00_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  HEDLEY_STATIC_CAST(void, b);
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t c0 = 0;
  return c0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x01_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a | t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x02_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | a;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = c & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x03_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = b | a;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x04_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x05_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = c | a;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x06_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x07_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = a | t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x08_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t t2 = t1 & c;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x09_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a | t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x0a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = c & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x0b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = t1 | c;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x0c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x0d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = t1 | b;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x0e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b | c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x0f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = ~a;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x10_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x11_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = c | b;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x12_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x13_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = b | t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x14_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x15_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = c | t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x16_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & t1;
  const uint_fast32_t t3 = ~a;
  const uint_fast32_t t4 = b ^ c;
  const uint_fast32_t t5 = t3 & t4;
  const uint_fast32_t t6 = t2 | t5;
  return t6;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x17_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = b & c;
  const uint_fast32_t t2 = (a & t0) | (~a & t1);
  const uint_fast32_t t3 = ~t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x18_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x19_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = b & c;
  const uint_fast32_t t2 = a & t1;
  const uint_fast32_t t3 = t0 ^ t2;
  const uint_fast32_t t4 = ~t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x1a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a ^ c;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x1b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = t1 | c;
  const uint_fast32_t t3 = t0 ^ t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x1c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a ^ b;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x1d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = t1 | b;
  const uint_fast32_t t3 = t0 ^ t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x1e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a ^ t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x1f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a & t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x20_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = t1 & c;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x21_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = b | t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x22_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = c & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x23_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 | c;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x24_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x25_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~c;
  const uint_fast32_t t3 = a ^ t2;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x26_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b ^ c;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x27_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 | c;
  const uint_fast32_t t3 = t0 ^ t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x28_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ a;
  const uint_fast32_t t1 = c & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x29_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 | c;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = b ^ c;
  const uint_fast32_t t4 = t2 ^ t3;
  const uint_fast32_t t5 = t1 & t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x2a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = c & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x2b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = c & t1;
  const uint_fast32_t t3 = ~c;
  const uint_fast32_t t4 = b | a;
  const uint_fast32_t t5 = ~t4;
  const uint_fast32_t t6 = t3 & t5;
  const uint_fast32_t t7 = t2 | t6;
  return t7;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x2c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x2d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = b | t0;
  const uint_fast32_t t2 = a ^ t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x2e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a & b;
  const uint_fast32_t t2 = t0 ^ t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x2f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = t1 & c;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x30_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x31_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = t1 | a;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x32_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a | c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x33_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = ~b;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x34_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a ^ b;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x35_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = t1 | a;
  const uint_fast32_t t3 = t0 ^ t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x36_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = b ^ t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x37_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = b & t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x38_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x39_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = b ^ t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x3a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a & t0;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 & c;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x3b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 & c;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x3c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = b ^ a;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x3d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = a | c;
  const uint_fast32_t t2 = ~t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x3e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & c;
  const uint_fast32_t t2 = a ^ b;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x3f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x40_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = t1 & b;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x41_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ a;
  const uint_fast32_t t1 = c | t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x42_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x43_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = a ^ t2;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x44_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = b & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x45_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 | b;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x46_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b ^ c;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x47_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 | b;
  const uint_fast32_t t3 = t0 ^ t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x48_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = b & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x49_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 | b;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = b ^ c;
  const uint_fast32_t t4 = t2 ^ t3;
  const uint_fast32_t t5 = t1 & t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x4a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x4b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 | c;
  const uint_fast32_t t2 = a ^ t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x4c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x4d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b & t1;
  const uint_fast32_t t3 = ~b;
  const uint_fast32_t t4 = a | c;
  const uint_fast32_t t5 = ~t4;
  const uint_fast32_t t6 = t3 & t5;
  const uint_fast32_t t7 = t2 | t6;
  return t7;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x4e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = c & t0;
  const uint_fast32_t t2 = ~c;
  const uint_fast32_t t3 = t2 & b;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x4f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = b & t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x50_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x51_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = t1 | a;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x52_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a ^ c;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x53_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = t1 | a;
  const uint_fast32_t t3 = t0 ^ t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x54_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a | b;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x55_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = ~c;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x56_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | a;
  const uint_fast32_t t1 = c ^ t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x57_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | a;
  const uint_fast32_t t1 = c & t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x58_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x59_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = c ^ t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x5a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = c ^ a;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x5b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a ^ c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x5c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a & t0;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 & b;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x5d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 & b;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x5e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t t2 = a ^ c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x5f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = c & a;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x60_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x61_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = a ^ c;
  const uint_fast32_t t4 = t2 ^ t3;
  const uint_fast32_t t5 = t1 & t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x62_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x63_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 | c;
  const uint_fast32_t t2 = b ^ t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x64_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | b;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x65_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 | b;
  const uint_fast32_t t2 = c ^ t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x66_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = c ^ b;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x67_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a | b;
  const uint_fast32_t t2 = ~t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x68_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a & t0;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = b & c;
  const uint_fast32_t t4 = t2 & t3;
  const uint_fast32_t t5 = t1 | t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x69_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a ^ t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x6a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = c ^ t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x6b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & c;
  const uint_fast32_t c1 = ~HEDLEY_STATIC_CAST(uint_fast32_t, 0);
  const uint_fast32_t t2 = a ^ c1;
  const uint_fast32_t t3 = b ^ c;
  const uint_fast32_t t4 = t2 ^ t3;
  const uint_fast32_t t5 = t1 | t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x6c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = b ^ t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x6d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t c1 = ~HEDLEY_STATIC_CAST(uint_fast32_t, 0);
  const uint_fast32_t t2 = a ^ c1;
  const uint_fast32_t t3 = b ^ c;
  const uint_fast32_t t4 = t2 ^ t3;
  const uint_fast32_t t5 = t1 | t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x6e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t t2 = b ^ c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x6f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x70_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x71_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b ^ c;
  const uint_fast32_t t3 = a & t2;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x72_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = c & t0;
  const uint_fast32_t t2 = ~c;
  const uint_fast32_t t3 = t2 & a;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x73_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = a & t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x74_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = b & t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = t2 & a;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x75_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = a & t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x76_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = b ^ c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x77_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = c & b;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x78_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = a ^ t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x79_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t c1 = ~HEDLEY_STATIC_CAST(uint_fast32_t, 0);
  const uint_fast32_t t2 = b ^ c1;
  const uint_fast32_t t3 = a ^ c;
  const uint_fast32_t t4 = t2 ^ t3;
  const uint_fast32_t t5 = t1 | t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x7a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = a ^ c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x7b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x7c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = a ^ b;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x7d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x7e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x7f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = t0 & c;
  const uint_fast32_t c1 = ~HEDLEY_STATIC_CAST(uint_fast32_t, 0);
  const uint_fast32_t t2 = t1 ^ c1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x80_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = a & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x81_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = a ^ t2;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x82_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ a;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = c & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x83_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 | c;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x84_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x85_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~c;
  const uint_fast32_t t3 = t2 | b;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x86_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = c ^ t1;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x87_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = a ^ t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x88_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = c & b;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x89_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 | b;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x8a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 | b;
  const uint_fast32_t t2 = c & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x8b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 | b;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = t2 | c;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x8c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 | c;
  const uint_fast32_t t2 = b & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x8d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 | b;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 | c;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x8e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = b ^ c;
  const uint_fast32_t t3 = t1 & t2;
  const uint_fast32_t t4 = t0 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x8f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b & c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x90_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x91_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = t2 | a;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x92_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = c ^ t1;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x93_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = b ^ t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x94_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = b ^ t1;
  const uint_fast32_t t3 = t0 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x95_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = c ^ t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x96_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a ^ t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x97_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = t1 ^ a;
  const uint_fast32_t t3 = b ^ c;
  const uint_fast32_t t4 = a ^ t3;
  const uint_fast32_t t5 = t2 | t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x98_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a | b;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x99_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = c ^ b;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x9a_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = t1 ^ c;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x9b_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 & c;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x9c_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = t1 ^ b;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x9d_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 & b;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x9e_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = c ^ t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0x9f_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a & t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa0_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = c & a;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa1_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = t2 | a;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa2_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a | t0;
  const uint_fast32_t t2 = c & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa3_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 | c;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa4_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a | b;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa5_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = c ^ a;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa6_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t t2 = t1 ^ c;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa7_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = t2 & c;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa8_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | b;
  const uint_fast32_t t1 = c & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xa9_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | a;
  const uint_fast32_t t1 = c ^ t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xaa_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  HEDLEY_STATIC_CAST(void, b);
  return c;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xab_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | a;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = c | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xac_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 & b;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xad_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b & c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xae_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t t2 = t1 | c;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xaf_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = c | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb0_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 | c;
  const uint_fast32_t t2 = a & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb1_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = t2 | c;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb2_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = b & t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = a | c;
  const uint_fast32_t t4 = t2 & t3;
  const uint_fast32_t t5 = t1 | t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb3_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a & c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb4_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t t2 = t1 ^ a;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb5_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~b;
  const uint_fast32_t t3 = t2 & a;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb6_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = c ^ t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb7_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = b & t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb8_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = t1 & a;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xb9_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xba_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = t1 | c;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xbb_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = c | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xbc_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = a ^ b;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xbd_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a ^ b;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xbe_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ a;
  const uint_fast32_t t1 = c | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xbf_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = c | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc0_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = b & a;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc1_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~c;
  const uint_fast32_t t3 = t2 | a;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc2_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a | c;
  const uint_fast32_t t3 = t1 & t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc3_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = b ^ a;
  const uint_fast32_t t1 = ~t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc4_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = b & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc5_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 | a;
  const uint_fast32_t t2 = ~a;
  const uint_fast32_t t3 = t2 | b;
  const uint_fast32_t t4 = t1 & t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc6_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & c;
  const uint_fast32_t t2 = t1 ^ b;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc7_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~c;
  const uint_fast32_t t3 = t2 & b;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc8_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = b & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xc9_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = b ^ t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xca_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 & c;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xcb_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b & c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xcc_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  HEDLEY_STATIC_CAST(void, c);
  return b;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xcd_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a | c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xce_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = t0 & c;
  const uint_fast32_t t2 = t1 | b;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xcf_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd0_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = b | t0;
  const uint_fast32_t t2 = a & t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd1_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & b;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd2_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & c;
  const uint_fast32_t t2 = t1 ^ a;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd3_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = ~c;
  const uint_fast32_t t3 = t2 & a;
  const uint_fast32_t t4 = t1 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd4_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = b & t0;
  const uint_fast32_t t2 = b ^ c;
  const uint_fast32_t t3 = ~t2;
  const uint_fast32_t t4 = a & t3;
  const uint_fast32_t t5 = t1 | t4;
  return t5;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd5_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a & b;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd6_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = b ^ t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd7_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ a;
  const uint_fast32_t t1 = c & t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd8_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = c & b;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = t1 & a;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xd9_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & b;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xda_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = a ^ c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xdb_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a ^ c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xdc_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 & a;
  const uint_fast32_t t2 = t1 | b;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xdd_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = b | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xde_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = b | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xdf_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe0_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a & t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe1_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a ^ t0;
  const uint_fast32_t t2 = ~t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe2_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = ~b;
  const uint_fast32_t t2 = t1 & c;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe3_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ b;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & c;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe4_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = c & a;
  const uint_fast32_t t1 = ~c;
  const uint_fast32_t t2 = t1 & b;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe5_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a & b;
  const uint_fast32_t t3 = t1 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe6_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & b;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe7_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~a;
  const uint_fast32_t t2 = t1 ^ c;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe8_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = a & t1;
  const uint_fast32_t t3 = t0 | t2;
  return t3;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xe9_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b ^ c;
  const uint_fast32_t t2 = t0 ^ t1;
  const uint_fast32_t t3 = a & b;
  const uint_fast32_t t4 = t2 | t3;
  return t4;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xea_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & a;
  const uint_fast32_t t1 = c | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xeb_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ a;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = c | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xec_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a & c;
  const uint_fast32_t t1 = b | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xed_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = a ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = b | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xee_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  const uint_fast32_t t0 = c | b;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xef_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~a;
  const uint_fast32_t t1 = b | c;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf0_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  HEDLEY_STATIC_CAST(void, c);
  return a;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf1_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf2_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 & c;
  const uint_fast32_t t2 = t1 | a;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf3_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = a | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf4_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = t0 & b;
  const uint_fast32_t t2 = t1 | a;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf5_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf6_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = a | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf7_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf8_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b & c;
  const uint_fast32_t t1 = a | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xf9_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b ^ c;
  const uint_fast32_t t1 = ~t0;
  const uint_fast32_t t2 = a | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xfa_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, b);
  const uint_fast32_t t0 = c | a;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xfb_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~b;
  const uint_fast32_t t1 = t0 | c;
  const uint_fast32_t t2 = a | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xfc_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t t0 = b | a;
  return t0;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xfd_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = ~c;
  const uint_fast32_t t1 = a | b;
  const uint_fast32_t t2 = t0 | t1;
  return t2;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xfe_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  const uint_fast32_t t0 = b | c;
  const uint_fast32_t t1 = a | t0;
  return t1;
}

SIMDE_FUNCTION_ATTRIBUTES
uint_fast32_t
simde_x_ternarylogic_0xff_impl_(uint_fast32_t a, uint_fast32_t b, uint_fast32_t c) {
  HEDLEY_STATIC_CAST(void, a);
  HEDLEY_STATIC_CAST(void, b);
  HEDLEY_STATIC_CAST(void, c);
  const uint_fast32_t c1 = ~HEDLEY_STATIC_CAST(uint_fast32_t, 0);
  return c1;
}

#define SIMDE_X_TERNARYLOGIC_CASE(value) \
  case value: \
    SIMDE_VECTORIZE \
    for (size_t i = 0 ; i < (sizeof(r_.u32f) / sizeof(r_.u32f[0])) ; i++) { \
      r_.u32f[i] = HEDLEY_CONCAT3(simde_x_ternarylogic_, value, _impl_)(a_.u32f[i], b_.u32f[i], c_.u32f[i]); \
    } \
    break;

#define SIMDE_X_TERNARYLOGIC_SWITCH(value) \
  switch(value) { \
    SIMDE_X_TERNARYLOGIC_CASE(0x00) \
    SIMDE_X_TERNARYLOGIC_CASE(0x01) \
    SIMDE_X_TERNARYLOGIC_CASE(0x02) \
    SIMDE_X_TERNARYLOGIC_CASE(0x03) \
    SIMDE_X_TERNARYLOGIC_CASE(0x04) \
    SIMDE_X_TERNARYLOGIC_CASE(0x05) \
    SIMDE_X_TERNARYLOGIC_CASE(0x06) \
    SIMDE_X_TERNARYLOGIC_CASE(0x07) \
    SIMDE_X_TERNARYLOGIC_CASE(0x08) \
    SIMDE_X_TERNARYLOGIC_CASE(0x09) \
    SIMDE_X_TERNARYLOGIC_CASE(0x0a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x0b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x0c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x0d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x0e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x0f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x10) \
    SIMDE_X_TERNARYLOGIC_CASE(0x11) \
    SIMDE_X_TERNARYLOGIC_CASE(0x12) \
    SIMDE_X_TERNARYLOGIC_CASE(0x13) \
    SIMDE_X_TERNARYLOGIC_CASE(0x14) \
    SIMDE_X_TERNARYLOGIC_CASE(0x15) \
    SIMDE_X_TERNARYLOGIC_CASE(0x16) \
    SIMDE_X_TERNARYLOGIC_CASE(0x17) \
    SIMDE_X_TERNARYLOGIC_CASE(0x18) \
    SIMDE_X_TERNARYLOGIC_CASE(0x19) \
    SIMDE_X_TERNARYLOGIC_CASE(0x1a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x1b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x1c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x1d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x1e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x1f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x20) \
    SIMDE_X_TERNARYLOGIC_CASE(0x21) \
    SIMDE_X_TERNARYLOGIC_CASE(0x22) \
    SIMDE_X_TERNARYLOGIC_CASE(0x23) \
    SIMDE_X_TERNARYLOGIC_CASE(0x24) \
    SIMDE_X_TERNARYLOGIC_CASE(0x25) \
    SIMDE_X_TERNARYLOGIC_CASE(0x26) \
    SIMDE_X_TERNARYLOGIC_CASE(0x27) \
    SIMDE_X_TERNARYLOGIC_CASE(0x28) \
    SIMDE_X_TERNARYLOGIC_CASE(0x29) \
    SIMDE_X_TERNARYLOGIC_CASE(0x2a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x2b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x2c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x2d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x2e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x2f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x30) \
    SIMDE_X_TERNARYLOGIC_CASE(0x31) \
    SIMDE_X_TERNARYLOGIC_CASE(0x32) \
    SIMDE_X_TERNARYLOGIC_CASE(0x33) \
    SIMDE_X_TERNARYLOGIC_CASE(0x34) \
    SIMDE_X_TERNARYLOGIC_CASE(0x35) \
    SIMDE_X_TERNARYLOGIC_CASE(0x36) \
    SIMDE_X_TERNARYLOGIC_CASE(0x37) \
    SIMDE_X_TERNARYLOGIC_CASE(0x38) \
    SIMDE_X_TERNARYLOGIC_CASE(0x39) \
    SIMDE_X_TERNARYLOGIC_CASE(0x3a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x3b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x3c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x3d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x3e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x3f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x40) \
    SIMDE_X_TERNARYLOGIC_CASE(0x41) \
    SIMDE_X_TERNARYLOGIC_CASE(0x42) \
    SIMDE_X_TERNARYLOGIC_CASE(0x43) \
    SIMDE_X_TERNARYLOGIC_CASE(0x44) \
    SIMDE_X_TERNARYLOGIC_CASE(0x45) \
    SIMDE_X_TERNARYLOGIC_CASE(0x46) \
    SIMDE_X_TERNARYLOGIC_CASE(0x47) \
    SIMDE_X_TERNARYLOGIC_CASE(0x48) \
    SIMDE_X_TERNARYLOGIC_CASE(0x49) \
    SIMDE_X_TERNARYLOGIC_CASE(0x4a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x4b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x4c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x4d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x4e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x4f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x50) \
    SIMDE_X_TERNARYLOGIC_CASE(0x51) \
    SIMDE_X_TERNARYLOGIC_CASE(0x52) \
    SIMDE_X_TERNARYLOGIC_CASE(0x53) \
    SIMDE_X_TERNARYLOGIC_CASE(0x54) \
    SIMDE_X_TERNARYLOGIC_CASE(0x55) \
    SIMDE_X_TERNARYLOGIC_CASE(0x56) \
    SIMDE_X_TERNARYLOGIC_CASE(0x57) \
    SIMDE_X_TERNARYLOGIC_CASE(0x58) \
    SIMDE_X_TERNARYLOGIC_CASE(0x59) \
    SIMDE_X_TERNARYLOGIC_CASE(0x5a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x5b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x5c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x5d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x5e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x5f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x60) \
    SIMDE_X_TERNARYLOGIC_CASE(0x61) \
    SIMDE_X_TERNARYLOGIC_CASE(0x62) \
    SIMDE_X_TERNARYLOGIC_CASE(0x63) \
    SIMDE_X_TERNARYLOGIC_CASE(0x64) \
    SIMDE_X_TERNARYLOGIC_CASE(0x65) \
    SIMDE_X_TERNARYLOGIC_CASE(0x66) \
    SIMDE_X_TERNARYLOGIC_CASE(0x67) \
    SIMDE_X_TERNARYLOGIC_CASE(0x68) \
    SIMDE_X_TERNARYLOGIC_CASE(0x69) \
    SIMDE_X_TERNARYLOGIC_CASE(0x6a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x6b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x6c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x6d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x6e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x6f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x70) \
    SIMDE_X_TERNARYLOGIC_CASE(0x71) \
    SIMDE_X_TERNARYLOGIC_CASE(0x72) \
    SIMDE_X_TERNARYLOGIC_CASE(0x73) \
    SIMDE_X_TERNARYLOGIC_CASE(0x74) \
    SIMDE_X_TERNARYLOGIC_CASE(0x75) \
    SIMDE_X_TERNARYLOGIC_CASE(0x76) \
    SIMDE_X_TERNARYLOGIC_CASE(0x77) \
    SIMDE_X_TERNARYLOGIC_CASE(0x78) \
    SIMDE_X_TERNARYLOGIC_CASE(0x79) \
    SIMDE_X_TERNARYLOGIC_CASE(0x7a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x7b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x7c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x7d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x7e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x7f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x80) \
    SIMDE_X_TERNARYLOGIC_CASE(0x81) \
    SIMDE_X_TERNARYLOGIC_CASE(0x82) \
    SIMDE_X_TERNARYLOGIC_CASE(0x83) \
    SIMDE_X_TERNARYLOGIC_CASE(0x84) \
    SIMDE_X_TERNARYLOGIC_CASE(0x85) \
    SIMDE_X_TERNARYLOGIC_CASE(0x86) \
    SIMDE_X_TERNARYLOGIC_CASE(0x87) \
    SIMDE_X_TERNARYLOGIC_CASE(0x88) \
    SIMDE_X_TERNARYLOGIC_CASE(0x89) \
    SIMDE_X_TERNARYLOGIC_CASE(0x8a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x8b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x8c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x8d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x8e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x8f) \
    SIMDE_X_TERNARYLOGIC_CASE(0x90) \
    SIMDE_X_TERNARYLOGIC_CASE(0x91) \
    SIMDE_X_TERNARYLOGIC_CASE(0x92) \
    SIMDE_X_TERNARYLOGIC_CASE(0x93) \
    SIMDE_X_TERNARYLOGIC_CASE(0x94) \
    SIMDE_X_TERNARYLOGIC_CASE(0x95) \
    SIMDE_X_TERNARYLOGIC_CASE(0x96) \
    SIMDE_X_TERNARYLOGIC_CASE(0x97) \
    SIMDE_X_TERNARYLOGIC_CASE(0x98) \
    SIMDE_X_TERNARYLOGIC_CASE(0x99) \
    SIMDE_X_TERNARYLOGIC_CASE(0x9a) \
    SIMDE_X_TERNARYLOGIC_CASE(0x9b) \
    SIMDE_X_TERNARYLOGIC_CASE(0x9c) \
    SIMDE_X_TERNARYLOGIC_CASE(0x9d) \
    SIMDE_X_TERNARYLOGIC_CASE(0x9e) \
    SIMDE_X_TERNARYLOGIC_CASE(0x9f) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa0) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa1) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa2) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa3) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa4) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa5) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa6) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa7) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa8) \
    SIMDE_X_TERNARYLOGIC_CASE(0xa9) \
    SIMDE_X_TERNARYLOGIC_CASE(0xaa) \
    SIMDE_X_TERNARYLOGIC_CASE(0xab) \
    SIMDE_X_TERNARYLOGIC_CASE(0xac) \
    SIMDE_X_TERNARYLOGIC_CASE(0xad) \
    SIMDE_X_TERNARYLOGIC_CASE(0xae) \
    SIMDE_X_TERNARYLOGIC_CASE(0xaf) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb0) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb1) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb2) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb3) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb4) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb5) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb6) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb7) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb8) \
    SIMDE_X_TERNARYLOGIC_CASE(0xb9) \
    SIMDE_X_TERNARYLOGIC_CASE(0xba) \
    SIMDE_X_TERNARYLOGIC_CASE(0xbb) \
    SIMDE_X_TERNARYLOGIC_CASE(0xbc) \
    SIMDE_X_TERNARYLOGIC_CASE(0xbd) \
    SIMDE_X_TERNARYLOGIC_CASE(0xbe) \
    SIMDE_X_TERNARYLOGIC_CASE(0xbf) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc0) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc1) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc2) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc3) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc4) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc5) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc6) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc7) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc8) \
    SIMDE_X_TERNARYLOGIC_CASE(0xc9) \
    SIMDE_X_TERNARYLOGIC_CASE(0xca) \
    SIMDE_X_TERNARYLOGIC_CASE(0xcb) \
    SIMDE_X_TERNARYLOGIC_CASE(0xcc) \
    SIMDE_X_TERNARYLOGIC_CASE(0xcd) \
    SIMDE_X_TERNARYLOGIC_CASE(0xce) \
    SIMDE_X_TERNARYLOGIC_CASE(0xcf) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd0) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd1) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd2) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd3) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd4) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd5) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd6) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd7) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd8) \
    SIMDE_X_TERNARYLOGIC_CASE(0xd9) \
    SIMDE_X_TERNARYLOGIC_CASE(0xda) \
    SIMDE_X_TERNARYLOGIC_CASE(0xdb) \
    SIMDE_X_TERNARYLOGIC_CASE(0xdc) \
    SIMDE_X_TERNARYLOGIC_CASE(0xdd) \
    SIMDE_X_TERNARYLOGIC_CASE(0xde) \
    SIMDE_X_TERNARYLOGIC_CASE(0xdf) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe0) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe1) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe2) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe3) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe4) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe5) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe6) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe7) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe8) \
    SIMDE_X_TERNARYLOGIC_CASE(0xe9) \
    SIMDE_X_TERNARYLOGIC_CASE(0xea) \
    SIMDE_X_TERNARYLOGIC_CASE(0xeb) \
    SIMDE_X_TERNARYLOGIC_CASE(0xec) \
    SIMDE_X_TERNARYLOGIC_CASE(0xed) \
    SIMDE_X_TERNARYLOGIC_CASE(0xee) \
    SIMDE_X_TERNARYLOGIC_CASE(0xef) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf0) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf1) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf2) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf3) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf4) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf5) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf6) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf7) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf8) \
    SIMDE_X_TERNARYLOGIC_CASE(0xf9) \
    SIMDE_X_TERNARYLOGIC_CASE(0xfa) \
    SIMDE_X_TERNARYLOGIC_CASE(0xfb) \
    SIMDE_X_TERNARYLOGIC_CASE(0xfc) \
    SIMDE_X_TERNARYLOGIC_CASE(0xfd) \
    SIMDE_X_TERNARYLOGIC_CASE(0xfe) \
    SIMDE_X_TERNARYLOGIC_CASE(0xff) \
  }

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_ternarylogic_epi32(a, b, c, imm8) _mm_ternarylogic_epi32(a, b, c, imm8)
#else
  SIMDE_HUGE_FUNCTION_ATTRIBUTES
  simde__m128i
  simde_mm_ternarylogic_epi32(simde__m128i a, simde__m128i b, simde__m128i c, int imm8)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255) {
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a),
      b_ = simde__m128i_to_private(b),
      c_ = simde__m128i_to_private(c);

    #if defined(SIMDE_TERNARYLOGIC_COMPRESSION)
      int to_do, mask;
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        simde__m128i_private t_;
        to_do = imm8;

        r_.u64 = a_.u64 ^ a_.u64;

        mask = 0xFF;
        if ((to_do & mask) == mask) {
          r_.u64 = ~r_.u64;
          to_do &= ~mask;
        }

        mask = 0xF0;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 = a_.u64;
          to_do &= ~mask;
        }

        mask = 0xCC;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64;
          to_do &= ~mask;
        }

        mask = 0xAA;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= c_.u64;
          to_do &= ~mask;
        }

        mask = 0x0F;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~a_.u64;
          to_do &= ~mask;
        }

        mask = 0x33;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~b_.u64;
          to_do &= ~mask;
        }

        mask = 0x55;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64;
          to_do &= ~mask;
        }

        mask = 0x3C;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 ^ b_.u64;
          to_do &= ~mask;
        }

        mask = 0x5A;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 ^ c_.u64;
          to_do &= ~mask;
        }

        mask = 0x66;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64 ^ c_.u64;
          to_do &= ~mask;
        }

        mask = 0xA0;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x50;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64 & a_.u64;
          to_do &= ~mask;
        }

        mask = 0x0A;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~a_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x88;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x44;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64 & b_.u64;
          to_do &= ~mask;
        }

        mask = 0x22;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~b_.u64 & c_.u64;
          to_do &= ~mask;
        }

        if (to_do & 0xc0) {
          t_.u64 = a_.u64 & b_.u64;
          if ((to_do & 0xc0) == 0xc0) r_.u64 |= t_.u64;
          else if (to_do & 0x80)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x30) {
          t_.u64 = ~b_.u64 & a_.u64;
          if ((to_do & 0x30) == 0x30) r_.u64 |= t_.u64;
          else if (to_do & 0x20)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x0c) {
          t_.u64 = ~a_.u64 & b_.u64;
          if ((to_do & 0x0c) == 0x0c) r_.u64 |= t_.u64;
          else if (to_do & 0x08)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x03) {
          t_.u64 = ~(a_.u64 | b_.u64);
          if ((to_do & 0x03) == 0x03) r_.u64 |= t_.u64;
          else if (to_do & 0x02)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }
      #else
        uint64_t t;

        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
          to_do = imm8;

          mask = 0xFF;
          if ((to_do & mask) == mask) {
            r_.u64[i] = UINT64_MAX;
            to_do &= ~mask;
          }
          else r_.u64[i] = 0;

          mask = 0xF0;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] = a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xCC;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xAA;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x0F;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x33;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x55;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x3C;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] ^ b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x5A;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] ^ c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x66;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i] ^ c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xA0;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x50;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i] & a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x0A;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~a_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x88;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x44;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i] & b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x22;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~b_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          if (to_do & 0xc0) {
            t = a_.u64[i] & b_.u64[i];
            if ((to_do & 0xc0) == 0xc0) r_.u64[i] |= t;
            else if (to_do & 0x80)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x30) {
            t = ~b_.u64[i] & a_.u64[i];
            if ((to_do & 0x30) == 0x30) r_.u64[i] |= t;
            else if (to_do & 0x20)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x0c) {
            t = ~a_.u64[i] & b_.u64[i];
            if ((to_do & 0x0c) == 0x0c) r_.u64[i] |= t;
            else if (to_do & 0x08)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x03) {
            t = ~(a_.u64[i] | b_.u64[i]);
            if ((to_do & 0x03) == 0x03) r_.u64[i] |= t;
            else if (to_do & 0x02)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }
        }
      #endif
    #else
      SIMDE_X_TERNARYLOGIC_SWITCH(imm8 & 255)
    #endif

    return simde__m128i_from_private(r_);
  }
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_ternarylogic_epi32
  #define _mm_ternarylogic_epi32(a, b, c, imm8) simde_mm_ternarylogic_epi32(a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_mask_ternarylogic_epi32(src, k, a, b, imm8) _mm_mask_ternarylogic_epi32(src, k, a, b, imm8)
#else
  #define simde_mm_mask_ternarylogic_epi32(src, k, a, b, imm8) simde_mm_mask_mov_epi32(src, k, simde_mm_ternarylogic_epi32(src, a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_ternarylogic_epi32
  #define _mm_mask_ternarylogic_epi32(src, k, a, b, imm8) simde_mm_mask_ternarylogic_epi32(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm_maskz_ternarylogic_epi32(k, a, b, c, imm8) _mm_maskz_ternarylogic_epi32(k, a, b, c, imm8)
#else
  #define simde_mm_maskz_ternarylogic_epi32(k, a, b, c, imm8) simde_mm_maskz_mov_epi32(k, simde_mm_ternarylogic_epi32(a, b, c, imm8))
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_ternarylogic_epi32
  #define _mm_maskz_ternarylogic_epi32(k, a, b, c, imm8) simde_mm_maskz_ternarylogic_epi32(k, a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm256_ternarylogic_epi32(a, b, c, imm8) _mm256_ternarylogic_epi32(a, b, c, imm8)
#else
  SIMDE_HUGE_FUNCTION_ATTRIBUTES
  simde__m256i
  simde_mm256_ternarylogic_epi32(simde__m256i a, simde__m256i b, simde__m256i c, int imm8)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255) {
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a),
      b_ = simde__m256i_to_private(b),
      c_ = simde__m256i_to_private(c);

    #if defined(SIMDE_TERNARYLOGIC_COMPRESSION)
      int to_do, mask;
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        simde__m256i_private t_;
        to_do = imm8;

        r_.u64 = a_.u64 ^ a_.u64;

        mask = 0xFF;
        if ((to_do & mask) == mask) {
          r_.u64 = ~r_.u64;
          to_do &= ~mask;
        }

        mask = 0xF0;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 = a_.u64;
          to_do &= ~mask;
        }

        mask = 0xCC;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64;
          to_do &= ~mask;
        }

        mask = 0xAA;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= c_.u64;
          to_do &= ~mask;
        }

        mask = 0x0F;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~a_.u64;
          to_do &= ~mask;
        }

        mask = 0x33;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~b_.u64;
          to_do &= ~mask;
        }

        mask = 0x55;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64;
          to_do &= ~mask;
        }

        mask = 0x3C;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 ^ b_.u64;
          to_do &= ~mask;
        }

        mask = 0x5A;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 ^ c_.u64;
          to_do &= ~mask;
        }

        mask = 0x66;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64 ^ c_.u64;
          to_do &= ~mask;
        }

        mask = 0xA0;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x50;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64 & a_.u64;
          to_do &= ~mask;
        }

        mask = 0x0A;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~a_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x88;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x44;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64 & b_.u64;
          to_do &= ~mask;
        }

        mask = 0x22;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~b_.u64 & c_.u64;
          to_do &= ~mask;
        }

        if (to_do & 0xc0) {
          t_.u64 = a_.u64 & b_.u64;
          if ((to_do & 0xc0) == 0xc0) r_.u64 |= t_.u64;
          else if (to_do & 0x80)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x30) {
          t_.u64 = ~b_.u64 & a_.u64;
          if ((to_do & 0x30) == 0x30) r_.u64 |= t_.u64;
          else if (to_do & 0x20)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x0c) {
          t_.u64 = ~a_.u64 & b_.u64;
          if ((to_do & 0x0c) == 0x0c) r_.u64 |= t_.u64;
          else if (to_do & 0x08)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x03) {
          t_.u64 = ~(a_.u64 | b_.u64);
          if ((to_do & 0x03) == 0x03) r_.u64 |= t_.u64;
          else if (to_do & 0x02)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }
      #else
        uint64_t t;

        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
          to_do = imm8;

          mask = 0xFF;
          if ((to_do & mask) == mask) {
            r_.u64[i] = UINT64_MAX;
            to_do &= ~mask;
          }
          else r_.u64[i] = 0;

          mask = 0xF0;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] = a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xCC;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xAA;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x0F;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x33;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x55;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x3C;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] ^ b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x5A;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] ^ c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x66;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i] ^ c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xA0;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x50;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i] & a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x0A;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~a_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x88;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x44;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i] & b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x22;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~b_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          if (to_do & 0xc0) {
            t = a_.u64[i] & b_.u64[i];
            if ((to_do & 0xc0) == 0xc0) r_.u64[i] |= t;
            else if (to_do & 0x80)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x30) {
            t = ~b_.u64[i] & a_.u64[i];
            if ((to_do & 0x30) == 0x30) r_.u64[i] |= t;
            else if (to_do & 0x20)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x0c) {
            t = ~a_.u64[i] & b_.u64[i];
            if ((to_do & 0x0c) == 0x0c) r_.u64[i] |= t;
            else if (to_do & 0x08)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x03) {
            t = ~(a_.u64[i] | b_.u64[i]);
            if ((to_do & 0x03) == 0x03) r_.u64[i] |= t;
            else if (to_do & 0x02)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }
        }
      #endif
    #else
      SIMDE_X_TERNARYLOGIC_SWITCH(imm8 & 255)
    #endif

    return simde__m256i_from_private(r_);
  }
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_ternarylogic_epi32
  #define _mm256_ternarylogic_epi32(a, b, c, imm8) simde_mm256_ternarylogic_epi32(a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm256_mask_ternarylogic_epi32(src, k, a, b, imm8) _mm256_mask_ternarylogic_epi32(src, k, a, b, imm8)
#else
  #define simde_mm256_mask_ternarylogic_epi32(src, k, a, b, imm8) simde_mm256_mask_mov_epi32(src, k, simde_mm256_ternarylogic_epi32(src, a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_ternarylogic_epi32
  #define _mm256_mask_ternarylogic_epi32(src, k, a, b, imm8) simde_mm256_mask_ternarylogic_epi32(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm256_maskz_ternarylogic_epi32(k, a, b, c, imm8) _mm256_maskz_ternarylogic_epi32(k, a, b, c, imm8)
#else
  #define simde_mm256_maskz_ternarylogic_epi32(k, a, b, c, imm8) simde_mm256_maskz_mov_epi32(k, simde_mm256_ternarylogic_epi32(a, b, c, imm8))
#endif
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_ternarylogic_epi32
  #define _mm256_maskz_ternarylogic_epi32(k, a, b, c, imm8) simde_mm256_maskz_ternarylogic_epi32(k, a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_ternarylogic_epi32(a, b, c, imm8) _mm512_ternarylogic_epi32(a, b, c, imm8)
#else
  SIMDE_HUGE_FUNCTION_ATTRIBUTES
  simde__m512i
  simde_mm512_ternarylogic_epi32(simde__m512i a, simde__m512i b, simde__m512i c, int imm8)
      SIMDE_REQUIRE_CONSTANT_RANGE(imm8, 0, 255) {
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a),
      b_ = simde__m512i_to_private(b),
      c_ = simde__m512i_to_private(c);

    #if defined(SIMDE_TERNARYLOGIC_COMPRESSION)
      int to_do, mask;
      #if defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
        simde__m512i_private t_;
        to_do = imm8;

        r_.u64 = a_.u64 ^ a_.u64;

        mask = 0xFF;
        if ((to_do & mask) == mask) {
          r_.u64 = ~r_.u64;
          to_do &= ~mask;
        }

        mask = 0xF0;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 = a_.u64;
          to_do &= ~mask;
        }

        mask = 0xCC;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64;
          to_do &= ~mask;
        }

        mask = 0xAA;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= c_.u64;
          to_do &= ~mask;
        }

        mask = 0x0F;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~a_.u64;
          to_do &= ~mask;
        }

        mask = 0x33;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~b_.u64;
          to_do &= ~mask;
        }

        mask = 0x55;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64;
          to_do &= ~mask;
        }

        mask = 0x3C;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 ^ b_.u64;
          to_do &= ~mask;
        }

        mask = 0x5A;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 ^ c_.u64;
          to_do &= ~mask;
        }

        mask = 0x66;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64 ^ c_.u64;
          to_do &= ~mask;
        }

        mask = 0xA0;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= a_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x50;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64 & a_.u64;
          to_do &= ~mask;
        }

        mask = 0x0A;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~a_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x88;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= b_.u64 & c_.u64;
          to_do &= ~mask;
        }

        mask = 0x44;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~c_.u64 & b_.u64;
          to_do &= ~mask;
        }

        mask = 0x22;
        if ((to_do & mask) && ((imm8 & mask) == mask)) {
          r_.u64 |= ~b_.u64 & c_.u64;
          to_do &= ~mask;
        }

        if (to_do & 0xc0) {
          t_.u64 = a_.u64 & b_.u64;
          if ((to_do & 0xc0) == 0xc0) r_.u64 |= t_.u64;
          else if (to_do & 0x80)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x30) {
          t_.u64 = ~b_.u64 & a_.u64;
          if ((to_do & 0x30) == 0x30) r_.u64 |= t_.u64;
          else if (to_do & 0x20)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x0c) {
          t_.u64 = ~a_.u64 & b_.u64;
          if ((to_do & 0x0c) == 0x0c) r_.u64 |= t_.u64;
          else if (to_do & 0x08)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }

        if (to_do & 0x03) {
          t_.u64 = ~(a_.u64 | b_.u64);
          if ((to_do & 0x03) == 0x03) r_.u64 |= t_.u64;
          else if (to_do & 0x02)      r_.u64 |=  c_.u64 & t_.u64;
          else                        r_.u64 |= ~c_.u64 & t_.u64;
        }
      #else
        uint64_t t;

        SIMDE_VECTORIZE
        for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
          to_do = imm8;

          mask = 0xFF;
          if ((to_do & mask) == mask) {
            r_.u64[i] = UINT64_MAX;
            to_do &= ~mask;
          }
          else r_.u64[i] = 0;

          mask = 0xF0;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] = a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xCC;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xAA;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x0F;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x33;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x55;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x3C;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] ^ b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x5A;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] ^ c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x66;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i] ^ c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0xA0;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= a_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x50;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i] & a_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x0A;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~a_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x88;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= b_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x44;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~c_.u64[i] & b_.u64[i];
            to_do &= ~mask;
          }

          mask = 0x22;
          if ((to_do & mask) && ((imm8 & mask) == mask)) {
            r_.u64[i] |= ~b_.u64[i] & c_.u64[i];
            to_do &= ~mask;
          }

          if (to_do & 0xc0) {
            t = a_.u64[i] & b_.u64[i];
            if ((to_do & 0xc0) == 0xc0) r_.u64[i] |= t;
            else if (to_do & 0x80)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x30) {
            t = ~b_.u64[i] & a_.u64[i];
            if ((to_do & 0x30) == 0x30) r_.u64[i] |= t;
            else if (to_do & 0x20)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x0c) {
            t = ~a_.u64[i] & b_.u64[i];
            if ((to_do & 0x0c) == 0x0c) r_.u64[i] |= t;
            else if (to_do & 0x08)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }

          if (to_do & 0x03) {
            t = ~(a_.u64[i] | b_.u64[i]);
            if ((to_do & 0x03) == 0x03) r_.u64[i] |= t;
            else if (to_do & 0x02)      r_.u64[i] |=  c_.u64[i] & t;
            else                        r_.u64[i] |= ~c_.u64[i] & t;
          }
        }
      #endif
    #else
      SIMDE_X_TERNARYLOGIC_SWITCH(imm8 & 255)
    #endif

    return simde__m512i_from_private(r_);
  }
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_ternarylogic_epi32
  #define _mm512_ternarylogic_epi32(a, b, c, imm8) simde_mm512_ternarylogic_epi32(a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_mask_ternarylogic_epi32(src, k, a, b, imm8) _mm512_mask_ternarylogic_epi32(src, k, a, b, imm8)
#else
  #define simde_mm512_mask_ternarylogic_epi32(src, k, a, b, imm8) simde_mm512_mask_mov_epi32(src, k, simde_mm512_ternarylogic_epi32(src, a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_ternarylogic_epi32
  #define _mm512_mask_ternarylogic_epi32(src, k, a, b, imm8) simde_mm512_mask_ternarylogic_epi32(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_maskz_ternarylogic_epi32(k, a, b, c, imm8) _mm512_maskz_ternarylogic_epi32(k, a, b, c, imm8)
#else
  #define simde_mm512_maskz_ternarylogic_epi32(k, a, b, c, imm8) simde_mm512_maskz_mov_epi32(k, simde_mm512_ternarylogic_epi32(a, b, c, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_ternarylogic_epi32
  #define _mm512_maskz_ternarylogic_epi32(k, a, b, c, imm8) simde_mm512_maskz_ternarylogic_epi32(k, a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_ternarylogic_epi64(a, b, c, imm8) _mm_ternarylogic_epi64(a, b, c, imm8)
#else
  #define simde_mm_ternarylogic_epi64(a, b, c, imm8) simde_mm_ternarylogic_epi32(a, b, c, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_ternarylogic_epi64
  #define _mm_ternarylogic_epi64(a, b, c, imm8) simde_mm_ternarylogic_epi64(a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_mask_ternarylogic_epi64(src, k, a, b, imm8) _mm_mask_ternarylogic_epi64(src, k, a, b, imm8)
#else
  #define simde_mm_mask_ternarylogic_epi64(src, k, a, b, imm8) simde_mm_mask_mov_epi64(src, k, simde_mm_ternarylogic_epi64(src, a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_ternarylogic_epi64
  #define _mm_mask_ternarylogic_epi64(src, k, a, b, imm8) simde_mm_mask_ternarylogic_epi64(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm_maskz_ternarylogic_epi64(k, a, b, c, imm8) _mm_maskz_ternarylogic_epi64(k, a, b, c, imm8)
#else
  #define simde_mm_maskz_ternarylogic_epi64(k, a, b, c, imm8) simde_mm_maskz_mov_epi64(k, simde_mm_ternarylogic_epi64(a, b, c, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_ternarylogic_epi64
  #define _mm_maskz_ternarylogic_epi64(k, a, b, c, imm8) simde_mm_maskz_ternarylogic_epi64(k, a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_ternarylogic_epi64(a, b, c, imm8) _mm256_ternarylogic_epi64(a, b, c, imm8)
#else
  #define simde_mm256_ternarylogic_epi64(a, b, c, imm8) simde_mm256_ternarylogic_epi32(a, b, c, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_ternarylogic_epi64
  #define _mm256_ternarylogic_epi64(a, b, c, imm8) simde_mm256_ternarylogic_epi64(a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_mask_ternarylogic_epi64(src, k, a, b, imm8) _mm256_mask_ternarylogic_epi64(src, k, a, b, imm8)
#else
  #define simde_mm256_mask_ternarylogic_epi64(src, k, a, b, imm8) simde_mm256_mask_mov_epi64(src, k, simde_mm256_ternarylogic_epi64(src, a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_ternarylogic_epi64
  #define _mm256_mask_ternarylogic_epi64(src, k, a, b, imm8) simde_mm256_mask_ternarylogic_epi64(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
  #define simde_mm256_maskz_ternarylogic_epi64(k, a, b, c, imm8) _mm256_maskz_ternarylogic_epi64(k, a, b, c, imm8)
#else
  #define simde_mm256_maskz_ternarylogic_epi64(k, a, b, c, imm8) simde_mm256_maskz_mov_epi64(k, simde_mm256_ternarylogic_epi64(a, b, c, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_ternarylogic_epi64
  #define _mm256_maskz_ternarylogic_epi64(k, a, b, c, imm8) simde_mm256_maskz_ternarylogic_epi64(k, a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_ternarylogic_epi64(a, b, c, imm8) _mm512_ternarylogic_epi64(a, b, c, imm8)
#else
  #define simde_mm512_ternarylogic_epi64(a, b, c, imm8) simde_mm512_ternarylogic_epi32(a, b, c, imm8)
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_ternarylogic_epi64
  #define _mm512_ternarylogic_epi64(a, b, c, imm8) simde_mm512_ternarylogic_epi64(a, b, c, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_mask_ternarylogic_epi64(src, k, a, b, imm8) _mm512_mask_ternarylogic_epi64(src, k, a, b, imm8)
#else
  #define simde_mm512_mask_ternarylogic_epi64(src, k, a, b, imm8) simde_mm512_mask_mov_epi64(src, k, simde_mm512_ternarylogic_epi64(src, a, b, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_ternarylogic_epi64
  #define _mm512_mask_ternarylogic_epi64(src, k, a, b, imm8) simde_mm512_mask_ternarylogic_epi64(src, k, a, b, imm8)
#endif

#if defined(SIMDE_X86_AVX512F_NATIVE)
  #define simde_mm512_maskz_ternarylogic_epi64(k, a, b, c, imm8) _mm512_maskz_ternarylogic_epi64(k, a, b, c, imm8)
#else
  #define simde_mm512_maskz_ternarylogic_epi64(k, a, b, c, imm8) simde_mm512_maskz_mov_epi64(k, simde_mm512_ternarylogic_epi64(a, b, c, imm8))
#endif
#if defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_ternarylogic_epi64
  #define _mm512_maskz_ternarylogic_epi64(k, a, b, c, imm8) simde_mm512_maskz_ternarylogic_epi64(k, a, b, c, imm8)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_TERNARYLOGIC_H) */
