/*
 * $XFree86: xc/lib/Xft/Xft.h,v 1.22 2002/02/21 05:30:31 keithp Exp $
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

#ifndef _XFT_H_
#define _XFT_H_

#define XftVersion  20000

#include <stdarg.h>
#include <freetype/freetype.h>
#include <fontconfig/fontconfig.h>
#include <X11/extensions/Xrender.h>

#include <X11/Xfuncproto.h>
/* #include <X11/Xosdefs.h>*/

#ifndef _XFT_NO_COMPAT_
#include <X11/Xft/XftCompat.h>
#endif

#define XFT_CORE		"core"
#define XFT_RENDER		"render"
#define XFT_XLFD		"xlfd"
#define XFT_MAX_GLYPH_MEMORY	"maxglyphmemory"

extern FT_Library	_XftFTlibrary;

typedef struct _XftFont {
    int		ascent;
    int		descent;
    int		height;
    int		max_advance_width;
    FcCharSet	*charset;
    FcPattern	*pattern;
} XftFont;

typedef struct _XftDraw XftDraw;

typedef struct _XftColor {
    unsigned long   pixel;
    XRenderColor    color;
} XftColor;

typedef struct _XftCharSpec {
    FcChar32	    ucs4;
    short	    x;
    short	    y;
} XftCharSpec;

typedef struct _XftCharFontSpec {
    XftFont	    *font;
    FcChar32	    ucs4;
    short	    x;
    short	    y;
} XftCharFontSpec;

typedef struct _XftGlyphSpec {
    FT_UInt	    glyph;
    short	    x;
    short	    y;
} XftGlyphSpec;

typedef struct _XftGlyphFontSpec {
    XftFont	    *font;
    FT_UInt	    glyph;
    short	    x;
    short	    y;
} XftGlyphFontSpec;

_XFUNCPROTOBEGIN

    
/* xftcolor.c */
Bool
XftColorAllocName (Display  *dpy,
		   Visual   *visual,
		   Colormap cmap,
		   char	    *name,
		   XftColor *result);

Bool
XftColorAllocValue (Display	    *dpy,
		    Visual	    *visual,
		    Colormap	    cmap,
		    XRenderColor    *color,
		    XftColor	    *result);

void
XftColorFree (Display	*dpy,
	      Visual	*visual,
	      Colormap	cmap,
	      XftColor	*color);


/* xftcore.c */

/* xftdir.c */
FcBool
XftDirScan (FcFontSet *set, const char *dir, FcBool force);

FcBool
XftDirSave (FcFontSet *set, const char *dir);

/* xftdpy.c */
Bool
XftDefaultHasRender (Display *dpy);
    
Bool
XftDefaultSet (Display *dpy, FcPattern *defaults);

void
XftDefaultSubstitute (Display *dpy, int screen, FcPattern *pattern);
    
/* xftdraw.c */

XftDraw *
XftDrawCreate (Display   *dpy,
	       Drawable  drawable,
	       Visual    *visual,
	       Colormap  colormap);

XftDraw *
XftDrawCreateBitmap (Display  *dpy,
		     Pixmap   bitmap);

XftDraw *
XftDrawCreateAlpha (Display *dpy, 
		    Pixmap  pixmap,
		    int	    depth);

void
XftDrawChange (XftDraw	*draw,
	       Drawable	drawable);

Display *
XftDrawDisplay (XftDraw *draw);

Drawable
XftDrawDrawable (XftDraw *draw);

Colormap
XftDrawColormap (XftDraw *draw);

Visual *
XftDrawVisual (XftDraw *draw);

void
XftDrawDestroy (XftDraw	*draw);

Picture
XftDrawPicture (XftDraw *draw);

void
XftDrawGlyphs (XftDraw	*draw,
	       XftColor	*color,
	       XftFont	*pub,
	       int	x,
	       int	y,
	       FT_UInt	*glyphs,
	       int	nglyphs);

void
XftDrawString8 (XftDraw		*d,
		XftColor	*color,
		XftFont		*font,
		int		x, 
		int		y,
		FcChar8	*string,
		int		len);

void
XftDrawString16 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*font,
		 int		x,
		 int		y,
		 FcChar16	*string,
		 int		len);

