/*
 * $XFree86: xc/lib/fontconfig/src/fcdbg.c,v 1.2 2002/02/18 22:29:28 keithp Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include "fcint.h"

void
FcValuePrint (FcValue v)
{
    switch (v.type) {
    case FcTypeVoid:
	printf (" <void>");
	break;
    case FcTypeInteger:
	printf (" %d", v.u.i);
	break;
    case FcTypeDouble:
	printf (" %g", v.u.d);
	break;
    case FcTypeString:
	printf (" \"%s\"", v.u.s);
	break;
    case FcTypeBool:
	printf (" %s", v.u.b ? "FcTrue" : "FcFalse");
	break;
    case FcTypeMatrix:
	printf (" (%f %f; %f %f)", v.u.m->xx, v.u.m->xy, v.u.m->yx, v.u.m->yy);
	break;
    case FcTypeCharSet:	/* XXX */
	printf (" set");
	break;
    }
}

void
FcValueListPrint (FcValueList *l)
{
    for (; l; l = l->next)
	FcValuePrint (l->value);
}

void
FcPatternPrint (FcPattern *p)
{
    int		    i;
    FcPatternElt   *e;
    
    if (!p)
    {
	printf ("Null pattern\n");
	return;
    }
    printf ("Pattern %d of %d\n", p->num, p->size);
    for (i = 0; i < p->num; i++)
    {
	e = &p->elts[i];
	printf ("\t%s:", e->object);
	FcValueListPrint (e->values);
	printf ("\n");
    }
    printf ("\n");
}

void
FcOpPrint (FcOp op)
{
    switch (op) {
    case FcOpInteger: printf ("Integer"); break;
    case FcOpDouble: printf ("Double"); break;
    case FcOpString: printf ("String"); break;
    case FcOpMatrix: printf ("Matrix"); break;
    case FcOpBool: printf ("Bool"); break;
    case FcOpCharSet: printf ("CharSet"); break;
    case FcOpField: printf ("Field"); break;
    case FcOpConst: printf ("Const"); break;
    case FcOpAssign: printf ("Assign"); break;
    case FcOpAssignReplace: printf ("AssignReplace"); break;
    case FcOpPrepend: printf ("Prepend"); break;
    case FcOpPrependFirst: printf ("PrependFirst"); break;
    case FcOpAppend: printf ("Append"); break;
    case FcOpAppendLast: printf ("AppendLast"); break;
    case FcOpQuest: printf ("Quest"); break;
    case FcOpOr: printf ("Or"); break;
    case FcOpAnd: printf ("And"); break;
    case FcOpEqual: printf ("Equal"); break;
    case FcOpContains: printf ("Contains"); break;
    case FcOpNotEqual: printf ("NotEqual"); break;
    case FcOpLess: printf ("Less"); break;
    case FcOpLessEqual: printf ("LessEqual"); break;
    case FcOpMore: printf ("More"); break;
    case FcOpMoreEqual: printf ("MoreEqual"); break;
    case FcOpPlus: printf ("Plus"); break;
    case FcOpMinus: printf ("Minus"); break;
    case FcOpTimes: printf ("Times"); break;
    case FcOpDivide: printf ("Divide"); break;
    case FcOpNot: printf ("Not"); break;
    case FcOpNil: printf ("Nil"); break;
    case FcOpComma: printf ("Comma"); break;
    case FcOpInvalid: printf ("Invalid"); break;
    }
}

