sub vp9_common_forward_decls() {
print <<EOF
/*
 * VP9
 */

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_enums.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct vp9_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;
EOF
}
forward_decls qw/vp9_common_forward_decls/;

# x86inc.asm doesn't work if pic is enabled on 32 bit platforms so no assembly.
if (vpx_config("CONFIG_USE_X86INC") eq "yes") {
  $mmx_x86inc = 'mmx';
  $sse_x86inc = 'sse';
  $sse2_x86inc = 'sse2';
  $ssse3_x86inc = 'ssse3';
  $avx_x86inc = 'avx';
  $avx2_x86inc = 'avx2';
} else {
  $mmx_x86inc = $sse_x86inc = $sse2_x86inc = $ssse3_x86inc =
  $avx_x86inc = $avx2_x86inc = '';
}

# this variable is for functions that are 64 bit only.
if ($opts{arch} eq "x86_64") {
  $mmx_x86_64 = 'mmx';
  $sse2_x86_64 = 'sse2';
  $ssse3_x86_64 = 'ssse3';
  $avx_x86_64 = 'avx';
  $avx2_x86_64 = 'avx2';
} else {
  $mmx_x86_64 = $sse2_x86_64 = $ssse3_x86_64 =
  $avx_x86_64 = $avx2_x86_64 = '';
}

# optimizations which depend on multiple features
if ((vpx_config("HAVE_AVX2") eq "yes") && (vpx_config("HAVE_SSSE3") eq "yes")) {
  $avx2_ssse3 = 'avx2';
} else {
  $avx2_ssse3 = '';
}

#
# RECON
#
add_proto qw/void vp9_d207_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_4x4 neon dspr2/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_4x4/;

add_proto qw/void vp9_d135_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_4x4/;

add_proto qw/void vp9_d153_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void vp9_v_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_4x4 neon/, "$sse_x86inc";

add_proto qw/void vp9_tm_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_4x4 neon dspr2/, "$sse_x86inc";

add_proto qw/void vp9_dc_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_4x4 dspr2/, "$sse_x86inc";

add_proto qw/void vp9_dc_top_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_4x4/, "$sse_x86inc";

add_proto qw/void vp9_dc_left_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_4x4/, "$sse_x86inc";

add_proto qw/void vp9_dc_128_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_4x4/, "$sse_x86inc";

add_proto qw/void vp9_d207_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_8x8 neon dspr2/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_8x8/;

add_proto qw/void vp9_d135_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_8x8/;

add_proto qw/void vp9_d153_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void vp9_v_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_8x8 neon/, "$sse_x86inc";

add_proto qw/void vp9_tm_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_8x8 neon dspr2/, "$sse2_x86inc";

add_proto qw/void vp9_dc_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_8x8 dspr2 neon/, "$sse_x86inc";

add_proto qw/void vp9_dc_top_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_8x8 neon/, "$sse_x86inc";

add_proto qw/void vp9_dc_left_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_8x8 neon/, "$sse_x86inc";

add_proto qw/void vp9_dc_128_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_8x8 neon/, "$sse_x86inc";

add_proto qw/void vp9_d207_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_16x16 neon dspr2/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_16x16/;

add_proto qw/void vp9_d135_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_16x16/;

add_proto qw/void vp9_d153_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void vp9_v_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_16x16 neon/, "$sse2_x86inc";

add_proto qw/void vp9_tm_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_16x16 neon/, "$sse2_x86inc";

add_proto qw/void vp9_dc_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_16x16 dspr2 neon/, "$sse2_x86inc";

add_proto qw/void vp9_dc_top_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_16x16 neon/, "$sse2_x86inc";

add_proto qw/void vp9_dc_left_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_16x16 neon/, "$sse2_x86inc";

add_proto qw/void vp9_dc_128_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_16x16 neon/, "$sse2_x86inc";

add_proto qw/void vp9_d207_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d207_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void vp9_d45_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d45_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void vp9_d63_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d63_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void vp9_h_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_h_predictor_32x32 neon/, "$ssse3_x86inc";

add_proto qw/void vp9_d117_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d117_predictor_32x32/;

add_proto qw/void vp9_d135_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d135_predictor_32x32/;

add_proto qw/void vp9_d153_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_d153_predictor_32x32/;

add_proto qw/void vp9_v_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_v_predictor_32x32 neon/, "$sse2_x86inc";

