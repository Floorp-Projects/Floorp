/*
 * $XFree86: xc/lib/Xft/xftglyphs.c,v 1.15 2002/02/15 07:36:11 keithp Exp $
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
#include <freetype/ftoutln.h>
#include <fontconfig/fcfreetype.h>

static const int    filters[3][3] = {
    /* red */
#if 0
{    65538*4/7,65538*2/7,65538*1/7 },
    /* green */
{    65536*1/4, 65536*2/4, 65537*1/4 },
    /* blue */
{    65538*1/7,65538*2/7,65538*4/7 },
#endif
{    65538*9/13,65538*3/13,65538*1/13 },
    /* green */
{    65538*1/6, 65538*4/6, 65538*1/6 },
    /* blue */
{    65538*1/13,65538*3/13,65538*9/13 },
};

void
XftFontLoadGlyphs (Display	*dpy,
		   XftFont	*public,
		   FcBool	need_bitmaps,
		   FT_UInt	*glyphs,
		   int		nglyph)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    XftFontInt	    *font = (XftFontInt *) public;
    FT_Error	    error;
    FT_UInt	    glyphindex;
    FT_GlyphSlot    glyphslot;
    XftGlyph	    *xftg;
    Glyph	    glyph;
    unsigned char   bufLocal[4096];
    unsigned char   *bufBitmap = bufLocal;
    int		    bufSize = sizeof (bufLocal);
    int		    size, pitch;
    unsigned char   bufLocalRgba[4096];
    unsigned char   *bufBitmapRgba = bufLocalRgba;
    int		    bufSizeRgba = sizeof (bufLocalRgba);
    int		    sizergba, pitchrgba, widthrgba;
    int		    width;
    int		    height;
    int		    left, right, top, bottom;
    int		    hmul = 1;
    int		    vmul = 1;
    FT_Bitmap	    ftbit;
    FT_Matrix	    matrix;
    FT_Vector	    vector;
    Bool	    subpixel = False;
    FT_Face	    face;

    if (!info)
	return;

    face = XftLockFace (&font->public);
    
    if (!_XftSetFace (font->file, font->size, &font->matrix))
    {
	XftUnlockFace (&font->public);
	return;
    }

    matrix.xx = matrix.yy = 0x10000L;
    matrix.xy = matrix.yx = 0;

    if (font->antialias)
    {
	switch (font->rgba) {
	case FC_RGBA_RGB:
	case FC_RGBA_BGR:
	    matrix.xx *= 3;
	    subpixel = True;
	    hmul = 3;
	    break;
	case FC_RGBA_VRGB:
	case FC_RGBA_VBGR:
	    matrix.yy *= 3;
	    vmul = 3;
	    subpixel = True;
	    break;
	}
    }

    while (nglyph--)
    {
	glyphindex = *glyphs++;
	xftg = font->glyphs[glyphindex];
	if (!xftg)
	    continue;
	
	error = FT_Load_Glyph (face, glyphindex, font->load_flags);
	if (error)
	{
	    /*
	     * If anti-aliasing or transforming glyphs and
	     * no outline version exists, fallback to the
	     * bitmap and let things look bad instead of
	     * missing the glyph
	     */
	    if (font->load_flags & FT_LOAD_NO_BITMAP)
		error = FT_Load_Glyph (face, glyphindex,
				       font->load_flags & ~FT_LOAD_NO_BITMAP);
	    if (error)
		continue;
	}

#define FLOOR(x)    ((x) & -64)
#define CEIL(x)	    (((x)+63) & -64)
#define TRUNC(x)    ((x) >> 6)
#define ROUND(x)    (((x)+32) & -64)
		
	glyphslot = face->glyph;

	/*
	 * Compute glyph metrics from FreeType information
	 */
	if(font->transform && glyphslot->format != ft_glyph_format_bitmap) 
	{
	    /*
	     * calculate the true width by transforming all four corners.
	     */
	    int xc, yc;
	    left = right = top = bottom = 0;
	    for(xc = 0; xc <= 1; xc ++) {
		for(yc = 0; yc <= 1; yc++) {
		    vector.x = glyphslot->metrics.horiBearingX + xc * glyphslot->metrics.width;
		    vector.y = glyphslot->metrics.horiBearingY - yc * glyphslot->metrics.height;
		    FT_Vector_Transform(&vector, &font->matrix);   
		    if (XftDebug() & XFT_DBG_GLYPH)
			printf("Trans %d %d: %d %d\n", (int) xc, (int) yc, 
			       (int) vector.x, (int) vector.y);
		    if(xc == 0 && yc == 0) {
			left = right = vector.x;
			top = bottom = vector.y;
		    } else {
			if(left > vector.x) left = vector.x;
			if(right < vector.x) right = vector.x;
			if(bottom > vector.y) bottom = vector.y;
			if(top < vector.y) top = vector.y;
		    }

		}
	    }
	    left = FLOOR(left);
	    right = CEIL(right);
	    bottom = FLOOR(bottom);
	    top = CEIL(top);

	} else {
	    left  = FLOOR( glyphslot->metrics.horiBearingX );
	    right = CEIL( glyphslot->metrics.horiBearingX + glyphslot->metrics.width );

	    top    = CEIL( glyphslot->metrics.horiBearingY );
	    bottom = FLOOR( glyphslot->metrics.horiBearingY - glyphslot->metrics.height );
	}

	width = TRUNC(right - left);
	height = TRUNC( top - bottom );

	/*
	 * Try to keep monospace fonts ink-inside
	 * XXX transformed?
	 */
	if (font->spacing != FC_PROPORTIONAL && !font->transform)
	{
	    if (font->load_flags & FT_LOAD_VERTICAL_LAYOUT)
	    {
		if (TRUNC(bottom) > font->public.max_advance_width)
		{
		    int adjust;
    
		    adjust = bottom - (font->public.max_advance_width << 6);
		    if (adjust > top)
			adjust = top;
		    top -= adjust;
		    bottom -= adjust;
		    height = font->public.max_advance_width;
		}
	    }
	    else
	    {
		if (TRUNC(right) > font->public.max_advance_width)
		{
		    int adjust;
    
		    adjust = right - (font->public.max_advance_width << 6);
		    if (adjust > left)
			adjust = left;
		    left -= adjust;
		    right -= adjust;
		    width = font->public.max_advance_width;
		}
	    }
	}

	if (font->antialias)
	    pitch = (width * hmul + 3) & ~3;
	else
	    pitch = ((width + 31) & ~31) >> 3;

	size = pitch * height * vmul;

	xftg->metrics.width = width;
	xftg->metrics.height = height;
	xftg->metrics.x = -TRUNC(left);
	xftg->metrics.y = TRUNC(top);

	if (font->spacing != FC_PROPORTIONAL)
	{
	    if (font->transform)
	    {
		if (font->load_flags & FT_LOAD_VERTICAL_LAYOUT)
		{
		    vector.x = 0;
		    vector.y = -font->public.max_advance_width;
		}
		else
		{
		    vector.x = font->public.max_advance_width;
		    vector.y = 0;
		}
		FT_Vector_Transform (&vector, &font->matrix);
		xftg->metrics.xOff = vector.x;
		xftg->metrics.yOff = -vector.y;
	    }
	    else
	    {
		if (font->load_flags & FT_LOAD_VERTICAL_LAYOUT)
		{
		    xftg->metrics.xOff = 0;
		    xftg->metrics.yOff = -font->public.max_advance_width;
		}
		else
		{
		    xftg->metrics.xOff = font->public.max_advance_width;
		    xftg->metrics.yOff = 0;
		}
	    }
	}
	else
	{
	    xftg->metrics.xOff = TRUNC(ROUND(glyphslot->advance.x));
	    xftg->metrics.yOff = -TRUNC(ROUND(glyphslot->advance.y));
	}
	
	/*
	 * If the glyph is relatively large (> 1% of server memory),
	 * don't send it until necessary
	 */
	if (!need_bitmaps && size > info->max_glyph_memory / 100)
	    continue;
	
	/*
	 * Make sure there's enough buffer space for the glyph
	 */
	if (size > bufSize)
	{
	    if (bufBitmap != bufLocal)
		free (bufBitmap);
	    bufBitmap = (unsigned char *) malloc (size);
	    if (!bufBitmap)
		continue;
	    bufSize = size;
	}
	memset (bufBitmap, 0, size);

	/*
	 * Rasterize into the local buffer
	 */
	switch (glyphslot->format) {
	case ft_glyph_format_outline:
	    ftbit.width      = width * hmul;
	    ftbit.rows       = height * vmul;
	    ftbit.pitch      = pitch;
	    if (font->antialias)
		ftbit.pixel_mode = ft_pixel_mode_grays;
	    else
		ftbit.pixel_mode = ft_pixel_mode_mono;
	    
	    ftbit.buffer     = bufBitmap;
	    
	    if (subpixel)
		FT_Outline_Transform (&glyphslot->outline, &matrix);

	    FT_Outline_Translate ( &glyphslot->outline, -left*hmul, -bottom*vmul );

	    FT_Outline_Get_Bitmap( _XftFTlibrary, &glyphslot->outline, &ftbit );
	    break;
	case ft_glyph_format_bitmap:
	    if (font->antialias)
	    {
		unsigned char	*srcLine, *dstLine;
		int		height;
		int		x;
		int	    h, v;

		srcLine = glyphslot->bitmap.buffer;
		dstLine = bufBitmap;
		height = glyphslot->bitmap.rows;
		while (height--)
		{
		    for (x = 0; x < glyphslot->bitmap.width; x++)
		    {
			/* always MSB bitmaps */
			unsigned char	a = ((srcLine[x >> 3] & (0x80 >> (x & 7))) ?
					     0xff : 0x00);
			if (subpixel)
			{
			    for (v = 0; v < vmul; v++)
				for (h = 0; h < hmul; h++)
				    dstLine[v * pitch + x*hmul + h] = a;
			}
			else
			    dstLine[x] = a;
		    }
		    dstLine += pitch * vmul;
		}
	    }
	    else
	    {
		unsigned char	*srcLine, *dstLine;
		int		h, bytes;

		srcLine = glyphslot->bitmap.buffer;
		dstLine = bufBitmap;
		h = glyphslot->bitmap.rows;
		bytes = (glyphslot->bitmap.width + 7) >> 3;
		while (h--)
		{
		    memcpy (dstLine, srcLine, bytes);
		    dstLine += pitch;
		    srcLine += glyphslot->bitmap.pitch;
		}
	    }
	    break;
	default:
	    if (XftDebug() & XFT_DBG_GLYPH)
		printf ("glyph %d is not in a usable format\n",
			(int) glyphindex);
	    continue;
	}
	
	if (XftDebug() & XFT_DBG_GLYPH)
	{
	    printf ("glyph %d:\n", (int) glyphindex);
	    printf (" xywh (%d %d %d %d), trans (%d %d %d %d) wh (%d %d)\n",
		    (int) glyphslot->metrics.horiBearingX,
		    (int) glyphslot->metrics.horiBearingY,
		    (int) glyphslot->metrics.width,
		    (int) glyphslot->metrics.height,
		    left, right, top, bottom,
		    width, height);
	    if (XftDebug() & XFT_DBG_GLYPHV)
	    {
		int		x, y;
		unsigned char	*line;

		line = bufBitmap;
		for (y = 0; y < height * vmul; y++)
		{
		    if (font->antialias) 
		    {
			static char    den[] = { " .:;=+*#" };
			for (x = 0; x < pitch; x++)
			    printf ("%c", den[line[x] >> 5]);
		    }
		    else
		    {
			for (x = 0; x < pitch * 8; x++)
			{
			    printf ("%c", line[x>>3] & (1 << (x & 7)) ? '#' : ' ');
			}
		    }
		    printf ("|\n");
		    line += pitch;
		}
		printf ("\n");
	    }
	}

	/*
	 * Use the glyph index as the wire encoding; it
	 * might be more efficient for some locales to map
	 * these by first usage to smaller values, but that
	 * would require persistently storing the map when
	 * glyphs were freed.
	 */
	glyph = (Glyph) glyphindex;

	if (subpixel)
	{
	    int		    x, y;
	    unsigned char   *in_line, *out_line, *in;
	    unsigned int    *out;
	    unsigned int    red, green, blue;
	    int		    rf, gf, bf;
	    int		    s;
	    int		    o, os;
	    
	    /*
	     * Filter the glyph to soften the color fringes
	     */
	    widthrgba = width;
	    pitchrgba = (widthrgba * 4 + 3) & ~3;
	    sizergba = pitchrgba * height;

	    os = 1;
	    switch (font->rgba) {
	    case FC_RGBA_VRGB:
		os = pitch;
	    case FC_RGBA_RGB:
	    default:
		rf = 0;
		gf = 1;
		bf = 2;
		break;
	    case FC_RGBA_VBGR:
		os = pitch;
	    case FC_RGBA_BGR:
		bf = 0;
		gf = 1;
		rf = 2;
		break;
	    }
	    if (sizergba > bufSizeRgba)
	    {
		if (bufBitmapRgba != bufLocalRgba)
		    free (bufBitmapRgba);
		bufBitmapRgba = (unsigned char *) malloc (sizergba);
		if (!bufBitmapRgba)
		    continue;
		bufSizeRgba = sizergba;
	    }
	    memset (bufBitmapRgba, 0, sizergba);
	    in_line = bufBitmap;
	    out_line = bufBitmapRgba;
	    for (y = 0; y < height; y++)
	    {
		in = in_line;
		out = (unsigned int *) out_line;
		in_line += pitch * vmul;
		out_line += pitchrgba;
		for (x = 0; x < width * hmul; x += hmul)
		{
		    red = green = blue = 0;
		    o = 0;
		    for (s = 0; s < 3; s++)
		    {
			red += filters[rf][s]*in[x+o];
			green += filters[gf][s]*in[x+o];
			blue += filters[bf][s]*in[x+o];
			o += os;
		    }
		    red = red / 65536;
		    green = green / 65536;
		    blue = blue / 65536;
		    *out++ = (green << 24) | (red << 16) | (green << 8) | blue;
		}
	    }
	    
	    xftg->glyph_memory = sizergba + sizeof (XftGlyph);
	    if (font->format)
	    {
		if (!font->glyphset)
		    font->glyphset = XRenderCreateGlyphSet (dpy, font->format);
		if (ImageByteOrder (dpy) != XftNativeByteOrder ())
		    XftSwapCARD32 ((CARD32 *) bufBitmapRgba, sizergba >> 2);
		XRenderAddGlyphs (dpy, font->glyphset, &glyph,
				  &xftg->metrics, 1, 
				  (char *) bufBitmapRgba, sizergba);
	    }
	    else
	    {
		xftg->bitmap = malloc (sizergba);
		if (xftg->bitmap)
		    memcpy (xftg->bitmap, bufBitmapRgba, sizergba);
	    }
	}
	else
	{
	    xftg->glyph_memory = size + sizeof (XftGlyph);
	    if (font->format)
	    {
		/*
		 * swap bit order around; FreeType is always MSBFirst
		 */
		if (!font->antialias)
		{
		    if (BitmapBitOrder (dpy) != MSBFirst)
		    {
			unsigned char   *line;
			unsigned char   c;
			int		    i;

			line = (unsigned char *) bufBitmap;
			i = size;
			while (i--)
			{
			    c = *line;
			    c = ((c << 1) & 0xaa) | ((c >> 1) & 0x55);
			    c = ((c << 2) & 0xcc) | ((c >> 2) & 0x33);
			    c = ((c << 4) & 0xf0) | ((c >> 4) & 0x0f);
			    *line++ = c;
			}
		    }
		}
		if (!font->glyphset)
		    font->glyphset = XRenderCreateGlyphSet (dpy, font->format);
		XRenderAddGlyphs (dpy, font->glyphset, &glyph,
				  &xftg->metrics, 1, 
				  (char *) bufBitmap, size);
	    }
	    else
	    {
		xftg->bitmap = malloc (size);
		if (xftg->bitmap)
		    memcpy (xftg->bitmap, bufBitmap, size);
	    }
	}
	font->glyph_memory += xftg->glyph_memory;
	info->glyph_memory += xftg->glyph_memory;
	if (XftDebug() & XFT_DBG_CACHEV)
	    printf ("Caching glyph 0x%x size %ld\n", glyphindex,
		    xftg->glyph_memory);
    }
    if (bufBitmap != bufLocal)
	free (bufBitmap);
    if (bufBitmapRgba != bufLocalRgba)
	free (bufBitmapRgba);
    XftUnlockFace (&font->public);
}

