/*
 * $XFree86: xc/lib/Xft/xftfreetype.c,v 1.18 2002/02/19 07:56:29 keithp Exp $
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
#include <stdio.h>
#include <string.h>
#include "xftint.h"
#include <X11/Xlibint.h>

FT_Library  _XftFTlibrary;

#define FT_Matrix_Equal(a,b)	((a)->xx == (b)->xx && \
				 (a)->yy == (b)->yy && \
				 (a)->xy == (b)->xy && \
				 (a)->yx == (b)->yx)
/*
 * List of all open files (each face in a file is managed separately)
 */

static XftFtFile *_XftFtFiles;
int XftMaxFreeTypeFiles = 5;

static XftFtFile *
_XftGetFile (const FcChar8 *file, int id)
{
    XftFtFile	*f;

    if (!XftInitFtLibrary ())
	return 0;

    for (f = _XftFtFiles; f; f = f->next)
    {
	if (!strcmp (f->file, (FcChar8 *) file) && f->id == id)
	{
	    ++f->ref;
	    if (XftDebug () & XFT_DBG_REF)
		printf ("FontFile %s/%d matches existing (%d)\n",
			file, id, f->ref);
	    return f;
	}
    }
    f = malloc (sizeof (XftFtFile) + strlen ((char *) file) + 1);
    if (!f)
	return 0;
    
    XftMemAlloc (XFT_MEM_FILE, sizeof (XftFtFile) + strlen ((char *) file) + 1);
    if (XftDebug () & XFT_DBG_REF)
    	printf ("FontFile %s/%d matches new\n",
		file, id);
    f->next = _XftFtFiles;
    _XftFtFiles = f;
    
    f->ref = 1;
    
    f->file = (char *) (f+1);
    strcpy (f->file, file);
    f->id = id;
    
    f->lock = 0;
    f->face = 0;
    f->size = 0;
    return f;
}

static int
_XftNumFiles (void)
{
    XftFtFile	*f;
    int		count = 0;
    for (f = _XftFtFiles; f; f = f->next)
	if (f->face && !f->lock)
	    ++count;
    return count;
}

static XftFtFile *
_XftNthFile (int n)
{
    XftFtFile	*f;
    int		count = 0;
    for (f = _XftFtFiles; f; f = f->next)
	if (f->face && !f->lock)
	    if (count++ == n)
		break;
    return f;
}

static void
_XftUncacheFiles (void)
{
    int		n;
    XftFtFile	*f;
    while ((n = _XftNumFiles ()) > XftMaxFreeTypeFiles)
    {
	f = _XftNthFile (rand () % n);
	if (f)
	{
	    if (XftDebug() & XFT_DBG_REF)
		printf ("Discard file %s/%d from cache\n",
			f->file, f->id);
	    FT_Done_Face (f->face);
	    f->face = 0;
	}
    }
}

static FT_Face
_XftLockFile (XftFtFile *f)
{
    ++f->lock;
    if (!f->face)
    {
	if (XftDebug() & XFT_DBG_REF)
	    printf ("Loading file %s/%d\n", f->file, f->id);
	if (FT_New_Face (_XftFTlibrary, f->file, f->id, &f->face))
	    --f->lock;
	    
	f->size = 0;
	f->matrix.xx = f->matrix.xy = f->matrix.yx = f->matrix.yy = 0;
	_XftUncacheFiles ();
    }
    return f->face;
}

static void
_XftLockError (char *reason)
{
    fprintf (stderr, "Xft: locking error %s\n", reason);
}

static void
_XftUnlockFile (XftFtFile *f)
{
    if (--f->lock < 0)
	_XftLockError ("too many file unlocks");
}