void
XftDrawString32 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*font,
		 int		x,
		 int		y,
		 FcChar32	*string,
		 int		len);

void
XftDrawStringUtf8 (XftDraw	*d,
		   XftColor	*color,
		   XftFont	*font,
		   int		x, 
		   int		y,
		   FcChar8	*string,
		   int		len);

void
XftDrawCharSpec (XftDraw	*d,
		 XftColor	*color,
		 XftFont	*font,
		 XftCharSpec	*chars,
		 int		len);

void
XftDrawCharFontSpec (XftDraw		*d,
		     XftColor		*color,
		     XftCharFontSpec	*chars,
		     int		len);

void
XftDrawGlyphSpec (XftDraw	*d,
		  XftColor	*color,
		  XftFont	*font,
		  XftGlyphSpec	*glyphs,
		  int		len);

void
XftDrawGlyphFontSpec (XftDraw		*d,
		      XftColor		*color,
		      XftGlyphFontSpec	*glyphs,
		      int		len);

void
XftDrawRect (XftDraw	    *d,
	     XftColor	    *color,
	     int	    x, 
	     int	    y,
	     unsigned int   width,
	     unsigned int   height);


Bool
XftDrawSetClip (XftDraw	    *d,
		Region	    r);

/* xftextent.c */

void
XftGlyphExtents (Display	*dpy,
		 XftFont	*pub,
		 FT_UInt	*glyphs,
		 int		nglyphs,
		 XGlyphInfo	*extents);

void
XftTextExtents8 (Display	*dpy,
		 XftFont	*font,
		 FcChar8	*string, 
		 int		len,
		 XGlyphInfo	*extents);

void
XftTextExtents16 (Display	    *dpy,
		  XftFont	    *font,
		  FcChar16	    *string, 
		  int		    len,
		  XGlyphInfo	    *extents);

void
XftTextExtents32 (Display	*dpy,
		  XftFont	*font,
		  FcChar32	*string, 
		  int		len,
		  XGlyphInfo	*extents);
    
void
XftTextExtentsUtf8 (Display	*dpy,
		    XftFont	*font,
		    FcChar8	*string, 
		    int		len,
		    XGlyphInfo	*extents);

/* xftfont.c */
FcPattern *
XftFontMatch (Display *dpy, int screen, FcPattern *pattern, FcResult *result);

XftFont *
XftFontOpen (Display *dpy, int screen, ...);

XftFont *
XftFontOpenName (Display *dpy, int screen, const char *name);

XftFont *
XftFontOpenXlfd (Display *dpy, int screen, const char *xlfd);

/* xftfreetype.c */

FT_Face
XftLockFace (XftFont *font);

void
XftUnlockFace (XftFont *font);

XftFont *
XftFontOpenPattern (Display *dpy, FcPattern *pattern);

XftFont *
XftFontCopy (Display *dpy, XftFont *font);

void
XftFontClose (Display *dpy, XftFont *font);

FcBool
XftInitFtLibrary(void);

/* xftglyphs.c */
void
XftFontLoadGlyphs (Display	*dpy,
		   XftFont	*font,
		   FcBool	need_bitmaps,
		   FT_UInt	*glyphs,
		   int		nglyph);

void
XftFontUnloadGlyphs (Display	*dpy,
		     XftFont	*pub,
		     FT_UInt	*glyphs,
		     int	nglyph);

#define XFT_NMISSING		256

FcBool
XftFontCheckGlyph (Display  *dpy,
		   XftFont  *font,
		   FcBool   need_bitmaps,
		   FT_UInt  glyph,
		   FT_UInt  *missing,
		   int	    *nmissing);

FcBool
XftCharExists (Display	    *dpy,
	       XftFont	    *pub,
	       FcChar32    ucs4);
    
FT_UInt
XftCharIndex (Display	    *dpy, 
	      XftFont	    *pub,
	      FcChar32	    ucs4);
    
/* xftgram.y */

/* xftinit.c */
FcBool
XftInit (char *config);

/* xftlex.l */

/* xftlist.c */

FcFontSet *
XftListFonts (Display	*dpy,
	      int	screen,
	      ...);

/* xftmatch.c */

/* xftmatrix.c */

