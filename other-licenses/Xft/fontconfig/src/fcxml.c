/*
 * $XFree86: xc/lib/fontconfig/src/fcxml.c,v 1.5 2002/02/22 18:54:07 keithp Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
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

#include <stdarg.h>
#include <xmlparse.h>
#include "fcint.h"

FcTest *
FcTestCreate (FcQual qual, const FcChar8 *field, FcOp compare, FcExpr *expr)
{
    FcTest	*test = (FcTest *) malloc (sizeof (FcTest));

    if (test)
    {
	test->next = 0;
	test->qual = qual;
	test->field = (char *) FcStrCopy (field);
	test->op = compare;
	test->expr = expr;
    }
    return test;
}

void
FcTestDestroy (FcTest *test)
{
    if (test->next)
	FcTestDestroy (test->next);
    FcExprDestroy (test->expr);
    FcStrFree ((FcChar8 *) test->field);
    free (test);
}

FcExpr *
FcExprCreateInteger (int i)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpInteger;
	e->u.ival = i;
    }
    return e;
}

FcExpr *
FcExprCreateDouble (double d)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpDouble;
	e->u.dval = d;
    }
    return e;
}

FcExpr *
FcExprCreateString (const FcChar8 *s)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpString;
	e->u.sval = FcStrCopy (s);
    }
    return e;
}

FcExpr *
FcExprCreateMatrix (const FcMatrix *m)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpMatrix;
	e->u.mval = FcMatrixCopy (m);
    }
    return e;
}

FcExpr *
FcExprCreateBool (FcBool b)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpBool;
	e->u.bval = b;
    }
    return e;
}

FcExpr *
FcExprCreateNil (void)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpNil;
    }
    return e;
}

FcExpr *
FcExprCreateField (const char *field)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpField;
	e->u.field = (char *) FcStrCopy ((FcChar8 *) field);
    }
    return e;
}

FcExpr *
FcExprCreateConst (const FcChar8 *constant)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = FcOpConst;
	e->u.constant = FcStrCopy (constant);
    }
    return e;
}

FcExpr *
FcExprCreateOp (FcExpr *left, FcOp op, FcExpr *right)
{
    FcExpr *e = (FcExpr *) malloc (sizeof (FcExpr));

    if (e)
    {
	e->op = op;
	e->u.tree.left = left;
	e->u.tree.right = right;
    }
    return e;
}

void
FcExprDestroy (FcExpr *e)
{
    switch (e->op) {
    case FcOpInteger:
	break;
    case FcOpDouble:
	break;
    case FcOpString:
	FcStrFree (e->u.sval);
	break;
    case FcOpMatrix:
	FcMatrixFree (e->u.mval);
	break;
    case FcOpCharSet:
	FcCharSetDestroy (e->u.cval);
	break;
    case FcOpBool:
	break;
    case FcOpField:
	FcStrFree ((FcChar8 *) e->u.field);
	break;
    case FcOpConst:
	FcStrFree (e->u.constant);
	break;
    case FcOpAssign:
    case FcOpAssignReplace:
    case FcOpPrepend:
    case FcOpPrependFirst:
    case FcOpAppend:
    case FcOpAppendLast:
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
    case FcOpQuest:
    case FcOpComma:
	FcExprDestroy (e->u.tree.right);
	/* fall through */
    case FcOpNot:
	FcExprDestroy (e->u.tree.left);
	break;
    case FcOpNil:
    case FcOpInvalid:
	break;
    }
    free (e);
}

FcEdit *
FcEditCreate (const char *field, FcOp op, FcExpr *expr)
{
    FcEdit *e = (FcEdit *) malloc (sizeof (FcEdit));

    if (e)
    {
	e->next = 0;
	e->field = field;   /* already saved in grammar */
	e->op = op;
	e->expr = expr;
    }
    return e;
}

void
FcEditDestroy (FcEdit *e)
{
    if (e->next)
	FcEditDestroy (e->next);
    FcStrFree ((FcChar8 *) e->field);
    if (e->expr)
	FcExprDestroy (e->expr);
}

char *
FcConfigSaveField (const char *field)
{
    return (char *) FcStrCopy ((FcChar8 *) field);
}

typedef enum _FcElement {
    FcElementNone,
    FcElementFontconfig,
    FcElementDir,
    FcElementCache,
    FcElementInclude,
    FcElementConfig,
    FcElementMatch,
    FcElementAlias,
	
    FcElementBlank,

    FcElementPrefer,
    FcElementAccept,
    FcElementDefault,
    FcElementFamily,

    FcElementTest,
    FcElementEdit,
    FcElementInt,
    FcElementDouble,
    FcElementString,
    FcElementMatrix,
    FcElementBool,
    FcElementCharset,
    FcElementName,
    FcElementConst,
    FcElementOr,
    FcElementAnd,
    FcElementEq,
    FcElementNotEq,
    FcElementLess,
    FcElementLessEq,
    FcElementMore,
    FcElementMoreEq,
    FcElementPlus,
    FcElementMinus,
    FcElementTimes,
    FcElementDivide,
    FcElementNot,
    FcElementIf,
    FcElementUnknown
} FcElement;

