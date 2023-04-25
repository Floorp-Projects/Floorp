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
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_ARM_SVE_REINTERPRET_H)
#define SIMDE_ARM_SVE_REINTERPRET_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

#if defined(SIMDE_ARM_SVE_NATIVE)
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_s16(   simde_svint16_t op) { return   svreinterpret_s8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_s32(   simde_svint32_t op) { return   svreinterpret_s8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_s64(   simde_svint64_t op) { return   svreinterpret_s8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t    simde_svreinterpret_s8_u8(   simde_svuint8_t op) { return    svreinterpret_s8_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_u16(  simde_svuint16_t op) { return   svreinterpret_s8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_u32(  simde_svuint32_t op) { return   svreinterpret_s8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_u64(  simde_svuint64_t op) { return   svreinterpret_s8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_f16( simde_svfloat16_t op) { return   svreinterpret_s8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_f32( simde_svfloat32_t op) { return   svreinterpret_s8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8_f64( simde_svfloat64_t op) { return   svreinterpret_s8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t   simde_svreinterpret_s16_s8(    simde_svint8_t op) { return   svreinterpret_s16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_s32(   simde_svint32_t op) { return  svreinterpret_s16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_s64(   simde_svint64_t op) { return  svreinterpret_s16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t   simde_svreinterpret_s16_u8(   simde_svuint8_t op) { return   svreinterpret_s16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_u16(  simde_svuint16_t op) { return  svreinterpret_s16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_u32(  simde_svuint32_t op) { return  svreinterpret_s16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_u64(  simde_svuint64_t op) { return  svreinterpret_s16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_f16( simde_svfloat16_t op) { return  svreinterpret_s16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_f32( simde_svfloat32_t op) { return  svreinterpret_s16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16_f64( simde_svfloat64_t op) { return  svreinterpret_s16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t   simde_svreinterpret_s32_s8(    simde_svint8_t op) { return   svreinterpret_s32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_s16(   simde_svint16_t op) { return  svreinterpret_s32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_s64(   simde_svint64_t op) { return  svreinterpret_s32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t   simde_svreinterpret_s32_u8(   simde_svuint8_t op) { return   svreinterpret_s32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_u16(  simde_svuint16_t op) { return  svreinterpret_s32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_u32(  simde_svuint32_t op) { return  svreinterpret_s32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_u64(  simde_svuint64_t op) { return  svreinterpret_s32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_f16( simde_svfloat16_t op) { return  svreinterpret_s32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_f32( simde_svfloat32_t op) { return  svreinterpret_s32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32_f64( simde_svfloat64_t op) { return  svreinterpret_s32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t   simde_svreinterpret_s64_s8(    simde_svint8_t op) { return   svreinterpret_s64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_s16(   simde_svint16_t op) { return  svreinterpret_s64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_s32(   simde_svint32_t op) { return  svreinterpret_s64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t   simde_svreinterpret_s64_u8(   simde_svuint8_t op) { return   svreinterpret_s64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_u16(  simde_svuint16_t op) { return  svreinterpret_s64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_u32(  simde_svuint32_t op) { return  svreinterpret_s64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_u64(  simde_svuint64_t op) { return  svreinterpret_s64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_f16( simde_svfloat16_t op) { return  svreinterpret_s64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_f32( simde_svfloat32_t op) { return  svreinterpret_s64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64_f64( simde_svfloat64_t op) { return  svreinterpret_s64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t    simde_svreinterpret_u8_s8(    simde_svint8_t op) { return    svreinterpret_u8_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_s16(   simde_svint16_t op) { return   svreinterpret_u8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_s32(   simde_svint32_t op) { return   svreinterpret_u8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_s64(   simde_svint64_t op) { return   svreinterpret_u8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_u16(  simde_svuint16_t op) { return   svreinterpret_u8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_u32(  simde_svuint32_t op) { return   svreinterpret_u8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_u64(  simde_svuint64_t op) { return   svreinterpret_u8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_f16( simde_svfloat16_t op) { return   svreinterpret_u8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_f32( simde_svfloat32_t op) { return   svreinterpret_u8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8_f64( simde_svfloat64_t op) { return   svreinterpret_u8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t   simde_svreinterpret_u16_s8(    simde_svint8_t op) { return   svreinterpret_u16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_s16(   simde_svint16_t op) { return  svreinterpret_u16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_s32(   simde_svint32_t op) { return  svreinterpret_u16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_s64(   simde_svint64_t op) { return  svreinterpret_u16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t   simde_svreinterpret_u16_u8(   simde_svuint8_t op) { return   svreinterpret_u16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_u32(  simde_svuint32_t op) { return  svreinterpret_u16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_u64(  simde_svuint64_t op) { return  svreinterpret_u16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_f16( simde_svfloat16_t op) { return  svreinterpret_u16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_f32( simde_svfloat32_t op) { return  svreinterpret_u16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16_f64( simde_svfloat64_t op) { return  svreinterpret_u16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t   simde_svreinterpret_u32_s8(    simde_svint8_t op) { return   svreinterpret_u32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_s16(   simde_svint16_t op) { return  svreinterpret_u32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_s32(   simde_svint32_t op) { return  svreinterpret_u32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_s64(   simde_svint64_t op) { return  svreinterpret_u32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t   simde_svreinterpret_u32_u8(   simde_svuint8_t op) { return   svreinterpret_u32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_u16(  simde_svuint16_t op) { return  svreinterpret_u32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_u64(  simde_svuint64_t op) { return  svreinterpret_u32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_f16( simde_svfloat16_t op) { return  svreinterpret_u32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_f32( simde_svfloat32_t op) { return  svreinterpret_u32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32_f64( simde_svfloat64_t op) { return  svreinterpret_u32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t   simde_svreinterpret_u64_s8(    simde_svint8_t op) { return   svreinterpret_u64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_s16(   simde_svint16_t op) { return  svreinterpret_u64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_s32(   simde_svint32_t op) { return  svreinterpret_u64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_s64(   simde_svint64_t op) { return  svreinterpret_u64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t   simde_svreinterpret_u64_u8(   simde_svuint8_t op) { return   svreinterpret_u64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_u16(  simde_svuint16_t op) { return  svreinterpret_u64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_u32(  simde_svuint32_t op) { return  svreinterpret_u64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_f16( simde_svfloat16_t op) { return  svreinterpret_u64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_f32( simde_svfloat32_t op) { return  svreinterpret_u64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64_f64( simde_svfloat64_t op) { return  svreinterpret_u64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t   simde_svreinterpret_f16_s8(    simde_svint8_t op) { return   svreinterpret_f16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_s16(   simde_svint16_t op) { return  svreinterpret_f16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_s32(   simde_svint32_t op) { return  svreinterpret_f16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_s64(   simde_svint64_t op) { return  svreinterpret_f16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t   simde_svreinterpret_f16_u8(   simde_svuint8_t op) { return   svreinterpret_f16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_u16(  simde_svuint16_t op) { return  svreinterpret_f16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_u32(  simde_svuint32_t op) { return  svreinterpret_f16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_u64(  simde_svuint64_t op) { return  svreinterpret_f16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_f32( simde_svfloat32_t op) { return  svreinterpret_f16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16_f64( simde_svfloat64_t op) { return  svreinterpret_f16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t   simde_svreinterpret_f32_s8(    simde_svint8_t op) { return   svreinterpret_f32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_s16(   simde_svint16_t op) { return  svreinterpret_f32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_s32(   simde_svint32_t op) { return  svreinterpret_f32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_s64(   simde_svint64_t op) { return  svreinterpret_f32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t   simde_svreinterpret_f32_u8(   simde_svuint8_t op) { return   svreinterpret_f32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_u16(  simde_svuint16_t op) { return  svreinterpret_f32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_u32(  simde_svuint32_t op) { return  svreinterpret_f32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_u64(  simde_svuint64_t op) { return  svreinterpret_f32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_f16( simde_svfloat16_t op) { return  svreinterpret_f32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32_f64( simde_svfloat64_t op) { return  svreinterpret_f32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t   simde_svreinterpret_f64_s8(    simde_svint8_t op) { return   svreinterpret_f64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_s16(   simde_svint16_t op) { return  svreinterpret_f64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_s32(   simde_svint32_t op) { return  svreinterpret_f64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_s64(   simde_svint64_t op) { return  svreinterpret_f64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t   simde_svreinterpret_f64_u8(   simde_svuint8_t op) { return   svreinterpret_f64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_u16(  simde_svuint16_t op) { return  svreinterpret_f64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_u32(  simde_svuint32_t op) { return  svreinterpret_f64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_u64(  simde_svuint64_t op) { return  svreinterpret_f64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_f16( simde_svfloat16_t op) { return  svreinterpret_f64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64_f32( simde_svfloat32_t op) { return  svreinterpret_f64_f32(op); }
#else
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_s16,     simde_svint8_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_s32,     simde_svint8_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_s64,     simde_svint8_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(   simde_svreinterpret_s8_u8,     simde_svint8_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_u16,     simde_svint8_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_u32,     simde_svint8_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_u64,     simde_svint8_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_f16,     simde_svint8_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_f32,     simde_svint8_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s8_f64,     simde_svint8_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s16_s8,    simde_svint16_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_s32,    simde_svint16_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_s64,    simde_svint16_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s16_u8,    simde_svint16_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_u16,    simde_svint16_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_u32,    simde_svint16_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_u64,    simde_svint16_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_f16,    simde_svint16_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_f32,    simde_svint16_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s16_f64,    simde_svint16_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s32_s8,    simde_svint32_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_s16,    simde_svint32_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_s64,    simde_svint32_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s32_u8,    simde_svint32_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_u16,    simde_svint32_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_u32,    simde_svint32_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_u64,    simde_svint32_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_f16,    simde_svint32_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_f32,    simde_svint32_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s32_f64,    simde_svint32_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s64_s8,    simde_svint64_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_s16,    simde_svint64_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_s32,    simde_svint64_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_s64_u8,    simde_svint64_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_u16,    simde_svint64_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_u32,    simde_svint64_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_u64,    simde_svint64_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_f16,    simde_svint64_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_f32,    simde_svint64_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_s64_f64,    simde_svint64_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(   simde_svreinterpret_u8_s8,    simde_svuint8_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_s16,    simde_svuint8_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_s32,    simde_svuint8_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_s64,    simde_svuint8_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_u16,    simde_svuint8_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_u32,    simde_svuint8_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_u64,    simde_svuint8_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_f16,    simde_svuint8_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_f32,    simde_svuint8_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u8_f64,    simde_svuint8_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u16_s8,   simde_svuint16_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_s16,   simde_svuint16_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_s32,   simde_svuint16_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_s64,   simde_svuint16_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u16_u8,   simde_svuint16_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_u32,   simde_svuint16_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_u64,   simde_svuint16_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_f16,   simde_svuint16_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_f32,   simde_svuint16_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u16_f64,   simde_svuint16_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u32_s8,   simde_svuint32_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_s16,   simde_svuint32_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_s32,   simde_svuint32_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_s64,   simde_svuint32_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u32_u8,   simde_svuint32_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_u16,   simde_svuint32_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_u64,   simde_svuint32_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_f16,   simde_svuint32_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_f32,   simde_svuint32_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u32_f64,   simde_svuint32_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u64_s8,   simde_svuint64_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_s16,   simde_svuint64_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_s32,   simde_svuint64_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_s64,   simde_svuint64_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_u64_u8,   simde_svuint64_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_u16,   simde_svuint64_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_u32,   simde_svuint64_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_f16,   simde_svuint64_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_f32,   simde_svuint64_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_u64_f64,   simde_svuint64_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f16_s8,  simde_svfloat16_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_s16,  simde_svfloat16_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_s32,  simde_svfloat16_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_s64,  simde_svfloat16_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f16_u8,  simde_svfloat16_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_u16,  simde_svfloat16_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_u32,  simde_svfloat16_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_u64,  simde_svfloat16_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_f32,  simde_svfloat16_t,  simde_svfloat32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f16_f64,  simde_svfloat16_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f32_s8,  simde_svfloat32_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_s16,  simde_svfloat32_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_s32,  simde_svfloat32_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_s64,  simde_svfloat32_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f32_u8,  simde_svfloat32_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_u16,  simde_svfloat32_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_u32,  simde_svfloat32_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_u64,  simde_svfloat32_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_f16,  simde_svfloat32_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f32_f64,  simde_svfloat32_t,  simde_svfloat64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f64_s8,  simde_svfloat64_t,     simde_svint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_s16,  simde_svfloat64_t,    simde_svint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_s32,  simde_svfloat64_t,    simde_svint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_s64,  simde_svfloat64_t,    simde_svint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_(  simde_svreinterpret_f64_u8,  simde_svfloat64_t,    simde_svuint8_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_u16,  simde_svfloat64_t,   simde_svuint16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_u32,  simde_svfloat64_t,   simde_svuint32_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_u64,  simde_svfloat64_t,   simde_svuint64_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_f16,  simde_svfloat64_t,  simde_svfloat16_t)
  SIMDE_DEFINE_CONVERSION_FUNCTION_( simde_svreinterpret_f64_f32,  simde_svfloat64_t,  simde_svfloat32_t)
#endif

#if defined(__cplusplus)
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svint16_t op) { return   simde_svreinterpret_s8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svint32_t op) { return   simde_svreinterpret_s8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svint64_t op) { return   simde_svreinterpret_s8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(   simde_svuint8_t op) { return    simde_svreinterpret_s8_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(  simde_svuint16_t op) { return   simde_svreinterpret_s8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(  simde_svuint32_t op) { return   simde_svreinterpret_s8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8(  simde_svuint64_t op) { return   simde_svreinterpret_s8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8( simde_svfloat16_t op) { return   simde_svreinterpret_s8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8( simde_svfloat32_t op) { return   simde_svreinterpret_s8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES     simde_svint8_t   simde_svreinterpret_s8( simde_svfloat64_t op) { return   simde_svreinterpret_s8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(    simde_svint8_t op) { return   simde_svreinterpret_s16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(   simde_svint32_t op) { return  simde_svreinterpret_s16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(   simde_svint64_t op) { return  simde_svreinterpret_s16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(   simde_svuint8_t op) { return   simde_svreinterpret_s16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(  simde_svuint16_t op) { return  simde_svreinterpret_s16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(  simde_svuint32_t op) { return  simde_svreinterpret_s16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16(  simde_svuint64_t op) { return  simde_svreinterpret_s16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16( simde_svfloat16_t op) { return  simde_svreinterpret_s16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16( simde_svfloat32_t op) { return  simde_svreinterpret_s16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint16_t  simde_svreinterpret_s16( simde_svfloat64_t op) { return  simde_svreinterpret_s16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(    simde_svint8_t op) { return   simde_svreinterpret_s32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(   simde_svint16_t op) { return  simde_svreinterpret_s32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(   simde_svint64_t op) { return  simde_svreinterpret_s32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(   simde_svuint8_t op) { return   simde_svreinterpret_s32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(  simde_svuint16_t op) { return  simde_svreinterpret_s32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(  simde_svuint32_t op) { return  simde_svreinterpret_s32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32(  simde_svuint64_t op) { return  simde_svreinterpret_s32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32( simde_svfloat16_t op) { return  simde_svreinterpret_s32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32( simde_svfloat32_t op) { return  simde_svreinterpret_s32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint32_t  simde_svreinterpret_s32( simde_svfloat64_t op) { return  simde_svreinterpret_s32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(    simde_svint8_t op) { return   simde_svreinterpret_s64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(   simde_svint16_t op) { return  simde_svreinterpret_s64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(   simde_svint32_t op) { return  simde_svreinterpret_s64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(   simde_svuint8_t op) { return   simde_svreinterpret_s64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(  simde_svuint16_t op) { return  simde_svreinterpret_s64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(  simde_svuint32_t op) { return  simde_svreinterpret_s64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64(  simde_svuint64_t op) { return  simde_svreinterpret_s64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64( simde_svfloat16_t op) { return  simde_svreinterpret_s64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64( simde_svfloat32_t op) { return  simde_svreinterpret_s64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svint64_t  simde_svreinterpret_s64( simde_svfloat64_t op) { return  simde_svreinterpret_s64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(    simde_svint8_t op) { return    simde_svreinterpret_u8_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(   simde_svint16_t op) { return   simde_svreinterpret_u8_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(   simde_svint32_t op) { return   simde_svreinterpret_u8_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(   simde_svint64_t op) { return   simde_svreinterpret_u8_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(  simde_svuint16_t op) { return   simde_svreinterpret_u8_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(  simde_svuint32_t op) { return   simde_svreinterpret_u8_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8(  simde_svuint64_t op) { return   simde_svreinterpret_u8_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8( simde_svfloat16_t op) { return   simde_svreinterpret_u8_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8( simde_svfloat32_t op) { return   simde_svreinterpret_u8_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES    simde_svuint8_t   simde_svreinterpret_u8( simde_svfloat64_t op) { return   simde_svreinterpret_u8_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(    simde_svint8_t op) { return   simde_svreinterpret_u16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svint16_t op) { return  simde_svreinterpret_u16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svint32_t op) { return  simde_svreinterpret_u16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svint64_t op) { return  simde_svreinterpret_u16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(   simde_svuint8_t op) { return   simde_svreinterpret_u16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(  simde_svuint32_t op) { return  simde_svreinterpret_u16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16(  simde_svuint64_t op) { return  simde_svreinterpret_u16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16( simde_svfloat16_t op) { return  simde_svreinterpret_u16_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16( simde_svfloat32_t op) { return  simde_svreinterpret_u16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint16_t  simde_svreinterpret_u16( simde_svfloat64_t op) { return  simde_svreinterpret_u16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(    simde_svint8_t op) { return   simde_svreinterpret_u32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svint16_t op) { return  simde_svreinterpret_u32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svint32_t op) { return  simde_svreinterpret_u32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svint64_t op) { return  simde_svreinterpret_u32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(   simde_svuint8_t op) { return   simde_svreinterpret_u32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(  simde_svuint16_t op) { return  simde_svreinterpret_u32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32(  simde_svuint64_t op) { return  simde_svreinterpret_u32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32( simde_svfloat16_t op) { return  simde_svreinterpret_u32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32( simde_svfloat32_t op) { return  simde_svreinterpret_u32_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint32_t  simde_svreinterpret_u32( simde_svfloat64_t op) { return  simde_svreinterpret_u32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(    simde_svint8_t op) { return   simde_svreinterpret_u64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svint16_t op) { return  simde_svreinterpret_u64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svint32_t op) { return  simde_svreinterpret_u64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svint64_t op) { return  simde_svreinterpret_u64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(   simde_svuint8_t op) { return   simde_svreinterpret_u64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(  simde_svuint16_t op) { return  simde_svreinterpret_u64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64(  simde_svuint32_t op) { return  simde_svreinterpret_u64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64( simde_svfloat16_t op) { return  simde_svreinterpret_u64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64( simde_svfloat32_t op) { return  simde_svreinterpret_u64_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES   simde_svuint64_t  simde_svreinterpret_u64( simde_svfloat64_t op) { return  simde_svreinterpret_u64_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(    simde_svint8_t op) { return   simde_svreinterpret_f16_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svint16_t op) { return  simde_svreinterpret_f16_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svint32_t op) { return  simde_svreinterpret_f16_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svint64_t op) { return  simde_svreinterpret_f16_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(   simde_svuint8_t op) { return   simde_svreinterpret_f16_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(  simde_svuint16_t op) { return  simde_svreinterpret_f16_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(  simde_svuint32_t op) { return  simde_svreinterpret_f16_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16(  simde_svuint64_t op) { return  simde_svreinterpret_f16_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16( simde_svfloat32_t op) { return  simde_svreinterpret_f16_f32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat16_t  simde_svreinterpret_f16( simde_svfloat64_t op) { return  simde_svreinterpret_f16_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(    simde_svint8_t op) { return   simde_svreinterpret_f32_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svint16_t op) { return  simde_svreinterpret_f32_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svint32_t op) { return  simde_svreinterpret_f32_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svint64_t op) { return  simde_svreinterpret_f32_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(   simde_svuint8_t op) { return   simde_svreinterpret_f32_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(  simde_svuint16_t op) { return  simde_svreinterpret_f32_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(  simde_svuint32_t op) { return  simde_svreinterpret_f32_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32(  simde_svuint64_t op) { return  simde_svreinterpret_f32_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32( simde_svfloat16_t op) { return  simde_svreinterpret_f32_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat32_t  simde_svreinterpret_f32( simde_svfloat64_t op) { return  simde_svreinterpret_f32_f64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(    simde_svint8_t op) { return   simde_svreinterpret_f64_s8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svint16_t op) { return  simde_svreinterpret_f64_s16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svint32_t op) { return  simde_svreinterpret_f64_s32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svint64_t op) { return  simde_svreinterpret_f64_s64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(   simde_svuint8_t op) { return   simde_svreinterpret_f64_u8(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(  simde_svuint16_t op) { return  simde_svreinterpret_f64_u16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(  simde_svuint32_t op) { return  simde_svreinterpret_f64_u32(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64(  simde_svuint64_t op) { return  simde_svreinterpret_f64_u64(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64( simde_svfloat16_t op) { return  simde_svreinterpret_f64_f16(op); }
  SIMDE_FUNCTION_ATTRIBUTES  simde_svfloat64_t  simde_svreinterpret_f64( simde_svfloat32_t op) { return  simde_svreinterpret_f64_f32(op); }

  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_s8,     svint8_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s16,    svint16_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s32,    svint32_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_s64,    svint64_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_(  svreinterpret_u8,    svuint8_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u16,   svuint16_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u32,   svuint32_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_u64,   svuint64_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,  svfloat32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f16,  svfloat16_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f32,  svfloat32_t,  svfloat64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,     svint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,    svuint8_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,   svuint16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,   svuint32_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,   svuint64_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,  svfloat16_t)
    SIMDE_DEFINE_CONVERSION_FUNCTION_( svreinterpret_f64,  svfloat64_t,  svfloat32_t)
  #endif /* defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES) */
#elif defined(SIMDE_GENERIC_)
  #define simde_svreinterpret_f64(op) \
    (_Generic((op), \
        simde_svint16_t: simde_svreinterpret_s8_s16, \
        simde_svint32_t: simde_svreinterpret_s8_s32, \
        simde_svint64_t: simde_svreinterpret_s8_s64, \
        simde_svuint8_t: simde_svreinterpret_s8_u8, \
       simde_svuint16_t: simde_svreinterpret_s8_u16, \
       simde_svuint32_t: simde_svreinterpret_s8_u32, \
       simde_svuint64_t: simde_svreinterpret_s8_u64, \
      simde_svfloat16_t: simde_svreinterpret_s8_f16, \
      simde_svfloat32_t: simde_svreinterpret_s8_f32, \
      simde_svfloat64_t: simde_svreinterpret_s8_f64)(op))
  #define simde_svreinterpret_s8(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_s16_s8, \
        simde_svint32_t: simde_svreinterpret_s16_s32, \
        simde_svint64_t: simde_svreinterpret_s16_s64, \
        simde_svuint8_t: simde_svreinterpret_s16_u8, \
       simde_svuint16_t: simde_svreinterpret_s16_u16, \
       simde_svuint32_t: simde_svreinterpret_s16_u32, \
       simde_svuint64_t: simde_svreinterpret_s16_u64, \
      simde_svfloat16_t: simde_svreinterpret_s16_f16, \
      simde_svfloat32_t: simde_svreinterpret_s16_f32, \
      simde_svfloat64_t: simde_svreinterpret_s16_f64)(op))
  #define simde_svreinterpret_s16(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_s32_s8, \
        simde_svint16_t: simde_svreinterpret_s32_s16, \
        simde_svint64_t: simde_svreinterpret_s32_s64, \
        simde_svuint8_t: simde_svreinterpret_s32_u8, \
       simde_svuint16_t: simde_svreinterpret_s32_u16, \
       simde_svuint32_t: simde_svreinterpret_s32_u32, \
       simde_svuint64_t: simde_svreinterpret_s32_u64, \
      simde_svfloat16_t: simde_svreinterpret_s32_f16, \
      simde_svfloat32_t: simde_svreinterpret_s32_f32, \
      simde_svfloat64_t: simde_svreinterpret_s32_f64)(op))
  #define simde_svreinterpret_s32(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_s64_s8, \
        simde_svint16_t: simde_svreinterpret_s64_s16, \
        simde_svint32_t: simde_svreinterpret_s64_s32, \
        simde_svuint8_t: simde_svreinterpret_s64_u8, \
       simde_svuint16_t: simde_svreinterpret_s64_u16, \
       simde_svuint32_t: simde_svreinterpret_s64_u32, \
       simde_svuint64_t: simde_svreinterpret_s64_u64, \
      simde_svfloat16_t: simde_svreinterpret_s64_f16, \
      simde_svfloat32_t: simde_svreinterpret_s64_f32, \
      simde_svfloat64_t: simde_svreinterpret_s64_f64)(op))
  #define simde_svreinterpret_s64(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u8_s8, \
        simde_svint16_t: simde_svreinterpret_u8_s16, \
        simde_svint32_t: simde_svreinterpret_u8_s32, \
        simde_svint64_t: simde_svreinterpret_u8_s64, \
       simde_svuint16_t: simde_svreinterpret_u8_u16, \
       simde_svuint32_t: simde_svreinterpret_u8_u32, \
       simde_svuint64_t: simde_svreinterpret_u8_u64, \
      simde_svfloat16_t: simde_svreinterpret_u8_f16, \
      simde_svfloat32_t: simde_svreinterpret_u8_f32, \
      simde_svfloat64_t: simde_svreinterpret_u8_f64)(op))
  #define simde_svreinterpret_u8(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u16_s8, \
        simde_svint16_t: simde_svreinterpret_u16_s16, \
        simde_svint32_t: simde_svreinterpret_u16_s32, \
        simde_svint64_t: simde_svreinterpret_u16_s64, \
        simde_svuint8_t: simde_svreinterpret_u16_u8, \
       simde_svuint32_t: simde_svreinterpret_u16_u32, \
       simde_svuint64_t: simde_svreinterpret_u16_u64, \
      simde_svfloat16_t: simde_svreinterpret_u16_f16, \
      simde_svfloat32_t: simde_svreinterpret_u16_f32, \
      simde_svfloat64_t: simde_svreinterpret_u16_f64)(op))
  #define simde_svreinterpret_u16(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u32_s8, \
        simde_svint16_t: simde_svreinterpret_u32_s16, \
        simde_svint32_t: simde_svreinterpret_u32_s32, \
        simde_svint64_t: simde_svreinterpret_u32_s64, \
        simde_svuint8_t: simde_svreinterpret_u32_u8, \
       simde_svuint16_t: simde_svreinterpret_u32_u16, \
       simde_svuint64_t: simde_svreinterpret_u32_u64, \
      simde_svfloat16_t: simde_svreinterpret_u32_f16, \
      simde_svfloat32_t: simde_svreinterpret_u32_f32, \
      simde_svfloat64_t: simde_svreinterpret_u32_f64)(op))
  #define simde_svreinterpret_u32(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_u64_s8, \
        simde_svint16_t: simde_svreinterpret_u64_s16, \
        simde_svint32_t: simde_svreinterpret_u64_s32, \
        simde_svint64_t: simde_svreinterpret_u64_s64, \
        simde_svuint8_t: simde_svreinterpret_u64_u8, \
       simde_svuint16_t: simde_svreinterpret_u64_u16, \
       simde_svuint32_t: simde_svreinterpret_u64_u32, \
      simde_svfloat16_t: simde_svreinterpret_u64_f16, \
      simde_svfloat32_t: simde_svreinterpret_u64_f32, \
      simde_svfloat64_t: simde_svreinterpret_u64_f64)(op))
  #define simde_svreinterpret_u64(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_f16_s8, \
        simde_svint16_t: simde_svreinterpret_f16_s16, \
        simde_svint32_t: simde_svreinterpret_f16_s32, \
        simde_svint64_t: simde_svreinterpret_f16_s64, \
        simde_svuint8_t: simde_svreinterpret_f16_u8, \
       simde_svuint16_t: simde_svreinterpret_f16_u16, \
       simde_svuint32_t: simde_svreinterpret_f16_u32, \
       simde_svuint64_t: simde_svreinterpret_f16_u64, \
      simde_svfloat32_t: simde_svreinterpret_f16_f32, \
      simde_svfloat64_t: simde_svreinterpret_f16_f64)(op))
  #define simde_svreinterpret_f16(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_f32_s8, \
        simde_svint16_t: simde_svreinterpret_f32_s16, \
        simde_svint32_t: simde_svreinterpret_f32_s32, \
        simde_svint64_t: simde_svreinterpret_f32_s64, \
        simde_svuint8_t: simde_svreinterpret_f32_u8, \
       simde_svuint16_t: simde_svreinterpret_f32_u16, \
       simde_svuint32_t: simde_svreinterpret_f32_u32, \
       simde_svuint64_t: simde_svreinterpret_f32_u64, \
      simde_svfloat16_t: simde_svreinterpret_f32_f16, \
      simde_svfloat64_t: simde_svreinterpret_f32_f64)(op))
  #define simde_svreinterpret_f32(op) \
    (_Generic((op), \
         simde_svint8_t: simde_svreinterpret_f64_s8, \
        simde_svint16_t: simde_svreinterpret_f64_s16, \
        simde_svint32_t: simde_svreinterpret_f64_s32, \
        simde_svint64_t: simde_svreinterpret_f64_s64, \
        simde_svuint8_t: simde_svreinterpret_f64_u8, \
       simde_svuint16_t: simde_svreinterpret_f64_u16, \
       simde_svuint32_t: simde_svreinterpret_f64_u32, \
       simde_svuint64_t: simde_svreinterpret_f64_u64, \
      simde_svfloat16_t: simde_svreinterpret_f64_f16, \
      simde_svfloat32_t: simde_svreinterpret_f64_f32)(op))
  #if defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES)
  #define svreinterpret_f64(op) \
    (_Generic((op), \
              svint16_t: svreinterpret_s8_s16, \
              svint32_t: svreinterpret_s8_s32, \
              svint64_t: svreinterpret_s8_s64, \
              svuint8_t: svreinterpret_s8_u8, \
             svuint16_t: svreinterpret_s8_u16, \
             svuint32_t: svreinterpret_s8_u32, \
             svuint64_t: svreinterpret_s8_u64, \
            svfloat16_t: svreinterpret_s8_f16, \
            svfloat32_t: svreinterpret_s8_f32, \
            svfloat64_t: svreinterpret_s8_f64)(op))
  #define svreinterpret_s8(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_s16_s8, \
              svint32_t: svreinterpret_s16_s32, \
              svint64_t: svreinterpret_s16_s64, \
              svuint8_t: svreinterpret_s16_u8, \
             svuint16_t: svreinterpret_s16_u16, \
             svuint32_t: svreinterpret_s16_u32, \
             svuint64_t: svreinterpret_s16_u64, \
            svfloat16_t: svreinterpret_s16_f16, \
            svfloat32_t: svreinterpret_s16_f32, \
            svfloat64_t: svreinterpret_s16_f64)(op))
  #define svreinterpret_s16(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_s32_s8, \
              svint16_t: svreinterpret_s32_s16, \
              svint64_t: svreinterpret_s32_s64, \
              svuint8_t: svreinterpret_s32_u8, \
             svuint16_t: svreinterpret_s32_u16, \
             svuint32_t: svreinterpret_s32_u32, \
             svuint64_t: svreinterpret_s32_u64, \
            svfloat16_t: svreinterpret_s32_f16, \
            svfloat32_t: svreinterpret_s32_f32, \
            svfloat64_t: svreinterpret_s32_f64)(op))
  #define svreinterpret_s32(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_s64_s8, \
              svint16_t: svreinterpret_s64_s16, \
              svint32_t: svreinterpret_s64_s32, \
              svuint8_t: svreinterpret_s64_u8, \
             svuint16_t: svreinterpret_s64_u16, \
             svuint32_t: svreinterpret_s64_u32, \
             svuint64_t: svreinterpret_s64_u64, \
            svfloat16_t: svreinterpret_s64_f16, \
            svfloat32_t: svreinterpret_s64_f32, \
            svfloat64_t: svreinterpret_s64_f64)(op))
  #define svreinterpret_s64(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u8_s8, \
              svint16_t: svreinterpret_u8_s16, \
              svint32_t: svreinterpret_u8_s32, \
              svint64_t: svreinterpret_u8_s64, \
             svuint16_t: svreinterpret_u8_u16, \
             svuint32_t: svreinterpret_u8_u32, \
             svuint64_t: svreinterpret_u8_u64, \
            svfloat16_t: svreinterpret_u8_f16, \
            svfloat32_t: svreinterpret_u8_f32, \
            svfloat64_t: svreinterpret_u8_f64)(op))
  #define svreinterpret_u8(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u16_s8, \
              svint16_t: svreinterpret_u16_s16, \
              svint32_t: svreinterpret_u16_s32, \
              svint64_t: svreinterpret_u16_s64, \
              svuint8_t: svreinterpret_u16_u8, \
             svuint32_t: svreinterpret_u16_u32, \
             svuint64_t: svreinterpret_u16_u64, \
            svfloat16_t: svreinterpret_u16_f16, \
            svfloat32_t: svreinterpret_u16_f32, \
            svfloat64_t: svreinterpret_u16_f64)(op))
  #define svreinterpret_u16(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u32_s8, \
              svint16_t: svreinterpret_u32_s16, \
              svint32_t: svreinterpret_u32_s32, \
              svint64_t: svreinterpret_u32_s64, \
              svuint8_t: svreinterpret_u32_u8, \
             svuint16_t: svreinterpret_u32_u16, \
             svuint64_t: svreinterpret_u32_u64, \
            svfloat16_t: svreinterpret_u32_f16, \
            svfloat32_t: svreinterpret_u32_f32, \
            svfloat64_t: svreinterpret_u32_f64)(op))
  #define svreinterpret_u32(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_u64_s8, \
              svint16_t: svreinterpret_u64_s16, \
              svint32_t: svreinterpret_u64_s32, \
              svint64_t: svreinterpret_u64_s64, \
              svuint8_t: svreinterpret_u64_u8, \
             svuint16_t: svreinterpret_u64_u16, \
             svuint32_t: svreinterpret_u64_u32, \
            svfloat16_t: svreinterpret_u64_f16, \
            svfloat32_t: svreinterpret_u64_f32, \
            svfloat64_t: svreinterpret_u64_f64)(op))
  #define svreinterpret_u64(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_f16_s8, \
              svint16_t: svreinterpret_f16_s16, \
              svint32_t: svreinterpret_f16_s32, \
              svint64_t: svreinterpret_f16_s64, \
              svuint8_t: svreinterpret_f16_u8, \
             svuint16_t: svreinterpret_f16_u16, \
             svuint32_t: svreinterpret_f16_u32, \
             svuint64_t: svreinterpret_f16_u64, \
            svfloat32_t: svreinterpret_f16_f32, \
            svfloat64_t: svreinterpret_f16_f64)(op))
  #define svreinterpret_f16(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_f32_s8, \
              svint16_t: svreinterpret_f32_s16, \
              svint32_t: svreinterpret_f32_s32, \
              svint64_t: svreinterpret_f32_s64, \
              svuint8_t: svreinterpret_f32_u8, \
             svuint16_t: svreinterpret_f32_u16, \
             svuint32_t: svreinterpret_f32_u32, \
             svuint64_t: svreinterpret_f32_u64, \
            svfloat16_t: svreinterpret_f32_f16, \
            svfloat64_t: svreinterpret_f32_f64)(op))
  #define svreinterpret_f32(op) \
    (_Generic((op), \
               svint8_t: svreinterpret_f64_s8, \
              svint16_t: svreinterpret_f64_s16, \
              svint32_t: svreinterpret_f64_s32, \
              svint64_t: svreinterpret_f64_s64, \
              svuint8_t: svreinterpret_f64_u8, \
             svuint16_t: svreinterpret_f64_u16, \
             svuint32_t: svreinterpret_f64_u32, \
             svuint64_t: svreinterpret_f64_u64, \
            svfloat16_t: svreinterpret_f64_f16, \
            svfloat32_t: svreinterpret_f64_f32)(op))
  #endif /* defined(SIMDE_ARM_SVE_ENABLE_NATIVE_ALIASES) */
#endif

HEDLEY_DIAGNOSTIC_POP

#endif /* SIMDE_ARM_SVE_REINTERPRET_H */