FcBool
_XftSetFace (XftFtFile *f, FT_F26Dot6 size, FT_Matrix *matrix)
{
    FT_Face face = f->face;
    
    if (f->size != size)
    {
	if (XftDebug() & XFT_DBG_GLYPH)
	    printf ("Set face size to %d (%d)\n", 
		    (int) (size >> 6), (int) size);
	if (FT_Set_Char_Size (face, size, size, 0, 0))
	    return False;
	f->size = size;
    }
    if (!FT_Matrix_Equal (&f->matrix, matrix))
    {
	if (XftDebug() & XFT_DBG_GLYPH)
	    printf ("Set face matrix to (%g,%g,%g,%g)\n",
		    (double) matrix->xx / 0x10000,
		    (double) matrix->xy / 0x10000,
		    (double) matrix->yx / 0x10000,
		    (double) matrix->yy / 0x10000);
	FT_Set_Transform (face, matrix, 0);
	f->matrix = *matrix;
    }
    return True;
}

static void
_XftReleaseFile (XftFtFile *f)
{
    XftFtFile	**prev;
    
    if (--f->ref != 0)
	return;
    if (f->lock)
	_XftLockError ("Attempt to close locked file");
    for (prev = &_XftFtFiles; *prev; prev = &(*prev)->next)
    {
	if (*prev == f)
	{
	    *prev = f->next;
	    break;
	}
    }
    if (f->face)
	FT_Done_Face (f->face);
    XftMemFree (XFT_MEM_FILE, sizeof (XftFtFile) + strlen (f->file) + 1);
    free (f);
}

/*
 * Find a prime larger than the minimum reasonable hash size
 */

static FcChar32
_XftSqrt (FcChar32 a)
{
    FcChar32	    l, h, m;

    l = 2;
    h = a/2;
    while ((h-l) > 1)
    {
	m = (h+l) >> 1;
	if (m * m < a)
	    l = m;
	else
	    h = m;
    }
    return h;
}

static FcBool
_XftIsPrime (FcChar32 i)
{
    FcChar32	l, t;

    if (i < 2)
	return FcFalse;
    if ((i & 1) == 0)
    {
	if (i == 2)
	    return FcTrue;
	return FcFalse;
    }
    l = _XftSqrt (i) + 1;
    for (t = 3; t <= l; t += 2)
	if (i % t == 0)
	    return FcFalse;
    return FcTrue;
}

static FcChar32
_XftHashSize (FcChar32 num_unicode)
{
    /* at least 31.25 % extra space */
    FcChar32	hash = num_unicode + (num_unicode >> 2) + (num_unicode >> 4);

    if ((hash & 1) == 0)
	hash++;
    while (!_XftIsPrime (hash))
	hash += 2;
    return hash;
}

FT_Face
XftLockFace (XftFont *public)
{
    XftFontInt	*font = (XftFontInt *) public;
    return _XftLockFile (font->file);
}

void
XftUnlockFace (XftFont *public)
{
    XftFontInt	*font = (XftFontInt *) public;
    _XftUnlockFile (font->file);
}

