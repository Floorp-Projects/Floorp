/*
 *  Copyright (c) 2013 Mozilla Foundation. All Rights Reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.
 */

#if defined(_WIN64)
/* 64 bit Windows */
#ifdef _MSC_VER
#include "vp9_rtcd_x86_64-win64-vs8.h"
#else
#include "vp9_rtcd_x86_64-win64-gcc.h"
#endif

#elif defined(_WIN32)
/* 32 bit Windows, MSVC. */
#ifdef _MSC_VER
#include "vp9_rtcd_x86-win32-vs8.h"
#else
#include "vp9_rtcd_x86-win32-gcc.h"
#endif

#elif defined(__APPLE__) && defined(__x86_64__)
/* 64 bit MacOS. */
#include "vp9_rtcd_x86_64-darwin9-gcc.h"

#elif defined(__APPLE__) && defined(__i386__)
/* 32 bit MacOS. */
#include "vp9_rtcd_x86-darwin9-gcc.h"

#elif defined(__ELF__) && (defined(__i386) || defined(__i386__))
/* 32 bit ELF platforms. */
#include "vp9_rtcd_x86-linux-gcc.h"

#elif defined(__ELF__) && (defined(__x86_64) || defined(__x86_64__))
/* 64 bit ELF platforms. */
#include "vp9_rtcd_x86_64-linux-gcc.h"

#elif defined(VPX_ARM_ASM)
/* Android */
#include "vp9_rtcd_armv7-android-gcc.h"

#else
/* Assume generic GNU/GCC configuration. */
#include "vp9_rtcd_generic-gnu.h"
#endif
