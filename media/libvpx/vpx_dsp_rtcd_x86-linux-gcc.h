#ifndef VPX_DSP_RTCD_H_
#define VPX_DSP_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * DSP
 */

#include "vpx/vpx_integer.h"


#ifdef __cplusplus
extern "C" {
#endif

void vpx_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride);
#define vpx_comp_avg_pred vpx_comp_avg_pred_c

void vpx_get16x16var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get16x16var_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get16x16var_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
RTCD_EXTERN void (*vpx_get16x16var)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);

unsigned int vpx_get4x4sse_cs_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride);
#define vpx_get4x4sse_cs vpx_get4x4sse_cs_c

void vpx_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get8x8var_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
void vpx_get8x8var_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
RTCD_EXTERN void (*vpx_get8x8var)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);

unsigned int vpx_get_mb_ss_c(const int16_t *);
unsigned int vpx_get_mb_ss_mmx(const int16_t *);
unsigned int vpx_get_mb_ss_sse2(const int16_t *);
RTCD_EXTERN unsigned int (*vpx_get_mb_ss)(const int16_t *);

unsigned int vpx_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x16_mmx(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x16_avx2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse16x16)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse16x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse16x8)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse8x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse8x16)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int vpx_mse8x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_mse8x8)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int vpx_sad16x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad16x16_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad16x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad16x16_avg vpx_sad16x16_avg_c

void vpx_sad16x16x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x16x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x16x3_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x16x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad16x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad16x16x4d vpx_sad16x16x4d_c

void vpx_sad16x16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x16x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x16x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad16x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad16x32 vpx_sad16x32_c

unsigned int vpx_sad16x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad16x32_avg vpx_sad16x32_avg_c

void vpx_sad16x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad16x32x4d vpx_sad16x32x4d_c

unsigned int vpx_sad16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad16x8_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad16x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad16x8_avg vpx_sad16x8_avg_c

void vpx_sad16x8x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x8x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x8x3_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x8x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad16x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad16x8x4d vpx_sad16x8x4d_c

void vpx_sad16x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad16x8x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad16x8x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad32x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x16_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad32x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x16_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad32x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad32x16x4d vpx_sad32x16x4d_c

unsigned int vpx_sad32x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad32x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad32x32x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad32x32x3 vpx_sad32x32x3_c

void vpx_sad32x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad32x32x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad32x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad32x32x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad32x32x8 vpx_sad32x32x8_c

unsigned int vpx_sad32x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad32x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad32x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad32x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad32x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad32x64x4d vpx_sad32x64x4d_c

unsigned int vpx_sad4x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad4x4_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad4x4)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad4x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad4x4_avg vpx_sad4x4_avg_c

void vpx_sad4x4x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad4x4x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad4x4x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad4x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad4x4x4d vpx_sad4x4x4d_c

void vpx_sad4x4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad4x4x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad4x4x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad4x8 vpx_sad4x8_c

unsigned int vpx_sad4x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad4x8_avg vpx_sad4x8_avg_c

void vpx_sad4x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad4x8x4d vpx_sad4x8x4d_c

void vpx_sad4x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad4x8x8 vpx_sad4x8x8_c

unsigned int vpx_sad64x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad64x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad64x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad64x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad64x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad64x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad64x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad64x32x4d vpx_sad64x32x4d_c

unsigned int vpx_sad64x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad64x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad64x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad64x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int vpx_sad64x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*vpx_sad64x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void vpx_sad64x64x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad64x64x3 vpx_sad64x64x3_c

void vpx_sad64x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void vpx_sad64x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad64x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

void vpx_sad64x64x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad64x64x8 vpx_sad64x64x8_c

unsigned int vpx_sad8x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad8x16_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad8x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad8x16_avg vpx_sad8x16_avg_c

void vpx_sad8x16x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x16x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x16x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad8x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad8x16x4d vpx_sad8x16x4d_c

void vpx_sad8x16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x16x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x16x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_sad8x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad8x4 vpx_sad8x4_c

unsigned int vpx_sad8x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad8x4_avg vpx_sad8x4_avg_c

void vpx_sad8x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad8x4x4d vpx_sad8x4x4d_c

void vpx_sad8x4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad8x4x8 vpx_sad8x4x8_c

unsigned int vpx_sad8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int vpx_sad8x8_mmx(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int vpx_sad8x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad8x8_avg vpx_sad8x8_avg_c

void vpx_sad8x8x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x8x3_sse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x8x3)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

void vpx_sad8x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad8x8x4d vpx_sad8x8x4d_c

void vpx_sad8x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
void vpx_sad8x8x8_sse4_1(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*vpx_sad8x8x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);

