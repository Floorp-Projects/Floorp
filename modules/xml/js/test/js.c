/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS shell.
 */
#include "jsstddef.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "plarena.h"
#include "prlog.h"
#include "prprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"

#ifdef LIVECONNECT
#include "jsjava.h"
#endif

#ifdef JSDEBUGGER
#include "jsdebug.h"
#ifdef JSDEBUGGER_JAVA_UI
#include "jsdjava.h"
#endif /* JSDEBUGGER_JAVA_UI */
#endif /* JSDEBUGGER */

#ifdef XP_UNIX
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef XP_MAC
#define isatty(f) 1
#endif

#include "xmljs.h"

#ifndef JSFILE
# error "JSFILE must be defined for this module to work."
#endif

#ifdef JSDEBUGGER
static JSDContext *_jsdc;
#ifdef JSDEBUGGER_JAVA_UI
static JSDJContext *_jsdjc;
#endif /* JSDEBUGGER_JAVA_UI */
#endif /* JSDEBUGGER */

static void
Process(JSContext *cx, JSObject *obj, char *filename)
{
    JSTokenStream *ts;
    JSCodeGenerator cg;
    JSBool ok;
    JSScript *script;
    jsval result;
    JSString *str;

    ts = js_NewFileTokenStream(cx, filename, stdin);
    if (!ts)
	goto out;
#ifdef JSDEBUGGER
    if (!filename)
	ts->filename = "typein";
#endif
    if (isatty(fileno(ts->file))) {
	ts->flags |= TSF_INTERACTIVE;
    } else {
        /* Support the UNIX #! shell hack; gobble the first line if it starts
         * with '#'.  TODO - this isn't quite compatible with sharp variables,
         * as a legal js program (using sharp variables) might start with '#'.
         * But that would require multi-character lookahead.
         */
        char ch = fgetc(ts->file);
        if (ch == '#') {
            while((ch = fgetc(ts->file)) != EOF) {
                if(ch == '\n' || ch == '\r')
                    break;
            } 
        }
        ungetc(ch, ts->file);
    }

    do {
	js_InitCodeGenerator(cx, &cg, ts->filename, ts->lineno, ts->principals);
	do {
	    if (ts->flags & TSF_INTERACTIVE)
		printf("js> ");
	    ok = js_CompileTokenStream(cx, obj, ts, &cg);
	    if (ts->flags & TSF_ERROR) {
		ts->flags &= ~TSF_ERROR;
		CLEAR_PUSHBACK(ts);
		ok = JS_TRUE;
	    }
	} while (ok && !(ts->flags & TSF_EOF) && CG_OFFSET(&cg) == 0);
	if (ok) {
	    script = js_NewScriptFromCG(cx, &cg, NULL);
	    if (script) {
		if (JS_ExecuteScript(cx, obj, script, &result) &&
		    (ts->flags & TSF_INTERACTIVE) &&
		    result != JSVAL_VOID) {
		    str = JS_ValueToString(cx, result);
		    if (str)
			printf("%s\n", JS_GetStringBytes(str));
		}
		JS_DestroyScript(cx, script);
	    }
	}
	cg.firstLine = ts->lineno;
	js_ResetCodeGenerator(cx, &cg);
	RESET_TOKENBUF(ts);
    } while (!(ts->flags & TSF_EOF));

out:
    if (ts)
	(void) js_CloseTokenStream(cx, ts);
    PR_FreeArenaPool(&cx->codePool);
    PR_FreeArenaPool(&cx->tempPool);
}

static JSBool
Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc > 0 && JSVAL_IS_INT(argv[0]))
	*rval = INT_TO_JSVAL(JS_SetVersion(cx, JSVAL_TO_INT(argv[0])));
    else
	*rval = INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

static JSBool
Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;
    const char *filename;
    JSScript *script;
    JSBool ok;
    jsval result;

    for (i = 0; i < argc; i++) {
	str = JS_ValueToString(cx, argv[i]);
	if (!str)
	    return JS_FALSE;
	argv[i] = STRING_TO_JSVAL(str);
	filename = JS_GetStringBytes(str);
	errno = 0;
	script = JS_CompileFile(cx, obj, filename);
	if (!script) {
	    fprintf(stderr, "js: cannot load %s", filename);
	    if (errno)
		fprintf(stderr, ": %s", strerror(errno));
	    putc('\n', stderr);
	    continue;
	}
	ok = JS_ExecuteScript(cx, obj, script, &result);
	JS_DestroyScript(cx, script);
	if (!ok)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, n;
    JSString *str;

    for (i = n = 0; i < argc; i++) {
	str = JS_ValueToString(cx, argv[i]);
	if (!str)
	    return JS_FALSE;
	printf("%s%s", i ? " " : "", JS_GetStringBytes(str));
	n++;
    }
    if (n)
	putchar('\n');
    return JS_TRUE;
}

