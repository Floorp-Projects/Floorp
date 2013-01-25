/**
   p_buf.h


   Copyright (C) 2003, Network Resonance, Inc.
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


   ekr@rtfm.com  Tue Nov 25 15:58:37 2003
 */


#ifndef _p_buf_h
#define _p_buf_h

typedef struct nr_p_buf_ {
     UCHAR *data;     /*The pointer to the buffer where the data lives */
     UINT4 size;      /*The size of the buffer */
     UINT4 r_offset;  /*The offset into the buffer where the data starts
                        when reading */
     UINT4 length;    /*The length of the data portion */

     STAILQ_ENTRY(nr_p_buf_) entry;
} nr_p_buf;

typedef STAILQ_HEAD(nr_p_buf_head_,nr_p_buf_) nr_p_buf_head;


typedef struct nr_p_buf_ctx_ {
     int buf_size;

     nr_p_buf_head free_list;
} nr_p_buf_ctx;

int nr_p_buf_ctx_create(int size,nr_p_buf_ctx **ctxp);
int nr_p_buf_ctx_destroy(nr_p_buf_ctx **ctxp);
int nr_p_buf_alloc(nr_p_buf_ctx *ctx,nr_p_buf **bufp);
int nr_p_buf_free(nr_p_buf_ctx *ctx,nr_p_buf *buf);
int nr_p_buf_free_chain(nr_p_buf_ctx *ctx,nr_p_buf_head *chain);
int nr_p_buf_write_to_chain(nr_p_buf_ctx *ctx,
                                       nr_p_buf_head *chain,
                                       UCHAR *data,UINT4 len);

#endif

