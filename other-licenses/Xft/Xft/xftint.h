/*
 * $XFree86: xc/lib/Xft/xftint.h,v 1.29 2002/02/15 07:36:11 keithp Exp $
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
 * These definitions are solely for use by the implementation of Xft
 * and constitute no kind of standard.  If you need any of these functions,
 * please drop me a note.  Either the library needs new functionality,
 * or there's a way to do what you need using the existing published
 * interfaces. keithp@xfree86.org
 */

#ifndef _XFTINT_H_
#define _XFTINT_H_

#include <X11/Xlib.h>
#include <X11/Xmd.h>
#define _XFT_NO_COMPAT_
#include "Xft.h"
#include <fontconfig/fcprivate.h>

typedef struct _XftMatcher {
    char    *object;
    double  (*compare) (char *object, FcValue value1, FcValue value2);
} XftMatcher;

typedef struct _XftSymbolic {
    const char	*name;
    int		value;
} XftSymbolic;

/*
 * Glyphs are stored in this structure
 */
typedef struct _XftGlyph {
    XGlyphInfo	    metrics;
    void	    *bitmap;
    unsigned long   glyph_memory;
} XftGlyph;

/*
 * A hash table translates Unicode values into glyph indicies
 */
typedef struct _XftUcsHash {
    FcChar32	    ucs4;
    FT_UInt	    glyph;
} XftUcsHash;

/*
 * Many fonts can share the same underlying face data; this
 * structure references that.  Note that many faces may in fact
 * live in the same font file; that is irrelevant to this structure
 * which is concerned only with the individual faces themselves
 */

typedef struct _XftFtFile {
    struct _XftFtFile	*next;
    int			ref;	    /* number of fonts using this file */
    
    char		*file;	    /* file name */
    int			id;	    /* font index within that file */

    FT_F26Dot6		size;	    /* current size setting */
    FT_Matrix		matrix;	    /* current matrix setting */
    
    int			lock;	    /* lock count; can't unload unless 0 */
    FT_Face		face;	    /* pointer to face; only valid when lock */
} XftFtFile;

/*
 * Internal version of the font with private data
 */

typedef struct _XftFontInt {
    XftFont		public;		/* public fields */
    XftFont		*next;		/* list of fonts for this display */
    XftFtFile		*file;		/* Associated free type file */
    int			ref;		/* reference count */
    /*
     * Per-glyph information, indexed by glyph ID
     * This array follows the font in memory
     */
    XftGlyph		**glyphs;
    int			num_glyphs;	/* size of glyphs/bitmaps arrays */
    /*
     * Hash table to get from Unicode value to glyph ID
     * This array follows the glyphs in memory
     */
    XftUcsHash		*hash_table;
    int			hash_value;
    int			rehash_value;
    /*
     * X specific fields
     */
    GlyphSet		glyphset;	/* Render glyphset */
    XRenderPictFormat	*format;	/* Render format for glyphs */
    /*
     * Rendering options
     */
    FT_F26Dot6		size;
    FcBool		antialias;	/* doing antialiasing */
    int			rgba;		/* subpixel order */
    FcBool		transform;	/* non-identity matrix */
    FT_Matrix		matrix;		/* glyph transformation matrix */
    FT_Int		load_flags;	/* glyph load flags */
    /*
     * Internal fields
     */
    FcBool		minspace;
    int			char_width;
    int			spacing;
    unsigned long	glyph_memory;
    unsigned long	max_glyph_memory;
    FcBool		use_free_glyphs;   /* Use XRenderFreeGlyphs */
} XftFontInt;

struct _XftDraw {
    Display	    *dpy;
    int		    screen;
    unsigned int    bits_per_pixel;
    unsigned int    depth;
    Drawable	    drawable;
    Visual	    *visual;	/* NULL for bitmaps */
    Colormap	    colormap;
    Region	    clip;
    struct {
	Picture		pict;
    } render;
    struct {
	GC		gc;
	int		use_pixmap;
    } core;
};

