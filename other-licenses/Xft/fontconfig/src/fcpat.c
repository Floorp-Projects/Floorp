/*
 * $XFree86: xc/lib/fontconfig/src/fcpat.c,v 1.3 2002/02/19 07:50:44 keithp Exp $
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
#include <string.h>
#include "fcint.h"

FcPattern *
FcPatternCreate (void)
{
    FcPattern	*p;

    p = (FcPattern *) malloc (sizeof (FcPattern));
    if (!p)
	return 0;
    FcMemAlloc (FC_MEM_PATTERN, sizeof (FcPattern));
    p->num = 0;
    p->size = 0;
    p->elts = 0;
    return p;
}

void
FcValueDestroy (FcValue v)
{
    switch (v.type) {
    case FcTypeString:
	FcStrFree ((FcChar8 *) v.u.s);
	break;
    case FcTypeMatrix:
	FcMatrixFree ((FcMatrix *) v.u.m);
	break;
    case FcTypeCharSet:
	FcCharSetDestroy ((FcCharSet *) v.u.c);
	break;
    default:
	break;
    }
}

FcValue
FcValueSave (FcValue v)
{
    switch (v.type) {
    case FcTypeString:
	v.u.s = FcStrCopy (v.u.s);
	if (!v.u.s)
	    v.type = FcTypeVoid;
	break;
    case FcTypeMatrix:
	v.u.m = FcMatrixCopy (v.u.m);
	if (!v.u.m)
	    v.type = FcTypeVoid;
	break;
    case FcTypeCharSet:
	v.u.c = FcCharSetCopy ((FcCharSet *) v.u.c);
	if (!v.u.c)
	    v.type = FcTypeVoid;
	break;
    default:
	break;
    }
    return v;
}

void
FcValueListDestroy (FcValueList *l)
{
    FcValueList    *next;
    for (; l; l = next)
    {
	switch (l->value.type) {
	case FcTypeString:
	    FcStrFree ((FcChar8 *) l->value.u.s);
	    break;
	case FcTypeMatrix:
	    FcMatrixFree ((FcMatrix *) l->value.u.m);
	    break;
	case FcTypeCharSet:
	    FcCharSetDestroy ((FcCharSet *) l->value.u.c);
	    break;
	default:
	    break;
	}
	next = l->next;
	FcMemFree (FC_MEM_VALLIST, sizeof (FcValueList));
	free (l);
    }
}

void
FcPatternDestroy (FcPattern *p)
{
    int		    i;
    
    for (i = 0; i < p->num; i++)
	FcValueListDestroy (p->elts[i].values);

    p->num = 0;
    if (p->elts)
    {
	FcMemFree (FC_MEM_PATELT, p->size * sizeof (FcPatternElt));
	free (p->elts);
	p->elts = 0;
    }
    p->size = 0;
    FcMemFree (FC_MEM_PATTERN, sizeof (FcPattern));
    free (p);
}

FcPatternElt *
FcPatternFind (FcPattern *p, const char *object, FcBool insert)
{
    int		    i;
    int		    s;
    FcPatternElt   *e;
    
    /* match existing */
    for (i = 0; i < p->num; i++)
    {
	if (!FcStrCmpIgnoreCase ((FcChar8 *) object, (FcChar8 *) p->elts[i].object))
	    return &p->elts[i];
    }

    if (!insert)
	return FcFalse;

    /* grow array */
    if (i == p->size)
    {
	s = p->size + 16;
	if (p->elts)
	    e = (FcPatternElt *) realloc (p->elts, s * sizeof (FcPatternElt));
	else
	    e = (FcPatternElt *) malloc (s * sizeof (FcPatternElt));
	if (!e)
	    return FcFalse;
	p->elts = e;
	if (p->size)
	    FcMemFree (FC_MEM_PATELT, p->size * sizeof (FcPatternElt));
	FcMemAlloc (FC_MEM_PATELT, s * sizeof (FcPatternElt));
	while (p->size < s)
	{
	    p->elts[p->size].object = 0;
	    p->elts[p->size].values = 0;
	    p->size++;
	}
    }
    
    /* bump count */
    p->num++;
    
    p->elts[i].object = object;
    
    return &p->elts[i];
}

FcBool
FcPatternAdd (FcPattern *p, const char *object, FcValue value, FcBool append)
{
    FcPatternElt   *e;
    FcValueList    *new, **prev;

    new = (FcValueList *) malloc (sizeof (FcValueList));
    if (!new)
	goto bail0;

    FcMemAlloc (FC_MEM_VALLIST, sizeof (FcValueList));
    /* dup string */
    value = FcValueSave (value);
    if (value.type == FcTypeVoid)
	goto bail1;

    new->value = value;
    new->next = 0;
    
    e = FcPatternFind (p, object, FcTrue);
    if (!e)
	goto bail2;
    
    if (append)
    {
	for (prev = &e->values; *prev; prev = &(*prev)->next);
	*prev = new;
    }
    else
    {
	new->next = e->values;
	e->values = new;
    }
    
    return FcTrue;

bail2:    
    switch (value.type) {
    case FcTypeString:
	FcStrFree ((FcChar8 *) value.u.s);
	break;
    case FcTypeMatrix:
	FcMatrixFree ((FcMatrix *) value.u.m);
	break;
    case FcTypeCharSet:
	FcCharSetDestroy ((FcCharSet *) value.u.c);
	break;
    default:
	break;
    }
bail1:
    FcMemFree (FC_MEM_VALLIST, sizeof (FcValueList));
    free (new);
bail0:
    return FcFalse;
}

