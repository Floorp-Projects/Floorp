/*
 * $XFree86: xc/lib/Xft/xftdpy.c,v 1.9 2002/02/15 07:36:11 keithp Exp $
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
#include <ctype.h>
#include <X11/Xlibint.h>
#include "xftint.h"

XftDisplayInfo	*_XftDisplayInfo;

static int
_XftCloseDisplay (Display *dpy, XExtCodes *codes)
{
    XftDisplayInfo  *info, **prev;

    for (prev = &_XftDisplayInfo; (info = *prev); prev = &(*prev)->next)
	if (info->codes == codes)
	    break;
    if (!info)
	return 0;
    *prev = info->next;
    if (info->defaults)
	FcPatternDestroy (info->defaults);
    free (info);
    return 0;
}


XftDisplayInfo *
_XftDisplayInfoGet (Display *dpy)
{
    XftDisplayInfo	*info, **prev;
    XRenderPictFormat	pf;
    int			i;

    for (prev = &_XftDisplayInfo; (info = *prev); prev = &(*prev)->next)
    {
	if (info->display == dpy)
	{
	    /*
	     * MRU the list
	     */
	    if (prev != &_XftDisplayInfo)
	    {
		*prev = info->next;
		info->next = _XftDisplayInfo;
		_XftDisplayInfo = info;
	    }
	    return info;
	}
    }
    info = (XftDisplayInfo *) malloc (sizeof (XftDisplayInfo));
    if (!info)
	goto bail0;
    info->codes = XAddExtension (dpy);
    if (!info->codes)
	goto bail1;
    (void) XESetCloseDisplay (dpy, info->codes->extension, _XftCloseDisplay);

    info->display = dpy;
    info->defaults = 0;
    info->hasRender = XRenderFindVisualFormat (dpy, DefaultVisual (dpy, DefaultScreen (dpy))) != 0;
    info->use_free_glyphs = FcTrue;
    if (info->hasRender)
    {
	int major, minor;
	XRenderQueryVersion (dpy, &major, &minor);
	if (major < 0 || (major == 0 && minor <= 2))
	    info->use_free_glyphs = FcFalse;
    }
    pf.type = PictTypeDirect;
    pf.depth = 32;
    pf.direct.redMask = 0xff;
    pf.direct.greenMask = 0xff;
    pf.direct.blueMask = 0xff;
    pf.direct.alphaMask = 0xff;
    info->solidFormat = XRenderFindFormat (dpy,
					   (PictFormatType|
					    PictFormatDepth|
					    PictFormatRedMask|
					    PictFormatGreenMask|
					    PictFormatBlueMask|
					    PictFormatAlphaMask),
					   &pf,
					   0);
    if (XftDebug () & XFT_DBG_RENDER)
    {
	Visual		    *visual = DefaultVisual (dpy, DefaultScreen (dpy));
	XRenderPictFormat   *format = XRenderFindVisualFormat (dpy, visual);
	
	printf ("XftDisplayInfoGet Default visual 0x%x ", 
		(int) visual->visualid);
	if (format)
	{
	    if (format->type == PictTypeDirect)
	    {
		printf ("format %d,%d,%d,%d\n",
			format->direct.alpha,
			format->direct.red,
			format->direct.green,
			format->direct.blue);
	    }
	    else
	    {
		printf ("format indexed\n");
	    }
	}
	else
	    printf ("No Render format for default visual\n");
	
	printf ("XftDisplayInfoGet initialized, hasRender set to \"%s\"\n",
		info->hasRender ? "True" : "False");
    }
    for (i = 0; i < XFT_NUM_SOLID_COLOR; i++)
    {
	info->colors[i].screen = -1;
	info->colors[i].pict = 0;
    }
    info->fonts = 0;
    
    info->next = _XftDisplayInfo;
    _XftDisplayInfo = info;
    
    info->glyph_memory = 0;
    info->max_glyph_memory = XftDefaultGetInteger (dpy,
						   XFT_MAX_GLYPH_MEMORY, 0,
						   XFT_DPY_MAX_GLYPH_MEMORY);
    if (XftDebug () & XFT_DBG_CACHE)
	printf ("global max cache memory %ld\n", info->max_glyph_memory);
    return info;
    
bail1:
    free (info);