static JSBool
Help(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static JSBool
Quit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#ifdef LIVECONNECT
    JSJ_SimpleShutdown();
#endif
    exit(0);
    return JS_FALSE;
}

#ifdef GC_MARK_DEBUG
extern JS_FRIEND_DATA(FILE *) js_DumpGCHeap;
#endif

static JSBool
GC(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSRuntime *rt;
    uint32 preBytes;

    rt = cx->runtime;
    preBytes = rt->gcBytes;
#ifdef GC_MARK_DEBUG
    if (argc && JSVAL_IS_STRING(argv[0])) {
    	char *name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    	FILE *file = fopen(name, "w");
    	if (!file) {
	    fprintf(stderr, "gc: can't open %s: %s\n", strerror(errno));
	    return JS_FALSE;
    	}
    	js_DumpGCHeap = file;
    } else {
	js_DumpGCHeap = stdout;
    }
#endif
    JS_GC(cx);
#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap != stdout)
    	fclose(js_DumpGCHeap);
    js_DumpGCHeap = NULL;
#endif
    printf("before %lu, after %lu, break %08lx\n",
	   (unsigned long)preBytes, (unsigned long)rt->gcBytes,
#ifdef XP_UNIX
	   (unsigned long)sbrk(0)
#else
	   0
#endif
	   );
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    return JS_TRUE;
}

static JSBool
GetTrapArgs(JSContext *cx, uintN argc, jsval *argv, JSScript **scriptp,
	    int32 *ip)
{
    uintN intarg;
    JSFunction *fun;

    *scriptp = cx->fp->down->script;
    *ip = 0;
    if (argc != 0) {
	intarg = 0;
	if (JS_TypeOfValue(cx, argv[0]) == JSTYPE_FUNCTION) {
	    fun = JS_ValueToFunction(cx, argv[0]);
	    if (!fun)
		return JS_FALSE;
	    *scriptp = fun->script;
	    intarg++;
	}
	if (argc > intarg) {
	    if (!JS_ValueToInt32(cx, argv[intarg], ip))
		return JS_FALSE;
	}
    }
    return JS_TRUE;
}

static JSTrapStatus
TrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
	    void *closure)
{
    JSString *str;
    JSStackFrame *caller;

    str = closure;
    caller = cx->fp->down;
    if (!JS_EvaluateScript(cx, caller->scopeChain,
			   JS_GetStringBytes(str), JS_GetStringLength(str),
			   caller->script->filename, caller->script->lineno,
			   rval)) {
	return JSTRAP_ERROR;
    }
    if (*rval != JSVAL_VOID)
	return JSTRAP_RETURN;
    return JSTRAP_CONTINUE;
}

static JSBool
Trap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    JSScript *script;
    int32 i;

    if (argc == 0) {
	JS_ReportError(cx, "usage: trap [fun] [pc] expr");
	return JS_FALSE;
    }
    argc--;
    str = JS_ValueToString(cx, argv[argc]);
    if (!str)
	return JS_FALSE;
    argv[argc] = STRING_TO_JSVAL(str);
    if (!GetTrapArgs(cx, argc, argv, &script, &i))
	return JS_FALSE;
    return JS_SetTrap(cx, script, script->code + i, TrapHandler, str);
}

static JSBool
Untrap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSScript *script;
    int32 i;

    if (!GetTrapArgs(cx, argc, argv, &script, &i))
	return JS_FALSE;
    JS_ClearTrap(cx, script, script->code + i, NULL, NULL);
    return JS_TRUE;
}

static JSBool
LineToPC(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSScript *script;
    int32 i;
    uintN lineno;
    jsbytecode *pc;

    if (argc == 0) {
	JS_ReportError(cx, "usage: line2pc [fun] line");
	return JS_FALSE;
    }
    script = cx->fp->down->script;
    if (!GetTrapArgs(cx, argc, argv, &script, &i))
	return JS_FALSE;
    lineno = (i == 0) ? script->lineno : (uintN)i;
    pc = JS_LineNumberToPC(cx, script, lineno);
    if (!pc)
	return JS_FALSE;
    *rval = INT_TO_JSVAL(pc - script->code);
    return JS_TRUE;
}