void
XftFontUnloadGlyphs (Display	*dpy,
		     XftFont	*public,
		     FT_UInt	*glyphs,
		     int	nglyph)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    XftFontInt	    *font = (XftFontInt *) public;
    XftGlyph	    *xftg;
    FT_UInt	    glyphindex;
    Glyph	    glyphBuf[1024];
    int		    nused;
    
    nused = 0;
    while (nglyph--)
    {
	glyphindex = *glyphs++;
	xftg = font->glyphs[glyphindex];
	if (!xftg)
	    continue;
	if (xftg->glyph_memory)
	{
	    if (font->format)
	    {
		if (font->glyphset)
		{
		    glyphBuf[nused++] = (Glyph) glyphindex;
		    if (nused == sizeof (glyphBuf) / sizeof (glyphBuf[0]))
		    {
			XRenderFreeGlyphs (dpy, font->glyphset, glyphBuf, nused);
			nused = 0;
		    }
		}
	    }
	    else
	    {
		if (xftg->bitmap)
		    free (xftg->bitmap);
	    }
	    font->glyph_memory -= xftg->glyph_memory;
	    if (info)
		info->glyph_memory -= xftg->glyph_memory;
	}
	free (xftg);
	XftMemFree (XFT_MEM_GLYPH, sizeof (XftGlyph));
	font->glyphs[glyphindex] = 0;
    }    
    if (font->glyphset && nused)
	XRenderFreeGlyphs (dpy, font->glyphset, glyphBuf, nused);
}

