/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Prototypes for the inline templates in vis.il
 */

#ifndef VIS_PROTO_H
#define VIS_PROTO_H

#pragma ident	"@(#)vis_proto.h	1.3	97/03/30 SMI"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Pure edge handling instructions */
int vis_edge8(void * /*frs1*/, void * /*frs2*/);
int vis_edge8l(void * /*frs1*/, void * /*frs2*/);
int vis_edge16(void * /*frs1*/, void * /*frs2*/);
int vis_edge16l(void * /*frs1*/, void * /*frs2*/);
int vis_edge32(void * /*frs1*/, void * /*frs2*/);
int vis_edge32l(void * /*frs1*/, void * /*frs2*/);

/* Edge handling instructions with negative return values if cc set. */
int vis_edge8cc(void * /*frs1*/, void * /*frs2*/);
int vis_edge8lcc(void * /*frs1*/, void * /*frs2*/);
int vis_edge16cc(void * /*frs1*/, void * /*frs2*/);
int vis_edge16lcc(void * /*frs1*/, void * /*frs2*/);
int vis_edge32cc(void * /*frs1*/, void * /*frs2*/);
int vis_edge32lcc(void * /*frs1*/, void * /*frs2*/);

/* Alignment instructions. */
void *vis_alignaddr(void * /*rs1*/, int /*rs2*/);
void *vis_alignaddrl(void * /*rs1*/, int /*rs2*/);
double vis_faligndata(double /*frs1*/, double /*frs2*/);

/* Partitioned comparison instructions. */
int vis_fcmple16(double /*frs1*/, double /*frs2*/);
int vis_fcmpne16(double /*frs1*/, double /*frs2*/);
int vis_fcmple32(double /*frs1*/, double /*frs2*/);
int vis_fcmpne32(double /*frs1*/, double /*frs2*/);
int vis_fcmpgt16(double /*frs1*/, double /*frs2*/);
int vis_fcmpeq16(double /*frs1*/, double /*frs2*/);
int vis_fcmpgt32(double /*frs1*/, double /*frs2*/);
int vis_fcmpeq32(double /*frs1*/, double /*frs2*/);

/* Partitioned multiplication. */
#if 0
double vis_fmul8x16(float /*frs1*/, double /*frs2*/);
#endif
double vis_fmul8x16_dummy(float /*frs1*/, int /*dummy*/, double /*frs2*/);
double vis_fmul8x16au(float /*frs1*/, float /*frs2*/);
double vis_fmul8x16al(float /*frs1*/, float /*frs2*/);
double vis_fmul8sux16(double /*frs1*/, double /*frs2*/);
double vis_fmul8ulx16(double /*frs1*/, double /*frs2*/);
double vis_fmuld8ulx16(float /*frs1*/, float /*frs2*/);
double vis_fmuld8sux16(float /*frs1*/, float /*frs2*/);

/* Partitioned addition & subtraction. */
double vis_fpadd16(double /*frs1*/, double /*frs2*/);
float vis_fpadd16s(float /*frs1*/, float /*frs2*/);
double vis_fpadd32(double /*frs1*/, double /*frs2*/);
float vis_fpadd32s(float /*frs1*/, float /*frs2*/);
double vis_fpsub16(double /*frs1*/, double /*frs2*/);
float vis_fpsub16s(float /*frs1*/, float /*frs2*/);
double vis_fpsub32(double /*frs1*/, double /*frs2*/);
float vis_fpsub32s(float /*frs1*/, float /*frs2*/);

/* Pixel packing & clamping. */
float vis_fpack16(double /*frs2*/);
double vis_fpack32(double /*frs1*/, double /*frs2*/);
float vis_fpackfix(double /*frs2*/);

/* Combined pack ops. */
double vis_fpack16_pair(double /*frs2*/, double /*frs2*/);
double vis_fpackfix_pair(double /*frs2*/, double /*frs2*/);
void vis_st2_fpack16(double, double, double *);
void vis_std_fpack16(double, double, double *);
void vis_st2_fpackfix(double, double, double *);

double vis_fpack16_to_hi(double /*frs1*/, double /*frs2*/);
double vis_fpack16_to_lo(double /*frs1*/, double /*frs2*/);

/* Motion estimation. */
double vis_pdist(double /*frs1*/, double /*frs2*/, double /*frd*/);

