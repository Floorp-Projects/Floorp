/*
 * $XFree86: xc/lib/Xft/xftcore.c,v 1.7 2002/02/19 07:51:20 keithp Exp $
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
#include "xftint.h"
#include <X11/Xmd.h>

void
XftRectCore (XftDraw	    *draw,
	     XftColor	    *color,
	     int	    x, 
	     int	    y,
	     unsigned int   width,
	     unsigned int   height)
{
    if (color->color.alpha >= 0x8000)
    {
	XSetForeground (draw->dpy, draw->core.gc, color->pixel);
	XFillRectangle (draw->dpy, draw->drawable, draw->core.gc,
			x, y, width, height);
    }
}

/*
 * Use the core protocol to draw the glyphs
 */

static void
_XftSharpGlyphMono (XftDraw	*draw,
		    XftGlyph	*glyph,
		    int		x,
		    int		y)
{
    unsigned char   *srcLine = glyph->bitmap, *src;
    unsigned char   bits, bitsMask;
    int		    width = glyph->metrics.width;
    int		    stride = ((width + 31) & ~31) >> 3;
    int		    height = glyph->metrics.height;
    int		    w;
    int		    xspan, lenspan;

    x -= glyph->metrics.x;
    y -= glyph->metrics.y;
    while (height--)
    {
	src = srcLine;
	srcLine += stride;
	w = width;
	
	bitsMask = 0x80;    /* FreeType is always MSB first */
	bits = *src++;
	
	xspan = x;
	while (w)
	{
	    if (bits & bitsMask)
	    {
		lenspan = 0;
		do
		{
		    lenspan++;
		    if (lenspan == w)
			break;
		    bitsMask = bitsMask >> 1;
		    if (!bitsMask)
		    {
			bits = *src++;
			bitsMask = 0x80;
		    }
		} while (bits & bitsMask);
		XFillRectangle (draw->dpy, draw->drawable, 
				draw->core.gc, xspan, y, lenspan, 1);
		xspan += lenspan;
		w -= lenspan;
	    }
	    else
	    {
		do
		{
		    w--;
		    xspan++;
		    if (!w)
			break;
		    bitsMask = bitsMask >> 1;
		    if (!bitsMask)
		    {
			bits = *src++;
			bitsMask = 0x80;
		    }
		} while (!(bits & bitsMask));
	    }
	}
	y++;
    }
}

/*
 * Draw solid color text from an anti-aliased bitmap.  This is a
 * fallback for cases where a particular drawable has no AA code
 */
static void
_XftSharpGlyphGray (XftDraw	*draw,
		    XftGlyph	*glyph,
		    int		x,
		    int		y)
{
    unsigned char   *srcLine = glyph->bitmap, *src, bits;
    int		    width = glyph->metrics.width;
    int		    stride = ((width + 3) & ~3);
    int		    height = glyph->metrics.height;
    int		    w;
    int		    xspan, lenspan;

    x -= glyph->metrics.x;
    y -= glyph->metrics.y;
    while (height--)
    {
	src = srcLine;
	srcLine += stride;
	w = width;
	
	bits = *src++;
	xspan = x;
	while (w)
	{
	    if (bits >= 0x80)
	    {
		lenspan = 0;
		do
		{
		    lenspan++;
		    if (lenspan == w)
			break;
		    bits = *src++;
		} while (bits >= 0x80);
		XFillRectangle (draw->dpy, draw->drawable, 
				draw->core.gc, xspan, y, lenspan, 1);
		xspan += lenspan;
		w -= lenspan;
	    }
	    else
	    {
		do
		{
		    w--;
		    xspan++;
		    if (!w)
			break;
		    bits = *src++;
		} while (bits < 0x80);
	    }
	}
	y++;
    }
}

static void
_XftSharpGlyphRgba (XftDraw	*draw,
		    XftGlyph	*glyph,
		    int		x,
		    int		y)
{
    CARD32	    *srcLine = glyph->bitmap, *src, bits;
    int		    width = glyph->metrics.width;
    int		    stride = ((width + 3) & ~3);
    int		    height = glyph->metrics.height;
    int		    w;
    int		    xspan, lenspan;

    x -= glyph->metrics.x;
    y -= glyph->metrics.y;
    while (height--)
    {
	src = srcLine;
	srcLine += stride;
	w = width;
	
	bits = *src++;
	xspan = x;
	while (w)
	{
	    if (bits >= 0x80000000)
	    {
		lenspan = 0;
		do
		{
		    lenspan++;
		    if (lenspan == w)
			break;
		    bits = *src++;
		} while (bits >= 0x80000000);
		XFillRectangle (draw->dpy, draw->drawable, 
				draw->core.gc, xspan, y, lenspan, 1);
		xspan += lenspan;
		w -= lenspan;
	    }
	    else
	    {
		do
		{
		    w--;
		    xspan++;
		    if (!w)
			break;
		    bits = *src++;
		} while (bits < 0x80000000);
	    }
	}
	y++;
    }
}

typedef void (*XftSharpGlyph) (XftDraw	*draw,
			       XftGlyph	*glyph,
			       int	x,
			       int	y);

