/* Copyright (c) 2011 The WebM project authors. All Rights Reserved. */
/*  */
/* Use of this source code is governed by a BSD-style license */
/* that can be found in the LICENSE file in the root of the source */
/* tree. An additional intellectual property rights grant can be found */
/* in the file PATENTS.  All contributing project authors may */
/* be found in the AUTHORS file in the root of the source tree. */
static const char* const cfg = "--target=armv7-linux-gcc --disable-vp8-encoder --disable-examples --disable-install-docs --enable-pic --enable-runtime-cpu-detect --enable-arm-asm-detok";
const char *vpx_codec_build_config(void) {return cfg;}
