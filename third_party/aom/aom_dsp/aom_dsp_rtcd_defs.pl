##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
sub aom_dsp_forward_decls() {
print <<EOF
/*
 * DSP
 */

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/enums.h"
#include "av1/common/blockd.h"

EOF
}
forward_decls qw/aom_dsp_forward_decls/;

# optimizations which depend on multiple features
$avx2_ssse3 = '';
if ((aom_config("HAVE_AVX2") eq "yes") && (aom_config("HAVE_SSSE3") eq "yes")) {
  $avx2_ssse3 = 'avx2';
}

# functions that are 64 bit only.
$mmx_x86_64 = $sse2_x86_64 = $ssse3_x86_64 = $avx_x86_64 = $avx2_x86_64 = '';
if ($opts{arch} eq "x86_64") {
  $mmx_x86_64 = 'mmx';
  $sse2_x86_64 = 'sse2';
  $ssse3_x86_64 = 'ssse3';
  $avx_x86_64 = 'avx';
  $avx2_x86_64 = 'avx2';
}

@block_widths = (4, 8, 16, 32, 64, 128);

@block_sizes = ();
foreach $w (@block_widths) {
  foreach $h (@block_widths) {
    push @block_sizes, [$w, $h] if ($w <= 2*$h && $h <= 2*$w) ;
  }
}
push @block_sizes, [4, 16];
push @block_sizes, [16, 4];
push @block_sizes, [8, 32];
push @block_sizes, [32, 8];
push @block_sizes, [16, 64];
push @block_sizes, [64, 16];

@tx_dims = (2, 4, 8, 16, 32, 64);
@tx_sizes = ();
foreach $w (@tx_dims) {
  push @tx_sizes, [$w, $w];
  foreach $h (@tx_dims) {
    push @tx_sizes, [$w, $h] if ($w >=4 && $h >=4 && ($w == 2*$h || $h == 2*$w));
    push @tx_sizes, [$w, $h] if ($w >=4 && $h >=4 && ($w == 4*$h || $h == 4*$w));
  }
}

@pred_names = qw/dc dc_top dc_left dc_128 v h paeth smooth smooth_v smooth_h/;

#
# Intra prediction
#

