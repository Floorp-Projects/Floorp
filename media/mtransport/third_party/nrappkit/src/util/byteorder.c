/**
   byteorder.c


   Copyright (C) 2007, Network Resonance, Inc.
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


   briank@networkresonance.com  Wed May 16 16:46:00 PDT 2007
 */


static char *RCSSTRING __UNUSED__ ="$Id: byteorder.c,v 1.2 2007/06/26 22:37:55 adamcain Exp $";

#include "nr_common.h"
#ifndef WIN32
#include <arpa/inet.h>
#endif
#include "r_types.h"
#include "byteorder.h"

#define IS_BIG_ENDIAN (htonl(0x1) == 0x1)

#define BYTE(n,i)   (((UCHAR*)&(n))[(i)])
#define SWAP(n,x,y) tmp=BYTE((n),(x)), \
                    BYTE((n),(x))=BYTE((n),(y)), \
                    BYTE((n),(y))=tmp

UINT8
htonll(UINT8 hostlonglong)
{
    UINT8 netlonglong = hostlonglong;
    UCHAR tmp;

    if (!IS_BIG_ENDIAN) {
        SWAP(netlonglong, 0, 7);
        SWAP(netlonglong, 1, 6);
        SWAP(netlonglong, 2, 5);
        SWAP(netlonglong, 3, 4);
    }

    return netlonglong;
}

UINT8
ntohll(UINT8 netlonglong)
{
    return htonll(netlonglong);
}

