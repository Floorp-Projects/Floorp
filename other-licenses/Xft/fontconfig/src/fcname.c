/*
 * $XFree86: xc/lib/fontconfig/src/fcname.c,v 1.3 2002/02/18 22:29:28 keithp Exp $
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fcint.h"

static const FcObjectType _FcBaseObjectTypes[] = {
    { FC_FAMILY,	FcTypeString, },
    { FC_STYLE,		FcTypeString, },
    { FC_SLANT,		FcTypeInteger, },
    { FC_WEIGHT,	FcTypeInteger, },
    { FC_SIZE,		FcTypeDouble, },
    { FC_PIXEL_SIZE,	FcTypeDouble, },
    { FC_SPACING,	FcTypeInteger, },
    { FC_FOUNDRY,	FcTypeString, },
/*    { FC_CORE,		FcTypeBool, }, */
    { FC_ANTIALIAS,	FcTypeBool, },
/*    { FC_XLFD,		FcTypeString, }, */
    { FC_FILE,		FcTypeString, },
    { FC_INDEX,		FcTypeInteger, },
    { FC_RASTERIZER,	FcTypeString, },
    { FC_OUTLINE,	FcTypeBool, },
    { FC_SCALABLE,	FcTypeBool, },
    { FC_RGBA,		FcTypeInteger, },
    { FC_SCALE,		FcTypeDouble, },
/*    { FC_RENDER,	FcTypeBool, },*/
    { FC_MINSPACE,	FcTypeBool, },
    { FC_CHAR_WIDTH,	FcTypeInteger },
    { FC_CHAR_HEIGHT,	FcTypeInteger },
    { FC_MATRIX,	FcTypeMatrix },
    { FC_CHARSET,	FcTypeCharSet },
    { FC_LANG,		FcTypeString },
};

#define NUM_OBJECT_TYPES    (sizeof _FcBaseObjectTypes / sizeof _FcBaseObjectTypes[0])

typedef struct _FcObjectTypeList    FcObjectTypeList;

struct _FcObjectTypeList {
    const FcObjectTypeList  *next;
    const FcObjectType	    *types;
    int			    ntypes;
};

static const FcObjectTypeList _FcBaseObjectTypesList = {
    0,
    _FcBaseObjectTypes,
    NUM_OBJECT_TYPES
};

static const FcObjectTypeList	*_FcObjectTypes = &_FcBaseObjectTypesList;

FcBool
FcNameRegisterObjectTypes (const FcObjectType *types, int ntypes)
{
    FcObjectTypeList	*l;

    l = (FcObjectTypeList *) malloc (sizeof (FcObjectTypeList));
    if (!l)
	return FcFalse;
    l->types = types;
    l->ntypes = ntypes;
    l->next = _FcObjectTypes;
    _FcObjectTypes = l;
    return FcTrue;
}

FcBool
FcNameUnregisterObjectTypes (const FcObjectType *types, int ntypes)
{
    const FcObjectTypeList	*l, **prev;

    for (prev = &_FcObjectTypes; 
	 (l = *prev); 
	 prev = (const FcObjectTypeList **) &(l->next))
    {
	if (l->types == types && l->ntypes == ntypes)
	{
	    *prev = l->next;
	    free ((void *) l);
	    return FcTrue;
	}
    }
    return FcFalse;
}

const FcObjectType *
FcNameGetObjectType (const char *object)
{
    int			    i;
    const FcObjectTypeList  *l;
    const FcObjectType	    *t;
    
    for (l = _FcObjectTypes; l; l = l->next)
    {
	for (i = 0; i < l->ntypes; i++)
	{
	    t = &l->types[i];
	    if (!FcStrCmpIgnoreCase ((FcChar8 *) object, (FcChar8 *) t->object))
		return t;
	}
    }
    return 0;
}