static JSBool
PCToLine(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSScript *script;
    int32 i;
    uintN lineno;

    if (!GetTrapArgs(cx, argc, argv, &script, &i))
	return JS_FALSE;
    lineno = JS_PCToLineNumber(cx, script, script->code + i);
    if (!lineno)
	return JS_FALSE;
    *rval = INT_TO_JSVAL(lineno);
    return JS_TRUE;
}

#ifdef DEBUG

static void
SingleNote(JSContext *cx, JSFunction *fun )
{
    uintN offset, delta;
    jssrcnote *notes, *sn;
    JSSrcNoteType type;
    jsatomid atomIndex;
    JSAtom *atom;

    notes = fun->script->notes;
    if (notes) {
	printf("\nSource notes:\n");
	offset = 0;
	for (sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
	    delta = SN_DELTA(sn);
	    offset += delta;
	    printf("%3u: %5u [%4u] %-8s",
		   sn - notes, offset, delta, js_SrcNoteName[SN_TYPE(sn)]);
	    type = SN_TYPE(sn);
	    switch (type) {
	      case SRC_SETLINE:
		printf(" lineno %u", (uintN) js_GetSrcNoteOffset(sn, 0));
		break;
	      case SRC_FOR:
		printf(" cond %u update %u tail %u",
		       (uintN) js_GetSrcNoteOffset(sn, 0),
		       (uintN) js_GetSrcNoteOffset(sn, 1),
		       (uintN) js_GetSrcNoteOffset(sn, 2));
		break;
	      case SRC_COMMA:
	      case SRC_PCBASE:
		printf(" offset %u", (uintN) js_GetSrcNoteOffset(sn, 0));
		break;
	      case SRC_LABEL:
	      case SRC_LABELBRACE:
	      case SRC_BREAK2LABEL:
	      case SRC_CONT2LABEL:
	      case SRC_FUNCDEF:
		atomIndex = (jsatomid) js_GetSrcNoteOffset(sn, 0);
		atom = js_GetAtom(cx, &fun->script->atomMap, atomIndex);
		printf(" atom %u (%s)", (uintN)atomIndex, ATOM_BYTES(atom));
		break;
	      default:;
	    }
	    putchar('\n');
	}
    }
}

static JSBool
Notes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSFunction *fun;

    for (i = 0; i < argc; i++) {
	fun = JS_ValueToFunction(cx, argv[i]);
	if (!fun)
	    return JS_FALSE;

	SingleNote(cx, fun);
    }
    return JS_TRUE;
}

static JSBool
ExceptionTable(JSContext *cx, JSFunction *fun)
{
    JSTryNote *iter = fun->script->trynotes;

    if (!iter)
	return JS_TRUE;
    printf("\nException table:\nstart\tend\tcatch\tfinally\n");
    while (iter->start && iter->end) {
	printf("  %d\t%d\t%d\t%d\n",
	       iter->start, iter->end, iter->catch, iter->finally);
	iter++;
    }
    return JS_TRUE;
}

static JSBool
Disassemble(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool lines;
    uintN i;
    JSFunction *fun;

    if (argc > 0 &&
	JSVAL_IS_STRING(argv[0]) &&
	!strcmp(JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), "-l")) {
	lines = JS_TRUE;
	argv++, argc--;
    } else {
	lines = JS_FALSE;
    }
    for (i = 0; i < argc; i++) {
	fun = JS_ValueToFunction(cx, argv[i]);
	if (!fun)
	    return JS_FALSE;

	js_Disassemble(cx, fun->script, lines, stdout);
	SingleNote(cx, fun);
	ExceptionTable(cx, fun);
    }
    return JS_TRUE;
}

