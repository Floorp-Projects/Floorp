/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file replaces the CPU info methods originally implemented in
 * src/dsp/cpu.c, due to missing dependencies for Andriod builds. It
 * controls if NEON/SSE/etc is used. */

#include "../dsp/dsp.h"
#include "mozilla/arm.h"
#include "mozilla/SSE.h"

extern "C" {
extern VP8CPUInfo VP8GetCPUInfo;
extern VP8CPUInfo SharpYuvGetCPUInfo;
}

static int MozCPUInfo(CPUFeature feature)
{
  switch (feature) {
    case kSSE2:
      return mozilla::supports_sse2();
    case kSSE3:
      return mozilla::supports_sse3();
    case kSSE4_1:
      return mozilla::supports_sse4_1();
    case kAVX:
      return mozilla::supports_avx();
    case kAVX2:
      return mozilla::supports_avx2();
    case kNEON:
      return mozilla::supports_neon();
#if defined(WEBP_USE_MIPS32) || defined(WEBP_USE_MIPS_DSP_R2) || defined(WEBP_USE_MSA)
    case kMIPS32:
    case kMIPSdspR2:
    case kMSA:
      return 1;
#endif
    default:
      return 0;
  }
}

VP8CPUInfo VP8GetCPUInfo = MozCPUInfo;
VP8CPUInfo SharpYuvGetCPUInfo = MozCPUInfo;
