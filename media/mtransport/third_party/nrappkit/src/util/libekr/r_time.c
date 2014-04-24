/**
   r_time.c


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
   r_time.c


   Copyright (C) 1999-2000 RTFM, Inc.
   All Rights Reserved

   This package is a SSLv3/TLS protocol analyzer written by Eric Rescorla
   <ekr@rtfm.com> and licensed by RTFM, Inc.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. All advertising materials mentioning features or use of this software
      must display the following acknowledgement:

      This product includes software developed by Eric Rescorla for
      RTFM, Inc.

   4. Neither the name of RTFM, Inc. nor the name of Eric Rescorla may be
      used to endorse or promote products derived from this
      software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY ERIC RESCORLA AND RTFM, INC. ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY SUCH DAMAGE.

   $Id: r_time.c,v 1.5 2008/11/26 03:22:02 adamcain Exp $

   ekr@rtfm.com  Thu Mar  4 08:43:46 1999
 */


static char *RCSSTRING __UNUSED__ ="$Id: r_time.c,v 1.5 2008/11/26 03:22:02 adamcain Exp $";

#include <r_common.h>
#include <r_time.h>

/*Note that t1 must be > t0 */
int r_timeval_diff(t1,t0,diff)
  struct timeval *t1;
  struct timeval *t0;
  struct timeval *diff;
  {
    long d;

    if(t0->tv_sec > t1->tv_sec)
      ERETURN(R_BAD_ARGS);
    if((t0->tv_sec == t1->tv_sec) && (t0->tv_usec > t1->tv_usec))
      ERETURN(R_BAD_ARGS);

    /*Easy case*/
    if(t0->tv_usec <= t1->tv_usec){
      diff->tv_sec=t1->tv_sec - t0->tv_sec;
      diff->tv_usec=t1->tv_usec - t0->tv_usec;
      return(0);
    }

    /*Hard case*/
    d=t0->tv_usec - t1->tv_usec;
    if(t1->tv_sec < (t0->tv_sec + 1))
      ERETURN(R_BAD_ARGS);
    diff->tv_sec=t1->tv_sec - (t0->tv_sec + 1);
    diff->tv_usec=1000000 - d;

    return(0);
  }

int r_timeval_add(t1,t2,sum)
  struct timeval *t1;
  struct timeval *t2;
  struct timeval *sum;
  {
    long tv_sec,tv_usec,d;

    tv_sec=t1->tv_sec + t2->tv_sec;

    d=t1->tv_usec + t2->tv_usec;
    if(d>1000000){
      tv_sec++;
      tv_usec=d-1000000;
    }
    else{
      tv_usec=d;
    }

    sum->tv_sec=tv_sec;
    sum->tv_usec=tv_usec;

    return(0);
  }

int r_timeval_cmp(t1,t2)
  struct timeval *t1;
  struct timeval *t2;
  {
    if(t1->tv_sec>t2->tv_sec)
      return(1);
    if(t1->tv_sec<t2->tv_sec)
      return(-1);
    if(t1->tv_usec>t2->tv_usec)
      return(1);
    if(t1->tv_usec<t2->tv_usec)
      return(-1);
    return(0);
  }


UINT8 r_timeval2int(tv)
  struct timeval *tv;
  {
    UINT8 r=0;

    r=(tv->tv_sec);
    r*=1000000;
    r+=tv->tv_usec;

    return r;
  }

int r_int2timeval(UINT8 t,struct timeval *tv)
  {
    tv->tv_sec=t/1000000;
    tv->tv_usec=t%1000000;

    return(0);
  }

UINT8 r_gettimeint()
  {
    struct timeval tv;

    gettimeofday(&tv,0);

    return r_timeval2int(&tv);
  }

/* t1-t0 in microseconds */
int r_timeval_diff_usec(struct timeval *t1, struct timeval *t0, INT8 *diff)
  {
    int r,_status;
    int sign;
    struct timeval tmp;

    sign = 1;
    if (r=r_timeval_diff(t1, t0, &tmp)) {
        if (r == R_BAD_ARGS) {
            sign = -1;
            if (r=r_timeval_diff(t0, t1, &tmp))
                ABORT(r);
        }
    }

    /* 1 second = 1000 milliseconds
     * 1 milliseconds = 1000 microseconds */

    *diff = ((tmp.tv_sec * (1000*1000)) + tmp.tv_usec) * sign;

    _status = 0;
  abort:
    return(_status);
  }

/* t1-t0 in milliseconds */
int r_timeval_diff_ms(struct timeval *t1, struct timeval *t0, INT8 *diff)
  {
    int r,_status;
    int sign;
    struct timeval tmp;

    sign = 1;
    if (r=r_timeval_diff(t1, t0, &tmp)) {
        if (r == R_BAD_ARGS) {
            sign = -1;
            if (r=r_timeval_diff(t0, t1, &tmp))
                ABORT(r);
        }
    }

    /* 1 second = 1000 milliseconds
     * 1 milliseconds = 1000 microseconds */

    *diff = ((tmp.tv_sec * 1000) + (tmp.tv_usec / 1000)) * sign;

    _status = 0;
  abort:
    return(_status);
  }