XftFont *
XftFontOpenPattern (Display *dpy, FcPattern *pattern)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    XftFtFile	    *file;
    FT_Face	    face;
    FcChar8	    *filename;
    int		    id;
    double	    dsize;
    FT_F26Dot6	    size;
    int		    spacing;
    int		    char_width;
    Bool	    minspace;
    XftFont	    *public;
    XftFontInt	    *font;
    FcBool	    render;
    int		    i;
    FcCharSet	    *charset;
    FcChar32	    num_unicode;
    FcChar32	    hash_value;
    FcChar32	    rehash_value;
    int		    max_glyph_memory;
    int		    alloc_size;
    
    /*
     * Font information placed in XftFontInt
     */
    FcBool	    antialias;
    int		    rgba;
    FcBool	    transform;
    FT_Matrix	    matrix;
    FT_Int	    load_flags;
    FcBool	    hinting, vertical_layout, autohint, global_advance;
    
    FcMatrix	    *font_matrix;

    int		    height, ascent, descent;
    XRenderPictFormat	pf, *format;
    
    if (!info)
	goto bail0;

    /*
     * Find the associated file
     */
    if (FcPatternGetString (pattern, FC_FILE, 0, &filename) != FcResultMatch)
	goto bail0;
    
    if (FcPatternGetInteger (pattern, FC_INDEX, 0, &id) != FcResultMatch)
	goto bail0;
    
    file = _XftGetFile (filename, id);
    if (!file)
	goto bail0;
    
    face = _XftLockFile (file);
    if (!face)
	goto bail1;

    /*
     * Extract the glyphset information from the pattern
     */
    if (FcPatternGetDouble (pattern, FC_PIXEL_SIZE, 0, &dsize) != FcResultMatch)
	goto bail2;
    
    switch (FcPatternGetBool (pattern, FC_MINSPACE, 0, &minspace)) {
    case FcResultNoMatch:
	minspace = False;
	break;
    case FcResultMatch:
	break;
    default:
	goto bail2;
    }
    
    switch (FcPatternGetInteger (pattern, FC_SPACING, 0, &spacing)) {
    case FcResultNoMatch:
	spacing = FC_PROPORTIONAL;
	break;
    case FcResultMatch:
	break;
    default:
	goto bail2;
    }

    if (FcPatternGetInteger (pattern, FC_CHAR_WIDTH, 
			      0, &char_width) != FcResultMatch)
    {
	char_width = 0;
    }
    else if (char_width)
	spacing = FC_MONO;

    if (face->face_flags & FT_FACE_FLAG_SCALABLE)
    {
	switch (FcPatternGetBool (pattern, FC_ANTIALIAS, 0, &antialias)) {
	case FcResultNoMatch:
	    antialias = True;
	    break;
	case FcResultMatch:
	    break;
	default:
	    goto bail2;
	}
    }
    else
	antialias = False;
    
    switch (FcPatternGetInteger (pattern, FC_RGBA, 0, &rgba)) {
    case FcResultNoMatch:
	rgba = FC_RGBA_NONE;
	break;
    case FcResultMatch:
	break;
    default:
	goto bail2;
    }
    
    matrix.xx = matrix.yy = 0x10000;
    matrix.xy = matrix.yx = 0;
    
    switch (FcPatternGetMatrix (pattern, FC_MATRIX, 0, &font_matrix)) {
    case FcResultNoMatch:
	break;
    case FcResultMatch:
	matrix.xx = 0x10000L * font_matrix->xx;
	matrix.yy = 0x10000L * font_matrix->yy;
	matrix.xy = 0x10000L * font_matrix->xy;
	matrix.yx = 0x10000L * font_matrix->yx;
	break;
    default:
	goto bail2;
    }
    
    transform = (matrix.xx != 0x10000 || matrix.xy != 0 ||
		 matrix.yx != 0 || matrix.yy != 0x10000);
    
    if (FcPatternGetCharSet (pattern, FC_CHARSET, 0, &charset) != FcResultMatch)
    {
	charset = 0;
    }

    if (FcPatternGetInteger (pattern, FC_CHAR_WIDTH, 
			      0, &char_width) != FcResultMatch)
    {
	char_width = 0;
    }
    else if (char_width)
	spacing = FC_MONO;

    switch (FcPatternGetBool (pattern, XFT_RENDER, 0, &render)) {
    case FcResultNoMatch:
	render = info->hasRender;
	break;
    case FcResultMatch:
	if (render && !info->hasRender)
	    render = FcFalse;
	break;
    default:
	goto bail2;
    }
    
    /*
     * Compute glyph load flags
     */
    load_flags = FT_LOAD_DEFAULT;

    /* disable bitmaps when anti-aliasing or transforming glyphs */
    if (antialias || transform)
	load_flags |= FT_LOAD_NO_BITMAP;

    /* disable hinting if requested */
    switch (FcPatternGetBool (pattern, FC_HINTING, 0, &hinting)) {
    case FcResultNoMatch:
	hinting = FcTrue;
	break;
    case FcResultMatch:
	break;
    default:
	goto bail2;
    }

    if (!hinting)
	load_flags |= FT_LOAD_NO_HINTING;
    
    /* set vertical layout if requested */
    switch (FcPatternGetBool (pattern, FC_VERTICAL_LAYOUT, 0, &vertical_layout)) {
    case FcResultNoMatch:
	vertical_layout = FcFalse;
	break;
    case FcResultMatch:
	break;
    default:
	goto bail2;
    }

    if (vertical_layout)
	load_flags |= FT_LOAD_VERTICAL_LAYOUT;

    /* force autohinting if requested */
    switch (FcPatternGetBool (pattern, FC_AUTOHINT, 0, &autohint)) {
    case FcResultNoMatch:
	autohint = FcFalse;
	break;
    case FcResultMatch:
	break;
    default:
	goto bail2;
    }

    if (autohint)
	load_flags |= FT_LOAD_FORCE_AUTOHINT;

    /* disable global advance width (for broken DynaLab TT CJK fonts) */
    switch (FcPatternGetBool (pattern, FC_GLOBAL_ADVANCE, 0, &global_advance)) {
    case FcResultNoMatch:
	global_advance = FcTrue;
	break;
    case FcResultMatch:
	break;
    default:
	goto bail2;
    }

    if (!global_advance)
	load_flags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
    
    if (FcPatternGetInteger (pattern, XFT_MAX_GLYPH_MEMORY, 
			     0, &max_glyph_memory) != FcResultMatch)
    {
	max_glyph_memory = XFT_FONT_MAX_GLYPH_MEMORY;
    }

    size = (FT_F26Dot6) (dsize * 64.0);
    
    /*
     * Match an existing font
     */
    for (public = info->fonts; public; public = font->next)
    {
	font = (XftFontInt *) public;
	if (font->file == file &&
	    font->minspace == minspace &&
	    font->char_width == char_width &&
	    font->size == size &&
	    font->spacing == spacing &&
	    font->rgba == rgba &&
	    font->antialias == antialias &&
	    font->load_flags == load_flags &&
	    (font->format != 0) == render &&
	    FT_Matrix_Equal (&font->matrix, &matrix))
	{
	    ++font->ref;
	    if (XftDebug () & XFT_DBG_REF)
	    {
		printf ("Face size %g matches existing (%d)\n",
			dsize, font->ref);
	    }
	    _XftUnlockFile (file);
	    _XftReleaseFile (file);
	    FcPatternDestroy (pattern);
	    return &font->public;
	}
    }
    
    if (XftDebug () & XFT_DBG_REF)
	printf ("Face size %g matches new\n", dsize);
    
    /*
     * No existing glyphset, create another.  
     */
    if (render)
    {
	if (antialias)
	{
	    if (rgba)
	    {
		pf.depth = 32;
		pf.type = PictTypeDirect;
		pf.direct.alpha = 24;
		pf.direct.alphaMask = 0xff;
		pf.direct.red = 16;
		pf.direct.redMask = 0xff;
		pf.direct.green = 8;
		pf.direct.greenMask = 0xff;
		pf.direct.blue = 0;
		pf.direct.blueMask = 0xff;
		format = XRenderFindFormat(dpy, 
					   PictFormatType|
					   PictFormatDepth|
					   PictFormatAlpha|
					   PictFormatAlphaMask|
					   PictFormatRed|
					   PictFormatRedMask|
					   PictFormatGreen|
					   PictFormatGreenMask|
					   PictFormatBlue|
					   PictFormatBlueMask,
					   &pf, 0);
	    }
	    else
	    {
		pf.depth = 8;
		pf.type = PictTypeDirect;
		pf.direct.alpha = 0;
		pf.direct.alphaMask = 0xff;
		format = XRenderFindFormat(dpy, 
					   PictFormatType|
					   PictFormatDepth|
					   PictFormatAlpha|
					   PictFormatAlphaMask,
					   &pf, 0);
	    }
	}
	else
	{
	    pf.depth = 1;
	    pf.type = PictTypeDirect;
	    pf.direct.alpha = 0;
	    pf.direct.alphaMask = 0x1;
	    format = XRenderFindFormat(dpy, 
				       PictFormatType|
				       PictFormatDepth|
				       PictFormatAlpha|
				       PictFormatAlphaMask,
				       &pf, 0);
	}
	
	if (!format)
	    goto bail2;
    }
    else
	format = 0;
    
    if (!_XftSetFace (file, size, &matrix))
	goto bail2;

    if (charset)
    {
	num_unicode = FcCharSetCount (charset);
	hash_value = _XftHashSize (num_unicode);
	rehash_value = hash_value - 2;
    }
    else
    {
	num_unicode = 0;
	hash_value = 0;
	rehash_value = 0;
    }
    
    alloc_size = (sizeof (XftFontInt) + 
		  face->num_glyphs * sizeof (XftGlyph *) +
		  hash_value * sizeof (XftUcsHash));
    font = malloc (alloc_size);
    
    if (!font)
	goto bail2;

    XftMemAlloc (XFT_MEM_FONT, alloc_size);

    /*
     * Public fields
     */
    descent = -(face->size->metrics.descender >> 6);
    ascent = face->size->metrics.ascender >> 6;
    if (minspace)
    {
	height = ascent + descent;
    }
    else
    {
	height = face->size->metrics.height >> 6;
    }
    font->public.ascent = ascent;
    font->public.descent = descent;
    font->public.height = height;
    
    if (char_width)
	font->public.max_advance_width = char_width;
    else
	font->public.max_advance_width = face->size->metrics.max_advance >> 6;
    font->public.charset = charset;
    font->public.pattern = pattern;
    
    /*
     * Management fields
     */
    font->next = info->fonts;
    info->fonts = &font->public;
    font->file = file;
    font->ref = 1;
    /*
     * Per glyph information
     */
    font->glyphs = (XftGlyph **) (font + 1);
    memset (font->glyphs, '\0', face->num_glyphs * sizeof (XftGlyph *));
    font->num_glyphs = face->num_glyphs;
    /*
     * Unicode hash table information
     */
    font->hash_table = (XftUcsHash *) (font->glyphs + font->num_glyphs);
    for (i = 0; i < hash_value; i++)
    {
	font->hash_table[i].ucs4 = ((FcChar32) ~0);
	font->hash_table[i].glyph = 0;
    }
    font->hash_value = hash_value;
    font->rehash_value = rehash_value;
    /*
     * X specific fields
     */
    font->glyphset = 0;
    font->format = format;
    /*
     * Rendering options
     */
    font->size = size;
    font->antialias = antialias;
    font->rgba = rgba;
    font->transform = transform;
    font->matrix = matrix;
    font->load_flags = load_flags;
    font->minspace = minspace;
    font->char_width = char_width;
    font->spacing = spacing;
    /*
     * Glyph memory management fields
     */
    font->glyph_memory = 0;
    font->max_glyph_memory = max_glyph_memory;
    font->use_free_glyphs = info->use_free_glyphs;
    
    _XftUnlockFile (file);

    return &font->public;
    
