/*
 * $XFree86: xc/lib/fontconfig/src/fccfg.c,v 1.3 2002/02/19 08:33:23 keithp Exp $
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
#include <stdio.h>
#include "fcint.h"

FcConfig    *_fcConfig;

FcConfig *
FcConfigCreate (void)
{
    FcSetName	set;
    FcConfig	*config;

    config = malloc (sizeof (FcConfig));
    if (!config)
	goto bail0;
    
    config->dirs = malloc (sizeof (char *));
    if (!config->dirs)
	goto bail1;
    config->dirs[0] = 0;
    
    config->configFiles = malloc (sizeof (char *));
    if (!config->configFiles)
	goto bail2;
    config->configFiles[0] = 0;
    
    config->cache = 0;
    if (!FcConfigSetCache (config, (FcChar8 *) ("~/" FC_USER_CACHE_FILE)))
	goto bail3;

    config->blanks = 0;

    config->substPattern = 0;
    config->substFont = 0;
    config->maxObjects = 0;
    for (set = FcSetSystem; set <= FcSetApplication; set++)
	config->fonts[set] = 0;
    
    return config;

bail3:
    free (config->configFiles);
bail2:
    free (config->dirs);
bail1:
    free (config);
bail0:
    return 0;
}

static void
FcSubstDestroy (FcSubst *s)
{
    FcSubst *n;
    
    while (s)
    {
	n = s->next;
	FcTestDestroy (s->test);
	FcEditDestroy (s->edit);
	s = n;
    }
}

static void
FcConfigDestroyStrings (FcChar8 **strings)
{
    FcChar8    **s;

    for (s = strings; s && *s; s++)
	free (*s);
    if (strings)
	free (strings);
}
    
static FcBool
FcConfigAddString (FcChar8 ***strings, FcChar8 *string)
{
    int	    n;
    FcChar8    **s;
    
    n = 0;
    for (s = *strings; s && *s; s++)
	n++;
    s = malloc ((n + 2) * sizeof (FcChar8 *));
    if (!s)
	return FcFalse;
    s[n] = string;
    s[n+1] = 0;
    memcpy (s, *strings, n * sizeof (FcChar8 *));
    free (*strings);
    *strings = s;
    return FcTrue;
}

void
FcConfigDestroy (FcConfig *config)
{
    FcSetName	set;
    FcConfigDestroyStrings (config->dirs);
    FcConfigDestroyStrings (config->configFiles);

    free (config->cache);

    FcSubstDestroy (config->substPattern);
    FcSubstDestroy (config->substFont);
    for (set = FcSetSystem; set <= FcSetApplication; set++)
	if (config->fonts[set])
	    FcFontSetDestroy (config->fonts[set]);
}

/*
 * Scan the current list of directories in the configuration
 * and build the set of available fonts. Update the
 * per-user cache file to reflect the new configuration
 */

FcBool
FcConfigBuildFonts (FcConfig *config)
{
    FcFontSet   *fonts;
    FcFileCache *cache;
    FcChar8	**d;

    fonts = FcFontSetCreate ();
    if (!fonts)
	goto bail0;
    
    cache = FcFileCacheCreate ();
    if (!cache)
	goto bail1;

    FcFileCacheLoad (cache, config->cache);

    for (d = config->dirs; d && *d; d++)
    {
	if (FcDebug () & FC_DBG_FONTSET)
	    printf ("scan dir %s\n", *d);
	FcDirScan (fonts, cache, config->blanks, *d, FcFalse);
    }
    
    if (FcDebug () & FC_DBG_FONTSET)
	FcFontSetPrint (fonts);

    FcFileCacheSave (cache, config->cache);
    FcFileCacheDestroy (cache);

    FcConfigSetFonts (config, fonts, FcSetSystem);
    
    return FcTrue;
bail1:
    FcFontSetDestroy (fonts);
bail0:
    return FcFalse;
}

FcBool
FcConfigSetCurrent (FcConfig *config)
{
    if (!config->fonts)
	if (!FcConfigBuildFonts (config))
	    return FcFalse;

    if (_fcConfig)
	FcConfigDestroy (_fcConfig);
    _fcConfig = config;
    return FcTrue;
}