/* Channel merging. */
double vis_fpmerge(float /*frs1*/, float /*frs2*/);

/* Pixel expansion. */
double vis_fexpand(float /*frs2*/);
double vis_fexpand_hi(double /*frs2*/);
double vis_fexpand_lo(double /*frs2*/);

/* Bitwise logical operators. */
double vis_fnor(double /*frs1*/, double /*frs2*/);
float vis_fnors(float /*frs1*/, float /*frs2*/);
double vis_fandnot(double /*frs1*/, double /*frs2*/);
float vis_fandnots(float /*frs1*/, float /*frs2*/);
double vis_fnot(double /*frs1*/);
float vis_fnots(float /*frs1*/);
double vis_fxor(double /*frs1*/, double /*frs2*/);
float vis_fxors(float /*frs1*/, float /*frs2*/);
double vis_fnand(double /*frs1*/, double /*frs2*/);
float vis_fnands(float /*frs1*/, float /*frs2*/);
double vis_fand(double /*frs1*/, double /*frs2*/);
float vis_fands(float /*frs1*/, float /*frs2*/);
double vis_fxnor(double /*frs1*/, double /*frs2*/);
float vis_fxnors(float /*frs1*/, float /*frs2*/);
double vis_fsrc(double /*frs1*/);
float vis_fsrcs(float /*frs1*/);
double vis_fornot(double /*frs1*/, double /*frs2*/);
float vis_fornots(float /*frs1*/, float /*frs2*/);
double vis_for(double /*frs1*/, double /*frs2*/);
float vis_fors(float /*frs1*/, float /*frs2*/);
double vis_fzero(void);
float vis_fzeros(void);
double vis_fone(void);
float vis_fones(void);

/* Partial stores. */
void vis_stdfa_ASI_PST8P(double /*frd*/, void * /*rs1*/, int /*rmask*/);
void vis_stdfa_ASI_PST8PL(double /*frd*/, void * /*rs1*/, int /*rmask*/);
void vis_stdfa_ASI_PST8P_int_pair(void * /*rs1*/, void * /*rs2*/,
                                  void * /*rs3*/, int /*rmask*/);
void vis_stdfa_ASI_PST8S(double /*frd*/, void * /*rs1*/, int /*rmask*/);
void vis_stdfa_ASI_PST16P(double /*frd*/, void * /*rs1*/, int /*rmask*/);
void vis_stdfa_ASI_PST16S(double /*frd*/, void * /*rs1*/, int /*rmask*/);
void vis_stdfa_ASI_PST32P(double /*frd*/, void * /*rs1*/, int /*rmask*/);
void vis_stdfa_ASI_PST32S(double /*frd*/, void * /*rs1*/, int /*rmask*/);

/* Byte & short stores. */
void vis_stdfa_ASI_FL8P(double /*frd*/, void * /*rs1*/);
void vis_stdfa_ASI_FL8P_index(double /*frd*/, void * /*rs1*/, long /*index*/);
void vis_stdfa_ASI_FL8S(double /*frd*/, void * /*rs1*/);
void vis_stdfa_ASI_FL16P(double /*frd*/, void * /*rs1*/);
void vis_stdfa_ASI_FL16P_index(double /*frd*/, void * /*rs1*/, long /*index*/);
void vis_stdfa_ASI_FL16S(double /*frd*/, void * /*rs1*/);
void vis_stdfa_ASI_FL8PL(double /*frd*/, void * /*rs1*/);
void vis_stdfa_ASI_FL8SL(double /*frd*/, void * /*rs1*/);
void vis_stdfa_ASI_FL16PL(double /*frd*/, void * /*rs1*/);
void vis_stdfa_ASI_FL16SL(double /*frd*/, void * /*rs1*/);

/* Byte & short loads. */
double vis_lddfa_ASI_FL8P(void * /*rs1*/);
double vis_lddfa_ASI_FL8P_index(void * /*rs1*/, long /*index*/);
double vis_lddfa_ASI_FL8P_hi(void * /*rs1*/, unsigned int /*index*/);
double vis_lddfa_ASI_FL8P_lo(void * /*rs1*/, unsigned int /*index*/);
double vis_lddfa_ASI_FL8S(void * /*rs1*/);
double vis_lddfa_ASI_FL16P(void * /*rs1*/);
double vis_lddfa_ASI_FL16P_index(void * /*rs1*/, long /*index*/);
double vis_lddfa_ASI_FL16S(void * /*rs1*/);
double vis_lddfa_ASI_FL8PL(void * /*rs1*/);
double vis_lddfa_ASI_FL8SL(void * /*rs1*/);
double vis_lddfa_ASI_FL16PL(void * /*rs1*/);
double vis_lddfa_ASI_FL16SL(void * /*rs1*/);

