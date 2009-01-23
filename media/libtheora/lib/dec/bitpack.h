/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggTheora SOURCE CODE IS (C) COPYRIGHT 1994-2008             *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function: packing variable sized words into an octet stream
  last mod: $Id: bitwise.c 7675 2004-09-01 00:34:39Z xiphmont $

 ********************************************************************/
#if !defined(_bitpack_H)
# define _bitpack_H (1)
# include <ogg/ogg.h>

void theorapackB_readinit(oggpack_buffer *_b,unsigned char *_buf,int _bytes);
int theorapackB_look1(oggpack_buffer *_b,long *_ret);
void theorapackB_adv1(oggpack_buffer *_b);
/*Here we assume 0<=_bits&&_bits<=32.*/
int theorapackB_read(oggpack_buffer *_b,int _bits,long *_ret);
int theorapackB_read1(oggpack_buffer *_b,long *_ret);
long theorapackB_bytes(oggpack_buffer *_b);
long theorapackB_bits(oggpack_buffer *_b);
unsigned char *theorapackB_get_buffer(oggpack_buffer *_b);

/*These two functions are implemented locally in huffdec.c*/
/*Read in bits without advancing the bitptr.
  Here we assume 0<=_bits&&_bits<=32.*/
/*static int theorapackB_look(oggpack_buffer *_b,int _bits,long *_ret);*/
/*static void theorapackB_adv(oggpack_buffer *_b,int _bits);*/


#endif
