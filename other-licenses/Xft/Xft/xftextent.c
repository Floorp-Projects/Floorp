/*
 * $XFree86: xc/lib/Xft/xftextent.c,v 1.7 2002/02/15 07:36:11 keithp Exp $
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
#include <string.h>
#include "xftint.h"
   
void
XftGlyphExtents (Display	*dpy,
		 XftFont	*public,
		 FT_UInt	*glyphs,
		 int		nglyphs,
		 XGlyphInfo	*extents)
{
    XftFontInt	*font = (XftFontInt *) public;
    FT_UInt	missing[XFT_NMISSING];
    int		nmissing;
    int		n;
    FT_UInt	*g;
    FT_UInt	glyph;
    XftGlyph	*xftg;
    FcBool	glyphs_loaded;
    int         x, y;
    int         left, right, top, bottom;
    int         overall_left, overall_right;
    int         overall_top, overall_bottom;
    
    g = glyphs;
    n = nglyphs;
    nmissing = 0;
    glyphs_loaded = FcFalse;
    while (n--)
	if (XftFontCheckGlyph (dpy, public, FcFalse, *g++, missing, &nmissing))
	    glyphs_loaded = FcTrue;
    if (nmissing)
	XftFontLoadGlyphs (dpy, public, FcFalse, missing, nmissing);
    g = glyphs;
    n = nglyphs;
    xftg = 0;
    while (n)
    {
	glyph = *g++;
	n--;
	if (glyph < font->num_glyphs &&
	    (xftg = font->glyphs[glyph]))
	    break;
    }
    if (n == 0 && !xftg)
    {
	extents->width = 0;
	extents->height = 0;
	extents->x = 0;
	extents->y = 0;
	extents->yOff = 0;
	extents->xOff = 0;
    }
    else
    {
	x = 0;
	y = 0;
	overall_left = x - xftg->metrics.x;
	overall_top = y - xftg->metrics.y;
	overall_right = overall_left + (int) xftg->metrics.width;
	overall_bottom = overall_top + (int) xftg->metrics.height;
	x += xftg->metrics.xOff;
	y += xftg->metrics.yOff;
	while (n--)
	{
	    glyph = *g++;
	    if (glyph < font->num_glyphs && (xftg = font->glyphs[glyph]))
	    {
		left = x - xftg->metrics.x;
		top = y - xftg->metrics.y;
		right = left + (int) xftg->metrics.width;
		bottom = top + (int) xftg->metrics.height;
		if (left < overall_left)
		    overall_left = left;
		if (top < overall_top)
		    overall_top = top;
		if (right > overall_right)
		    overall_right = right;
		if (bottom > overall_bottom)
		    overall_bottom = bottom;
		x += xftg->metrics.xOff;
		y += xftg->metrics.yOff;
	    }
	}
	extents->x = -overall_left;
	extents->y = -overall_top;
	extents->width = overall_right - overall_left;
	extents->height = overall_bottom - overall_top;
	extents->xOff = x;
	extents->yOff = y;
    }
    if (glyphs_loaded)
	_XftFontManageMemory (dpy, public);
}

#define NUM_LOCAL   1024

void
XftTextExtents8 (Display    *dpy,
		 XftFont    *public,
		 FcChar8    *string, 
		 int	    len,
		 XGlyphInfo *extents)
{
    FT_UInt	    *glyphs, glyphs_local[NUM_LOCAL];
    int		    i;

    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (FT_UInt));
	if (!glyphs)
	{
	    memset (extents, '\0', sizeof (XGlyphInfo));
	    return;
	}
    }
    for (i = 0; i < len; i++)
	glyphs[i] = XftCharIndex (dpy, public, string[i]);
    XftGlyphExtents (dpy, public, glyphs, len, extents);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftTextExtents16 (Display	*dpy,
		  XftFont	*public,
		  FcChar16	*string,
		  int		len,
		  XGlyphInfo	*extents)
{
    FT_UInt	    *glyphs, glyphs_local[NUM_LOCAL];
    int		    i;

    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (FT_UInt));
	if (!glyphs)
	{
	    memset (extents, '\0', sizeof (XGlyphInfo));
	    return;
	}
    }
    for (i = 0; i < len; i++)
	glyphs[i] = XftCharIndex (dpy, public, string[i]);
    XftGlyphExtents (dpy, public, glyphs, len, extents);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftTextExtents32 (Display	*dpy,
		  XftFont	*public,
		  FcChar32	*string,
		  int		len,
		  XGlyphInfo	*extents)
{
    FT_UInt	    *glyphs, glyphs_local[NUM_LOCAL];
    int		    i;

    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (FT_UInt));
	if (!glyphs)
	{
	    memset (extents, '\0', sizeof (XGlyphInfo));
	    return;
	}
    }
    for (i = 0; i < len; i++)
	glyphs[i] = XftCharIndex (dpy, public, string[i]);
    XftGlyphExtents (dpy, public, glyphs, len, extents);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftTextExtentsUtf8 (Display	*dpy,
		    XftFont	*public,
		    FcChar8	*string,
		    int		len,
		    XGlyphInfo	*extents)
{
    FT_UInt	    *glyphs, *glyphs_new, glyphs_local[NUM_LOCAL];
    FcChar32	    ucs4;
    int		    i;
    int		    l;
    int		    size;

    i = 0;
    glyphs = glyphs_local;
    size = NUM_LOCAL;
    while (len && (l = FcUtf8ToUcs4 (string, &ucs4, len)) > 0)
    {
	if (i == size)
	{
	    glyphs_new = malloc (size * 2 * sizeof (FT_UInt));
	    if (!glyphs_new)
	    {
		if (glyphs != glyphs_local)
		    free (glyphs);
		memset (extents, '\0', sizeof (XGlyphInfo));
		return;
	    }
	    memcpy (glyphs_new, glyphs, size * sizeof (FT_UInt));
	    size *= 2;
	    if (glyphs != glyphs_local)
		free (glyphs);
	    glyphs = glyphs_new;
	}
	glyphs[i++] = ucs4;
	string += l;
	len -= l;
    }
    XftGlyphExtents (dpy, public, glyphs, i, extents);
    if (glyphs != glyphs_local)
	free (glyphs);
}
