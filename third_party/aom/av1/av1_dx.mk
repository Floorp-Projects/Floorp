##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##

AV1_DX_EXPORTS += exports_dec

AV1_DX_SRCS-yes += $(AV1_COMMON_SRCS-yes)
AV1_DX_SRCS-no  += $(AV1_COMMON_SRCS-no)
AV1_DX_SRCS_REMOVE-yes += $(AV1_COMMON_SRCS_REMOVE-yes)
AV1_DX_SRCS_REMOVE-no  += $(AV1_COMMON_SRCS_REMOVE-no)

AV1_DX_SRCS-yes += av1_dx_iface.c

AV1_DX_SRCS-yes += decoder/decodemv.c
AV1_DX_SRCS-yes += decoder/decodeframe.c
AV1_DX_SRCS-yes += decoder/decodeframe.h
AV1_DX_SRCS-yes += decoder/detokenize.c
AV1_DX_SRCS-yes += decoder/decodemv.h
AV1_DX_SRCS-$(CONFIG_LV_MAP) += decoder/decodetxb.c
AV1_DX_SRCS-$(CONFIG_LV_MAP) += decoder/decodetxb.h
AV1_DX_SRCS-yes += decoder/detokenize.h
AV1_DX_SRCS-yes += decoder/dthread.c
AV1_DX_SRCS-yes += decoder/dthread.h
AV1_DX_SRCS-yes += decoder/decoder.c
AV1_DX_SRCS-yes += decoder/decoder.h
AV1_DX_SRCS-yes += decoder/dsubexp.c
AV1_DX_SRCS-yes += decoder/dsubexp.h

ifeq ($(CONFIG_ACCOUNTING),yes)
AV1_DX_SRCS-yes += decoder/accounting.h
AV1_DX_SRCS-yes += decoder/accounting.c
endif

ifeq ($(CONFIG_INSPECTION),yes)
AV1_DX_SRCS-yes += decoder/inspection.c
AV1_DX_SRCS-yes += decoder/inspection.h
endif

ifeq ($(CONFIG_PVQ),yes)
# PVQ from daala
AV1_DX_SRCS-yes += decoder/pvq_decoder.c
AV1_DX_SRCS-yes += decoder/pvq_decoder.h
AV1_DX_SRCS-yes += decoder/decint.h
AV1_DX_SRCS-yes += decoder/generic_decoder.c
AV1_DX_SRCS-yes += decoder/laplace_decoder.c
AV1_DX_SRCS-yes += encoder/hybrid_fwd_txfm.c
AV1_DX_SRCS-yes += encoder/hybrid_fwd_txfm.h

AV1_DX_SRCS-yes += encoder/dct.c
AV1_DX_SRCS-$(HAVE_SSE2) += encoder/x86/dct_sse2.asm
AV1_DX_SRCS-$(HAVE_SSE2) += encoder/x86/dct_intrin_sse2.c
AV1_DX_SRCS-$(HAVE_SSSE3) += encoder/x86/dct_ssse3.c

ifneq ($(CONFIG_HIGHBITDEPTH),yes)
AV1_DX_SRCS-$(HAVE_NEON) += encoder/arm/neon/dct_neon.c
endif

AV1_DX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct4x4_msa.c
AV1_DX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct8x8_msa.c
AV1_DX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct16x16_msa.c
AV1_DX_SRCS-$(HAVE_MSA) += encoder/mips/msa/fdct_msa.h
endif

AV1_DX_SRCS-yes := $(filter-out $(AV1_DX_SRCS_REMOVE-yes),$(AV1_DX_SRCS-yes))