static FcElement
FcElementMap (const XML_Char *name)
{
    static struct {
	char	    *name;
	FcElement   element;
    } fcElementMap[] = {
	{ "fontconfig",	FcElementFontconfig },
	{ "dir",	FcElementDir },
	{ "cache",	FcElementCache },
	{ "include",	FcElementInclude },
	{ "config",	FcElementConfig },
	{ "match",	FcElementMatch },
	{ "alias",	FcElementAlias },
	
	{ "blank",	FcElementBlank },

	{ "prefer",	FcElementPrefer },
	{ "accept",	FcElementAccept },
	{ "default",	FcElementDefault },
	{ "family",	FcElementFamily },

	{ "test",	FcElementTest },
	{ "edit",	FcElementEdit },
	{ "int",	FcElementInt },
	{ "double",	FcElementDouble },
	{ "string",	FcElementString },
	{ "matrix",	FcElementMatrix },
	{ "bool",	FcElementBool },
	{ "charset",	FcElementCharset },
	{ "name",	FcElementName },
	{ "const",	FcElementConst },
	{ "or",		FcElementOr },
	{ "and",	FcElementAnd },
	{ "eq",		FcElementEq },
	{ "not_eq",	FcElementNotEq },
	{ "less",	FcElementLess },
	{ "less_eq",	FcElementLessEq },
	{ "more",	FcElementMore },
	{ "more_eq",	FcElementMoreEq },
	{ "plus",	FcElementPlus },
	{ "minus",	FcElementMinus },
	{ "times",	FcElementTimes },
	{ "divide",	FcElementDivide },
	{ "not",	FcElementNot },
	{ "if",		FcElementIf },
	
	{ 0,		0 }
    };

    int	    i;
    for (i = 0; fcElementMap[i].name; i++)
	if (!strcmp ((char *) name, fcElementMap[i].name))
	    return fcElementMap[i].element;
    return FcElementUnknown;
}

typedef struct _FcPStack {
    struct _FcPStack   *prev;
    FcElement		element;
    FcChar8		**attr;
    FcStrBuf		str;
} FcPStack;
    
typedef enum _FcVStackTag {
    FcVStackNone,

    FcVStackString,
    FcVStackFamily,
    FcVStackField,
    FcVStackConstant,
    
    FcVStackPrefer,
    FcVStackAccept,
    FcVStackDefault,
    
    FcVStackInteger,
    FcVStackDouble,
    FcVStackMatrix,
    FcVStackBool,
    
    FcVStackTest,
    FcVStackExpr,
    FcVStackEdit
} FcVStackTag;

typedef struct _FcVStack {
    struct _FcVStack	*prev;
    FcPStack		*pstack;	/* related parse element */
    FcVStackTag		tag;
    union {
	FcChar8		*string;

	int		integer;
	double		_double;
	FcMatrix	*matrix;
	FcBool		bool;

	FcTest		*test;
	FcQual		qual;
	FcOp		op;
	FcExpr		*expr;
	FcEdit		*edit;
    } u;
} FcVStack;

typedef struct _FcConfigParse {
    FcPStack	    *pstack;
    FcVStack	    *vstack;
    FcBool	    error;
    const FcChar8   *name;
    FcConfig	    *config;
    XML_Parser	    parser;
} FcConfigParse;

static void
FcConfigError (FcConfigParse *parse, char *fmt, ...)
{
    va_list	args;

    va_start (args, fmt);
    if (parse)
    {
	if (parse->name)
	    fprintf (stderr, "Fontconfig error: \"%s\", line %d: ",
		     parse->name, XML_GetCurrentLineNumber (parse->parser));
	else
	    fprintf (stderr, "Fontconfig error: line %d: ",
		     XML_GetCurrentLineNumber (parse->parser));
	parse->error = FcTrue;
    }
    else
	fprintf (stderr, "Fontconfig error: ");
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
    va_end (args);
}

static void
FcVStackPush (FcConfigParse *parse, FcVStack *vstack)
{
    vstack->prev = parse->vstack;
    vstack->pstack = parse->pstack ? parse->pstack->prev : 0;
    parse->vstack = vstack;
}

static FcVStack *
FcVStackCreate (void)
{
    FcVStack    *new;

    new = malloc (sizeof (FcVStack));
    if (!new)
	return 0;
    new->tag = FcVStackNone;
    new->prev = 0;
    return new;
}