static JSBool
DisassWithSrc(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#define LINE_BUF_LEN 512
    uintN i, len, line1, line2, bupline;
    JSFunction *fun;
    FILE *file;
    char linebuf[LINE_BUF_LEN];
    jsbytecode *pc, *end;
    static char sep[] = ";-------------------------";

    for (i = 0; i < argc; i++) {
	fun = JS_ValueToFunction(cx, argv[i]);
	if (!fun)
	    return JS_FALSE;

	if (!fun->script || !fun->script->filename) {
	    JS_ReportError(cx, "only works on JS scripts read from files");
	    return JS_FALSE;
	}

	file = fopen(fun->script->filename, "r");
	if (!file) {
	    JS_ReportError(cx, "can't open %s: %s", fun->script->filename, strerror(errno));
	    return JS_FALSE;
	}

	pc = fun->script->code;
	end = pc + fun->script->length;

	/* burn the leading lines */
	line2 = JS_PCToLineNumber(cx, fun->script, pc);
	for (line1 = 0; line1 < line2 - 1; line1++)
	    fgets(linebuf, LINE_BUF_LEN, file);

	bupline = 0;
	while (pc < end) {
	    line2 = JS_PCToLineNumber(cx, fun->script, pc);

	    if (line2 < line1) {
		if (bupline != line2) {
		    bupline = line2;
		    printf("%s %3u: BACKUP\n", sep, line2);
		}
	    } else {
		if (bupline && line1 == line2)
		    printf("%s %3u: RESTORE\n", sep, line2);
		bupline = 0;
		while (line1 < line2) {
		    if (!fgets(linebuf, LINE_BUF_LEN, file)) {
			JS_ReportError(cx, "unexpected EOF in %s",
				       fun->script->filename);
		    	goto bail;
		    }
		    line1++;
		    printf("%s %3u: %s", sep, line1, linebuf);
		}
	    }

	    len = js_Disassemble1(cx, fun->script, pc, pc - fun->script->code,
				  JS_TRUE, stdout);
	    if (!len)
		return JS_FALSE;
	    pc += len;
	}

      bail:
	fclose(file);
    }
    return JS_TRUE;
}

static JSBool
Tracing(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool bval;
    JSString *str;

    if (argc == 0) {
	*rval = BOOLEAN_TO_JSVAL(cx->tracefp != 0);
	return JS_TRUE;
    }

    switch (JS_TypeOfValue(cx, argv[0])) {
      case JSTYPE_NUMBER:
	bval = JSVAL_IS_INT(argv[0])
	       ? JSVAL_TO_INT(argv[0])
	       : (jsint) *JSVAL_TO_DOUBLE(argv[0]);
	break;
      case JSTYPE_BOOLEAN:
	bval = JSVAL_TO_BOOLEAN(argv[0]);
	break;
      default:
	str = JS_ValueToString(cx, argv[0]);
	if (!str)
	    return JS_FALSE;
	fprintf(stderr, "tracing: illegal argument %s\n",
		JS_GetStringBytes(str));
	return JS_TRUE;
    }
    cx->tracefp = bval ? stdout : NULL;
    return JS_TRUE;
}

int
DumpAtom(PRHashEntry *he, int i, void *arg)
{
    FILE *fp = arg;
    JSAtom *atom = (JSAtom *)he;

    fprintf(fp, "%3d %08x %5lu ",
	    i, (uintN)he->keyHash, (unsigned long)atom->number);
    if (ATOM_IS_STRING(atom))
	fprintf(fp, "\"%s\"\n", ATOM_BYTES(atom));
    else if (ATOM_IS_INT(atom))
	fprintf(fp, "%ld\n", (long)ATOM_TO_INT(atom));
    else
	fprintf(fp, "%.16g\n", *ATOM_TO_DOUBLE(atom));
    return HT_ENUMERATE_NEXT;
}

int
DumpSymbol(PRHashEntry *he, int i, void *arg)
{
    FILE *fp = arg;
    JSSymbol *sym = (JSSymbol *)he;

    fprintf(fp, "%3d %08x", i, (uintN)he->keyHash);
    if (JSVAL_IS_INT(sym_id(sym)))
	fprintf(fp, " [%ld]\n", (long)JSVAL_TO_INT(sym_id(sym)));
    else
	fprintf(fp, " \"%s\"\n", ATOM_BYTES(sym_atom(sym)));
    return HT_ENUMERATE_NEXT;
}

extern JS_FRIEND_DATA(JSScopeOps) js_list_scope_ops;

