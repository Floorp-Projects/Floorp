/*
 * $XFree86: xc/lib/fontconfig/fontconfig/fontconfig.h,v 1.3 2002/02/19 07:50:43 keithp Exp $
 *
 * Copyright © 2001 Keith Packard, member of The XFree86 Project, Inc.
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

#ifndef _FONTCONFIG_H_
#define _FONTCONFIG_H_

#include <stdarg.h>

typedef unsigned char	FcChar8;
typedef unsigned short	FcChar16;
typedef unsigned int	FcChar32;
typedef int		FcBool;

/*
 * Current Fontconfig version number
 */
#define FC_MAJOR	1
#define FC_MINOR	0
#define FC_REVISION	0

#define FC_VERSION	((FC_MAJOR * 10000) + (FC_MINOR * 100) + (FC_REVISION))

#define FcTrue		1
#define FcFalse		0

#define FC_FAMILY	    "family"		/* String */
#define FC_STYLE	    "style"		/* String */
#define FC_SLANT	    "slant"		/* Int */
#define FC_WEIGHT	    "weight"		/* Int */
#define FC_SIZE		    "size"		/* Double */
#define FC_PIXEL_SIZE	    "pixelsize"		/* Double */
#define FC_SPACING	    "spacing"		/* Int */
#define FC_FOUNDRY	    "foundry"		/* String */
#define FC_ANTIALIAS	    "antialias"		/* Bool (depends) */
#define FC_HINTING	    "hinting"		/* Bool (true) */
#define FC_VERTICAL_LAYOUT  "verticallayout"	/* Bool (false) */
#define FC_AUTOHINT	    "autohint"		/* Bool (false) */
#define FC_GLOBAL_ADVANCE   "globaladvance"	/* Bool (true) */
#define FC_FILE		    "file"		/* String */
#define FC_INDEX	    "index"		/* Int */
#define FC_RASTERIZER	    "rasterizer"	/* String */
#define FC_OUTLINE	    "outline"		/* Bool */
#define FC_SCALABLE	    "scalable"		/* Bool */
#define FC_SCALE	    "scale"		/* double */
#define FC_DPI		    "dpi"		/* double */
#define FC_RGBA		    "rgba"		/* Int */
#define FC_MINSPACE	    "minspace"		/* Bool use minimum line spacing */
#define FC_SOURCE	    "source"		/* String (X11, freetype) */
#define FC_CHARSET	    "charset"		/* CharSet */
#define FC_LANG		    "lang"		/* String OS/2 CodePageRange */

#define FC_DIR_CACHE_FILE	    "fonts.cache"
#define FC_USER_CACHE_FILE	    ".fonts.cache"

/* Adjust outline rasterizer */
#define FC_CHAR_WIDTH	    "charwidth"	/* Int */
#define FC_CHAR_HEIGHT	    "charheight"/* Int */
#define FC_MATRIX	    "matrix"    /* FcMatrix */

#define FC_WEIGHT_LIGHT	    0
#define FC_WEIGHT_MEDIUM    100
#define FC_WEIGHT_DEMIBOLD  180
#define FC_WEIGHT_BOLD	    200
#define FC_WEIGHT_BLACK	    210

#define FC_SLANT_ROMAN	    0
#define FC_SLANT_ITALIC	    100
#define FC_SLANT_OBLIQUE    110

#define FC_PROPORTIONAL	    0
#define FC_MONO		    100
#define FC_CHARCELL	    110

/* sub-pixel order */
#define FC_RGBA_NONE	    0
#define FC_RGBA_RGB	    1
#define FC_RGBA_BGR	    2
#define FC_RGBA_VRGB	    3
#define FC_RGBA_VBGR	    4

