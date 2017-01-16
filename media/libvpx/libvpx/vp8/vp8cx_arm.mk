##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##


VP8_CX_SRCS-$(ARCH_ARM)  += vp8cx_arm.mk

#File list for arm
# encoder
VP8_CX_SRCS-$(ARCH_ARM)  += encoder/arm/dct_arm.c

#File list for media
# encoder
VP8_CX_SRCS-$(HAVE_MEDIA)  += encoder/arm/armv6/vp8_short_fdct4x4_armv6$(ASM)
VP8_CX_SRCS-$(HAVE_MEDIA)  += encoder/arm/armv6/walsh_v6$(ASM)

#File list for neon
# encoder
VP8_CX_SRCS-$(HAVE_NEON)  += encoder/arm/neon/denoising_neon.c
VP8_CX_SRCS-$(HAVE_NEON)  += encoder/arm/neon/fastquantizeb_neon.c
VP8_CX_SRCS-$(HAVE_NEON)  += encoder/arm/neon/shortfdct_neon.c
VP8_CX_SRCS-$(HAVE_NEON)  += encoder/arm/neon/vp8_shortwalsh4x4_neon.c