FcBool
FcPatternDel (FcPattern *p, const char *object)
{
    FcPatternElt   *e;
    int		    i;

    e = FcPatternFind (p, object, FcFalse);
    if (!e)
	return FcFalse;

    i = e - p->elts;
    
    /* destroy value */
    FcValueListDestroy (e->values);
    
    /* shuffle existing ones down */
    memmove (e, e+1, (p->elts + p->num - (e + 1)) * sizeof (FcPatternElt));
    p->num--;
    p->elts[p->num].object = 0;
    p->elts[p->num].values = 0;
    return FcTrue;
}

FcBool
FcPatternAddInteger (FcPattern *p, const char *object, int i)
{
    FcValue	v;

    v.type = FcTypeInteger;
    v.u.i = i;
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddDouble (FcPattern *p, const char *object, double d)
{
    FcValue	v;

    v.type = FcTypeDouble;
    v.u.d = d;
    return FcPatternAdd (p, object, v, FcTrue);
}


FcBool
FcPatternAddString (FcPattern *p, const char *object, const FcChar8 *s)
{
    FcValue	v;

    v.type = FcTypeString;
    v.u.s = s;
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddMatrix (FcPattern *p, const char *object, const FcMatrix *s)
{
    FcValue	v;

    v.type = FcTypeMatrix;
    v.u.m = (FcMatrix *) s;
    return FcPatternAdd (p, object, v, FcTrue);
}


FcBool
FcPatternAddBool (FcPattern *p, const char *object, FcBool b)
{
    FcValue	v;

    v.type = FcTypeBool;
    v.u.b = b;
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddCharSet (FcPattern *p, const char *object, const FcCharSet *c)
{
    FcValue	v;

    v.type = FcTypeCharSet;
    v.u.c = (FcCharSet *) c;
    return FcPatternAdd (p, object, v, FcTrue);
}

FcResult
FcPatternGet (FcPattern *p, const char *object, int id, FcValue *v)
{
    FcPatternElt   *e;
    FcValueList    *l;

    e = FcPatternFind (p, object, FcFalse);
    if (!e)
	return FcResultNoMatch;
    for (l = e->values; l; l = l->next)
    {
	if (!id)
	{
	    *v = l->value;
	    return FcResultMatch;
	}
	id--;
    }
    return FcResultNoId;
}

FcResult
FcPatternGetInteger (FcPattern *p, const char *object, int id, int *i)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    switch (v.type) {
    case FcTypeDouble:
	*i = (int) v.u.d;
	break;
    case FcTypeInteger:
	*i = v.u.i;
	break;
    default:
        return FcResultTypeMismatch;
    }
    return FcResultMatch;
}

FcResult
FcPatternGetDouble (FcPattern *p, const char *object, int id, double *d)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    switch (v.type) {
    case FcTypeDouble:
	*d = v.u.d;
	break;
    case FcTypeInteger:
	*d = (double) v.u.i;
	break;
    default:
        return FcResultTypeMismatch;
    }
    return FcResultMatch;
}

FcResult
FcPatternGetString (FcPattern *p, const char *object, int id, FcChar8 ** s)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeString)
        return FcResultTypeMismatch;
    *s = (FcChar8 *) v.u.s;
    return FcResultMatch;
}

FcResult
FcPatternGetMatrix (FcPattern *p, const char *object, int id, FcMatrix **m)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeMatrix)
        return FcResultTypeMismatch;
    *m = (FcMatrix *) v.u.m;
    return FcResultMatch;
}


FcResult
FcPatternGetBool (FcPattern *p, const char *object, int id, FcBool *b)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeBool)
        return FcResultTypeMismatch;
    *b = v.u.b;
    return FcResultMatch;
}

FcResult
FcPatternGetCharSet (FcPattern *p, const char *object, int id, FcCharSet **c)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeCharSet)
        return FcResultTypeMismatch;
    *c = (FcCharSet *) v.u.c;
    return FcResultMatch;
}

FcPattern *
FcPatternDuplicate (FcPattern *orig)
{
    FcPattern	    *new;
    int		    i;
    FcValueList    *l;

    new = FcPatternCreate ();
    if (!new)
	goto bail0;

    for (i = 0; i < orig->num; i++)
    {
	for (l = orig->elts[i].values; l; l = l->next)
	    if (!FcPatternAdd (new, orig->elts[i].object, l->value, FcTrue))
		goto bail1;
    }

    return new;

bail1:
    FcPatternDestroy (new);
bail0:
    return 0;
}

FcPattern *
FcPatternVaBuild (FcPattern *orig, va_list va)
{
    FcPattern	*ret;
    
    FcPatternVapBuild (ret, orig, va);
    return ret;
}

FcPattern *
FcPatternBuild (FcPattern *orig, ...)
{
    va_list	va;
    
    va_start (va, orig);
    FcPatternVapBuild (orig, orig, va);
    va_end (va);
    return orig;
}