/* language groups from the OS/2 CodePageRange bits */
#define FC_LANG_LATIN_1			"latin1"		/* 0 */
#define FC_LANG_LATIN_2_EASTERN_EUROPE	"latin2easterneurope"	/* 1 */
#define FC_LANG_CYRILLIC		"cyrillic"		/* 2 */
#define FC_LANG_GREEK			"greek"			/* 3 */
#define FC_LANG_TURKISH			"turkish"		/* 4 */
#define FC_LANG_HEBREW			"hebrew"		/* 5 */
#define FC_LANG_ARABIC			"arabic"		/* 6 */
#define FC_LANG_WINDOWS_BALTIC		"windowsbaltic"		/* 7 */
#define FC_LANG_VIETNAMESE		"vietnamese"		/* 8 */
/* 9-15 reserved for Alternate ANSI */
#define FC_LANG_THAI			"thai"			/* 16 */
#define FC_LANG_JAPANESE		"japanese"		/* 17 */
#define FC_LANG_SIMPLIFIED_CHINESE	"simplifiedchinese"	/* 18 */
#define FC_LANG_KOREAN_WANSUNG		"koreanwansung"		/* 19 */
#define FC_LANG_TRADITIONAL_CHINESE	"traditionalchinese"	/* 20 */
#define FC_LANG_KOREAN_JOHAB		"koreanjohab"		/* 21 */
/* 22-28 reserved for Alternate ANSI & OEM */
#define FC_LANG_MACINTOSH		"macintosh"		/* 29 */
#define FC_LANG_OEM			"oem"			/* 30 */
#define FC_LANG_SYMBOL			"symbol"		/* 31 */
/* 32-47 reserved for OEM */
#define FC_LANG_IBM_GREEK		"ibmgreek"		/* 48 */
#define FC_LANG_MSDOS_RUSSIAN		"msdosrussian"		/* 49 */
#define FC_LANG_MSDOS_NORDIC		"msdosnordic"		/* 50 */
#define FC_LANG_ARABIC_864    		"arabic864"		/* 51 */
#define FC_LANG_MSDOS_CANADIAN_FRENCH	"msdoscanadianfrench"	/* 52 */
#define FC_LANG_HEBREW_862		"hebrew862"		/* 53 */
#define FC_LANG_MSDOS_ICELANDIC		"msdosicelandic"	/* 54 */
#define FC_LANG_MSDOS_PORTUGUESE	"msdosportuguese"	/* 55 */
#define FC_LANG_IBM_TURKISH		"ibmturkish"		/* 56 */
#define FC_LANG_IBM_CYRILLIC		"ibmcyrillic"		/* 57 */
#define FC_LANG_LATIN_2			"latin2"		/* 58 */
#define FC_LANG_MSDOS_BALTIC		"msdosbaltic"		/* 59 */
#define FC_LANG_GREEK_437_G		"greek437g"		/* 60 */
#define FC_LANG_ARABIC_ASMO_708		"arabicasmo708"		/* 61 */
#define FC_LANG_WE_LATIN_1		"welatin1"		/* 62 */
#define FC_LANG_US			"us"			/* 63 */

typedef enum _FcType {
    FcTypeVoid, 
    FcTypeInteger, 
    FcTypeDouble, 
    FcTypeString, 
    FcTypeBool,
    FcTypeMatrix,
    FcTypeCharSet
} FcType;

typedef struct _FcMatrix {
    double xx, xy, yx, yy;
} FcMatrix;

#define FcMatrixInit(m)	((m)->xx = (m)->yy = 1, \
			 (m)->xy = (m)->yx = 0)

/*
 * A data structure to represent the available glyphs in a font.
 * This is represented as a sparse boolean btree.
 */

typedef struct _FcCharSet FcCharSet;

typedef struct _FcObjectType {
    const char	*object;
    FcType	type;
} FcObjectType;

typedef struct _FcConstant {
    const FcChar8  *name;
    const char	*object;
    int		value;
} FcConstant;

typedef enum _FcResult {
    FcResultMatch, FcResultNoMatch, FcResultTypeMismatch, FcResultNoId
} FcResult;

typedef struct _FcValue {
    FcType	type;
    union {
	const FcChar8	*s;
	int		i;
	FcBool		b;
	double		d;
	const FcMatrix	*m;
	const FcCharSet	*c;
    } u;
} FcValue;

typedef struct _FcPattern   FcPattern;

typedef struct _FcFontSet {
    int		nfont;
    int		sfont;
    FcPattern	**fonts;
} FcFontSet;

typedef struct _FcObjectSet {
    int		nobject;
    int		sobject;
    const char	**objects;
} FcObjectSet;
    
typedef enum _FcMatchKind {
    FcMatchPattern, FcMatchFont 
} FcMatchKind;

typedef enum _FcSetName {
    FcSetSystem = 0,
    FcSetApplication = 1
} FcSetName;

#if defined(__cplusplus) || defined(c_plusplus) /* for C++ V2.0 */
#define _FCFUNCPROTOBEGIN extern "C" {	/* do not leave open across includes */
#define _FCFUNCPROTOEND }
#else
#define _FCFUNCPROTOBEGIN
#define _FCFUNCPROTOEND
#endif