add_proto qw/void vp9_tm_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_tm_predictor_32x32 neon/, "$sse2_x86_64";

add_proto qw/void vp9_dc_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_predictor_32x32/, "$sse2_x86inc";

add_proto qw/void vp9_dc_top_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_top_predictor_32x32/, "$sse2_x86inc";

add_proto qw/void vp9_dc_left_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_left_predictor_32x32/, "$sse2_x86inc";

add_proto qw/void vp9_dc_128_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/vp9_dc_128_predictor_32x32/, "$sse2_x86inc";

#
# Loopfilter
#
add_proto qw/void vp9_lpf_vertical_16/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/vp9_lpf_vertical_16 sse2 neon_asm dspr2 msa/;
$vp9_lpf_vertical_16_neon_asm=vp9_lpf_vertical_16_neon;

add_proto qw/void vp9_lpf_vertical_16_dual/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/vp9_lpf_vertical_16_dual sse2 neon_asm dspr2 msa/;
$vp9_lpf_vertical_16_dual_neon_asm=vp9_lpf_vertical_16_dual_neon;

add_proto qw/void vp9_lpf_vertical_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_vertical_8 sse2 neon_asm dspr2 msa/;
$vp9_lpf_vertical_8_neon_asm=vp9_lpf_vertical_8_neon;

add_proto qw/void vp9_lpf_vertical_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_vertical_8_dual sse2 neon_asm dspr2 msa/;
$vp9_lpf_vertical_8_dual_neon_asm=vp9_lpf_vertical_8_dual_neon;

add_proto qw/void vp9_lpf_vertical_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_vertical_4 mmx neon dspr2 msa/;

add_proto qw/void vp9_lpf_vertical_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_vertical_4_dual sse2 neon dspr2 msa/;

add_proto qw/void vp9_lpf_horizontal_16/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_horizontal_16 sse2 avx2 neon_asm dspr2 msa/;
$vp9_lpf_horizontal_16_neon_asm=vp9_lpf_horizontal_16_neon;

add_proto qw/void vp9_lpf_horizontal_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_horizontal_8 sse2 neon_asm dspr2 msa/;
$vp9_lpf_horizontal_8_neon_asm=vp9_lpf_horizontal_8_neon;

add_proto qw/void vp9_lpf_horizontal_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_horizontal_8_dual sse2 neon_asm dspr2 msa/;
$vp9_lpf_horizontal_8_dual_neon_asm=vp9_lpf_horizontal_8_dual_neon;

add_proto qw/void vp9_lpf_horizontal_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/vp9_lpf_horizontal_4 mmx neon dspr2 msa/;

add_proto qw/void vp9_lpf_horizontal_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/vp9_lpf_horizontal_4_dual sse2 neon dspr2 msa/;

#
# post proc
#
if (vpx_config("CONFIG_VP9_POSTPROC") eq "yes") {
add_proto qw/void vp9_mbpost_proc_down/, "uint8_t *dst, int pitch, int rows, int cols, int flimit";
specialize qw/vp9_mbpost_proc_down sse2/;
$vp9_mbpost_proc_down_sse2=vp9_mbpost_proc_down_xmm;

add_proto qw/void vp9_mbpost_proc_across_ip/, "uint8_t *src, int pitch, int rows, int cols, int flimit";
specialize qw/vp9_mbpost_proc_across_ip sse2/;
$vp9_mbpost_proc_across_ip_sse2=vp9_mbpost_proc_across_ip_xmm;

add_proto qw/void vp9_post_proc_down_and_across/, "const uint8_t *src_ptr, uint8_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit";
specialize qw/vp9_post_proc_down_and_across sse2/;
$vp9_post_proc_down_and_across_sse2=vp9_post_proc_down_and_across_xmm;

add_proto qw/void vp9_plane_add_noise/, "uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch";
specialize qw/vp9_plane_add_noise sse2/;
$vp9_plane_add_noise_sse2=vp9_plane_add_noise_wmt;

add_proto qw/void vp9_filter_by_weight16x16/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int src_weight";
specialize qw/vp9_filter_by_weight16x16 sse2/;

add_proto qw/void vp9_filter_by_weight8x8/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int src_weight";
specialize qw/vp9_filter_by_weight8x8 sse2/;
}

