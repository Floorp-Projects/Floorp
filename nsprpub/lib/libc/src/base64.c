/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plbase64.h"
#include "prlog.h" /* For PR_NOT_REACHED */
#include "prmem.h" /* for malloc / PR_MALLOC */

#include <string.h> /* for strlen */

static unsigned char *base = (unsigned char *)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
encode3to4
(
    const unsigned char    *src,
    unsigned char          *dest
)
{
    PRUint32 b32 = (PRUint32)0;
    PRIntn i, j = 18;

    for( i = 0; i < 3; i++ )
    {
        b32 <<= 8;
        b32 |= (PRUint32)src[i];
    }

    for( i = 0; i < 4; i++ )
    {
        dest[i] = base[ (PRUint32)((b32>>j) & 0x3F) ];
        j -= 6;
    }

    return;
}

static void
encode2to4
(
    const unsigned char    *src,
    unsigned char          *dest
)
{
    dest[0] = base[ (PRUint32)((src[0]>>2) & 0x3F) ];
    dest[1] = base[ (PRUint32)(((src[0] & 0x03) << 4) | ((src[1] >> 4) & 0x0F)) ];
    dest[2] = base[ (PRUint32)((src[1] & 0x0F) << 2) ];
    dest[3] = (unsigned char)'=';
    return;
}

static void
encode1to4
(
    const unsigned char    *src,
    unsigned char          *dest
)
{
    dest[0] = base[ (PRUint32)((src[0]>>2) & 0x3F) ];
    dest[1] = base[ (PRUint32)((src[0] & 0x03) << 4) ];
    dest[2] = (unsigned char)'=';
    dest[3] = (unsigned char)'=';
    return;
}

static void
encode
(
    const unsigned char    *src,
    PRUint32                srclen,
    unsigned char          *dest
)
{
    while( srclen >= 3 )
    {
        encode3to4(src, dest);
        src += 3;
        dest += 4;
        srclen -= 3;
    }

    switch( srclen )
    {
        case 2:
            encode2to4(src, dest);
            break;
        case 1:
            encode1to4(src, dest);
            break;
        case 0:
            break;
        default:
            PR_NOT_REACHED("coding error");
    }

    return;
}

/*
 * PL_Base64Encode
 *
 * If the destination argument is NULL, a return buffer is 
 * allocated, and the data therein will be null-terminated.  
 * If the destination argument is not NULL, it is assumed to
 * be of sufficient size, and the contents will not be null-
 * terminated by this routine.
 *
 * Returns null if the allocation fails.
 */

PR_IMPLEMENT(char *)
PL_Base64Encode
(
    const char *src,
    PRUint32    srclen,
    char       *dest
)
{
    if( 0 == srclen )
    {
        size_t len = strlen(src);
        srclen = len;
        /* Detect truncation. */
        if( srclen != len )
        {
            return (char *)0;
        }
    }

    if( (char *)0 == dest )
    {
        PRUint32 destlen;
        /* Ensure all PRUint32 values stay within range. */
        if( srclen > (PR_UINT32_MAX/4) * 3 )
        {
            return (char *)0;
        }
        destlen = ((srclen + 2)/3) * 4;
        dest = (char *)PR_MALLOC(destlen + 1);
        if( (char *)0 == dest )
        {
            return (char *)0;
        }
        dest[ destlen ] = (char)0; /* null terminate */
    }

    encode((const unsigned char *)src, srclen, (unsigned char *)dest);
    return dest;
}

static PRInt32
codetovalue
(
    unsigned char c
)
{
    if( (c >= (unsigned char)'A') && (c <= (unsigned char)'Z') )
    {
        return (PRInt32)(c - (unsigned char)'A');
    }
    else if( (c >= (unsigned char)'a') && (c <= (unsigned char)'z') )
    {
        return ((PRInt32)(c - (unsigned char)'a') +26);
    }
    else if( (c >= (unsigned char)'0') && (c <= (unsigned char)'9') )
    {
        return ((PRInt32)(c - (unsigned char)'0') +52);
    }
    else if( (unsigned char)'+' == c )
    {
        return (PRInt32)62;
    }
    else if( (unsigned char)'/' == c )
    {
        return (PRInt32)63;
    }
    else
    {
        return -1;
    }
}