typedef struct _FcConfig    FcConfig;

typedef struct _FcFileCache FcFileCache;

typedef struct _FcBlanks    FcBlanks;

_FCFUNCPROTOBEGIN

/* fcblanks.c */
FcBlanks *
FcBlanksCreate (void);

void
FcBlanksDestroy (FcBlanks *b);

FcBool
FcBlanksAdd (FcBlanks *b, FcChar32 ucs4);

FcBool
FcBlanksIsMember (FcBlanks *b, FcChar32 ucs4);

/* fccfg.c */
FcChar8 *
FcConfigFilename (const FcChar8 *url);
    
FcConfig *
FcConfigCreate (void);

void
FcConfigDestroy (FcConfig *config);

FcBool
FcConfigSetCurrent (FcConfig *config);

FcConfig *
FcConfigGetCurrent (void);

FcBool
FcConfigBuildFonts (FcConfig *config);

FcChar8 **
FcConfigGetDirs (FcConfig   *config);

FcChar8 **
FcConfigGetConfigFiles (FcConfig    *config);

FcChar8 *
FcConfigGetCache (FcConfig  *config);

FcBlanks *
FcConfigGetBlanks (FcConfig *config);

FcFontSet *
FcConfigGetFonts (FcConfig	*config,
		  FcSetName	set);

FcBool
FcConfigAppFontAddFile (FcConfig    *config,
			const FcChar8  *file);

FcBool
FcConfigAppFontAddDir (FcConfig	    *config,
		       const FcChar8   *dir);

void
FcConfigAppFontClear (FcConfig	    *config);

FcBool
FcConfigSubstitute (FcConfig	*config,
		    FcPattern	*p,
		    FcMatchKind	kind);

/* fccharset.c */
FcCharSet *
FcCharSetCreate (void);

void
FcCharSetDestroy (FcCharSet *fcs);

FcBool
FcCharSetAddChar (FcCharSet *fcs, FcChar32 ucs4);

FcCharSet *
FcCharSetCopy (FcCharSet *src);

FcBool
FcCharSetEqual (const FcCharSet *a, const FcCharSet *b);

FcCharSet *
FcCharSetIntersect (const FcCharSet *a, const FcCharSet *b);

FcCharSet *
FcCharSetUnion (const FcCharSet *a, const FcCharSet *b);

FcCharSet *
FcCharSetSubtract (const FcCharSet *a, const FcCharSet *b);

FcBool
FcCharSetHasChar (const FcCharSet *fcs, FcChar32 ucs4);

FcChar32
FcCharSetCount (const FcCharSet *a);

FcChar32
FcCharSetIntersectCount (const FcCharSet *a, const FcCharSet *b);

FcChar32
FcCharSetSubtractCount (const FcCharSet *a, const FcCharSet *b);

FcChar32
FcCharSetCoverage (const FcCharSet *a, FcChar32 page, FcChar32 *result);

/* fcdbg.c */
void
FcPatternPrint (FcPattern *p);

/* fcdefault.c */
void
FcDefaultSubstitute (FcPattern *pattern);

/* fcdir.c */
FcBool
FcFileScan (FcFontSet	    *set,
	    FcFileCache	    *cache,
	    FcBlanks	    *blanks,
	    const FcChar8   *file,
	    FcBool	    force);

FcBool
FcDirScan (FcFontSet	    *set,
	   FcFileCache	    *cache,
	   FcBlanks	    *blanks,
	   const FcChar8    *dir,
	   FcBool	    force);

FcBool
FcDirSave (FcFontSet *set, const FcChar8 *dir);

/* fcfreetype.c */
FcPattern *
FcFreeTypeQuery (const FcChar8 *file, int id, FcBlanks *blanks, int *count);

/* fcfs.c */

FcFontSet *
FcFontSetCreate (void);

void
FcFontSetDestroy (FcFontSet *s);

FcBool
FcFontSetAdd (FcFontSet *s, FcPattern *font);

/* fcinit.c */
FcBool
FcInitFonts (void);

FcBool
FcInitConfig (void);

FcBool
FcInit (void);

/* fclist.c */
FcObjectSet *
FcObjectSetCreate (void);

FcBool
FcObjectSetAdd (FcObjectSet *os, const char *object);

void
FcObjectSetDestroy (FcObjectSet *os);