foreach (@tx_sizes) {
  ($w, $h) = @$_;
  foreach $pred_name (@pred_names) {
    add_proto "void", "aom_${pred_name}_predictor_${w}x${h}",
              "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
    add_proto "void", "aom_highbd_${pred_name}_predictor_${w}x${h}",
              "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  }
}

specialize qw/aom_dc_top_predictor_4x4 msa neon sse2/;
specialize qw/aom_dc_top_predictor_4x8 sse2/;
specialize qw/aom_dc_top_predictor_4x16 sse2/;
specialize qw/aom_dc_top_predictor_8x4 sse2/;
specialize qw/aom_dc_top_predictor_8x8 neon msa sse2/;
specialize qw/aom_dc_top_predictor_8x16 sse2/;
specialize qw/aom_dc_top_predictor_8x32 sse2/;
specialize qw/aom_dc_top_predictor_16x4 sse2/;
specialize qw/aom_dc_top_predictor_16x8 sse2/;
specialize qw/aom_dc_top_predictor_16x16 neon msa sse2/;
specialize qw/aom_dc_top_predictor_16x32 sse2/;
specialize qw/aom_dc_top_predictor_16x64 sse2/;
specialize qw/aom_dc_top_predictor_32x8 sse2/;
specialize qw/aom_dc_top_predictor_32x16 sse2 avx2/;
specialize qw/aom_dc_top_predictor_32x32 msa neon sse2 avx2/;
specialize qw/aom_dc_top_predictor_32x64 sse2 avx2/;
specialize qw/aom_dc_top_predictor_64x64 sse2 avx2/;
specialize qw/aom_dc_top_predictor_64x32 sse2 avx2/;
specialize qw/aom_dc_top_predictor_64x16 sse2 avx2/;
specialize qw/aom_dc_left_predictor_4x4 msa neon sse2/;
specialize qw/aom_dc_left_predictor_4x8 sse2/;
specialize qw/aom_dc_left_predictor_4x16 sse2/;
specialize qw/aom_dc_left_predictor_8x4 sse2/;
specialize qw/aom_dc_left_predictor_8x8 neon msa sse2/;
specialize qw/aom_dc_left_predictor_8x16 sse2/;
specialize qw/aom_dc_left_predictor_8x32 sse2/;
specialize qw/aom_dc_left_predictor_16x4 sse2/;
specialize qw/aom_dc_left_predictor_16x8 sse2/;
specialize qw/aom_dc_left_predictor_16x16 neon msa sse2/;
specialize qw/aom_dc_left_predictor_16x32 sse2/;
specialize qw/aom_dc_left_predictor_16x64 sse2/;
specialize qw/aom_dc_left_predictor_32x8 sse2/;
specialize qw/aom_dc_left_predictor_32x16 sse2 avx2/;
specialize qw/aom_dc_left_predictor_32x32 msa neon sse2 avx2/;
specialize qw/aom_dc_left_predictor_32x64 sse2 avx2/;
specialize qw/aom_dc_left_predictor_64x64 sse2 avx2/;
specialize qw/aom_dc_left_predictor_64x32 sse2 avx2/;
specialize qw/aom_dc_left_predictor_64x16 sse2 avx2/;
specialize qw/aom_dc_128_predictor_4x4 msa neon sse2/;
specialize qw/aom_dc_128_predictor_4x8 sse2/;
specialize qw/aom_dc_128_predictor_4x16 sse2/;
specialize qw/aom_dc_128_predictor_8x4 sse2/;
specialize qw/aom_dc_128_predictor_8x8 neon msa sse2/;
specialize qw/aom_dc_128_predictor_8x16 sse2/;
specialize qw/aom_dc_128_predictor_8x32 sse2/;
specialize qw/aom_dc_128_predictor_16x4 sse2/;
specialize qw/aom_dc_128_predictor_16x8 sse2/;
specialize qw/aom_dc_128_predictor_16x16 neon msa sse2/;
specialize qw/aom_dc_128_predictor_16x32 sse2/;
specialize qw/aom_dc_128_predictor_16x64 sse2/;
specialize qw/aom_dc_128_predictor_32x8 sse2/;
specialize qw/aom_dc_128_predictor_32x16 sse2 avx2/;
specialize qw/aom_dc_128_predictor_32x32 msa neon sse2 avx2/;
specialize qw/aom_dc_128_predictor_32x64 sse2 avx2/;
specialize qw/aom_dc_128_predictor_64x64 sse2 avx2/;
specialize qw/aom_dc_128_predictor_64x32 sse2 avx2/;
specialize qw/aom_dc_128_predictor_64x16 sse2 avx2/;
specialize qw/aom_v_predictor_4x4 neon msa sse2/;
specialize qw/aom_v_predictor_4x8 sse2/;
specialize qw/aom_v_predictor_4x16 sse2/;
specialize qw/aom_v_predictor_8x4 sse2/;
specialize qw/aom_v_predictor_8x8 neon msa sse2/;
specialize qw/aom_v_predictor_8x16 sse2/;
specialize qw/aom_v_predictor_8x32 sse2/;
specialize qw/aom_v_predictor_16x4 sse2/;
specialize qw/aom_v_predictor_16x8 sse2/;
specialize qw/aom_v_predictor_16x16 neon msa sse2/;
specialize qw/aom_v_predictor_16x32 sse2/;
specialize qw/aom_v_predictor_16x64 sse2/;
specialize qw/aom_v_predictor_32x8 sse2/;
specialize qw/aom_v_predictor_32x16 sse2 avx2/;
specialize qw/aom_v_predictor_32x32 neon msa sse2 avx2/;
specialize qw/aom_v_predictor_32x64 sse2 avx2/;
specialize qw/aom_v_predictor_64x64 sse2 avx2/;
specialize qw/aom_v_predictor_64x32 sse2 avx2/;
specialize qw/aom_v_predictor_64x16 sse2 avx2/;
specialize qw/aom_h_predictor_4x8 sse2/;
specialize qw/aom_h_predictor_4x16 sse2/;
specialize qw/aom_h_predictor_4x4 neon dspr2 msa sse2/;
specialize qw/aom_h_predictor_8x4 sse2/;
specialize qw/aom_h_predictor_8x8 neon dspr2 msa sse2/;
specialize qw/aom_h_predictor_8x16 sse2/;
specialize qw/aom_h_predictor_8x32 sse2/;
specialize qw/aom_h_predictor_16x4 sse2/;
specialize qw/aom_h_predictor_16x8 sse2/;
specialize qw/aom_h_predictor_16x16 neon dspr2 msa sse2/;
specialize qw/aom_h_predictor_16x32 sse2/;
specialize qw/aom_h_predictor_16x64 sse2/;
specialize qw/aom_h_predictor_32x8 sse2/;
specialize qw/aom_h_predictor_32x16 sse2/;
specialize qw/aom_h_predictor_32x32 neon msa sse2 avx2/;
specialize qw/aom_h_predictor_32x64 sse2/;
specialize qw/aom_h_predictor_64x64 sse2/;
specialize qw/aom_h_predictor_64x32 sse2/;
specialize qw/aom_h_predictor_64x16 sse2/;
specialize qw/aom_paeth_predictor_4x4 ssse3/;
specialize qw/aom_paeth_predictor_4x8 ssse3/;
specialize qw/aom_paeth_predictor_4x16 ssse3/;
specialize qw/aom_paeth_predictor_8x4 ssse3/;
specialize qw/aom_paeth_predictor_8x8 ssse3/;
specialize qw/aom_paeth_predictor_8x16 ssse3/;
specialize qw/aom_paeth_predictor_8x32 ssse3/;
specialize qw/aom_paeth_predictor_16x4 ssse3/;
specialize qw/aom_paeth_predictor_16x8 ssse3 avx2/;
specialize qw/aom_paeth_predictor_16x16 ssse3 avx2/;
specialize qw/aom_paeth_predictor_16x32 ssse3 avx2/;
specialize qw/aom_paeth_predictor_16x64 ssse3 avx2/;
specialize qw/aom_paeth_predictor_32x8 ssse3/;
specialize qw/aom_paeth_predictor_32x16 ssse3 avx2/;
specialize qw/aom_paeth_predictor_32x32 ssse3 avx2/;
specialize qw/aom_paeth_predictor_32x64 ssse3 avx2/;
specialize qw/aom_paeth_predictor_64x32 ssse3 avx2/;
specialize qw/aom_paeth_predictor_64x64 ssse3 avx2/;
specialize qw/aom_paeth_predictor_64x16 ssse3 avx2/;
specialize qw/aom_paeth_predictor_16x8 ssse3/;
specialize qw/aom_paeth_predictor_16x16 ssse3/;
specialize qw/aom_paeth_predictor_16x32 ssse3/;
specialize qw/aom_paeth_predictor_32x16 ssse3/;
specialize qw/aom_paeth_predictor_32x32 ssse3/;
specialize qw/aom_smooth_predictor_4x4 ssse3/;
specialize qw/aom_smooth_predictor_4x8 ssse3/;
specialize qw/aom_smooth_predictor_4x16 ssse3/;
specialize qw/aom_smooth_predictor_8x4 ssse3/;
specialize qw/aom_smooth_predictor_8x8 ssse3/;
specialize qw/aom_smooth_predictor_8x16 ssse3/;
specialize qw/aom_smooth_predictor_8x32 ssse3/;
specialize qw/aom_smooth_predictor_16x4 ssse3/;
specialize qw/aom_smooth_predictor_16x8 ssse3/;
specialize qw/aom_smooth_predictor_16x16 ssse3/;
specialize qw/aom_smooth_predictor_16x32 ssse3/;
specialize qw/aom_smooth_predictor_16x64 ssse3/;
specialize qw/aom_smooth_predictor_32x8 ssse3/;
specialize qw/aom_smooth_predictor_32x16 ssse3/;
specialize qw/aom_smooth_predictor_32x32 ssse3/;
specialize qw/aom_smooth_predictor_32x64 ssse3/;
specialize qw/aom_smooth_predictor_64x64 ssse3/;
specialize qw/aom_smooth_predictor_64x32 ssse3/;
specialize qw/aom_smooth_predictor_64x16 ssse3/;

specialize qw/aom_smooth_v_predictor_4x4 ssse3/;
specialize qw/aom_smooth_v_predictor_4x8 ssse3/;
specialize qw/aom_smooth_v_predictor_4x16 ssse3/;
specialize qw/aom_smooth_v_predictor_8x4 ssse3/;
specialize qw/aom_smooth_v_predictor_8x8 ssse3/;
specialize qw/aom_smooth_v_predictor_8x16 ssse3/;
specialize qw/aom_smooth_v_predictor_8x32 ssse3/;
specialize qw/aom_smooth_v_predictor_16x4 ssse3/;
specialize qw/aom_smooth_v_predictor_16x8 ssse3/;
specialize qw/aom_smooth_v_predictor_16x16 ssse3/;
specialize qw/aom_smooth_v_predictor_16x32 ssse3/;
specialize qw/aom_smooth_v_predictor_16x64 ssse3/;
specialize qw/aom_smooth_v_predictor_32x8 ssse3/;
specialize qw/aom_smooth_v_predictor_32x16 ssse3/;
specialize qw/aom_smooth_v_predictor_32x32 ssse3/;
specialize qw/aom_smooth_v_predictor_32x64 ssse3/;
specialize qw/aom_smooth_v_predictor_64x64 ssse3/;
specialize qw/aom_smooth_v_predictor_64x32 ssse3/;
specialize qw/aom_smooth_v_predictor_64x16 ssse3/;

specialize qw/aom_smooth_h_predictor_4x4 ssse3/;
specialize qw/aom_smooth_h_predictor_4x8 ssse3/;
specialize qw/aom_smooth_h_predictor_4x16 ssse3/;
specialize qw/aom_smooth_h_predictor_8x4 ssse3/;
specialize qw/aom_smooth_h_predictor_8x8 ssse3/;
specialize qw/aom_smooth_h_predictor_8x16 ssse3/;
specialize qw/aom_smooth_h_predictor_8x32 ssse3/;
specialize qw/aom_smooth_h_predictor_16x4 ssse3/;
specialize qw/aom_smooth_h_predictor_16x8 ssse3/;
specialize qw/aom_smooth_h_predictor_16x16 ssse3/;
specialize qw/aom_smooth_h_predictor_16x32 ssse3/;
specialize qw/aom_smooth_h_predictor_16x64 ssse3/;
specialize qw/aom_smooth_h_predictor_32x8 ssse3/;
specialize qw/aom_smooth_h_predictor_32x16 ssse3/;
specialize qw/aom_smooth_h_predictor_32x32 ssse3/;
specialize qw/aom_smooth_h_predictor_32x64 ssse3/;
specialize qw/aom_smooth_h_predictor_64x64 ssse3/;
specialize qw/aom_smooth_h_predictor_64x32 ssse3/;
specialize qw/aom_smooth_h_predictor_64x16 ssse3/;

# TODO(yunqingwang): optimize rectangular DC_PRED to replace division
# by multiply and shift.
specialize qw/aom_dc_predictor_4x4 dspr2 msa neon sse2/;
specialize qw/aom_dc_predictor_4x8 sse2/;
specialize qw/aom_dc_predictor_4x16 sse2/;
specialize qw/aom_dc_predictor_8x4 sse2/;
specialize qw/aom_dc_predictor_8x8 dspr2 neon msa sse2/;
specialize qw/aom_dc_predictor_8x16 sse2/;
specialize qw/aom_dc_predictor_8x32 sse2/;
specialize qw/aom_dc_predictor_16x4 sse2/;
specialize qw/aom_dc_predictor_16x8 sse2/;
specialize qw/aom_dc_predictor_16x16 dspr2 neon msa sse2/;
specialize qw/aom_dc_predictor_16x32 sse2/;
specialize qw/aom_dc_predictor_16x64 sse2/;
specialize qw/aom_dc_predictor_32x8 sse2/;
specialize qw/aom_dc_predictor_32x16 sse2 avx2/;
specialize qw/aom_dc_predictor_32x32 msa neon sse2 avx2/;
specialize qw/aom_dc_predictor_32x64 sse2 avx2/;
specialize qw/aom_dc_predictor_64x64 sse2 avx2/;
specialize qw/aom_dc_predictor_64x32 sse2 avx2/;
specialize qw/aom_dc_predictor_64x16 sse2 avx2/;

  specialize qw/aom_highbd_v_predictor_4x4 sse2/;
  specialize qw/aom_highbd_v_predictor_4x8 sse2/;
  specialize qw/aom_highbd_v_predictor_8x4 sse2/;
  specialize qw/aom_highbd_v_predictor_8x8 sse2/;
  specialize qw/aom_highbd_v_predictor_8x16 sse2/;
  specialize qw/aom_highbd_v_predictor_16x8 sse2/;
  specialize qw/aom_highbd_v_predictor_16x16 sse2/;
  specialize qw/aom_highbd_v_predictor_16x32 sse2/;
  specialize qw/aom_highbd_v_predictor_32x16 sse2/;
  specialize qw/aom_highbd_v_predictor_32x32 sse2/;

  # TODO(yunqingwang): optimize rectangular DC_PRED to replace division
  # by multiply and shift.
  specialize qw/aom_highbd_dc_predictor_4x4 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_4x8 sse2/;
  specialize qw/aom_highbd_dc_predictor_8x4 sse2/;;
  specialize qw/aom_highbd_dc_predictor_8x8 sse2 neon/;;
  specialize qw/aom_highbd_dc_predictor_8x16 sse2/;;
  specialize qw/aom_highbd_dc_predictor_16x8 sse2/;
  specialize qw/aom_highbd_dc_predictor_16x16 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_16x32 sse2/;
  specialize qw/aom_highbd_dc_predictor_32x16 sse2/;
  specialize qw/aom_highbd_dc_predictor_32x32 sse2 neon/;
  specialize qw/aom_highbd_dc_predictor_64x64 neon/;

  specialize qw/aom_highbd_h_predictor_4x4 sse2/;
  specialize qw/aom_highbd_h_predictor_4x8 sse2/;
  specialize qw/aom_highbd_h_predictor_8x4 sse2/;
  specialize qw/aom_highbd_h_predictor_8x8 sse2/;
  specialize qw/aom_highbd_h_predictor_8x16 sse2/;
  specialize qw/aom_highbd_h_predictor_16x8 sse2/;
  specialize qw/aom_highbd_h_predictor_16x16 sse2/;
  specialize qw/aom_highbd_h_predictor_16x32 sse2/;
  specialize qw/aom_highbd_h_predictor_32x16 sse2/;
  specialize qw/aom_highbd_h_predictor_32x32 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_4x4 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_4x4 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_4x4 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_4x8 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_4x8 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_4x8 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_8x4 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_8x4 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_8x4 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_8x8 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_8x8 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_8x8 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_8x16 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_8x16 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_8x16 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_16x8 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_16x8 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_16x8 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_16x16 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_16x16 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_16x16 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_16x32 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_16x32 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_16x32 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_32x16 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_32x16 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_32x16 sse2/;
  specialize qw/aom_highbd_dc_left_predictor_32x32 sse2/;
  specialize qw/aom_highbd_dc_top_predictor_32x32 sse2/;
  specialize qw/aom_highbd_dc_128_predictor_32x32 sse2/;

#
# Sub Pixel Filters
#
add_proto qw/void aom_convolve_copy/,             "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
add_proto qw/void aom_convolve8_horiz/,           "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
add_proto qw/void aom_convolve8_vert/,            "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";

specialize qw/aom_convolve_copy       sse2      /;
specialize qw/aom_convolve8_horiz     sse2 ssse3/, "$avx2_ssse3";
specialize qw/aom_convolve8_vert      sse2 ssse3/, "$avx2_ssse3";

add_proto qw/void aom_highbd_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/aom_highbd_convolve_copy sse2 avx2/;

add_proto qw/void aom_highbd_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/aom_highbd_convolve8_horiz avx2/, "$sse2_x86_64";

add_proto qw/void aom_highbd_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/aom_highbd_convolve8_vert avx2/, "$sse2_x86_64";

#
# Loopfilter
#
add_proto qw/void aom_lpf_vertical_14/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_14 sse2 neon/;

add_proto qw/void aom_lpf_vertical_14_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_14_dual sse2/;

add_proto qw/void aom_lpf_vertical_6/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_6 sse2 neon/;

add_proto qw/void aom_lpf_vertical_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_8 sse2 neon/;

add_proto qw/void aom_lpf_vertical_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_8_dual sse2/;

add_proto qw/void aom_lpf_vertical_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_4 sse2 neon/;

add_proto qw/void aom_lpf_vertical_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_4_dual sse2/;

add_proto qw/void aom_lpf_horizontal_14/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_14 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_14_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_14_dual sse2/;

add_proto qw/void aom_lpf_horizontal_6/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_6 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_6_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_6_dual sse2/;

add_proto qw/void aom_lpf_horizontal_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_8 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_8_dual sse2/;

add_proto qw/void aom_lpf_horizontal_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_horizontal_4 sse2 neon/;

add_proto qw/void aom_lpf_horizontal_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_4_dual sse2/;

add_proto qw/void aom_highbd_lpf_vertical_14/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_vertical_14 sse2/;

add_proto qw/void aom_highbd_lpf_vertical_14_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
specialize qw/aom_highbd_lpf_vertical_14_dual sse2 avx2/;

add_proto qw/void aom_highbd_lpf_vertical_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_vertical_8 sse2/;

add_proto qw/void aom_highbd_lpf_vertical_6/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_vertical_6 sse2/;

add_proto qw/void aom_lpf_vertical_6_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_6_dual sse2/;

add_proto qw/void aom_highbd_lpf_vertical_6_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
specialize qw/aom_highbd_lpf_vertical_6_dual sse2/;

add_proto qw/void aom_highbd_lpf_vertical_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
specialize qw/aom_highbd_lpf_vertical_8_dual sse2 avx2/;

add_proto qw/void aom_highbd_lpf_vertical_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_vertical_4 sse2/;

add_proto qw/void aom_highbd_lpf_vertical_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
specialize qw/aom_highbd_lpf_vertical_4_dual sse2 avx2/;

add_proto qw/void aom_highbd_lpf_horizontal_14/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_horizontal_14 sse2/;

add_proto qw/void aom_highbd_lpf_horizontal_14_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limt1, const uint8_t *thresh1,int bd";
specialize qw/aom_highbd_lpf_horizontal_14_dual sse2 avx2/;

add_proto qw/void aom_highbd_lpf_horizontal_6/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_horizontal_6 sse2/;

add_proto qw/void aom_highbd_lpf_horizontal_6_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
specialize qw/aom_highbd_lpf_horizontal_6_dual sse2/;

add_proto qw/void aom_highbd_lpf_horizontal_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_horizontal_8 sse2/;

add_proto qw/void aom_highbd_lpf_horizontal_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
specialize qw/aom_highbd_lpf_horizontal_8_dual sse2 avx2/;

add_proto qw/void aom_highbd_lpf_horizontal_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
specialize qw/aom_highbd_lpf_horizontal_4 sse2/;

add_proto qw/void aom_highbd_lpf_horizontal_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
specialize qw/aom_highbd_lpf_horizontal_4_dual sse2 avx2/;

# Helper functions.
add_proto qw/void av1_round_shift_array/, "int32_t *arr, int size, int bit";
specialize "av1_round_shift_array", qw/sse4_1 neon/;

#
# Encoder functions.
#

#
# Forward transform
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes"){
    add_proto qw/void aom_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/aom_fdct8x8 sse2/, "$ssse3_x86_64";

    # High bit depth
    add_proto qw/void aom_highbd_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/aom_highbd_fdct8x8 sse2/;

    # FFT/IFFT (float) only used for denoising (and noise power spectral density estimation)
    add_proto qw/void aom_fft2x2_float/, "const float *input, float *temp, float *output";

    add_proto qw/void aom_fft4x4_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft4x4_float                  sse2/;

    add_proto qw/void aom_fft8x8_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft8x8_float avx2             sse2/;

    add_proto qw/void aom_fft16x16_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft16x16_float avx2           sse2/;

    add_proto qw/void aom_fft32x32_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_fft32x32_float avx2           sse2/;

    add_proto qw/void aom_ifft2x2_float/, "const float *input, float *temp, float *output";

    add_proto qw/void aom_ifft4x4_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft4x4_float                 sse2/;

    add_proto qw/void aom_ifft8x8_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft8x8_float avx2            sse2/;

    add_proto qw/void aom_ifft16x16_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft16x16_float avx2          sse2/;

    add_proto qw/void aom_ifft32x32_float/, "const float *input, float *temp, float *output";
    specialize qw/aom_ifft32x32_float avx2          sse2/;
}  # CONFIG_AV1_ENCODER