FcBool
XftFontCheckGlyph (Display	*dpy,
		   XftFont	*public,
		   FcBool	need_bitmaps,
		   FT_UInt	glyph,
		   FT_UInt	*missing,
		   int		*nmissing)
{
    XftFontInt	    *font = (XftFontInt *) public;
    XftGlyph	    *xftg;
    int		    n;
    
    if (glyph >= font->num_glyphs)
	return FcFalse;
    xftg = font->glyphs[glyph];
    if (!xftg || (need_bitmaps && !xftg->glyph_memory))
    {
	if (!xftg)
	{
	    xftg = (XftGlyph *) malloc (sizeof (XftGlyph));
	    if (!xftg)
		return FcFalse;
	    XftMemAlloc (XFT_MEM_GLYPH, sizeof (XftGlyph));
	    xftg->bitmap = 0;
	    xftg->glyph_memory = 0;
	    font->glyphs[glyph] = xftg;
	}
	n = *nmissing;
	missing[n++] = glyph;
	if (n == XFT_NMISSING)
	{
	    XftFontLoadGlyphs (dpy, public, need_bitmaps, missing, n);
	    n = 0;
	}
	*nmissing = n;
	return FcTrue;
    }
    else
	return FcFalse;
}

FcBool
XftCharExists (Display	    *dpy,
	       XftFont	    *public,
	       FcChar32    ucs4)
{
    if (public->charset)
	return FcCharSetHasChar (public->charset, ucs4);
    return FcFalse;
}