FcConfig *
FcConfigGetCurrent (void)
{
    if (!_fcConfig)
	if (!FcInit ())
	    return 0;
    return _fcConfig;
}

FcBool
FcConfigAddDir (FcConfig    *config,
		const FcChar8  *d)
{
    FcChar8    *dir;
    FcChar8    *h;

    if (*d == '~')
    {
	h = (FcChar8 *) getenv ("HOME");
	if (!h)
	    return FcFalse;
	dir = (FcChar8 *) malloc (strlen ((char *) h) + strlen ((char *) d));
	if (!dir)
	    return FcFalse;
	strcpy ((char *) dir, (char *) h);
	strcat ((char *) dir, (char *) d+1);
    }
    else
    {
	dir = (FcChar8 *) malloc (strlen ((char *) d) + 1);
	if (!dir)
	    return FcFalse;
	strcpy (dir, d);
    }
    if (!FcConfigAddString (&config->dirs, dir))
    {
	free (dir);
	return FcFalse;
    }
    return FcTrue;
}

FcChar8 **
FcConfigGetDirs (FcConfig   *config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->dirs;
}

FcBool
FcConfigAddConfigFile (FcConfig	    *config,
		       const FcChar8   *f)
{
    FcChar8    *file;
    file = FcConfigFilename (f);
    if (!file)
	return FcFalse;
    if (!FcConfigAddString (&config->configFiles, file))
    {
	free (file);
	return FcFalse;
    }
    return FcTrue;
}

FcChar8 **
FcConfigGetConfigFiles (FcConfig    *config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->configFiles;
}

FcBool
FcConfigSetCache (FcConfig	*config,
		  const FcChar8	*c)
{
    FcChar8    *new;
    FcChar8    *h;

    if (*c == '~')
    {
	h = (FcChar8 *) getenv ("HOME");
	if (!h)
	    return FcFalse;
	new = (FcChar8 *) malloc (strlen ((char *) h) + strlen ((char *) c));
	if (!new)
	    return FcFalse;
	strcpy ((char *) new, (char *) h);
	strcat ((char *) new, (char *) c+1);
    }
    else
    {
	new = FcStrCopy (c);
    }
    if (config->cache)
	free (config->cache);
    config->cache = new;
    return FcTrue;
}

FcChar8 *
FcConfigGetCache (FcConfig  *config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->cache;
}

FcFontSet *
FcConfigGetFonts (FcConfig	*config,
		  FcSetName	set)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->fonts[set];
}

void
FcConfigSetFonts (FcConfig	*config,
		  FcFontSet	*fonts,
		  FcSetName	set)
{
    if (config->fonts[set])
	FcFontSetDestroy (config->fonts[set]);
    config->fonts[set] = fonts;
}

FcBlanks *
FcConfigGetBlanks (FcConfig	*config)
{
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    return config->blanks;
}

FcBool
FcConfigAddBlank (FcConfig	*config,
		  FcChar32    	blank)
{
    FcBlanks	*b;
    
    b = config->blanks;
    if (!b)
    {
	b = FcBlanksCreate ();
	if (!b)
	    return FcFalse;
    }
    if (!FcBlanksAdd (b, blank))
	return FcFalse;
    config->blanks = b;
    return FcTrue;
}

FcBool
FcConfigAddEdit (FcConfig	*config,
		 FcTest		*test,
		 FcEdit		*edit,
		 FcMatchKind	kind)
{
    FcSubst	*subst, **prev;
    FcTest	*t;
    int		num;

    subst = (FcSubst *) malloc (sizeof (FcSubst));
    if (!subst)
	return FcFalse;
    if (kind == FcMatchPattern)
	prev = &config->substPattern;
    else
	prev = &config->substFont;
    for (; *prev; prev = &(*prev)->next);
    *prev = subst;
    subst->next = 0;
    subst->test = test;
    subst->edit = edit;
    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("Add Subst ");
	FcSubstPrint (subst);
    }
    num = 0;
    for (t = test; t; t = t->next)
	num++;
    if (config->maxObjects < num)
	config->maxObjects = num;
    return FcTrue;
}