void
DumpScope(JSContext *cx, JSObject *obj, PRHashEnumerator dump, FILE *fp)
{
    JSScope *scope;
    JSSymbol *sym;
    int i;

    fprintf(fp, "\n%s scope contents:\n", OBJ_GET_CLASS(cx, obj)->name);
    scope = (JSScope *)obj->map;
    if (!MAP_IS_NATIVE(&scope->map))
    	return;
    if (scope->ops == &js_list_scope_ops) {
	for (sym = (JSSymbol *)scope->data, i = 0; sym;
	     sym = (JSSymbol *)sym->entry.next, i++) {
	    DumpSymbol(&sym->entry, i, fp);
	}
    } else {
	PR_HashTableDump(scope->data, dump, fp);
    }
}

/* These are callable from gdb. */
void Dsym(JSSymbol *sym) { if (sym) DumpSymbol(&sym->entry, 0, stderr); }
void Datom(JSAtom *atom) { if (atom) DumpAtom(&atom->entry, 0, stderr); }

static JSBool
DumpStats(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;
    const char *bytes;
    JSAtom *atom;
    JSObject *obj2;
    JSProperty *prop;
    jsval value;

    for (i = 0; i < argc; i++) {
	str = JS_ValueToString(cx, argv[i]);
	if (!str)
	    return JS_FALSE;
	bytes = JS_GetStringBytes(str);
	if (strcmp(bytes, "arena") == 0) {
#ifdef ARENAMETER
	    PR_DumpArenaStats(stdout);
#endif
	} else if (strcmp(bytes, "atom") == 0) {
	    printf("\natom table contents:\n");
	    PR_HashTableDump(cx->runtime->atomState.table, DumpAtom, stdout);
	} else if (strcmp(bytes, "global") == 0) {
	    DumpScope(cx, cx->globalObject, DumpSymbol, stdout);
	} else {
	    atom = js_Atomize(cx, bytes, JS_GetStringLength(str), 0);
	    if (!atom)
		return JS_FALSE;
	    if (!js_FindProperty(cx, (jsid)atom, &obj, &obj2, &prop))
		return JS_FALSE;
	    if (prop) {
		OBJ_DROP_PROPERTY(cx, obj2, prop);
		if (!OBJ_GET_PROPERTY(cx, obj, (jsid)atom, &value))
		    return JS_FALSE;
	    }
	    if (!prop || !JSVAL_IS_OBJECT(value)) {
		fprintf(stderr, "js: invalid stats argument %s\n",
			bytes);
	    	continue;
	    }
	    obj = JSVAL_TO_OBJECT(value);
	    if (obj)
		DumpScope(cx, obj, DumpSymbol, stdout);
	}
    }
    return JS_TRUE;
}

#endif /* DEBUG */

#ifdef TEST_EXPORT
static JSBool
DoExport(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSAtom *atom;
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;
    uintN attrs;

    if (argc != 2) {
	JS_ReportError(cx, "usage: doexp obj id");
	return JS_FALSE;
    }
    if (!JS_ValueToObject(cx, argv[0], &obj))
	return JS_FALSE;
    argv[0] = OBJECT_TO_JSVAL(obj);
    atom = js_ValueToStringAtom(cx, argv[1]);
    if (!atom)
	return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &obj2, &prop))
	return JS_FALSE;
    if (!prop) {
	ok = OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID, NULL, NULL,
				 JSPROP_EXPORTED, NULL);
    } else {
	ok = OBJ_GET_ATTRIBUTES(cx, obj, (jsid)atom, prop, &attrs);
	if (ok) {
	    attrs |= JSPROP_EXPORTED;
	    ok = OBJ_SET_ATTRIBUTES(cx, obj, (jsid)atom, prop, &attrs);
	}
	OBJ_DROP_PROPERTY(cx, obj2, prop);
    }
    return ok;
}
#endif

#ifdef TEST_CVTARGS
static JSBool
ConvertArgs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool b;
    jschar c;
    int32 i, j;
    uint32 u;
    jsdouble d, I;
    char *s;
    JSString *str;
    JSObject *obj;
    JSFunction *fun;
    jsval v;

    if (!JS_ConvertArguments(cx, argc, argv, "b/ciujdIsSofv*",
			     &b, &c, &i, &u, &j, &d, &I, &s, &str, &obj, &fun,
			     &v)) {
	return JS_FALSE;
    }
    printf("b %u, c %x (%c), i %ld, u %lu, j %ld\n", b, c, (char)c, i, u, j);
    printf("d %g, I %g, s %s, S %s, obj %s, fun %s, v %s\n",
    	   d, I, s, JS_GetStringBytes(str),
    	   JS_GetStringBytes(JS_ValueToString(cx, OBJECT_TO_JSVAL(obj))),
    	   JS_GetStringBytes(JS_DecompileFunction(cx, fun, 4)),
    	   JS_GetStringBytes(JS_ValueToString(cx, v)));
    return JS_TRUE;
}
#endif