static XftSharpGlyph
_XftSharpGlyphFind (XftDraw *draw, XftFont *public)
{
    XftFontInt *font = (XftFontInt *) public;

    if (!font->antialias)
	return _XftSharpGlyphMono;
    else if (font->rgba == FC_RGBA_NONE)
	return _XftSharpGlyphGray;
    else
	return _XftSharpGlyphRgba;
}

/*
 * Draw glyphs to a target that supports anti-aliasing
 */

/*
 * Primitives for converting between RGB values and TrueColor pixels
 */
 
static void
_XftExamineBitfield (unsigned long mask, int *shift, int *len)
{
    int	s, l;

    s = 0;
    while ((mask & 1) == 0)
    {
	mask >>= 1;
	s++;
    }
    l = 0;
    while ((mask & 1) == 1)
    {
	mask >>= 1;
	l++;
    }
    *shift = s;
    *len = l;
}

static CARD32
_XftGetField (unsigned long l_pixel, int shift, int len)
{
    CARD32  pixel = (CARD32) l_pixel;
    
    pixel = pixel & (((1 << (len)) - 1) << shift);
    pixel = pixel << (32 - (shift + len)) >> 24;
    while (len < 8)
    {
	pixel |= (pixel >> len);
	len <<= 1;
    }
    return pixel;
}

static unsigned long
_XftPutField (CARD32 pixel, int shift, int len)
{
    unsigned long   l_pixel = (unsigned long) pixel;

    shift = shift - (8 - len);
    if (len <= 8)
	l_pixel &= (((1 << len) - 1) << (8 - len));
    if (shift < 0)
	l_pixel >>= -shift;
    else
	l_pixel <<= shift;
    return l_pixel;
}

/*
 * This is used when doing XftCharFontSpec/XftGlyphFontSpec where
 * some of the fonts are bitmaps and some are anti-aliased to handle
 * the bitmap portions
 */
static void
_XftSmoothGlyphMono (XImage	*image,
		     XftGlyph	*xftg,
		     int	x,
		     int	y,
		     XftColor   *color)
{
    unsigned char   *srcLine = xftg->bitmap, *src;
    unsigned char   bits, bitsMask;
    int		    width = xftg->metrics.width;
    int		    stride = ((width + 31) & ~31) >> 3;
    int		    height = xftg->metrics.height;
    int		    w;
    int		    xspan;
    int		    r_shift, r_len;
    int		    g_shift, g_len;
    int		    b_shift, b_len;
    unsigned long   pixel;

    _XftExamineBitfield (image->red_mask, &r_shift, &r_len);
    _XftExamineBitfield (image->green_mask, &g_shift, &g_len);
    _XftExamineBitfield (image->blue_mask, &b_shift, &b_len);
    pixel = (_XftPutField (color->color.red >> 8, r_shift, r_len) |
	     _XftPutField (color->color.green >> 8, g_shift, g_len) |
	     _XftPutField (color->color.blue >> 8, b_shift, b_len));
    x -= xftg->metrics.x;
    y -= xftg->metrics.y;
    while (height--)
    {
	src = srcLine;
	srcLine += stride;
	w = width;
	
	bitsMask = 0x80;    /* FreeType is always MSB first */
	bits = *src++;
	
	xspan = x;
	while (w)
	{
	    if (bits & bitsMask)
		XPutPixel (image, xspan, y, pixel);
    	    bitsMask = bitsMask >> 1;
    	    if (!bitsMask)
    	    {
    		bits = *src++;
    		bitsMask = 0x80;
    	    }
	    xspan++;
	}
	y++;
    }
}

/*
 * As simple anti-aliasing is likely to be common, there are three
 * optimized versions for the usual true color pixel formats (888, 565, 555).
 * Other formats are handled by the general case
 */

#define cvt8888to0565(s)    ((((s) >> 3) & 0x001f) | \
			     (((s) >> 5) & 0x07e0) | \
			     (((s) >> 8) & 0xf800))

#define cvt0565to8888(s)    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) | \
			     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) | \
			     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

#define cvt8888to0555(s)    ((((s) >> 3) & 0x001f) | \
			     (((s) >> 6) & 0x03e0) | \
			     (((s) >> 7) & 0x7c00))

#define cvt0555to8888(s)    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) | \
			     ((((s) << 6) & 0xf800) | (((s) >> 0) & 0x300)) | \
			     ((((s) << 9) & 0xf80000) | (((s) << 4) & 0x70000)))


#define XftIntMult(a,b,t) ( (t) = (a) * (b) + 0x80, ( ( ( (t)>>8 ) + (t) )>>8 ) )
#define XftIntDiv(a,b)	 (((CARD16) (a) * 255) / (b))

#define XftGet8(v,i)   ((CARD16) (CARD8) ((v) >> i))

/*
 * There are two ways of handling alpha -- either as a single unified value or
 * a separate value for each component, hence each macro must have two
 * versions.  The unified alpha version has a 'U' at the end of the name,
 * the component version has a 'C'.  Similarly, functions which deal with
 * this difference will have two versions using the same convention.
 */