static const FcConstant _FcBaseConstants[] = {
    { (FcChar8 *) "light",	    "weight",   FC_WEIGHT_LIGHT, },
    { (FcChar8 *) "medium",	    "weight",   FC_WEIGHT_MEDIUM, },
    { (FcChar8 *) "demibold",	    "weight",   FC_WEIGHT_DEMIBOLD, },
    { (FcChar8 *) "bold",	    "weight",   FC_WEIGHT_BOLD, },
    { (FcChar8 *) "black",	    "weight",   FC_WEIGHT_BLACK, },

    { (FcChar8 *) "roman",	    "slant",    FC_SLANT_ROMAN, },
    { (FcChar8 *) "italic",	    "slant",    FC_SLANT_ITALIC, },
    { (FcChar8 *) "oblique",	    "slant",    FC_SLANT_OBLIQUE, },

    { (FcChar8 *) "proportional",   "spacing",  FC_PROPORTIONAL, },
    { (FcChar8 *) "mono",	    "spacing",  FC_MONO, },
    { (FcChar8 *) "charcell",	    "spacing",  FC_CHARCELL, },

    { (FcChar8 *) "rgb",	    "rgba",	    FC_RGBA_RGB, },
    { (FcChar8 *) "bgr",	    "rgba",	    FC_RGBA_BGR, },
    { (FcChar8 *) "vrgb",	    "rgba",	    FC_RGBA_VRGB },
    { (FcChar8 *) "vbgr",	    "rgba",	    FC_RGBA_VBGR },
};

#define NUM_FC_CONSTANTS   (sizeof _FcBaseConstants/sizeof _FcBaseConstants[0])

typedef struct _FcConstantList FcConstantList;

struct _FcConstantList {
    const FcConstantList    *next;
    const FcConstant	    *consts;
    int			    nconsts;
};

static const FcConstantList _FcBaseConstantList = {
    0,
    _FcBaseConstants,
    NUM_FC_CONSTANTS
};

static const FcConstantList	*_FcConstants = &_FcBaseConstantList;

FcBool
FcNameRegisterConstants (const FcConstant *consts, int nconsts)
{
    FcConstantList	*l;

    l = (FcConstantList *) malloc (sizeof (FcConstantList));
    if (!l)
	return FcFalse;
    l->consts = consts;
    l->nconsts = nconsts;
    l->next = _FcConstants;
    _FcConstants = l;
    return FcTrue;
}

FcBool
FcNameUnregisterConstants (const FcConstant *consts, int nconsts)
{
    const FcConstantList	*l, **prev;

    for (prev = &_FcConstants; 
	 (l = *prev); 
	 prev = (const FcConstantList **) &(l->next))
    {
	if (l->consts == consts && l->nconsts == nconsts)
	{
	    *prev = l->next;
	    free ((void *) l);
	    return FcTrue;
	}
    }
    return FcFalse;
}

const FcConstant *
FcNameGetConstant (FcChar8 *string)
{
    const FcConstantList    *l;
    int			    i;
    
    for (l = _FcConstants; l; l = l->next)
    {
	for (i = 0; i < l->nconsts; i++)
	    if (!FcStrCmpIgnoreCase (string, l->consts[i].name))
		return &l->consts[i];
    }
    return 0;
}

FcBool
FcNameConstant (FcChar8 *string, int *result)
{
    const FcConstant	*c;

    if ((c = FcNameGetConstant(string)))
    {
	*result = c->value;
	return FcTrue;
    }
    return FcFalse;
}

FcBool
FcNameBool (FcChar8 *v, FcBool *result)
{
    char    c0, c1;

    c0 = *v;
    if (isupper (c0))
	c0 = tolower (c0);
    if (c0 == 't' || c0 == 'y' || c0 == '1')
    {
	*result = FcTrue;
	return FcTrue;
    }
    if (c0 == 'f' || c0 == 'n' || c0 == '0')
    {
	*result = FcFalse;
	return FcTrue;
    }
    if (c0 == 'o')
    {
	c1 = v[1];
	if (isupper (c1))
	    c1 = tolower (c1);
	if (c1 == 'n')
	{
	    *result = FcTrue;
	    return FcTrue;
	}
	if (c1 == 'f')
	{
	    *result = FcFalse;
	    return FcTrue;
	}
    }
    return FcFalse;
}