static JSFunctionSpec shell_functions[] = {
    {"version",         Version,        0},
    {"load",            Load,           1},
    {"print",           Print,          0},
    {"help",            Help,           0},
    {"quit",            Quit,           0},
    {"gc",              GC,             0},
    {"trap",            Trap,           3},
    {"untrap",          Untrap,         2},
    {"line2pc",         LineToPC,       0},
    {"pc2line",         PCToLine,       0},
#ifdef DEBUG
    {"dis",             Disassemble,    1},
    {"dissrc",          DisassWithSrc,  1},
    {"notes",           Notes,          1},
    {"tracing",         Tracing,        0},
    {"stats",           DumpStats,      1},
#endif
#ifdef TEST_EXPORT
    {"doexp",           DoExport,       2},
#endif
#ifdef TEST_CVTARGS
    {"cvtargs",         ConvertArgs,    0, 0, 12},
#endif
    {0}
};

/* NOTE: These must be kept in sync with the above. */

static char *shell_help_messages[] = {
    "version [number]       Get or set JavaScript version number",
    "load ['foo.js' ...]    Load files named by string arguments",
    "print [expr ...]       Evaluate and print expressions",
    "help [name ...]        Display usage and help messages",
    "quit                   Quit mocha",
    "gc                     Run the garbage collector",
    "trap [fun] [pc] expr   Trap bytecode execution",
    "untrap [fun] [pc]      Remove a trap",
    "line2pc [fun] line     Map line number to PC",
    "pc2line [fun] [pc]     Map PC to line number",
#ifdef DEBUG
    "dis [fun]              Disassemble functions into bytecodes",
    "dissrc [fun]           Disassemble functions with source lines",
    "notes [fun]            Show source notes for functions",
    "tracing [toggle]       Turn tracing on or off",
    "stats [string ...]     Dump 'arena', 'atom', 'global' stats",
#endif
#ifdef TEST_EXPORT
    "doexp obj id           Export identified property from object",
#endif
#ifdef TEST_CVTARGS
    "cvtargs b c ...        Test JS_ConvertArguments",
#endif
    0
};

static void
ShowHelpHeader(void)
{
    printf("%-9s %-22s %s\n", "Command", "Usage", "Description");
    printf("%-9s %-22s %s\n", "=======", "=====", "===========");
}

static void
ShowHelpForCommand(uintN n)
{
    printf("%-9.9s %s\n", shell_functions[n].name, shell_help_messages[n]);
}

static JSBool
Help(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, j;
    int did_header, did_something;
    JSType type;
    JSFunction *fun;
    JSString *str;
    const char *bytes;

    if (argc == 0) {
	ShowHelpHeader();
	for (i = 0; shell_functions[i].name; i++)
	    ShowHelpForCommand(i);
    } else {
	did_header = 0;
	for (i = 0; i < argc; i++) {
	    did_something = 0;
	    type = JS_TypeOfValue(cx, argv[i]);
	    if (type == JSTYPE_FUNCTION) {
		fun = JS_ValueToFunction(cx, argv[i]);
		str = fun->atom ? ATOM_TO_STRING(fun->atom) : NULL;
	    } else if (type == JSTYPE_STRING) {
		str = JSVAL_TO_STRING(argv[i]);
	    } else {
		str = NULL;
	    }
	    if (str) {
		bytes = JS_GetStringBytes(str);
		for (j = 0; shell_functions[j].name; j++) {
		    if (!strcmp(bytes, shell_functions[j].name)) {
			if (!did_header) {
			    did_header = 1;
			    ShowHelpHeader();
			}
			did_something = 1;
			ShowHelpForCommand(j);
			break;
		    }
		}
	    }
	    if (!did_something) {
		str = JS_ValueToString(cx, argv[i]);
		if (!str)
		    return JS_FALSE;
		fprintf(stderr, "Sorry, no help for %s\n",
			JS_GetStringBytes(str));
	    }
	}
    }
    return JS_TRUE;
}

/*
 * Define a JS object called "it".  Give it class operations that printf why
 * they're being called for tutorial purposes.
 */