#
# Sub Pixel Filters
#
add_proto qw/void vp9_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve_copy neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void vp9_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve_avg neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void vp9_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8 sse2 ssse3 neon dspr2 msa/, "$avx2_ssse3";

add_proto qw/void vp9_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_horiz sse2 ssse3 neon dspr2 msa/, "$avx2_ssse3";

add_proto qw/void vp9_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_vert sse2 ssse3 neon dspr2 msa/, "$avx2_ssse3";

add_proto qw/void vp9_convolve8_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_avg sse2 ssse3 neon dspr2 msa/;

add_proto qw/void vp9_convolve8_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_avg_horiz sse2 ssse3 neon dspr2 msa/;

add_proto qw/void vp9_convolve8_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/vp9_convolve8_avg_vert sse2 ssse3 neon dspr2 msa/;

#
# dct
#
if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {
  # Note as optimized versions of these functions are added we need to add a check to ensure
  # that when CONFIG_EMULATE_HARDWARE is on, it defaults to the C versions only.
  add_proto qw/void vp9_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct4x4_1_add/;

  add_proto qw/void vp9_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct4x4_16_add/;

  add_proto qw/void vp9_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct8x8_1_add/;

  add_proto qw/void vp9_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct8x8_64_add/;

  add_proto qw/void vp9_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct8x8_12_add/;

  add_proto qw/void vp9_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct16x16_1_add/;

  add_proto qw/void vp9_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct16x16_256_add/;

  add_proto qw/void vp9_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct16x16_10_add/;

  add_proto qw/void vp9_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct32x32_1024_add/;

  add_proto qw/void vp9_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct32x32_34_add/;

  add_proto qw/void vp9_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_idct32x32_1_add/;

  add_proto qw/void vp9_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
  specialize qw/vp9_iht4x4_16_add/;

  add_proto qw/void vp9_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
  specialize qw/vp9_iht8x8_64_add/;

  add_proto qw/void vp9_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type";
  specialize qw/vp9_iht16x16_256_add/;

  # dct and add

  add_proto qw/void vp9_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_iwht4x4_1_add/;

  add_proto qw/void vp9_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/vp9_iwht4x4_16_add/;

} else {
  # Force C versions if CONFIG_EMULATE_HARDWARE is 1
  if (vpx_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void vp9_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct4x4_1_add/;

    add_proto qw/void vp9_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct4x4_16_add/;

    add_proto qw/void vp9_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct8x8_1_add/;

    add_proto qw/void vp9_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct8x8_64_add/;

    add_proto qw/void vp9_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct8x8_12_add/;

    add_proto qw/void vp9_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct16x16_1_add/;

    add_proto qw/void vp9_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct16x16_256_add/;

    add_proto qw/void vp9_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct16x16_10_add/;

    add_proto qw/void vp9_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct32x32_1024_add/;

    add_proto qw/void vp9_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct32x32_34_add/;

    add_proto qw/void vp9_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct32x32_1_add/;

    add_proto qw/void vp9_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/vp9_iht4x4_16_add/;

    add_proto qw/void vp9_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/vp9_iht8x8_64_add/;

    add_proto qw/void vp9_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type";
    specialize qw/vp9_iht16x16_256_add/;

    # dct and add

    add_proto qw/void vp9_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_iwht4x4_1_add/;

    add_proto qw/void vp9_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_iwht4x4_16_add/;
  } else {
    add_proto qw/void vp9_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct4x4_1_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct4x4_16_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct8x8_1_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct8x8_64_add sse2 neon dspr2 msa/, "$ssse3_x86_64";

    add_proto qw/void vp9_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct8x8_12_add sse2 neon dspr2 msa/, "$ssse3_x86_64";

    add_proto qw/void vp9_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct16x16_1_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct16x16_256_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct16x16_10_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct32x32_1024_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct32x32_34_add sse2 neon_asm dspr2 msa/;
    #is this a typo?
    $vp9_idct32x32_34_add_neon_asm=vp9_idct32x32_1024_add_neon;

    add_proto qw/void vp9_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_idct32x32_1_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/vp9_iht4x4_16_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/vp9_iht8x8_64_add sse2 neon dspr2 msa/;

    add_proto qw/void vp9_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type";
    specialize qw/vp9_iht16x16_256_add sse2 dspr2 msa/;

    # dct and add

    add_proto qw/void vp9_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_iwht4x4_1_add msa/;

    add_proto qw/void vp9_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/vp9_iwht4x4_16_add msa/;
  }
}

