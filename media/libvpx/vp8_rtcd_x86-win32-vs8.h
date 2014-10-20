#ifndef VP8_RTCD_H_
#define VP8_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * VP8
 */

struct blockd;
struct macroblockd;
struct loop_filter_info;

/* Encoder forward decls */
struct block;
struct macroblock;
struct variance_vtable;
union int_mv;
struct yv12_buffer_config;

void vp8_bilinear_predict16x16_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict16x16_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict16x16_sse2(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict16x16_ssse3(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_bilinear_predict16x16)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

void vp8_bilinear_predict4x4_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict4x4_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_bilinear_predict4x4)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

void vp8_bilinear_predict8x4_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict8x4_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_bilinear_predict8x4)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

void vp8_bilinear_predict8x8_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict8x8_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict8x8_sse2(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_bilinear_predict8x8_ssse3(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_bilinear_predict8x8)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

void vp8_blend_b_c(unsigned char *y, unsigned char *u, unsigned char *v, int y1, int u1, int v1, int alpha, int stride);
#define vp8_blend_b vp8_blend_b_c

void vp8_blend_mb_inner_c(unsigned char *y, unsigned char *u, unsigned char *v, int y1, int u1, int v1, int alpha, int stride);
#define vp8_blend_mb_inner vp8_blend_mb_inner_c

void vp8_blend_mb_outer_c(unsigned char *y, unsigned char *u, unsigned char *v, int y1, int u1, int v1, int alpha, int stride);
#define vp8_blend_mb_outer vp8_blend_mb_outer_c

int vp8_block_error_c(short *coeff, short *dqcoeff);
int vp8_block_error_mmx(short *coeff, short *dqcoeff);
int vp8_block_error_xmm(short *coeff, short *dqcoeff);
RTCD_EXTERN int (*vp8_block_error)(short *coeff, short *dqcoeff);

void vp8_build_intra_predictors_mbuv_s_c(struct macroblockd *x, unsigned char * uabove_row, unsigned char * vabove_row,  unsigned char *uleft, unsigned char *vleft, int left_stride, unsigned char * upred_ptr, unsigned char * vpred_ptr, int pred_stride);
void vp8_build_intra_predictors_mbuv_s_sse2(struct macroblockd *x, unsigned char * uabove_row, unsigned char * vabove_row,  unsigned char *uleft, unsigned char *vleft, int left_stride, unsigned char * upred_ptr, unsigned char * vpred_ptr, int pred_stride);
void vp8_build_intra_predictors_mbuv_s_ssse3(struct macroblockd *x, unsigned char * uabove_row, unsigned char * vabove_row,  unsigned char *uleft, unsigned char *vleft, int left_stride, unsigned char * upred_ptr, unsigned char * vpred_ptr, int pred_stride);
RTCD_EXTERN void (*vp8_build_intra_predictors_mbuv_s)(struct macroblockd *x, unsigned char * uabove_row, unsigned char * vabove_row,  unsigned char *uleft, unsigned char *vleft, int left_stride, unsigned char * upred_ptr, unsigned char * vpred_ptr, int pred_stride);

void vp8_build_intra_predictors_mby_s_c(struct macroblockd *x, unsigned char * yabove_row, unsigned char * yleft, int left_stride, unsigned char * ypred_ptr, int y_stride);
void vp8_build_intra_predictors_mby_s_sse2(struct macroblockd *x, unsigned char * yabove_row, unsigned char * yleft, int left_stride, unsigned char * ypred_ptr, int y_stride);
void vp8_build_intra_predictors_mby_s_ssse3(struct macroblockd *x, unsigned char * yabove_row, unsigned char * yleft, int left_stride, unsigned char * ypred_ptr, int y_stride);
RTCD_EXTERN void (*vp8_build_intra_predictors_mby_s)(struct macroblockd *x, unsigned char * yabove_row, unsigned char * yleft, int left_stride, unsigned char * ypred_ptr, int y_stride);

void vp8_clear_system_state_c();
void vpx_reset_mmx_state();
RTCD_EXTERN void (*vp8_clear_system_state)();

void vp8_copy32xn_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int ref_stride, int n);
void vp8_copy32xn_sse2(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int ref_stride, int n);
void vp8_copy32xn_sse3(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int ref_stride, int n);
RTCD_EXTERN void (*vp8_copy32xn)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int ref_stride, int n);

void vp8_copy_mem16x16_c(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);
void vp8_copy_mem16x16_mmx(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);
void vp8_copy_mem16x16_sse2(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_copy_mem16x16)(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);

void vp8_copy_mem8x4_c(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);
void vp8_copy_mem8x4_mmx(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_copy_mem8x4)(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);

void vp8_copy_mem8x8_c(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);
void vp8_copy_mem8x8_mmx(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_copy_mem8x8)(unsigned char *src, int src_pitch, unsigned char *dst, int dst_pitch);

void vp8_dc_only_idct_add_c(short input, unsigned char *pred, int pred_stride, unsigned char *dst, int dst_stride);
void vp8_dc_only_idct_add_mmx(short input, unsigned char *pred, int pred_stride, unsigned char *dst, int dst_stride);
RTCD_EXTERN void (*vp8_dc_only_idct_add)(short input, unsigned char *pred, int pred_stride, unsigned char *dst, int dst_stride);

int vp8_denoiser_filter_c(unsigned char *mc_running_avg_y, int mc_avg_y_stride, unsigned char *running_avg_y, int avg_y_stride, unsigned char *sig, int sig_stride, unsigned int motion_magnitude, int increase_denoising);
int vp8_denoiser_filter_sse2(unsigned char *mc_running_avg_y, int mc_avg_y_stride, unsigned char *running_avg_y, int avg_y_stride, unsigned char *sig, int sig_stride, unsigned int motion_magnitude, int increase_denoising);
RTCD_EXTERN int (*vp8_denoiser_filter)(unsigned char *mc_running_avg_y, int mc_avg_y_stride, unsigned char *running_avg_y, int avg_y_stride, unsigned char *sig, int sig_stride, unsigned int motion_magnitude, int increase_denoising);

int vp8_denoiser_filter_uv_c(unsigned char *mc_running_avg, int mc_avg_stride, unsigned char *running_avg, int avg_stride, unsigned char *sig, int sig_stride, unsigned int motion_magnitude, int increase_denoising);
int vp8_denoiser_filter_uv_sse2(unsigned char *mc_running_avg, int mc_avg_stride, unsigned char *running_avg, int avg_stride, unsigned char *sig, int sig_stride, unsigned int motion_magnitude, int increase_denoising);
RTCD_EXTERN int (*vp8_denoiser_filter_uv)(unsigned char *mc_running_avg, int mc_avg_stride, unsigned char *running_avg, int avg_stride, unsigned char *sig, int sig_stride, unsigned int motion_magnitude, int increase_denoising);

