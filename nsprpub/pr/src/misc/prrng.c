/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
