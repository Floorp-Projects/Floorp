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
#define vpx_get16x16var vpx_get16x16var_c

unsigned int vpx_get4x4sse_cs_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride);
#define vpx_get4x4sse_cs vpx_get4x4sse_cs_c

void vpx_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define vpx_get8x8var vpx_get8x8var_c

unsigned int vpx_get_mb_ss_c(const int16_t *);
#define vpx_get_mb_ss vpx_get_mb_ss_c

unsigned int vpx_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define vpx_mse16x16 vpx_mse16x16_c

unsigned int vpx_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define vpx_mse16x8 vpx_mse16x8_c

unsigned int vpx_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define vpx_mse8x16 vpx_mse8x16_c

unsigned int vpx_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define vpx_mse8x8 vpx_mse8x8_c

unsigned int vpx_sad16x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad16x16 vpx_sad16x16_c

unsigned int vpx_sad16x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad16x16_avg vpx_sad16x16_avg_c

void vpx_sad16x16x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad16x16x3 vpx_sad16x16x3_c

void vpx_sad16x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad16x16x4d vpx_sad16x16x4d_c

void vpx_sad16x16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad16x16x8 vpx_sad16x16x8_c

unsigned int vpx_sad16x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad16x32 vpx_sad16x32_c

unsigned int vpx_sad16x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad16x32_avg vpx_sad16x32_avg_c

void vpx_sad16x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad16x32x4d vpx_sad16x32x4d_c

unsigned int vpx_sad16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad16x8 vpx_sad16x8_c

unsigned int vpx_sad16x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad16x8_avg vpx_sad16x8_avg_c

void vpx_sad16x8x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad16x8x3 vpx_sad16x8x3_c

void vpx_sad16x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad16x8x4d vpx_sad16x8x4d_c

void vpx_sad16x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad16x8x8 vpx_sad16x8x8_c

unsigned int vpx_sad32x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad32x16 vpx_sad32x16_c

unsigned int vpx_sad32x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad32x16_avg vpx_sad32x16_avg_c

void vpx_sad32x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad32x16x4d vpx_sad32x16x4d_c

unsigned int vpx_sad32x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad32x32 vpx_sad32x32_c

unsigned int vpx_sad32x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad32x32_avg vpx_sad32x32_avg_c

void vpx_sad32x32x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad32x32x3 vpx_sad32x32x3_c

void vpx_sad32x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad32x32x4d vpx_sad32x32x4d_c

void vpx_sad32x32x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad32x32x8 vpx_sad32x32x8_c

unsigned int vpx_sad32x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad32x64 vpx_sad32x64_c

unsigned int vpx_sad32x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad32x64_avg vpx_sad32x64_avg_c

void vpx_sad32x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad32x64x4d vpx_sad32x64x4d_c

unsigned int vpx_sad4x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad4x4 vpx_sad4x4_c

unsigned int vpx_sad4x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad4x4_avg vpx_sad4x4_avg_c

void vpx_sad4x4x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad4x4x3 vpx_sad4x4x3_c

void vpx_sad4x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad4x4x4d vpx_sad4x4x4d_c

void vpx_sad4x4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad4x4x8 vpx_sad4x4x8_c

unsigned int vpx_sad4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad4x8 vpx_sad4x8_c

unsigned int vpx_sad4x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad4x8_avg vpx_sad4x8_avg_c

void vpx_sad4x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad4x8x4d vpx_sad4x8x4d_c

void vpx_sad4x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad4x8x8 vpx_sad4x8x8_c

unsigned int vpx_sad64x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad64x32 vpx_sad64x32_c

unsigned int vpx_sad64x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad64x32_avg vpx_sad64x32_avg_c

void vpx_sad64x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad64x32x4d vpx_sad64x32x4d_c

unsigned int vpx_sad64x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad64x64 vpx_sad64x64_c

unsigned int vpx_sad64x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad64x64_avg vpx_sad64x64_avg_c

void vpx_sad64x64x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad64x64x3 vpx_sad64x64x3_c

void vpx_sad64x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad64x64x4d vpx_sad64x64x4d_c

void vpx_sad64x64x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad64x64x8 vpx_sad64x64x8_c

unsigned int vpx_sad8x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad8x16 vpx_sad8x16_c

unsigned int vpx_sad8x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad8x16_avg vpx_sad8x16_avg_c

void vpx_sad8x16x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad8x16x3 vpx_sad8x16x3_c

void vpx_sad8x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad8x16x4d vpx_sad8x16x4d_c

void vpx_sad8x16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad8x16x8 vpx_sad8x16x8_c

unsigned int vpx_sad8x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad8x4 vpx_sad8x4_c

unsigned int vpx_sad8x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad8x4_avg vpx_sad8x4_avg_c

void vpx_sad8x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad8x4x4d vpx_sad8x4x4d_c

void vpx_sad8x4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad8x4x8 vpx_sad8x4x8_c

unsigned int vpx_sad8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define vpx_sad8x8 vpx_sad8x8_c

unsigned int vpx_sad8x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define vpx_sad8x8_avg vpx_sad8x8_avg_c

void vpx_sad8x8x3_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad8x8x3 vpx_sad8x8x3_c

void vpx_sad8x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define vpx_sad8x8x4d vpx_sad8x8x4d_c

void vpx_sad8x8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array);
#define vpx_sad8x8x8 vpx_sad8x8x8_c

unsigned int vpx_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance16x16 vpx_variance16x16_c

unsigned int vpx_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance16x32 vpx_variance16x32_c

unsigned int vpx_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance16x8 vpx_variance16x8_c

unsigned int vpx_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance32x16 vpx_variance32x16_c

unsigned int vpx_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance32x32 vpx_variance32x32_c

unsigned int vpx_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance32x64 vpx_variance32x64_c

unsigned int vpx_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance4x4 vpx_variance4x4_c

unsigned int vpx_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance4x8 vpx_variance4x8_c

unsigned int vpx_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance64x32 vpx_variance64x32_c

unsigned int vpx_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance64x64 vpx_variance64x64_c

unsigned int vpx_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance8x16 vpx_variance8x16_c

unsigned int vpx_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance8x4 vpx_variance8x4_c

unsigned int vpx_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define vpx_variance8x8 vpx_variance8x8_c

void vpx_dsp_rtcd(void);

#include "vpx_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void)
{
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