void vp8_dequant_idct_add_c(short *input, short *dq, unsigned char *output, int stride);
void vp8_dequant_idct_add_mmx(short *input, short *dq, unsigned char *output, int stride);
RTCD_EXTERN void (*vp8_dequant_idct_add)(short *input, short *dq, unsigned char *output, int stride);

void vp8_dequant_idct_add_uv_block_c(short *q, short *dq, unsigned char *dst_u, unsigned char *dst_v, int stride, char *eobs);
void vp8_dequant_idct_add_uv_block_mmx(short *q, short *dq, unsigned char *dst_u, unsigned char *dst_v, int stride, char *eobs);
void vp8_dequant_idct_add_uv_block_sse2(short *q, short *dq, unsigned char *dst_u, unsigned char *dst_v, int stride, char *eobs);
RTCD_EXTERN void (*vp8_dequant_idct_add_uv_block)(short *q, short *dq, unsigned char *dst_u, unsigned char *dst_v, int stride, char *eobs);

void vp8_dequant_idct_add_y_block_c(short *q, short *dq, unsigned char *dst, int stride, char *eobs);
void vp8_dequant_idct_add_y_block_mmx(short *q, short *dq, unsigned char *dst, int stride, char *eobs);
void vp8_dequant_idct_add_y_block_sse2(short *q, short *dq, unsigned char *dst, int stride, char *eobs);
RTCD_EXTERN void (*vp8_dequant_idct_add_y_block)(short *q, short *dq, unsigned char *dst, int stride, char *eobs);

void vp8_dequantize_b_c(struct blockd*, short *dqc);
void vp8_dequantize_b_mmx(struct blockd*, short *dqc);
RTCD_EXTERN void (*vp8_dequantize_b)(struct blockd*, short *dqc);

int vp8_diamond_search_sad_c(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, union int_mv *best_mv, int search_param, int sad_per_bit, int *num00, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);
int vp8_diamond_search_sadx4(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, union int_mv *best_mv, int search_param, int sad_per_bit, int *num00, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);
RTCD_EXTERN int (*vp8_diamond_search_sad)(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, union int_mv *best_mv, int search_param, int sad_per_bit, int *num00, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);

void vp8_fast_quantize_b_c(struct block *, struct blockd *);
void vp8_fast_quantize_b_sse2(struct block *, struct blockd *);
void vp8_fast_quantize_b_ssse3(struct block *, struct blockd *);
RTCD_EXTERN void (*vp8_fast_quantize_b)(struct block *, struct blockd *);

void vp8_fast_quantize_b_pair_c(struct block *b1, struct block *b2, struct blockd *d1, struct blockd *d2);
#define vp8_fast_quantize_b_pair vp8_fast_quantize_b_pair_c

void vp8_filter_by_weight16x16_c(unsigned char *src, int src_stride, unsigned char *dst, int dst_stride, int src_weight);
void vp8_filter_by_weight16x16_sse2(unsigned char *src, int src_stride, unsigned char *dst, int dst_stride, int src_weight);
RTCD_EXTERN void (*vp8_filter_by_weight16x16)(unsigned char *src, int src_stride, unsigned char *dst, int dst_stride, int src_weight);

void vp8_filter_by_weight4x4_c(unsigned char *src, int src_stride, unsigned char *dst, int dst_stride, int src_weight);
#define vp8_filter_by_weight4x4 vp8_filter_by_weight4x4_c

void vp8_filter_by_weight8x8_c(unsigned char *src, int src_stride, unsigned char *dst, int dst_stride, int src_weight);
void vp8_filter_by_weight8x8_sse2(unsigned char *src, int src_stride, unsigned char *dst, int dst_stride, int src_weight);
RTCD_EXTERN void (*vp8_filter_by_weight8x8)(unsigned char *src, int src_stride, unsigned char *dst, int dst_stride, int src_weight);

int vp8_full_search_sad_c(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);
int vp8_full_search_sadx3(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);
int vp8_full_search_sadx8(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);
RTCD_EXTERN int (*vp8_full_search_sad)(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);

unsigned int vp8_get4x4sse_cs_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride);
unsigned int vp8_get4x4sse_cs_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride);
RTCD_EXTERN unsigned int (*vp8_get4x4sse_cs)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride);

unsigned int vp8_get_mb_ss_c(const short *);
unsigned int vp8_get_mb_ss_mmx(const short *);
unsigned int vp8_get_mb_ss_sse2(const short *);
RTCD_EXTERN unsigned int (*vp8_get_mb_ss)(const short *);

void vp8_intra4x4_predict_c(unsigned char *Above, unsigned char *yleft, int left_stride, int b_mode, unsigned char *dst, int dst_stride, unsigned char top_left);
#define vp8_intra4x4_predict vp8_intra4x4_predict_c