/* Direct write to GSR, read from GSR */
void vis_write_gsr(unsigned int /*GSR*/);
unsigned int vis_read_gsr(void);

/* Voxel texture mapping. */
#if !defined(_NO_LONGLONG)
unsigned long vis_array8(unsigned long long /*rs1*/, int /*rs2*/);
unsigned long vis_array16(unsigned long long /*rs1*/, int /*rs2*/);
unsigned long vis_array32(unsigned long long /*rs1*/, int /*rs2*/);
#endif /* !defined(_NO_LONGLONG) */

/* Register aliasing and type casts. */
float vis_read_hi(double /*frs1*/);
float vis_read_lo(double /*frs1*/);
double vis_write_hi(double /*frs1*/, float /*frs2*/);
double vis_write_lo(double /*frs1*/, float /*frs2*/);
double vis_freg_pair(float /*frs1*/, float /*frs2*/);
float vis_to_float(unsigned int /*value*/);
double vis_to_double(unsigned int /*value1*/, unsigned int /*value2*/);
double vis_to_double_dup(unsigned int /*value*/);
#if !defined(_NO_LONGLONG)
double vis_ll_to_double(unsigned long long /*value*/);
#endif /* !defined(_NO_LONGLONG) */

/* Miscellany (no inlines) */
void vis_error(char * /*fmt*/, int /*a0*/);
void vis_sim_init(void);

/* For better performance */
#define vis_fmul8x16(farg,darg) vis_fmul8x16_dummy((farg),0,(darg))

/* Nicknames for explicit ASI loads and stores. */
#define vis_st_u8      vis_stdfa_ASI_FL8P
#define vis_st_u8_i    vis_stdfa_ASI_FL8P_index
#define vis_st_u8_le   vis_stdfa_ASI_FL8PL
#define vis_st_u16     vis_stdfa_ASI_FL16P
#define vis_st_u16_i   vis_stdfa_ASI_FL16P_index
#define vis_st_u16_le  vis_stdfa_ASI_FL16PL

#define vis_ld_u8      vis_lddfa_ASI_FL8P
#define vis_ld_u8_i    vis_lddfa_ASI_FL8P_index
#define vis_ld_u8_le   vis_lddfa_ASI_FL8PL
#define vis_ld_u16     vis_lddfa_ASI_FL16P
#define vis_ld_u16_i   vis_lddfa_ASI_FL16P_index
#define vis_ld_u16_le  vis_lddfa_ASI_FL16PL

#define vis_pst_8      vis_stdfa_ASI_PST8P
#define vis_pst_16     vis_stdfa_ASI_PST16P
#define vis_pst_32     vis_stdfa_ASI_PST32P

#define vis_st_u8s     vis_stdfa_ASI_FL8S
#define vis_st_u8s_le  vis_stdfa_ASI_FL8SL
#define vis_st_u16s    vis_stdfa_ASI_FL16S
#define vis_st_u16s_le vis_stdfa_ASI_FL16SL

#define vis_ld_u8s     vis_lddfa_ASI_FL8S
#define vis_ld_u8s_le  vis_lddfa_ASI_FL8SL
#define vis_ld_u16s    vis_lddfa_ASI_FL16S
#define vis_ld_u16s_le vis_lddfa_ASI_FL16SL

#define vis_pst_8s     vis_stdfa_ASI_PST8S
#define vis_pst_16s    vis_stdfa_ASI_PST16S
#define vis_pst_32s    vis_stdfa_ASI_PST32S

/* "<" and ">=" may be implemented in terms of ">" and "<=". */
#define vis_fcmplt16(a,b) vis_fcmpgt16((b),(a))
#define vis_fcmplt32(a,b) vis_fcmpgt32((b),(a))
#define vis_fcmpge16(a,b) vis_fcmple16((b),(a))
#define vis_fcmpge32(a,b) vis_fcmple32((b),(a))

#ifdef __cplusplus
} // End of extern "C"
#endif /* __cplusplus */

#endif /* VIS_PROTO_H */