#define XftOverU(x,y,i,a,t) ((t) = XftIntMult(XftGet8(y,i),(a),(t)) + XftGet8(x,i),\
			   (CARD32) ((CARD8) ((t) | (0 - ((t) >> 8)))) << (i))

#define XftOverC(x,y,i,a,t) ((t) = XftIntMult(XftGet8(y,i),XftGet8(a,i),(t)) + XftGet8(x,i),\
			    (CARD32) ((CARD8) ((t) | (0 - ((t) >> 8)))) << (i))

#define XftInU(x,i,a,t) ((CARD32) XftIntMult(XftGet8(x,i),(a),(t)) << (i))

#define XftInC(x,i,a,t) ((CARD32) XftIntMult(XftGet8(x,i),XftGet8(a,i),(t)) << (i))

#define XftGen(x,y,i,ax,ay,t,u,v) ((t) = (XftIntMult(XftGet8(y,i),ay,(u)) + \
					 XftIntMult(XftGet8(x,i),ax,(v))),\
				  (CARD32) ((CARD8) ((t) | \
						     (0 - ((t) >> 8)))) << (i))

#define XftAdd(x,y,i,t)	((t) = XftGet8(x,i) + XftGet8(y,i), \
			 (CARD32) ((CARD8) ((t) | (0 - ((t) >> 8)))) << (i))


static CARD32
fbOver24 (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o;

    m = XftOverU(x,y,0,a,t);
    n = XftOverU(x,y,8,a,t);
    o = XftOverU(x,y,16,a,t);
    return m|n|o;
}

static CARD32
fbIn (CARD32 x, CARD8 y)
{
    CARD16  a = y;
    CARD16  t;
    CARD32  m,n,o,p;

    m = XftInU(x,0,a,t);
    n = XftInU(x,8,a,t);
    o = XftInU(x,16,a,t);
    p = XftInU(x,24,a,t);
    return m|n|o|p;
}

static void
_XftSmoothGlyphGray8888 (XImage	    *image,
			 XftGlyph   *xftg,
			 int	    x,
			 int	    y,
			 XftColor   *color)
{
    CARD32	src, srca;
    CARD32	r, g, b;
    CARD32	*dstLine, *dst, d;
    CARD8	*maskLine, *mask, m;
    int		dstStride, maskStride;
    int		width, height;
    int		w;

    srca = color->color.alpha >> 8;
    
    /* This handles only RGB and BGR */
    g = (color->color.green & 0xff00);
    if (image->red_mask == 0xff0000)
    {
	r = (color->color.red & 0xff00) << 8;
	b = color->color.blue >> 8;
    }
    else
    {
	r = color->color.red >> 8;
	b = (color->color.blue & 0xff00) << 8;
    }
    src = (srca << 24) | r | g | b;
    
    width = xftg->metrics.width;
    height = xftg->metrics.height;
    
    x -= xftg->metrics.x;
    y -= xftg->metrics.y;

    dstLine = (CARD32 *) (image->data + image->bytes_per_line * y + (x << 2));
    dstStride = image->bytes_per_line >> 2;
    maskLine = (unsigned char *) xftg->bitmap;
    maskStride = (width + 3) & ~3;
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    *dst = src;
		else
		    *dst = fbOver24 (src, *dst);
	    }
	    else if (m)
	    {
		d = fbIn (src, m);
		*dst = fbOver24 (d, *dst);
	    }
	    dst++;
	}
    }
}

static void
_XftSmoothGlyphGray565 (XImage	    *image,
			XftGlyph    *xftg,
			int	    x,
			int	    y,
			XftColor    *color)
{
    CARD32	src, srca;
    CARD32	r, g, b;
    CARD32	d;
    CARD16	*dstLine, *dst;
    CARD8	*maskLine, *mask, m;
    int		dstStride, maskStride;
    int		width, height;
    int		w;

    srca = color->color.alpha >> 8;
    
    /* This handles only RGB and BGR */
    g = (color->color.green & 0xff00);
    if (image->red_mask == 0xf800)
    {
	r = (color->color.red & 0xff00) << 8;
	b = color->color.blue >> 8;
    }
    else
    {
	r = color->color.red >> 8;
	b = (color->color.blue & 0xff00) << 8;
    }
    src = (srca << 24) | r | g | b;
    
    width = xftg->metrics.width;
    height = xftg->metrics.height;
    
    x -= xftg->metrics.x;
    y -= xftg->metrics.y;

    dstLine = (CARD16 *) (image->data + image->bytes_per_line * y + (x << 1));
    dstStride = image->bytes_per_line >> 1;
    maskLine = (unsigned char *) xftg->bitmap;
    maskStride = (width + 3) & ~3;
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = *dst;
		    d = fbOver24 (src, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    else if (m)
	    {
		d = *dst;
		d = fbOver24 (fbIn(src,m), cvt0565to8888(d));
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

static void
_XftSmoothGlyphGray555 (XImage	    *image,
			XftGlyph    *xftg,
			int	    x,
			int	    y,
			XftColor    *color)
{
    CARD32	src, srca;
    CARD32	r, g, b;
    CARD32	d;
    CARD16	*dstLine, *dst;
    CARD8	*maskLine, *mask, m;
    int		dstStride, maskStride;
    int		width, height;
    int		w;

    srca = color->color.alpha >> 8;
    
    /* This handles only RGB and BGR */
    g = (color->color.green & 0xff00);
    if (image->red_mask == 0xf800)
    {
	r = (color->color.red & 0xff00) << 8;
	b = color->color.blue >> 8;
    }
    else
    {
	r = color->color.red >> 8;
	b = (color->color.blue & 0xff00) << 8;
    }
    src = (srca << 24) | r | g | b;
    
    width = xftg->metrics.width;
    height = xftg->metrics.height;
    
    x -= xftg->metrics.x;
    y -= xftg->metrics.y;

    dstLine = (CARD16 *) (image->data + image->bytes_per_line * y + (x << 1));
    dstStride = image->bytes_per_line >> 1;
    maskLine = (unsigned char *) xftg->bitmap;
    maskStride = (width + 3) & ~3;
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = *dst;
		    d = fbOver24 (src, cvt0555to8888(d));
		}
		*dst = cvt8888to0555(d);
	    }
	    else if (m)
	    {
		d = *dst;
		d = fbOver24 (fbIn(src,m), cvt0555to8888(d));
		*dst = cvt8888to0555(d);
	    }
	    dst++;
	}
    }
}