static void
FcVStackDestroy (FcVStack *vstack)
{
    FcVStack    *prev;

    for (; vstack; vstack = prev)
    {
	prev = vstack->prev;
	switch (vstack->tag) {
	case FcVStackNone:
	    break;
	case FcVStackString:
	case FcVStackFamily:
	case FcVStackField:
	case FcVStackConstant:
	    FcStrFree (vstack->u.string);
	    break;
	case FcVStackInteger:
	case FcVStackDouble:
	    break;
	case FcVStackMatrix:
	    FcMatrixFree (vstack->u.matrix);
	    break;
	case FcVStackBool:
	    break;
	case FcVStackTest:
	    FcTestDestroy (vstack->u.test);
	    break;
	case FcVStackExpr:
	case FcVStackPrefer:
	case FcVStackAccept:
	case FcVStackDefault:
	    FcExprDestroy (vstack->u.expr);
	    break;
	case FcVStackEdit:
	    FcEditDestroy (vstack->u.edit);
	    break;
	}
	free (vstack);
    }
}

static FcBool
FcVStackPushString (FcConfigParse *parse, FcVStackTag tag, FcChar8 *string)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    vstack->u.string = string;
    vstack->tag = tag;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcBool
FcVStackPushInteger (FcConfigParse *parse, int integer)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    vstack->u.integer = integer;
    vstack->tag = FcVStackInteger;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcBool
FcVStackPushDouble (FcConfigParse *parse, double _double)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    vstack->u._double = _double;
    vstack->tag = FcVStackDouble;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcBool
FcVStackPushMatrix (FcConfigParse *parse, FcMatrix *matrix)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    matrix = FcMatrixCopy (matrix);
    if (!matrix)
    {
	FcVStackDestroy (vstack);
	return FcFalse;
    }
    vstack->u.matrix = matrix;
    vstack->tag = FcVStackMatrix;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcBool
FcVStackPushBool (FcConfigParse *parse, FcBool bool)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    vstack->u.bool = bool;
    vstack->tag = FcVStackBool;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcBool
FcVStackPushTest (FcConfigParse *parse, FcTest *test)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    vstack->u.test = test;
    vstack->tag = FcVStackTest;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcBool
FcVStackPushExpr (FcConfigParse *parse, FcVStackTag tag, FcExpr *expr)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    vstack->u.expr = expr;
    vstack->tag = tag;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcBool
FcVStackPushEdit (FcConfigParse *parse, FcEdit *edit)
{
    FcVStack    *vstack = FcVStackCreate ();
    if (!vstack)
	return FcFalse;
    vstack->u.edit = edit;
    vstack->tag = FcVStackEdit;
    FcVStackPush (parse, vstack);
    return FcTrue;
}

static FcVStack *
FcVStackFetch (FcConfigParse *parse, int off)
{
    FcVStack    *vstack;

    for (vstack = parse->vstack; vstack && off-- > 0; vstack = vstack->prev);
    return vstack;
}

static void
FcVStackClear (FcConfigParse *parse)
{
    while (parse->vstack && parse->vstack->pstack == parse->pstack)
    {
	FcVStack    *vstack = parse->vstack;
	parse->vstack = vstack->prev;
	vstack->prev = 0;
	FcVStackDestroy (vstack);
    }
}

static FcVStack *
FcVStackPop (FcConfigParse *parse)
{
    FcVStack	*vstack = parse->vstack;
    
    if (!vstack || vstack->pstack != parse->pstack)
	return 0;
    parse->vstack = vstack->prev;
    vstack->prev = 0;
    return vstack;
}

static int
FcVStackElements (FcConfigParse *parse)
{
    int		h = 0;
    FcVStack	*vstack = parse->vstack;
    while (vstack && vstack->pstack == parse->pstack)
    {
	h++;
	vstack = vstack->prev;
    }
    return h;
}

static FcChar8 **
FcConfigSaveAttr (const XML_Char **attr)
{
    int		n;
    int		slen;
    int		i;
    FcChar8	**new;
    FcChar8	*s;

    if (!attr)
	return 0;
    slen = 0;
    for (i = 0; attr[i]; i++)
	slen += strlen (attr[i]) + 1;
    n = i;
    new = malloc ((i + 1) * sizeof (FcChar8 *) + slen);
    if (!new)
	return 0;
    s = (FcChar8 *) (new + (i + 1));
    for (i = 0; attr[i]; i++)
    {
	new[i] = s;
	strcpy ((char *) s, (char *) attr[i]);
	s += strlen ((char *) s) + 1;
    }
    new[i] = 0;
    return new;
}

