/*
 * $XFree86: xc/lib/Xft/xftdraw.c,v 1.17 2002/02/19 07:56:29 keithp Exp $
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
#include <string.h>
#include "xftint.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/*
 * Ok, this is a pain.  To share source pictures across multiple destinations,
 * the screen for each drawable must be discovered.
 */

static int
_XftDrawScreen (Display *dpy, Drawable drawable, Visual *visual)
{
    int		    s;
    Window	    root;
    int		    x, y;
    unsigned int    width, height, borderWidth, depth;
    /* Special case the most common environment */
    if (ScreenCount (dpy) == 1)
	return 0;
    /*
     * If we've got a visual, look for the screen that points at it.
     * This requires no round trip.
     */
    if (visual)
    {
	for (s = 0; s < ScreenCount (dpy); s++)
	{
	    XVisualInfo	template, *ret;
	    int		nret;

	    template.visualid = visual->visualid;
	    template.screen = s;
	    ret = XGetVisualInfo (dpy, VisualIDMask|VisualScreenMask,
				  &template, &nret);
	    if (ret)
	    {
		XFree (ret);
		return s;
	    }
	}
    }
    /*
     * Otherwise, as the server for the drawable geometry and find
     * the screen from the root window.
     * This takes a round trip.
     */
    if (XGetGeometry (dpy, drawable, &root, &x, &y, &width, &height,
		      &borderWidth, &depth))
    {
	for (s = 0; s < ScreenCount (dpy); s++)
	{
	    if (RootWindow (dpy, s) == root)
		return s;
	}
    }
    /*
     * Make a guess -- it's probably wrong, but then the app probably
     * handed us a bogus drawable in this case
     */
    return 0;
}

unsigned int
XftDrawDepth (XftDraw *draw)
{
    if (!draw->depth)
    {
	Window		    root;
	int		    x, y;
	unsigned int	    width, height, borderWidth, depth;
	if (XGetGeometry (draw->dpy, draw->drawable, 
			  &root, &x, &y, &width, &height,
			  &borderWidth, &depth))
	    draw->depth = depth;
    }
    return draw->depth;
}

unsigned int
XftDrawBitsPerPixel (XftDraw	*draw)
{
    if (!draw->bits_per_pixel)
    {
	XPixmapFormatValues *formats;
	int		    nformats;
	unsigned int	    depth;
	
	if ((depth = XftDrawDepth (draw)) &&
	    (formats = XListPixmapFormats (draw->dpy, &nformats)))
	{
	    int	i;

	    for (i = 0; i < nformats; i++)
	    {
		if (formats[i].depth == depth)
		{
		    draw->bits_per_pixel = formats[i].bits_per_pixel;
		    break;
		}
	    }
	    XFree (formats);
	}
    }
    return draw->bits_per_pixel;
}

XftDraw *
XftDrawCreate (Display   *dpy,
	       Drawable  drawable,
	       Visual    *visual,
	       Colormap  colormap)
{
    XftDraw	*draw;

    draw = (XftDraw *) malloc (sizeof (XftDraw));
    if (!draw)
	return 0;
    
    draw->dpy = dpy;
    draw->drawable = drawable;
    draw->screen = _XftDrawScreen (dpy, drawable, visual);
    draw->depth = 0;		/* don't find out unless we need to know */
    draw->bits_per_pixel = 0;	/* don't find out unless we need to know */
    draw->visual = visual;
    draw->colormap = colormap;
    draw->render.pict = 0;
    draw->core.gc = 0;
    draw->core.use_pixmap = 0;
    draw->clip = 0;
    XftMemAlloc (XFT_MEM_DRAW, sizeof (XftDraw));
    return draw;
}

XftDraw *
XftDrawCreateBitmap (Display	*dpy,
		     Pixmap	bitmap)
{
    XftDraw	*draw;

    draw = (XftDraw *) malloc (sizeof (XftDraw));
    if (!draw)
	return 0;
    draw->dpy = dpy;
    draw->drawable = (Drawable) bitmap;
    draw->screen = _XftDrawScreen (dpy, bitmap, 0);
    draw->depth = 1;
    draw->bits_per_pixel = 1;
    draw->visual = 0;
    draw->colormap = 0;
    draw->render.pict = 0;
    draw->core.gc = 0;
    draw->clip = 0;
    XftMemAlloc (XFT_MEM_DRAW, sizeof (XftDraw));
    return draw;
}