#define Missing	    ((FT_UInt) ~0)

FT_UInt
XftCharIndex (Display	    *dpy, 
	      XftFont	    *public,
	      FcChar32	    ucs4)
{
    XftFontInt	*font = (XftFontInt *) public;
    FcChar32	ent, offset;
    FT_Face	face;
    
    ent = ucs4 % font->hash_value;
    offset = 0;
    while (font->hash_table[ent].ucs4 != ucs4)
    {
	if (font->hash_table[ent].ucs4 == (FcChar32) ~0)
	{
	    if (!XftCharExists (dpy, public, ucs4))
		return 0;
	    face  = XftLockFace (public);
	    if (!face)
		return 0;
	    font->hash_table[ent].ucs4 = ucs4;
	    font->hash_table[ent].glyph = FcFreeTypeCharIndex (face, ucs4);
	    XftUnlockFace (public);
	    break;
	}
	if (!offset)
	{
	    offset = ucs4 % font->rehash_value;
	    if (!offset)
		offset = 1;
	}
	ent = ent + offset;
	if (ent > font->hash_value)
	    ent -= font->hash_value;
    }
    return font->hash_table[ent].glyph;
}

/*
 * Pick a random glyph from the font and remove it from the cache
 */
void
_XftFontUncacheGlyph (Display *dpy, XftFont *public)
{
    XftFontInt	    *font = (XftFontInt *) public;
    unsigned long   glyph_memory;
    FT_UInt	    glyphindex;
    XftGlyph	    *xftg;
    
    if (!font->glyph_memory)
	return;
    if (font->use_free_glyphs)
    {
	glyph_memory = rand() % font->glyph_memory;
    }
    else
    {
	if (font->glyphset)
	{
	    XRenderFreeGlyphSet (dpy, font->glyphset);
	    font->glyphset = 0;
	}
	glyph_memory = 0;
    }
	
    for (glyphindex = 0; glyphindex < font->num_glyphs; glyphindex++)
    {
	xftg = font->glyphs[glyphindex];
	if (xftg)
	{
	    if (xftg->glyph_memory > glyph_memory)
	    {
		if (XftDebug() & XFT_DBG_CACHEV)
		    printf ("Uncaching glyph 0x%x size %ld\n",
			    glyphindex, xftg->glyph_memory);
		XftFontUnloadGlyphs (dpy, public, &glyphindex, 1);
		if (!font->use_free_glyphs)
		    continue;
		break;
	    }
	    glyph_memory -= xftg->glyph_memory;
	}
    }
}

void
_XftFontManageMemory (Display *dpy, XftFont *public)
{
    XftFontInt	*font = (XftFontInt *) public;

    if (font->max_glyph_memory)
    {
	if (XftDebug() & XFT_DBG_CACHE)
	{
	    if (font->glyph_memory > font->max_glyph_memory)
		printf ("Reduce memory for font 0x%lx from %ld to %ld\n",
			font->glyphset ? font->glyphset : (unsigned long) font,
			font->glyph_memory, font->max_glyph_memory);
	}
	while (font->glyph_memory > font->max_glyph_memory)
	    _XftFontUncacheGlyph (dpy, public);
    }
    _XftDisplayManageMemory (dpy);
}
