/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: packing variable sized words into an octet stream
  last mod: $Id: bitwise.c 14546 2008-02-29 01:14:05Z tterribe $

 ********************************************************************/

/* We're 'MSb' endian; if we write a word but read individual bits,
   then we'll read the msb first */

#include <string.h>
#include <stdlib.h>
#include "bitwise.h"

void theorapackB_reset(oggpack_buffer *b){
  b->ptr=b->buffer;
  b->buffer[0]=0;
  b->endbit=b->endbyte=0;
}

void theorapackB_readinit(oggpack_buffer *b,unsigned char *buf,int bytes){
  memset(b,0,sizeof(*b));
  b->buffer=b->ptr=buf;
  b->storage=bytes;
}

int theorapackB_look1(oggpack_buffer *b,long *_ret){
  if(b->endbyte>=b->storage){
    *_ret=0L;
    return -1;
  }
  *_ret=((b->ptr[0]>>(7-b->endbit))&1);
  return 0;
}

void theorapackB_adv1(oggpack_buffer *b){
  if(++(b->endbit)>7){
    b->endbit=0;
    b->ptr++;
    b->endbyte++;
  }
}

/* bits <= 32 */
int theorapackB_read(oggpack_buffer *b,int bits,long *_ret){
  long ret;
  long m;
  int fail;
  m=32-bits;
  bits+=b->endbit;
  if(b->endbyte+4>=b->storage){
    /* not the main path */
    if(b->endbyte*8+bits>b->storage*8){
      *_ret=0L;
      fail=-1;
      goto overflow;
    }
    /* special case to avoid reading b->ptr[0], which might be past the end of
        the buffer; also skips some useless accounting */
    else if(!bits){
      *_ret=0L;
      return 0;
    }
  }
  ret=b->ptr[0]<<(24+b->endbit);
  if(bits>8){
    ret|=b->ptr[1]<<(16+b->endbit);
    if(bits>16){
      ret|=b->ptr[2]<<(8+b->endbit);
      if(bits>24){
        ret|=b->ptr[3]<<(b->endbit);
        if(bits>32 && b->endbit)
          ret|=b->ptr[4]>>(8-b->endbit);
      }
    }
  }
  *_ret=((ret&0xffffffffUL)>>(m>>1))>>((m+1)>>1);
  fail=0;
overflow:
  b->ptr+=bits/8;
  b->endbyte+=bits/8;
  b->endbit=bits&7;
  return fail;
}

int theorapackB_read1(oggpack_buffer *b,long *_ret){
  int fail;
  if(b->endbyte>=b->storage){
    /* not the main path */
    *_ret=0L;
    fail=-1;
    goto overflow;
  }
  *_ret=(b->ptr[0]>>(7-b->endbit))&1;
  fail=0;
overflow:
  b->endbit++;
  if(b->endbit>7){
    b->endbit=0;
    b->ptr++;
    b->endbyte++;
  }
  return fail;
}

long theorapackB_bytes(oggpack_buffer *b){
  return(b->endbyte+(b->endbit+7)/8);
}

long theorapackB_bits(oggpack_buffer *b){
  return(b->endbyte*8+b->endbit);
}

unsigned char *theorapackB_get_buffer(oggpack_buffer *b){
  return(b->buffer);
}
