/**
   async_timer.h

   
   Copyright (C) 2004, Network Resonance, Inc.
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
   

   ekr@rtfm.com  Sun Feb 22 19:35:24 2004
 */


#ifndef _async_timer_h
#define _async_timer_h


int NR_async_timer_init(void);
int NR_async_timer_set(int delay_ms,NR_async_cb cb,void *cb_arg,char *function,int line,void **handle);
int NR_async_timer_cancel(void *handle);
int NR_async_timer_update_time(struct timeval *tv);
int NR_async_timer_next_timeout(int *delay_ms);
int NR_async_timer_sanity_check_for_cb_deleted(NR_async_cb cb,void *cb_arg);

#define NR_ASYNC_TIMER_SET(d,c,a,hp) NR_async_timer_set(d,c,a,(char *)__FUNCTION__,__LINE__,hp)

#endif