static FcBool
FcPStackPush (FcConfigParse *parse, FcElement element, const XML_Char **attr)
{
    FcPStack   *new = malloc (sizeof (FcPStack));

    if (!new)
	return FcFalse;
    new->prev = parse->pstack;
    new->element = element;
    if (attr)
    {
	new->attr = FcConfigSaveAttr (attr);
	if (!new->attr)
	    FcConfigError (parse, "out of memory");
    }
    else
	new->attr = 0;
    FcStrBufInit (&new->str, 0, 0);
    parse->pstack = new;
    return FcTrue;
}

static FcBool
FcPStackPop (FcConfigParse *parse)
{
    FcPStack   *old;
    
    if (!parse->pstack) 
    {
	FcConfigError (parse, "mismatching element");
	return FcFalse;
    }
    FcVStackClear (parse);
    old = parse->pstack;
    parse->pstack = old->prev;
    FcStrBufDestroy (&old->str);
    if (old->attr)
	free (old->attr);
    free (old);
    return FcTrue;
}

static FcBool
FcConfigInit (FcConfigParse *parse, const FcChar8 *name, FcConfig *config, XML_Parser parser)
{
    parse->pstack = 0;
    parse->vstack = 0;
    parse->error = FcFalse;
    parse->name = name;
    parse->config = config;
    parse->parser = parser;
    return FcTrue;
}

static void
FcConfigCleanup (FcConfigParse	*parse)
{
    while (parse->pstack)
	FcPStackPop (parse);
}

static const FcChar8 *
FcConfigGetAttribute (FcConfigParse *parse, char *attr)
{
    FcChar8 **attrs;
    if (!parse->pstack)
	return 0;

    attrs = parse->pstack->attr;
    while (*attrs)
    {
	if (!strcmp ((char *) *attrs, attr))
	    return attrs[1];
	attrs += 2;
    }
    return 0;
}

static void
FcStartElement(void *userData, const XML_Char *name, const XML_Char **attr)
{
    FcConfigParse   *parse = userData;
    FcElement	    element;
    
    element = FcElementMap (name);
    if (element == FcElementUnknown)
    {
	FcConfigError (parse, "unknown element \"%s\"", name);
	return;
    }
    
    if (!FcPStackPush (parse, element, attr))
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    return;
}

static void
FcParseBlank (FcConfigParse *parse)
{
    int	    n = FcVStackElements (parse);
    while (n-- > 0)
    {
	FcVStack    *v = FcVStackFetch (parse, n);
	if (v->tag != FcVStackInteger)
	    FcConfigError (parse, "non-integer blank");
	else
	{
	    if (!parse->config->blanks)
	    {
		parse->config->blanks = FcBlanksCreate ();
		if (!parse->config->blanks)
		{
		    FcConfigError (parse, "out of memory");
		    break;
		}
	    }
	    if (!FcBlanksAdd (parse->config->blanks, v->u.integer))
	    {
		FcConfigError (parse, "out of memory");
		break;
	    }
	}
    }
}

static void
FcParseInt (FcConfigParse *parse)
{
    FcChar8 *s, *end;
    int	    l;
    
    if (!parse->pstack)
	return;
    s = FcStrBufDone (&parse->pstack->str);
    if (!s)
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    end = 0;
    l = (int) strtol ((char *) s, (char **)&end, 0);
    if (end != s + strlen ((char *) s))
	FcConfigError (parse, "\"%s\": not a valid integer", s);
    else
	FcVStackPushInteger (parse, l);
    FcStrFree (s);
}

static void
FcParseDouble (FcConfigParse *parse)
{
    FcChar8 *s, *end;
    double  d;
    
    if (!parse->pstack)
	return;
    s = FcStrBufDone (&parse->pstack->str);
    if (!s)
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    end = 0;
    d = strtod ((char *) s, (char **)&end);
    if (end != s + strlen ((char *) s))
	FcConfigError (parse, "\"%s\": not a valid double", s);
    else
	FcVStackPushDouble (parse, d);
    FcStrFree (s);
}

static void
FcParseString (FcConfigParse *parse, FcVStackTag tag)
{
    FcChar8 *s;
    
    if (!parse->pstack)
	return;
    s = FcStrBufDone (&parse->pstack->str);
    if (!s)
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    if (!FcVStackPushString (parse, tag, s))
	FcStrFree (s);
}

static void
FcParseMatrix (FcConfigParse *parse)
{
    FcVStack	*vstack;
    enum { m_done, m_xx, m_xy, m_yx, m_yy } matrix_state = m_yy;
    FcMatrix	m;
    
    while ((vstack = FcVStackPop (parse)))
    {
	if (vstack->tag != FcVStackDouble)
	    FcConfigError (parse, "non-double matrix element");
	else
	{
	    double v = vstack->u._double;
	    switch (matrix_state) {
	    case m_xx: m.xx = v; break;
	    case m_xy: m.xy = v; break;
	    case m_yx: m.yx = v; break;
	    case m_yy: m.yy = v; break;
	    default: break;
	    }
	    matrix_state--;
	}
    }
    if (matrix_state != m_done)
	FcConfigError (parse, "wrong number of matrix elements");
    else
	FcVStackPushMatrix (parse, &m);
}