static void
_XftSmoothGlyphGray (XImage	*image,
		     XftGlyph	*xftg,
		     int	x,
		     int	y,
		     XftColor   *color)
{
    CARD32	    src, srca;
    int		    r_shift, r_len;
    int		    g_shift, g_len;
    int		    b_shift, b_len;
    CARD8	    *maskLine, *mask, m;
    int		    maskStride;
    CARD32	    d;
    unsigned long   pixel;
    int		    width, height;
    int		    w, tx;
    
    srca = color->color.alpha >> 8;
    src = (srca << 24 |
	   (color->color.red & 0xff00) << 8 |
	   (color->color.green & 0xff00) |
	   (color->color.blue) >> 8);
    x -= xftg->metrics.x;
    y -= xftg->metrics.y;
    width = xftg->metrics.width;
    height = xftg->metrics.height;
    
    maskLine = (unsigned char *) xftg->bitmap;
    maskStride = (width + 3) & ~3;

    _XftExamineBitfield (image->red_mask, &r_shift, &r_len);
    _XftExamineBitfield (image->green_mask, &g_shift, &g_len);
    _XftExamineBitfield (image->blue_mask, &b_shift, &b_len);
    while (height--)
    {
	mask = maskLine;
	maskLine += maskStride;
	w = width;
	tx = x;
	
	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    pixel = XGetPixel (image, tx, y);
		    d = (_XftGetField (pixel, r_shift, r_len) << 16 |
			 _XftGetField (pixel, g_shift, g_len) << 8 |
			 _XftGetField (pixel, b_shift, b_len));
		    d = fbOver24 (src, d);
		}
		pixel = (_XftPutField ((d >> 16) & 0xff, r_shift, r_len) |
			 _XftPutField ((d >>  8) & 0xff, g_shift, g_len) |
			 _XftPutField ((d      ) & 0xff, b_shift, b_len));
		XPutPixel (image, tx, y, pixel);
	    }
	    else if (m)
	    {
		pixel = XGetPixel (image, tx, y);
		d = (_XftGetField (pixel, r_shift, r_len) << 16 |
		     _XftGetField (pixel, g_shift, g_len) << 8 |
		     _XftGetField (pixel, b_shift, b_len));
		d = fbOver24 (fbIn(src,m), d);
		pixel = (_XftPutField ((d >> 16) & 0xff, r_shift, r_len) |
			 _XftPutField ((d >>  8) & 0xff, g_shift, g_len) |
			 _XftPutField ((d      ) & 0xff, b_shift, b_len));
		XPutPixel (image, tx, y, pixel);
	    }
	    tx++;
	}
	y++;
    }
}

