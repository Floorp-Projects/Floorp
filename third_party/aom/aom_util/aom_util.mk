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


UTIL_SRCS-yes += aom_util.mk
UTIL_SRCS-yes += aom_thread.c
UTIL_SRCS-yes += aom_thread.h
UTIL_SRCS-$(CONFIG_BITSTREAM_DEBUG) += debug_util.c
UTIL_SRCS-$(CONFIG_BITSTREAM_DEBUG) += debug_util.h
UTIL_SRCS-yes += endian_inl.h
