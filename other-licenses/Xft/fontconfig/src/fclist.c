/*
 * $XFree86: xc/lib/fontconfig/src/fclist.c,v 1.1.1.1 2002/02/14 23:34:12 keithp Exp $
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
#include "fcint.h"

FcObjectSet *
FcObjectSetCreate (void)
{
    FcObjectSet    *os;

    os = (FcObjectSet *) malloc (sizeof (FcObjectSet));
    if (!os)
	return 0;
    FcMemAlloc (FC_MEM_OBJECTSET, sizeof (FcObjectSet));
    os->nobject = 0;
    os->sobject = 0;
    os->objects = 0;
    return os;
}

FcBool
FcObjectSetAdd (FcObjectSet *os, const char *object)
{
    int		s;
    const char	**objects;
    
    if (os->nobject == os->sobject)
    {
	s = os->sobject + 4;
	if (os->objects)
	    objects = (const char **) realloc ((void *) os->objects,
					       s * sizeof (const char *));
	else
	    objects = (const char **) malloc (s * sizeof (const char *));
	if (!objects)
	    return FcFalse;
	if (os->sobject)
	    FcMemFree (FC_MEM_OBJECTPTR, os->sobject * sizeof (const char *));
	FcMemAlloc (FC_MEM_OBJECTPTR, s * sizeof (const char *));
	os->objects = objects;
	os->sobject = s;
    }
    os->objects[os->nobject++] = object;
    return FcTrue;
}

void
FcObjectSetDestroy (FcObjectSet *os)
{
    if (os->objects)
    {
	FcMemFree (FC_MEM_OBJECTPTR, os->sobject * sizeof (const char *));
	free ((void *) os->objects);
    }
    FcMemFree (FC_MEM_OBJECTSET, sizeof (FcObjectSet));
    free (os);
}

FcObjectSet *
FcObjectSetVaBuild (const char *first, va_list va)
{
    FcObjectSet    *ret;

    FcObjectSetVapBuild (ret, first, va);
    return ret;
}

FcObjectSet *
FcObjectSetBuild (const char *first, ...)
{
    va_list	    va;
    FcObjectSet    *os;

    va_start (va, first);
    FcObjectSetVapBuild (os, first, va);
    va_end (va);
    return os;
}

static FcBool
FcListValueListMatchAny (FcValueList *v1orig,
			 FcValueList *v2orig)
{
    FcValueList	    *v1, *v2;

    for (v1 = v1orig; v1; v1 = v1->next)
	for (v2 = v2orig; v2; v2 = v2->next)
	    if (FcConfigCompareValue (v2->value, FcOpContains, v1->value))
		return FcTrue;
    return FcFalse;
}

static FcBool
FcListValueListEqual (FcValueList   *v1orig,
		      FcValueList   *v2orig)
{
    FcValueList	    *v1, *v2;

    for (v1 = v1orig; v1; v1 = v1->next)
    {
	for (v2 = v2orig; v2; v2 = v2->next)
	    if (FcConfigCompareValue (v1->value, FcOpEqual, v2->value))
		break;
	if (!v2)
	    return FcFalse;
    }
    for (v2 = v2orig; v2; v2 = v2->next)
    {
	for (v1 = v1orig; v1; v1 = v1->next)
	    if (FcConfigCompareValue (v1->value, FcOpEqual, v2->value))
		break;
	if (!v1)
	    return FcFalse;
    }
    return FcTrue;
}

/*
 * FcTrue iff all objects in "p" match "font"
 */

static FcBool
FcListPatternMatchAny (FcPattern *p,
		       FcPattern *font)
{
    int		    i;
    FcPatternElt   *e;

    for (i = 0; i < p->num; i++)
    {
	e = FcPatternFind (font, p->elts[i].object, FcFalse);
	if (!e)
	    return FcFalse;
	if (!FcListValueListMatchAny (p->elts[i].values, e->values))
	    return FcFalse;
    }
    return FcTrue;
}

static FcBool
FcListPatternEqual (FcPattern	*p1,
		    FcPattern	*p2,
		    FcObjectSet	*os)
{
    int		    i;
    FcPatternElt    *e1, *e2;

    for (i = 0; i < os->nobject; i++)
    {
	e1 = FcPatternFind (p1, os->objects[i], FcFalse);
	e2 = FcPatternFind (p2, os->objects[i], FcFalse);
	if (!e1 && !e2)
	    return FcTrue;
	if (!e1 || !e2)
	    return FcFalse;
	if (!FcListValueListEqual (e1->values, e2->values))
	    return FcFalse;
    }
    return FcTrue;
}

static FcChar32
FcListStringHash (const FcChar8	*s)
{
    FcChar32	h = 0;
    FcChar8	c;

    while ((c = *s++))
    {
	c = FcToLower (c);
	h = ((h << 3) ^ (h >> 3)) ^ c;
    }
    return h;
}

static FcChar32
FcListMatrixHash (const FcMatrix *m)
{
    int	    xx = (int) (m->xx * 100), 
	    xy = (int) (m->xy * 100), 
	    yx = (int) (m->yx * 100),
	    yy = (int) (m->yy * 100);

    return ((FcChar32) xx) ^ ((FcChar32) xy) ^ ((FcChar32) yx) ^ ((FcChar32) yy);
}

static FcChar32
FcListValueHash (FcValue    v)
{
    switch (v.type) {
    case FcTypeVoid:
	return 0;
    case FcTypeInteger:
	return (FcChar32) v.u.i;
    case FcTypeDouble:
	return (FcChar32) (int) v.u.d;
    case FcTypeString:
	return FcListStringHash (v.u.s);
    case FcTypeBool:
	return (FcChar32) v.u.b;
    case FcTypeMatrix:
	return FcListMatrixHash (v.u.m);
    case FcTypeCharSet:
	return FcCharSetCount (v.u.c);
    }
    return 0;
}