typedef struct _FcSubState {
    FcPatternElt   *elt;
    FcValueList    *value;
} FcSubState;

static const FcMatrix    FcIdentityMatrix = { 1, 0, 0, 1 };

static FcValue
FcConfigPromote (FcValue v, FcValue u)
{
    if (v.type == FcTypeInteger)
    {
	v.type = FcTypeDouble;
	v.u.d = (double) v.u.i;
    }
    else if (v.type == FcTypeVoid && u.type == FcTypeMatrix)
    {
	v.u.m = (FcMatrix *) &FcIdentityMatrix;
	v.type = FcTypeMatrix;
    }
    return v;
}

FcBool
FcConfigCompareValue (FcValue	m,
		      FcOp	op,
		      FcValue	v)
{
    FcBool    ret = FcFalse;
    
    m = FcConfigPromote (m, v);
    v = FcConfigPromote (v, m);
    if (m.type == v.type) 
    {
	ret = FcFalse;
	switch (m.type) {
	case FcTypeInteger:
	    break;	/* FcConfigPromote prevents this from happening */
	case FcTypeDouble:
	    switch (op) {
	    case FcOpEqual:
	    case FcOpContains:
		ret = m.u.d == v.u.d;
		break;
	    case FcOpNotEqual:    
		ret = m.u.d != v.u.d;
		break;
	    case FcOpLess:    
		ret = m.u.d < v.u.d;
		break;
	    case FcOpLessEqual:    
		ret = m.u.d <= v.u.d;
		break;
	    case FcOpMore:    
		ret = m.u.d > v.u.d;
		break;
	    case FcOpMoreEqual:    
		ret = m.u.d >= v.u.d;
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeBool:
	    switch (op) {
	    case FcOpEqual:    
	    case FcOpContains:
		ret = m.u.b == v.u.b;
		break;
	    case FcOpNotEqual:    
		ret = m.u.b != v.u.b;
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeString:
	    switch (op) {
	    case FcOpEqual:    
	    case FcOpContains:
		ret = FcStrCmpIgnoreCase (m.u.s, v.u.s) == 0;
		break;
	    case FcOpNotEqual:    
		ret = FcStrCmpIgnoreCase (m.u.s, v.u.s) != 0;
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeMatrix:
	    switch (op) {
	    case FcOpEqual:
	    case FcOpContains:
		ret = FcMatrixEqual (m.u.m, v.u.m);
		break;
	    case FcOpNotEqual:
		ret = !FcMatrixEqual (m.u.m, v.u.m);
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeCharSet:
	    switch (op) {
	    case FcOpContains:
		/* m contains v if v - m is empty */
		ret = FcCharSetSubtractCount (v.u.c, m.u.c) == 0;
		break;
	    case FcOpEqual:
		ret = FcCharSetEqual (m.u.c, v.u.c);
		break;
	    case FcOpNotEqual:
		ret = !FcCharSetEqual (m.u.c, v.u.c);
		break;
	    default:
		break;
	    }
	    break;
	case FcTypeVoid:
	    switch (op) {
	    case FcOpEqual:
	    case FcOpContains:
		ret = FcTrue;
		break;
	    default:
		break;
	    }
	    break;
	}
    }
    else
    {
	if (op == FcOpNotEqual)
	    ret = FcTrue;
    }
    return ret;
}


static FcValue
FcConfigEvaluate (FcPattern *p, FcExpr *e)
{
    FcValue	v, vl, vr;
    FcResult	r;
    FcMatrix	*m;
    
    switch (e->op) {
    case FcOpInteger:
	v.type = FcTypeInteger;
	v.u.i = e->u.ival;
	break;
    case FcOpDouble:
	v.type = FcTypeDouble;
	v.u.d = e->u.dval;
	break;
    case FcOpString:
	v.type = FcTypeString;
	v.u.s = e->u.sval;
	v = FcValueSave (v);
	break;
    case FcOpMatrix:
	v.type = FcTypeMatrix;
	v.u.m = e->u.mval;
	v = FcValueSave (v);
	break;
    case FcOpCharSet:
	v.type = FcTypeCharSet;
	v.u.c = e->u.cval;
	v = FcValueSave (v);
	break;
    case FcOpBool:
	v.type = FcTypeBool;
	v.u.b = e->u.bval;
	break;
    case FcOpField:
	r = FcPatternGet (p, e->u.field, 0, &v);
	if (r != FcResultMatch)
	    v.type = FcTypeVoid;
	break;
    case FcOpConst:
	if (FcNameConstant (e->u.constant, &v.u.i))
	    v.type = FcTypeInteger;
	else
	    v.type = FcTypeVoid;
	break;
    case FcOpQuest:
	vl = FcConfigEvaluate (p, e->u.tree.left);
	if (vl.type == FcTypeBool)
	{
	    if (vl.u.b)
		v = FcConfigEvaluate (p, e->u.tree.right->u.tree.left);
	    else
		v = FcConfigEvaluate (p, e->u.tree.right->u.tree.right);
	}
	else
	    v.type = FcTypeVoid;
	FcValueDestroy (vl);
	break;
    case FcOpOr:
    case FcOpAnd:
    case FcOpEqual:
    case FcOpContains:
    case FcOpNotEqual:
    case FcOpLess:
    case FcOpLessEqual:
    case FcOpMore:
    case FcOpMoreEqual:
    case FcOpPlus:
    case FcOpMinus:
    case FcOpTimes:
    case FcOpDivide:
	vl = FcConfigEvaluate (p, e->u.tree.left);
	vr = FcConfigEvaluate (p, e->u.tree.right);
	vl = FcConfigPromote (vl, vr);
	vr = FcConfigPromote (vr, vl);
	if (vl.type == vr.type)
	{
	    switch (vl.type) {
	    case FcTypeDouble:
		switch (e->op) {
		case FcOpPlus:	   
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d + vr.u.d; 
		    break;
		case FcOpMinus:
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d - vr.u.d; 
		    break;
		case FcOpTimes:
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d * vr.u.d; 
		    break;
		case FcOpDivide:
		    v.type = FcTypeDouble;
		    v.u.d = vl.u.d / vr.u.d; 
		    break;
		case FcOpEqual:
		case FcOpContains:
		    v.type = FcTypeBool; 
		    v.u.b = vl.u.d == vr.u.d;
		    break;
		case FcOpNotEqual:    
		    v.type = FcTypeBool; 
		    v.u.b = vl.u.d != vr.u.d;
		    break;
		case FcOpLess:    
		    v.type = FcTypeBool; 
		    v.u.b = vl.u.d < vr.u.d;
		    break;
		case FcOpLessEqual:    
		    v.type = FcTypeBool; 
		    v.u.b = vl.u.d <= vr.u.d;
		    break;
		case FcOpMore:    
		    v.type = FcTypeBool; 
		    v.u.b = vl.u.d > vr.u.d;
		    break;
		case FcOpMoreEqual:    
		    v.type = FcTypeBool; 
		    v.u.b = vl.u.d >= vr.u.d;
		    break;
		default:
		    v.type = FcTypeVoid; 
		    break;
		}
		if (v.type == FcTypeDouble &&
		    v.u.d == (double) (int) v.u.d)
		{
		    v.type = FcTypeInteger;
		    v.u.i = (int) v.u.d;
		}
		break;
	    case FcTypeBool:
		switch (e->op) {
		case FcOpOr:
		    v.type = FcTypeBool;
		    v.u.b = vl.u.b || vr.u.b;
		    break;
		case FcOpAnd:
		    v.type = FcTypeBool;
		    v.u.b = vl.u.b && vr.u.b;
		    break;
		case FcOpEqual:
		case FcOpContains:
		    v.type = FcTypeBool;
		    v.u.b = vl.u.b == vr.u.b;
		    break;
		case FcOpNotEqual:
		    v.type = FcTypeBool;
		    v.u.b = vl.u.b != vr.u.b;
		    break;
		default:
		    v.type = FcTypeVoid; 
		    break;
		}
		break;
	    case FcTypeString:
		switch (e->op) {
		case FcOpEqual:
		case FcOpContains:
		    v.type = FcTypeBool;
		    v.u.b = FcStrCmpIgnoreCase (vl.u.s, vr.u.s) == 0;
		    break;
		case FcOpNotEqual:
		    v.type = FcTypeBool;
		    v.u.b = FcStrCmpIgnoreCase (vl.u.s, vr.u.s) != 0;
		    break;
		case FcOpPlus:
		    v.type = FcTypeString;
		    v.u.s = FcStrPlus (vl.u.s, vr.u.s);
		    if (!v.u.s)
			v.type = FcTypeVoid;
		    break;
		default:
		    v.type = FcTypeVoid;
		    break;
		}
	    case FcTypeMatrix:
		switch (e->op) {
		case FcOpEqual:
		case FcOpContains:
		    v.type = FcTypeBool;
		    v.u.b = FcMatrixEqual (vl.u.m, vr.u.m);
		    break;
		case FcOpNotEqual:
		    v.type = FcTypeBool;
		    v.u.b = FcMatrixEqual (vl.u.m, vr.u.m);
		    break;
		case FcOpTimes:
		    v.type = FcTypeMatrix;
		    m = malloc (sizeof (FcMatrix));
		    if (m)
		    {
			FcMemAlloc (FC_MEM_MATRIX, sizeof (FcMatrix));
			FcMatrixMultiply (m, vl.u.m, vr.u.m);
			v.u.m = m;
		    }
		    else
		    {
			v.type = FcTypeVoid;
		    }
		    break;
		default:
		    v.type = FcTypeVoid;
		    break;
		}
		break;
	    case FcTypeCharSet:
		switch (e->op) {
		case FcOpContains:
		    /* vl contains vr if vr - vl is empty */
		    v.type = FcTypeBool;
		    v.u.b = FcCharSetSubtractCount (vr.u.c, vl.u.c) == 0;
		    break;
		case FcOpEqual:
		    v.type = FcTypeBool;
		    v.u.b = FcCharSetEqual (vl.u.c, vr.u.c);
		    break;
		case FcOpNotEqual:
		    v.type = FcTypeBool;
		    v.u.b = !FcCharSetEqual (vl.u.c, vr.u.c);
		    break;
		default:
		    v.type = FcTypeVoid;
		    break;
		}
		break;
	    default:
		v.type = FcTypeVoid;
		break;
	    }
	}
	else
	    v.type = FcTypeVoid;
	FcValueDestroy (vl);
	FcValueDestroy (vr);
	break;
    case FcOpNot:
	vl = FcConfigEvaluate (p, e->u.tree.left);
	switch (vl.type) {
	case FcTypeBool:
	    v.type = FcTypeBool;
	    v.u.b = !vl.u.b;
	    break;
	default:
	    v.type = FcTypeVoid;
	    break;
	}
	FcValueDestroy (vl);
	break;
    default:
	v.type = FcTypeVoid;
	break;
    }
    return v;
}

static FcValueList *
FcConfigMatchValueList (FcPattern	*p,
			FcTest		*t,
			FcValueList	*v)
{
    FcValueList    *ret = 0;
    FcValue	    value = FcConfigEvaluate (p, t->expr);
    
    for (; v; v = v->next)
    {
	if (FcConfigCompareValue (v->value, t->op, value))
	{
	    if (!ret)
		ret = v;
	}
	else
	{
	    if (t->qual == FcQualAll)
	    {
		ret = 0;
		break;
	    }
	}
    }
    FcValueDestroy (value);
    return ret;
}

static FcValueList *
FcConfigValues (FcPattern *p, FcExpr *e)
{
    FcValueList	*l;
    
    if (!e)
	return 0;
    l = (FcValueList *) malloc (sizeof (FcValueList));
    if (!l)
	return 0;
    FcMemAlloc (FC_MEM_VALLIST, sizeof (FcValueList));
    if (e->op == FcOpComma)
    {
	l->value = FcConfigEvaluate (p, e->u.tree.left);
	l->next  = FcConfigValues (p, e->u.tree.right);
    }
    else
    {
	l->value = FcConfigEvaluate (p, e);
	l->next  = 0;
    }
    while (l->value.type == FcTypeVoid)
    {
	FcValueList	*next = l->next;
	
	FcMemFree (FC_MEM_VALLIST, sizeof (FcValueList));
	free (l);
	l = next;
    }
    return l;
}

static FcBool
FcConfigAdd (FcValueList    **head,
	     FcValueList    *position,
	     FcBool	    append,
	     FcValueList    *new)
{
    FcValueList    **prev, *last;
    
    if (append)
    {
	if (position)
	    prev = &position->next;
	else
	    for (prev = head; *prev; prev = &(*prev)->next)
		;
    }
    else
    {
	if (position)
	{
	    for (prev = head; *prev; prev = &(*prev)->next)
	    {
		if (*prev == position)
		    break;
	    }
	}
	else
	    prev = head;

	if (FcDebug () & FC_DBG_EDIT)
	{
	    if (!*prev)
		printf ("position not on list\n");
	}
    }

    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("%s list before ", append ? "Append" : "Prepend");
	FcValueListPrint (*head);
	printf ("\n");
    }
    
    if (new)
    {
	last = new;
	while (last->next)
	    last = last->next;
    
	last->next = *prev;
	*prev = new;
    }
    
    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("%s list after ", append ? "Append" : "Prepend");
	FcValueListPrint (*head);
	printf ("\n");
    }
    
    return FcTrue;
}

static void
FcConfigDel (FcValueList    **head,
	     FcValueList    *position)
{
    FcValueList    **prev;

    for (prev = head; *prev; prev = &(*prev)->next)
    {
	if (*prev == position)
	{
	    *prev = position->next;
	    position->next = 0;
	    FcValueListDestroy (position);
	    break;
	}
    }
}

static void
FcConfigPatternAdd (FcPattern	*p,
		    const char	*object,
		    FcValueList	*list,
		    FcBool	append)
{
    if (list)
    {
	FcPatternElt    *e = FcPatternFind (p, object, FcTrue);
    
	if (!e)
	    return;
	FcConfigAdd (&e->values, 0, append, list);
    }
}

/*
 * Delete all values associated with a field
 */
static void
FcConfigPatternDel (FcPattern	*p,
		    const char	*object)
{
    FcPatternElt    *e = FcPatternFind (p, object, FcFalse);
    if (!e)
	return;
    while (e->values)
	FcConfigDel (&e->values, e->values);
}

static void
FcConfigPatternCanon (FcPattern	    *p,
		      const char    *object)
{
    FcPatternElt    *e = FcPatternFind (p, object, FcFalse);
    if (!e)
	return;
    if (!e->values)
	FcPatternDel (p, object);
}

FcBool
FcConfigSubstitute (FcConfig	*config,
		    FcPattern	*p,
		    FcMatchKind	kind)
{
    FcSubst	    *s;
    FcSubState	    *st;
    int		    i;
    FcTest	    *t;
    FcEdit	    *e;
    FcValueList	    *l;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }

    st = (FcSubState *) malloc (config->maxObjects * sizeof (FcSubState));
    if (!st && config->maxObjects)
	return FcFalse;
    FcMemAlloc (FC_MEM_SUBSTATE, config->maxObjects * sizeof (FcSubState));

    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("FcConfigSubstitute ");
	FcPatternPrint (p);
    }
    if (kind == FcMatchPattern)
	s = config->substPattern;
    else
	s = config->substFont;
    for (; s; s = s->next)
    {
	/*
	 * Check the tests to see if
	 * they all match the pattern
	 */
	for (t = s->test, i = 0; t; t = t->next, i++)
	{
	    if (FcDebug () & FC_DBG_EDIT)
	    {
		printf ("FcConfigSubstitute test ");
		FcTestPrint (t);
	    }
	    st[i].elt = FcPatternFind (p, t->field, FcFalse);
	    /*
	     * If there's no such field in the font,
	     * then FcQualAll matches while FcQualAny does not
	     */
	    if (!st[i].elt)
	    {
		if (t->qual == FcQualAll)
		{
		    st[i].value = 0;
		    continue;
		}
		else
		    break;
	    }
	    /*
	     * Check to see if there is a match, mark the location
	     * to apply match-relative edits
	     */
	    st[i].value = FcConfigMatchValueList (p, t, st[i].elt->values);
	    if (!st[i].value)
		break;
	}
	if (t)
	{
	    if (FcDebug () & FC_DBG_EDIT)
		printf ("No match\n");
	    continue;
	}
	if (FcDebug () & FC_DBG_EDIT)
	{
	    printf ("Substitute ");
	    FcSubstPrint (s);
	}
	for (e = s->edit; e; e = e->next)
	{
	    /*
	     * Evaluate the list of expressions
	     */
	    l = FcConfigValues (p, e->expr);
	    /*
	     * Locate any test associated with this field
	     */
	    for (t = s->test, i = 0; t; t = t->next, i++)
		if (!FcStrCmpIgnoreCase ((FcChar8 *) t->field, (FcChar8 *) e->field))
		    break;
	    switch (e->op) {
	    case FcOpAssign:
		/*
		 * If there was a test, then replace the matched
		 * value with the new list of values
		 */
		if (t)
		{
		    FcValueList	*thisValue = st[i].value;
		    FcValueList	*nextValue = thisValue ? thisValue->next : 0;
		    
		    /*
		     * Append the new list of values after the current value
		     */
		    FcConfigAdd (&st[i].elt->values, thisValue, FcTrue, l);
		    /*
		     * Delete the marked value
		     */
		    FcConfigDel (&st[i].elt->values, thisValue);
		    /*
		     * Adjust any pointers into the value list to ensure
		     * future edits occur at the same place
		     */
		    for (t = s->test, i = 0; t; t = t->next, i++)
		    {
			if (st[i].value == thisValue)
			    st[i].value = nextValue;
		    }
		    break;
		}
		/* fall through ... */
	    case FcOpAssignReplace:
		/*
		 * Delete all of the values and insert
		 * the new set
		 */
		FcConfigPatternDel (p, e->field);
		FcConfigPatternAdd (p, e->field, l, FcTrue);
		/*
		 * Adjust any pointers into the value list as they no
		 * longer point to anything valid
		 */
		if (t)
		{
		    FcPatternElt    *thisElt = st[i].elt;
		    for (t = s->test, i = 0; t; t = t->next, i++)
		    {
			if (st[i].elt == thisElt)
			    st[i].value = 0;
		    }
		}
		break;
	    case FcOpPrepend:
		if (t)
		{
		    FcConfigAdd (&st[i].elt->values, st[i].value, FcFalse, l);
		    break;
		}
		/* fall through ... */
	    case FcOpPrependFirst:
		FcConfigPatternAdd (p, e->field, l, FcFalse);
		break;
	    case FcOpAppend:
		if (t)
		{
		    FcConfigAdd (&st[i].elt->values, st[i].value, FcTrue, l);
		    break;
		}
		/* fall through ... */
	    case FcOpAppendLast:
		FcConfigPatternAdd (p, e->field, l, FcTrue);
		break;
	    default:
		break;
	    }
	}
	/*
	 * Now go through the pattern and eliminate
	 * any properties without data
	 */
	for (e = s->edit; e; e = e->next)
	    FcConfigPatternCanon (p, e->field);

	if (FcDebug () & FC_DBG_EDIT)
	{
	    printf ("FcConfigSubstitute edit");
	    FcPatternPrint (p);
	}
    }
    FcMemFree (FC_MEM_SUBSTATE, config->maxObjects * sizeof (FcSubState));
    free (st);
    if (FcDebug () & FC_DBG_EDIT)
    {
	printf ("FcConfigSubstitute done");
	FcPatternPrint (p);
    }
    return FcTrue;
}