XftDraw *
XftDrawCreateAlpha (Display *dpy,
		    Pixmap  pixmap,
		    int	    depth)
{
    XftDraw	*draw;

    draw = (XftDraw *) malloc (sizeof (XftDraw));
    if (!draw)
	return 0;
    draw->dpy = dpy;
    draw->drawable = (Drawable) pixmap;
    draw->screen = _XftDrawScreen (dpy, pixmap, 0);
    draw->depth = depth;
    draw->bits_per_pixel = 0;	/* don't find out until we need it */
    draw->visual = 0;
    draw->colormap = 0;
    draw->render.pict = 0;
    draw->core.gc = 0;
    draw->clip = 0;
    XftMemAlloc (XFT_MEM_DRAW, sizeof (XftDraw));
    return draw;
}

static XRenderPictFormat *
_XftDrawFormat (XftDraw	*draw)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (draw->dpy);

    if (!info->hasRender)
	return 0;

    if (draw->visual == 0)
    {
	XRenderPictFormat   pf;

	pf.type = PictTypeDirect;
	pf.depth = XftDrawDepth (draw);
	pf.direct.alpha = 0;
	pf.direct.alphaMask = (1 << pf.depth) - 1;
	return XRenderFindFormat (draw->dpy,
				  (PictFormatType|
				   PictFormatDepth|
				   PictFormatAlpha|
				   PictFormatAlphaMask),
				  &pf,
				  0);
    }
    else
	return XRenderFindVisualFormat (draw->dpy, draw->visual);
}

void
XftDrawChange (XftDraw	*draw,
	       Drawable	drawable)
{
    draw->drawable = drawable;
    if (draw->render.pict)
    {
	XRenderFreePicture (draw->dpy, draw->render.pict);
	draw->render.pict = 0;
    }
    if (draw->core.gc)
    {
	XFreeGC (draw->dpy, draw->core.gc);
	draw->core.gc = 0;
    }
}

Display *
XftDrawDisplay (XftDraw *draw)
{
    return draw->dpy;
}

Drawable
XftDrawDrawable (XftDraw *draw)
{
    return draw->drawable;
}

Colormap
XftDrawColormap (XftDraw *draw)
{
    return draw->colormap;
}

Visual *
XftDrawVisual (XftDraw *draw)
{
    return draw->visual;
}

void
XftDrawDestroy (XftDraw	*draw)
{
    if (draw->render.pict)
	XRenderFreePicture (draw->dpy, draw->render.pict);
    if (draw->core.gc)
	XFreeGC (draw->dpy, draw->core.gc);
    if (draw->clip)
	XDestroyRegion (draw->clip);
    XftMemFree (XFT_MEM_DRAW, sizeof (XftDraw));
    free (draw);
}

static Picture
_XftDrawSrcPicture (XftDraw *draw, XftColor *color)
{
    Display	    *dpy = draw->dpy;
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    int		    i;
    XftColor	    bitmapColor;

    if (!info)
	return 0;
    
    /*
     * Monochrome targets require special handling; the PictOp controls
     * the color, and the color must be opaque
     */
    if (!draw->visual && draw->depth == 1)
    {
	bitmapColor.color.alpha = 0xffff;
	bitmapColor.color.red   = 0xffff;
	bitmapColor.color.green = 0xffff;
	bitmapColor.color.blue  = 0xffff;
	color = &bitmapColor;
    }

    /*
     * See if there's one already available
     */
    for (i = 0; i < XFT_NUM_SOLID_COLOR; i++)
    {
	if (info->colors[i].pict && 
	    info->colors[i].screen == draw->screen &&
	    !memcmp ((void *) &color->color, 
		     (void *) &info->colors[i].color,
		     sizeof (XRenderColor)))
	    return info->colors[i].pict;
    }
    /*
     * Pick one to replace at random
     */
    i = (unsigned int) rand () % XFT_NUM_SOLID_COLOR;
    /*
     * Recreate if it was for the wrong screen
     */
    if (info->colors[i].screen != draw->screen && info->colors[i].pict)
    {
	XRenderFreePicture (dpy, info->colors[i].pict);
	info->colors[i].pict = 0;
    }
    /*
     * Create picture if necessary
     */
    if (!info->colors[i].pict)
    {
	Pixmap			    pix;
        XRenderPictureAttributes    pa;
	
	pix = XCreatePixmap (dpy, RootWindow (dpy, draw->screen), 1, 1,
			     info->solidFormat->depth);
	pa.repeat = True;
	info->colors[i].pict = XRenderCreatePicture (draw->dpy,
						     pix,
						     info->solidFormat,
						     CPRepeat, &pa);
	XFreePixmap (dpy, pix);
    }
    /*
     * Set to the new color
     */
    info->colors[i].color = color->color;
    info->colors[i].screen = draw->screen;
    XRenderFillRectangle (dpy, PictOpSrc,
			  info->colors[i].pict,
			  &color->color, 0, 0, 1, 1);
    return info->colors[i].pict;
}

