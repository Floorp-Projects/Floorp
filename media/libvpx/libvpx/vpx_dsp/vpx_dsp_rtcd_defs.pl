sub vpx_dsp_forward_decls() {
print <<EOF
/*
 * DSP
 */

#include "vpx/vpx_integer.h"

EOF
}
forward_decls qw/vpx_dsp_forward_decls/;

# Functions which use x86inc.asm instead of x86_abi_support.asm
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

# Functions which are 64 bit only.
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

if (vpx_config("CONFIG_ENCODERS") eq "yes") {
#
# Single block SAD
#
add_proto qw/unsigned int vpx_sad64x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad64x64 avx2 neon/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad64x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad64x32 avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad32x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad32x64 avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad32x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad32x32 avx2 neon/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad32x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad32x16 avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad16x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad16x32/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad16x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad16x16 mmx media neon/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad16x8 mmx neon/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad8x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad8x16 mmx neon/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad8x8 mmx neon/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad8x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad8x4/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad4x8/, "$sse_x86inc";

add_proto qw/unsigned int vpx_sad4x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/vpx_sad4x4 mmx neon/, "$sse_x86inc";

#
# Avg
#
add_proto qw/unsigned int vpx_sad64x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad64x64_avg avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad64x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad64x32_avg avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad32x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad32x64_avg avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad32x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad32x32_avg avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad32x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad32x16_avg avx2/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad16x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad16x32_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad16x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad16x16_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad16x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad16x8_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad8x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad8x16_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad8x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad8x8_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad8x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad8x4_avg/, "$sse2_x86inc";

add_proto qw/unsigned int vpx_sad4x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad4x8_avg/, "$sse_x86inc";

add_proto qw/unsigned int vpx_sad4x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/vpx_sad4x4_avg/, "$sse_x86inc";

#
# Multi-block SAD, comparing a reference to N blocks 1 pixel apart horizontally
#
# Blocks of 3
add_proto qw/void vpx_sad64x64x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";

add_proto qw/void vpx_sad32x32x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";

add_proto qw/void vpx_sad16x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad16x16x3 sse3 ssse3/;

add_proto qw/void vpx_sad16x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad16x8x3 sse3 ssse3/;

add_proto qw/void vpx_sad8x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad8x16x3 sse3/;

add_proto qw/void vpx_sad8x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad8x8x3 sse3/;

add_proto qw/void vpx_sad4x4x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad4x4x3 sse3/;

# Blocks of 8
add_proto qw/void vpx_sad64x64x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";

add_proto qw/void vpx_sad32x32x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";

add_proto qw/void vpx_sad16x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad16x16x8 sse4_1/;

add_proto qw/void vpx_sad16x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad16x8x8 sse4_1/;

add_proto qw/void vpx_sad8x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad8x16x8 sse4_1/;

add_proto qw/void vpx_sad8x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad8x8x8 sse4_1/;

add_proto qw/void vpx_sad8x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";

add_proto qw/void vpx_sad4x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";

add_proto qw/void vpx_sad4x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad4x4x8 sse4_1/;

#
# Multi-block SAD, comparing a reference to N independent blocks
#
add_proto qw/void vpx_sad64x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad64x64x4d avx2 neon/, "$sse2_x86inc";

add_proto qw/void vpx_sad64x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad64x32x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad32x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad32x64x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad32x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad32x32x4d avx2 neon/, "$sse2_x86inc";

add_proto qw/void vpx_sad32x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad32x16x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad16x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad16x32x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad16x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad16x16x4d neon/, "$sse2_x86inc";

add_proto qw/void vpx_sad16x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad16x8x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad8x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad8x16x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad8x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad8x8x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad8x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad8x4x4d/, "$sse2_x86inc";

add_proto qw/void vpx_sad4x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad4x8x4d/, "$sse_x86inc";

add_proto qw/void vpx_sad4x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/vpx_sad4x4x4d/, "$sse_x86inc";

if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {
  #
  # Single block SAD
  #
  add_proto qw/unsigned int vpx_highbd_sad64x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad64x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad32x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad32x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad32x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad16x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad16x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad8x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad8x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad4x8/;

  add_proto qw/unsigned int vpx_highbd_sad4x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/vpx_highbd_sad4x4/;

  #
  # Avg
  #
  add_proto qw/unsigned int vpx_highbd_sad64x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad64x64_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad64x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad64x32_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad32x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad32x64_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad32x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad32x32_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad32x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad32x16_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad16x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad16x32_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad16x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad16x16_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad16x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad16x8_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad8x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad8x16_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad8x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad8x8_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad8x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad8x4_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int vpx_highbd_sad4x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad4x8_avg/;

  add_proto qw/unsigned int vpx_highbd_sad4x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/vpx_highbd_sad4x4_avg/;

  #
  # Multi-block SAD, comparing a reference to N blocks 1 pixel apart horizontally
  #
  # Blocks of 3
  add_proto qw/void vpx_highbd_sad64x64x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad64x64x3/;

  add_proto qw/void vpx_highbd_sad32x32x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad32x32x3/;

  add_proto qw/void vpx_highbd_sad16x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad16x16x3/;

  add_proto qw/void vpx_highbd_sad16x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad16x8x3/;

  add_proto qw/void vpx_highbd_sad8x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x16x3/;

  add_proto qw/void vpx_highbd_sad8x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x8x3/;

  add_proto qw/void vpx_highbd_sad4x4x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad4x4x3/;

  # Blocks of 8
  add_proto qw/void vpx_highbd_sad64x64x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad64x64x8/;

  add_proto qw/void vpx_highbd_sad32x32x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad32x32x8/;

  add_proto qw/void vpx_highbd_sad16x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad16x16x8/;

  add_proto qw/void vpx_highbd_sad16x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad16x8x8/;

  add_proto qw/void vpx_highbd_sad8x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x16x8/;

  add_proto qw/void vpx_highbd_sad8x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x8x8/;

  add_proto qw/void vpx_highbd_sad8x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x4x8/;

  add_proto qw/void vpx_highbd_sad4x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad4x8x8/;

  add_proto qw/void vpx_highbd_sad4x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad4x4x8/;

  #
  # Multi-block SAD, comparing a reference to N independent blocks
  #
  add_proto qw/void vpx_highbd_sad64x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad64x64x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad64x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad64x32x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad32x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad32x64x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad32x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad32x32x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad32x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad32x16x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad16x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad16x32x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad16x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad16x16x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad16x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad16x8x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad8x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x16x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad8x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x8x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad8x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad8x4x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad4x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad4x8x4d/, "$sse2_x86inc";

  add_proto qw/void vpx_highbd_sad4x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/vpx_highbd_sad4x4x4d/, "$sse2_x86inc";

}  # CONFIG_VP9_HIGHBITDEPTH
}  # CONFIG_ENCODERS