static PRStatus
decode4to3
(
    const unsigned char    *src,
    unsigned char          *dest
)
{
    PRUint32 b32 = (PRUint32)0;
    PRInt32 bits;
    PRIntn i;

    for( i = 0; i < 4; i++ )
    {
        bits = codetovalue(src[i]);
        if( bits < 0 )
        {
            return PR_FAILURE;
        }

        b32 <<= 6;
        b32 |= bits;
    }

    dest[0] = (unsigned char)((b32 >> 16) & 0xFF);
    dest[1] = (unsigned char)((b32 >>  8) & 0xFF);
    dest[2] = (unsigned char)((b32      ) & 0xFF);

    return PR_SUCCESS;
}

static PRStatus
decode3to2
(
    const unsigned char    *src,
    unsigned char          *dest
)
{
    PRUint32 b32 = (PRUint32)0;
    PRInt32 bits;
    PRUint32 ubits;

    bits = codetovalue(src[0]);
    if( bits < 0 )
    {
        return PR_FAILURE;
    }

    b32 = (PRUint32)bits;
    b32 <<= 6;

    bits = codetovalue(src[1]);
    if( bits < 0 )
    {
        return PR_FAILURE;
    }

    b32 |= (PRUint32)bits;
    b32 <<= 4;

    bits = codetovalue(src[2]);
    if( bits < 0 )
    {
        return PR_FAILURE;
    }

    ubits = (PRUint32)bits;
    b32 |= (ubits >> 2);

    dest[0] = (unsigned char)((b32 >> 8) & 0xFF);
    dest[1] = (unsigned char)((b32     ) & 0xFF);

    return PR_SUCCESS;
}

static PRStatus
decode2to1
(
    const unsigned char    *src,
    unsigned char          *dest
)
{
    PRUint32 b32;
    PRUint32 ubits;
    PRInt32 bits;

    bits = codetovalue(src[0]);
    if( bits < 0 )
    {
        return PR_FAILURE;
    }

    ubits = (PRUint32)bits;
    b32 = (ubits << 2);

    bits = codetovalue(src[1]);
    if( bits < 0 )
    {
        return PR_FAILURE;
    }

    ubits = (PRUint32)bits;
    b32 |= (ubits >> 4);

    dest[0] = (unsigned char)b32;

    return PR_SUCCESS;
}

static PRStatus
decode
(
    const unsigned char    *src,
    PRUint32                srclen,
    unsigned char          *dest
)
{
    PRStatus rv;

    while( srclen >= 4 )
    {
        rv = decode4to3(src, dest);
        if( PR_SUCCESS != rv )
        {
            return PR_FAILURE;
        }

        src += 4;
        dest += 3;
        srclen -= 4;
    }

    switch( srclen )
    {
        case 3:
            rv = decode3to2(src, dest);
            break;
        case 2:
            rv = decode2to1(src, dest);
            break;
        case 1:
            rv = PR_FAILURE;
            break;
        case 0:
            rv = PR_SUCCESS;
            break;
        default:
            PR_NOT_REACHED("coding error");
    }

    return rv;
}

/*
 * PL_Base64Decode
 *
 * If the destination argument is NULL, a return buffer is
 * allocated and the data therein will be null-terminated.
 * If the destination argument is not null, it is assumed
 * to be of sufficient size, and the data will not be null-
 * terminated by this routine.
 * 
 * Returns null if the allocation fails, or if the source string is 
 * not well-formed.
 */

PR_IMPLEMENT(char *)
PL_Base64Decode
(
    const char *src,
    PRUint32    srclen,
    char       *dest
)
{
    PRStatus status;
    PRBool allocated = PR_FALSE;

    if( (char *)0 == src )
    {
        return (char *)0;
    }

    if( 0 == srclen )
    {
        size_t len = strlen(src);
        srclen = len;
        /* Detect truncation. */
        if( srclen != len )
        {
            return (char *)0;
        }
    }

    if( srclen && (0 == (srclen & 3)) )
    {
        if( (char)'=' == src[ srclen-1 ] )
        {
            if( (char)'=' == src[ srclen-2 ] )
            {
                srclen -= 2;
            }
            else
            {
                srclen -= 1;
            }
        }
    }

    if( (char *)0 == dest )
    {
        /* The following computes ((srclen * 3) / 4) without overflow. */
        PRUint32 destlen = (srclen / 4) * 3 + ((srclen % 4) * 3) / 4;
        dest = (char *)PR_MALLOC(destlen + 1);
        if( (char *)0 == dest )
        {
            return (char *)0;
        }
        dest[ destlen ] = (char)0; /* null terminate */
        allocated = PR_TRUE;
    }

    status = decode((const unsigned char *)src, srclen, (unsigned char *)dest);
    if( PR_SUCCESS != status )
    {
        if( PR_TRUE == allocated )
        {
            PR_DELETE(dest);
        }

        return (char *)0;
    }

    return dest;
}