# High bitdepth functions
if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {
  #
  # Intra prediction
  #
  add_proto qw/void vp9_highbd_d207_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d207_predictor_4x4/;

  add_proto qw/void vp9_highbd_d45_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d45_predictor_4x4/;

  add_proto qw/void vp9_highbd_d63_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d63_predictor_4x4/;

  add_proto qw/void vp9_highbd_h_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_h_predictor_4x4/;

  add_proto qw/void vp9_highbd_d117_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d117_predictor_4x4/;

  add_proto qw/void vp9_highbd_d135_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d135_predictor_4x4/;

  add_proto qw/void vp9_highbd_d153_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d153_predictor_4x4/;

  add_proto qw/void vp9_highbd_v_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_v_predictor_4x4/, "$sse_x86inc";

  add_proto qw/void vp9_highbd_tm_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_tm_predictor_4x4/, "$sse_x86inc";

  add_proto qw/void vp9_highbd_dc_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_predictor_4x4/, "$sse_x86inc";

  add_proto qw/void vp9_highbd_dc_top_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_top_predictor_4x4/;

  add_proto qw/void vp9_highbd_dc_left_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_left_predictor_4x4/;

  add_proto qw/void vp9_highbd_dc_128_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_128_predictor_4x4/;

  add_proto qw/void vp9_highbd_d207_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d207_predictor_8x8/;

  add_proto qw/void vp9_highbd_d45_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d45_predictor_8x8/;

  add_proto qw/void vp9_highbd_d63_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d63_predictor_8x8/;

  add_proto qw/void vp9_highbd_h_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_h_predictor_8x8/;

  add_proto qw/void vp9_highbd_d117_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d117_predictor_8x8/;

  add_proto qw/void vp9_highbd_d135_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d135_predictor_8x8/;

  add_proto qw/void vp9_highbd_d153_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d153_predictor_8x8/;

  add_proto qw/void vp9_highbd_v_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_v_predictor_8x8/, "$sse2_x86inc";

  add_proto qw/void vp9_highbd_tm_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_tm_predictor_8x8/, "$sse2_x86inc";

  add_proto qw/void vp9_highbd_dc_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_predictor_8x8/, "$sse2_x86inc";;

  add_proto qw/void vp9_highbd_dc_top_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_top_predictor_8x8/;

  add_proto qw/void vp9_highbd_dc_left_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_left_predictor_8x8/;

  add_proto qw/void vp9_highbd_dc_128_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_128_predictor_8x8/;

  add_proto qw/void vp9_highbd_d207_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d207_predictor_16x16/;

  add_proto qw/void vp9_highbd_d45_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d45_predictor_16x16/;

  add_proto qw/void vp9_highbd_d63_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d63_predictor_16x16/;

  add_proto qw/void vp9_highbd_h_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_h_predictor_16x16/;

  add_proto qw/void vp9_highbd_d117_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d117_predictor_16x16/;

  add_proto qw/void vp9_highbd_d135_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d135_predictor_16x16/;

  add_proto qw/void vp9_highbd_d153_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d153_predictor_16x16/;

  add_proto qw/void vp9_highbd_v_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_v_predictor_16x16/, "$sse2_x86inc";

  add_proto qw/void vp9_highbd_tm_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_tm_predictor_16x16/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_dc_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_predictor_16x16/, "$sse2_x86inc";

  add_proto qw/void vp9_highbd_dc_top_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_top_predictor_16x16/;

  add_proto qw/void vp9_highbd_dc_left_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_left_predictor_16x16/;

  add_proto qw/void vp9_highbd_dc_128_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_128_predictor_16x16/;

  add_proto qw/void vp9_highbd_d207_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d207_predictor_32x32/;

  add_proto qw/void vp9_highbd_d45_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d45_predictor_32x32/;

  add_proto qw/void vp9_highbd_d63_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d63_predictor_32x32/;

  add_proto qw/void vp9_highbd_h_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_h_predictor_32x32/;

  add_proto qw/void vp9_highbd_d117_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d117_predictor_32x32/;

  add_proto qw/void vp9_highbd_d135_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d135_predictor_32x32/;

  add_proto qw/void vp9_highbd_d153_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_d153_predictor_32x32/;

  add_proto qw/void vp9_highbd_v_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_v_predictor_32x32/, "$sse2_x86inc";

  add_proto qw/void vp9_highbd_tm_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_tm_predictor_32x32/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_dc_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_predictor_32x32/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_dc_top_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_top_predictor_32x32/;

  add_proto qw/void vp9_highbd_dc_left_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_left_predictor_32x32/;

  add_proto qw/void vp9_highbd_dc_128_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/vp9_highbd_dc_128_predictor_32x32/;

  #
  # Sub Pixel Filters
  #
  add_proto qw/void vp9_highbd_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve_copy/;

  add_proto qw/void vp9_highbd_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve_avg/;

  add_proto qw/void vp9_highbd_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve8/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve8_horiz/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve8_vert/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_convolve8_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve8_avg/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_convolve8_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve8_avg_horiz/, "$sse2_x86_64";

  add_proto qw/void vp9_highbd_convolve8_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/vp9_highbd_convolve8_avg_vert/, "$sse2_x86_64";

  #
  # Loopfilter
  #
  add_proto qw/void vp9_highbd_lpf_vertical_16/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/vp9_highbd_lpf_vertical_16 sse2/;

  add_proto qw/void vp9_highbd_lpf_vertical_16_dual/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/vp9_highbd_lpf_vertical_16_dual sse2/;

  add_proto qw/void vp9_highbd_lpf_vertical_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/vp9_highbd_lpf_vertical_8 sse2/;

  add_proto qw/void vp9_highbd_lpf_vertical_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/vp9_highbd_lpf_vertical_8_dual sse2/;

  add_proto qw/void vp9_highbd_lpf_vertical_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/vp9_highbd_lpf_vertical_4 sse2/;

  add_proto qw/void vp9_highbd_lpf_vertical_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/vp9_highbd_lpf_vertical_4_dual sse2/;

  add_proto qw/void vp9_highbd_lpf_horizontal_16/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/vp9_highbd_lpf_horizontal_16 sse2/;

  add_proto qw/void vp9_highbd_lpf_horizontal_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/vp9_highbd_lpf_horizontal_8 sse2/;

  add_proto qw/void vp9_highbd_lpf_horizontal_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/vp9_highbd_lpf_horizontal_8_dual sse2/;

  add_proto qw/void vp9_highbd_lpf_horizontal_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/vp9_highbd_lpf_horizontal_4 sse2/;

  add_proto qw/void vp9_highbd_lpf_horizontal_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/vp9_highbd_lpf_horizontal_4_dual sse2/;

  #
  # post proc
  #
  if (vpx_config("CONFIG_VP9_POSTPROC") eq "yes") {
    add_proto qw/void vp9_highbd_mbpost_proc_down/, "uint16_t *dst, int pitch, int rows, int cols, int flimit";
    specialize qw/vp9_highbd_mbpost_proc_down/;

    add_proto qw/void vp9_highbd_mbpost_proc_across_ip/, "uint16_t *src, int pitch, int rows, int cols, int flimit";
    specialize qw/vp9_highbd_mbpost_proc_across_ip/;

    add_proto qw/void vp9_highbd_post_proc_down_and_across/, "const uint16_t *src_ptr, uint16_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit";
    specialize qw/vp9_highbd_post_proc_down_and_across/;

    add_proto qw/void vp9_highbd_plane_add_noise/, "uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch";
    specialize qw/vp9_highbd_plane_add_noise/;
  }

  #
  # dct
  #
  # Note as optimized versions of these functions are added we need to add a check to ensure
  # that when CONFIG_EMULATE_HARDWARE is on, it defaults to the C versions only.
  add_proto qw/void vp9_highbd_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_idct4x4_1_add/;

  add_proto qw/void vp9_highbd_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_idct8x8_1_add/;

  add_proto qw/void vp9_highbd_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_idct16x16_1_add/;

  add_proto qw/void vp9_highbd_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_idct32x32_1024_add/;

  add_proto qw/void vp9_highbd_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_idct32x32_34_add/;

  add_proto qw/void vp9_highbd_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_idct32x32_1_add/;

  add_proto qw/void vp9_highbd_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd";
  specialize qw/vp9_highbd_iht4x4_16_add/;

  add_proto qw/void vp9_highbd_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd";
  specialize qw/vp9_highbd_iht8x8_64_add/;

  add_proto qw/void vp9_highbd_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type, int bd";
  specialize qw/vp9_highbd_iht16x16_256_add/;

  # dct and add

  add_proto qw/void vp9_highbd_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_iwht4x4_1_add/;

  add_proto qw/void vp9_highbd_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/vp9_highbd_iwht4x4_16_add/;

  # Force C versions if CONFIG_EMULATE_HARDWARE is 1
  if (vpx_config("CONFIG_EMULATE_HARDWARE") eq "yes") {

    add_proto qw/void vp9_highbd_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct4x4_16_add/;

    add_proto qw/void vp9_highbd_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct8x8_64_add/;

    add_proto qw/void vp9_highbd_idct8x8_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct8x8_10_add/;

    add_proto qw/void vp9_highbd_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct16x16_256_add/;

    add_proto qw/void vp9_highbd_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct16x16_10_add/;

  } else {

    add_proto qw/void vp9_highbd_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct4x4_16_add sse2/;

    add_proto qw/void vp9_highbd_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct8x8_64_add sse2/;

    add_proto qw/void vp9_highbd_idct8x8_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct8x8_10_add sse2/;

    add_proto qw/void vp9_highbd_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct16x16_256_add sse2/;

    add_proto qw/void vp9_highbd_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/vp9_highbd_idct16x16_10_add sse2/;
  }
}