unsigned int vpx_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance16x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance16x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance16x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance32x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance32x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance32x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance4x4_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance4x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance4x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance4x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance4x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance64x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance64x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x16_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance8x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance8x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int vpx_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x8_mmx(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int vpx_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vpx_variance8x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

void vpx_dsp_rtcd(void);

#ifdef RTCD_C
#include "vpx_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;

    vpx_get16x16var = vpx_get16x16var_c;
    if (flags & HAS_SSE2) vpx_get16x16var = vpx_get16x16var_sse2;
    if (flags & HAS_AVX2) vpx_get16x16var = vpx_get16x16var_avx2;
    vpx_get8x8var = vpx_get8x8var_c;
    if (flags & HAS_MMX) vpx_get8x8var = vpx_get8x8var_mmx;
    if (flags & HAS_SSE2) vpx_get8x8var = vpx_get8x8var_sse2;
    vpx_get_mb_ss = vpx_get_mb_ss_c;
    if (flags & HAS_MMX) vpx_get_mb_ss = vpx_get_mb_ss_mmx;
    if (flags & HAS_SSE2) vpx_get_mb_ss = vpx_get_mb_ss_sse2;
    vpx_mse16x16 = vpx_mse16x16_c;
    if (flags & HAS_MMX) vpx_mse16x16 = vpx_mse16x16_mmx;
    if (flags & HAS_SSE2) vpx_mse16x16 = vpx_mse16x16_sse2;
    if (flags & HAS_AVX2) vpx_mse16x16 = vpx_mse16x16_avx2;
    vpx_mse16x8 = vpx_mse16x8_c;
    if (flags & HAS_SSE2) vpx_mse16x8 = vpx_mse16x8_sse2;
    vpx_mse8x16 = vpx_mse8x16_c;
    if (flags & HAS_SSE2) vpx_mse8x16 = vpx_mse8x16_sse2;
    vpx_mse8x8 = vpx_mse8x8_c;
    if (flags & HAS_SSE2) vpx_mse8x8 = vpx_mse8x8_sse2;
    vpx_sad16x16 = vpx_sad16x16_c;
    if (flags & HAS_MMX) vpx_sad16x16 = vpx_sad16x16_mmx;
    vpx_sad16x16x3 = vpx_sad16x16x3_c;
    if (flags & HAS_SSE3) vpx_sad16x16x3 = vpx_sad16x16x3_sse3;
    if (flags & HAS_SSSE3) vpx_sad16x16x3 = vpx_sad16x16x3_ssse3;
    vpx_sad16x16x8 = vpx_sad16x16x8_c;
    if (flags & HAS_SSE4_1) vpx_sad16x16x8 = vpx_sad16x16x8_sse4_1;
    vpx_sad16x8 = vpx_sad16x8_c;
    if (flags & HAS_MMX) vpx_sad16x8 = vpx_sad16x8_mmx;
    vpx_sad16x8x3 = vpx_sad16x8x3_c;
    if (flags & HAS_SSE3) vpx_sad16x8x3 = vpx_sad16x8x3_sse3;
    if (flags & HAS_SSSE3) vpx_sad16x8x3 = vpx_sad16x8x3_ssse3;
    vpx_sad16x8x8 = vpx_sad16x8x8_c;
    if (flags & HAS_SSE4_1) vpx_sad16x8x8 = vpx_sad16x8x8_sse4_1;
    vpx_sad32x16 = vpx_sad32x16_c;
    if (flags & HAS_AVX2) vpx_sad32x16 = vpx_sad32x16_avx2;
    vpx_sad32x16_avg = vpx_sad32x16_avg_c;
    if (flags & HAS_AVX2) vpx_sad32x16_avg = vpx_sad32x16_avg_avx2;
    vpx_sad32x32 = vpx_sad32x32_c;
    if (flags & HAS_AVX2) vpx_sad32x32 = vpx_sad32x32_avx2;
    vpx_sad32x32_avg = vpx_sad32x32_avg_c;
    if (flags & HAS_AVX2) vpx_sad32x32_avg = vpx_sad32x32_avg_avx2;
    vpx_sad32x32x4d = vpx_sad32x32x4d_c;
    if (flags & HAS_AVX2) vpx_sad32x32x4d = vpx_sad32x32x4d_avx2;
    vpx_sad32x64 = vpx_sad32x64_c;
    if (flags & HAS_AVX2) vpx_sad32x64 = vpx_sad32x64_avx2;
    vpx_sad32x64_avg = vpx_sad32x64_avg_c;
    if (flags & HAS_AVX2) vpx_sad32x64_avg = vpx_sad32x64_avg_avx2;
    vpx_sad4x4 = vpx_sad4x4_c;
    if (flags & HAS_MMX) vpx_sad4x4 = vpx_sad4x4_mmx;
    vpx_sad4x4x3 = vpx_sad4x4x3_c;
    if (flags & HAS_SSE3) vpx_sad4x4x3 = vpx_sad4x4x3_sse3;
    vpx_sad4x4x8 = vpx_sad4x4x8_c;
    if (flags & HAS_SSE4_1) vpx_sad4x4x8 = vpx_sad4x4x8_sse4_1;
    vpx_sad64x32 = vpx_sad64x32_c;
    if (flags & HAS_AVX2) vpx_sad64x32 = vpx_sad64x32_avx2;
    vpx_sad64x32_avg = vpx_sad64x32_avg_c;
    if (flags & HAS_AVX2) vpx_sad64x32_avg = vpx_sad64x32_avg_avx2;
    vpx_sad64x64 = vpx_sad64x64_c;
    if (flags & HAS_AVX2) vpx_sad64x64 = vpx_sad64x64_avx2;
    vpx_sad64x64_avg = vpx_sad64x64_avg_c;
    if (flags & HAS_AVX2) vpx_sad64x64_avg = vpx_sad64x64_avg_avx2;
    vpx_sad64x64x4d = vpx_sad64x64x4d_c;
    if (flags & HAS_AVX2) vpx_sad64x64x4d = vpx_sad64x64x4d_avx2;
    vpx_sad8x16 = vpx_sad8x16_c;
    if (flags & HAS_MMX) vpx_sad8x16 = vpx_sad8x16_mmx;
    vpx_sad8x16x3 = vpx_sad8x16x3_c;
    if (flags & HAS_SSE3) vpx_sad8x16x3 = vpx_sad8x16x3_sse3;
    vpx_sad8x16x8 = vpx_sad8x16x8_c;
    if (flags & HAS_SSE4_1) vpx_sad8x16x8 = vpx_sad8x16x8_sse4_1;
    vpx_sad8x8 = vpx_sad8x8_c;
    if (flags & HAS_MMX) vpx_sad8x8 = vpx_sad8x8_mmx;
    vpx_sad8x8x3 = vpx_sad8x8x3_c;
    if (flags & HAS_SSE3) vpx_sad8x8x3 = vpx_sad8x8x3_sse3;
    vpx_sad8x8x8 = vpx_sad8x8x8_c;
    if (flags & HAS_SSE4_1) vpx_sad8x8x8 = vpx_sad8x8x8_sse4_1;
    vpx_variance16x16 = vpx_variance16x16_c;
    if (flags & HAS_MMX) vpx_variance16x16 = vpx_variance16x16_mmx;
    if (flags & HAS_SSE2) vpx_variance16x16 = vpx_variance16x16_sse2;
    if (flags & HAS_AVX2) vpx_variance16x16 = vpx_variance16x16_avx2;
    vpx_variance16x32 = vpx_variance16x32_c;
    if (flags & HAS_SSE2) vpx_variance16x32 = vpx_variance16x32_sse2;
    vpx_variance16x8 = vpx_variance16x8_c;
    if (flags & HAS_MMX) vpx_variance16x8 = vpx_variance16x8_mmx;
    if (flags & HAS_SSE2) vpx_variance16x8 = vpx_variance16x8_sse2;
    vpx_variance32x16 = vpx_variance32x16_c;
    if (flags & HAS_SSE2) vpx_variance32x16 = vpx_variance32x16_sse2;
    if (flags & HAS_AVX2) vpx_variance32x16 = vpx_variance32x16_avx2;
    vpx_variance32x32 = vpx_variance32x32_c;
    if (flags & HAS_SSE2) vpx_variance32x32 = vpx_variance32x32_sse2;
    if (flags & HAS_AVX2) vpx_variance32x32 = vpx_variance32x32_avx2;
    vpx_variance32x64 = vpx_variance32x64_c;
    if (flags & HAS_SSE2) vpx_variance32x64 = vpx_variance32x64_sse2;
    vpx_variance4x4 = vpx_variance4x4_c;
    if (flags & HAS_MMX) vpx_variance4x4 = vpx_variance4x4_mmx;
    if (flags & HAS_SSE2) vpx_variance4x4 = vpx_variance4x4_sse2;
    vpx_variance4x8 = vpx_variance4x8_c;
    if (flags & HAS_SSE2) vpx_variance4x8 = vpx_variance4x8_sse2;
    vpx_variance64x32 = vpx_variance64x32_c;
    if (flags & HAS_SSE2) vpx_variance64x32 = vpx_variance64x32_sse2;
    if (flags & HAS_AVX2) vpx_variance64x32 = vpx_variance64x32_avx2;
    vpx_variance64x64 = vpx_variance64x64_c;
    if (flags & HAS_SSE2) vpx_variance64x64 = vpx_variance64x64_sse2;
    if (flags & HAS_AVX2) vpx_variance64x64 = vpx_variance64x64_avx2;
    vpx_variance8x16 = vpx_variance8x16_c;
    if (flags & HAS_MMX) vpx_variance8x16 = vpx_variance8x16_mmx;
    if (flags & HAS_SSE2) vpx_variance8x16 = vpx_variance8x16_sse2;
    vpx_variance8x4 = vpx_variance8x4_c;
    if (flags & HAS_SSE2) vpx_variance8x4 = vpx_variance8x4_sse2;
    vpx_variance8x8 = vpx_variance8x8_c;
    if (flags & HAS_MMX) vpx_variance8x8 = vpx_variance8x8_mmx;
    if (flags & HAS_SSE2) vpx_variance8x8 = vpx_variance8x8_sse2;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