static FcBool
FcConfigLexBool (const FcChar8 *bool)
{
    if (*bool == 't' || *bool == 'T')
	return FcTrue;
    if (*bool == 'y' || *bool == 'Y')
	return FcTrue;
    if (*bool == '1')
	return FcTrue;
    return FcFalse;
}

static void
FcParseBool (FcConfigParse *parse)
{
    FcChar8 *s;

    if (!parse->pstack)
	return;
    s = FcStrBufDone (&parse->pstack->str);
    if (!s)
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    FcVStackPushBool (parse, FcConfigLexBool (s));
    FcStrFree (s);
}

static void
FcParseFamilies (FcConfigParse *parse, FcVStackTag tag)
{
    FcVStack	*vstack;
    FcExpr	*left, *expr = 0, *new;

    while ((vstack = FcVStackPop (parse)))
    {
	if (vstack->tag != FcVStackFamily)
	{
	    FcConfigError (parse, "non-family");
	    break;
	}
	left = vstack->u.expr;
	vstack->tag = FcVStackNone;
	FcVStackDestroy (vstack);
	if (expr)
	{
	    new = FcExprCreateOp (left, FcOpComma, expr);
	    if (!new)
	    {
		FcConfigError (parse, "out of memory");
		FcExprDestroy (left);
		FcExprDestroy (expr);
		break;
	    }
	    expr = new;
	}
	else
	    expr = left;
    }
    if (expr)
    {
	if (!FcVStackPushExpr (parse, tag, expr))
	{
	    FcConfigError (parse, "out of memory");
	    if (expr)
		FcExprDestroy (expr);
	}
    }
}

static void
FcParseFamily (FcConfigParse *parse)
{
    FcChar8 *s;
    FcExpr  *expr;

    if (!parse->pstack)
	return;
    s = FcStrBufDone (&parse->pstack->str);
    if (!s)
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    expr = FcExprCreateString (s);
    FcStrFree (s);
    if (expr)
	FcVStackPushExpr (parse, FcVStackFamily, expr);
}

static void
FcParseAlias (FcConfigParse *parse)
{
    FcExpr	*family = 0, *accept = 0, *prefer = 0, *def = 0;
    FcEdit	*edit = 0, *next;
    FcVStack	*vstack;
    FcTest	*test;

    while ((vstack = FcVStackPop (parse))) 
    {
	switch (vstack->tag) {
	case FcVStackFamily:
	    if (family)
		FcExprDestroy (family);
	    family = vstack->u.expr;
	    vstack->tag = FcVStackNone;
	    break;
	case FcVStackPrefer:
	    if (prefer)
		FcExprDestroy (prefer);
	    prefer = vstack->u.expr;
	    vstack->tag = FcVStackNone;
	    break;
	case FcVStackAccept:
	    if (accept)
		FcExprDestroy (accept);
	    accept = vstack->u.expr;
	    vstack->tag = FcVStackNone;
	    break;
	case FcVStackDefault:
	    if (def)
		FcExprDestroy (def);
	    def = vstack->u.expr;
	    vstack->tag = FcVStackNone;
	    break;
	default:
	    FcConfigError (parse, "bad alias");
	    break;
	}
	FcVStackDestroy (vstack);
    }
    if (!family)
    {
	FcConfigError (parse, "missing family in alias");
	return;
    }
    if (prefer)
    {
	edit = FcEditCreate (FcConfigSaveField ("family"),
			     FcOpPrepend,
			     prefer);
	if (edit)
	    edit->next = 0;
	else
	    FcExprDestroy (prefer);
    }
    if (accept)
    {
	next = edit;
	edit = FcEditCreate (FcConfigSaveField ("family"),
			     FcOpAppend,
			     accept);
	if (edit)
	    edit->next = next;
	else
	    FcExprDestroy (accept);
    }
    if (def)
    {
	next = edit;
	edit = FcEditCreate (FcConfigSaveField ("family"),
			      FcOpAppendLast,
			      def);
	if (edit)
	    edit->next = next;
	else
	    FcExprDestroy (def);
    }
    if (edit)
    {
	test = FcTestCreate (FcQualAny,
			     FcStrCopy ((FcChar8 *) "family"),
			     FcOpEqual,
			     family);
	if (test)
	    if (!FcConfigAddEdit (parse->config, test, edit, FcMatchPattern))
		FcTestDestroy (test);
    }
    else
	FcExprDestroy (family);
}

