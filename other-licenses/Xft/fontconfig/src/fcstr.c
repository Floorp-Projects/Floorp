/*
 * $XFree86: xc/lib/fontconfig/src/fcstr.c,v 1.3 2002/02/18 22:29:28 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "fcint.h"

FcChar8 *
FcStrCopy (const FcChar8 *s)
{
    FcChar8	*r;

    if (!s)
	return 0;
    r = (FcChar8 *) malloc (strlen ((char *) s) + 1);
    if (!r)
	return 0;
    FcMemAlloc (FC_MEM_STRING, strlen ((char *) s) + 1);
    strcpy ((char *) r, (char *) s);
    return r;
}

FcChar8 *
FcStrPlus (const FcChar8 *s1, const FcChar8 *s2)
{
    int	    l = strlen ((char *)s1) + strlen ((char *) s2) + 1;
    FcChar8 *s = malloc (l);

    if (!s)
	return 0;
    FcMemAlloc (FC_MEM_STRING, l);
    strcpy ((char *) s, (char *) s1);
    strcat ((char *) s, (char *) s2);
    return s;
}

void
FcStrFree (FcChar8 *s)
{
    FcMemFree (FC_MEM_STRING, strlen ((char *) s) + 1);
    free (s);
}

int
FcStrCmpIgnoreCase (const FcChar8 *s1, const FcChar8 *s2)
{
    FcChar8 c1, c2;
    
    for (;;) 
    {
	c1 = *s1++;
	c2 = *s2++;
	if (!c1 || !c2)
	    break;
	c1 = FcToLower (c1);
	c2 = FcToLower (c2);
	if (c1 != c2)
	    break;
    }
    return (int) c2 - (int) c1;
}

int
FcUtf8ToUcs4 (FcChar8   *src_orig,
	      FcChar32  *dst,
	      int	len)
{
    FcChar8	*src = src_orig;
    FcChar8	s;
    int		extra;
    FcChar32	result;

    if (len == 0)
	return 0;
    
    s = *src++;
    len--;
    
    if (!(s & 0x80))
    {
	result = s;
	extra = 0;
    } 
    else if (!(s & 0x40))
    {
	return -1;
    }
    else if (!(s & 0x20))
    {
	result = s & 0x1f;
	extra = 1;
    }
    else if (!(s & 0x10))
    {
	result = s & 0xf;
	extra = 2;
    }
    else if (!(s & 0x08))
    {
	result = s & 0x07;
	extra = 3;
    }
    else if (!(s & 0x04))
    {
	result = s & 0x03;
	extra = 4;
    }
    else if ( ! (s & 0x02))
    {
	result = s & 0x01;
	extra = 5;
    }
    else
    {
	return -1;
    }
    if (extra > len)
	return -1;
    
    while (extra--)
    {
	result <<= 6;
	s = *src++;
	
	if ((s & 0xc0) != 0x80)
	    return -1;
	
	result |= s & 0x3f;
    }
    *dst = result;
    return src - src_orig;
}

FcBool
FcUtf8Len (FcChar8	*string,
	    int		len,
	    int		*nchar,
	    int		*wchar)
{
    int		n;
    int		clen;
    FcChar32	c;
    FcChar32	max;
    
    n = 0;
    max = 0;
    while (len)
    {
	clen = FcUtf8ToUcs4 (string, &c, len);
	if (clen <= 0)	/* malformed UTF8 string */
	    return FcFalse;
	if (c > max)
	    max = c;
	string += clen;
	len -= clen;
	n++;
    }
    *nchar = n;
    if (max >= 0x10000)
	*wchar = 4;
    else if (max > 0x100)
	*wchar = 2;
    else
	*wchar = 1;
    return FcTrue;
}

void
FcStrBufInit (FcStrBuf *buf, FcChar8 *init, int size)
{
    buf->buf = init;
    buf->allocated = FcFalse;
    buf->failed = FcFalse;
    buf->len = 0;
    buf->size = size;
}

void
FcStrBufDestroy (FcStrBuf *buf)
{
    if (buf->allocated)
    {
	free (buf->buf);
	FcStrBufInit (buf, 0, 0);
    }
}

FcChar8 *
FcStrBufDone (FcStrBuf *buf)
{
    FcChar8 *ret;

    ret = malloc (buf->len + 1);
    if (ret)
    {
	memcpy (ret, buf->buf, buf->len);
	ret[buf->len] = '\0';
    }
    FcStrBufDestroy (buf);
    return ret;
}

FcBool
FcStrBufChar (FcStrBuf *buf, FcChar8 c)
{
    if (buf->len == buf->size)
    {
	FcChar8	    *new;
	int	    size;

	if (buf->allocated)
	{
	    size = buf->size * 2;
	    new = realloc (buf->buf, size);
	}
	else
	{
	    size = buf->size + 1024;
	    new = malloc (size);
	    if (new)
	    {
		buf->allocated = FcTrue;
		memcpy (new, buf->buf, buf->len);
	    }
	}
	if (!new)
	{
	    buf->failed = FcTrue;
	    return FcFalse;
	}
	buf->size = size;
	buf->buf = new;
    }
    buf->buf[buf->len++] = c;
    return FcTrue;
}

FcBool
FcStrBufString (FcStrBuf *buf, const FcChar8 *s)
{
    FcChar8 c;
    while ((c = *s++))
	if (!FcStrBufChar (buf, c))
	    return FcFalse;
    return FcTrue;
}

FcBool
FcStrBufData (FcStrBuf *buf, const FcChar8 *s, int len)
{
    while (len-- > 0)
	if (!FcStrBufChar (buf, *s++))
	    return FcFalse;
    return FcTrue;
}

