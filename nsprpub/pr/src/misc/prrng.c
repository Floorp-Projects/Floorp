/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "primpl.h"


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
