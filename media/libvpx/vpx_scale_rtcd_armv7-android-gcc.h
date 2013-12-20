#ifndef VPX_SCALE_RTCD_H_
#define VPX_SCALE_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

struct yv12_buffer_config;

void vp8_horizontal_line_5_4_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
#define vp8_horizontal_line_5_4_scale vp8_horizontal_line_5_4_scale_c

void vp8_vertical_band_5_4_scale_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
#define vp8_vertical_band_5_4_scale vp8_vertical_band_5_4_scale_c

void vp8_horizontal_line_5_3_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
#define vp8_horizontal_line_5_3_scale vp8_horizontal_line_5_3_scale_c

void vp8_vertical_band_5_3_scale_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
#define vp8_vertical_band_5_3_scale vp8_vertical_band_5_3_scale_c

void vp8_horizontal_line_2_1_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
#define vp8_horizontal_line_2_1_scale vp8_horizontal_line_2_1_scale_c

void vp8_vertical_band_2_1_scale_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
#define vp8_vertical_band_2_1_scale vp8_vertical_band_2_1_scale_c

void vp8_vertical_band_2_1_scale_i_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
#define vp8_vertical_band_2_1_scale_i vp8_vertical_band_2_1_scale_i_c

void vp8_yv12_extend_frame_borders_c(struct yv12_buffer_config *ybf);
void vp8_yv12_extend_frame_borders_neon(struct yv12_buffer_config *ybf);
RTCD_EXTERN void (*vp8_yv12_extend_frame_borders)(struct yv12_buffer_config *ybf);

void vp8_yv12_copy_frame_c(const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc);
void vp8_yv12_copy_frame_neon(const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc);
RTCD_EXTERN void (*vp8_yv12_copy_frame)(const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc);

void vpx_yv12_copy_y_c(const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc);
void vpx_yv12_copy_y_neon(const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc);
RTCD_EXTERN void (*vpx_yv12_copy_y)(const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc);

void vp9_extend_frame_borders_c(struct yv12_buffer_config *ybf, int subsampling_x, int subsampling_y);
#define vp9_extend_frame_borders vp9_extend_frame_borders_c

void vp9_extend_frame_inner_borders_c(struct yv12_buffer_config *ybf, int subsampling_x, int subsampling_y);
#define vp9_extend_frame_inner_borders vp9_extend_frame_inner_borders_c

void vpx_scale_rtcd(void);
#include "vpx_config.h"

#ifdef RTCD_C
#include "vpx_ports/arm.h"
static void setup_rtcd_internal(void)
{
    int flags = arm_cpu_caps();

    (void)flags;








    vp8_yv12_extend_frame_borders = vp8_yv12_extend_frame_borders_c;
    if (flags & HAS_NEON) vp8_yv12_extend_frame_borders = vp8_yv12_extend_frame_borders_neon;

    vp8_yv12_copy_frame = vp8_yv12_copy_frame_c;
    if (flags & HAS_NEON) vp8_yv12_copy_frame = vp8_yv12_copy_frame_neon;

    vpx_yv12_copy_y = vpx_yv12_copy_y_c;
    if (flags & HAS_NEON) vpx_yv12_copy_y = vpx_yv12_copy_y_neon;
}
#endif
#endif
