/*
 * $XFree86: xc/lib/fontconfig/src/fcmatch.c,v 1.4 2002/03/03 18:39:05 keithp Exp $
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

#include <string.h>
#include <ctype.h>
#include "fcint.h"
#include <stdio.h>

static double
FcCompareInteger (char *object, FcValue value1, FcValue value2)
{
    int	v;
    
    if (value2.type != FcTypeInteger || value1.type != FcTypeInteger)
	return -1.0;
    v = value2.u.i - value1.u.i;
    if (v < 0)
	v = -v;
    return (double) v;
}

static double
FcCompareString (char *object, FcValue value1, FcValue value2)
{
    if (value2.type != FcTypeString || value1.type != FcTypeString)
	return -1.0;
    return (double) FcStrCmpIgnoreCase (value1.u.s, value2.u.s) != 0;
}

static double
FcCompareBool (char *object, FcValue value1, FcValue value2)
{
    if (value2.type != FcTypeBool || value1.type != FcTypeBool)
	return -1.0;
    return (double) value2.u.b != value1.u.b;
}

static double
FcCompareCharSet (char *object, FcValue value1, FcValue value2)
{
    if (value2.type != FcTypeCharSet || value1.type != FcTypeCharSet)
	return -1.0;
    return (double) FcCharSetSubtractCount (value1.u.c, value2.u.c);
}

static double
FcCompareSize (char *object, FcValue value1, FcValue value2)
{
    double  v1, v2, v;

    switch (value1.type) {
    case FcTypeInteger:
	v1 = value1.u.i;
	break;
    case FcTypeDouble:
	v1 = value1.u.d;
	break;
    default:
	return -1;
    }
    switch (value2.type) {
    case FcTypeInteger:
	v2 = value2.u.i;
	break;
    case FcTypeDouble:
	v2 = value2.u.d;
	break;
    default:
	return -1;
    }
    if (v2 == 0)
	return 0;
    v = v2 - v1;
    if (v < 0)
	v = -v;
    return v;
}

/*
 * Order is significant, it defines the precedence of
 * each value, earlier values are more significant than
 * later values
 */
static FcMatcher _FcMatchers [] = {
    { FC_FOUNDRY,	FcCompareString, },
    { FC_CHARSET,	FcCompareCharSet },
    { FC_ANTIALIAS,	FcCompareBool, },
    { FC_LANG,		FcCompareString },
    { FC_FAMILY,	FcCompareString, },
    { FC_SPACING,	FcCompareInteger, },
    { FC_PIXEL_SIZE,	FcCompareSize, },
    { FC_STYLE,		FcCompareString, },
    { FC_SLANT,		FcCompareInteger, },
    { FC_WEIGHT,	FcCompareInteger, },
    { FC_RASTERIZER,	FcCompareString, },
    { FC_OUTLINE,	FcCompareBool, },
};

#define NUM_MATCHER (sizeof _FcMatchers / sizeof _FcMatchers[0])

static FcBool
FcCompareValueList (const char  *object,
		    FcValueList	*v1orig,	/* pattern */
		    FcValueList *v2orig,	/* target */
		    FcValue	*bestValue,
		    double	*value,
		    FcResult	*result)
{
    FcValueList    *v1, *v2;
    double    	    v, best;
    int		    j;
    int		    i;
    
    for (i = 0; i < NUM_MATCHER; i++)
    {
	if (!FcStrCmpIgnoreCase ((FcChar8 *) _FcMatchers[i].object,
				 (FcChar8 *) object))
	    break;
    }
    if (i == NUM_MATCHER)
    {
	if (bestValue)
	    *bestValue = v2orig->value;
	return FcTrue;
    }
    
    best = 1e99;
    j = 0;
    for (v1 = v1orig; v1; v1 = v1->next)
    {
	for (v2 = v2orig; v2; v2 = v2->next)
	{
	    v = (*_FcMatchers[i].compare) (_FcMatchers[i].object,
					    v1->value,
					    v2->value);
	    if (v < 0)
	    {
		*result = FcResultTypeMismatch;
		return FcFalse;
	    }
	    if (FcDebug () & FC_DBG_MATCHV)
		printf (" v %g j %d ", v, j);
	    v = v * 100 + j;
	    if (v < best)
	    {
		if (bestValue)
		    *bestValue = v2->value;
		best = v;
	    }
	}
	j++;
    }
    if (FcDebug () & FC_DBG_MATCHV)
    {
	printf (" %s: %g ", object, best);
	FcValueListPrint (v1orig);
	printf (", ");
	FcValueListPrint (v2orig);
	printf ("\n");
    }
    value[i] += best;
    return FcTrue;
}