void vp8_loop_filter_bh_c(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_bh_mmx(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_bh_sse2(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
RTCD_EXTERN void (*vp8_loop_filter_bh)(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);

void vp8_loop_filter_bv_c(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_bv_mmx(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_bv_sse2(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
RTCD_EXTERN void (*vp8_loop_filter_bv)(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);

void vp8_loop_filter_mbh_c(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_mbh_mmx(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_mbh_sse2(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
RTCD_EXTERN void (*vp8_loop_filter_mbh)(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);

void vp8_loop_filter_mbv_c(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_mbv_mmx(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
void vp8_loop_filter_mbv_sse2(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);
RTCD_EXTERN void (*vp8_loop_filter_mbv)(unsigned char *y, unsigned char *u, unsigned char *v, int ystride, int uv_stride, struct loop_filter_info *lfi);

void vp8_loop_filter_bhs_c(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_bhs_mmx(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_bhs_sse2(unsigned char *y, int ystride, const unsigned char *blimit);
RTCD_EXTERN void (*vp8_loop_filter_simple_bh)(unsigned char *y, int ystride, const unsigned char *blimit);

void vp8_loop_filter_bvs_c(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_bvs_mmx(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_bvs_sse2(unsigned char *y, int ystride, const unsigned char *blimit);
RTCD_EXTERN void (*vp8_loop_filter_simple_bv)(unsigned char *y, int ystride, const unsigned char *blimit);

void vp8_loop_filter_simple_horizontal_edge_c(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_simple_horizontal_edge_mmx(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_simple_horizontal_edge_sse2(unsigned char *y, int ystride, const unsigned char *blimit);
RTCD_EXTERN void (*vp8_loop_filter_simple_mbh)(unsigned char *y, int ystride, const unsigned char *blimit);

void vp8_loop_filter_simple_vertical_edge_c(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_simple_vertical_edge_mmx(unsigned char *y, int ystride, const unsigned char *blimit);
void vp8_loop_filter_simple_vertical_edge_sse2(unsigned char *y, int ystride, const unsigned char *blimit);
RTCD_EXTERN void (*vp8_loop_filter_simple_mbv)(unsigned char *y, int ystride, const unsigned char *blimit);

int vp8_mbblock_error_c(struct macroblock *mb, int dc);
int vp8_mbblock_error_mmx(struct macroblock *mb, int dc);
int vp8_mbblock_error_xmm(struct macroblock *mb, int dc);
RTCD_EXTERN int (*vp8_mbblock_error)(struct macroblock *mb, int dc);

void vp8_mbpost_proc_across_ip_c(unsigned char *dst, int pitch, int rows, int cols,int flimit);
void vp8_mbpost_proc_across_ip_xmm(unsigned char *dst, int pitch, int rows, int cols,int flimit);
RTCD_EXTERN void (*vp8_mbpost_proc_across_ip)(unsigned char *dst, int pitch, int rows, int cols,int flimit);

void vp8_mbpost_proc_down_c(unsigned char *dst, int pitch, int rows, int cols,int flimit);
void vp8_mbpost_proc_down_mmx(unsigned char *dst, int pitch, int rows, int cols,int flimit);
void vp8_mbpost_proc_down_xmm(unsigned char *dst, int pitch, int rows, int cols,int flimit);
RTCD_EXTERN void (*vp8_mbpost_proc_down)(unsigned char *dst, int pitch, int rows, int cols,int flimit);

int vp8_mbuverror_c(struct macroblock *mb);
int vp8_mbuverror_mmx(struct macroblock *mb);
int vp8_mbuverror_xmm(struct macroblock *mb);
RTCD_EXTERN int (*vp8_mbuverror)(struct macroblock *mb);

unsigned int vp8_mse16x16_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_mse16x16_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_mse16x16_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_mse16x16)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

void vp8_plane_add_noise_c(unsigned char *s, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int w, unsigned int h, int pitch);
void vp8_plane_add_noise_mmx(unsigned char *s, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int w, unsigned int h, int pitch);
void vp8_plane_add_noise_wmt(unsigned char *s, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int w, unsigned int h, int pitch);
RTCD_EXTERN void (*vp8_plane_add_noise)(unsigned char *s, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int w, unsigned int h, int pitch);

void vp8_post_proc_down_and_across_mb_row_c(unsigned char *src, unsigned char *dst, int src_pitch, int dst_pitch, int cols, unsigned char *flimits, int size);
void vp8_post_proc_down_and_across_mb_row_sse2(unsigned char *src, unsigned char *dst, int src_pitch, int dst_pitch, int cols, unsigned char *flimits, int size);
RTCD_EXTERN void (*vp8_post_proc_down_and_across_mb_row)(unsigned char *src, unsigned char *dst, int src_pitch, int dst_pitch, int cols, unsigned char *flimits, int size);

void vp8_quantize_mb_c(struct macroblock *);
#define vp8_quantize_mb vp8_quantize_mb_c

void vp8_quantize_mbuv_c(struct macroblock *);
#define vp8_quantize_mbuv vp8_quantize_mbuv_c

void vp8_quantize_mby_c(struct macroblock *);
#define vp8_quantize_mby vp8_quantize_mby_c

int vp8_refining_search_sad_c(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);
int vp8_refining_search_sadx4(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);
RTCD_EXTERN int (*vp8_refining_search_sad)(struct macroblock *x, struct block *b, struct blockd *d, union int_mv *ref_mv, int sad_per_bit, int distance, struct variance_vtable *fn_ptr, int *mvcost[2], union int_mv *center_mv);

void vp8_regular_quantize_b_c(struct block *, struct blockd *);
void vp8_regular_quantize_b_sse2(struct block *, struct blockd *);
void vp8_regular_quantize_b_sse4_1(struct block *, struct blockd *);
RTCD_EXTERN void (*vp8_regular_quantize_b)(struct block *, struct blockd *);

void vp8_regular_quantize_b_pair_c(struct block *b1, struct block *b2, struct blockd *d1, struct blockd *d2);
#define vp8_regular_quantize_b_pair vp8_regular_quantize_b_pair_c

unsigned int vp8_sad16x16_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad16x16_mmx(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad16x16_wmt(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad16x16_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
RTCD_EXTERN unsigned int (*vp8_sad16x16)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);

void vp8_sad16x16x3_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp8_sad16x16x3_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp8_sad16x16x3_ssse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad16x16x3)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp8_sad16x16x4d_c(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp8_sad16x16x4d_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad16x16x4d)(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp8_sad16x16x8_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
void vp8_sad16x16x8_sse4(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
RTCD_EXTERN void (*vp8_sad16x16x8)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);

unsigned int vp8_sad16x8_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad16x8_mmx(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad16x8_wmt(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
RTCD_EXTERN unsigned int (*vp8_sad16x8)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);

void vp8_sad16x8x3_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp8_sad16x8x3_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp8_sad16x8x3_ssse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad16x8x3)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp8_sad16x8x4d_c(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp8_sad16x8x4d_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad16x8x4d)(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp8_sad16x8x8_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
void vp8_sad16x8x8_sse4(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
RTCD_EXTERN void (*vp8_sad16x8x8)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);

unsigned int vp8_sad4x4_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad4x4_mmx(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad4x4_wmt(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
RTCD_EXTERN unsigned int (*vp8_sad4x4)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);

void vp8_sad4x4x3_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp8_sad4x4x3_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad4x4x3)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp8_sad4x4x4d_c(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp8_sad4x4x4d_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad4x4x4d)(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp8_sad4x4x8_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
void vp8_sad4x4x8_sse4(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
RTCD_EXTERN void (*vp8_sad4x4x8)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);

unsigned int vp8_sad8x16_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad8x16_mmx(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad8x16_wmt(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
RTCD_EXTERN unsigned int (*vp8_sad8x16)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);

void vp8_sad8x16x3_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp8_sad8x16x3_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad8x16x3)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp8_sad8x16x4d_c(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp8_sad8x16x4d_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad8x16x4d)(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp8_sad8x16x8_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
void vp8_sad8x16x8_sse4(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
RTCD_EXTERN void (*vp8_sad8x16x8)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);

unsigned int vp8_sad8x8_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad8x8_mmx(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
unsigned int vp8_sad8x8_wmt(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);
RTCD_EXTERN unsigned int (*vp8_sad8x8)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int ref_stride, unsigned int max_sad);

void vp8_sad8x8x3_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
void vp8_sad8x8x3_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad8x8x3)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sad_array);

void vp8_sad8x8x4d_c(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
void vp8_sad8x8x4d_sse3(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);
RTCD_EXTERN void (*vp8_sad8x8x4d)(const unsigned char *src_ptr, int src_stride, const unsigned char * const ref_ptr[], int  ref_stride, unsigned int *sad_array);

void vp8_sad8x8x8_c(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
void vp8_sad8x8x8_sse4(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);
RTCD_EXTERN void (*vp8_sad8x8x8)(const unsigned char *src_ptr, int src_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned short *sad_array);

void vp8_short_fdct4x4_c(short *input, short *output, int pitch);
void vp8_short_fdct4x4_mmx(short *input, short *output, int pitch);
void vp8_short_fdct4x4_sse2(short *input, short *output, int pitch);
RTCD_EXTERN void (*vp8_short_fdct4x4)(short *input, short *output, int pitch);

void vp8_short_fdct8x4_c(short *input, short *output, int pitch);
void vp8_short_fdct8x4_mmx(short *input, short *output, int pitch);
void vp8_short_fdct8x4_sse2(short *input, short *output, int pitch);
RTCD_EXTERN void (*vp8_short_fdct8x4)(short *input, short *output, int pitch);

void vp8_short_idct4x4llm_c(short *input, unsigned char *pred, int pitch, unsigned char *dst, int dst_stride);
void vp8_short_idct4x4llm_mmx(short *input, unsigned char *pred, int pitch, unsigned char *dst, int dst_stride);
RTCD_EXTERN void (*vp8_short_idct4x4llm)(short *input, unsigned char *pred, int pitch, unsigned char *dst, int dst_stride);

void vp8_short_inv_walsh4x4_c(short *input, short *output);
void vp8_short_inv_walsh4x4_mmx(short *input, short *output);
void vp8_short_inv_walsh4x4_sse2(short *input, short *output);
RTCD_EXTERN void (*vp8_short_inv_walsh4x4)(short *input, short *output);

void vp8_short_inv_walsh4x4_1_c(short *input, short *output);
#define vp8_short_inv_walsh4x4_1 vp8_short_inv_walsh4x4_1_c

void vp8_short_walsh4x4_c(short *input, short *output, int pitch);
void vp8_short_walsh4x4_sse2(short *input, short *output, int pitch);
RTCD_EXTERN void (*vp8_short_walsh4x4)(short *input, short *output, int pitch);

void vp8_sixtap_predict16x16_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict16x16_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict16x16_sse2(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict16x16_ssse3(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_sixtap_predict16x16)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

void vp8_sixtap_predict4x4_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict4x4_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict4x4_ssse3(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_sixtap_predict4x4)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

void vp8_sixtap_predict8x4_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict8x4_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict8x4_sse2(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict8x4_ssse3(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_sixtap_predict8x4)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

void vp8_sixtap_predict8x8_c(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict8x8_mmx(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict8x8_sse2(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
void vp8_sixtap_predict8x8_ssse3(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);
RTCD_EXTERN void (*vp8_sixtap_predict8x8)(unsigned char *src, int src_pitch, int xofst, int yofst, unsigned char *dst, int dst_pitch);

unsigned int vp8_sub_pixel_mse16x16_c(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_mse16x16_mmx(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_mse16x16_wmt(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_sub_pixel_mse16x16)(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);

unsigned int vp8_sub_pixel_variance16x16_c(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance16x16_mmx(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance16x16_wmt(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance16x16_ssse3(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_sub_pixel_variance16x16)(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);

unsigned int vp8_sub_pixel_variance16x8_c(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance16x8_mmx(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance16x8_wmt(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance16x8_ssse3(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_sub_pixel_variance16x8)(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);

unsigned int vp8_sub_pixel_variance4x4_c(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance4x4_mmx(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance4x4_wmt(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_sub_pixel_variance4x4)(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);

unsigned int vp8_sub_pixel_variance8x16_c(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance8x16_mmx(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance8x16_wmt(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_sub_pixel_variance8x16)(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);

unsigned int vp8_sub_pixel_variance8x8_c(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance8x8_mmx(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
unsigned int vp8_sub_pixel_variance8x8_wmt(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_sub_pixel_variance8x8)(const unsigned char  *src_ptr, int  source_stride, int  xoffset, int  yoffset, const unsigned char *ref_ptr, int Refstride, unsigned int *sse);

void vp8_subtract_b_c(struct block *be, struct blockd *bd, int pitch);
void vp8_subtract_b_mmx(struct block *be, struct blockd *bd, int pitch);
void vp8_subtract_b_sse2(struct block *be, struct blockd *bd, int pitch);
RTCD_EXTERN void (*vp8_subtract_b)(struct block *be, struct blockd *bd, int pitch);

void vp8_subtract_mbuv_c(short *diff, unsigned char *usrc, unsigned char *vsrc, int src_stride, unsigned char *upred, unsigned char *vpred, int pred_stride);
void vp8_subtract_mbuv_mmx(short *diff, unsigned char *usrc, unsigned char *vsrc, int src_stride, unsigned char *upred, unsigned char *vpred, int pred_stride);
void vp8_subtract_mbuv_sse2(short *diff, unsigned char *usrc, unsigned char *vsrc, int src_stride, unsigned char *upred, unsigned char *vpred, int pred_stride);
RTCD_EXTERN void (*vp8_subtract_mbuv)(short *diff, unsigned char *usrc, unsigned char *vsrc, int src_stride, unsigned char *upred, unsigned char *vpred, int pred_stride);

void vp8_subtract_mby_c(short *diff, unsigned char *src, int src_stride, unsigned char *pred, int pred_stride);
void vp8_subtract_mby_mmx(short *diff, unsigned char *src, int src_stride, unsigned char *pred, int pred_stride);
void vp8_subtract_mby_sse2(short *diff, unsigned char *src, int src_stride, unsigned char *pred, int pred_stride);
RTCD_EXTERN void (*vp8_subtract_mby)(short *diff, unsigned char *src, int src_stride, unsigned char *pred, int pred_stride);

void vp8_temporal_filter_apply_c(unsigned char *frame1, unsigned int stride, unsigned char *frame2, unsigned int block_size, int strength, int filter_weight, unsigned int *accumulator, unsigned short *count);
void vp8_temporal_filter_apply_sse2(unsigned char *frame1, unsigned int stride, unsigned char *frame2, unsigned int block_size, int strength, int filter_weight, unsigned int *accumulator, unsigned short *count);
RTCD_EXTERN void (*vp8_temporal_filter_apply)(unsigned char *frame1, unsigned int stride, unsigned char *frame2, unsigned int block_size, int strength, int filter_weight, unsigned int *accumulator, unsigned short *count);

unsigned int vp8_variance16x16_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance16x16_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance16x16_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance16x16)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

unsigned int vp8_variance16x8_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance16x8_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance16x8_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance16x8)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

unsigned int vp8_variance4x4_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance4x4_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance4x4_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance4x4)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

unsigned int vp8_variance8x16_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance8x16_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance8x16_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance8x16)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

unsigned int vp8_variance8x8_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance8x8_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance8x8_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance8x8)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

unsigned int vp8_variance_halfpixvar16x16_h_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance_halfpixvar16x16_h_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance_halfpixvar16x16_h_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance_halfpixvar16x16_h)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

unsigned int vp8_variance_halfpixvar16x16_hv_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance_halfpixvar16x16_hv_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance_halfpixvar16x16_hv_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance_halfpixvar16x16_hv)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

unsigned int vp8_variance_halfpixvar16x16_v_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance_halfpixvar16x16_v_mmx(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
unsigned int vp8_variance_halfpixvar16x16_v_wmt(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*vp8_variance_halfpixvar16x16_v)(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, unsigned int *sse);

void vp8_rtcd(void);

#ifdef RTCD_C
#include "vpx_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;

    vp8_bilinear_predict16x16 = vp8_bilinear_predict16x16_c;
    if (flags & HAS_MMX) vp8_bilinear_predict16x16 = vp8_bilinear_predict16x16_mmx;
    if (flags & HAS_SSE2) vp8_bilinear_predict16x16 = vp8_bilinear_predict16x16_sse2;
    if (flags & HAS_SSSE3) vp8_bilinear_predict16x16 = vp8_bilinear_predict16x16_ssse3;
    vp8_bilinear_predict4x4 = vp8_bilinear_predict4x4_c;
    if (flags & HAS_MMX) vp8_bilinear_predict4x4 = vp8_bilinear_predict4x4_mmx;
    vp8_bilinear_predict8x4 = vp8_bilinear_predict8x4_c;
    if (flags & HAS_MMX) vp8_bilinear_predict8x4 = vp8_bilinear_predict8x4_mmx;
    vp8_bilinear_predict8x8 = vp8_bilinear_predict8x8_c;
    if (flags & HAS_MMX) vp8_bilinear_predict8x8 = vp8_bilinear_predict8x8_mmx;
    if (flags & HAS_SSE2) vp8_bilinear_predict8x8 = vp8_bilinear_predict8x8_sse2;
    if (flags & HAS_SSSE3) vp8_bilinear_predict8x8 = vp8_bilinear_predict8x8_ssse3;
    vp8_block_error = vp8_block_error_c;
    if (flags & HAS_MMX) vp8_block_error = vp8_block_error_mmx;
    if (flags & HAS_SSE2) vp8_block_error = vp8_block_error_xmm;
    vp8_build_intra_predictors_mbuv_s = vp8_build_intra_predictors_mbuv_s_c;
    if (flags & HAS_SSE2) vp8_build_intra_predictors_mbuv_s = vp8_build_intra_predictors_mbuv_s_sse2;
    if (flags & HAS_SSSE3) vp8_build_intra_predictors_mbuv_s = vp8_build_intra_predictors_mbuv_s_ssse3;
    vp8_build_intra_predictors_mby_s = vp8_build_intra_predictors_mby_s_c;
    if (flags & HAS_SSE2) vp8_build_intra_predictors_mby_s = vp8_build_intra_predictors_mby_s_sse2;
    if (flags & HAS_SSSE3) vp8_build_intra_predictors_mby_s = vp8_build_intra_predictors_mby_s_ssse3;
    vp8_clear_system_state = vp8_clear_system_state_c;
    if (flags & HAS_MMX) vp8_clear_system_state = vpx_reset_mmx_state;
    vp8_copy32xn = vp8_copy32xn_c;
    if (flags & HAS_SSE2) vp8_copy32xn = vp8_copy32xn_sse2;
    if (flags & HAS_SSE3) vp8_copy32xn = vp8_copy32xn_sse3;
    vp8_copy_mem16x16 = vp8_copy_mem16x16_c;
    if (flags & HAS_MMX) vp8_copy_mem16x16 = vp8_copy_mem16x16_mmx;
    if (flags & HAS_SSE2) vp8_copy_mem16x16 = vp8_copy_mem16x16_sse2;
    vp8_copy_mem8x4 = vp8_copy_mem8x4_c;
    if (flags & HAS_MMX) vp8_copy_mem8x4 = vp8_copy_mem8x4_mmx;
    vp8_copy_mem8x8 = vp8_copy_mem8x8_c;
    if (flags & HAS_MMX) vp8_copy_mem8x8 = vp8_copy_mem8x8_mmx;
    vp8_dc_only_idct_add = vp8_dc_only_idct_add_c;
    if (flags & HAS_MMX) vp8_dc_only_idct_add = vp8_dc_only_idct_add_mmx;
    vp8_denoiser_filter = vp8_denoiser_filter_c;
    if (flags & HAS_SSE2) vp8_denoiser_filter = vp8_denoiser_filter_sse2;
    vp8_denoiser_filter_uv = vp8_denoiser_filter_uv_c;
    if (flags & HAS_SSE2) vp8_denoiser_filter_uv = vp8_denoiser_filter_uv_sse2;
    vp8_dequant_idct_add = vp8_dequant_idct_add_c;
    if (flags & HAS_MMX) vp8_dequant_idct_add = vp8_dequant_idct_add_mmx;
    vp8_dequant_idct_add_uv_block = vp8_dequant_idct_add_uv_block_c;
    if (flags & HAS_MMX) vp8_dequant_idct_add_uv_block = vp8_dequant_idct_add_uv_block_mmx;
    if (flags & HAS_SSE2) vp8_dequant_idct_add_uv_block = vp8_dequant_idct_add_uv_block_sse2;
    vp8_dequant_idct_add_y_block = vp8_dequant_idct_add_y_block_c;
    if (flags & HAS_MMX) vp8_dequant_idct_add_y_block = vp8_dequant_idct_add_y_block_mmx;
    if (flags & HAS_SSE2) vp8_dequant_idct_add_y_block = vp8_dequant_idct_add_y_block_sse2;
    vp8_dequantize_b = vp8_dequantize_b_c;
    if (flags & HAS_MMX) vp8_dequantize_b = vp8_dequantize_b_mmx;
    vp8_diamond_search_sad = vp8_diamond_search_sad_c;
    if (flags & HAS_SSE3) vp8_diamond_search_sad = vp8_diamond_search_sadx4;
    vp8_fast_quantize_b = vp8_fast_quantize_b_c;
    if (flags & HAS_SSE2) vp8_fast_quantize_b = vp8_fast_quantize_b_sse2;
    if (flags & HAS_SSSE3) vp8_fast_quantize_b = vp8_fast_quantize_b_ssse3;
    vp8_filter_by_weight16x16 = vp8_filter_by_weight16x16_c;
    if (flags & HAS_SSE2) vp8_filter_by_weight16x16 = vp8_filter_by_weight16x16_sse2;
    vp8_filter_by_weight8x8 = vp8_filter_by_weight8x8_c;
    if (flags & HAS_SSE2) vp8_filter_by_weight8x8 = vp8_filter_by_weight8x8_sse2;
    vp8_full_search_sad = vp8_full_search_sad_c;
    if (flags & HAS_SSE3) vp8_full_search_sad = vp8_full_search_sadx3;
    if (flags & HAS_SSE4_1) vp8_full_search_sad = vp8_full_search_sadx8;
    vp8_get4x4sse_cs = vp8_get4x4sse_cs_c;
    if (flags & HAS_MMX) vp8_get4x4sse_cs = vp8_get4x4sse_cs_mmx;
    vp8_get_mb_ss = vp8_get_mb_ss_c;
    if (flags & HAS_MMX) vp8_get_mb_ss = vp8_get_mb_ss_mmx;
    if (flags & HAS_SSE2) vp8_get_mb_ss = vp8_get_mb_ss_sse2;
    vp8_loop_filter_bh = vp8_loop_filter_bh_c;
    if (flags & HAS_MMX) vp8_loop_filter_bh = vp8_loop_filter_bh_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_bh = vp8_loop_filter_bh_sse2;
    vp8_loop_filter_bv = vp8_loop_filter_bv_c;
    if (flags & HAS_MMX) vp8_loop_filter_bv = vp8_loop_filter_bv_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_bv = vp8_loop_filter_bv_sse2;
    vp8_loop_filter_mbh = vp8_loop_filter_mbh_c;
    if (flags & HAS_MMX) vp8_loop_filter_mbh = vp8_loop_filter_mbh_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_mbh = vp8_loop_filter_mbh_sse2;
    vp8_loop_filter_mbv = vp8_loop_filter_mbv_c;
    if (flags & HAS_MMX) vp8_loop_filter_mbv = vp8_loop_filter_mbv_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_mbv = vp8_loop_filter_mbv_sse2;
    vp8_loop_filter_simple_bh = vp8_loop_filter_bhs_c;
    if (flags & HAS_MMX) vp8_loop_filter_simple_bh = vp8_loop_filter_bhs_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_simple_bh = vp8_loop_filter_bhs_sse2;
    vp8_loop_filter_simple_bv = vp8_loop_filter_bvs_c;
    if (flags & HAS_MMX) vp8_loop_filter_simple_bv = vp8_loop_filter_bvs_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_simple_bv = vp8_loop_filter_bvs_sse2;
    vp8_loop_filter_simple_mbh = vp8_loop_filter_simple_horizontal_edge_c;
    if (flags & HAS_MMX) vp8_loop_filter_simple_mbh = vp8_loop_filter_simple_horizontal_edge_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_simple_mbh = vp8_loop_filter_simple_horizontal_edge_sse2;
    vp8_loop_filter_simple_mbv = vp8_loop_filter_simple_vertical_edge_c;
    if (flags & HAS_MMX) vp8_loop_filter_simple_mbv = vp8_loop_filter_simple_vertical_edge_mmx;
    if (flags & HAS_SSE2) vp8_loop_filter_simple_mbv = vp8_loop_filter_simple_vertical_edge_sse2;
    vp8_mbblock_error = vp8_mbblock_error_c;
    if (flags & HAS_MMX) vp8_mbblock_error = vp8_mbblock_error_mmx;
    if (flags & HAS_SSE2) vp8_mbblock_error = vp8_mbblock_error_xmm;
    vp8_mbpost_proc_across_ip = vp8_mbpost_proc_across_ip_c;
    if (flags & HAS_SSE2) vp8_mbpost_proc_across_ip = vp8_mbpost_proc_across_ip_xmm;
    vp8_mbpost_proc_down = vp8_mbpost_proc_down_c;
    if (flags & HAS_MMX) vp8_mbpost_proc_down = vp8_mbpost_proc_down_mmx;
    if (flags & HAS_SSE2) vp8_mbpost_proc_down = vp8_mbpost_proc_down_xmm;
    vp8_mbuverror = vp8_mbuverror_c;
    if (flags & HAS_MMX) vp8_mbuverror = vp8_mbuverror_mmx;
    if (flags & HAS_SSE2) vp8_mbuverror = vp8_mbuverror_xmm;
    vp8_mse16x16 = vp8_mse16x16_c;
    if (flags & HAS_MMX) vp8_mse16x16 = vp8_mse16x16_mmx;
    if (flags & HAS_SSE2) vp8_mse16x16 = vp8_mse16x16_wmt;
    vp8_plane_add_noise = vp8_plane_add_noise_c;
    if (flags & HAS_MMX) vp8_plane_add_noise = vp8_plane_add_noise_mmx;
    if (flags & HAS_SSE2) vp8_plane_add_noise = vp8_plane_add_noise_wmt;
    vp8_post_proc_down_and_across_mb_row = vp8_post_proc_down_and_across_mb_row_c;
    if (flags & HAS_SSE2) vp8_post_proc_down_and_across_mb_row = vp8_post_proc_down_and_across_mb_row_sse2;
    vp8_refining_search_sad = vp8_refining_search_sad_c;
    if (flags & HAS_SSE3) vp8_refining_search_sad = vp8_refining_search_sadx4;
    vp8_regular_quantize_b = vp8_regular_quantize_b_c;
    if (flags & HAS_SSE2) vp8_regular_quantize_b = vp8_regular_quantize_b_sse2;
    if (flags & HAS_SSE4_1) vp8_regular_quantize_b = vp8_regular_quantize_b_sse4_1;
    vp8_sad16x16 = vp8_sad16x16_c;
    if (flags & HAS_MMX) vp8_sad16x16 = vp8_sad16x16_mmx;
    if (flags & HAS_SSE2) vp8_sad16x16 = vp8_sad16x16_wmt;
    if (flags & HAS_SSE3) vp8_sad16x16 = vp8_sad16x16_sse3;
    vp8_sad16x16x3 = vp8_sad16x16x3_c;
    if (flags & HAS_SSE3) vp8_sad16x16x3 = vp8_sad16x16x3_sse3;
    if (flags & HAS_SSSE3) vp8_sad16x16x3 = vp8_sad16x16x3_ssse3;
    vp8_sad16x16x4d = vp8_sad16x16x4d_c;
    if (flags & HAS_SSE3) vp8_sad16x16x4d = vp8_sad16x16x4d_sse3;
    vp8_sad16x16x8 = vp8_sad16x16x8_c;
    if (flags & HAS_SSE4_1) vp8_sad16x16x8 = vp8_sad16x16x8_sse4;
    vp8_sad16x8 = vp8_sad16x8_c;
    if (flags & HAS_MMX) vp8_sad16x8 = vp8_sad16x8_mmx;
    if (flags & HAS_SSE2) vp8_sad16x8 = vp8_sad16x8_wmt;
    vp8_sad16x8x3 = vp8_sad16x8x3_c;
    if (flags & HAS_SSE3) vp8_sad16x8x3 = vp8_sad16x8x3_sse3;
    if (flags & HAS_SSSE3) vp8_sad16x8x3 = vp8_sad16x8x3_ssse3;
    vp8_sad16x8x4d = vp8_sad16x8x4d_c;
    if (flags & HAS_SSE3) vp8_sad16x8x4d = vp8_sad16x8x4d_sse3;
    vp8_sad16x8x8 = vp8_sad16x8x8_c;
    if (flags & HAS_SSE4_1) vp8_sad16x8x8 = vp8_sad16x8x8_sse4;
    vp8_sad4x4 = vp8_sad4x4_c;
    if (flags & HAS_MMX) vp8_sad4x4 = vp8_sad4x4_mmx;
    if (flags & HAS_SSE2) vp8_sad4x4 = vp8_sad4x4_wmt;
    vp8_sad4x4x3 = vp8_sad4x4x3_c;
    if (flags & HAS_SSE3) vp8_sad4x4x3 = vp8_sad4x4x3_sse3;
    vp8_sad4x4x4d = vp8_sad4x4x4d_c;
    if (flags & HAS_SSE3) vp8_sad4x4x4d = vp8_sad4x4x4d_sse3;
    vp8_sad4x4x8 = vp8_sad4x4x8_c;
    if (flags & HAS_SSE4_1) vp8_sad4x4x8 = vp8_sad4x4x8_sse4;
    vp8_sad8x16 = vp8_sad8x16_c;
    if (flags & HAS_MMX) vp8_sad8x16 = vp8_sad8x16_mmx;
    if (flags & HAS_SSE2) vp8_sad8x16 = vp8_sad8x16_wmt;
    vp8_sad8x16x3 = vp8_sad8x16x3_c;
    if (flags & HAS_SSE3) vp8_sad8x16x3 = vp8_sad8x16x3_sse3;
    vp8_sad8x16x4d = vp8_sad8x16x4d_c;
    if (flags & HAS_SSE3) vp8_sad8x16x4d = vp8_sad8x16x4d_sse3;
    vp8_sad8x16x8 = vp8_sad8x16x8_c;
    if (flags & HAS_SSE4_1) vp8_sad8x16x8 = vp8_sad8x16x8_sse4;
    vp8_sad8x8 = vp8_sad8x8_c;
    if (flags & HAS_MMX) vp8_sad8x8 = vp8_sad8x8_mmx;
    if (flags & HAS_SSE2) vp8_sad8x8 = vp8_sad8x8_wmt;
    vp8_sad8x8x3 = vp8_sad8x8x3_c;
    if (flags & HAS_SSE3) vp8_sad8x8x3 = vp8_sad8x8x3_sse3;
    vp8_sad8x8x4d = vp8_sad8x8x4d_c;
    if (flags & HAS_SSE3) vp8_sad8x8x4d = vp8_sad8x8x4d_sse3;
    vp8_sad8x8x8 = vp8_sad8x8x8_c;
    if (flags & HAS_SSE4_1) vp8_sad8x8x8 = vp8_sad8x8x8_sse4;
    vp8_short_fdct4x4 = vp8_short_fdct4x4_c;
    if (flags & HAS_MMX) vp8_short_fdct4x4 = vp8_short_fdct4x4_mmx;
    if (flags & HAS_SSE2) vp8_short_fdct4x4 = vp8_short_fdct4x4_sse2;
    vp8_short_fdct8x4 = vp8_short_fdct8x4_c;
    if (flags & HAS_MMX) vp8_short_fdct8x4 = vp8_short_fdct8x4_mmx;
    if (flags & HAS_SSE2) vp8_short_fdct8x4 = vp8_short_fdct8x4_sse2;
    vp8_short_idct4x4llm = vp8_short_idct4x4llm_c;
    if (flags & HAS_MMX) vp8_short_idct4x4llm = vp8_short_idct4x4llm_mmx;
    vp8_short_inv_walsh4x4 = vp8_short_inv_walsh4x4_c;
    if (flags & HAS_MMX) vp8_short_inv_walsh4x4 = vp8_short_inv_walsh4x4_mmx;
    if (flags & HAS_SSE2) vp8_short_inv_walsh4x4 = vp8_short_inv_walsh4x4_sse2;
    vp8_short_walsh4x4 = vp8_short_walsh4x4_c;
    if (flags & HAS_SSE2) vp8_short_walsh4x4 = vp8_short_walsh4x4_sse2;
    vp8_sixtap_predict16x16 = vp8_sixtap_predict16x16_c;
    if (flags & HAS_MMX) vp8_sixtap_predict16x16 = vp8_sixtap_predict16x16_mmx;
    if (flags & HAS_SSE2) vp8_sixtap_predict16x16 = vp8_sixtap_predict16x16_sse2;
    if (flags & HAS_SSSE3) vp8_sixtap_predict16x16 = vp8_sixtap_predict16x16_ssse3;
    vp8_sixtap_predict4x4 = vp8_sixtap_predict4x4_c;
    if (flags & HAS_MMX) vp8_sixtap_predict4x4 = vp8_sixtap_predict4x4_mmx;
    if (flags & HAS_SSSE3) vp8_sixtap_predict4x4 = vp8_sixtap_predict4x4_ssse3;
    vp8_sixtap_predict8x4 = vp8_sixtap_predict8x4_c;
    if (flags & HAS_MMX) vp8_sixtap_predict8x4 = vp8_sixtap_predict8x4_mmx;
    if (flags & HAS_SSE2) vp8_sixtap_predict8x4 = vp8_sixtap_predict8x4_sse2;
    if (flags & HAS_SSSE3) vp8_sixtap_predict8x4 = vp8_sixtap_predict8x4_ssse3;
    vp8_sixtap_predict8x8 = vp8_sixtap_predict8x8_c;
    if (flags & HAS_MMX) vp8_sixtap_predict8x8 = vp8_sixtap_predict8x8_mmx;
    if (flags & HAS_SSE2) vp8_sixtap_predict8x8 = vp8_sixtap_predict8x8_sse2;
    if (flags & HAS_SSSE3) vp8_sixtap_predict8x8 = vp8_sixtap_predict8x8_ssse3;
    vp8_sub_pixel_mse16x16 = vp8_sub_pixel_mse16x16_c;
    if (flags & HAS_MMX) vp8_sub_pixel_mse16x16 = vp8_sub_pixel_mse16x16_mmx;
    if (flags & HAS_SSE2) vp8_sub_pixel_mse16x16 = vp8_sub_pixel_mse16x16_wmt;
    vp8_sub_pixel_variance16x16 = vp8_sub_pixel_variance16x16_c;
    if (flags & HAS_MMX) vp8_sub_pixel_variance16x16 = vp8_sub_pixel_variance16x16_mmx;
    if (flags & HAS_SSE2) vp8_sub_pixel_variance16x16 = vp8_sub_pixel_variance16x16_wmt;
    if (flags & HAS_SSSE3) vp8_sub_pixel_variance16x16 = vp8_sub_pixel_variance16x16_ssse3;
    vp8_sub_pixel_variance16x8 = vp8_sub_pixel_variance16x8_c;
    if (flags & HAS_MMX) vp8_sub_pixel_variance16x8 = vp8_sub_pixel_variance16x8_mmx;
    if (flags & HAS_SSE2) vp8_sub_pixel_variance16x8 = vp8_sub_pixel_variance16x8_wmt;
    if (flags & HAS_SSSE3) vp8_sub_pixel_variance16x8 = vp8_sub_pixel_variance16x8_ssse3;
    vp8_sub_pixel_variance4x4 = vp8_sub_pixel_variance4x4_c;
    if (flags & HAS_MMX) vp8_sub_pixel_variance4x4 = vp8_sub_pixel_variance4x4_mmx;
    if (flags & HAS_SSE2) vp8_sub_pixel_variance4x4 = vp8_sub_pixel_variance4x4_wmt;
    vp8_sub_pixel_variance8x16 = vp8_sub_pixel_variance8x16_c;
    if (flags & HAS_MMX) vp8_sub_pixel_variance8x16 = vp8_sub_pixel_variance8x16_mmx;
    if (flags & HAS_SSE2) vp8_sub_pixel_variance8x16 = vp8_sub_pixel_variance8x16_wmt;
    vp8_sub_pixel_variance8x8 = vp8_sub_pixel_variance8x8_c;
    if (flags & HAS_MMX) vp8_sub_pixel_variance8x8 = vp8_sub_pixel_variance8x8_mmx;
    if (flags & HAS_SSE2) vp8_sub_pixel_variance8x8 = vp8_sub_pixel_variance8x8_wmt;
    vp8_subtract_b = vp8_subtract_b_c;
    if (flags & HAS_MMX) vp8_subtract_b = vp8_subtract_b_mmx;
    if (flags & HAS_SSE2) vp8_subtract_b = vp8_subtract_b_sse2;
    vp8_subtract_mbuv = vp8_subtract_mbuv_c;
    if (flags & HAS_MMX) vp8_subtract_mbuv = vp8_subtract_mbuv_mmx;
    if (flags & HAS_SSE2) vp8_subtract_mbuv = vp8_subtract_mbuv_sse2;
    vp8_subtract_mby = vp8_subtract_mby_c;
    if (flags & HAS_MMX) vp8_subtract_mby = vp8_subtract_mby_mmx;
    if (flags & HAS_SSE2) vp8_subtract_mby = vp8_subtract_mby_sse2;
    vp8_temporal_filter_apply = vp8_temporal_filter_apply_c;
    if (flags & HAS_SSE2) vp8_temporal_filter_apply = vp8_temporal_filter_apply_sse2;
    vp8_variance16x16 = vp8_variance16x16_c;
    if (flags & HAS_MMX) vp8_variance16x16 = vp8_variance16x16_mmx;
    if (flags & HAS_SSE2) vp8_variance16x16 = vp8_variance16x16_wmt;
    vp8_variance16x8 = vp8_variance16x8_c;
    if (flags & HAS_MMX) vp8_variance16x8 = vp8_variance16x8_mmx;
    if (flags & HAS_SSE2) vp8_variance16x8 = vp8_variance16x8_wmt;
    vp8_variance4x4 = vp8_variance4x4_c;
    if (flags & HAS_MMX) vp8_variance4x4 = vp8_variance4x4_mmx;
    if (flags & HAS_SSE2) vp8_variance4x4 = vp8_variance4x4_wmt;
    vp8_variance8x16 = vp8_variance8x16_c;
    if (flags & HAS_MMX) vp8_variance8x16 = vp8_variance8x16_mmx;
    if (flags & HAS_SSE2) vp8_variance8x16 = vp8_variance8x16_wmt;
    vp8_variance8x8 = vp8_variance8x8_c;
    if (flags & HAS_MMX) vp8_variance8x8 = vp8_variance8x8_mmx;
    if (flags & HAS_SSE2) vp8_variance8x8 = vp8_variance8x8_wmt;
    vp8_variance_halfpixvar16x16_h = vp8_variance_halfpixvar16x16_h_c;
    if (flags & HAS_MMX) vp8_variance_halfpixvar16x16_h = vp8_variance_halfpixvar16x16_h_mmx;
    if (flags & HAS_SSE2) vp8_variance_halfpixvar16x16_h = vp8_variance_halfpixvar16x16_h_wmt;
    vp8_variance_halfpixvar16x16_hv = vp8_variance_halfpixvar16x16_hv_c;
    if (flags & HAS_MMX) vp8_variance_halfpixvar16x16_hv = vp8_variance_halfpixvar16x16_hv_mmx;
    if (flags & HAS_SSE2) vp8_variance_halfpixvar16x16_hv = vp8_variance_halfpixvar16x16_hv_wmt;
    vp8_variance_halfpixvar16x16_v = vp8_variance_halfpixvar16x16_v_c;
    if (flags & HAS_MMX) vp8_variance_halfpixvar16x16_v = vp8_variance_halfpixvar16x16_v_mmx;
    if (flags & HAS_SSE2) vp8_variance_halfpixvar16x16_v = vp8_variance_halfpixvar16x16_v_wmt;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
