/*
 * $XFree86: xc/lib/Xft/xftcolor.c,v 1.2 2001/05/16 17:20:06 keithp Exp $
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

#include "xftint.h"

Bool
XftColorAllocName (Display  *dpy,
		   Visual   *visual,
		   Colormap cmap,
		   char	    *name,
		   XftColor *result)
{
    XColor  screen, exact;
    
    if (!XAllocNamedColor (dpy, cmap, name, &screen, &exact))
    {
	/* XXX stick standard colormap stuff here */
	return False;
    }

    result->pixel = screen.pixel;
    result->color.red = exact.red;
    result->color.green = exact.green;
    result->color.blue = exact.blue;
    result->color.alpha = 0xffff;
    return True;
}

static short
maskbase (unsigned long m)
{
    short        i;

    if (!m)
	return 0;
    i = 0;
    while (!(m&1))
    {
	m>>=1;
	i++;
    }
    return i;
}

static short
masklen (unsigned long m)
{
    unsigned long y;

    y = (m >> 1) &033333333333;
    y = m - y - ((y >>1) & 033333333333);
    return (short) (((y + (y >> 3)) & 030707070707) % 077);
}

Bool
XftColorAllocValue (Display	    *dpy,
		    Visual	    *visual,
		    Colormap	    cmap,
		    XRenderColor    *color,
		    XftColor	    *result)
{
    if (visual->class == TrueColor)
    {
	int	    red_shift, red_len;
	int	    green_shift, green_len;
	int	    blue_shift, blue_len;

	red_shift = maskbase (visual->red_mask);
	red_len = masklen (visual->red_mask);
	green_shift = maskbase (visual->green_mask);
	green_len = masklen (visual->green_mask);
	blue_shift = maskbase (visual->blue_mask);
	blue_len = masklen (visual->blue_mask);
	result->pixel = (((color->red >> (16 - red_len)) << red_shift) |
			 ((color->green >> (16 - green_len)) << green_shift) |
			 ((color->blue >> (16 - blue_len)) << blue_shift));
    }
    else
    {
	XColor  xcolor;
	    
	xcolor.red = color->red;
	xcolor.green = color->green;
	xcolor.blue = color->blue;
	if (!XAllocColor (dpy, cmap, &xcolor))
	    return False;
	result->pixel = xcolor.pixel;
    }
    result->color.red = color->red;
    result->color.green = color->green;
    result->color.blue = color->blue;
    result->color.alpha = color->alpha;
    return True;
}

void
XftColorFree (Display	*dpy,
	      Visual	*visual,
	      Colormap	cmap,
	      XftColor	*color)
{
    if (visual->class != TrueColor)
	XFreeColors (dpy, cmap, &color->pixel, 1, 0);
}
