/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPX_PORTS_ASM_OFFSETS_H
#define VPX_PORTS_ASM_OFFSETS_H

#include <stddef.h>

#define ct_assert(name,cond) \
  static void assert_##name(void) UNUSED;\
  static void assert_##name(void) {switch(0){case 0:case !!(cond):;}}

#if INLINE_ASM
#define DEFINE(sym, val) asm("\n" #sym " EQU %0" : : "i" (val))
#define BEGIN int main(void) {
#define END return 0; }
#else
#define DEFINE(sym, val) const int sym = val
#define BEGIN
#define END
#endif

#endif /* VPX_PORTS_ASM_OFFSETS_H */
