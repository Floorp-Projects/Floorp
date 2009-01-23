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
    last mod: $Id: quant.h 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

#if !defined(_quant_H)
# define _quant_H (1)
# include "theora/codec.h"
# include "ocintrin.h"

typedef ogg_uint16_t   oc_quant_table[64];
typedef oc_quant_table oc_quant_tables[64];


/*Maximum scaled quantizer value.*/
#define OC_QUANT_MAX          (1024<<2)


void oc_dequant_tables_init(oc_quant_table *_dequant[2][3],
 int _pp_dc_scale[64],const th_quant_info *_qinfo);

#endif