static FcChar32
FcListValueListHash (FcValueList    *list)
{
    FcChar32	h = 0;
    
    while (list)
    {
	h = h ^ FcListValueHash (list->value);
	list = list->next;
    }
    return h;
}

static FcChar32
FcListPatternHash (FcPattern	*font,
		   FcObjectSet	*os)
{
    int		    n;
    FcPatternElt    *e;
    FcChar32	    h = 0;

    for (n = 0; n < os->nobject; n++)
    {
	e = FcPatternFind (font, os->objects[n], FcFalse);
	if (e)
	    h = h ^ FcListValueListHash (e->values);
    }
    return h;
}

typedef struct _FcListBucket {
    struct _FcListBucket    *next;
    FcChar32		    hash;
    FcPattern		    *pattern;
} FcListBucket;

#define FC_LIST_HASH_SIZE   4099

typedef struct _FcListHashTable {
    int		    entries;
    FcListBucket    *buckets[FC_LIST_HASH_SIZE];
} FcListHashTable;
    
static void
FcListHashTableInit (FcListHashTable *table)
{
    table->entries = 0;
    memset (table->buckets, '\0', sizeof (table->buckets));
}

static void
FcListHashTableCleanup (FcListHashTable *table)
{
    int	i;
    FcListBucket    *bucket, *next;

    for (i = 0; i < FC_LIST_HASH_SIZE; i++)
    {
	for (bucket = table->buckets[i]; bucket; bucket = next)
	{
	    next = bucket->next;
	    FcPatternDestroy (bucket->pattern);
	    FcMemFree (FC_MEM_LISTBUCK, sizeof (FcListBucket));
	    free (bucket);
	}
	table->buckets[i] = 0;
    }
    table->entries = 0;
}

static FcBool
FcListAppend (FcListHashTable	*table,
	      FcPattern		*font,
	      FcObjectSet	*os)
{
    int		    o;
    FcPatternElt    *e;
    FcValueList	    *v;
    FcChar32	    hash;
    FcListBucket    **prev, *bucket;

    hash = FcListPatternHash (font, os);
    for (prev = &table->buckets[hash % FC_LIST_HASH_SIZE];
	 (bucket = *prev); prev = &(bucket->next))
    {
	if (bucket->hash == hash && 
	    FcListPatternEqual (bucket->pattern, font, os))
	    return FcTrue;
    }
    bucket = (FcListBucket *) malloc (sizeof (FcListBucket));
    if (!bucket)
	goto bail0;
    FcMemAlloc (FC_MEM_LISTBUCK, sizeof (FcListBucket));
    bucket->next = 0;
    bucket->hash = hash;
    bucket->pattern = FcPatternCreate ();
    if (!bucket->pattern)
	goto bail1;
    
    for (o = 0; o < os->nobject; o++)
    {
	e = FcPatternFind (font, os->objects[o], FcFalse);
	if (e)
	{
	    for (v = e->values; v; v = v->next)
	    {
		if (!FcPatternAdd (bucket->pattern, 
				   os->objects[o], 
				   v->value, FcTrue))
		    goto bail2;
	    }
	}
    }
    *prev = bucket;
    ++table->entries;

    return FcTrue;
    
bail2:
    FcPatternDestroy (bucket->pattern);
bail1:
    FcMemFree (FC_MEM_LISTBUCK, sizeof (FcListBucket));
    free (bucket);
bail0:
    return FcFalse;
}

FcFontSet *
FcFontList (FcConfig	*config,
	    FcPattern	*p,
	    FcObjectSet *os)
{
    FcFontSet	    *ret;
    FcFontSet	    *s;
    int		    f;
    FcSetName	    set;
    FcListHashTable table;
    int		    i;
    FcListBucket    *bucket;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    goto bail0;
    }
    FcListHashTableInit (&table);
    /*
     * Walk all available fonts adding those that
     * match to the hash table
     */
    for (set = FcSetSystem; set <= FcSetApplication; set++)
    {
	s = config->fonts[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++)
	    if (FcListPatternMatchAny (p, s->fonts[f]))
		if (!FcListAppend (&table, s->fonts[f], os))
		    goto bail1;
    }
#if 0
    {
	int	max = 0;
	int	full = 0;
	int	ents = 0;
	int	len;
	for (i = 0; i < FC_LIST_HASH_SIZE; i++)
	{
	    if ((bucket = table.buckets[i]))
	    {
		len = 0;
		for (; bucket; bucket = bucket->next)
		{
		    ents++;
		    len++;
		}
		if (len > max)
		    max = len;
		full++;
	    }
	}
	printf ("used: %d max: %d avg: %g\n", full, max, 
		(double) ents / FC_LIST_HASH_SIZE);
    }
#endif
    /*
     * Walk the hash table and build
     * a font set
     */
    ret = FcFontSetCreate ();
    if (!ret)
	goto bail0;
    for (i = 0; i < FC_LIST_HASH_SIZE; i++)
	while ((bucket = table.buckets[i]))
	{
	    if (!FcFontSetAdd (ret, bucket->pattern))
		goto bail2;
	    table.buckets[i] = bucket->next;
	    FcMemFree (FC_MEM_LISTBUCK, sizeof (FcListBucket));
	    free (bucket);
	}
    
    return ret;

bail2:
    FcFontSetDestroy (ret);
bail1:
    FcListHashTableCleanup (&table);
bail0:
    return 0;
}