/*
 * Instead of taking two round trips for each blending request,
 * assume that if a particular drawable fails GetImage that it will
 * fail for a "while"; use temporary pixmaps to avoid the errors
 */

#define XFT_ASSUME_PIXMAP	20

typedef struct _XftSolidColor {
    XRenderColor    color;
    int		    screen;
    Picture	    pict;
} XftSolidColor;

#define XFT_NUM_SOLID_COLOR	16

typedef struct _XftDisplayInfo {
    struct _XftDisplayInfo  *next;
    Display		    *display;
    XExtCodes		    *codes;
    FcPattern		    *defaults;
    FcBool		    hasRender;
    XftFont		    *fonts;
    XRenderPictFormat	    *solidFormat;
    XftSolidColor	    colors[XFT_NUM_SOLID_COLOR];
    unsigned long	    glyph_memory;
    unsigned long	    max_glyph_memory;
    FcBool		    use_free_glyphs;
} XftDisplayInfo;

/*
 * By default, use no more than 4 meg of server memory total, and no
 * more than 1 meg for any one font
 */
#define XFT_DPY_MAX_GLYPH_MEMORY    (4 * 1024 * 1024)
#define XFT_FONT_MAX_GLYPH_MEMORY   (1024 * 1024)

extern XftDisplayInfo	*_XftDisplayInfo;

#define XFT_DBG_OPEN	1
#define XFT_DBG_OPENV	2
#define XFT_DBG_RENDER	4
#define XFT_DBG_DRAW	8
#define XFT_DBG_REF	16
#define XFT_DBG_GLYPH	32
#define XFT_DBG_GLYPHV	64
#define XFT_DBG_CACHE	128
#define XFT_DBG_CACHEV	256
#define XFT_DBG_MEMORY	512

#define XFT_MEM_DRAW	0
#define XFT_MEM_FONT	1
#define XFT_MEM_FILE	2
#define XFT_MEM_GLYPH	3
#define XFT_MEM_NUM	4

/* xftcompat.c */
void XftFontSetDestroy (FcFontSet *s);
FcBool XftMatrixEqual (const FcMatrix *mat1, const FcMatrix *mat2);
void XftMatrixMultiply (FcMatrix *result, FcMatrix *a, FcMatrix *b);
void XftMatrixRotate (FcMatrix *m, double c, double s);
void XftMatrixScale (FcMatrix *m, double sx, double sy);
void XftMatrixShear (FcMatrix *m, double sh, double sv);
FcPattern *XftPatternCreate (void);
void XftValueDestroy (FcValue v);
void XftPatternDestroy (FcPattern *p);
FcBool XftPatternAdd (FcPattern *p, const char *object, FcValue value, FcBool append);
FcBool XftPatternDel (FcPattern *p, const char *object);
FcBool XftPatternAddInteger (FcPattern *p, const char *object, int i);
FcBool XftPatternAddDouble (FcPattern *p, const char *object, double i);
FcBool XftPatternAddMatrix (FcPattern *p, const char *object, FcMatrix *i);
FcBool XftPatternAddString (FcPattern *p, const char *object, char *i);
FcBool XftPatternAddBool (FcPattern *p, const char *object, FcBool i);
FcResult XftPatternGet (FcPattern *p, const char *object, int id, FcValue *v);
FcResult XftPatternGetInteger (FcPattern *p, const char *object, int id, int *i);
FcResult XftPatternGetDouble (FcPattern *p, const char *object, int id, double *i);
FcResult XftPatternGetString (FcPattern *p, const char *object, int id, char **i);
FcResult XftPatternGetMatrix (FcPattern *p, const char *object, int id, FcMatrix **i);
FcResult XftPatternGetBool (FcPattern *p, const char *object, int id, FcBool *i);
FcPattern *XftPatternDuplicate (FcPattern *orig);
FcPattern *XftPatternVaBuild (FcPattern *orig, va_list va);
FcPattern *XftPatternBuild (FcPattern *orig, ...);
FcBool XftNameUnparse (FcPattern *pat, char *dest, int len);
FcBool XftGlyphExists (Display *dpy, XftFont *font, FcChar32 ucs4);
FcObjectSet *XftObjectSetCreate (void);
Bool XftObjectSetAdd (FcObjectSet *os, const char *object);
void XftObjectSetDestroy (FcObjectSet *os);
FcObjectSet *XftObjectSetVaBuild (const char *first, va_list va);
FcObjectSet *XftObjectSetBuild (const char *first, ...);
FcFontSet *XftListFontSets (FcFontSet **sets, int nsets, FcPattern *p, FcObjectSet *os);