#
# Quantization
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  add_proto qw/void aom_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_quantize_b sse2/, "$ssse3_x86_64", "$avx_x86_64";

  add_proto qw/void aom_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_quantize_b_32x32/, "$ssse3_x86_64", "$avx_x86_64";

  add_proto qw/void aom_quantize_b_64x64/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
}  # CONFIG_AV1_ENCODER

if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  add_proto qw/void aom_highbd_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_highbd_quantize_b sse2 avx2/;

  add_proto qw/void aom_highbd_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/aom_highbd_quantize_b_32x32 sse2/;

  add_proto qw/void aom_highbd_quantize_b_64x64/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";

}  # CONFIG_AV1_ENCODER

#
# Alpha blending with mask
#
add_proto qw/void aom_lowbd_blend_a64_d16_mask/, "uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, ConvolveParams *conv_params";
specialize qw/aom_lowbd_blend_a64_d16_mask sse4_1 neon/;
add_proto qw/void aom_highbd_blend_a64_d16_mask/, "uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, ConvolveParams *conv_params, const int bd";
add_proto qw/void aom_blend_a64_mask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby";
add_proto qw/void aom_blend_a64_hmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h";
add_proto qw/void aom_blend_a64_vmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h";
specialize "aom_blend_a64_mask", qw/sse4_1/;
specialize "aom_blend_a64_hmask", qw/sse4_1 neon/;
specialize "aom_blend_a64_vmask", qw/sse4_1 neon/;