enum its_tinyid {
    ITS_COLOR, ITS_HEIGHT, ITS_WIDTH, ITS_FUNNY, ITS_ARRAY, ITS_RDONLY
};

static JSPropertySpec its_props[] = {
    {"color",           ITS_COLOR,	JSPROP_ENUMERATE},
    {"height",          ITS_HEIGHT,	JSPROP_ENUMERATE},
    {"width",           ITS_WIDTH,	JSPROP_ENUMERATE},
    {"funny",           ITS_FUNNY,	JSPROP_ENUMERATE},
    {"array",           ITS_ARRAY,	JSPROP_ENUMERATE},
    {"rdonly",		ITS_RDONLY,	JSPROP_READONLY},
    {0}
};

static JSBool its_noisy;    /* whether to be noisy when finalizing it */

static JSBool
its_addProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
	printf("adding its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" initial value %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_delProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
	printf("deleting its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" current value %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
	printf("getting its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" current value %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    return JS_TRUE;
}

static JSBool
its_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    if (its_noisy) {
	printf("setting its property %s,",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
	printf(" new value %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, *vp)));
    }
    if (JSVAL_IS_STRING(id) &&
	!strcmp(JS_GetStringBytes(JSVAL_TO_STRING(id)), "noisy")) {
	return JS_ValueToBoolean(cx, *vp, &its_noisy);
    }
    return JS_TRUE;
}

static JSBool
its_enumerate(JSContext *cx, JSObject *obj)
{
    if (its_noisy)
	printf("enumerate its properties\n");
    return JS_TRUE;
}

static JSBool
its_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    if (its_noisy) {
	printf("resolving its property %s\n",
	       JS_GetStringBytes(JS_ValueToString(cx, id)));
    }
    return JS_TRUE;
}

static JSBool
its_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    if (its_noisy)
	printf("converting it to %s type\n", JS_GetTypeName(cx, type));
    return JS_TRUE;
}

static void
its_finalize(JSContext *cx, JSObject *obj)
{
    if (its_noisy)
	printf("finalizing it\n");
}

static JSClass its_class = {
    "It", 0,
    its_addProperty,  its_delProperty,  its_getProperty,  its_setProperty,
    its_enumerate,    its_resolve,      its_convert,      its_finalize
};

static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    int i, j, k, n;

    fputs("js: ", stderr);
    if (!report) {
	fprintf(stderr, "%s\n", message);
	return;
    }

    if (report->filename)
	fprintf(stderr, "%s, ", report->filename);
    if (report->lineno)
	fprintf(stderr, "line %u: ", report->lineno);
    fputs(message, stderr);
    if (!report->linebuf) {
	putc('\n', stderr);
	return;
    }

    fprintf(stderr, ":\n%s\n", report->linebuf);
    n = report->tokenptr - report->linebuf;
    for (i = j = 0; i < n; i++) {
	if (report->linebuf[i] == '\t') {
	    for (k = (j + 8) & ~7; j < k; j++)
		putc('.', stderr);
	    continue;
	}
	putc('.', stderr);
	j++;
    }
    fputs("^\n", stderr);
}

#if defined DEBUG && defined XP_UNIX
static JSBool
Exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    const char *name, **nargv;
    uintN i, nargc;
    JSString *str;
    pid_t pid;
    int status;

    fun = JS_ValueToFunction(cx, argv[-2]);
    if (!fun)
	return JS_FALSE;
    if (!fun->atom)
	return JS_TRUE;
    name = ATOM_BYTES(fun->atom);
    nargc = 1 + argc;
    nargv = JS_malloc(cx, (nargc + 1) * sizeof(char *));
    if (!nargv)
	return JS_FALSE;
    nargv[0] = name;
    for (i = 1; i < nargc; i++) {
	str = JS_ValueToString(cx, argv[i-1]);
	if (!str) {
	    JS_free(cx, nargv);
	    return JS_FALSE;
	}
	nargv[i] = JS_GetStringBytes(str);
    }
    nargv[nargc] = 0;
    pid = fork();
    switch (pid) {
      case -1:
	perror("js");
	break;
      case 0:
	(void) execvp(name, (char **)nargv);
	perror("js");
	exit(127);
      default:
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
	    ;
	break;
    }
    JS_free(cx, nargv);
    return JS_TRUE;
}
#endif

