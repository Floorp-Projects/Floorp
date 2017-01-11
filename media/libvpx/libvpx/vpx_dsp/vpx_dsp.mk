##
## Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

DSP_SRCS-yes += vpx_dsp.mk

ifeq ($(CONFIG_ENCODERS),yes)
DSP_SRCS-yes            += sad.c

DSP_SRCS-$(HAVE_MEDIA)  += arm/sad_media$(ASM)
DSP_SRCS-$(HAVE_NEON)   += arm/sad4d_neon.c
DSP_SRCS-$(HAVE_NEON)   += arm/sad_neon.c


DSP_SRCS-$(HAVE_MMX)    += x86/sad_mmx.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/sad_sse2.asm
DSP_SRCS-$(HAVE_SSE3)   += x86/sad_sse3.asm
DSP_SRCS-$(HAVE_SSSE3)  += x86/sad_ssse3.asm
DSP_SRCS-$(HAVE_SSE4_1) += x86/sad_sse4.asm
DSP_SRCS-$(HAVE_AVX2)   += x86/sad4d_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/sad_avx2.c

ifeq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_sad4d_sse2.asm
DSP_SRCS-$(HAVE_SSE2) += x86/highbd_sad_sse2.asm

endif  # CONFIG_VP9_HIGHBITDEPTH
endif  # CONFIG_ENCODERS

ifneq ($(filter yes,$(CONFIG_ENCODERS) $(CONFIG_POSTPROC) $(CONFIG_VP9_POSTPROC)),)
DSP_SRCS-yes            += variance.c

DSP_SRCS-$(HAVE_MEDIA)  += arm/variance_media$(ASM)
DSP_SRCS-$(HAVE_NEON)   += arm/variance_neon.c

DSP_SRCS-$(HAVE_MMX)    += x86/variance_mmx.c
DSP_SRCS-$(HAVE_MMX)    += x86/variance_impl_mmx.asm
DSP_SRCS-$(HAVE_SSE2)   += x86/variance_sse2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/variance_avx2.c
DSP_SRCS-$(HAVE_AVX2)   += x86/variance_impl_avx2.c

ifeq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_variance_sse2.c
DSP_SRCS-$(HAVE_SSE2)   += x86/highbd_variance_impl_sse2.asm
endif  # CONFIG_VP9_HIGHBITDEPTH
endif  # CONFIG_ENCODERS || CONFIG_POSTPROC || CONFIG_VP9_POSTPROC

DSP_SRCS-no += $(DSP_SRCS_REMOVE-yes)

DSP_SRCS-yes += vpx_dsp_rtcd.c
DSP_SRCS-yes += vpx_dsp_rtcd_defs.pl

$(eval $(call rtcd_h_template,vpx_dsp_rtcd,vpx_dsp/vpx_dsp_rtcd_defs.pl))
