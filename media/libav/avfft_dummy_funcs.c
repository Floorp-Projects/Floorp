/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// In libavcodec/fft_template.c, fft initialization happens via if statements
// checked on preprocessor defines. On many platforms, these statements are
// culled during compilation. However, in situations where optimization is
// disabled on windows visual studio (PGO, using --disable-optimization in
// mozconfig, etc), these branches are still compiled, meaning we end up with
// linker errors due calls to undefined functions. The dummy functions in this
// file provide bodies so that the library will link in that case, even though
// these will never be called.

#include "libavcodec/fft.h"

void
ff_fft_init_aarch64(FFTContext *s)
{
}

void
ff_fft_init_arm(FFTContext *s)
{
}

void
ff_fft_init_ppc(FFTContext *s)
{
}

void
ff_fft_fixed_init_arm(FFTContext *s)
{
}

void
ff_rdft_init_arm(RDFTContext *s)
{
}

int
ff_get_cpu_flags_aarch64(void)
{
  return 0;
}

int
ff_get_cpu_flags_arm(void)
{
  return 0;
}

int
ff_get_cpu_flags_ppc(void)
{
  return 0;
}

void
ff_mdct_calcw_c(FFTContext *s, FFTDouble *out, const FFTSample *input)
{
}