static JSBool
global_resolve(JSContext *cx, JSObject *obj, jsval id)
{
#if defined DEBUG && defined XP_UNIX
    /*
     * Do this expensive hack only for unoptimized Unix builds, which are not
     * used for benchmarking.
     */
    char *path, *comp, *full;
    const char *name;
    JSBool ok, found;
    JSFunction *fun;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;
    path = getenv("PATH");
    if (!path)
	return JS_TRUE;
    path = JS_strdup(cx, path);
    if (!path)
	return JS_FALSE;
    name = JS_GetStringBytes(JSVAL_TO_STRING(id));
    ok = JS_TRUE;
    for (comp = strtok(path, ":"); comp; comp = strtok(NULL, ":")) {
	if (*comp != '\0') {
	    full = PR_smprintf("%s/%s", comp, name);
	    if (!full) {
		JS_ReportOutOfMemory(cx);
		ok = JS_FALSE;
		break;
	    }
	} else {
	    full = (char *)name;
	}
	found = (access(full, X_OK) == 0);
	if (*comp != '\0')
	    free(full);
	if (found) {
	    fun = JS_DefineFunction(cx, obj, name, Exec, 0, JSPROP_ENUMERATE);
	    ok = (fun != NULL);
	    break;
	}
    }
    JS_free(cx, path);
    return ok;
#else
    return JS_TRUE;
#endif
}

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, global_resolve,   JS_ConvertStub,   JS_FinalizeStub
};

int
main(int argc, char **argv)
{
    int c, i;
    JSVersion version;
    JSRuntime *rt;
    JSContext *cx;
    JSObject *glob, *it;

#ifdef XP_OS2
   /* these streams are normally line buffered on OS/2 and need a \n, *
    * so we need to unbuffer then to get a reasonable prompt          */
    setbuf(stdout,0);
    setbuf(stderr,0);
#endif

    version = JSVERSION_DEFAULT;
#ifdef XP_UNIX
    while ((c = getopt(argc, argv, "v:")) != -1) {
	switch (c) {
	  case 'v':
	    version = atoi(optarg);
	    break;
	  default:
	    fprintf(stderr, "usage: js [-v version]\n");
	    return 2;
	}
    }
    argc -= optind;
    argv += optind;
#else
    c = -1;
    argc--;
    argv++;
#endif

    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
	return 1;
    cx = JS_NewContext(rt, 8192);
    if (!cx)
	return 1;
    JS_SetErrorReporter(cx, my_ErrorReporter);

    glob = JS_NewObject(cx, &global_class, NULL, NULL);
    if (!glob)
	return 1;
    if (!JS_InitStandardClasses(cx, glob))
	return 1;
    if (!XML_Init(cx, glob))
        return 1;
    if (!JS_DefineFunctions(cx, glob, shell_functions))
	return 1;

    /* Set version only after there is a global object. */
    if (version != JSVERSION_DEFAULT)
	JS_SetVersion(cx, version);

    it = JS_DefineObject(cx, glob, "it", &its_class, NULL, 0);
    if (!it)
	return 1;
    if (!JS_DefineProperties(cx, it, its_props))
	return 1;

#ifdef LIVECONNECT
	if (!JSJ_SimpleInit(cx, glob, NULL, getenv("CLASSPATH")))
            return 1;
#endif

#ifdef JSDEBUGGER
    /*
    * XXX A command line option to enable debugging (or not) would be good
    */
    _jsdc = JSD_DebuggerOnForUser(rt, NULL, NULL);
    if (!_jsdc)
	return 1;
    JSD_JSContextInUse(_jsdc, cx);

#ifdef JSDEBUGGER_JAVA_UI
    _jsdjc = JSDJ_CreateContext();
    if (! _jsdjc)
	return 1;
    JSDJ_SetJSDContext(_jsdjc, _jsdc);
    JSDJ_CreateJavaVMAndStartDebugger(_jsdjc);
    /*
    * XXX This would be the place to wait for the debugger to start.
    * Waiting would be nice in general, but especially when a js file
    * is passed on the cmd line.
    */
#endif /* JSDEBUGGER_JAVA_UI */
#endif /* JSDEBUGGER */

    if (argc > 0) {
	for (i = 0; i < argc; i++)
	    Process(cx, glob, argv[i]);
    } else {
	Process(cx, glob, NULL);
    }

#ifdef JSDEBUGGER
    if (_jsdc)
	JSD_DebuggerOff(_jsdc);
#endif  /* JSDEBUGGER */

    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return 0;
}