static FcExpr *
FcPopExpr (FcConfigParse *parse)
{
    FcVStack	*vstack = FcVStackPop (parse);
    FcExpr	*expr = 0;
    if (!vstack)
	return 0;
    switch (vstack->tag) {
    case FcVStackNone:
	break;
    case FcVStackString:
    case FcVStackFamily:
	expr = FcExprCreateString (vstack->u.string);
	break;
    case FcVStackField:
	expr = FcExprCreateField ((char *) vstack->u.string);
	break;
    case FcVStackConstant:
	expr = FcExprCreateConst (vstack->u.string);
	break;
    case FcVStackPrefer:
    case FcVStackAccept:
    case FcVStackDefault:
	expr = vstack->u.expr;
	vstack->tag = FcVStackNone;
	break;
    case FcVStackInteger:
	expr = FcExprCreateInteger (vstack->u.integer);
	break;
    case FcVStackDouble:
	expr = FcExprCreateDouble (vstack->u._double);
	break;
    case FcVStackMatrix:
	expr = FcExprCreateMatrix (vstack->u.matrix);
	break;
    case FcVStackBool:
	expr = FcExprCreateBool (vstack->u.bool);
	break;
    case FcVStackTest:
	break;
    case FcVStackExpr:
	expr = vstack->u.expr;
	break;
    case FcVStackEdit:
	break;
    }
    FcVStackDestroy (vstack);
    return expr;
}

static FcExpr *
FcPopExprs (FcConfigParse *parse, FcOp op)
{
    FcExpr  *left, *expr = 0, *new;

    while ((left = FcPopExpr (parse)))
    {
	if (expr)
	{
	    new = FcExprCreateOp (left, op, expr);
	    if (!new)
	    {
		FcConfigError (parse, "out of memory");
		FcExprDestroy (left);
		FcExprDestroy (expr);
		break;
	    }
	    expr = new;
	}
	else
	    expr = left;
    }
    return expr;
}

static void
FcParseExpr (FcConfigParse *parse, FcOp op)
{
    FcExpr  *expr = FcPopExprs (parse, op);
    if (expr)
	FcVStackPushExpr (parse, FcVStackExpr, expr);
}

static void
FcParseInclude (FcConfigParse *parse)
{
    FcChar8	    *s;
    const FcChar8   *i;
    FcBool	    ignore_missing = FcFalse;
    
    s = FcStrBufDone (&parse->pstack->str);
    if (!s)
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    i = FcConfigGetAttribute (parse, "ignore_missing");
    if (i && FcConfigLexBool ((FcChar8 *) i) == FcTrue)
	ignore_missing = FcTrue;
    if (!FcConfigParseAndLoad (parse->config, s, !ignore_missing))
	parse->error = FcTrue;
    free (s);
}

typedef struct _FcOpMap {
    char    *name;
    FcOp    op;
} FcOpMap;

static FcOp
FcConfigLexOp (const FcChar8 *op, const FcOpMap	*map, int nmap)
{
    int	i;

    for (i = 0; i < nmap; i++)
	if (!strcmp ((char *) op, map[i].name)) 
	    return map[i].op;
    return FcOpInvalid;
}

static const FcOpMap fcCompareOps[] = {
    { "eq",		FcOpEqual	    },
    { "not_eq",		FcOpNotEqual	    },
    { "less",		FcOpLess	    },
    { "less_eq",	FcOpLessEqual	    },
    { "more",		FcOpMore	    },
    { "more_eq",	FcOpMoreEqual	    }
};

#define NUM_COMPARE_OPS (sizeof fcCompareOps / sizeof fcCompareOps[0])

static FcOp
FcConfigLexCompare (const FcChar8 *compare)
{
    return FcConfigLexOp (compare, fcCompareOps, NUM_COMPARE_OPS);
}


static void
FcParseTest (FcConfigParse *parse)
{
    const FcChar8   *qual_string;
    FcQual	    qual;
    const FcChar8   *name;
    const FcChar8   *compare_string;
    FcOp	    compare;
    FcExpr	    *expr;
    FcTest	    *test;

    qual_string = FcConfigGetAttribute (parse, "qual");
    if (!qual_string)
	qual = FcQualAny;
    else
    {
	if (!strcmp ((char *) qual_string, "any"))
	    qual = FcQualAny;
	else if (!strcmp ((char *) qual_string, "all"))
	    qual = FcQualAll;
	else
	{
	    FcConfigError (parse, "invalid test qual \"%s\"", qual_string);
	    return;
	}
    }
    name = FcConfigGetAttribute (parse, "name");
    if (!name)
    {
	FcConfigError (parse, "missing test name");
	return;
    }
    compare_string = FcConfigGetAttribute (parse, "compare");
    if (!compare_string)
	compare = FcOpEqual;
    else
    {
	compare = FcConfigLexCompare (compare_string);
	if (compare == FcOpInvalid)
	{
	    FcConfigError (parse, "invalid test compare \"%s\"", compare_string);
	    return;
	}
    }
    expr = FcPopExpr (parse);
    if (!expr)
    {
	FcConfigError (parse, "missing test expression");
	return;
    }
    test = FcTestCreate (qual, name, compare, expr);
    if (!test)
    {
	FcConfigError (parse, "out of memory");
	return;
    }
    FcVStackPushTest (parse, test);
}