/* xftcore.c */
void
XftRectCore (XftDraw	    *draw,
	     XftColor	    *color,
	     int	    x, 
	     int	    y,
	     unsigned int   width,
	     unsigned int   height);

void
XftGlyphCore (XftDraw	*draw,
	      XftColor	*color,
	      XftFont	*public,
	      int	x,
	      int	y,
	      FT_UInt	*glyphs,
	      int	nglyphs);

void
XftGlyphSpecCore (XftDraw	*draw,
		  XftColor	*color,
		  XftFont	*public,
		  XftGlyphSpec	*glyphs,
		  int		nglyphs);

void
XftGlyphFontSpecCore (XftDraw		*draw,
		      XftColor		*color,
		      XftGlyphFontSpec	*glyphs,
		      int		nglyphs);

/* xftdbg.c */
int
XftDebug (void);

/* xftdpy.c */
XftDisplayInfo *
_XftDisplayInfoGet (Display *dpy);

void
_XftDisplayManageMemory (Display *dpy);

int
XftDefaultParseBool (char *v);

FcBool
XftDefaultGetBool (Display *dpy, const char *object, int screen, FcBool def);

int
XftDefaultGetInteger (Display *dpy, const char *object, int screen, int def);

double
XftDefaultGetDouble (Display *dpy, const char *object, int screen, double def);

FcFontSet *
XftDisplayGetFontSet (Display *dpy);

/* xftdraw.c */
unsigned int
XftDrawDepth (XftDraw *draw);

unsigned int
XftDrawBitsPerPixel (XftDraw *draw);

FcBool
XftDrawRenderPrepare (XftDraw	*draw);

/* xftextent.c */
    
/* xftfont.c */

/* xftfreetype.c */
FcBool
_XftSetFace (XftFtFile *f, FT_F26Dot6 size, FT_Matrix *matrix);

/* xftglyph.c */
void
_XftFontUncacheGlyph (Display *dpy, XftFont *public);

void
_XftFontManageMemory (Display *dpy, XftFont *public);

/* xftinit.c */
int
XftNativeByteOrder (void);

void
XftMemReport (void);

void
XftMemAlloc (int kind, int size);

void
XftMemFree (int kind, int size);

/* xftlist.c */
FcFontSet *
XftListFontsPatternObjects (Display	    *dpy,
			    int		    screen,
			    FcPattern	    *pattern,
			    FcObjectSet    *os);

FcFontSet *
XftListFonts (Display	*dpy,
	      int	screen,
	      ...);

/* xftname.c */
void 
_XftNameInit (void);

/* xftrender.c */

/* xftstr.c */
int
_XftMatchSymbolic (XftSymbolic *s, int n, const char *name, int def);

/* xftswap.c */
int
XftNativeByteOrder (void);

void
XftSwapCARD32 (CARD32 *data, int n);

void
XftSwapCARD24 (CARD8 *data, int width, int height);

void
XftSwapCARD16 (CARD16 *data, int n);

void
XftSwapImage (XImage *image);

/* xftxlfd.c */
#endif /* _XFT_INT_H_ */
