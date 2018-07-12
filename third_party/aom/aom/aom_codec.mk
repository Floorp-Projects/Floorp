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


API_EXPORTS += exports

API_SRCS-$(CONFIG_AV1_ENCODER) += aom.h
API_SRCS-$(CONFIG_AV1_ENCODER) += aomcx.h
API_DOC_SRCS-$(CONFIG_AV1_ENCODER) += aom.h
API_DOC_SRCS-$(CONFIG_AV1_ENCODER) += aomcx.h

API_SRCS-$(CONFIG_AV1_DECODER) += aom.h
API_SRCS-$(CONFIG_AV1_DECODER) += aomdx.h
API_DOC_SRCS-$(CONFIG_AV1_DECODER) += aom.h
API_DOC_SRCS-$(CONFIG_AV1_DECODER) += aomdx.h

API_DOC_SRCS-yes += aom_codec.h
API_DOC_SRCS-yes += aom_decoder.h
API_DOC_SRCS-yes += aom_encoder.h
API_DOC_SRCS-yes += aom_frame_buffer.h
API_DOC_SRCS-yes += aom_image.h

API_SRCS-yes += src/aom_decoder.c
API_SRCS-yes += aom_decoder.h
API_SRCS-yes += src/aom_encoder.c
API_SRCS-yes += aom_encoder.h
API_SRCS-yes += internal/aom_codec_internal.h
API_SRCS-yes += src/aom_codec.c
API_SRCS-yes += src/aom_image.c
API_SRCS-yes += aom_codec.h
API_SRCS-yes += aom_codec.mk
API_SRCS-yes += aom_frame_buffer.h
API_SRCS-yes += aom_image.h
API_SRCS-yes += aom_integer.h
