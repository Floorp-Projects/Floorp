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
 *   2020      Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_X86_AVX512_H)
#define SIMDE_X86_AVX512_H

#include "avx512/types.h"

#include "avx512/2intersect.h"
#include "avx512/4dpwssd.h"
#include "avx512/4dpwssds.h"
#include "avx512/abs.h"
#include "avx512/add.h"
#include "avx512/adds.h"
#include "avx512/and.h"
#include "avx512/andnot.h"
#include "avx512/avg.h"
#include "avx512/bitshuffle.h"
#include "avx512/blend.h"
#include "avx512/broadcast.h"
#include "avx512/cast.h"
#include "avx512/cmp.h"
#include "avx512/cmpeq.h"
#include "avx512/cmpge.h"
#include "avx512/cmpgt.h"
#include "avx512/cmple.h"
#include "avx512/cmplt.h"
#include "avx512/cmpneq.h"
#include "avx512/compress.h"
#include "avx512/conflict.h"
#include "avx512/copysign.h"
#include "avx512/cvt.h"
#include "avx512/cvtt.h"
#include "avx512/cvts.h"
#include "avx512/dbsad.h"
#include "avx512/div.h"
#include "avx512/dpbf16.h"
#include "avx512/dpbusd.h"
#include "avx512/dpbusds.h"
#include "avx512/dpwssd.h"
#include "avx512/dpwssds.h"
#include "avx512/expand.h"
#include "avx512/extract.h"
#include "avx512/fixupimm.h"
#include "avx512/fixupimm_round.h"
#include "avx512/flushsubnormal.h"
#include "avx512/fmadd.h"
#include "avx512/fmsub.h"
#include "avx512/fnmadd.h"
#include "avx512/fnmsub.h"
#include "avx512/insert.h"
#include "avx512/kshift.h"
#include "avx512/knot.h"
#include "avx512/kxor.h"
#include "avx512/load.h"
#include "avx512/loadu.h"
#include "avx512/lzcnt.h"
#include "avx512/madd.h"
#include "avx512/maddubs.h"
#include "avx512/max.h"
#include "avx512/min.h"
#include "avx512/mov.h"
#include "avx512/mov_mask.h"
#include "avx512/movm.h"
#include "avx512/mul.h"
#include "avx512/mulhi.h"
#include "avx512/mulhrs.h"
#include "avx512/mullo.h"
#include "avx512/multishift.h"
#include "avx512/negate.h"
#include "avx512/or.h"
#include "avx512/packs.h"
#include "avx512/packus.h"
#include "avx512/permutexvar.h"
#include "avx512/permutex2var.h"
#include "avx512/popcnt.h"
#include "avx512/range.h"
#include "avx512/range_round.h"
#include "avx512/rol.h"
#include "avx512/rolv.h"
#include "avx512/ror.h"
#include "avx512/rorv.h"
#include "avx512/round.h"
#include "avx512/roundscale.h"
#include "avx512/roundscale_round.h"
#include "avx512/sad.h"
#include "avx512/scalef.h"
#include "avx512/set.h"
#include "avx512/set1.h"
#include "avx512/set4.h"
#include "avx512/setr.h"
#include "avx512/setr4.h"
#include "avx512/setzero.h"
#include "avx512/setone.h"
#include "avx512/shldv.h"
#include "avx512/shuffle.h"
#include "avx512/sll.h"
#include "avx512/slli.h"
#include "avx512/sllv.h"
#include "avx512/sqrt.h"
#include "avx512/sra.h"
#include "avx512/srai.h"
#include "avx512/srav.h"
#include "avx512/srl.h"
#include "avx512/srli.h"
#include "avx512/srlv.h"
#include "avx512/store.h"
#include "avx512/storeu.h"
#include "avx512/sub.h"
#include "avx512/subs.h"
#include "avx512/ternarylogic.h"
#include "avx512/test.h"
#include "avx512/testn.h"
#include "avx512/unpacklo.h"
#include "avx512/unpackhi.h"
#include "avx512/xor.h"
#include "avx512/xorsign.h"

#endif