add_proto qw/void aom_highbd_blend_a64_mask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, int bd";
add_proto qw/void aom_highbd_blend_a64_hmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd";
add_proto qw/void aom_highbd_blend_a64_vmask/, "uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd";
specialize "aom_highbd_blend_a64_mask", qw/sse4_1/;
specialize "aom_highbd_blend_a64_hmask", qw/sse4_1/;
specialize "aom_highbd_blend_a64_vmask", qw/sse4_1/;

if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  #
  # Block subtraction
  #
  add_proto qw/void aom_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride";
  specialize qw/aom_subtract_block neon msa sse2 avx2/;

  add_proto qw/void aom_highbd_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride, int bd";
  specialize qw/aom_highbd_subtract_block sse2/;

  if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
    #
    # Sum of Squares
    #
    add_proto qw/uint64_t aom_sum_squares_2d_i16/, "const int16_t *src, int stride, int width, int height";
    specialize qw/aom_sum_squares_2d_i16 sse2/;

    add_proto qw/uint64_t aom_sum_squares_i16/, "const int16_t *src, uint32_t N";
    specialize qw/aom_sum_squares_i16 sse2/;
  }


  #
  # Single block SAD / Single block Avg SAD
  #
  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_sad${w}x${h}", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
    add_proto qw/unsigned int/, "aom_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
    add_proto qw/unsigned int/, "aom_jnt_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param";
  }

  specialize qw/aom_sad128x128    avx2          sse2/;
  specialize qw/aom_sad128x64     avx2          sse2/;
  specialize qw/aom_sad64x128     avx2          sse2/;
  specialize qw/aom_sad64x64      avx2 neon msa sse2/;
  specialize qw/aom_sad64x32      avx2      msa sse2/;
  specialize qw/aom_sad32x64      avx2      msa sse2/;
  specialize qw/aom_sad32x32      avx2 neon msa sse2/;
  specialize qw/aom_sad32x16      avx2      msa sse2/;
  specialize qw/aom_sad16x32                msa sse2/;
  specialize qw/aom_sad16x16           neon msa sse2/;
  specialize qw/aom_sad16x8            neon msa sse2/;
  specialize qw/aom_sad8x16            neon msa sse2/;
  specialize qw/aom_sad8x8             neon msa sse2/;
  specialize qw/aom_sad8x4                  msa sse2/;
  specialize qw/aom_sad4x8                  msa sse2/;
  specialize qw/aom_sad4x4             neon msa sse2/;

  specialize qw/aom_sad128x128_avg avx2     sse2/;
  specialize qw/aom_sad128x64_avg  avx2     sse2/;
  specialize qw/aom_sad64x128_avg  avx2     sse2/;
  specialize qw/aom_sad64x64_avg   avx2 msa sse2/;
  specialize qw/aom_sad64x32_avg   avx2 msa sse2/;
  specialize qw/aom_sad32x64_avg   avx2 msa sse2/;
  specialize qw/aom_sad32x32_avg   avx2 msa sse2/;
  specialize qw/aom_sad32x16_avg   avx2 msa sse2/;
  specialize qw/aom_sad16x32_avg        msa sse2/;
  specialize qw/aom_sad16x16_avg        msa sse2/;
  specialize qw/aom_sad16x8_avg         msa sse2/;
  specialize qw/aom_sad8x16_avg         msa sse2/;
  specialize qw/aom_sad8x8_avg          msa sse2/;
  specialize qw/aom_sad8x4_avg          msa sse2/;
  specialize qw/aom_sad4x8_avg          msa sse2/;
  specialize qw/aom_sad4x4_avg          msa sse2/;

  specialize qw/aom_sad4x16      sse2/;
  specialize qw/aom_sad16x4      sse2/;
  specialize qw/aom_sad8x32      sse2/;
  specialize qw/aom_sad32x8      sse2/;
  specialize qw/aom_sad16x64     sse2/;
  specialize qw/aom_sad64x16     sse2/;

  specialize qw/aom_sad4x16_avg  sse2/;
  specialize qw/aom_sad16x4_avg  sse2/;
  specialize qw/aom_sad8x32_avg  sse2/;
  specialize qw/aom_sad32x8_avg  sse2/;
  specialize qw/aom_sad16x64_avg sse2/;
  specialize qw/aom_sad64x16_avg sse2/;

  specialize qw/aom_jnt_sad128x128_avg ssse3/;
  specialize qw/aom_jnt_sad128x64_avg  ssse3/;
  specialize qw/aom_jnt_sad64x128_avg  ssse3/;
  specialize qw/aom_jnt_sad64x64_avg   ssse3/;
  specialize qw/aom_jnt_sad64x32_avg   ssse3/;
  specialize qw/aom_jnt_sad32x64_avg   ssse3/;
  specialize qw/aom_jnt_sad32x32_avg   ssse3/;
  specialize qw/aom_jnt_sad32x16_avg   ssse3/;
  specialize qw/aom_jnt_sad16x32_avg   ssse3/;
  specialize qw/aom_jnt_sad16x16_avg   ssse3/;
  specialize qw/aom_jnt_sad16x8_avg    ssse3/;
  specialize qw/aom_jnt_sad8x16_avg    ssse3/;
  specialize qw/aom_jnt_sad8x8_avg     ssse3/;
  specialize qw/aom_jnt_sad8x4_avg     ssse3/;
  specialize qw/aom_jnt_sad4x8_avg     ssse3/;
  specialize qw/aom_jnt_sad4x4_avg     ssse3/;

  specialize qw/aom_jnt_sad4x16_avg     ssse3/;
  specialize qw/aom_jnt_sad16x4_avg     ssse3/;
  specialize qw/aom_jnt_sad8x32_avg     ssse3/;
  specialize qw/aom_jnt_sad32x8_avg     ssse3/;
  specialize qw/aom_jnt_sad16x64_avg     ssse3/;
  specialize qw/aom_jnt_sad64x16_avg     ssse3/;

  add_proto qw/unsigned int/, "aom_sad4xh", "const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height";
  add_proto qw/unsigned int/, "aom_sad8xh", "const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height";
  add_proto qw/unsigned int/, "aom_sad16xh", "const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height";
  add_proto qw/unsigned int/, "aom_sad32xh", "const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height";
  add_proto qw/unsigned int/, "aom_sad64xh", "const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height";
  add_proto qw/unsigned int/, "aom_sad128xh", "const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height";

  specialize qw/aom_sad4xh   sse2/;
  specialize qw/aom_sad8xh   sse2/;
  specialize qw/aom_sad16xh  sse2/;
  specialize qw/aom_sad32xh  sse2/;
  specialize qw/aom_sad64xh  sse2/;
  specialize qw/aom_sad128xh sse2/;


    foreach (@block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_highbd_sad${w}x${h}", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
      add_proto qw/unsigned int/, "aom_highbd_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
      if ($w != 128 && $h != 128 && $w != 4) {
        specialize "aom_highbd_sad${w}x${h}", qw/sse2/;
        specialize "aom_highbd_sad${w}x${h}_avg", qw/sse2/;
      }
      add_proto qw/unsigned int/, "aom_highbd_jnt_sad${w}x${h}_avg", "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param";
    }
    specialize qw/aom_highbd_sad128x128 avx2/;
    specialize qw/aom_highbd_sad128x64  avx2/;
    specialize qw/aom_highbd_sad64x128  avx2/;
    specialize qw/aom_highbd_sad64x64   avx2 sse2/;
    specialize qw/aom_highbd_sad64x32   avx2 sse2/;
    specialize qw/aom_highbd_sad32x64   avx2 sse2/;
    specialize qw/aom_highbd_sad32x32   avx2 sse2/;
    specialize qw/aom_highbd_sad32x16   avx2 sse2/;
    specialize qw/aom_highbd_sad16x32   avx2 sse2/;
    specialize qw/aom_highbd_sad16x16   avx2 sse2/;
    specialize qw/aom_highbd_sad16x8    avx2 sse2/;
    specialize qw/aom_highbd_sad8x4     sse2/;

    specialize qw/aom_highbd_sad128x128_avg avx2/;
    specialize qw/aom_highbd_sad128x64_avg  avx2/;
    specialize qw/aom_highbd_sad64x128_avg  avx2/;
    specialize qw/aom_highbd_sad64x64_avg   avx2 sse2/;
    specialize qw/aom_highbd_sad64x32_avg   avx2 sse2/;
    specialize qw/aom_highbd_sad32x64_avg   avx2 sse2/;
    specialize qw/aom_highbd_sad32x32_avg   avx2 sse2/;
    specialize qw/aom_highbd_sad32x16_avg   avx2 sse2/;
    specialize qw/aom_highbd_sad16x32_avg   avx2 sse2/;
    specialize qw/aom_highbd_sad16x16_avg   avx2 sse2/;
    specialize qw/aom_highbd_sad16x8_avg    avx2 sse2/;
    specialize qw/aom_highbd_sad8x4_avg     sse2/;

    specialize qw/aom_highbd_sad16x4       sse2/;
    specialize qw/aom_highbd_sad8x32       sse2/;
    specialize qw/aom_highbd_sad32x8       sse2/;
    specialize qw/aom_highbd_sad16x64      sse2/;
    specialize qw/aom_highbd_sad64x16      sse2/;

    specialize qw/aom_highbd_sad16x4_avg   sse2/;
    specialize qw/aom_highbd_sad8x32_avg   sse2/;
    specialize qw/aom_highbd_sad32x8_avg   sse2/;
    specialize qw/aom_highbd_sad16x64_avg  sse2/;
    specialize qw/aom_highbd_sad64x16_avg  sse2/;

  #
  # Masked SAD
  #
  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_masked_sad${w}x${h}", "const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask";
    specialize "aom_masked_sad${w}x${h}", qw/ssse3 avx2/;
  }


    foreach (@block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_highbd_masked_sad${w}x${h}", "const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask";
      specialize "aom_highbd_masked_sad${w}x${h}", qw/ssse3 avx2/;
    }


  #
  # OBMC SAD
  #
  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_obmc_sad${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask";
    if (! (($w == 128 && $h == 32) || ($w == 32 && $h == 128))) {
       specialize "aom_obmc_sad${w}x${h}", qw/sse4_1 avx2/;
    }
  }


    foreach (@block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_highbd_obmc_sad${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask";
      if (! (($w == 128 && $h == 32) || ($w == 32 && $h == 128))) {
        specialize "aom_highbd_obmc_sad${w}x${h}", qw/sse4_1 avx2/;
      }
    }


  #
  # Multi-block SAD, comparing a reference to N independent blocks
  #
  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/void/, "aom_sad${w}x${h}x4d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
  }

  specialize qw/aom_sad128x128x4d avx2          sse2/;
  specialize qw/aom_sad128x64x4d  avx2          sse2/;
  specialize qw/aom_sad64x128x4d  avx2          sse2/;
  specialize qw/aom_sad64x64x4d   avx2 neon msa sse2/;
  specialize qw/aom_sad64x32x4d   avx2      msa sse2/;
  specialize qw/aom_sad32x64x4d   avx2      msa sse2/;
  specialize qw/aom_sad32x32x4d   avx2 neon msa sse2/;
  specialize qw/aom_sad32x16x4d             msa sse2/;
  specialize qw/aom_sad16x32x4d             msa sse2/;
  specialize qw/aom_sad16x16x4d        neon msa sse2/;
  specialize qw/aom_sad16x8x4d              msa sse2/;
  specialize qw/aom_sad8x16x4d              msa sse2/;
  specialize qw/aom_sad8x8x4d               msa sse2/;
  specialize qw/aom_sad8x4x4d               msa sse2/;
  specialize qw/aom_sad4x8x4d               msa sse2/;
  specialize qw/aom_sad4x4x4d               msa sse2/;

  specialize qw/aom_sad4x16x4d  sse2/;
  specialize qw/aom_sad16x4x4d  sse2/;
  specialize qw/aom_sad8x32x4d  sse2/;
  specialize qw/aom_sad32x8x4d  sse2/;
  specialize qw/aom_sad16x64x4d sse2/;
  specialize qw/aom_sad64x16x4d sse2/;

  #
  # Multi-block SAD, comparing a reference to N independent blocks
  #
  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/void/, "aom_highbd_sad${w}x${h}x4d", "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
    if ($w != 128 && $h != 128) {
      specialize "aom_highbd_sad${w}x${h}x4d", qw/sse2/;
    }
  }
  specialize qw/aom_highbd_sad128x128x4d avx2/;
  specialize qw/aom_highbd_sad128x64x4d  avx2/;
  specialize qw/aom_highbd_sad64x128x4d  avx2/;
  specialize qw/aom_highbd_sad64x64x4d   sse2 avx2/;
  specialize qw/aom_highbd_sad64x32x4d   sse2 avx2/;
  specialize qw/aom_highbd_sad32x64x4d   sse2 avx2/;
  specialize qw/aom_highbd_sad32x32x4d   sse2 avx2/;
  specialize qw/aom_highbd_sad32x16x4d   sse2 avx2/;
  specialize qw/aom_highbd_sad16x32x4d   sse2 avx2/;
  specialize qw/aom_highbd_sad16x16x4d   sse2 avx2/;
  specialize qw/aom_highbd_sad16x8x4d    sse2 avx2/;
  specialize qw/aom_highbd_sad8x16x4d    sse2/;
  specialize qw/aom_highbd_sad8x8x4d     sse2/;
  specialize qw/aom_highbd_sad8x4x4d     sse2/;
  specialize qw/aom_highbd_sad4x8x4d     sse2/;
  specialize qw/aom_highbd_sad4x4x4d     sse2/;

  specialize qw/aom_highbd_sad4x16x4d  sse2/;
  specialize qw/aom_highbd_sad16x4x4d  sse2/;
  specialize qw/aom_highbd_sad8x32x4d  sse2/;
  specialize qw/aom_highbd_sad32x8x4d  sse2/;
  specialize qw/aom_highbd_sad16x64x4d sse2/;
  specialize qw/aom_highbd_sad64x16x4d sse2/;


  #
  # Structured Similarity (SSIM)
  #
  if (aom_config("CONFIG_INTERNAL_STATS") eq "yes") {
    add_proto qw/void aom_ssim_parms_8x8/, "const uint8_t *s, int sp, const uint8_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
    specialize qw/aom_ssim_parms_8x8/, "$sse2_x86_64";

    add_proto qw/void aom_ssim_parms_16x16/, "const uint8_t *s, int sp, const uint8_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
    specialize qw/aom_ssim_parms_16x16/, "$sse2_x86_64";

    add_proto qw/void aom_highbd_ssim_parms_8x8/, "const uint16_t *s, int sp, const uint16_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";

  }
}  # CONFIG_AV1_ENCODER