static void
_XftSmoothGlyphRgba (XImage	*image,
		     XftGlyph	*xftg,
		     int	x,
		     int	y,
		     XftColor   *color)
{
    CARD32	    src, srca;
    int		    r_shift, r_len;
    int		    g_shift, g_len;
    int		    b_shift, b_len;
    CARD32	    *mask, ma;
    CARD32	    d;
    unsigned long   pixel;
    int		    width, height;
    int		    w, tx;
    
    srca = color->color.alpha >> 8;
    src = (srca << 24 |
	   (color->color.red & 0xff00) << 8 |
	   (color->color.green & 0xff00) |
	   (color->color.blue) >> 8);
    x -= xftg->metrics.x;
    y -= xftg->metrics.y;
    width = xftg->metrics.width;
    height = xftg->metrics.height;
    
    mask = (CARD32 *) xftg->bitmap;

    _XftExamineBitfield (image->red_mask, &r_shift, &r_len);
    _XftExamineBitfield (image->green_mask, &g_shift, &g_len);
    _XftExamineBitfield (image->blue_mask, &b_shift, &b_len);
    while (height--)
    {
	w = width;
	tx = x;
	
	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    pixel = XGetPixel (image, tx, y);
		    d = (_XftGetField (pixel, r_shift, r_len) << 16 |
			 _XftGetField (pixel, g_shift, g_len) << 8 |
			 _XftGetField (pixel, b_shift, b_len));
		    d = fbOver24 (src, d);
		}
		pixel = (_XftPutField ((d >> 16) & 0xff, r_shift, r_len) |
			 _XftPutField ((d >>  8) & 0xff, g_shift, g_len) |
			 _XftPutField ((d      ) & 0xff, b_shift, b_len));
		XPutPixel (image, tx, y, pixel);
	    }
	    else if (ma)
	    {
		CARD32	m,n,o;
		pixel = XGetPixel (image, tx, y);
		d = (_XftGetField (pixel, r_shift, r_len) << 16 |
		     _XftGetField (pixel, g_shift, g_len) << 8 |
		     _XftGetField (pixel, b_shift, b_len));
#define XftInOverC(src,srca,msk,dst,i,result) { \
    CARD16  __a = XftGet8(msk,i); \
    CARD32  __t, __ta; \
    CARD32  __i; \
    __t = XftIntMult (XftGet8(src,i), __a,__i); \
    __ta = (CARD8) ~XftIntMult (srca, __a,__i); \
    __t = __t + XftIntMult(XftGet8(dst,i),__ta,__i); \
    __t = (CARD32) (CARD8) (__t | (-(__t >> 8))); \
    result = __t << (i); \
}
		XftInOverC(src,srca,ma,d,0,m);
		XftInOverC(src,srca,ma,d,8,n);
		XftInOverC(src,srca,ma,d,16,o);
		d = m | n | o;
		pixel = (_XftPutField ((d >> 16) & 0xff, r_shift, r_len) |
			 _XftPutField ((d >>  8) & 0xff, g_shift, g_len) |
			 _XftPutField ((d      ) & 0xff, b_shift, b_len));
		XPutPixel (image, tx, y, pixel);
	    }
	    tx++;
	}
	y++;
    }
}

static FcBool
_XftSmoothGlyphPossible (XftDraw *draw)
{
    if (!draw->visual)
	return FcFalse;
    if (draw->visual->class != TrueColor)
	return FcFalse;
    return FcTrue;
}

typedef	void (*XftSmoothGlyph) (XImage	*image, 
			       XftGlyph *xftg,
			       int	x,
			       int	y,
			       XftColor	*color);

static XftSmoothGlyph
_XftSmoothGlyphFind (XftDraw *draw, XftFont *public)
{
    XftFontInt *font = (XftFontInt *) public;

    if (!font->antialias)
	return _XftSmoothGlyphMono;
    else if (font->rgba == FC_RGBA_NONE)
    {
	switch (XftDrawBitsPerPixel (draw)) {
	case 32:
	    if ((draw->visual->red_mask   == 0xff0000 &&
		 draw->visual->green_mask == 0x00ff00 &&
		 draw->visual->blue_mask  == 0x0000ff) ||
		(draw->visual->red_mask   == 0x0000ff &&
		 draw->visual->green_mask == 0x00ff00 &&
		 draw->visual->blue_mask  == 0xff0000))
	    {
		return _XftSmoothGlyphGray8888;
	    }
	    break;
	case 16:
	    if ((draw->visual->red_mask   == 0xf800 &&
		 draw->visual->green_mask == 0x07e0 &&
		 draw->visual->blue_mask  == 0x001f) ||
		(draw->visual->red_mask   == 0x001f &&
		 draw->visual->green_mask == 0x07e0 &&
		 draw->visual->blue_mask  == 0xf800))
	    {
		return _XftSmoothGlyphGray565;
	    }
	    if ((draw->visual->red_mask   == 0x7c00 &&
		 draw->visual->green_mask == 0x03e0 &&
		 draw->visual->blue_mask  == 0x001f) ||
		(draw->visual->red_mask   == 0x001f &&
		 draw->visual->green_mask == 0x03e0 &&
		 draw->visual->blue_mask  == 0x7c00))
	    {
		return _XftSmoothGlyphGray555;
	    }
	    break;
	default:
	    break;
	}
	return _XftSmoothGlyphGray;
    }
    else
	return _XftSmoothGlyphRgba;
}

static XftGlyph *
_XftGlyphDefault (Display *dpy, XftFont   *public)
{
    XftFontInt	    *font = (XftFontInt *) public;
    FT_UInt	    missing[XFT_NMISSING];
    int		    nmissing;
    FcBool	    glyphs_loaded = FcFalse;

    if (XftFontCheckGlyph (dpy, public, FcTrue, 0, missing, &nmissing))
	glyphs_loaded = FcTrue;
    if (nmissing)
	XftFontLoadGlyphs (dpy, public, FcTrue, missing, nmissing);
    return font->glyphs[0];
}

static int XftGetImageErrorHandler (Display *dpy, XErrorEvent *error_event)
{
    return 0;
}