#ifndef FONTCONFIG_PATH
#define FONTCONFIG_PATH	"/etc/fonts"
#endif

#ifndef FONTCONFIG_FILE
#define FONTCONFIG_FILE	"fonts.conf"
#endif

static FcChar8 *
FcConfigFileExists (const FcChar8 *dir, const FcChar8 *file)
{
    FcChar8    *path;

    if (!dir)
	dir = (FcChar8 *) "";
    path = malloc (strlen ((char *) dir) + 1 + strlen ((char *) file) + 1);
    if (!path)
	return 0;

    strcpy (path, dir);
    /* make sure there's a single separating / */
    if ((!path[0] || path[strlen((char *) path)-1] != '/') && file[0] != '/')
	strcat ((char *) path, "/");
    strcat ((char *) path, (char *) file);

    if (access ((char *) path, R_OK) == 0)
	return path;
    
    free (path);
    return 0;
}

static FcChar8 **
FcConfigGetPath (void)
{
    FcChar8    **path;
    FcChar8    *env, *e, *colon;
    FcChar8    *dir;
    int	    npath;
    int	    i;

    npath = 2;	/* default dir + null */
    env = (FcChar8 *) getenv ("FONTCONFIG_PATH");
    if (env)
    {
	e = env;
	npath++;
	while (*e)
	    if (*e++ == ':')
		npath++;
    }
    path = calloc (npath, sizeof (FcChar8 *));
    if (!path)
	goto bail0;
    i = 0;

    if (env)
    {
	e = env;
	while (*e) 
	{
	    colon = (FcChar8 *) strchr ((char *) e, ':');
	    if (!colon)
		colon = e + strlen ((char *) e);
	    path[i] = malloc (colon - e + 1);
	    if (!path[i])
		goto bail1;
	    strncpy (path[i], e, colon - e);
	    path[i][colon - e] = '\0';
	    if (*colon)
		e = colon + 1;
	    else
		e = colon;
	    i++;
	}
    }
    
    dir = (FcChar8 *) FONTCONFIG_PATH;
    path[i] = malloc (strlen ((char *) dir) + 1);
    if (!path[i])
	goto bail1;
    strcpy (path[i], dir);
    return path;

bail1:
    for (i = 0; path[i]; i++)
	free (path[i]);
    free (path);
bail0:
    return 0;
}