static FcValue
FcNameConvert (FcType type, FcChar8 *string, FcMatrix *m)
{
    FcValue	v;

    v.type = type;
    switch (v.type) {
    case FcTypeInteger:
	if (!FcNameConstant (string, &v.u.i))
	    v.u.i = atoi ((char *) string);
	break;
    case FcTypeString:
	v.u.s = string;
	break;
    case FcTypeBool:
	if (!FcNameBool (string, &v.u.b))
	    v.u.b = FcFalse;
	break;
    case FcTypeDouble:
	v.u.d = strtod ((char *) string, 0);
	break;
    case FcTypeMatrix:
	v.u.m = m;
	sscanf ((char *) string, "%lg %lg %lg %lg", &m->xx, &m->xy, &m->yx, &m->yy);
	break;
    case FcTypeCharSet:
	v.u.c = FcNameParseCharSet (string);
	break;
    default:
	break;
    }
    return v;
}

static const FcChar8 *
FcNameFindNext (const FcChar8 *cur, const char *delim, FcChar8 *save, FcChar8 *last)
{
    FcChar8    c;
    
    while ((c = *cur))
    {
	if (c == '\\')
	{
	    ++cur;
	    if (!(c = *cur))
		break;
	}
	else if (strchr (delim, c))
	    break;
	++cur;
	*save++ = c;
    }
    *save = 0;
    *last = *cur;
    if (*cur)
	cur++;
    return cur;
}

FcPattern *
FcNameParse (const FcChar8 *name)
{
    FcChar8		*save;
    FcPattern		*pat;
    double		d;
    FcChar8		*e;
    FcChar8		delim;
    FcValue		v;
    FcMatrix		m;
    const FcObjectType	*t;
    const FcConstant	*c;

    save = malloc (strlen ((char *) name) + 1);
    if (!save)
	goto bail0;
    pat = FcPatternCreate ();
    if (!pat)
	goto bail1;

    for (;;)
    {
	name = FcNameFindNext (name, "-,:", save, &delim);
	if (save[0])
	{
	    if (!FcPatternAddString (pat, FC_FAMILY, save))
		goto bail2;
	}
	if (delim != ',')
	    break;
    }
    if (delim == '-')
    {
	for (;;)
	{
	    name = FcNameFindNext (name, "-,:", save, &delim);
	    d = strtod ((char *) save, (char **) &e);
	    if (e != save)
	    {
		if (!FcPatternAddDouble (pat, FC_SIZE, d))
		    goto bail2;
	    }
	    if (delim != ',')
		break;
	}
    }
    while (delim == ':')
    {
	name = FcNameFindNext (name, "=_:", save, &delim);
	if (save[0])
	{
	    if (delim == '=' || delim == '_')
	    {
		t = FcNameGetObjectType ((char *) save);
		for (;;)
		{
		    name = FcNameFindNext (name, ":,", save, &delim);
		    if (save[0] && t)
		    {
			v = FcNameConvert (t->type, save, &m);
			if (!FcPatternAdd (pat, t->object, v, FcTrue))
			{
			    if (v.type == FcTypeCharSet)
				FcCharSetDestroy ((FcCharSet *) v.u.c);
			    goto bail2;
			}
			if (v.type == FcTypeCharSet)
			    FcCharSetDestroy ((FcCharSet *) v.u.c);
		    }
		    if (delim != ',')
			break;
		}
	    }
	    else
	    {
		if ((c = FcNameGetConstant (save)))
		{
		    if (!FcPatternAddInteger (pat, c->object, c->value))
			goto bail2;
		}
	    }
	}
    }

    free (save);
    return pat;

bail2:
    FcPatternDestroy (pat);
bail1:
    free (save);
bail0:
    return 0;
}
static FcBool
FcNameUnparseString (FcStrBuf	    *buf, 
		     const FcChar8  *string,
		     const FcChar8  *escape)
{
    FcChar8 c;
    while ((c = *string++))
    {
	if (escape && strchr ((char *) escape, (char) c))
	{
	    if (!FcStrBufChar (buf, escape[0]))
		return FcFalse;
	}
	if (!FcStrBufChar (buf, c))
	    return FcFalse;
    }
    return FcTrue;
}