bail0:
    if (XftDebug () & XFT_DBG_RENDER)
    {
	printf ("XftDisplayInfoGet failed to initialize, Xft unhappy\n");
    }
    return 0;
}

/*
 * Reduce memory usage in X server
 */
void
_XftDisplayManageMemory (Display *dpy)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    unsigned long   glyph_memory;
    XftFont	    *public;
    XftFontInt	    *font;

    if (!info || !info->max_glyph_memory)
	return;
    if (XftDebug () & XFT_DBG_CACHE)
    {
	if (info->glyph_memory > info->max_glyph_memory)
	    printf ("Reduce global memory from %ld to %ld\n",
		    info->glyph_memory, info->max_glyph_memory);
    }
    while (info->glyph_memory > info->max_glyph_memory)
    {
	glyph_memory = rand () % info->glyph_memory;
	public = info->fonts;
	while (public)
	{
	    font = (XftFontInt *) public;

	    if (font->glyph_memory > glyph_memory)
	    {
		_XftFontUncacheGlyph (dpy, public);
		break;
	    }
	    public = font->next;
	    glyph_memory -= font->glyph_memory;
	}
    }
}

Bool
XftDefaultHasRender (Display *dpy)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);

    if (!info)
	return False;
    return info->hasRender;
}

Bool
XftDefaultSet (Display *dpy, FcPattern *defaults)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);

    if (!info)
	return False;
    if (info->defaults)
	FcPatternDestroy (info->defaults);
    info->defaults = defaults;
    if (!info->max_glyph_memory)
	info->max_glyph_memory = XFT_DPY_MAX_GLYPH_MEMORY;
    info->max_glyph_memory = XftDefaultGetInteger (dpy,
						   XFT_MAX_GLYPH_MEMORY, 0,
						   info->max_glyph_memory);
    return True;
}

int
XftDefaultParseBool (char *v)
{
    char    c0, c1;

    c0 = *v;
    if (isupper (c0))
	c0 = tolower (c0);
    if (c0 == 't' || c0 == 'y' || c0 == '1')
	return 1;
    if (c0 == 'f' || c0 == 'n' || c0 == '0')
	return 0;
    if (c0 == 'o')
    {
	c1 = v[1];
	if (isupper (c1))
	    c1 = tolower (c1);
	if (c1 == 'n')
	    return 1;
	if (c1 == 'f')
	    return 0;
    }
    return -1;
}

static Bool
_XftDefaultInitBool (Display *dpy, FcPattern *pat, char *option)
{
    char    *v;
    int	    i;

    v = XGetDefault (dpy, "Xft", option);
    if (v && (i = XftDefaultParseBool (v)) >= 0)
	return FcPatternAddBool (pat, option, i != 0);
    return True;
}

static Bool
_XftDefaultInitDouble (Display *dpy, FcPattern *pat, char *option)
{
    char    *v, *e;
    double  d;

    v = XGetDefault (dpy, "Xft", option);
    if (v)
    {
	d = strtod (v, &e);
	if (e != v)
	    return FcPatternAddDouble (pat, option, d);
    }
    return True;
}

static Bool
_XftDefaultInitInteger (Display *dpy, FcPattern *pat, char *option)
{
    char    *v, *e;
    int	    i;

    v = XGetDefault (dpy, "Xft", option);
    if (v)
    {
	if (FcNameConstant ((FcChar8 *) v, &i))
	    return FcPatternAddInteger (pat, option, i);
	i = strtol (v, &e, 0);
	if (e != v)
	    return FcPatternAddInteger (pat, option, i);
    }
    return True;
}

static FcPattern *
_XftDefaultInit (Display *dpy)
{
    FcPattern	*pat;

    pat = FcPatternCreate ();
    if (!pat)
	goto bail0;

    if (!_XftDefaultInitDouble (dpy, pat, FC_SCALE))
	goto bail1;
    if (!_XftDefaultInitDouble (dpy, pat, FC_DPI))
	goto bail1;
    if (!_XftDefaultInitBool (dpy, pat, XFT_RENDER))
	goto bail1;
    if (!_XftDefaultInitInteger (dpy, pat, FC_RGBA))
	goto bail1;
    if (!_XftDefaultInitBool (dpy, pat, FC_ANTIALIAS))
	goto bail1;
    if (!_XftDefaultInitBool (dpy, pat, FC_MINSPACE))
	goto bail1;
    if (!_XftDefaultInitInteger (dpy, pat, XFT_MAX_GLYPH_MEMORY))
	goto bail1;
    
    return pat;
    
bail1:
    FcPatternDestroy (pat);
bail0:
    return 0;
}

