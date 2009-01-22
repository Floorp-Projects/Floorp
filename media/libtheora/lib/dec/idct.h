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
    last mod: $Id: idct.h 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

/*Inverse DCT transforms.*/
#include <ogg/ogg.h>
#if !defined(_idct_H)
# define _idct_H (1)

void oc_idct8x8_c(ogg_int16_t _y[64],const ogg_int16_t _x[64]);
void oc_idct8x8_10_c(ogg_int16_t _y[64],const ogg_int16_t _x[64]);

#endif
