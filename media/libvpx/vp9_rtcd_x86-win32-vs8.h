#ifndef VP9_RTCD_H_
#define VP9_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * VP9
 */

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_enums.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct vp9_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;

int64_t vp9_block_error_c(const int16_t *coeff, const int16_t *dqcoeff, intptr_t block_size, int64_t *ssz);
int64_t vp9_block_error_sse2(const int16_t *coeff, const int16_t *dqcoeff, intptr_t block_size, int64_t *ssz);
RTCD_EXTERN int64_t (*vp9_block_error)(const int16_t *coeff, const int16_t *dqcoeff, intptr_t block_size, int64_t *ssz);

void vp9_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_avg)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_avg_horiz)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_avg_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_avg_vert)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_horiz)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve8_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve8_vert)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve_avg_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve_avg)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void vp9_convolve_copy_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*vp9_convolve_copy)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void vp9_d117_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_16x16 vp9_d117_predictor_16x16_c

void vp9_d117_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_32x32 vp9_d117_predictor_32x32_c

void vp9_d117_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_4x4 vp9_d117_predictor_4x4_c

void vp9_d117_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_8x8 vp9_d117_predictor_8x8_c

void vp9_d135_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_16x16 vp9_d135_predictor_16x16_c

void vp9_d135_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_32x32 vp9_d135_predictor_32x32_c

void vp9_d135_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_4x4 vp9_d135_predictor_4x4_c

void vp9_d135_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_8x8 vp9_d135_predictor_8x8_c

void vp9_d153_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d153_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d153_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d153_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_32x32 vp9_d153_predictor_32x32_c

void vp9_d153_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d153_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d153_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d153_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d153_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d153_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d207_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d207_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d207_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d207_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d207_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d207_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d207_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d207_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d207_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d207_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d207_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d207_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d45_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d45_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d45_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d45_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d45_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d45_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d45_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d45_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d45_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d45_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d45_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d45_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d63_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d63_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d63_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d63_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d63_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d63_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d63_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d63_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d63_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_d63_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_d63_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_d63_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_dc_128_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_16x16 vp9_dc_128_predictor_16x16_c

void vp9_dc_128_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_32x32 vp9_dc_128_predictor_32x32_c

void vp9_dc_128_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_4x4 vp9_dc_128_predictor_4x4_c

void vp9_dc_128_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_8x8 vp9_dc_128_predictor_8x8_c

void vp9_dc_left_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_16x16 vp9_dc_left_predictor_16x16_c

void vp9_dc_left_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_32x32 vp9_dc_left_predictor_32x32_c

void vp9_dc_left_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_4x4 vp9_dc_left_predictor_4x4_c

void vp9_dc_left_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_8x8 vp9_dc_left_predictor_8x8_c

void vp9_dc_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_dc_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_dc_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_dc_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_dc_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_dc_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_dc_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_dc_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_dc_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_dc_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_dc_predictor_8x8_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_dc_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_dc_top_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_16x16 vp9_dc_top_predictor_16x16_c

void vp9_dc_top_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_32x32 vp9_dc_top_predictor_32x32_c

void vp9_dc_top_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_4x4 vp9_dc_top_predictor_4x4_c

void vp9_dc_top_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_8x8 vp9_dc_top_predictor_8x8_c

int vp9_diamond_search_sad_c(const struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv);
#define vp9_diamond_search_sad vp9_diamond_search_sad_c

void vp9_fdct16x16_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct16x16_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct16x16)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct16x16_1_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct16x16_1_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct16x16_1)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct32x32_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct32x32_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct32x32)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct32x32_1_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct32x32_1_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct32x32_1)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct32x32_rd_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct32x32_rd_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct32x32_rd)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct4x4_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct4x4_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct4x4)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct4x4_1_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct4x4_1_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct4x4_1)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct8x8_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct8x8_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct8x8)(const int16_t *input, int16_t *output, int stride);

void vp9_fdct8x8_1_c(const int16_t *input, int16_t *output, int stride);
void vp9_fdct8x8_1_sse2(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fdct8x8_1)(const int16_t *input, int16_t *output, int stride);

void vp9_fht16x16_c(const int16_t *input, int16_t *output, int stride, int tx_type);
void vp9_fht16x16_sse2(const int16_t *input, int16_t *output, int stride, int tx_type);
RTCD_EXTERN void (*vp9_fht16x16)(const int16_t *input, int16_t *output, int stride, int tx_type);

void vp9_fht4x4_c(const int16_t *input, int16_t *output, int stride, int tx_type);
void vp9_fht4x4_sse2(const int16_t *input, int16_t *output, int stride, int tx_type);
RTCD_EXTERN void (*vp9_fht4x4)(const int16_t *input, int16_t *output, int stride, int tx_type);

void vp9_fht8x8_c(const int16_t *input, int16_t *output, int stride, int tx_type);
void vp9_fht8x8_sse2(const int16_t *input, int16_t *output, int stride, int tx_type);
RTCD_EXTERN void (*vp9_fht8x8)(const int16_t *input, int16_t *output, int stride, int tx_type);

int vp9_full_range_search_c(const struct macroblock *x, const struct search_site_config *cfg, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv);
#define vp9_full_range_search vp9_full_range_search_c

