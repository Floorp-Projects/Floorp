/*
 * $XFree86: xc/lib/fontconfig/src/fcint.h,v 1.5 2002/02/22 18:54:07 keithp Exp $
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

#ifndef _FCINT_H_
#define _FCINT_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcprivate.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _FcMatcher {
    char    *object;
    double  (*compare) (char *object, FcValue value1, FcValue value2);
} FcMatcher;

typedef struct _FcSymbolic {
    const char	*name;
    int		value;
} FcSymbolic;

#ifndef FC_CONFIG_PATH
#define FC_CONFIG_PATH "fonts.conf"
#endif

#define FC_DBG_MATCH	1
#define FC_DBG_MATCHV	2
#define FC_DBG_EDIT	4
#define FC_DBG_FONTSET	8
#define FC_DBG_CACHE	16
#define FC_DBG_CACHEV	32
#define FC_DBG_PARSE	64
#define FC_DBG_SCAN	128
#define FC_DBG_MEMORY	512

#define FC_MEM_CHARSET	    0
#define FC_MEM_CHARNODE	    1
#define FC_MEM_FONTSET	    2
#define FC_MEM_FONTPTR	    3
#define FC_MEM_OBJECTSET    4
#define FC_MEM_OBJECTPTR    5
#define FC_MEM_MATRIX	    6
#define FC_MEM_PATTERN	    7
#define FC_MEM_PATELT	    8
#define FC_MEM_VALLIST	    9
#define FC_MEM_SUBSTATE	    10
#define FC_MEM_STRING	    11
#define FC_MEM_LISTBUCK	    12
#define FC_MEM_NUM	    13

typedef struct _FcValueList {
    struct _FcValueList    *next;
    FcValue		    value;
} FcValueList;

typedef struct _FcPatternElt {
    const char	    *object;
    FcValueList    *values;
} FcPatternElt;

struct _FcPattern {
    int		    num;
    int		    size;
    FcPatternElt   *elts;
};

typedef enum _FcOp {
    FcOpInteger, FcOpDouble, FcOpString, FcOpMatrix, FcOpBool, FcOpCharSet, 
    FcOpNil,
    FcOpField, FcOpConst,
    FcOpAssign, FcOpAssignReplace, 
    FcOpPrependFirst, FcOpPrepend, FcOpAppend, FcOpAppendLast,
    FcOpQuest,
    FcOpOr, FcOpAnd, FcOpEqual, FcOpNotEqual, FcOpContains,
    FcOpLess, FcOpLessEqual, FcOpMore, FcOpMoreEqual,
    FcOpPlus, FcOpMinus, FcOpTimes, FcOpDivide,
    FcOpNot, FcOpComma, FcOpInvalid
} FcOp;

typedef struct _FcExpr {
    FcOp   op;
    union {
	int	    ival;
	double	    dval;
	FcChar8	    *sval;
	FcMatrix    *mval;
	FcBool	    bval;
	FcCharSet   *cval;
	char	    *field;
	FcChar8	    *constant;
	struct {
	    struct _FcExpr *left, *right;
	} tree;
    } u;
} FcExpr;

typedef enum _FcQual {
    FcQualAny, FcQualAll
} FcQual;

typedef struct _FcTest {
    struct _FcTest	*next;
    FcQual		qual;
    const char		*field;
    FcOp		op;
    FcExpr		*expr;
} FcTest;

typedef struct _FcEdit {
    struct _FcEdit *next;
    const char	    *field;
    FcOp	    op;
    FcExpr	    *expr;
} FcEdit;

typedef struct _FcSubst {
    struct _FcSubst	*next;
    FcTest		*test;
    FcEdit		*edit;
} FcSubst;

typedef struct _FcCharLeaf FcCharLeaf;
typedef struct _FcCharBranch FcCharBranch;
typedef union  _FcCharNode FcCharNode;

struct _FcCharLeaf {
    FcChar32	map[256/32];
};

union _FcCharNode {
    FcCharBranch    *branch;
    FcCharLeaf	    *leaf;
};

struct _FcCharBranch {
    FcCharNode	    nodes[256];
};

struct _FcCharSet {
    int		    levels;
    int		    ref;	/* reference count */
    FcBool	    constant;	/* shared constant */
    FcCharNode	    node;
};

typedef struct _FcStrBuf {
    FcChar8 *buf;
    FcBool  allocated;
    FcBool  failed;
    int	    len;
    int	    size;
} FcStrBuf;

typedef struct _FcFileCacheEnt {
    struct _FcFileCacheEnt *next;
    unsigned int	    hash;
    FcChar8		    *file;
    int			    id;
    time_t		    time;
    FcChar8		    *name;
    FcBool		    referenced;
} FcFileCacheEnt;

#define FC_FILE_CACHE_HASH_SIZE   509

struct _FcFileCache {
    FcFileCacheEnt	*ents[FC_FILE_CACHE_HASH_SIZE];
    FcBool		updated;
    int			entries;
    int			referenced;
};

struct _FcBlanks {
    int		nblank;
    int		sblank;
    FcChar32	*blanks;
};