void
FcExprPrint (FcExpr *expr)
{
    switch (expr->op) {
    case FcOpInteger: printf ("%d", expr->u.ival); break;
    case FcOpDouble: printf ("%g", expr->u.dval); break;
    case FcOpString: printf ("\"%s\"", expr->u.sval); break;
    case FcOpMatrix: printf ("[%g %g %g %g]",
			      expr->u.mval->xx,
			      expr->u.mval->xy,
			      expr->u.mval->yx,
			      expr->u.mval->yy);
    case FcOpBool: printf ("%s", expr->u.bval ? "true" : "false"); break;
    case FcOpCharSet: printf ("charset\n"); break;
    case FcOpNil: printf ("nil\n");
    case FcOpField: printf ("%s", expr->u.field); break;
    case FcOpConst: printf ("%s", expr->u.constant); break;
    case FcOpQuest:
	FcExprPrint (expr->u.tree.left);
	printf (" quest ");
	FcExprPrint (expr->u.tree.right->u.tree.left);
	printf (" colon ");
	FcExprPrint (expr->u.tree.right->u.tree.right);
	break;
    case FcOpAssign:
    case FcOpAssignReplace:
    case FcOpPrependFirst:
    case FcOpPrepend:
    case FcOpAppend:
    case FcOpAppendLast:
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
    case FcOpComma:
	FcExprPrint (expr->u.tree.left);
	printf (" ");
	switch (expr->op) {
	case FcOpAssign: printf ("Assign"); break;
	case FcOpAssignReplace: printf ("AssignReplace"); break;
	case FcOpPrependFirst: printf ("PrependFirst"); break;
	case FcOpPrepend: printf ("Prepend"); break;
	case FcOpAppend: printf ("Append"); break;
	case FcOpAppendLast: printf ("AppendLast"); break;
	case FcOpOr: printf ("Or"); break;
	case FcOpAnd: printf ("And"); break;
	case FcOpEqual: printf ("Equal"); break;
	case FcOpContains: printf ("Contains"); break;
	case FcOpNotEqual: printf ("NotEqual"); break;
	case FcOpLess: printf ("Less"); break;
	case FcOpLessEqual: printf ("LessEqual"); break;
	case FcOpMore: printf ("More"); break;
	case FcOpMoreEqual: printf ("MoreEqual"); break;
	case FcOpPlus: printf ("Plus"); break;
	case FcOpMinus: printf ("Minus"); break;
	case FcOpTimes: printf ("Times"); break;
	case FcOpDivide: printf ("Divide"); break;
	case FcOpComma: printf ("Comma"); break;
	default: break;
	}
	printf (" ");
	FcExprPrint (expr->u.tree.right);
	break;
    case FcOpNot:
	printf ("Not ");
	FcExprPrint (expr->u.tree.left);
	break;
    case FcOpInvalid: printf ("Invalid"); break;
    }
}

void
FcTestPrint (FcTest *test)
{
    switch (test->qual) {
    case FcQualAny:
	printf ("any ");
	break;
    case FcQualAll:
	printf ("all ");
	break;
    }
    printf ("%s ", test->field);
    FcOpPrint (test->op);
    printf (" ");
    FcExprPrint (test->expr);
    printf ("\n");
}

void
FcEditPrint (FcEdit *edit)
{
    printf ("Edit %s ", edit->field);
    FcOpPrint (edit->op);
    printf (" ");
    FcExprPrint (edit->expr);
}

void
FcSubstPrint (FcSubst *subst)
{
    FcEdit	*e;
    FcTest	*t;
    
    printf ("match\n");
    for (t = subst->test; t; t = t->next)
    {
	printf ("\t");
	FcTestPrint (t);
    }
    printf ("edit\n");
    for (e = subst->edit; e; e = e->next)
    {
	printf ("\t");
	FcEditPrint (e);
	printf (";\n");
    }
    printf ("\n");
}

void
FcFontSetPrint (FcFontSet *s)
{
    int	    i;

    printf ("FontSet %d of %d\n", s->nfont, s->sfont);
    for (i = 0; i < s->nfont; i++)
    {
	printf ("Font %d ", i);
	FcPatternPrint (s->fonts[i]);
    }
}

int
FcDebug (void)
{
    static int  initialized;
    static int  debug;

    if (!initialized)
    {
	char    *e;

	initialized = 1;
	e = getenv ("FC_DEBUG");
	if (e)
	{
	    printf ("FC_DEBUG=%s\n", e);
	    debug = atoi (e);
	    if (debug < 0)
		debug = 0;
	}
    }
    return debug;
}