if (vpx_config("CONFIG_ENCODERS") eq "yes" || vpx_config("CONFIG_POSTPROC") eq "yes" || vpx_config("CONFIG_VP9_POSTPROC") eq "yes") {

add_proto qw/unsigned int vpx_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance64x64 sse2 avx2 neon/;

add_proto qw/unsigned int vpx_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance64x32 sse2 avx2 neon/;

add_proto qw/unsigned int vpx_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance32x64 sse2 neon/;

add_proto qw/unsigned int vpx_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance32x32 sse2 avx2 neon/;

add_proto qw/unsigned int vpx_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance32x16 sse2 avx2/;

add_proto qw/unsigned int vpx_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance16x32 sse2/;

add_proto qw/unsigned int vpx_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance16x16 mmx sse2 avx2 media neon/;

add_proto qw/unsigned int vpx_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance16x8 mmx sse2 neon/;

add_proto qw/unsigned int vpx_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance8x16 mmx sse2 neon/;

add_proto qw/unsigned int vpx_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance8x8 mmx sse2 media neon/;

add_proto qw/unsigned int vpx_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance8x4 sse2/;

add_proto qw/unsigned int vpx_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance4x8 sse2/;

add_proto qw/unsigned int vpx_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_variance4x4 mmx sse2/;


add_proto qw/void vpx_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  specialize qw/vpx_get16x16var sse2 avx2 neon/;

add_proto qw/void vpx_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  specialize qw/vpx_get8x8var mmx sse2 neon/;

add_proto qw/unsigned int vpx_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_mse16x16 mmx sse2 avx2 media neon/;

add_proto qw/unsigned int vpx_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_mse16x8 sse2/;

add_proto qw/unsigned int vpx_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_mse8x16 sse2/;

add_proto qw/unsigned int vpx_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_mse8x8 sse2/;

add_proto qw/unsigned int vpx_get_mb_ss/, "const int16_t *";
  specialize qw/vpx_get_mb_ss mmx sse2/;

add_proto qw/unsigned int vpx_get4x4sse_cs/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride";
  specialize qw/vpx_get4x4sse_cs neon/;

add_proto qw/void vpx_comp_avg_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride";

if (vpx_config("CONFIG_VP9_HIGHBITDEPTH") eq "yes") {
  add_proto qw/unsigned int vpx_highbd_12_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance64x64 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance64x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance32x64 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance32x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance32x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance16x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance16x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance16x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance8x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_variance8x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_12_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_12_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/unsigned int vpx_highbd_10_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance64x64 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance64x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance32x64 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance32x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance32x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance16x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance16x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance16x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance8x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_variance8x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_10_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_10_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/unsigned int vpx_highbd_8_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance64x64 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance64x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance32x64 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance32x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance32x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance16x32 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance16x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance16x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance8x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_variance8x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_8_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_8_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/void vpx_highbd_8_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  add_proto qw/void vpx_highbd_8_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  add_proto qw/void vpx_highbd_10_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  add_proto qw/void vpx_highbd_10_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  add_proto qw/void vpx_highbd_12_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  add_proto qw/void vpx_highbd_12_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  add_proto qw/unsigned int vpx_highbd_8_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_mse16x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_8_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_8_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_8_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_highbd_8_mse8x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_mse16x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_10_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_10_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_10_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_highbd_10_mse8x8 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_mse16x16 sse2/;

  add_proto qw/unsigned int vpx_highbd_12_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_12_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int vpx_highbd_12_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/vpx_highbd_12_mse8x8 sse2/;

  add_proto qw/void vpx_highbd_comp_avg_pred/, "uint16_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride";
}  # CONFIG_VP9_HIGHBITDEPTH
}  # CONFIG_ENCODERS || CONFIG_POSTPROC || CONFIG_VP9_POSTPROC

1;
