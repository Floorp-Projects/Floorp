/*
 * $XFree86: xc/lib/Xft/xftpat.c,v 1.7 2002/02/15 07:36:11 keithp Exp $
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
#include "xftint.h"

FcPattern *
XftPatternCreate (void)
{
    FcPattern	*p;

    p = (FcPattern *) malloc (sizeof (FcPattern));
    if (!p)
	return 0;
    p->num = 0;
    p->size = 0;
    p->elts = 0;
    return p;
}

void
XftValueDestroy (FcValue v)
{ FcValueDestroy (v); }

void
XftValueListDestroy (XftValueList *l)
{ FcValueListDestroy (l); }

void
XftPatternDestroy (FcPattern *p)
{ FcPatternDestroy (p); }

XftPatternElt *
XftPatternFind (FcPattern *p, const char *object, Bool insert)
{ return FcPatternFind (p, object, insert); }

Bool
XftPatternAdd (FcPattern *p, const char *object, FcValue value, Bool append)
{ return FcPatternAdd (p, object, value, append); }

Bool
XftPatternDel (FcPattern *p, const char *object)
{
    XftPatternElt   *e;
    int		    i;

    e = XftPatternFind (p, object, False);
    if (!e)
	return False;

    i = e - p->elts;
    
    /* destroy value */
    XftValueListDestroy (e->values);
    
    /* shuffle existing ones down */
    memmove (e, e+1, (p->elts + p->num - (e + 1)) * sizeof (XftPatternElt));
    p->num--;
    p->elts[p->num].object = 0;
    p->elts[p->num].values = 0;
    return True;
}

Bool
XftPatternAddInteger (FcPattern *p, const char *object, int i)
{
    FcValue	v;

    v.type = FcTypeInteger;
    v.u.i = i;
    return XftPatternAdd (p, object, v, True);
}

Bool
XftPatternAddDouble (FcPattern *p, const char *object, double d)
{
    FcValue	v;

    v.type = FcTypeDouble;
    v.u.d = d;
    return XftPatternAdd (p, object, v, True);
}


Bool
XftPatternAddString (FcPattern *p, const char *object, const char *s)
{
    FcValue	v;

    v.type = FcTypeString;
    v.u.s = (char *) s;
    return XftPatternAdd (p, object, v, True);
}

Bool
XftPatternAddMatrix (FcPattern *p, const char *object, const XftMatrix *s)
{
    FcValue	v;

    v.type = FcTypeMatrix;
    v.u.m = (XftMatrix *) s;
    return XftPatternAdd (p, object, v, True);
}


Bool
XftPatternAddBool (FcPattern *p, const char *object, Bool b)
{
    FcValue	v;

    v.type = FcTypeBool;
    v.u.b = b;
    return XftPatternAdd (p, object, v, True);
}

XftResult
XftPatternGet (FcPattern *p, const char *object, int id, FcValue *v)
{
    XftPatternElt   *e;
    XftValueList    *l;

    e = XftPatternFind (p, object, False);
    if (!e)
	return XftResultNoMatch;
    for (l = e->values; l; l = l->next)
    {
	if (!id)
	{
	    *v = l->value;
	    return XftResultMatch;
	}
	id--;
    }
    return XftResultNoId;
}

XftResult
XftPatternGetInteger (FcPattern *p, const char *object, int id, int *i)
{
    FcValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    switch (v.type) {
    case FcTypeDouble:
	*i = (int) v.u.d;
	break;
    case FcTypeInteger:
	*i = v.u.i;
	break;
    default:
        return XftResultTypeMismatch;
    }
    return XftResultMatch;
}

XftResult
XftPatternGetDouble (FcPattern *p, const char *object, int id, double *d)
{
    FcValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    switch (v.type) {
    case FcTypeDouble:
	*d = v.u.d;
	break;
    case FcTypeInteger:
	*d = (double) v.u.i;
	break;
    default:
        return XftResultTypeMismatch;
    }
    return XftResultMatch;
}

XftResult
XftPatternGetString (FcPattern *p, const char *object, int id, char **s)
{
    FcValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    if (v.type != FcTypeString)
        return XftResultTypeMismatch;
    *s = v.u.s;
    return XftResultMatch;
}

XftResult
XftPatternGetMatrix (FcPattern *p, const char *object, int id, XftMatrix **m)
{
    FcValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    if (v.type != FcTypeMatrix)
        return XftResultTypeMismatch;
    *m = v.u.m;
    return XftResultMatch;
}


XftResult
XftPatternGetBool (FcPattern *p, const char *object, int id, Bool *b)
{
    FcValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    if (v.type != FcTypeBool)
        return XftResultTypeMismatch;
    *b = v.u.b;
    return XftResultMatch;
}

FcPattern *
XftPatternDuplicate (FcPattern *orig)
{
    FcPattern	    *new;
    int		    i;
    XftValueList    *l;

    new = XftPatternCreate ();
    if (!new)
	goto bail0;

    for (i = 0; i < orig->num; i++)
    {
	for (l = orig->elts[i].values; l; l = l->next)
	    if (!XftPatternAdd (new, orig->elts[i].object, l->value, True))
		goto bail1;
    }

    return new;

bail1:
    XftPatternDestroy (new);
bail0:
    return 0;
}

FcPattern *
XftPatternVaBuild (FcPattern *orig, va_list va)
{
    FcPattern	*ret;
    
    _XftPatternVapBuild (ret, orig, va);
    return ret;
}

FcPattern *
XftPatternBuild (FcPattern *orig, ...)
{
    va_list	va;
    
    va_start (va, orig);
    _XftPatternVapBuild (orig, orig, va);
    va_end (va);
    return orig;
}
