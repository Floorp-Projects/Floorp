/**
   r_thread.h

   
   Copyright (C) 1999, RTFM, Inc.
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
   

   ekr@rtfm.com  Tue Feb 23 14:58:36 1999
 */


#ifndef _r_thread_h
#define _r_thread_h

typedef void *r_thread;
typedef void *r_rwlock;
typedef void * r_cond;

int r_thread_fork (void (*func)(void *),void *arg,
  r_thread *tid);
int r_thread_destroy (r_thread tid);
int r_thread_yield (void);
int r_thread_exit (void);
int r_thread_wait_last (void);
int r_thread_self (void);

int r_rwlock_create (r_rwlock **lockp);
int r_rwlock_destroy (r_rwlock **lock);
int r_rwlock_lock (r_rwlock *lock,int action);

int r_cond_init (r_cond *cond);  
int r_cond_wait (r_cond cond);
int r_cond_signal (r_cond cond);

#define R_RWLOCK_UNLOCK 0
#define R_RWLOCK_RLOCK 1
#define R_RWLOCK_WLOCK 2

#endif