static const FcOpMap fcModeOps[] = {
    { "assign",		FcOpAssign	    },
    { "assign_replace",	FcOpAssignReplace   },
    { "prepend",	FcOpPrepend	    },
    { "prepend_first",	FcOpPrependFirst    },
    { "append",		FcOpAppend	    },
    { "append_last",	FcOpAppendLast	    },
};

#define NUM_MODE_OPS (sizeof fcModeOps / sizeof fcModeOps[0])

static FcOp
FcConfigLexMode (const FcChar8 *mode)
{
    return FcConfigLexOp (mode, fcModeOps, NUM_MODE_OPS);
}

static void
FcParseEdit (FcConfigParse *parse)
{
    const FcChar8   *name;
    const FcChar8   *mode_string;
    FcOp	    mode;
    FcExpr	    *expr;
    FcEdit	    *edit;

    name = FcConfigGetAttribute (parse, "name");
    if (!name)
    {
	FcConfigError (parse, "missing edit name");
	return;
    }
    mode_string = FcConfigGetAttribute (parse, "mode");
    if (!mode_string)
	mode = FcOpEqual;
    else
    {
	mode = FcConfigLexMode (mode_string);
	if (mode == FcOpInvalid)
	{
	    FcConfigError (parse, "invalid edit mode \"%s\"", mode_string);
	    return;
	}
    }
    expr = FcPopExprs (parse, FcOpComma);
    edit = FcEditCreate ((char *) FcStrCopy (name), mode, expr);
    if (!edit)
    {
	FcConfigError (parse, "out of memory");
	FcExprDestroy (expr);
	return;
    }
    if (!FcVStackPushEdit (parse, edit))
	FcEditDestroy (edit);
}

static void
FcParseMatch (FcConfigParse *parse)
{
    const FcChar8   *kind_name;
    FcMatchKind	    kind;
    FcTest	    *test = 0;
    FcEdit	    *edit = 0;
    FcVStack	    *vstack;

    kind_name = FcConfigGetAttribute (parse, "target");
    if (!kind_name)
	kind = FcMatchPattern;
    else
    {
	if (!strcmp ((char *) kind_name, "pattern"))
	    kind = FcMatchPattern;
	else if (!strcmp ((char *) kind_name, "font"))
	    kind = FcMatchFont;
	else
	{
	    FcConfigError (parse, "invalid match target \"%s\"", kind_name);
	    return;
	}
    }
    while ((vstack = FcVStackPop (parse)))
    {
	switch (vstack->tag) {
	case FcVStackTest:
	    vstack->u.test->next = test;
	    test = vstack->u.test;
	    vstack->tag = FcVStackNone;
	    break;
	case FcVStackEdit:
	    vstack->u.edit->next = edit;
	    edit = vstack->u.edit;
	    vstack->tag = FcVStackNone;
	    break;
	default:
	    FcConfigError (parse, "invalid match element");
	    break;
	}
	FcVStackDestroy (vstack);
    }
    if (!FcConfigAddEdit (parse->config, test, edit, kind))
	FcConfigError (parse, "out of memory");
}