static FcBool
FcNameUnparseValue (FcStrBuf	*buf,
		    FcValue	v,
		    FcChar8	*escape)
{
    FcChar8	temp[1024];
    
    switch (v.type) {
    case FcTypeVoid:
	return FcTrue;
    case FcTypeInteger:
	sprintf ((char *) temp, "%d", v.u.i);
	return FcNameUnparseString (buf, temp, 0);
    case FcTypeDouble:
	sprintf ((char *) temp, "%g", v.u.d);
	return FcNameUnparseString (buf, temp, 0);
    case FcTypeString:
	return FcNameUnparseString (buf, v.u.s, escape);
    case FcTypeBool:
	return FcNameUnparseString (buf, v.u.b ? (FcChar8 *) "True" : (FcChar8 *) "False", 0);
    case FcTypeMatrix:
	sprintf ((char *) temp, "%g %g %g %g", 
		 v.u.m->xx, v.u.m->xy, v.u.m->yx, v.u.m->yy);
	return FcNameUnparseString (buf, temp, 0);
    case FcTypeCharSet:
	return FcNameUnparseCharSet (buf, v.u.c);
    }
    return FcFalse;
}

static FcBool
FcNameUnparseValueList (FcStrBuf	*buf,
			FcValueList	*v,
			FcChar8		*escape)
{
    while (v)
    {
	if (!FcNameUnparseValue (buf, v->value, escape))
	    return FcFalse;
	if ((v = v->next))
	    if (!FcNameUnparseString (buf, (FcChar8 *) ",", 0))
		return FcFalse;
    }
    return FcTrue;
}

#define FC_ESCAPE_FIXED    "\\-:,"
#define FC_ESCAPE_VARIABLE "\\=_:,"

FcChar8 *
FcNameUnparse (FcPattern *pat)
{
    FcStrBuf		    buf;
    FcChar8		    buf_static[8192];
    int			    i;
    FcPatternElt	    *e;
    const FcObjectTypeList  *l;
    const FcObjectType	    *o;

    FcStrBufInit (&buf, buf_static, sizeof (buf_static));
    e = FcPatternFind (pat, FC_FAMILY, FcFalse);
    if (e)
    {
	if (!FcNameUnparseValueList (&buf, e->values, (FcChar8 *) FC_ESCAPE_FIXED))
	    goto bail0;
    }
    e = FcPatternFind (pat, FC_SIZE, FcFalse);
    if (e)
    {
	if (!FcNameUnparseString (&buf, (FcChar8 *) "-", 0))
	    goto bail0;
	if (!FcNameUnparseValueList (&buf, e->values, (FcChar8 *) FC_ESCAPE_FIXED))
	    goto bail0;
    }
    for (l = _FcObjectTypes; l; l = l->next)
    {
	for (i = 0; i < l->ntypes; i++)
	{
	    o = &l->types[i];
	    if (!strcmp (o->object, FC_FAMILY) || 
		!strcmp (o->object, FC_SIZE) ||
		!strcmp (o->object, FC_FILE))
		continue;
	    
	    e = FcPatternFind (pat, o->object, FcFalse);
	    if (e)
	    {
		if (!FcNameUnparseString (&buf, (FcChar8 *) ":", 0))
		    goto bail0;
		if (!FcNameUnparseString (&buf, (FcChar8 *) o->object, (FcChar8 *) FC_ESCAPE_VARIABLE))
		    goto bail0;
		if (!FcNameUnparseString (&buf, (FcChar8 *) "=", 0))
		    goto bail0;
		if (!FcNameUnparseValueList (&buf, e->values, 
					     (FcChar8 *) FC_ESCAPE_VARIABLE))
		    goto bail0;
	    }
	}
    }
    return FcStrBufDone (&buf);
bail0:
    FcStrBufDestroy (&buf);
    return 0;
}
