/*
 * $XFree86: xc/lib/Xft/xftfont.c,v 1.9 2002/02/15 07:36:11 keithp Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include "xftint.h"

FcPattern *
XftFontMatch (Display *dpy, int screen, FcPattern *pattern, FcResult *result)
{
    FcPattern	*new;
    FcPattern	*match;

    if (!XftInit (0))
	return 0;
    
    new = FcPatternDuplicate (pattern);
    if (!new)
	return 0;

    if (XftDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch pattern ");
	FcPatternPrint (new);
    }
    FcConfigSubstitute (0, new, FcMatchPattern);
    if (XftDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch after FcConfig substitutions ");
	FcPatternPrint (new);
    }
    XftDefaultSubstitute (dpy, screen, new);
    if (XftDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch after X resource substitutions ");
	FcPatternPrint (new);
    }
    
    match = FcFontMatch (0, new, result);
    if (XftDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch result ");
	FcPatternPrint (match);
    }
    FcPatternDestroy (new);
    return match;
}

XftFont *
XftFontOpen (Display *dpy, int screen, ...)
{
    va_list	    va;
    FcPattern	    *pat;
    FcPattern	    *match;
    FcResult	    result;
    XftFont	    *font;

    va_start (va, screen);
    pat = FcPatternVaBuild (0, va);
    va_end (va);
    if (!pat)
    {
	if (XftDebug () & XFT_DBG_OPEN)
	    printf ("XftFontOpen: Invalid pattern argument\n");
	return 0;
    }
    match = XftFontMatch (dpy, screen, pat, &result);
    if (XftDebug () & XFT_DBG_OPEN)
    {
	printf ("Pattern ");
	FcPatternPrint (pat);
	if (match)
	{
	    printf ("Match ");
	    FcPatternPrint (match);
	}
	else
	    printf ("No Match\n");
    }
    FcPatternDestroy (pat);
    if (!match)
	return 0;
    
    font = XftFontOpenPattern (dpy, match);
    if (!font)
    {
	if (XftDebug () & XFT_DBG_OPEN)
	    printf ("No Font\n");
	FcPatternDestroy (match);
    }

    return font;
}

XftFont *
XftFontOpenName (Display *dpy, int screen, const char *name)
{
    FcPattern	    *pat;
    FcPattern	    *match;
    FcResult	    result;
    XftFont	    *font;

    pat = FcNameParse ((FcChar8 *) name);
    if (XftDebug () & XFT_DBG_OPEN)
    {
	printf ("XftFontOpenName \"%s\": ", name);
	if (pat)
	    FcPatternPrint (pat);
	else
	    printf ("Invalid name\n");
    }
			     
    if (!pat)
	return 0;
    match = XftFontMatch (dpy, screen, pat, &result);
    if (XftDebug () & XFT_DBG_OPEN)
    {
	if (match)
	{
	    printf ("Match ");
	    FcPatternPrint (match);
	}
	else
	    printf ("No Match\n");
    }
    FcPatternDestroy (pat);
    if (!match)
	return 0;
    
    font = XftFontOpenPattern (dpy, match);
    if (!font)
	FcPatternDestroy (match);
    
    return font;
}

XftFont *
XftFontOpenXlfd (Display *dpy, int screen, const char *xlfd)
{
    FcPattern	    *pat;
    FcPattern	    *match;
    FcResult	    result;
    XftFont	    *font;

    pat = XftXlfdParse (xlfd, FcFalse, FcFalse);
    if (XftDebug () & XFT_DBG_OPEN)
    {
	printf ("XftFontOpenXlfd \"%s\": ", xlfd);
	if (pat)
	    printf ("Invalid xlfd\n");
	else
	    FcPatternPrint (pat);
    }
			     
    if (!pat)
	return 0;
    match = XftFontMatch (dpy, screen, pat, &result);
    if (XftDebug () & XFT_DBG_OPEN)
    {
	if (match)
	{
	    printf ("Match ");
	    FcPatternPrint (match);
	}
	else
	    printf ("No Match\n");
    }
    FcPatternDestroy (pat);
    if (!match)
	return 0;
    
    font = XftFontOpenPattern (dpy, match);
    if (!font)
	FcPatternDestroy (match);
    
    return font;
}

