/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: x86int.h 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

#if !defined(_x86_x86int_vc_H)
# define _x86_x86int_vc_H (1)
# include "../../internal.h"

void oc_state_vtable_init_x86(oc_theora_state *_state);

void oc_frag_recon_intra_mmx(unsigned char *_dst,int _dst_ystride,
 const ogg_int16_t *_residue);

void oc_frag_recon_inter_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src,int _src_ystride,const ogg_int16_t *_residue);

void oc_frag_recon_inter2_mmx(unsigned char *_dst,int _dst_ystride,
 const unsigned char *_src1,int _src1_ystride,const unsigned char *_src2,
 int _src2_ystride,const ogg_int16_t *_residue);

void oc_state_frag_copy_mmx(const oc_theora_state *_state,const int *_fragis,
 int _nfragis,int _dst_frame,int _src_frame,int _pli);

void oc_restore_fpu_mmx(void);

void oc_state_frag_recon_mmx(oc_theora_state *_state,const oc_fragment *_frag,                                               
 int _pli,ogg_int16_t _dct_coeffs[128],int _last_zzi,int _ncoefs,                                                             
 ogg_uint16_t _dc_iquant,const ogg_uint16_t _ac_iquant[64]);

void oc_idct8x8_mmx(ogg_int16_t _y[64]);
void oc_idct8x8_10_mmx(ogg_int16_t _y[64]);

void oc_state_loop_filter_frag_rows_mmx(oc_theora_state *_state,int *_bv,                                                    
  int _refi,int _pli,int _fragy0,int _fragy_end);

#endif