#
# Encoder functions below this point.
#
if (vpx_config("CONFIG_VP9_ENCODER") eq "yes") {


# variance
add_proto qw/unsigned int vp9_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance64x64 avx2 neon/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance64x64 avx2/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance32x64/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance32x64/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance64x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance64x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance32x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance32x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance16x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance16x32/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance32x32 avx2 neon/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance32x32 avx2/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance16x16 neon/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance16x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance8x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance8x16/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance16x8/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance16x8/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance8x8 neon/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance8x8/, "$sse2_x86inc", "$ssse3_x86inc";

# TODO(jingning): need to convert 8x4/4x8 functions into mmx/sse form
add_proto qw/unsigned int vp9_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance8x4/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance8x4/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance4x8/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance4x8/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
specialize qw/vp9_sub_pixel_variance4x4/, "$sse_x86inc", "$ssse3_x86inc";
#vp9_sub_pixel_variance4x4_sse2=vp9_sub_pixel_variance4x4_wmt

add_proto qw/unsigned int vp9_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
specialize qw/vp9_sub_pixel_avg_variance4x4/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/unsigned int vp9_avg_8x8/, "const uint8_t *, int p";
specialize qw/vp9_avg_8x8 sse2 neon/;

add_proto qw/unsigned int vp9_avg_4x4/, "const uint8_t *, int p";
specialize qw/vp9_avg_4x4 sse2/;

add_proto qw/void vp9_minmax_8x8/, "const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max";
specialize qw/vp9_minmax_8x8 sse2/;

add_proto qw/void vp9_hadamard_8x8/, "int16_t const *src_diff, int src_stride, int16_t *coeff";
specialize qw/vp9_hadamard_8x8 sse2/, "$ssse3_x86_64";

add_proto qw/void vp9_hadamard_16x16/, "int16_t const *src_diff, int src_stride, int16_t *coeff";
specialize qw/vp9_hadamard_16x16 sse2/;

add_proto qw/int16_t vp9_satd/, "const int16_t *coeff, int length";
specialize qw/vp9_satd sse2/;

add_proto qw/void vp9_int_pro_row/, "int16_t *hbuf, uint8_t const *ref, const int ref_stride, const int height";
specialize qw/vp9_int_pro_row sse2/;

add_proto qw/int16_t vp9_int_pro_col/, "uint8_t const *ref, const int width";
specialize qw/vp9_int_pro_col sse2/;

add_proto qw/int vp9_vector_var/, "int16_t const *ref, int16_t const *src, const int bwl";
specialize qw/vp9_vector_var sse2/;

if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {
  add_proto qw/unsigned int vp9_highbd_avg_8x8/, "const uint8_t *, int p";
  specialize qw/vp9_highbd_avg_8x8/;
  add_proto qw/unsigned int vp9_highbd_avg_4x4/, "const uint8_t *, int p";
  specialize qw/vp9_highbd_avg_4x4/;
  add_proto qw/void vp9_highbd_minmax_8x8/, "const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max";
  specialize qw/vp9_highbd_minmax_8x8/;
}

# ENCODEMB INVOKE

add_proto qw/void vp9_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride";
specialize qw/vp9_subtract_block neon/, "$sse2_x86inc";

#
# Denoiser
#
if (vpx_config("CONFIG_VP9_TEMPORAL_DENOISING") eq "yes") {
  add_proto qw/int vp9_denoiser_filter/, "const uint8_t *sig, int sig_stride, const uint8_t *mc_avg, int mc_avg_stride, uint8_t *avg, int avg_stride, int increase_denoising, BLOCK_SIZE bs, int motion_magnitude";
  specialize qw/vp9_denoiser_filter sse2/;
}

if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {
# the transform coefficients are held in 32-bit
# values, so the assembler code for  vp9_block_error can no longer be used.
  add_proto qw/int64_t vp9_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
  specialize qw/vp9_block_error/;

  add_proto qw/void vp9_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_fp/;

  add_proto qw/void vp9_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_fp_32x32/;

  add_proto qw/void vp9_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_b/;

  add_proto qw/void vp9_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_b_32x32/;

  add_proto qw/void vp9_fdct8x8_quant/, "const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_fdct8x8_quant/;
} else {
  add_proto qw/int64_t vp9_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
  specialize qw/vp9_block_error avx2/, "$sse2_x86inc";

  add_proto qw/int64_t vp9_block_error_fp/, "const int16_t *coeff, const int16_t *dqcoeff, int block_size";
  specialize qw/vp9_block_error_fp/, "$sse2_x86inc";

  add_proto qw/void vp9_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_fp neon sse2/, "$ssse3_x86_64";

  add_proto qw/void vp9_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_fp_32x32/, "$ssse3_x86_64";

  add_proto qw/void vp9_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_b sse2/, "$ssse3_x86_64";

  add_proto qw/void vp9_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_quantize_b_32x32/, "$ssse3_x86_64";

  add_proto qw/void vp9_fdct8x8_quant/, "const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_fdct8x8_quant sse2 ssse3 neon/;
}

#
# Structured Similarity (SSIM)
#
if (vpx_config("CONFIG_INTERNAL_STATS") eq "yes") {
    add_proto qw/void vp9_ssim_parms_8x8/, "uint8_t *s, int sp, uint8_t *r, int rp, unsigned long *sum_s, unsigned long *sum_r, unsigned long *sum_sq_s, unsigned long *sum_sq_r, unsigned long *sum_sxr";
    specialize qw/vp9_ssim_parms_8x8/, "$sse2_x86_64";

    add_proto qw/void vp9_ssim_parms_16x16/, "uint8_t *s, int sp, uint8_t *r, int rp, unsigned long *sum_s, unsigned long *sum_r, unsigned long *sum_sq_s, unsigned long *sum_sq_r, unsigned long *sum_sxr";
    specialize qw/vp9_ssim_parms_16x16/, "$sse2_x86_64";
}

# fdct functions

if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void vp9_fht4x4/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_fht4x4 sse2/;

  add_proto qw/void vp9_fht8x8/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_fht8x8 sse2/;

  add_proto qw/void vp9_fht16x16/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_fht16x16 sse2/;

  add_proto qw/void vp9_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fwht4x4/, "$mmx_x86inc";

  add_proto qw/void vp9_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct4x4_1 sse2/;

  add_proto qw/void vp9_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct4x4 sse2/;

  add_proto qw/void vp9_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct8x8_1 sse2/;

  add_proto qw/void vp9_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct8x8 sse2/;

  add_proto qw/void vp9_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct16x16_1 sse2/;

  add_proto qw/void vp9_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct16x16 sse2/;

  add_proto qw/void vp9_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct32x32_1 sse2/;

  add_proto qw/void vp9_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct32x32 sse2/;

  add_proto qw/void vp9_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct32x32_rd sse2/;
} else {
  add_proto qw/void vp9_fht4x4/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_fht4x4 sse2/;

  add_proto qw/void vp9_fht8x8/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_fht8x8 sse2/;

  add_proto qw/void vp9_fht16x16/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_fht16x16 sse2/;

  add_proto qw/void vp9_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fwht4x4/, "$mmx_x86inc";

  add_proto qw/void vp9_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct4x4_1 sse2/;

  add_proto qw/void vp9_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct4x4 sse2/;

  add_proto qw/void vp9_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct8x8_1 sse2 neon/;

  add_proto qw/void vp9_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct8x8 sse2 neon/, "$ssse3_x86_64";

  add_proto qw/void vp9_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct16x16_1 sse2/;

  add_proto qw/void vp9_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct16x16 sse2/;

  add_proto qw/void vp9_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct32x32_1 sse2/;

  add_proto qw/void vp9_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct32x32 sse2 avx2/;

  add_proto qw/void vp9_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_fdct32x32_rd sse2 avx2/;
}