static int
_XftDrawOp (XftDraw *draw, XftColor *color)
{
    if (draw->visual || draw->depth != 1)
	return PictOpOver;
    if (color->color.alpha >= 0x8000)
	return PictOpOver;
    return PictOpOutReverse;
}

static FcBool
_XftDrawRenderPrepare (XftDraw	*draw)
{
    if (!draw->render.pict)
    {
	XRenderPictFormat	    *format;

	format = _XftDrawFormat (draw);
	if (!format)
	    return FcFalse;
	draw->render.pict = XRenderCreatePicture (draw->dpy, draw->drawable,
						  format, 0, 0);
	if (!draw->render.pict)
	    return FcFalse;
	if (draw->clip)
	    XRenderSetPictureClipRegion (draw->dpy, draw->render.pict,
					 draw->clip);
    }
    return FcTrue;
}

static FcBool
_XftDrawCorePrepare (XftDraw *draw, XftColor *color)
{
    if (!draw->core.gc)
    {
	draw->core.gc = XCreateGC (draw->dpy, draw->drawable, 0, 0);
	if (!draw->core.gc)
	    return FcFalse;
	if (draw->clip)
	    XSetRegion (draw->dpy, draw->core.gc, draw->clip);
    }
    XSetForeground (draw->dpy, draw->core.gc, color->pixel);
    return FcTrue;
}
			
Picture
XftDrawPicture (XftDraw *draw)
{
    if (!_XftDrawRenderPrepare (draw))
	return 0;
    return draw->render.pict;
}

#define NUM_LOCAL   1024

void
XftDrawGlyphs (XftDraw	*draw,
	       XftColor	*color,
	       XftFont	*public,
	       int	x,
	       int	y,
	       FT_UInt	*glyphs,
	       int	nglyphs)
{
    XftFontInt	*font = (XftFontInt *) public;

    if (font->format)
    {
	Picture	    src;
	
	if (_XftDrawRenderPrepare (draw) &&
	    (src = _XftDrawSrcPicture (draw, color)))
	    XftGlyphRender (draw->dpy, _XftDrawOp (draw, color),
			     src, public, draw->render.pict,
			     0, 0, x, y, glyphs, nglyphs);
    }
    else
    {
	if (_XftDrawCorePrepare (draw, color))
	    XftGlyphCore (draw, color, public, x, y, glyphs, nglyphs);
    }
}

void
XftDrawString8 (XftDraw		*draw,
		XftColor	*color,
		XftFont		*public,
		int		x,
		int		y,
		FcChar8		*string,
		int		len)
{
    FT_UInt	    *glyphs, glyphs_local[NUM_LOCAL];
    int		    i;

    if (XftDebug () & XFT_DBG_DRAW)
	printf ("DrawString \"%*.*s\"\n", len, len, string);
    
    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (FT_UInt));
	if (!glyphs)
	    return;
    }
    for (i = 0; i < len; i++)
	glyphs[i] = XftCharIndex (draw->dpy, public, string[i]);
    XftDrawGlyphs (draw, color, public, x, y, glyphs, len);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftDrawString16 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*public,
		 int		x,
		 int		y,
		 FcChar16	*string,
		 int		len)
{
    FT_UInt	    *glyphs, glyphs_local[NUM_LOCAL];
    int		    i;

    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (FT_UInt));
	if (!glyphs)
	    return;
    }
    for (i = 0; i < len; i++)
	glyphs[i] = XftCharIndex (draw->dpy, public, string[i]);
    
    XftDrawGlyphs (draw, color, public, x, y, glyphs, len);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftDrawString32 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*public,
		 int		x,
		 int		y,
		 FcChar32	*string,
		 int		len)
{
    FT_UInt	    *glyphs, glyphs_local[NUM_LOCAL];
    int		    i;

    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (FT_UInt));
	if (!glyphs)
	    return;
    }
    for (i = 0; i < len; i++)
	glyphs[i] = XftCharIndex (draw->dpy, public, string[i]);
    
    XftDrawGlyphs (draw, color, public, x, y, glyphs, len);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftDrawStringUtf8 (XftDraw	*draw,
		   XftColor	*color,
		   XftFont	*public,
		   int		x,
		   int		y,
		   FcChar8	*string,
		   int		len)
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
    XftDrawGlyphs (draw, color, public, x, y, glyphs, len);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftDrawGlyphSpec (XftDraw	*draw,
		  XftColor	*color,
		  XftFont	*public,
		  XftGlyphSpec	*glyphs,
		  int		len)
{
    XftFontInt	*font = (XftFontInt *) public;

    if (font->format)
    {
	Picture	src;

	if (_XftDrawRenderPrepare (draw) &&
	    (src = _XftDrawSrcPicture (draw, color)))
	{
	    XftGlyphSpecRender (draw->dpy, _XftDrawOp (draw, color),
				src, public, draw->render.pict,
				0, 0, glyphs, len);
	}
    }
    else
    {
	if (_XftDrawCorePrepare (draw, color))
	    XftGlyphSpecCore (draw, color, public, glyphs, len);
    }
}

