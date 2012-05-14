/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

/*
 * We were not including <string.h> in optimized builds.  On AIX this
 * caused libnspr4.so to export memcpy and some binaries linked with
 * libnspr4.so resolved their memcpy references with libnspr4.so.  To
 * be backward compatible with old libnspr4.so binaries, we do not
 * include <string.h> in optimized builds for AIX.  (bug 200561)
 */
#if !(defined(AIX) && !defined(DEBUG))
#include <string.h>
#endif

PRSize _pr_CopyLowBits( 
    void *dst, 
    PRSize dstlen, 
    void *src, 
    PRSize srclen )
{
    if (srclen <= dstlen) {
    	memcpy(dst, src, srclen);
	    return srclen;
    }
#if defined IS_BIG_ENDIAN
    memcpy(dst, (char*)src + (srclen - dstlen), dstlen);
#else
    memcpy(dst, src, dstlen);
#endif
    return dstlen;
}    

PR_IMPLEMENT(PRSize) PR_GetRandomNoise( 
    void    *buf,
    PRSize  size
)
{
    return( _PR_MD_GET_RANDOM_NOISE( buf, size ));
} /* end PR_GetRandomNoise() */
/* end prrng.c */