/*
 * Return a value indicating the distance between the two lists of
 * values
 */

static FcBool
FcCompare (FcPattern	*pat,
	   FcPattern	*fnt,
	   double	*value,
	   FcResult	*result)
{
    int		    i, i1, i2;
    
    for (i = 0; i < NUM_MATCHER; i++)
	value[i] = 0.0;
    
    for (i1 = 0; i1 < pat->num; i1++)
    {
	for (i2 = 0; i2 < fnt->num; i2++)
	{
	    if (!FcStrCmpIgnoreCase ((FcChar8 *) pat->elts[i1].object,
				     (FcChar8 *) fnt->elts[i2].object))
	    {
		if (!FcCompareValueList (pat->elts[i1].object,
					 pat->elts[i1].values,
					 fnt->elts[i2].values,
					 0,
					 value,
					 result))
		    return FcFalse;
		break;
	    }
	}
#if 0
	/*
	 * Overspecified patterns are slightly penalized in
	 * case some other font includes the requested field
	 */
	if (i2 == fnt->num)
	{
	    for (i2 = 0; i2 < NUM_MATCHER; i2++)
	    {
		if (!FcStrCmpIgnoreCase (_FcMatchers[i2].object,
					 pat->elts[i1].object))
		{
		    value[i2] = 1.0;
		    break;
		}
	    }
	}
#endif
    }
    return FcTrue;
}

FcPattern *
FcFontRenderPrepare (FcConfig	    *config,
		     FcPattern	    *pat,
		     FcPattern	    *font)
{
    FcPattern	    *new;
    int		    i;
    FcPatternElt    *fe, *pe;
    FcValue	    v;
    double	    score[NUM_MATCHER];
    FcResult	    result;
    
    new = FcPatternCreate ();
    if (!new)
	return 0;
    for (i = 0; i < font->num; i++)
    {
	fe = &font->elts[i];
	pe = FcPatternFind (pat, fe->object, FcFalse);
	if (pe)
	{
	    if (!FcCompareValueList (pe->object, pe->values, 
				     fe->values, &v, score, &result))
	    {
		FcPatternDestroy (new);
		return 0;
	    }
	}
	else
	    v = fe->values->value;
	FcPatternAdd (new, fe->object, v, FcTrue);
    }
    for (i = 0; i < pat->num; i++)
    {
	pe = &pat->elts[i];
	fe = FcPatternFind (font, pe->object, FcFalse);
	if (!fe)
	    FcPatternAdd (new, pe->object, pe->values->value, FcTrue);
    }
    FcConfigSubstitute (config, new, FcMatchFont);
    return new;
}

FcPattern *
FcFontSetMatch (FcConfig    *config,
		FcFontSet   **sets,
		int	    nsets,
		FcPattern   *p,
		FcResult    *result)
{
    double    	    score[NUM_MATCHER], bestscore[NUM_MATCHER];
    int		    f;
    FcFontSet	    *s;
    FcPattern	    *best;
    int		    i;
    int		    set;

    for (i = 0; i < NUM_MATCHER; i++)
	bestscore[i] = 0;
    best = 0;
    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Match ");
	FcPatternPrint (p);
    }
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++)
	{
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    if (!FcCompare (p, s->fonts[f], score, result))
		return 0;
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Score");
		for (i = 0; i < NUM_MATCHER; i++)
		{
		    printf (" %g", score[i]);
		}
		printf ("\n");
	    }
	    for (i = 0; i < NUM_MATCHER; i++)
	    {
		if (best && bestscore[i] < score[i])
		    break;
		if (!best || score[i] < bestscore[i])
		{
		    for (i = 0; i < NUM_MATCHER; i++)
			bestscore[i] = score[i];
		    best = s->fonts[f];
		    break;
		}
	    }
	}
    }
    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Best score");
	for (i = 0; i < NUM_MATCHER; i++)
	    printf (" %g", bestscore[i]);
	FcPatternPrint (best);
    }
    if (!best)
    {
	*result = FcResultNoMatch;
	return 0;
    }
    return FcFontRenderPrepare (config, p, best);
}