struct _FcConfig {
    /*
     * File names loaded from the configuration -- saved here as the
     * cache file must be consulted before the directories are scanned,
     * and those directives may occur in any order
     */
    FcChar8	**dirs;		    /* directories containing fonts */
    FcChar8	*cache;		    /* name of per-user cache file */
    /*
     * Set of allowed blank chars -- used to
     * trim fonts of bogus glyphs
     */
    FcBlanks	*blanks;
    /*
     * Names of all of the configuration files used
     * to create this configuration
     */
    FcChar8	**configFiles;	    /* config files loaded */
    /*
     * Substitution instructions for patterns and fonts;
     * maxObjects is used to allocate appropriate intermediate storage
     * while performing a whole set of substitutions
     */
    FcSubst	*substPattern;	    /* substitutions for patterns */
    FcSubst	*substFont;	    /* substitutions for fonts */
    int		maxObjects;	    /* maximum number of tests in all substs */
    /*
     * The set of fonts loaded from the listed directories; the
     * order within the set does not determine the font selection,
     * except in the case of identical matches in which case earlier fonts
     * match preferrentially
     */
    FcFontSet	*fonts[FcSetApplication + 1];
};
 
extern FcConfig	*_fcConfig;

/* fcblanks.c */

/* fccache.c */

FcFileCache *
FcFileCacheCreate (void);

FcChar8 *
FcFileCacheFind (FcFileCache	*cache,
		 const FcChar8	*file,
		 int		id,
		 int		*count);

void
FcFileCacheDestroy (FcFileCache	*cache);

void
FcFileCacheLoad (FcFileCache	*cache,
		 const FcChar8	*cache_file);

FcBool
FcFileCacheUpdate (FcFileCache	    *cache,
		   const FcChar8    *file,
		   int		    id,
		   const FcChar8    *name);

FcBool
FcFileCacheSave (FcFileCache	*cache,
		 const FcChar8	*cache_file);

FcBool
FcFileCacheReadDir (FcFontSet *set, const FcChar8 *cache_file);

FcBool
FcFileCacheWriteDir (FcFontSet *set, const FcChar8 *cache_file);
    
/* fccfg.c */

FcBool
FcConfigAddDir (FcConfig	*config,
		const FcChar8	*d);

FcBool
FcConfigAddConfigFile (FcConfig		*config,
		       const FcChar8	*f);

FcBool
FcConfigSetCache (FcConfig	*config,
		  const FcChar8	*c);

FcBool
FcConfigAddBlank (FcConfig	*config,
		  FcChar32    	blank);

FcBool
FcConfigAddEdit (FcConfig	*config,
		 FcTest		*test,
		 FcEdit		*edit,
		 FcMatchKind	kind);

void
FcConfigSetFonts (FcConfig	*config,
		  FcFontSet	*fonts,
		  FcSetName	set);

FcBool
FcConfigCompareValue (const FcValue m,
		      FcOp	    op,
		      const FcValue v);

/* fccharset.c */
FcBool
FcNameUnparseCharSet (FcStrBuf *buf, const FcCharSet *c);

FcCharSet *
FcNameParseCharSet (FcChar8 *string);

/* fcdbg.c */
void
FcValuePrint (FcValue v);

void
FcValueListPrint (FcValueList *l);

void
FcOpPrint (FcOp op);

void
FcTestPrint (FcTest *test);

void
FcExprPrint (FcExpr *expr);

void
FcEditPrint (FcEdit *edit);

void
FcSubstPrint (FcSubst *subst);

void
FcFontSetPrint (FcFontSet *s);

int
FcDebug (void);

/* fcdir.c */

/* fcfont.c */
int
FcFontDebug (void);
    
/* fcfs.c */
/* fcgram.y */
int
FcConfigparse (void);

int
FcConfigwrap (void);
    
void
FcConfigerror (char *fmt, ...);
    
char *
FcConfigSaveField (const char *field);

FcTest *
FcTestCreate (FcQual qual, const FcChar8 *field, FcOp compare, FcExpr *expr);

void
FcTestDestroy (FcTest *test);

FcExpr *
FcExprCreateInteger (int i);

FcExpr *
FcExprCreateDouble (double d);

FcExpr *
FcExprCreateString (const FcChar8 *s);

FcExpr *
FcExprCreateMatrix (const FcMatrix *m);

FcExpr *
FcExprCreateBool (FcBool b);

FcExpr *
FcExprCreateNil (void);

FcExpr *
FcExprCreateField (const char *field);

FcExpr *
FcExprCreateConst (const FcChar8 *constant);

FcExpr *
FcExprCreateOp (FcExpr *left, FcOp op, FcExpr *right);

void
FcExprDestroy (FcExpr *e);

FcEdit *
FcEditCreate (const char *field, FcOp op, FcExpr *expr);

void
FcEditDestroy (FcEdit *e);

/* fcinit.c */

void
FcMemReport (void);

void
FcMemAlloc (int kind, int size);

void
FcMemFree (int kind, int size);

/* fclist.c */

/* fcmatch.c */

/* fcname.c */

FcBool
FcNameBool (FcChar8 *v, FcBool *result);

/* fcpat.c */
void
FcValueListDestroy (FcValueList *l);
    
FcPatternElt *
FcPatternFind (FcPattern *p, const char *object, FcBool insert);

/* fcrender.c */

/* fcmatrix.c */
void
FcMatrixFree (FcMatrix *mat);

/* fcstr.c */
FcChar8 *
FcStrPlus (const FcChar8 *s1, const FcChar8 *s2);
    
void
FcStrFree (FcChar8 *s);

void
FcStrBufInit (FcStrBuf *buf, FcChar8 *init, int size);

void
FcStrBufDestroy (FcStrBuf *buf);

FcChar8 *
FcStrBufDone (FcStrBuf *buf);

FcBool
FcStrBufChar (FcStrBuf *buf, FcChar8 c);

FcBool
FcStrBufString (FcStrBuf *buf, const FcChar8 *s);

FcBool
FcStrBufData (FcStrBuf *buf, const FcChar8 *s, int len);

#endif /* _FC_INT_H_ */