FcObjectSet *
FcObjectSetVaBuild (const char *first, va_list va);

FcObjectSet *
FcObjectSetBuild (const char *first, ...);

FcFontSet *
FcFontList (FcConfig	*config,
	    FcPattern	*p,
	    FcObjectSet *os);

/* fcmatch.c */
FcPattern *
FcFontMatch (FcConfig	*config,
	     FcPattern	*p, 
	     FcResult	*result);

/* fcmatrix.c */
FcMatrix *
FcMatrixCopy (const FcMatrix *mat);

FcBool
FcMatrixEqual (const FcMatrix *mat1, const FcMatrix *mat2);

void
FcMatrixMultiply (FcMatrix *result, const FcMatrix *a, const FcMatrix *b);

void
FcMatrixRotate (FcMatrix *m, double c, double s);

void
FcMatrixScale (FcMatrix *m, double sx, double sy);

void
FcMatrixShear (FcMatrix *m, double sh, double sv);

/* fcname.c */

FcBool
FcNameRegisterObjectTypes (const FcObjectType *types, int ntype);

FcBool
FcNameUnregisterObjectTypes (const FcObjectType *types, int ntype);
    
const FcObjectType *
FcNameGetObjectType (const char *object);

FcBool
FcNameRegisterConstants (const FcConstant *consts, int nconsts);

FcBool
FcNameUnregisterConstants (const FcConstant *consts, int nconsts);
    
const FcConstant *
FcNameGetConstant (FcChar8 *string);

FcBool
FcNameConstant (FcChar8 *string, int *result);

FcPattern *
FcNameParse (const FcChar8 *name);

FcChar8 *
FcNameUnparse (FcPattern *pat);

/* fcpat.c */
FcPattern *
FcPatternCreate (void);

FcPattern *
FcPatternDuplicate (FcPattern *p);

void
FcValueDestroy (FcValue v);

FcValue
FcValueSave (FcValue v);

void
FcPatternDestroy (FcPattern *p);

FcBool
FcPatternAdd (FcPattern *p, const char *object, FcValue value, FcBool append);
    
FcResult
FcPatternGet (FcPattern *p, const char *object, int id, FcValue *v);
    
FcBool
FcPatternDel (FcPattern *p, const char *object);

FcBool
FcPatternAddInteger (FcPattern *p, const char *object, int i);

FcBool
FcPatternAddDouble (FcPattern *p, const char *object, double d);

FcBool
FcPatternAddString (FcPattern *p, const char *object, const FcChar8 *s);

FcBool
FcPatternAddMatrix (FcPattern *p, const char *object, const FcMatrix *s);

FcBool
FcPatternAddCharSet (FcPattern *p, const char *object, const FcCharSet *c);

FcBool
FcPatternAddBool (FcPattern *p, const char *object, FcBool b);

FcResult
FcPatternGetInteger (FcPattern *p, const char *object, int n, int *i);

FcResult
FcPatternGetDouble (FcPattern *p, const char *object, int n, double *d);

FcResult
FcPatternGetString (FcPattern *p, const char *object, int n, FcChar8 ** s);

FcResult
FcPatternGetMatrix (FcPattern *p, const char *object, int n, FcMatrix **s);

FcResult
FcPatternGetCharSet (FcPattern *p, const char *object, int n, FcCharSet **c);

FcResult
FcPatternGetBool (FcPattern *p, const char *object, int n, FcBool *b);

FcPattern *
FcPatternVaBuild (FcPattern *orig, va_list va);
    
FcPattern *
FcPatternBuild (FcPattern *orig, ...);

/* fcstr.c */

FcChar8 *
FcStrCopy (const FcChar8 *s);

#define FcToLower(c)	(('A' <= (c) && (c) <= 'Z') ? (c) - 'A' + 'a' : (c))

int
FcStrCmpIgnoreCase (const FcChar8 *s1, const FcChar8 *s2);

int
FcUtf8ToUcs4 (FcChar8   *src_orig,
	      FcChar32  *dst,
	      int	len);

FcBool
FcUtf8Len (FcChar8	*string,
	   int		len,
	   int		*nchar,
	   int		*wchar);

/* fcxml.c */
FcBool
FcConfigParseAndLoad (FcConfig *config, const FcChar8 *file, FcBool complain);

_FCFUNCPROTOEND

#endif /* _FONTCONFIG_H_ */