static FcResult
_XftDefaultGet (Display *dpy, const char *object, int screen, FcValue *v)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    FcResult	    r;

    if (!info)
	return FcResultNoMatch;
    
    if (!info->defaults)
    {
	info->defaults = _XftDefaultInit (dpy);
	if (!info->defaults)
	    return FcResultNoMatch;
    }
    r = FcPatternGet (info->defaults, object, screen, v);
    if (r == FcResultNoId && screen > 0)
	r = FcPatternGet (info->defaults, object, 0, v);
    return r;
}

Bool
XftDefaultGetBool (Display *dpy, const char *object, int screen, Bool def)
{
    FcResult	    r;
    FcValue	    v;

    r = _XftDefaultGet (dpy, object, screen, &v);
    if (r != FcResultMatch || v.type != FcTypeBool)
	return def;
    return v.u.b;
}

int
XftDefaultGetInteger (Display *dpy, const char *object, int screen, int def)
{
    FcResult	    r;
    FcValue	    v;

    r = _XftDefaultGet (dpy, object, screen, &v);
    if (r != FcResultMatch || v.type != FcTypeInteger)
	return def;
    return v.u.i;
}

double
XftDefaultGetDouble (Display *dpy, const char *object, int screen, double def)
{
    FcResult	    r;
    FcValue	    v;

    r = _XftDefaultGet (dpy, object, screen, &v);
    if (r != FcResultMatch || v.type != FcTypeDouble)
	return def;
    return v.u.d;
}

void
XftDefaultSubstitute (Display *dpy, int screen, FcPattern *pattern)
{
    FcValue	v;
    double	dpi;

    if (FcPatternGet (pattern, XFT_RENDER, 0, &v) == FcResultNoMatch)
    {
	FcPatternAddBool (pattern, XFT_RENDER,
			   XftDefaultGetBool (dpy, XFT_RENDER, screen, 
					      XftDefaultHasRender (dpy)));
    }
    if (FcPatternGet (pattern, FC_ANTIALIAS, 0, &v) == FcResultNoMatch)
    {
	FcPatternAddBool (pattern, FC_ANTIALIAS,
			   XftDefaultGetBool (dpy, FC_ANTIALIAS, screen,
					      True));
    }
    if (FcPatternGet (pattern, FC_RGBA, 0, &v) == FcResultNoMatch)
    {
	FcPatternAddInteger (pattern, FC_RGBA,
			      XftDefaultGetInteger (dpy, FC_RGBA, screen, 
						    FC_RGBA_NONE));
    }
    if (FcPatternGet (pattern, FC_MINSPACE, 0, &v) == FcResultNoMatch)
    {
	FcPatternAddBool (pattern, FC_MINSPACE,
			   XftDefaultGetBool (dpy, FC_MINSPACE, screen,
					      False));
    }
    if (FcPatternGet (pattern, FC_DPI, 0, &v) == FcResultNoMatch)
    {
	dpi = (((double) DisplayHeight (dpy, screen) * 25.4) / 
	       (double) DisplayHeightMM (dpy, screen));
	FcPatternAddDouble (pattern, FC_DPI, 
			    XftDefaultGetDouble (dpy, FC_DPI, screen, 
						 dpi));
    }
    if (FcPatternGet (pattern, FC_SCALE, 0, &v) == FcResultNoMatch)
    {
	FcPatternAddDouble (pattern, FC_SCALE,
			    XftDefaultGetDouble (dpy, FC_SCALE, screen, 1.0));
    }
    if (FcPatternGet (pattern, XFT_MAX_GLYPH_MEMORY, 0, &v) == FcResultNoMatch)
    {
	FcPatternAddInteger (pattern, XFT_MAX_GLYPH_MEMORY,
			     XftDefaultGetInteger (dpy, XFT_MAX_GLYPH_MEMORY,
						   screen,
						   XFT_FONT_MAX_GLYPH_MEMORY));
    }
    FcDefaultSubstitute (pattern);
}