#
# Motion search
#
add_proto qw/int vp9_full_search_sad/, "const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv";
specialize qw/vp9_full_search_sad sse3 sse4_1/;
$vp9_full_search_sad_sse3=vp9_full_search_sadx3;
$vp9_full_search_sad_sse4_1=vp9_full_search_sadx8;

add_proto qw/int vp9_diamond_search_sad/, "const struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv";
specialize qw/vp9_diamond_search_sad/;

add_proto qw/int vp9_full_range_search/, "const struct macroblock *x, const struct search_site_config *cfg, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv";
specialize qw/vp9_full_range_search/;

add_proto qw/void vp9_temporal_filter_apply/, "uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count";
specialize qw/vp9_temporal_filter_apply sse2/;

if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance4x8/;

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance4x8/;

  add_proto qw/unsigned int vp9_highbd_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_sub_pixel_variance4x4/;

  add_proto qw/unsigned int vp9_highbd_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_sub_pixel_avg_variance4x4/;

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance4x8/;

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance4x8/;

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_10_sub_pixel_variance4x4/;

  add_proto qw/unsigned int vp9_highbd_10_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_10_sub_pixel_avg_variance4x4/;

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance4x8/;

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance4x8/;

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vp9_highbd_12_sub_pixel_variance4x4/;

  add_proto qw/unsigned int vp9_highbd_12_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred";
  specialize qw/vp9_highbd_12_sub_pixel_avg_variance4x4/;


  # ENCODEMB INVOKE

  add_proto qw/int64_t vp9_highbd_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz, int bd";
  specialize qw/vp9_highbd_block_error sse2/;

  add_proto qw/void vp9_highbd_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride, int bd";
  specialize qw/vp9_highbd_subtract_block/;

  add_proto qw/void vp9_highbd_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_highbd_quantize_fp/;

  add_proto qw/void vp9_highbd_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_highbd_quantize_fp_32x32/;

  add_proto qw/void vp9_highbd_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_highbd_quantize_b sse2/;

  add_proto qw/void vp9_highbd_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/vp9_highbd_quantize_b_32x32 sse2/;

  #
  # Structured Similarity (SSIM)
  #
  if (vpx_config("CONFIG_INTERNAL_STATS") eq "yes") {
    add_proto qw/void vp9_highbd_ssim_parms_8x8/, "uint16_t *s, int sp, uint16_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
    specialize qw/vp9_highbd_ssim_parms_8x8/;
  }

  # fdct functions
  add_proto qw/void vp9_highbd_fht4x4/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_highbd_fht4x4 sse2/;

  add_proto qw/void vp9_highbd_fht8x8/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_highbd_fht8x8 sse2/;

  add_proto qw/void vp9_highbd_fht16x16/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/vp9_highbd_fht16x16 sse2/;

  add_proto qw/void vp9_highbd_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fwht4x4/;

  add_proto qw/void vp9_highbd_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct4x4 sse2/;

  add_proto qw/void vp9_highbd_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct8x8_1/;

  add_proto qw/void vp9_highbd_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct8x8 sse2/;

  add_proto qw/void vp9_highbd_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct16x16_1/;

  add_proto qw/void vp9_highbd_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct16x16 sse2/;

  add_proto qw/void vp9_highbd_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct32x32_1/;

  add_proto qw/void vp9_highbd_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct32x32 sse2/;

  add_proto qw/void vp9_highbd_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/vp9_highbd_fdct32x32_rd sse2/;

  add_proto qw/void vp9_highbd_temporal_filter_apply/, "uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count";
  specialize qw/vp9_highbd_temporal_filter_apply/;

}
# End vp9_high encoder functions

}
# end encoder functions
1;
