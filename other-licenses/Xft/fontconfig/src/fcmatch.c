/*
 * $XFree86: xc/lib/fontconfig/src/fcmatch.c,v 1.2 2002/02/15 06:01:28 keithp Exp $
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
FcFontMatch (FcConfig	*config,
	     FcPattern	*p, 
	     FcResult	*result)
{
    double    	    score[NUM_MATCHER], bestscore[NUM_MATCHER];
    int		    f;
    FcFontSet	    *s;
    FcPattern	    *best;
    FcPattern	    *new;
    FcPatternElt   *fe, *pe;
    FcValue	    v;
    int		    i;
    FcSetName	    set;

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
    for (set = FcSetSystem; set <= FcSetApplication; set++)
    {
	s = config->fonts[set];
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
    new = FcPatternCreate ();
    if (!new)
	return 0;
    for (i = 0; i < best->num; i++)
    {
	fe = &best->elts[i];
	pe = FcPatternFind (p, fe->object, FcFalse);
	if (pe)
	{
	    if (!FcCompareValueList (pe->object, pe->values, 
				     fe->values, &v, score, result))
	    {
		FcPatternDestroy (new);
		return 0;
	    }
	}
	else
	    v = fe->values->value;
	FcPatternAdd (new, fe->object, v, FcTrue);
    }
    for (i = 0; i < p->num; i++)
    {
	pe = &p->elts[i];
	fe = FcPatternFind (best, pe->object, FcFalse);
	if (!fe)
	    FcPatternAdd (new, pe->object, pe->values->value, FcTrue);
    }
    FcConfigSubstitute (config, new, FcMatchFont);
    return new;
}