static void
FcConfigFreePath (FcChar8 **path)
{
    FcChar8    **p;

    for (p = path; *p; p++)
	free (*p);
    free (path);
}

FcChar8 *
FcConfigFilename (const FcChar8 *url)
{
    FcChar8    *file, *dir, **path, **p;
    
    if (!url || !*url)
    {
	url = (FcChar8 *) getenv ("FONTCONFIG_FILE");
	if (!url)
	    url = (FcChar8 *) FONTCONFIG_FILE;
    }
    file = 0;
    switch (*url) {
    case '~':
	dir = (FcChar8 *) getenv ("HOME");
	if (dir)
	    file = FcConfigFileExists (dir, url + 1);
	else
	    file = 0;
	break;
    case '/':
	file = FcConfigFileExists (0, url);
	break;
    default:
	path = FcConfigGetPath ();
	if (!path)
	    return 0;
	for (p = path; *p; p++)
	{
	    file = FcConfigFileExists (*p, url);
	    if (file)
		break;
	}
	FcConfigFreePath (path);
	break;
    }
    return file;
}

/*
 * Manage the application-specific fonts
 */

FcBool
FcConfigAppFontAddFile (FcConfig    *config,
			const FcChar8  *file)
{
    FcFontSet	*set;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }

    set = FcConfigGetFonts (config, FcSetApplication);
    if (!set)
    {
	set = FcFontSetCreate ();
	if (!set)
	    return FcFalse;
	FcConfigSetFonts (config, set, FcSetApplication);
    }
    return FcFileScan (set, 0, config->blanks, file, FcFalse);
}

FcBool
FcConfigAppFontAddDir (FcConfig	    *config,
		       const FcChar8   *dir)
{
    FcFontSet	*set;
    
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return FcFalse;
    }
    set = FcConfigGetFonts (config, FcSetApplication);
    if (!set)
    {
	set = FcFontSetCreate ();
	if (!set)
	    return FcFalse;
	FcConfigSetFonts (config, set, FcSetApplication);
    }
    return FcDirScan (set, 0, config->blanks, dir, FcFalse);
}

void
FcConfigAppFontClear (FcConfig	    *config)
{
    FcConfigSetFonts (config, 0, FcSetApplication);
}