int vp9_full_search_sad_c(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
int vp9_full_search_sadx3(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
int vp9_full_search_sadx8(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
RTCD_EXTERN int (*vp9_full_search_sad)(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);

void vp9_fwht4x4_c(const int16_t *input, int16_t *output, int stride);
void vp9_fwht4x4_mmx(const int16_t *input, int16_t *output, int stride);
RTCD_EXTERN void (*vp9_fwht4x4)(const int16_t *input, int16_t *output, int stride);

void vp9_get16x16var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vp9_get16x16var_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
RTCD_EXTERN void (*vp9_get16x16var)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);

void vp9_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vp9_get8x8var_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
RTCD_EXTERN void (*vp9_get8x8var)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);

unsigned int vp9_get_mb_ss_c(const int16_t *);
unsigned int vp9_get_mb_ss_sse2(const int16_t *);
RTCD_EXTERN unsigned int (*vp9_get_mb_ss)(const int16_t *);

void vp9_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_h_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_h_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_h_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_h_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_h_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_idct16x16_10_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_10_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_10_add_ssse3(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct16x16_10_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct16x16_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_1_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct16x16_1_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct16x16_256_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_256_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct16x16_256_add_ssse3(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct16x16_256_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct32x32_1024_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_1024_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct32x32_1024_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct32x32_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_1_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct32x32_1_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct32x32_34_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct32x32_34_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct32x32_34_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct4x4_16_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct4x4_16_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct4x4_16_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct4x4_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct4x4_1_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct4x4_1_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct8x8_12_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_12_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct8x8_12_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct8x8_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_1_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct8x8_1_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_idct8x8_64_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
void vp9_idct8x8_64_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride);
RTCD_EXTERN void (*vp9_idct8x8_64_add)(const int16_t *input, uint8_t *dest, int dest_stride);

void vp9_iht16x16_256_add_c(const int16_t *input, uint8_t *output, int pitch, int tx_type);
void vp9_iht16x16_256_add_sse2(const int16_t *input, uint8_t *output, int pitch, int tx_type);
RTCD_EXTERN void (*vp9_iht16x16_256_add)(const int16_t *input, uint8_t *output, int pitch, int tx_type);

void vp9_iht4x4_16_add_c(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
void vp9_iht4x4_16_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
RTCD_EXTERN void (*vp9_iht4x4_16_add)(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);

void vp9_iht8x8_64_add_c(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
void vp9_iht8x8_64_add_sse2(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
RTCD_EXTERN void (*vp9_iht8x8_64_add)(const int16_t *input, uint8_t *dest, int dest_stride, int tx_type);

void vp9_iwht4x4_16_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_iwht4x4_16_add vp9_iwht4x4_16_add_c

void vp9_iwht4x4_1_add_c(const int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_iwht4x4_1_add vp9_iwht4x4_1_add_c

void vp9_lpf_horizontal_16_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_horizontal_16_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vp9_lpf_horizontal_16)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vp9_lpf_horizontal_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_horizontal_4_mmx(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vp9_lpf_horizontal_4)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vp9_lpf_horizontal_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_horizontal_4_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vp9_lpf_horizontal_4_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void vp9_lpf_horizontal_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_horizontal_8_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vp9_lpf_horizontal_8)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vp9_lpf_horizontal_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_horizontal_8_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vp9_lpf_horizontal_8_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void vp9_lpf_vertical_16_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void vp9_lpf_vertical_16_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*vp9_lpf_vertical_16)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void vp9_lpf_vertical_16_dual_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void vp9_lpf_vertical_16_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*vp9_lpf_vertical_16_dual)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void vp9_lpf_vertical_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_vertical_4_mmx(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vp9_lpf_vertical_4)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vp9_lpf_vertical_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_vertical_4_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vp9_lpf_vertical_4_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void vp9_lpf_vertical_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
void vp9_lpf_vertical_8_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
RTCD_EXTERN void (*vp9_lpf_vertical_8)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);

void vp9_lpf_vertical_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void vp9_lpf_vertical_8_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*vp9_lpf_vertical_8_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

unsigned int vp9_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vp9_mse16x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_mse16x16)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vp9_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vp9_mse16x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_mse16x8)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vp9_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vp9_mse8x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_mse8x16)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vp9_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vp9_mse8x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_mse8x8)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

void vp9_quantize_b_c(const int16_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, const int16_t *dequant_ptr, int zbin_oq_value, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp9_quantize_b vp9_quantize_b_c

void vp9_quantize_b_32x32_c(const int16_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, const int16_t *dequant_ptr, int zbin_oq_value, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp9_quantize_b_32x32 vp9_quantize_b_32x32_c

void vp9_quantize_fp_c(const int16_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, const int16_t *dequant_ptr, int zbin_oq_value, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp9_quantize_fp vp9_quantize_fp_c

void vp9_quantize_fp_32x32_c(const int16_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr, const int16_t *dequant_ptr, int zbin_oq_value, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define vp9_quantize_fp_32x32 vp9_quantize_fp_32x32_c

int vp9_refining_search_sad_c(const struct macroblock *x, struct mv *ref_mv, int sad_per_bit, int distance, const struct vp9_variance_vtable *fn_ptr, const struct mv *center_mv);
#define vp9_refining_search_sad vp9_refining_search_sad_c

unsigned int vp9_sad16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
unsigned int vp9_sad16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad16x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);

unsigned int vp9_sad16x16_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad16x16_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad16x16_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);

void vp9_sad16x16x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x16x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x16x3_ssse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad16x16x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad16x16x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad16x16x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad16x16x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp9_sad16x16x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad16x16x8 vp9_sad16x16x8_c

unsigned int vp9_sad16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad16x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vp9_sad16x32_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad16x32_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad16x32_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vp9_sad16x32x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad16x32x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad16x32x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

unsigned int vp9_sad16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
unsigned int vp9_sad16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad16x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);

unsigned int vp9_sad16x8_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad16x8_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad16x8_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);

void vp9_sad16x8x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x8x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad16x8x3_ssse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad16x8x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad16x8x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad16x8x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad16x8x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp9_sad16x8x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad16x8x8 vp9_sad16x8x8_c

unsigned int vp9_sad32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad32x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vp9_sad32x16_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad32x16_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad32x16_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vp9_sad32x16x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad32x16x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad32x16x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

unsigned int vp9_sad32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
unsigned int vp9_sad32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad32x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);

unsigned int vp9_sad32x32_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad32x32_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad32x32_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);

void vp9_sad32x32x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
#define vp9_sad32x32x3 vp9_sad32x32x3_c

void vp9_sad32x32x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad32x32x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad32x32x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp9_sad32x32x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad32x32x8 vp9_sad32x32x8_c

unsigned int vp9_sad32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad32x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vp9_sad32x64_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad32x64_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad32x64_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vp9_sad32x64x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad32x64x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad32x64x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

unsigned int vp9_sad4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
unsigned int vp9_sad4x4_sse(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad4x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);

unsigned int vp9_sad4x4_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad4x4_avg_sse(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad4x4_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);

void vp9_sad4x4x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad4x4x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad4x4x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad4x4x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad4x4x4d_sse(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad4x4x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp9_sad4x4x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad4x4x8 vp9_sad4x4x8_c

unsigned int vp9_sad4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad4x8_sse(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad4x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vp9_sad4x8_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad4x8_avg_sse(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad4x8_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vp9_sad4x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array);
void vp9_sad4x8x4d_sse(const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad4x8x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array);

void vp9_sad4x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vp9_sad4x8x8 vp9_sad4x8x8_c

unsigned int vp9_sad64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad64x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vp9_sad64x32_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad64x32_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad64x32_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vp9_sad64x32x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad64x32x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad64x32x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

unsigned int vp9_sad64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
unsigned int vp9_sad64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad64x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);

unsigned int vp9_sad64x64_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad64x64_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad64x64_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);

