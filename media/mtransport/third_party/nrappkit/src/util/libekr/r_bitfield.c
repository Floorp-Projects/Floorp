/**
   r_bitfield.c


   Copyright (C) 2002-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


 */

/**
   r_bitfield.c

   Copyright (C) 2001 RTFM, Inc.
   All Rights Reserved.

   ekr@rtfm.com  Wed Oct  3 11:15:23 2001
 */


static char *RCSSTRING __UNUSED__ ="$Id: r_bitfield.c,v 1.2 2006/08/16 19:39:17 adamcain Exp $";

#include <string.h>
#include <r_common.h>
#include <string.h>
#include "r_bitfield.h"

int r_bitfield_create(setp,size)
  r_bitfield **setp;
  UINT4 size;
  {
    r_bitfield *set=0;
    int _status;
    int num_words=size/32+!!(size%32);

    if(!(set=(r_bitfield *)RMALLOC(sizeof(r_bitfield))))
      ABORT(R_NO_MEMORY);

    if(!(set->data=(UINT4 *)RMALLOC(num_words*4)))
      ABORT(R_NO_MEMORY);
    memset(set->data,0,4*num_words);

    set->base=0;
    set->len=num_words;

    *setp=set;

    _status=0;
  abort:
    if(_status){
      r_bitfield_destroy(&set);
    }
    return(_status);
  }

int r_bitfield_destroy(setp)
  r_bitfield **setp;
  {
    r_bitfield *set;

    if(!setp || !*setp)
      return(0);

    set=*setp;

    RFREE(set->data);
    RFREE(set);

    *setp=0;
    return(0);
  }

int r_bitfield_set(set,bit)
  r_bitfield *set;
  int bit;
  {
    int word=(bit-set->base)/32;
    int bbit=(bit-set->base)%32;
    int _status;

    /* Resize? */
    if(word>set->len){
      UINT4 newlen=set->len;
      UINT4 *tmp;

      while(newlen<word)
	newlen*=2;

      if(!(tmp=(UINT4 *)RMALLOC(newlen)))
	ABORT(R_NO_MEMORY);

      memcpy(tmp,set->data,set->len*4);
      memset(tmp+set->len*4,0,(newlen-set->len)*4);

      RFREE(set->data);
      set->data=tmp;
    }

    set->data[word]|=1<<bbit;

    _status=0;
  abort:
    return(_status);
  }

int r_bitfield_isset(set,bit)
  r_bitfield *set;
  int bit;
  {
    int word=(bit-set->base)/32;
    int bbit=(bit-set->base)%32;
    int _status;

    if(bit<set->base)
      return(0);

    /* Resize? */
    if(word>set->len)
      return(0);

    return(set->data[word]&(1<<bbit));

    _status=0;
    return(_status);
  }