void
XftDrawGlyphFontSpec (XftDraw		*draw,
		      XftColor		*color,
		      XftGlyphFontSpec	*glyphs,
		      int		len)
{
    int		i;
    int		start;

    i = 0;
    while (i < len);
    {
	start = i;
	if (((XftFontInt *) glyphs[i].font)->format)
	{
	    Picture	src;
	    while (((XftFontInt *) glyphs[i].font)->format)
	    {
		i++;
	    }
	    if (_XftDrawRenderPrepare (draw) &&
		(src = _XftDrawSrcPicture (draw, color)))
	    {
		XftGlyphFontSpecRender (draw->dpy, _XftDrawOp (draw, color),
					src, draw->render.pict,
					0, 0, glyphs, i - start);
	    }
	}
	else
	{
	    while (!((XftFontInt *) glyphs[i].font)->format)
	    {
		i++;
	    }
	    if (_XftDrawCorePrepare (draw, color))
		XftGlyphFontSpecCore (draw, color, glyphs, len);
	}
    }
}

void
XftDrawCharSpec (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*public,
		 XftCharSpec	*chars,
		 int		len)
{
    XftGlyphSpec    *glyphs, glyphs_local[NUM_LOCAL];
    int		    i;

    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (XftGlyphSpec));
	if (!glyphs)
	    return;
    }
    for (i = 0; i < len; i++)
    {
	glyphs[i].glyph = XftCharIndex(draw->dpy, public, chars[i].ucs4);
	glyphs[i].x = chars[i].x;
	glyphs[i].y = chars[i].y;
    }

    XftDrawGlyphSpec (draw, color, public, glyphs, len);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftDrawCharFontSpec (XftDraw		*draw,
		     XftColor		*color,
		     XftCharFontSpec	*chars,
		     int		len)
{
    XftGlyphFontSpec	*glyphs, glyphs_local[NUM_LOCAL];
    int			i;

    if (len <= NUM_LOCAL)
	glyphs = glyphs_local;
    else
    {
	glyphs = malloc (len * sizeof (XftGlyphFontSpec));
	if (!glyphs)
	    return;
    }
    for (i = 0; i < len; i++)
    {
	glyphs[i].font = chars[i].font;
	glyphs[i].glyph = XftCharIndex(draw->dpy, glyphs[i].font, chars[i].ucs4);
	glyphs[i].x = chars[i].x;
	glyphs[i].y = chars[i].y;
    }

    XftDrawGlyphFontSpec (draw, color, glyphs, len);
    if (glyphs != glyphs_local)
	free (glyphs);
}

void
XftDrawRect (XftDraw	    *draw,
	     XftColor	    *color,
	     int	    x, 
	     int	    y,
	     unsigned int   width,
	     unsigned int   height)
{
    if (_XftDrawRenderPrepare (draw))
    {
	XRenderFillRectangle (draw->dpy, PictOpOver, draw->render.pict,
			      &color->color, x, y, width, height);
    }
    else if (_XftDrawCorePrepare (draw, color))
    {
	XftRectCore (draw, color, x, y, width, height);
    }
}

Bool
XftDrawSetClip (XftDraw	*draw,
		Region	r)
{
    Region			n = 0;

    if (!r && !draw->clip)
	return True;

    if (r && draw->clip && XEqualRegion (r, draw->clip))
	return True;

    if (r)
    {
	n = XCreateRegion ();
	if (n)
	{
	    if (!XUnionRegion (n, r, n))
	    {
		XDestroyRegion (n);
		return False;
	    }
	}
    }
    if (draw->clip)
	XDestroyRegion (draw->clip);
    draw->clip = n;
    if (draw->render.pict)
    {
	if (n)
	    XRenderSetPictureClipRegion (draw->dpy, draw->render.pict, n);
	else
	{
	    XRenderPictureAttributes	pa;
	    pa.clip_mask = None;
	    XRenderChangePicture (draw->dpy, draw->render.pict,
				  CPClipMask, &pa);
	}
    }
    if (draw->core.gc)
    {
	if (n)
	    XSetRegion (draw->dpy, draw->core.gc, draw->clip);
	else
	    XSetClipMask (draw->dpy, draw->core.gc, None);
    }
    return True;
}
