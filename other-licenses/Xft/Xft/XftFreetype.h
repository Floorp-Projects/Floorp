/*
 * $XFree86: xc/lib/Xft/XftFreetype.h,v 1.16 2002/02/15 07:36:10 keithp Exp $
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

/*
 * This file is strictly for backwards compatibility with 
 * old Xft applications
 */

#ifndef _XFTFREETYPE_H_
#define _XFTFREETYPE_H_

/* #include <Xft/Xft.h> */
/* #include <X11/Xft/Xft.h>*/
#include "Xft.h"
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>

typedef struct _XftFontStruct {
    FT_Face             __DEPRECATED_face;      /* handle to face object */
    GlyphSet            __DEPRECATED_glyphset;
    int                 min_char;
    int                 max_char;
    FT_F26Dot6          size;
    int                 ascent;
    int                 descent;
    int                 height;
    int                 max_advance_width;
    int                 __DEPRECATED_spacing;
    int                 __DEPRECATED_rgba;
    Bool                __DEPRECATED_antialias;
    int                 __DEPRECATED_charmap;    /* -1 for unencoded */
    XRenderPictFormat   *__DEPRECATED_format;
    XGlyphInfo          **__DEPRECATED_realized;
    int                 __DEPRECATED_nrealized;
    FcBool              __DEPRECATED_transform;
    FT_Matrix           __DEPRECATED_matrix;
    /* private field */
    XftFont		*font;
} XftFontStruct;

_XFUNCPROTOBEGIN

XftFontStruct *
XftFreeTypeOpen (Display *dpy, FcPattern *pattern);

XftFontStruct *
XftFreeTypeGet (XftFont *font);

void
XftFreeTypeClose (Display *dpy, XftFontStruct *font);

void
XftGlyphLoad (Display           *dpy,
	      XftFontStruct     *font,
	      FcChar32		*glyphs,
	      int               nglyph);

void
XftGlyphCheck (Display          *dpy,
	       XftFontStruct    *font,
	       FcChar32		glyph,
	       FcChar32        *missing,
	       int              *nmissing);

void
XftGlyphLoad (Display		*dpy,
	      XftFontStruct	*font,
	      FcChar32		*glyphs,
	      int		nglyph);

void
XftGlyphCheck (Display		*dpy,
	       XftFontStruct	*font,
	       FcChar32	glyph,
	       FcChar32	*missing,
	       int		*nmissing);

Bool
XftFreeTypeGlyphExists (Display		*dpy,
			XftFontStruct	*font,
			FcChar32	glyph);

/* xftrender.c */

void
XftRenderString8 (Display *dpy, Picture src, 
		  XftFontStruct *font, Picture dst,
		  int srcx, int srcy,
		  int x, int y,
		  FcChar8 *string, int len);

void
XftRenderString16 (Display *dpy, Picture src, 
		   XftFontStruct *font, Picture dst,
		   int srcx, int srcy,
		   int x, int y,
		   FcChar16 *string, int len);

void
XftRenderString32 (Display *dpy, Picture src, 
		   XftFontStruct *font, Picture dst,
		   int srcx, int srcy,
		   int x, int y,
		   FcChar32 *string, int len);

void
XftRenderStringUtf8 (Display *dpy, Picture src, 
		     XftFontStruct *font, Picture dst,
		     int srcx, int srcy,
		     int x, int y,
		     FcChar8 *string, int len);

void
XftRenderExtents8 (Display	    *dpy,
		   XftFontStruct    *font,
		   FcChar8	    *string, 
		   int		    len,
		   XGlyphInfo	    *extents);

void
XftRenderExtents16 (Display	    *dpy,
		    XftFontStruct   *font,
		    FcChar16	    *string, 
		    int		    len,
		    XGlyphInfo	    *extents);

void
XftRenderExtents32 (Display	    *dpy,
		    XftFontStruct   *font,
		    FcChar32	    *string, 
		    int		    len,
		    XGlyphInfo	    *extents);

void
XftRenderExtentsUtf8 (Display	    *dpy,
		      XftFontStruct *font,
		      FcChar8	    *string, 
		      int	    len,
		      XGlyphInfo    *extents);

_XFUNCPROTOEND

#endif /* _XFTFREETYPE_H_ */
