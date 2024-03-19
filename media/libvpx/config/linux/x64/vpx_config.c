/* Copyright (c) 2011 The WebM project authors. All Rights Reserved. */
/*  */
/* Use of this source code is governed by a BSD-style license */
/* that can be found in the LICENSE file in the root of the source */
/* tree. An additional intellectual property rights grant can be found */
/* in the file PATENTS.  All contributing project authors may */
/* be found in the AUTHORS file in the root of the source tree. */
#include "vpx/vpx_codec.h"
static const char* const cfg = "--target=x86_64-linux-gcc --enable-external-build --disable-examples --disable-install-docs --disable-unit-tests --enable-multi-res-encoding --size-limit=8192x4608 --enable-pic --disable-avx512 --enable-postproc --enable-vp9-postproc --as=yasm --log=/home/cm/Work/gecko-dev/media/libvpx/config/linux/x64/config.log";
const char *vpx_codec_build_config(void) {return cfg;}