void
XftGlyphCore (XftDraw	*draw,
	      XftColor	*color,
	      XftFont	*public,
	      int	x,
	      int	y,
	      FT_UInt	*glyphs,
	      int	nglyphs)
{
    Display	    *dpy = draw->dpy;
    XftFontInt	    *font = (XftFontInt *) public;
    XftGlyph	    *xftg;
    FT_UInt	    glyph, *g;
    FT_UInt	    missing[XFT_NMISSING];
    FcBool	    glyphs_loaded;
    int		    nmissing;
    int		    n;
    XErrorHandler   prev_error;

    /*
     * Load missing glyphs
     */
    g = glyphs;
    n = nglyphs;
    nmissing = 0;
    glyphs_loaded = FcFalse;
    while (n--)
	if (XftFontCheckGlyph (dpy, public, FcTrue, *g++, missing, &nmissing))
	    glyphs_loaded = FcTrue;
    if (nmissing)
	XftFontLoadGlyphs (dpy, public, FcTrue, missing, nmissing);
    
    g = glyphs;
    n = nglyphs;
    if ((font->antialias || color->color.alpha != 0xffff) &&
	_XftSmoothGlyphPossible (draw))
    {
	XGlyphInfo	gi;
	XImage		*image;
        unsigned int    depth;
	int		ox, oy;
	XftSmoothGlyph	smooth = _XftSmoothGlyphFind (draw, public);
	
	XftGlyphExtents (dpy, public, glyphs, nglyphs, &gi);
	if (!gi.width || !gi.height)
	    goto bail1;
	ox = x - gi.x;
	oy = y - gi.y;
	/*
	 * Try to get bits directly from the drawable; if that fails,
	 * use a temporary pixmap.  When it does fail, assume it
	 * will probably fail for a while and keep using temporary
	 * pixmaps for a while to avoid double round trips.
	 */
	if (draw->core.use_pixmap == 0)
	{
	    prev_error = XSetErrorHandler (XftGetImageErrorHandler);
	    image = XGetImage (dpy, draw->drawable,
			       ox, oy,
			       gi.width, gi.height, AllPlanes,
			       ZPixmap);
	    XSetErrorHandler (prev_error);
	    if (!image)
		draw->core.use_pixmap = XFT_ASSUME_PIXMAP;
	}
	else
	{
	    draw->core.use_pixmap--;
	    image = 0;
	}
	if (!image && (depth = XftDrawDepth (draw)))
	{
	    Pixmap	pix;
	    GC		gc;
	    XGCValues	gcv;

	    pix = XCreatePixmap (dpy, draw->drawable,
				 gi.width, gi.height, depth);
	    gcv.graphics_exposures = False;
	    gc = XCreateGC (dpy, pix, GCGraphicsExposures, &gcv);
	    XCopyArea (dpy, draw->drawable, pix, gc, ox, oy,
		       gi.width, gi.height, 0, 0);
	    XFreeGC (dpy, gc);
	    image = XGetImage (dpy, pix, 0, 0, gi.width, gi.height, AllPlanes,
			       ZPixmap);
	    XFreePixmap (dpy, pix);
	}
	if (!image)
	    goto bail1;
	image->red_mask = draw->visual->red_mask;
	image->green_mask = draw->visual->green_mask;
	image->blue_mask = draw->visual->blue_mask;
	if (image->byte_order != XftNativeByteOrder ())
	    XftSwapImage (image);
	while (n--)
	{
	    glyph = *g++;
	    if (glyph >= font->num_glyphs || !(xftg = font->glyphs[glyph]))
		xftg = _XftGlyphDefault (dpy, public);
	    if (xftg)
	    {
		(*smooth) (image, xftg, x - ox, y - oy, color);
		x += xftg->metrics.xOff;
		y += xftg->metrics.yOff;
	    }
	}
	if (image->byte_order != XftNativeByteOrder ())
	    XftSwapImage (image);
	XPutImage (dpy, draw->drawable, draw->core.gc, image, 0, 0, ox, oy,
		   gi.width, gi.height);
    }
    else
    {
	XftSharpGlyph	sharp = _XftSharpGlyphFind (draw, public);
	while (n--)
	{
	    glyph = *g++;
	    if (glyph >= font->num_glyphs || !(xftg = font->glyphs[glyph]))
		xftg = _XftGlyphDefault (dpy, public);
	    if (xftg)
	    {
		(*sharp) (draw, xftg, x, y);
		x += xftg->metrics.xOff;
		y += xftg->metrics.yOff;
	    }
	}
    }
bail1:
    if (glyphs_loaded)
	_XftFontManageMemory (dpy, public);
}

#define NUM_LOCAL   1024