if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {

  #
  # Specialty Variance
  #
  add_proto qw/void aom_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  add_proto qw/void aom_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  specialize qw/aom_get16x16var           neon msa/;
  specialize qw/aom_get8x8var             neon msa/;


  add_proto qw/unsigned int aom_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";

  specialize qw/aom_mse16x16          sse2 avx2 neon msa/;
  specialize qw/aom_mse16x8           sse2           msa/;
  specialize qw/aom_mse8x16           sse2           msa/;
  specialize qw/aom_mse8x8            sse2           msa/;

    foreach $bd (8, 10, 12) {
      add_proto qw/void/, "aom_highbd_${bd}_get16x16var", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
      add_proto qw/void/, "aom_highbd_${bd}_get8x8var", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse16x16", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse16x8", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse8x16", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
      add_proto qw/unsigned int/, "aom_highbd_${bd}_mse8x8", "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";

      specialize "aom_highbd_${bd}_mse16x16", qw/sse2/;
      specialize "aom_highbd_${bd}_mse8x8", qw/sse2/;
    }


  #
  #
  #
  add_proto qw/void aom_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                          const MV *const mv, uint8_t *comp_pred, int width, int height, int subpel_x_q3,
                                          int subpel_y_q3, const uint8_t *ref, int ref_stride";
  specialize qw/aom_upsampled_pred sse2/;

  add_proto qw/void aom_comp_avg_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                   const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                   int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                   int ref_stride";
  specialize qw/aom_comp_avg_upsampled_pred sse2/;

  add_proto qw/void aom_jnt_comp_avg_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                       const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                       int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                       int ref_stride, const JNT_COMP_PARAMS *jcp_param";
  specialize qw/aom_jnt_comp_avg_upsampled_pred ssse3/;


  add_proto qw/void aom_highbd_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                 const MV *const mv, uint16_t *comp_pred, int width, int height, int subpel_x_q3,
                                                 int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd";
  specialize qw/aom_highbd_upsampled_pred sse2/;

  add_proto qw/void aom_highbd_comp_avg_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                          const MV *const mv, uint16_t *comp_pred, const uint8_t *pred8, int width,
                                                          int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd";
  specialize qw/aom_highbd_comp_avg_upsampled_pred sse2/;

  add_proto qw/void aom_highbd_jnt_comp_avg_upsampled_pred/, "MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                              const MV *const mv, uint16_t *comp_pred, const uint8_t *pred8, int width,
                                                              int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
                                                              int ref_stride, int bd, const JNT_COMP_PARAMS *jcp_param";
  specialize qw/aom_highbd_jnt_comp_avg_upsampled_pred sse2/;


  #
  #
  #
  add_proto qw/unsigned int aom_get_mb_ss/, "const int16_t *";
  add_proto qw/unsigned int aom_get4x4sse_cs/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int ref_stride";

  specialize qw/aom_get_mb_ss sse2 msa/;
  specialize qw/aom_get4x4sse_cs neon msa/;

  #
  # Variance / Subpixel Variance / Subpixel Avg Variance
  #
  add_proto qw/unsigned int/, "aom_variance2x2", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/unsigned int/, "aom_variance2x4", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/unsigned int/, "aom_variance4x2", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/uint32_t/, "aom_sub_pixel_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    add_proto qw/uint32_t/, "aom_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    add_proto qw/uint32_t/, "aom_jnt_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param";
  }
  specialize qw/aom_variance128x128   sse2 avx2         /;
  specialize qw/aom_variance128x64    sse2 avx2         /;
  specialize qw/aom_variance64x128    sse2 avx2         /;
  specialize qw/aom_variance64x64     sse2 avx2 neon msa/;
  specialize qw/aom_variance64x32     sse2 avx2 neon msa/;
  specialize qw/aom_variance32x64     sse2 avx2 neon msa/;
  specialize qw/aom_variance32x32     sse2 avx2 neon msa/;
  specialize qw/aom_variance32x16     sse2 avx2 msa/;
  specialize qw/aom_variance16x32     sse2 avx2 msa/;
  specialize qw/aom_variance16x16     sse2 avx2 neon msa/;
  specialize qw/aom_variance16x8      sse2 avx2 neon msa/;
  specialize qw/aom_variance8x16      sse2      neon msa/;
  specialize qw/aom_variance8x8       sse2      neon msa/;
  specialize qw/aom_variance8x4       sse2           msa/;
  specialize qw/aom_variance4x8       sse2           msa/;
  specialize qw/aom_variance4x4       sse2           msa/;

  specialize qw/aom_sub_pixel_variance128x128   avx2          sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance128x64    avx2          sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance64x128    avx2          sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance64x64     avx2 neon msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance64x32     avx2      msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance32x64     avx2      msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance32x32     avx2 neon msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance32x16     avx2      msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance16x32               msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance16x16          neon msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance16x8                msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance8x16                msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance8x8            neon msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance8x4                 msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance4x8                 msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance4x4                 msa sse2 ssse3/;

  specialize qw/aom_sub_pixel_avg_variance128x128 avx2     sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance128x64  avx2     sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance64x128  avx2     sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance64x64   avx2 msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance64x32   avx2 msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance32x64   avx2 msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance32x32   avx2 msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance32x16   avx2 msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x32        msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x16        msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x8         msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance8x16         msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance8x8          msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance8x4          msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance4x8          msa sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance4x4          msa sse2 ssse3/;

  specialize qw/aom_variance4x16 sse2/;
  specialize qw/aom_variance16x4 sse2 avx2/;
  specialize qw/aom_variance8x32 sse2/;
  specialize qw/aom_variance32x8 sse2 avx2/;
  specialize qw/aom_variance16x64 sse2 avx2/;
  specialize qw/aom_variance64x16 sse2 avx2/;
  specialize qw/aom_sub_pixel_variance4x16 sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance16x4 sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance8x32 sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance32x8 sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance16x64 sse2 ssse3/;
  specialize qw/aom_sub_pixel_variance64x16 sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance4x16 sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x4 sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance8x32 sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance32x8 sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance16x64 sse2 ssse3/;
  specialize qw/aom_sub_pixel_avg_variance64x16 sse2 ssse3/;

  specialize qw/aom_jnt_sub_pixel_avg_variance64x64 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance64x32 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance32x64 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance32x32 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance32x16 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance16x32 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance16x16 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance16x8  ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance8x16  ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance8x8   ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance8x4   ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance4x8   ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance4x4   ssse3/;

  specialize qw/aom_jnt_sub_pixel_avg_variance4x16  ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance16x4  ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance8x32  ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance32x8  ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance16x64 ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance64x16 ssse3/;

  specialize qw/aom_jnt_sub_pixel_avg_variance128x128  ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance128x64   ssse3/;
  specialize qw/aom_jnt_sub_pixel_avg_variance64x128   ssse3/;


  foreach $bd (8, 10, 12) {
    add_proto qw/unsigned int/, "aom_highbd_${bd}_variance2x2", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

    add_proto qw/unsigned int/, "aom_highbd_${bd}_variance2x4", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

    add_proto qw/unsigned int/, "aom_highbd_${bd}_variance4x2", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

    foreach (@block_sizes) {
      ($w, $h) = @$_;
      add_proto qw/unsigned int/, "aom_highbd_${bd}_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
      add_proto qw/uint32_t/, "aom_highbd_${bd}_sub_pixel_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
      add_proto qw/uint32_t/, "aom_highbd_${bd}_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
      if ($w != 128 && $h != 128 && $w != 4 && $h != 4) {
        specialize "aom_highbd_${bd}_variance${w}x${h}", "sse2";
      }
      # TODO(david.barker): When ext-partition-types is enabled, we currently
      # don't have vectorized 4x16 highbd variance functions
      if ($w == 4 && $h == 4) {
          specialize "aom_highbd_${bd}_variance${w}x${h}", "sse4_1";
        }
      if ($w != 128 && $h != 128 && $w != 4) {
        specialize "aom_highbd_${bd}_sub_pixel_variance${w}x${h}", qw/sse2/;
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance${w}x${h}", qw/sse2/;
      }
      if ($w == 4 && $h == 4) {
        specialize "aom_highbd_${bd}_sub_pixel_variance${w}x${h}", "sse4_1";
        specialize "aom_highbd_${bd}_sub_pixel_avg_variance${w}x${h}", "sse4_1";
      }

      add_proto qw/uint32_t/, "aom_highbd_${bd}_jnt_sub_pixel_avg_variance${w}x${h}", "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param";
    }
  }

  #
  # Masked Variance / Masked Subpixel Variance
  #
  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_masked_sub_pixel_variance${w}x${h}", "const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse";
    specialize "aom_masked_sub_pixel_variance${w}x${h}", qw/ssse3/;
  }


    foreach $bd ("_8_", "_10_", "_12_") {
      foreach (@block_sizes) {
        ($w, $h) = @$_;
        add_proto qw/unsigned int/, "aom_highbd${bd}masked_sub_pixel_variance${w}x${h}", "const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse";
        specialize "aom_highbd${bd}masked_sub_pixel_variance${w}x${h}", qw/ssse3/;
      }
    }


  #
  # OBMC Variance / OBMC Subpixel Variance
  #
  foreach (@block_sizes) {
    ($w, $h) = @$_;
    add_proto qw/unsigned int/, "aom_obmc_variance${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
    add_proto qw/unsigned int/, "aom_obmc_sub_pixel_variance${w}x${h}", "const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
    specialize "aom_obmc_variance${w}x${h}", q/sse4_1/;
    specialize "aom_obmc_sub_pixel_variance${w}x${h}", q/sse4_1/;
  }


    foreach $bd ("_", "_10_", "_12_") {
      foreach (@block_sizes) {
        ($w, $h) = @$_;
        add_proto qw/unsigned int/, "aom_highbd${bd}obmc_variance${w}x${h}", "const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
        add_proto qw/unsigned int/, "aom_highbd${bd}obmc_sub_pixel_variance${w}x${h}", "const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse";
        specialize "aom_highbd${bd}obmc_variance${w}x${h}", qw/sse4_1/;
      }
    }


  add_proto qw/uint32_t aom_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance64x64 avx2 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance64x32 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance32x64 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance32x32 avx2 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance32x16 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance16x32 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance16x16 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance16x8 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance8x16 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance8x8 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance8x4 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance4x8 msa sse2 ssse3/;

  add_proto qw/uint32_t aom_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance4x4 msa sse2 ssse3/;
  #
  # Specialty Subpixel
  #
  add_proto qw/uint32_t aom_variance_halfpixvar16x16_h/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse";
  specialize qw/aom_variance_halfpixvar16x16_h sse2/;

  add_proto qw/uint32_t aom_variance_halfpixvar16x16_v/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse";
  specialize qw/aom_variance_halfpixvar16x16_v sse2/;

  add_proto qw/uint32_t aom_variance_halfpixvar16x16_hv/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse";
  specialize qw/aom_variance_halfpixvar16x16_hv sse2/;

  #
  # Comp Avg
  #
  add_proto qw/void aom_comp_avg_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride";

  add_proto qw/void aom_jnt_comp_avg_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const JNT_COMP_PARAMS *jcp_param";
  specialize qw/aom_jnt_comp_avg_pred ssse3/;


    add_proto qw/unsigned int aom_highbd_12_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance64x64 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance64x32 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance32x64 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance32x32 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance32x16 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance16x32 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance16x16 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance16x8 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance8x16 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_variance8x8 sse2/;

    add_proto qw/unsigned int aom_highbd_12_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_12_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_12_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

    add_proto qw/unsigned int aom_highbd_10_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance64x64 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance64x32 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance32x64 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance32x32 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance32x16 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance16x32 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance16x16 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance16x8 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance8x16 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_variance8x8 sse2/;

    add_proto qw/unsigned int aom_highbd_10_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_10_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_10_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

    add_proto qw/unsigned int aom_highbd_8_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance64x64 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance64x32 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance32x64 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance32x32 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance32x16 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance16x32 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance16x16 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance16x8 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance8x16 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_variance8x8 sse2/;

    add_proto qw/unsigned int aom_highbd_8_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_8_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_8_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

    add_proto qw/void aom_highbd_8_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
    add_proto qw/void aom_highbd_8_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

    add_proto qw/void aom_highbd_10_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
    add_proto qw/void aom_highbd_10_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

    add_proto qw/void aom_highbd_12_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
    add_proto qw/void aom_highbd_12_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

    add_proto qw/unsigned int aom_highbd_8_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_mse16x16 sse2/;

    add_proto qw/unsigned int aom_highbd_8_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_8_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_8_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    specialize qw/aom_highbd_8_mse8x8 sse2/;

    add_proto qw/unsigned int aom_highbd_10_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_mse16x16 sse2/;

    add_proto qw/unsigned int aom_highbd_10_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_10_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_10_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    specialize qw/aom_highbd_10_mse8x8 sse2/;

    add_proto qw/unsigned int aom_highbd_12_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_mse16x16 sse2/;

    add_proto qw/unsigned int aom_highbd_12_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_12_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    add_proto qw/unsigned int aom_highbd_12_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
    specialize qw/aom_highbd_12_mse8x8 sse2/;

    add_proto qw/void aom_highbd_comp_avg_pred/, "uint16_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride";

    add_proto qw/void aom_highbd_jnt_comp_avg_pred/, "uint16_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const JNT_COMP_PARAMS *jcp_param";
    specialize qw/aom_highbd_jnt_comp_avg_pred sse2/;

    #
    # Subpixel Variance
    #
    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance64x64 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance64x32 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance32x64 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance32x32 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance32x16 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance16x32 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance16x16 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance16x8 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance8x16 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance8x8 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_12_sub_pixel_variance8x4 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance64x64 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance64x32 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance32x64 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance32x32 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance32x16 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance16x32 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance16x16 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance16x8 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance8x16 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance8x8 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_10_sub_pixel_variance8x4 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance64x64 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance64x32 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance32x64 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance32x32 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance32x16 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance16x32 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance16x16 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance16x8 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance8x16 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance8x8 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    specialize qw/aom_highbd_8_sub_pixel_variance8x4 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
    add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance64x64 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance64x32 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance32x64 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance32x32 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance32x16 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance16x32 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance16x16 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance16x8 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance8x16 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance8x8 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_12_sub_pixel_avg_variance8x4 sse2/;

    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance64x64 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance64x32 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance32x64 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance32x32 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance32x16 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance16x32 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance16x16 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance16x8 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance8x16 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance8x8 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_10_sub_pixel_avg_variance8x4 sse2/;

    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance64x64 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance64x32 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance32x64 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance32x32 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance32x16 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance16x32 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance16x16 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance16x8 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance8x16 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance8x8 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    specialize qw/aom_highbd_8_sub_pixel_avg_variance8x4 sse2/;

    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
    add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";



  add_proto qw/void aom_comp_mask_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask";
  specialize qw/aom_comp_mask_pred ssse3 avx2/;

  add_proto qw/void aom_highbd_comp_mask_pred/, "uint16_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask";
  specialize qw/aom_highbd_comp_mask_pred avx2/;

}  # CONFIG_AV1_ENCODER

1;