void vp9_sad64x64x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
#define vp9_sad64x64x3 vp9_sad64x64x3_c

void vp9_sad64x64x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad64x64x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad64x64x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp9_sad64x64x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad64x64x8 vp9_sad64x64x8_c

unsigned int vp9_sad8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
unsigned int vp9_sad8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad8x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);

unsigned int vp9_sad8x16_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad8x16_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad8x16_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);

void vp9_sad8x16x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad8x16x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad8x16x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad8x16x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad8x16x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad8x16x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp9_sad8x16x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad8x16x8 vp9_sad8x16x8_c

unsigned int vp9_sad8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vp9_sad8x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad8x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vp9_sad8x4_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad8x4_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad8x4_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vp9_sad8x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array);
void vp9_sad8x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad8x4x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, unsigned int *sad_array);

void vp9_sad8x4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vp9_sad8x4x8 vp9_sad8x4x8_c

unsigned int vp9_sad8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
unsigned int vp9_sad8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp9_sad8x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride);

unsigned int vp9_sad8x8_avg_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
unsigned int vp9_sad8x8_avg_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sad8x8_avg)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, const uint8_t *second_pred);

void vp9_sad8x8x3_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp9_sad8x8x3_sse3(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad8x8x3)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp9_sad8x8x4d_c(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp9_sad8x8x4d_sse2(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp9_sad8x8x4d)(const uint8_t *src_ptr, int  src_stride, const uint8_t* const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp9_sad8x8x8_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int  ref_stride, uint32_t *sad_array);
#define vp9_sad8x8x8 vp9_sad8x8x8_c

unsigned int vp9_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance16x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance16x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance32x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance32x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance32x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance32x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance32x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance32x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance4x4_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance4x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance4x8_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance4x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance4x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance64x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance64x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance64x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance64x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance8x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance8x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance8x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance8x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
unsigned int vp9_sub_pixel_avg_variance8x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_avg_variance8x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, const uint8_t *second_pred);