/* xftname.c */
FcPattern 
*XftNameParse (const char *name);

/* xftpat.c */

/* xftrender.c */
void
XftGlyphRender (Display	    *dpy,
		int	    op,
		Picture	    src,
		XftFont	    *pub,
		Picture	    dst,
		int	    srcx,
		int	    srcy,
		int	    x,
		int	    y,
		FT_UInt	    *glyphs,
		int	    nglyphs);

void
XftGlyphSpecRender (Display	    *dpy,
		    int		    op,
		    Picture	    src,
		    XftFont	    *pub,
		    Picture	    dst,
		    int		    srcx,
		    int		    srcy,
		    XftGlyphSpec    *glyphs,
		    int		    nglyphs);

void
XftCharSpecRender (Display	    *dpy,
		   int		    op,
		   Picture	    src,
		   XftFont	    *pub,
		   Picture	    dst,
		   int		    srcx, 
		   int		    srcy,
		   XftCharSpec	    *chars,
		   int		    len);

void
XftGlyphFontSpecRender (Display		    *dpy,
			int		    op,
			Picture		    src,
			Picture		    dst,
			int		    srcx,
			int		    srcy,
			XftGlyphFontSpec    *glyphs,
			int		    nglyphs);

void
XftCharFontSpecRender (Display		    *dpy,
		       int		    op,
		       Picture		    src,
		       Picture		    dst,
		       int		    srcx,
		       int		    srcy,
		       XftCharFontSpec	    *chars,
		       int		    len);

void
XftTextRender8 (Display *dpy,
		int	op,
		Picture	src,
		XftFont	*pub,
		Picture	dst,
		int	srcx,
		int	srcy,
		int	x,
		int	y,
		FcChar8	*string,
		int	len);

void
XftTextRender16 (Display    *dpy,
		 int	    op,
		 Picture    src,
		 XftFont    *pub,
		 Picture    dst,
		 int	    srcx,
		 int	    srcy,
		 int	    x,
		 int	    y,
		 FcChar16   *string,
		 int	    len);

void
XftTextRender16BE (Display  *dpy,
		   int	    op,
		   Picture  src,
		   XftFont  *pub,
		   Picture  dst,
		   int	    srcx,
		   int	    srcy,
		   int	    x,
		   int	    y,
		   FcChar8  *string,
		   int	    len);

void
XftTextRender16LE (Display  *dpy,
		   int	    op,
		   Picture  src,
		   XftFont  *pub,
		   Picture  dst,
		   int	    srcx,
		   int	    srcy,
		   int	    x,
		   int	    y,
		   FcChar8  *string,
		   int	    len);

void
XftTextRender32 (Display    *dpy,
		 int	    op,
		 Picture    src,
		 XftFont    *pub,
		 Picture    dst,
		 int	    srcx,
		 int	    srcy,
		 int	    x,
		 int	    y,
		 FcChar32   *string,
		 int	    len);

void
XftTextRender32BE (Display  *dpy,
		   int	    op,
		   Picture  src,
		   XftFont  *pub,
		   Picture  dst,
		   int	    srcx,
		   int	    srcy,
		   int	    x,
		   int	    y,
		   FcChar8  *string,
		   int	    len);

void
XftTextRender32LE (Display  *dpy,
		   int	    op,
		   Picture  src,
		   XftFont  *pub,
		   Picture  dst,
		   int	    srcx,
		   int	    srcy,
		   int	    x,
		   int	    y,
		   FcChar8  *string,
		   int	    len);

void
XftTextRenderUtf8 (Display  *dpy,
		   int	    op,
		   Picture  src,
		   XftFont  *pub,
		   Picture  dst,
		   int	    srcx,
		   int	    srcy,
		   int	    x,
		   int	    y,
		   FcChar8  *string,
		   int	    len);

/* xftstr.c */

/* xftxlfd.c */
FcPattern *
XftXlfdParse (const char *xlfd_orig, Bool ignore_scalable, Bool complete);
    
XFontStruct *
XftCoreOpen (Display *dpy, FcPattern *pattern);

void
XftCoreClose (Display *dpy, XFontStruct *font);

_XFUNCPROTOEND

#endif /* _XFT_H_ */