void
XftGlyphSpecCore (XftDraw	*draw,
		  XftColor	*color,
		  XftFont	*public,
		  XftGlyphSpec	*glyphs,
		  int		nglyphs)
{
    Display	    *dpy = draw->dpy;
    XftFontInt	    *font = (XftFontInt *) public;
    XftGlyph	    *xftg;
    FT_UInt	    missing[XFT_NMISSING];
    FcBool	    glyphs_loaded;
    int		    nmissing;
    int		    i;
    XErrorHandler   prev_error;
    int		    x1, y1, x2, y2;

    /*
     * Load missing glyphs
     */
    nmissing = 0;
    glyphs_loaded = FcFalse;
    x1 = y1 = x2 = y2 = 0;
    for (i = 0; i < nglyphs; i++)
    {
	XGlyphInfo	gi;
	int		g_x1, g_x2, g_y1, g_y2;
	
	if (XftFontCheckGlyph (dpy, public, FcTrue, glyphs[i].glyph, missing, &nmissing))
	    glyphs_loaded = FcTrue;
	XftGlyphExtents (dpy, public, &glyphs[i].glyph, 1, &gi);
	g_x1 = glyphs[i].x - gi.x;
	g_y1 = glyphs[i].y - gi.y;
	g_x2 = g_x1 + gi.width;
	g_y2 = g_y1 + gi.height;
	if (i)
	{
	    if (g_x1 < x1)
		x1 = g_x1;
	    if (g_y1 < y1)
		y1 = g_y1;
	    if (g_x2 > x2)
		x2 = g_x2;
	    if (g_y2 > y2)
		y2 = g_y2;
	}
	else
	{
	    x1 = g_x1;
	    y1 = g_y1;
	    x2 = g_x2;
	    y2 = g_y2;
	}
    }
    if (nmissing)
	XftFontLoadGlyphs (dpy, public, FcTrue, missing, nmissing);
    
    if (x1 == x2 || y1 == y2)
	goto bail1;

    if ((font->antialias || color->color.alpha != 0xffff) &&
	_XftSmoothGlyphPossible (draw))
    {
	XImage		*image;
        unsigned int    depth;
	int		width = x2 - x1, height = y2 - y1;
	XftSmoothGlyph	smooth = _XftSmoothGlyphFind (draw, public);

	/*
	 * Try to get bits directly from the drawable; if that fails,
	 * use a temporary pixmap.  When it does fail, assume it
	 * will probably fail for a while and keep using temporary
	 * pixmaps for a while to avoid double round trips.
	 */
	if (draw->core.use_pixmap == 0)
	{
	    prev_error = XSetErrorHandler (XftGetImageErrorHandler);
	    image = XGetImage (dpy, draw->drawable,
			       x1, y1,
			       width, height, AllPlanes,
			       ZPixmap);
	    XSetErrorHandler (prev_error);
	    if (!image)
		draw->core.use_pixmap = XFT_ASSUME_PIXMAP;
	}
	else
	{
	    draw->core.use_pixmap--;
	    image = 0;
	}
	if (!image && (depth = XftDrawDepth (draw)))
	{
	    Pixmap	pix;
	    GC		gc;
	    XGCValues	gcv;

	    pix = XCreatePixmap (dpy, draw->drawable,
				 width, height, depth);
	    gcv.graphics_exposures = False;
	    gc = XCreateGC (dpy, pix, GCGraphicsExposures, &gcv);
	    XCopyArea (dpy, draw->drawable, pix, gc, x1, y1,
		       width, height, 0, 0);
	    XFreeGC (dpy, gc);
	    image = XGetImage (dpy, pix, 0, 0, width, height, AllPlanes,
			       ZPixmap);
	    XFreePixmap (dpy, pix);
	}
	if (!image)
	    goto bail1;
	image->red_mask = draw->visual->red_mask;
	image->green_mask = draw->visual->green_mask;
	image->blue_mask = draw->visual->blue_mask;
	if (image->byte_order != XftNativeByteOrder ())
	    XftSwapImage (image);
	for (i = 0; i < nglyphs; i++)
	{
	    FT_UInt glyph = glyphs[i].glyph;
	    if (glyph >= font->num_glyphs || !(xftg = font->glyphs[glyph]))
		xftg = _XftGlyphDefault (dpy, public);
	    if (xftg)
	    {
		(*smooth) (image, xftg, glyphs[i].x - x1, 
			   glyphs[i].y - y1, color);
	    }
	}
	if (image->byte_order != XftNativeByteOrder ())
	    XftSwapImage (image);
	XPutImage (dpy, draw->drawable, draw->core.gc, image, 0, 0, x1, y1,
		   width, height);
    }
    else
    {
	XftSharpGlyph	sharp = _XftSharpGlyphFind (draw, public);
	for (i = 0; i < nglyphs; i++)
	{
	    FT_UInt glyph = glyphs[i].glyph;
	    if (glyph >= font->num_glyphs || !(xftg = font->glyphs[glyph]))
		xftg = _XftGlyphDefault (dpy, public);
	    if (xftg)
		(*sharp) (draw, xftg, glyphs[i].x, glyphs[i].y);
	}
    }
bail1:
    if (glyphs_loaded)
	_XftFontManageMemory (dpy, public);
}