static void
FcEndElement(void *userData, const XML_Char *name)
{
    FcConfigParse   *parse = userData;
    FcChar8	    *data;
    
    if (!parse->pstack)
	return;
    switch (parse->pstack->element) {
    case FcElementNone:
	break;
    case FcElementFontconfig:
	break;
    case FcElementDir:
	data = FcStrBufDone (&parse->pstack->str);
	if (!data)
	{
	    FcConfigError (parse, "out of memory");
	    break;
	}
	if (!FcConfigAddDir (parse->config, data))
	    FcConfigError (parse, "out of memory");
	free (data);
	break;
    case FcElementCache:
	data = FcStrBufDone (&parse->pstack->str);
	if (!data)
	{
	    FcConfigError (parse, "out of memory");
	    break;
	}
	if (!FcConfigSetCache (parse->config, data))
	    FcConfigError (parse, "out of memory");
	free (data);
	break;
    case FcElementInclude:
	FcParseInclude (parse);
	break;
    case FcElementConfig:
	break;
    case FcElementMatch:
	FcParseMatch (parse);
	break;
    case FcElementAlias:
	FcParseAlias (parse);
	break;

    case FcElementBlank:
	FcParseBlank (parse);
	break;
	
    case FcElementPrefer:
	FcParseFamilies (parse, FcVStackPrefer);
	break;
    case FcElementAccept:
	FcParseFamilies (parse, FcVStackAccept);
	break;
    case FcElementDefault:
	FcParseFamilies (parse, FcVStackDefault);
	break;
    case FcElementFamily:
	FcParseFamily (parse);
	break;

    case FcElementTest:
	FcParseTest (parse);
	break;
    case FcElementEdit:
	FcParseEdit (parse);
	break;

    case FcElementInt:
	FcParseInt (parse);
	break;
    case FcElementDouble:
	FcParseDouble (parse);
	break;
    case FcElementString:
	FcParseString (parse, FcVStackString);
	break;
    case FcElementMatrix:
	FcParseMatrix (parse);
	break;
    case FcElementBool:
	FcParseBool (parse);
	break;
    case FcElementCharset:
/*	FcParseCharset (parse); */
	break;

    case FcElementName:
	FcParseString (parse, FcVStackField);
	break;
    case FcElementConst:
	FcParseString (parse, FcVStackConstant);
	break;
    case FcElementOr:
	FcParseExpr (parse, FcOpOr);
	break;
    case FcElementAnd:
	FcParseExpr (parse, FcOpAnd);
	break;
    case FcElementEq:
	FcParseExpr (parse, FcOpEqual);
	break;
    case FcElementNotEq:
	FcParseExpr (parse, FcOpNotEqual);
	break;
    case FcElementLess:
	FcParseExpr (parse, FcOpLess);
	break;
    case FcElementLessEq:
	FcParseExpr (parse, FcOpLessEqual);
	break;
    case FcElementMore:
	FcParseExpr (parse, FcOpMore);
	break;
    case FcElementMoreEq:
	FcParseExpr (parse, FcOpMoreEqual);
	break;
    case FcElementPlus:
	FcParseExpr (parse, FcOpPlus);
	break;
    case FcElementMinus:
	FcParseExpr (parse, FcOpMinus);
	break;
    case FcElementTimes:
	FcParseExpr (parse, FcOpTimes);
	break;
    case FcElementDivide:
	FcParseExpr (parse, FcOpDivide);
	break;
    case FcElementNot:
	FcParseExpr (parse, FcOpNot);
	break;
    case FcElementIf:
	FcParseExpr (parse, FcOpQuest);
	break;
    case FcElementUnknown:
	break;
    }
    (void) FcPStackPop (parse);
}

static void
FcCharacterData (void *userData, const XML_Char *s, int len)
{
    FcConfigParse   *parse = userData;
    
    if (!parse->pstack)
	return;
    if (!FcStrBufData (&parse->pstack->str, (FcChar8 *) s, len))
	FcConfigError (parse, "out of memory");
}

static void
FcStartDoctypeDecl (void	    *userData,
		    const XML_Char  *doctypeName,
		    const XML_Char  *sysid,
		    const XML_Char  *pubid,
		    int		    has_internal_subset)
{
    FcConfigParse   *parse = userData;

    if (strcmp ((char *) doctypeName, "fontconfig") != 0)
	FcConfigError (parse, "invalid doctype \"%s\"", doctypeName);
}

static void
FcEndDoctypeDecl (void *userData)
{
}

FcBool
FcConfigParseAndLoad (FcConfig	    *config,
		      const FcChar8 *name,
		      FcBool	    complain)
{

    XML_Parser	    p;
    FcChar8	    *filename;
    FILE	    *f;
    int		    len;
    void	    *buf;
    FcConfigParse   parse;
    FcBool	    error = FcTrue;
    
    filename = FcConfigFilename (name);
    if (!filename)
	goto bail0;
    f = fopen ((char *) filename, "r");
    free (filename);
    if (!f)
	goto bail0;
    
    p = XML_ParserCreate ("UTF-8");
    if (!p)
	goto bail1;

    if (!FcConfigInit (&parse, name, config, p))
	goto bail2;

    XML_SetUserData (p, &parse);
    
    XML_SetDoctypeDeclHandler (p, FcStartDoctypeDecl, FcEndDoctypeDecl);
    XML_SetElementHandler (p, FcStartElement, FcEndElement);
    XML_SetCharacterDataHandler (p, FcCharacterData);
	
    do {
	buf = XML_GetBuffer (p, BUFSIZ);
	if (!buf)
	    goto bail3;
	len = fread (buf, 1, BUFSIZ, f);
	if (len < 0)
	    goto bail3;
	if (!XML_ParseBuffer (p, len, len == 0))
	    goto bail3;
    } while (len != 0);
    error = parse.error;
bail3:
    FcConfigCleanup (&parse);
bail2:
    XML_ParserFree (p);
bail1:
    fclose (f);
bail0:
    if (error && complain)
    {
	if (name)
	    FcConfigError (0, "Cannot load config file \"%s\"", name);
	else
	    FcConfigError (0, "Cannot load default config file");
	return FcFalse;
    }
    return FcTrue;
}