bail2:
    _XftUnlockFile (file);
bail1:
    _XftReleaseFile (file);
bail0:
    return 0;
}

XftFont *
XftFontCopy (Display *dpy, XftFont *public)
{
    XftFontInt	    *font = (XftFontInt *) public;

    font->ref++;
    return public;
}

void
XftFontClose (Display *dpy, XftFont *public)
{
    XftFontInt	    *font = (XftFontInt *) public;
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    XftFont	    **prev;
    int		    i;

    if (--font->ref == 0)
    {
	if (info)
	{
	    /* Unhook from display list */
	    for (prev = &info->fonts; *prev; prev = &(*(XftFontInt **) prev)->next)
	    {
		if (*prev == public)
		{
		    *prev = font->next;
		    break;
		}
	    }
	}
	/* Free resources */
	/* Dereference the file */
	_XftReleaseFile (font->file);
	/* Free the glyphset */
	if (font->glyphset)
	    XRenderFreeGlyphSet (dpy, font->glyphset);
	/* Free the glyphs */
	for (i = 0; i < font->num_glyphs; i++)
	{
	    XftGlyph	*xftg = font->glyphs[i];
	    if (xftg)
	    {
		if (xftg->bitmap)
		    free (xftg->bitmap);
		free (xftg);
	    }
	}
	
	/* Finally, free the font structure */
	XftMemFree (XFT_MEM_FONT, sizeof (XftFontInt) +
		    font->num_glyphs * sizeof (XftGlyph *) +
		    font->hash_value * sizeof (XftUcsHash));
	free (font);
    }
}

FcBool
XftInitFtLibrary (void)
{
    if (_XftFTlibrary)
	return FcTrue;
    if (FT_Init_FreeType (&_XftFTlibrary))
	return FcFalse;
    return FcTrue;
}
