#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Usage: ./update.sh <libwebp_directory>
#
# Copies the needed files from a directory containing the original
# libwebp source.

cp $1/AUTHORS .
cp $1/COPYING .
cp $1/NEWS .
cp $1/PATENTS .
cp $1/README .
cp $1/README.mux .

mkdir -p src/webp
cp $1/src/webp/*.h src/webp

mkdir -p src/dec
cp $1/src/dec/*.h src/dec
cp $1/src/dec/alpha_dec.c src/dec
cp $1/src/dec/buffer_dec.c src/dec
cp $1/src/dec/frame_dec.c src/dec
cp $1/src/dec/idec_dec.c src/dec
cp $1/src/dec/io_dec.c src/dec
cp $1/src/dec/quant_dec.c src/dec
cp $1/src/dec/tree_dec.c src/dec
cp $1/src/dec/vp8_dec.c src/dec
cp $1/src/dec/vp8l_dec.c src/dec
cp $1/src/dec/webp_dec.c src/dec

mkdir -p src/demux
cp $1/src/demux/demux.c src/demux

mkdir -p src/dsp
cp $1/src/dsp/*.h src/dsp
cp $1/src/dsp/alpha_processing.c src/dsp
cp $1/src/dsp/alpha_processing_mips_dsp_r2.c src/dsp
cp $1/src/dsp/alpha_processing_neon.c src/dsp
cp $1/src/dsp/alpha_processing_sse2.c src/dsp
cp $1/src/dsp/alpha_processing_sse41.c src/dsp
cp $1/src/dsp/dec.c src/dsp
cp $1/src/dsp/dec_clip_tables.c src/dsp
cp $1/src/dsp/dec_mips32.c src/dsp
cp $1/src/dsp/dec_mips_dsp_r2.c src/dsp
cp $1/src/dsp/dec_msa.c src/dsp
cp $1/src/dsp/dec_neon.c src/dsp
cp $1/src/dsp/dec_sse2.c src/dsp
cp $1/src/dsp/dec_sse41.c src/dsp
cp $1/src/dsp/filters.c src/dsp
cp $1/src/dsp/filters_mips_dsp_r2.c src/dsp
cp $1/src/dsp/filters_msa.c src/dsp
cp $1/src/dsp/filters_neon.c src/dsp
cp $1/src/dsp/filters_sse2.c src/dsp
cp $1/src/dsp/lossless.c src/dsp
cp $1/src/dsp/lossless_mips_dsp_r2.c src/dsp
cp $1/src/dsp/lossless_msa.c src/dsp
cp $1/src/dsp/lossless_neon.c src/dsp
cp $1/src/dsp/lossless_sse2.c src/dsp
cp $1/src/dsp/rescaler.c src/dsp
cp $1/src/dsp/rescaler_mips32.c src/dsp
cp $1/src/dsp/rescaler_mips_dsp_r2.c src/dsp
cp $1/src/dsp/rescaler_msa.c src/dsp
cp $1/src/dsp/rescaler_neon.c src/dsp
cp $1/src/dsp/rescaler_sse2.c src/dsp
cp $1/src/dsp/upsampling.c src/dsp
cp $1/src/dsp/upsampling_mips_dsp_r2.c src/dsp
cp $1/src/dsp/upsampling_msa.c src/dsp
cp $1/src/dsp/upsampling_neon.c src/dsp
cp $1/src/dsp/upsampling_sse2.c src/dsp
cp $1/src/dsp/upsampling_sse41.c src/dsp
cp $1/src/dsp/yuv.c src/dsp
cp $1/src/dsp/yuv_mips32.c src/dsp
cp $1/src/dsp/yuv_mips_dsp_r2.c src/dsp
cp $1/src/dsp/yuv_neon.c src/dsp
cp $1/src/dsp/yuv_sse2.c src/dsp
cp $1/src/dsp/yuv_sse41.c src/dsp

mkdir -p src/enc
cp $1/src/enc/*.h src/enc

mkdir -p src/utils
cp $1/src/utils/*.h src/utils
cp $1/src/utils/bit_reader_utils.c src/utils
cp $1/src/utils/color_cache_utils.c src/utils
cp $1/src/utils/filters_utils.c src/utils
cp $1/src/utils/huffman_utils.c src/utils
cp $1/src/utils/quant_levels_dec_utils.c src/utils
cp $1/src/utils/quant_levels_utils.c src/utils
cp $1/src/utils/random_utils.c src/utils
cp $1/src/utils/rescaler_utils.c src/utils
cp $1/src/utils/thread_utils.c src/utils
cp $1/src/utils/utils.c src/utils