void
XftGlyphFontSpecCore (XftDraw		*draw,
		      XftColor		*color,
		      XftGlyphFontSpec	*glyphs,
		      int		nglyphs)
{
    Display	    *dpy = draw->dpy;
    XftGlyph	    *xftg;
    FT_UInt	    missing[XFT_NMISSING];
    FcBool	    glyphs_loaded;
    int		    nmissing;
    int		    i;
    XErrorHandler   prev_error;
    int		    x1, y1, x2, y2;

    /*
     * Load missing glyphs
     */
    glyphs_loaded = FcFalse;
    x1 = y1 = x2 = y2 = 0;
    for (i = 0; i < nglyphs; i++)
    {
	XftFont		*public = glyphs[i].font;
	XGlyphInfo	gi;
	int		g_x1, g_x2, g_y1, g_y2;
	
	nmissing = 0;
	if (XftFontCheckGlyph (dpy, public, FcTrue, glyphs[i].glyph, missing, &nmissing))
	    glyphs_loaded = FcTrue;
	if (nmissing)
	    XftFontLoadGlyphs (dpy, public, FcTrue, missing, nmissing);
	
	XftGlyphExtents (dpy, public, &glyphs[i].glyph, 1, &gi);
	g_x1 = glyphs[i].x - gi.x;
	g_y1 = glyphs[i].y - gi.y;
	g_x2 = g_x1 + gi.width;
	g_y2 = g_y1 + gi.height;
	if (i)
	{
	    if (g_x1 < x1)
		x1 = g_x1;
	    if (g_y1 < y1)
		y1 = g_y1;
	    if (g_x2 > x2)
		x2 = g_x2;
	    if (g_y2 > y2)
		y2 = g_y2;
	}
	else
	{
	    x1 = g_x1;
	    y1 = g_y1;
	    x2 = g_x2;
	    y2 = g_y2;
	}
    }
    
    for (i = 0; i < nglyphs; i++)
	if (((XftFontInt *) glyphs[i].font)->antialias)
	    break;
    
    if ((i != nglyphs || color->color.alpha != 0xffff) &&
	_XftSmoothGlyphPossible (draw))
    {
	XImage		*image;
        unsigned int    depth;
	int		width = x2 - x1, height = y2 - y1;

	/*
	 * Try to get bits directly from the drawable; if that fails,
	 * use a temporary pixmap.  When it does fail, assume it
	 * will probably fail for a while and keep using temporary
	 * pixmaps for a while to avoid double round trips.
	 */
	if (draw->core.use_pixmap == 0)
	{
	    prev_error = XSetErrorHandler (XftGetImageErrorHandler);
	    image = XGetImage (dpy, draw->drawable,
			       x1, y1,
			       width, height, AllPlanes,
			       ZPixmap);
	    XSetErrorHandler (prev_error);
	    if (!image)
		draw->core.use_pixmap = XFT_ASSUME_PIXMAP;
	}
	else
	{
	    draw->core.use_pixmap--;
	    image = 0;
	}
	if (!image && (depth = XftDrawDepth (draw)))
	{
	    Pixmap	pix;
	    GC		gc;
	    XGCValues	gcv;

	    pix = XCreatePixmap (dpy, draw->drawable,
				 width, height, depth);
	    gcv.graphics_exposures = False;
	    gc = XCreateGC (dpy, pix, GCGraphicsExposures, &gcv);
	    XCopyArea (dpy, draw->drawable, pix, gc, x1, y1,
		       width, height, 0, 0);
	    XFreeGC (dpy, gc);
	    image = XGetImage (dpy, pix, 0, 0, width, height, AllPlanes,
			       ZPixmap);
	    XFreePixmap (dpy, pix);
	}
	if (!image)
	    goto bail1;
	image->red_mask = draw->visual->red_mask;
	image->green_mask = draw->visual->green_mask;
	image->blue_mask = draw->visual->blue_mask;
	if (image->byte_order != XftNativeByteOrder ())
	    XftSwapImage (image);
	for (i = 0; i < nglyphs; i++)
	{
	    XftFont	    *public = glyphs[i].font;
	    XftFontInt	    *font = (XftFontInt *) public;
	    XftSmoothGlyph  smooth = _XftSmoothGlyphFind (draw, public);
	    FT_UInt	    glyph = glyphs[i].glyph;
	    
	    if (glyph >= font->num_glyphs || !(xftg = font->glyphs[glyph]))
		xftg = _XftGlyphDefault (dpy, public);
	    if (xftg)
	    {
		(*smooth) (image, xftg, glyphs[i].x - x1, 
			   glyphs[i].y - y1, color);
	    }
	}
	if (image->byte_order != XftNativeByteOrder ())
	    XftSwapImage (image);
	XPutImage (dpy, draw->drawable, draw->core.gc, image, 0, 0, x1, y1,
		   width, height);
    }
    else
    {
	for (i = 0; i < nglyphs; i++)
	{
	    XftFont		*public = glyphs[i].font;
	    XftFontInt		*font = (XftFontInt *) public;
	    XftSharpGlyph	sharp = _XftSharpGlyphFind (draw, public);
	    FT_UInt		glyph = glyphs[i].glyph;
	    
	    if (glyph >= font->num_glyphs || !(xftg = font->glyphs[glyph]))
		xftg = _XftGlyphDefault (dpy, public);
	    if (xftg)
		(*sharp) (draw, xftg, glyphs[i].x, glyphs[i].y);
	}
    }
bail1:
    if (glyphs_loaded)
	for (i = 0; i < nglyphs; i++)
	    _XftFontManageMemory (dpy, glyphs[i].font);
}
