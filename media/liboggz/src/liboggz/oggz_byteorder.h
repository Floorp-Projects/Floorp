/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __OGGZ_BYTEORDER_H__
#define __OGGZ_BYTEORDER_H__

#include "config.h"

#ifdef _UNUSED_
static  unsigned short
_le_16 (unsigned short s)
{
  unsigned short ret=s;
#ifdef WORDS_BIGENDIAN
  ret = (s>>8) & 0x00ffU;
  ret += (s<<8) & 0xff00U;
#endif
  return ret;
}
#endif /* _UNUSED_ */

static  ogg_uint32_t
_le_32 (ogg_uint32_t i)
{
   ogg_uint32_t ret=i;
#ifdef WORDS_BIGENDIAN
   ret =  (i>>24);
   ret += (i>>8) & 0x0000ff00;
   ret += (i<<8) & 0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

static  unsigned short
_be_16 (unsigned short s)
{
  unsigned short ret=s;
#ifndef WORDS_BIGENDIAN
  ret = (s>>8) & 0x00ffU;
  ret += (s<<8) & 0xff00U;
#endif
  return ret;
}

static  ogg_uint32_t
_be_32 (ogg_uint32_t i)
{
   ogg_uint32_t ret=i;
#ifndef WORDS_BIGENDIAN
   ret =  (i>>24);
   ret += (i>>8) & 0x0000ff00;
   ret += (i<<8) & 0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

static  ogg_int64_t
_le_64 (ogg_int64_t l)
{
  ogg_int64_t ret=l;
  unsigned char *ucptr = (unsigned char *)&ret;
#ifdef WORDS_BIGENDIAN
  unsigned char temp;

  temp = ucptr [0] ;
  ucptr [0] = ucptr [7] ;
  ucptr [7] = temp ;

  temp = ucptr [1] ;
  ucptr [1] = ucptr [6] ;
  ucptr [6] = temp ;

  temp = ucptr [2] ;
  ucptr [2] = ucptr [5] ;
  ucptr [5] = temp ;

  temp = ucptr [3] ;
  ucptr [3] = ucptr [4] ;
  ucptr [4] = temp ;

#endif
  return (*(ogg_int64_t *)ucptr);
}

static ogg_int32_t
int32_be_at (unsigned char *c)
{
  return (c [0] <<  24) | (c [1] <<  16) | (c [2] <<  8) | c [3] ;
}

static unsigned short
int16_be_at (unsigned char *c)
{
  return (c [0] <<  8) | c [1];
}

static ogg_int32_t
int32_le_at (unsigned char *c)
{
  return c [0] | (c [1] <<  8) | (c [2] <<  16) | (c [3] <<  24);
}

static ogg_int64_t
int64_le_at (unsigned char *c)
{
  ogg_uint32_t a = c [0] | (c [1] <<  8) | (c [2] <<  16) | (c [3] <<  24);
  ogg_uint32_t b = c [4] | (c [5] <<  8) | (c [6] <<  16) | (c [7] <<  24);

  return (((ogg_int64_t)b) << 32) | a;
}

#endif /* __OGGZ_BYTEORDER_H__ */