FcPattern *
FcFontMatch (FcConfig	*config,
	     FcPattern	*p, 
	     FcResult	*result)
{
    FcFontSet	*sets[2];
    int		nsets;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    nsets = 0;
    if (config->fonts[FcSetSystem])
	sets[nsets++] = config->fonts[FcSetSystem];
    if (config->fonts[FcSetApplication])
	sets[nsets++] = config->fonts[FcSetApplication];
    return FcFontSetMatch (config, sets, nsets, p, result);
}

#include "fcavl.h"

typedef struct _FcSortNode {
    FcAvlNode	avl;
    FcPattern	*pattern;
    double	score[NUM_MATCHER];
} FcSortNode;

static FcBool
FcSortMore (FcAvlNode *aa, FcAvlNode *ab)
{
    FcSortNode	*a = (FcSortNode *) aa;
    FcSortNode	*b = (FcSortNode *) ab;
    int		i;

    for (i = 0; i < NUM_MATCHER; i++)
    {
	if (a->score[i] > b->score[i])
	    return FcTrue;
	if (a->score[i] < b->score[i])
	    return FcFalse;
    }
    if (aa > ab)
	return FcTrue;
    return FcFalse;
}

static FcBool
FcSortWalk (FcSortNode *n, FcFontSet *fs, FcCharSet **cs, FcBool trim)
{
    FcCharSet	*ncs;
    
    if (!n)
	return FcTrue;
    if (!FcSortWalk ((FcSortNode *) n->avl.left, fs, cs, trim))
	return FcFalse;
    if (FcPatternGetCharSet (n->pattern, FC_CHARSET, 0, &ncs) == FcResultMatch)
    {
	if (!trim || !*cs || FcCharSetSubtractCount (ncs, *cs) != 0)
	{
	    if (*cs)
	    {
		ncs = FcCharSetUnion (ncs, *cs);
		if (!ncs)
		    return FcFalse;
		FcCharSetDestroy (*cs);
	    }
	    else
		ncs = FcCharSetCopy (ncs);
	    *cs = ncs;
	    if (!FcFontSetAdd (fs, n->pattern))
		return FcFalse;
	}
    }
    if (!FcSortWalk ((FcSortNode *) n->avl.right, fs, cs, trim))
	return FcFalse;
    return FcTrue;
}

FcFontSet *
FcFontSetSort (FcConfig	    *config,
	       FcFontSet    **sets,
	       int	    nsets,
	       FcPattern    *p,
	       FcBool	    trim,
	       FcCharSet    **csp,
	       FcResult	    *result)
{
    FcFontSet	    *ret;
    FcFontSet	    *s;
    FcSortNode	    *nodes;
    int		    nnodes;
    FcSortNode	    *root;
    FcSortNode	    *new;
    FcCharSet	    *cs;
    int		    set;
    int		    f;
    int		    i;

    nnodes = 0;
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	nnodes += s->nfont;
    }
    if (!nnodes)
	goto bail0;
    nodes = malloc (nnodes * sizeof (FcSortNode));
    if (!nodes)
	goto bail0;
    
    root = 0;
    new = nodes;
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++)
	{
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    new->pattern = s->fonts[f];
	    if (!FcCompare (p, new->pattern, new->score, result))
		goto bail1;
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Score");
		for (i = 0; i < NUM_MATCHER; i++)
		{
		    printf (" %g", new->score[i]);
		}
		printf ("\n");
	    }
	    FcAvlInsert (FcSortMore, (FcAvlNode **) &root, &new->avl);
	    new++;
	}
    }

    ret = FcFontSetCreate ();
    if (!ret)
	goto bail1;

    cs = 0;

    if (!FcSortWalk (root, ret, &cs, trim))
	goto bail2;

    *csp = cs;

    return ret;

bail2:
    if (cs)
	FcCharSetDestroy (cs);
    FcFontSetDestroy (ret);
bail1:
    free (nodes);
bail0:
    return 0;
}