unsigned int vp9_sub_pixel_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance16x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance32x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance32x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance32x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance32x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance32x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance32x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance4x4_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance4x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance4x8_sse(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance4x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance4x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance64x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance64x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance64x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance64x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance8x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance8x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_sub_pixel_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_sub_pixel_variance8x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_sub_pixel_variance8x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

void vp9_subtract_block_c(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);
void vp9_subtract_block_sse2(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);
RTCD_EXTERN void (*vp9_subtract_block)(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);

void vp9_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
void vp9_temporal_filter_apply_sse2(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
RTCD_EXTERN void (*vp9_temporal_filter_apply)(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);

void vp9_tm_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_tm_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_tm_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_tm_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_32x32 vp9_tm_predictor_32x32_c

void vp9_tm_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_tm_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_tm_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_tm_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_tm_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_tm_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_v_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_v_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_4x4_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_v_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void vp9_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void vp9_v_predictor_8x8_sse(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*vp9_v_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

unsigned int vp9_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance16x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance16x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance16x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance32x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance32x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance32x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance4x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance4x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance4x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance4x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance64x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance64x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance8x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance8x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vp9_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vp9_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp9_variance8x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

void vp9_rtcd(void);

#ifdef RTCD_C
#include "vpx_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;

    vp9_block_error = vp9_block_error_c;
    if (flags & HAS_SSE2) vp9_block_error = vp9_block_error_sse2;
    vp9_convolve8 = vp9_convolve8_c;
    if (flags & HAS_SSE2) vp9_convolve8 = vp9_convolve8_sse2;
    if (flags & HAS_SSSE3) vp9_convolve8 = vp9_convolve8_ssse3;
    vp9_convolve8_avg = vp9_convolve8_avg_c;
    if (flags & HAS_SSE2) vp9_convolve8_avg = vp9_convolve8_avg_sse2;
    if (flags & HAS_SSSE3) vp9_convolve8_avg = vp9_convolve8_avg_ssse3;
    vp9_convolve8_avg_horiz = vp9_convolve8_avg_horiz_c;
    if (flags & HAS_SSE2) vp9_convolve8_avg_horiz = vp9_convolve8_avg_horiz_sse2;
    if (flags & HAS_SSSE3) vp9_convolve8_avg_horiz = vp9_convolve8_avg_horiz_ssse3;
    vp9_convolve8_avg_vert = vp9_convolve8_avg_vert_c;
    if (flags & HAS_SSE2) vp9_convolve8_avg_vert = vp9_convolve8_avg_vert_sse2;
    if (flags & HAS_SSSE3) vp9_convolve8_avg_vert = vp9_convolve8_avg_vert_ssse3;
    vp9_convolve8_horiz = vp9_convolve8_horiz_c;
    if (flags & HAS_SSE2) vp9_convolve8_horiz = vp9_convolve8_horiz_sse2;
    if (flags & HAS_SSSE3) vp9_convolve8_horiz = vp9_convolve8_horiz_ssse3;
    vp9_convolve8_vert = vp9_convolve8_vert_c;
    if (flags & HAS_SSE2) vp9_convolve8_vert = vp9_convolve8_vert_sse2;
    if (flags & HAS_SSSE3) vp9_convolve8_vert = vp9_convolve8_vert_ssse3;
    vp9_convolve_avg = vp9_convolve_avg_c;
    if (flags & HAS_SSE2) vp9_convolve_avg = vp9_convolve_avg_sse2;
    vp9_convolve_copy = vp9_convolve_copy_c;
    if (flags & HAS_SSE2) vp9_convolve_copy = vp9_convolve_copy_sse2;
    vp9_d153_predictor_16x16 = vp9_d153_predictor_16x16_c;
    if (flags & HAS_SSSE3) vp9_d153_predictor_16x16 = vp9_d153_predictor_16x16_ssse3;
    vp9_d153_predictor_4x4 = vp9_d153_predictor_4x4_c;
    if (flags & HAS_SSSE3) vp9_d153_predictor_4x4 = vp9_d153_predictor_4x4_ssse3;
    vp9_d153_predictor_8x8 = vp9_d153_predictor_8x8_c;
    if (flags & HAS_SSSE3) vp9_d153_predictor_8x8 = vp9_d153_predictor_8x8_ssse3;
    vp9_d207_predictor_16x16 = vp9_d207_predictor_16x16_c;
    if (flags & HAS_SSSE3) vp9_d207_predictor_16x16 = vp9_d207_predictor_16x16_ssse3;
    vp9_d207_predictor_32x32 = vp9_d207_predictor_32x32_c;
    if (flags & HAS_SSSE3) vp9_d207_predictor_32x32 = vp9_d207_predictor_32x32_ssse3;
    vp9_d207_predictor_4x4 = vp9_d207_predictor_4x4_c;
    if (flags & HAS_SSSE3) vp9_d207_predictor_4x4 = vp9_d207_predictor_4x4_ssse3;
    vp9_d207_predictor_8x8 = vp9_d207_predictor_8x8_c;
    if (flags & HAS_SSSE3) vp9_d207_predictor_8x8 = vp9_d207_predictor_8x8_ssse3;
    vp9_d45_predictor_16x16 = vp9_d45_predictor_16x16_c;
    if (flags & HAS_SSSE3) vp9_d45_predictor_16x16 = vp9_d45_predictor_16x16_ssse3;
    vp9_d45_predictor_32x32 = vp9_d45_predictor_32x32_c;
    if (flags & HAS_SSSE3) vp9_d45_predictor_32x32 = vp9_d45_predictor_32x32_ssse3;
    vp9_d45_predictor_4x4 = vp9_d45_predictor_4x4_c;
    if (flags & HAS_SSSE3) vp9_d45_predictor_4x4 = vp9_d45_predictor_4x4_ssse3;
    vp9_d45_predictor_8x8 = vp9_d45_predictor_8x8_c;
    if (flags & HAS_SSSE3) vp9_d45_predictor_8x8 = vp9_d45_predictor_8x8_ssse3;
    vp9_d63_predictor_16x16 = vp9_d63_predictor_16x16_c;
    if (flags & HAS_SSSE3) vp9_d63_predictor_16x16 = vp9_d63_predictor_16x16_ssse3;
    vp9_d63_predictor_32x32 = vp9_d63_predictor_32x32_c;
    if (flags & HAS_SSSE3) vp9_d63_predictor_32x32 = vp9_d63_predictor_32x32_ssse3;
    vp9_d63_predictor_4x4 = vp9_d63_predictor_4x4_c;
    if (flags & HAS_SSSE3) vp9_d63_predictor_4x4 = vp9_d63_predictor_4x4_ssse3;
    vp9_d63_predictor_8x8 = vp9_d63_predictor_8x8_c;
    if (flags & HAS_SSSE3) vp9_d63_predictor_8x8 = vp9_d63_predictor_8x8_ssse3;
    vp9_dc_predictor_16x16 = vp9_dc_predictor_16x16_c;
    if (flags & HAS_SSE2) vp9_dc_predictor_16x16 = vp9_dc_predictor_16x16_sse2;
    vp9_dc_predictor_32x32 = vp9_dc_predictor_32x32_c;
    if (flags & HAS_SSE2) vp9_dc_predictor_32x32 = vp9_dc_predictor_32x32_sse2;
    vp9_dc_predictor_4x4 = vp9_dc_predictor_4x4_c;
    if (flags & HAS_SSE) vp9_dc_predictor_4x4 = vp9_dc_predictor_4x4_sse;
    vp9_dc_predictor_8x8 = vp9_dc_predictor_8x8_c;
    if (flags & HAS_SSE) vp9_dc_predictor_8x8 = vp9_dc_predictor_8x8_sse;
    vp9_fdct16x16 = vp9_fdct16x16_c;
    if (flags & HAS_SSE2) vp9_fdct16x16 = vp9_fdct16x16_sse2;
    vp9_fdct16x16_1 = vp9_fdct16x16_1_c;
    if (flags & HAS_SSE2) vp9_fdct16x16_1 = vp9_fdct16x16_1_sse2;
    vp9_fdct32x32 = vp9_fdct32x32_c;
    if (flags & HAS_SSE2) vp9_fdct32x32 = vp9_fdct32x32_sse2;
    vp9_fdct32x32_1 = vp9_fdct32x32_1_c;
    if (flags & HAS_SSE2) vp9_fdct32x32_1 = vp9_fdct32x32_1_sse2;
    vp9_fdct32x32_rd = vp9_fdct32x32_rd_c;
    if (flags & HAS_SSE2) vp9_fdct32x32_rd = vp9_fdct32x32_rd_sse2;
    vp9_fdct4x4 = vp9_fdct4x4_c;
    if (flags & HAS_SSE2) vp9_fdct4x4 = vp9_fdct4x4_sse2;
    vp9_fdct4x4_1 = vp9_fdct4x4_1_c;
    if (flags & HAS_SSE2) vp9_fdct4x4_1 = vp9_fdct4x4_1_sse2;
    vp9_fdct8x8 = vp9_fdct8x8_c;
    if (flags & HAS_SSE2) vp9_fdct8x8 = vp9_fdct8x8_sse2;
    vp9_fdct8x8_1 = vp9_fdct8x8_1_c;
    if (flags & HAS_SSE2) vp9_fdct8x8_1 = vp9_fdct8x8_1_sse2;
    vp9_fht16x16 = vp9_fht16x16_c;
    if (flags & HAS_SSE2) vp9_fht16x16 = vp9_fht16x16_sse2;
    vp9_fht4x4 = vp9_fht4x4_c;
    if (flags & HAS_SSE2) vp9_fht4x4 = vp9_fht4x4_sse2;
    vp9_fht8x8 = vp9_fht8x8_c;
    if (flags & HAS_SSE2) vp9_fht8x8 = vp9_fht8x8_sse2;
    vp9_full_search_sad = vp9_full_search_sad_c;
    if (flags & HAS_SSE3) vp9_full_search_sad = vp9_full_search_sadx3;
    if (flags & HAS_SSE4_1) vp9_full_search_sad = vp9_full_search_sadx8;
    vp9_fwht4x4 = vp9_fwht4x4_c;
    if (flags & HAS_MMX) vp9_fwht4x4 = vp9_fwht4x4_mmx;
    vp9_get16x16var = vp9_get16x16var_c;
    if (flags & HAS_SSE2) vp9_get16x16var = vp9_get16x16var_sse2;
    vp9_get8x8var = vp9_get8x8var_c;
    if (flags & HAS_SSE2) vp9_get8x8var = vp9_get8x8var_sse2;
    vp9_get_mb_ss = vp9_get_mb_ss_c;
    if (flags & HAS_SSE2) vp9_get_mb_ss = vp9_get_mb_ss_sse2;
    vp9_h_predictor_16x16 = vp9_h_predictor_16x16_c;
    if (flags & HAS_SSSE3) vp9_h_predictor_16x16 = vp9_h_predictor_16x16_ssse3;
    vp9_h_predictor_32x32 = vp9_h_predictor_32x32_c;
    if (flags & HAS_SSSE3) vp9_h_predictor_32x32 = vp9_h_predictor_32x32_ssse3;
    vp9_h_predictor_4x4 = vp9_h_predictor_4x4_c;
    if (flags & HAS_SSSE3) vp9_h_predictor_4x4 = vp9_h_predictor_4x4_ssse3;
    vp9_h_predictor_8x8 = vp9_h_predictor_8x8_c;
    if (flags & HAS_SSSE3) vp9_h_predictor_8x8 = vp9_h_predictor_8x8_ssse3;
    vp9_idct16x16_10_add = vp9_idct16x16_10_add_c;
    if (flags & HAS_SSE2) vp9_idct16x16_10_add = vp9_idct16x16_10_add_sse2;
    if (flags & HAS_SSSE3) vp9_idct16x16_10_add = vp9_idct16x16_10_add_ssse3;
    vp9_idct16x16_1_add = vp9_idct16x16_1_add_c;
    if (flags & HAS_SSE2) vp9_idct16x16_1_add = vp9_idct16x16_1_add_sse2;
    vp9_idct16x16_256_add = vp9_idct16x16_256_add_c;
    if (flags & HAS_SSE2) vp9_idct16x16_256_add = vp9_idct16x16_256_add_sse2;
    if (flags & HAS_SSSE3) vp9_idct16x16_256_add = vp9_idct16x16_256_add_ssse3;
    vp9_idct32x32_1024_add = vp9_idct32x32_1024_add_c;
    if (flags & HAS_SSE2) vp9_idct32x32_1024_add = vp9_idct32x32_1024_add_sse2;
    vp9_idct32x32_1_add = vp9_idct32x32_1_add_c;
    if (flags & HAS_SSE2) vp9_idct32x32_1_add = vp9_idct32x32_1_add_sse2;
    vp9_idct32x32_34_add = vp9_idct32x32_34_add_c;
    if (flags & HAS_SSE2) vp9_idct32x32_34_add = vp9_idct32x32_34_add_sse2;
    vp9_idct4x4_16_add = vp9_idct4x4_16_add_c;
    if (flags & HAS_SSE2) vp9_idct4x4_16_add = vp9_idct4x4_16_add_sse2;
    vp9_idct4x4_1_add = vp9_idct4x4_1_add_c;
    if (flags & HAS_SSE2) vp9_idct4x4_1_add = vp9_idct4x4_1_add_sse2;
    vp9_idct8x8_12_add = vp9_idct8x8_12_add_c;
    if (flags & HAS_SSE2) vp9_idct8x8_12_add = vp9_idct8x8_12_add_sse2;
    vp9_idct8x8_1_add = vp9_idct8x8_1_add_c;
    if (flags & HAS_SSE2) vp9_idct8x8_1_add = vp9_idct8x8_1_add_sse2;
    vp9_idct8x8_64_add = vp9_idct8x8_64_add_c;
    if (flags & HAS_SSE2) vp9_idct8x8_64_add = vp9_idct8x8_64_add_sse2;
    vp9_iht16x16_256_add = vp9_iht16x16_256_add_c;
    if (flags & HAS_SSE2) vp9_iht16x16_256_add = vp9_iht16x16_256_add_sse2;
    vp9_iht4x4_16_add = vp9_iht4x4_16_add_c;
    if (flags & HAS_SSE2) vp9_iht4x4_16_add = vp9_iht4x4_16_add_sse2;
    vp9_iht8x8_64_add = vp9_iht8x8_64_add_c;
    if (flags & HAS_SSE2) vp9_iht8x8_64_add = vp9_iht8x8_64_add_sse2;
    vp9_lpf_horizontal_16 = vp9_lpf_horizontal_16_c;
    if (flags & HAS_SSE2) vp9_lpf_horizontal_16 = vp9_lpf_horizontal_16_sse2;
    vp9_lpf_horizontal_4 = vp9_lpf_horizontal_4_c;
    if (flags & HAS_MMX) vp9_lpf_horizontal_4 = vp9_lpf_horizontal_4_mmx;
    vp9_lpf_horizontal_4_dual = vp9_lpf_horizontal_4_dual_c;
    if (flags & HAS_SSE2) vp9_lpf_horizontal_4_dual = vp9_lpf_horizontal_4_dual_sse2;
    vp9_lpf_horizontal_8 = vp9_lpf_horizontal_8_c;
    if (flags & HAS_SSE2) vp9_lpf_horizontal_8 = vp9_lpf_horizontal_8_sse2;
    vp9_lpf_horizontal_8_dual = vp9_lpf_horizontal_8_dual_c;
    if (flags & HAS_SSE2) vp9_lpf_horizontal_8_dual = vp9_lpf_horizontal_8_dual_sse2;
    vp9_lpf_vertical_16 = vp9_lpf_vertical_16_c;
    if (flags & HAS_SSE2) vp9_lpf_vertical_16 = vp9_lpf_vertical_16_sse2;
    vp9_lpf_vertical_16_dual = vp9_lpf_vertical_16_dual_c;
    if (flags & HAS_SSE2) vp9_lpf_vertical_16_dual = vp9_lpf_vertical_16_dual_sse2;
    vp9_lpf_vertical_4 = vp9_lpf_vertical_4_c;
    if (flags & HAS_MMX) vp9_lpf_vertical_4 = vp9_lpf_vertical_4_mmx;
    vp9_lpf_vertical_4_dual = vp9_lpf_vertical_4_dual_c;
    if (flags & HAS_SSE2) vp9_lpf_vertical_4_dual = vp9_lpf_vertical_4_dual_sse2;
    vp9_lpf_vertical_8 = vp9_lpf_vertical_8_c;
    if (flags & HAS_SSE2) vp9_lpf_vertical_8 = vp9_lpf_vertical_8_sse2;
    vp9_lpf_vertical_8_dual = vp9_lpf_vertical_8_dual_c;
    if (flags & HAS_SSE2) vp9_lpf_vertical_8_dual = vp9_lpf_vertical_8_dual_sse2;
    vp9_mse16x16 = vp9_mse16x16_c;
    if (flags & HAS_SSE2) vp9_mse16x16 = vp9_mse16x16_sse2;
    vp9_mse16x8 = vp9_mse16x8_c;
    if (flags & HAS_SSE2) vp9_mse16x8 = vp9_mse16x8_sse2;
    vp9_mse8x16 = vp9_mse8x16_c;
    if (flags & HAS_SSE2) vp9_mse8x16 = vp9_mse8x16_sse2;
    vp9_mse8x8 = vp9_mse8x8_c;
    if (flags & HAS_SSE2) vp9_mse8x8 = vp9_mse8x8_sse2;
    vp9_sad16x16 = vp9_sad16x16_c;
    if (flags & HAS_SSE2) vp9_sad16x16 = vp9_sad16x16_sse2;
    vp9_sad16x16_avg = vp9_sad16x16_avg_c;
    if (flags & HAS_SSE2) vp9_sad16x16_avg = vp9_sad16x16_avg_sse2;
    vp9_sad16x16x3 = vp9_sad16x16x3_c;
    if (flags & HAS_SSE3) vp9_sad16x16x3 = vp9_sad16x16x3_sse3;
    if (flags & HAS_SSSE3) vp9_sad16x16x3 = vp9_sad16x16x3_ssse3;
    vp9_sad16x16x4d = vp9_sad16x16x4d_c;
    if (flags & HAS_SSE2) vp9_sad16x16x4d = vp9_sad16x16x4d_sse2;
    vp9_sad16x32 = vp9_sad16x32_c;
    if (flags & HAS_SSE2) vp9_sad16x32 = vp9_sad16x32_sse2;
    vp9_sad16x32_avg = vp9_sad16x32_avg_c;
    if (flags & HAS_SSE2) vp9_sad16x32_avg = vp9_sad16x32_avg_sse2;
    vp9_sad16x32x4d = vp9_sad16x32x4d_c;
    if (flags & HAS_SSE2) vp9_sad16x32x4d = vp9_sad16x32x4d_sse2;
    vp9_sad16x8 = vp9_sad16x8_c;
    if (flags & HAS_SSE2) vp9_sad16x8 = vp9_sad16x8_sse2;
    vp9_sad16x8_avg = vp9_sad16x8_avg_c;
    if (flags & HAS_SSE2) vp9_sad16x8_avg = vp9_sad16x8_avg_sse2;
    vp9_sad16x8x3 = vp9_sad16x8x3_c;
    if (flags & HAS_SSE3) vp9_sad16x8x3 = vp9_sad16x8x3_sse3;
    if (flags & HAS_SSSE3) vp9_sad16x8x3 = vp9_sad16x8x3_ssse3;
    vp9_sad16x8x4d = vp9_sad16x8x4d_c;
    if (flags & HAS_SSE2) vp9_sad16x8x4d = vp9_sad16x8x4d_sse2;
    vp9_sad32x16 = vp9_sad32x16_c;
    if (flags & HAS_SSE2) vp9_sad32x16 = vp9_sad32x16_sse2;
    vp9_sad32x16_avg = vp9_sad32x16_avg_c;
    if (flags & HAS_SSE2) vp9_sad32x16_avg = vp9_sad32x16_avg_sse2;
    vp9_sad32x16x4d = vp9_sad32x16x4d_c;
    if (flags & HAS_SSE2) vp9_sad32x16x4d = vp9_sad32x16x4d_sse2;
    vp9_sad32x32 = vp9_sad32x32_c;
    if (flags & HAS_SSE2) vp9_sad32x32 = vp9_sad32x32_sse2;
    vp9_sad32x32_avg = vp9_sad32x32_avg_c;
    if (flags & HAS_SSE2) vp9_sad32x32_avg = vp9_sad32x32_avg_sse2;
    vp9_sad32x32x4d = vp9_sad32x32x4d_c;
    if (flags & HAS_SSE2) vp9_sad32x32x4d = vp9_sad32x32x4d_sse2;
    vp9_sad32x64 = vp9_sad32x64_c;
    if (flags & HAS_SSE2) vp9_sad32x64 = vp9_sad32x64_sse2;
    vp9_sad32x64_avg = vp9_sad32x64_avg_c;
    if (flags & HAS_SSE2) vp9_sad32x64_avg = vp9_sad32x64_avg_sse2;
    vp9_sad32x64x4d = vp9_sad32x64x4d_c;
    if (flags & HAS_SSE2) vp9_sad32x64x4d = vp9_sad32x64x4d_sse2;
    vp9_sad4x4 = vp9_sad4x4_c;
    if (flags & HAS_SSE) vp9_sad4x4 = vp9_sad4x4_sse;
    vp9_sad4x4_avg = vp9_sad4x4_avg_c;
    if (flags & HAS_SSE) vp9_sad4x4_avg = vp9_sad4x4_avg_sse;
    vp9_sad4x4x3 = vp9_sad4x4x3_c;
    if (flags & HAS_SSE3) vp9_sad4x4x3 = vp9_sad4x4x3_sse3;
    vp9_sad4x4x4d = vp9_sad4x4x4d_c;
    if (flags & HAS_SSE) vp9_sad4x4x4d = vp9_sad4x4x4d_sse;
    vp9_sad4x8 = vp9_sad4x8_c;
    if (flags & HAS_SSE) vp9_sad4x8 = vp9_sad4x8_sse;
    vp9_sad4x8_avg = vp9_sad4x8_avg_c;
    if (flags & HAS_SSE) vp9_sad4x8_avg = vp9_sad4x8_avg_sse;
    vp9_sad4x8x4d = vp9_sad4x8x4d_c;
    if (flags & HAS_SSE) vp9_sad4x8x4d = vp9_sad4x8x4d_sse;
    vp9_sad64x32 = vp9_sad64x32_c;
    if (flags & HAS_SSE2) vp9_sad64x32 = vp9_sad64x32_sse2;
    vp9_sad64x32_avg = vp9_sad64x32_avg_c;
    if (flags & HAS_SSE2) vp9_sad64x32_avg = vp9_sad64x32_avg_sse2;
    vp9_sad64x32x4d = vp9_sad64x32x4d_c;
    if (flags & HAS_SSE2) vp9_sad64x32x4d = vp9_sad64x32x4d_sse2;
    vp9_sad64x64 = vp9_sad64x64_c;
    if (flags & HAS_SSE2) vp9_sad64x64 = vp9_sad64x64_sse2;
    vp9_sad64x64_avg = vp9_sad64x64_avg_c;
    if (flags & HAS_SSE2) vp9_sad64x64_avg = vp9_sad64x64_avg_sse2;
    vp9_sad64x64x4d = vp9_sad64x64x4d_c;
    if (flags & HAS_SSE2) vp9_sad64x64x4d = vp9_sad64x64x4d_sse2;
    vp9_sad8x16 = vp9_sad8x16_c;
    if (flags & HAS_SSE2) vp9_sad8x16 = vp9_sad8x16_sse2;
    vp9_sad8x16_avg = vp9_sad8x16_avg_c;
    if (flags & HAS_SSE2) vp9_sad8x16_avg = vp9_sad8x16_avg_sse2;
    vp9_sad8x16x3 = vp9_sad8x16x3_c;
    if (flags & HAS_SSE3) vp9_sad8x16x3 = vp9_sad8x16x3_sse3;
    vp9_sad8x16x4d = vp9_sad8x16x4d_c;
    if (flags & HAS_SSE2) vp9_sad8x16x4d = vp9_sad8x16x4d_sse2;
    vp9_sad8x4 = vp9_sad8x4_c;
    if (flags & HAS_SSE2) vp9_sad8x4 = vp9_sad8x4_sse2;
    vp9_sad8x4_avg = vp9_sad8x4_avg_c;
    if (flags & HAS_SSE2) vp9_sad8x4_avg = vp9_sad8x4_avg_sse2;
    vp9_sad8x4x4d = vp9_sad8x4x4d_c;
    if (flags & HAS_SSE2) vp9_sad8x4x4d = vp9_sad8x4x4d_sse2;
    vp9_sad8x8 = vp9_sad8x8_c;
    if (flags & HAS_SSE2) vp9_sad8x8 = vp9_sad8x8_sse2;
    vp9_sad8x8_avg = vp9_sad8x8_avg_c;
    if (flags & HAS_SSE2) vp9_sad8x8_avg = vp9_sad8x8_avg_sse2;
    vp9_sad8x8x3 = vp9_sad8x8x3_c;
    if (flags & HAS_SSE3) vp9_sad8x8x3 = vp9_sad8x8x3_sse3;
    vp9_sad8x8x4d = vp9_sad8x8x4d_c;
    if (flags & HAS_SSE2) vp9_sad8x8x4d = vp9_sad8x8x4d_sse2;
    vp9_sub_pixel_avg_variance16x16 = vp9_sub_pixel_avg_variance16x16_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance16x16 = vp9_sub_pixel_avg_variance16x16_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance16x16 = vp9_sub_pixel_avg_variance16x16_ssse3;
    vp9_sub_pixel_avg_variance16x32 = vp9_sub_pixel_avg_variance16x32_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance16x32 = vp9_sub_pixel_avg_variance16x32_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance16x32 = vp9_sub_pixel_avg_variance16x32_ssse3;
    vp9_sub_pixel_avg_variance16x8 = vp9_sub_pixel_avg_variance16x8_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance16x8 = vp9_sub_pixel_avg_variance16x8_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance16x8 = vp9_sub_pixel_avg_variance16x8_ssse3;
    vp9_sub_pixel_avg_variance32x16 = vp9_sub_pixel_avg_variance32x16_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance32x16 = vp9_sub_pixel_avg_variance32x16_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance32x16 = vp9_sub_pixel_avg_variance32x16_ssse3;
    vp9_sub_pixel_avg_variance32x32 = vp9_sub_pixel_avg_variance32x32_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance32x32 = vp9_sub_pixel_avg_variance32x32_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance32x32 = vp9_sub_pixel_avg_variance32x32_ssse3;
    vp9_sub_pixel_avg_variance32x64 = vp9_sub_pixel_avg_variance32x64_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance32x64 = vp9_sub_pixel_avg_variance32x64_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance32x64 = vp9_sub_pixel_avg_variance32x64_ssse3;
    vp9_sub_pixel_avg_variance4x4 = vp9_sub_pixel_avg_variance4x4_c;
    if (flags & HAS_SSE) vp9_sub_pixel_avg_variance4x4 = vp9_sub_pixel_avg_variance4x4_sse;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance4x4 = vp9_sub_pixel_avg_variance4x4_ssse3;
    vp9_sub_pixel_avg_variance4x8 = vp9_sub_pixel_avg_variance4x8_c;
    if (flags & HAS_SSE) vp9_sub_pixel_avg_variance4x8 = vp9_sub_pixel_avg_variance4x8_sse;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance4x8 = vp9_sub_pixel_avg_variance4x8_ssse3;
    vp9_sub_pixel_avg_variance64x32 = vp9_sub_pixel_avg_variance64x32_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance64x32 = vp9_sub_pixel_avg_variance64x32_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance64x32 = vp9_sub_pixel_avg_variance64x32_ssse3;
    vp9_sub_pixel_avg_variance64x64 = vp9_sub_pixel_avg_variance64x64_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance64x64 = vp9_sub_pixel_avg_variance64x64_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance64x64 = vp9_sub_pixel_avg_variance64x64_ssse3;
    vp9_sub_pixel_avg_variance8x16 = vp9_sub_pixel_avg_variance8x16_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance8x16 = vp9_sub_pixel_avg_variance8x16_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance8x16 = vp9_sub_pixel_avg_variance8x16_ssse3;
    vp9_sub_pixel_avg_variance8x4 = vp9_sub_pixel_avg_variance8x4_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance8x4 = vp9_sub_pixel_avg_variance8x4_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance8x4 = vp9_sub_pixel_avg_variance8x4_ssse3;
    vp9_sub_pixel_avg_variance8x8 = vp9_sub_pixel_avg_variance8x8_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_avg_variance8x8 = vp9_sub_pixel_avg_variance8x8_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_avg_variance8x8 = vp9_sub_pixel_avg_variance8x8_ssse3;
    vp9_sub_pixel_variance16x16 = vp9_sub_pixel_variance16x16_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance16x16 = vp9_sub_pixel_variance16x16_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance16x16 = vp9_sub_pixel_variance16x16_ssse3;
    vp9_sub_pixel_variance16x32 = vp9_sub_pixel_variance16x32_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance16x32 = vp9_sub_pixel_variance16x32_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance16x32 = vp9_sub_pixel_variance16x32_ssse3;
    vp9_sub_pixel_variance16x8 = vp9_sub_pixel_variance16x8_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance16x8 = vp9_sub_pixel_variance16x8_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance16x8 = vp9_sub_pixel_variance16x8_ssse3;
    vp9_sub_pixel_variance32x16 = vp9_sub_pixel_variance32x16_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance32x16 = vp9_sub_pixel_variance32x16_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance32x16 = vp9_sub_pixel_variance32x16_ssse3;
    vp9_sub_pixel_variance32x32 = vp9_sub_pixel_variance32x32_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance32x32 = vp9_sub_pixel_variance32x32_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance32x32 = vp9_sub_pixel_variance32x32_ssse3;
    vp9_sub_pixel_variance32x64 = vp9_sub_pixel_variance32x64_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance32x64 = vp9_sub_pixel_variance32x64_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance32x64 = vp9_sub_pixel_variance32x64_ssse3;
    vp9_sub_pixel_variance4x4 = vp9_sub_pixel_variance4x4_c;
    if (flags & HAS_SSE) vp9_sub_pixel_variance4x4 = vp9_sub_pixel_variance4x4_sse;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance4x4 = vp9_sub_pixel_variance4x4_ssse3;
    vp9_sub_pixel_variance4x8 = vp9_sub_pixel_variance4x8_c;
    if (flags & HAS_SSE) vp9_sub_pixel_variance4x8 = vp9_sub_pixel_variance4x8_sse;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance4x8 = vp9_sub_pixel_variance4x8_ssse3;
    vp9_sub_pixel_variance64x32 = vp9_sub_pixel_variance64x32_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance64x32 = vp9_sub_pixel_variance64x32_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance64x32 = vp9_sub_pixel_variance64x32_ssse3;
    vp9_sub_pixel_variance64x64 = vp9_sub_pixel_variance64x64_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance64x64 = vp9_sub_pixel_variance64x64_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance64x64 = vp9_sub_pixel_variance64x64_ssse3;
    vp9_sub_pixel_variance8x16 = vp9_sub_pixel_variance8x16_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance8x16 = vp9_sub_pixel_variance8x16_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance8x16 = vp9_sub_pixel_variance8x16_ssse3;
    vp9_sub_pixel_variance8x4 = vp9_sub_pixel_variance8x4_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance8x4 = vp9_sub_pixel_variance8x4_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance8x4 = vp9_sub_pixel_variance8x4_ssse3;
    vp9_sub_pixel_variance8x8 = vp9_sub_pixel_variance8x8_c;
    if (flags & HAS_SSE2) vp9_sub_pixel_variance8x8 = vp9_sub_pixel_variance8x8_sse2;
    if (flags & HAS_SSSE3) vp9_sub_pixel_variance8x8 = vp9_sub_pixel_variance8x8_ssse3;
    vp9_subtract_block = vp9_subtract_block_c;
    if (flags & HAS_SSE2) vp9_subtract_block = vp9_subtract_block_sse2;
    vp9_temporal_filter_apply = vp9_temporal_filter_apply_c;
    if (flags & HAS_SSE2) vp9_temporal_filter_apply = vp9_temporal_filter_apply_sse2;
    vp9_tm_predictor_16x16 = vp9_tm_predictor_16x16_c;
    if (flags & HAS_SSE2) vp9_tm_predictor_16x16 = vp9_tm_predictor_16x16_sse2;
    vp9_tm_predictor_4x4 = vp9_tm_predictor_4x4_c;
    if (flags & HAS_SSE) vp9_tm_predictor_4x4 = vp9_tm_predictor_4x4_sse;
    vp9_tm_predictor_8x8 = vp9_tm_predictor_8x8_c;
    if (flags & HAS_SSE2) vp9_tm_predictor_8x8 = vp9_tm_predictor_8x8_sse2;
    vp9_v_predictor_16x16 = vp9_v_predictor_16x16_c;
    if (flags & HAS_SSE2) vp9_v_predictor_16x16 = vp9_v_predictor_16x16_sse2;
    vp9_v_predictor_32x32 = vp9_v_predictor_32x32_c;
    if (flags & HAS_SSE2) vp9_v_predictor_32x32 = vp9_v_predictor_32x32_sse2;
    vp9_v_predictor_4x4 = vp9_v_predictor_4x4_c;
    if (flags & HAS_SSE) vp9_v_predictor_4x4 = vp9_v_predictor_4x4_sse;
    vp9_v_predictor_8x8 = vp9_v_predictor_8x8_c;
    if (flags & HAS_SSE) vp9_v_predictor_8x8 = vp9_v_predictor_8x8_sse;
    vp9_variance16x16 = vp9_variance16x16_c;
    if (flags & HAS_SSE2) vp9_variance16x16 = vp9_variance16x16_sse2;
    vp9_variance16x32 = vp9_variance16x32_c;
    if (flags & HAS_SSE2) vp9_variance16x32 = vp9_variance16x32_sse2;
    vp9_variance16x8 = vp9_variance16x8_c;
    if (flags & HAS_SSE2) vp9_variance16x8 = vp9_variance16x8_sse2;
    vp9_variance32x16 = vp9_variance32x16_c;
    if (flags & HAS_SSE2) vp9_variance32x16 = vp9_variance32x16_sse2;
    vp9_variance32x32 = vp9_variance32x32_c;
    if (flags & HAS_SSE2) vp9_variance32x32 = vp9_variance32x32_sse2;
    vp9_variance32x64 = vp9_variance32x64_c;
    if (flags & HAS_SSE2) vp9_variance32x64 = vp9_variance32x64_sse2;
    vp9_variance4x4 = vp9_variance4x4_c;
    if (flags & HAS_SSE2) vp9_variance4x4 = vp9_variance4x4_sse2;
    vp9_variance4x8 = vp9_variance4x8_c;
    if (flags & HAS_SSE2) vp9_variance4x8 = vp9_variance4x8_sse2;
    vp9_variance64x32 = vp9_variance64x32_c;
    if (flags & HAS_SSE2) vp9_variance64x32 = vp9_variance64x32_sse2;
    vp9_variance64x64 = vp9_variance64x64_c;
    if (flags & HAS_SSE2) vp9_variance64x64 = vp9_variance64x64_sse2;
    vp9_variance8x16 = vp9_variance8x16_c;
    if (flags & HAS_SSE2) vp9_variance8x16 = vp9_variance8x16_sse2;
    vp9_variance8x4 = vp9_variance8x4_c;
    if (flags & HAS_SSE2) vp9_variance8x4 = vp9_variance8x4_sse2;
    vp9_variance8x8 = vp9_variance8x8_c;
    if (flags & HAS_SSE2) vp9_variance8x8 = vp9_variance8x8_sse2;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
