/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is Darin Fisher.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "prefread.h"

#ifdef TEST_PREFREAD
#include <stdio.h>
#define NS_WARNING(_s) printf(">>> " _s "!\n")
#define NS_NOTREACHED(_s) NS_WARNING(_s)
#else
#include "nsDebug.h" // for NS_WARNING
#endif

/* pref parser states */
enum {
    PREF_PARSE_INIT,
    PREF_PARSE_MATCH_STRING,
    PREF_PARSE_UNTIL_NAME,
    PREF_PARSE_QUOTED_STRING,
    PREF_PARSE_UNTIL_COMMA,
    PREF_PARSE_UNTIL_VALUE,
    PREF_PARSE_INT_VALUE,
    PREF_PARSE_COMMENT_MAYBE_START,
    PREF_PARSE_COMMENT_BLOCK,
    PREF_PARSE_COMMENT_BLOCK_MAYBE_END,
    PREF_PARSE_ESC_SEQUENCE,
    PREF_PARSE_UNTIL_OPEN_PAREN,
    PREF_PARSE_UNTIL_CLOSE_PAREN,
    PREF_PARSE_UNTIL_SEMICOLON,
    PREF_PARSE_UNTIL_EOL
};

static const char kUserPref[] = "user_pref";
static const char kPref[] = "pref";
static const char kTrue[] = "true";
static const char kFalse[] = "false";

/**
 * pref_GrowBuf
 * 
 * this function will increase the size of the buffer owned
 * by the given pref parse state.  the only requirement is
 * that it increase the buffer by at least one byte, but we
 * use a simple doubling algorithm.
 * 
 * this buffer is used to store partial pref lines.  it is
 * freed when the parse state is destroyed.
 *
 * @param ps
 *        parse state instance
 *
 * this function updates all pointers that reference an 
 * address within lb since realloc may relocate the buffer.
 *
 * @return PR_FALSE if insufficient memory.
 */
static PRBool
pref_GrowBuf(PrefParseState *ps)
{
    int bufLen, curPos, valPos;

    bufLen = ps->lbend - ps->lb;
    curPos = ps->lbcur - ps->lb;
    valPos = ps->vb    - ps->lb;

    if (bufLen == 0)
        bufLen = 128;  /* default buffer size */
    else
        bufLen <<= 1;  /* double buffer size */

#ifdef TEST_PREFREAD
    fprintf(stderr, ">>> realloc(%d)\n", bufLen);
#endif

    ps->lb = (char*) realloc(ps->lb, bufLen);
    if (!ps->lb)
        return PR_FALSE;

    ps->lbcur = ps->lb + curPos;
    ps->lbend = ps->lb + bufLen;
    ps->vb    = ps->lb + valPos;

    return PR_TRUE;
}

/**
 * pref_DoCallback
 *
 * this function is called when a complete pref name-value pair has
 * been extracted from the input data.
 *
 * @param ps
 *        parse state instance
 *
 * @return PR_FALSE to indicate a fatal error.
 */
static PRBool
pref_DoCallback(PrefParseState *ps)
{
    PrefValue  value;

    switch (ps->vtype) {
    case PREF_STRING:
        value.stringVal = ps->vb;
        break;
    case PREF_INT:
        if ((ps->vb[0] == '-' || ps->vb[0] == '+') && ps->vb[1] == '\0') {
            NS_WARNING("malformed integer value");
            return PR_FALSE;
        }
        value.intVal = atoi(ps->vb);
        break;
    case PREF_BOOL:
        value.boolVal = (ps->vb == kTrue);
        break;
    default:
        break;
    }
    (*ps->reader)(ps->closure, ps->lb, value, ps->vtype, ps->fdefault);
    return PR_TRUE;
}

void
PREF_InitParseState(PrefParseState *ps, PrefReader reader, void *closure)
{
    memset(ps, 0, sizeof(*ps));
    ps->reader = reader;
    ps->closure = closure;
}

void
PREF_FinalizeParseState(PrefParseState *ps)
{
    if (ps->lb)
        free(ps->lb);
}

/**
 * Pseudo-BNF
 * ----------
 * function      = LJUNK function-name JUNK function-args
 * function-name = "user_pref" | "pref"
 * function-args = "(" JUNK pref-name JUNK "," JUNK pref-value JUNK ")" JUNK ";"
 * pref-name     = quoted-string
 * pref-value    = quoted-string | "true" | "false" | integer-value
 * JUNK          = *(WS | comment-block | comment-line)
 * LJUNK         = *(WS | comment-block | comment-line | bcomment-line)
 * WS            = SP | HT | LF | VT | FF | CR
 * SP            = <US-ASCII SP, space (32)>
 * HT            = <US-ASCII HT, horizontal-tab (9)>
 * LF            = <US-ASCII LF, linefeed (10)>
 * VT            = <US-ASCII HT, vertical-tab (11)>
 * FF            = <US-ASCII FF, form-feed (12)>
 * CR            = <US-ASCII CR, carriage return (13)>
 * comment-block = <C/C++ style comment block>
 * comment-line  = <C++ style comment line>
 * bcomment-line = <bourne-shell style comment line>
 */
PRBool
PREF_ParseBuf(PrefParseState *ps, const char *buf, int bufLen)
{
    const char *end;
    char c;
    int state;

    state = ps->state;
    for (end = buf + bufLen; buf != end; ++buf) {
        c = *buf;
        switch (state) {
        /* initial state */
        case PREF_PARSE_INIT:
            if (ps->lbcur != ps->lb) { /* reset state */
                ps->lbcur = ps->lb;
                ps->vb    = NULL;
                ps->vtype = PREF_INVALID;
                ps->fdefault = PR_FALSE;
            }
            switch (c) {
            case '/':       /* begin comment block or line? */
                state = PREF_PARSE_COMMENT_MAYBE_START;
                break;
            case '#':       /* accept shell style comments */
                state = PREF_PARSE_UNTIL_EOL;
                break;
            case 'u':       /* indicating user_pref */
            case 'p':       /* indicating pref */
                ps->smatch = (c == 'u' ? kUserPref : kPref);
                ps->sindex = 1;
                ps->nextstate = PREF_PARSE_UNTIL_OPEN_PAREN;
                state = PREF_PARSE_MATCH_STRING;
                break;
            /* else skip char */
            }
            break;

        /* string matching */
        case PREF_PARSE_MATCH_STRING:
            if (c == ps->smatch[ps->sindex++]) {
                /* if we've matched all characters, then move to next state. */
                if (ps->smatch[ps->sindex] == '\0') {
                    state = ps->nextstate;
                    ps->nextstate = PREF_PARSE_INIT; /* reset next state */
                }
                /* else wait for next char */
            }
            else {
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;

        /* quoted string parsing */
        case PREF_PARSE_QUOTED_STRING:
            /* we assume that the initial quote has already been consumed */
            if (ps->lbcur == ps->lbend && !pref_GrowBuf(ps))
                return PR_FALSE; /* out of memory */
            if (c == '\\')
                state = PREF_PARSE_ESC_SEQUENCE;
            else if (c == ps->quotechar) {
                *ps->lbcur++ = '\0';
                state = ps->nextstate;
                ps->nextstate = PREF_PARSE_INIT; /* reset next state */
            }
            else
                *ps->lbcur++ = c;
            break;

        /* name parsing */
        case PREF_PARSE_UNTIL_NAME:
            if (c == '\"' || c == '\'') {
                ps->fdefault = (ps->smatch == kPref);
                ps->quotechar = c;
                ps->nextstate = PREF_PARSE_UNTIL_COMMA; /* return here when done */
                state = PREF_PARSE_QUOTED_STRING;
            }
            else if (c == '/') {       /* allow embedded comment */
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;

        /* parse until we find a comma separating name and value */
        case PREF_PARSE_UNTIL_COMMA:
            if (c == ',') {
                ps->vb = ps->lbcur;
                state = PREF_PARSE_UNTIL_VALUE;
            }
            else if (c == '/') {       /* allow embedded comment */
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;

        /* value parsing */
        case PREF_PARSE_UNTIL_VALUE:
            /* the pref value type is unknown.  so, we scan for the first
             * character of the value, and determine the type from that. */
            if (c == '\"' || c == '\'') {
                ps->vtype = PREF_STRING;
                ps->quotechar = c;
                ps->nextstate = PREF_PARSE_UNTIL_CLOSE_PAREN;
                state = PREF_PARSE_QUOTED_STRING;
            }
            else if (c == 't' || c == 'f') {
                ps->vb = (char *) (c == 't' ? kTrue : kFalse);
                ps->vtype = PREF_BOOL;
                ps->smatch = ps->vb;
                ps->sindex = 1;
                ps->nextstate = PREF_PARSE_UNTIL_CLOSE_PAREN;
                state = PREF_PARSE_MATCH_STRING;
            }
            else if (isdigit(c) || (c == '-') || (c == '+')) {
                ps->vtype = PREF_INT;
                /* write c to line buffer... */
                if (ps->lbcur == ps->lbend && !pref_GrowBuf(ps))
                    return PR_FALSE; /* out of memory */
                *ps->lbcur++ = c;
                state = PREF_PARSE_INT_VALUE;
            }
            else if (c == '/') {       /* allow embedded comment */
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;
        case PREF_PARSE_INT_VALUE:
            /* grow line buffer if necessary... */
            if (ps->lbcur == ps->lbend && !pref_GrowBuf(ps))
                return PR_FALSE; /* out of memory */
            if (isdigit(c))
                *ps->lbcur++ = c;
            else {
                *ps->lbcur++ = '\0'; /* stomp null terminator; we are done. */
                if (c == ')')
                    state = PREF_PARSE_UNTIL_SEMICOLON;
                else if (c == '/') { /* allow embedded comment */
                    ps->nextstate = PREF_PARSE_UNTIL_CLOSE_PAREN;
                    state = PREF_PARSE_COMMENT_MAYBE_START;
                }
                else if (isspace(c))
                    state = PREF_PARSE_UNTIL_CLOSE_PAREN;
                else {
                    NS_WARNING("malformed pref file");
                    return PR_FALSE;
                }
            }
            break;

        /* comment parsing */
        case PREF_PARSE_COMMENT_MAYBE_START:
            switch (c) {
            case '*': /* comment block */
                state = PREF_PARSE_COMMENT_BLOCK;
                break;
            case '/': /* comment line */
                state = PREF_PARSE_UNTIL_EOL;
                break;
            default:
                /* pref file is malformed */
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;
        case PREF_PARSE_COMMENT_BLOCK:
            if (c == '*') 
                state = PREF_PARSE_COMMENT_BLOCK_MAYBE_END;
            break;
        case PREF_PARSE_COMMENT_BLOCK_MAYBE_END:
            switch (c) {
            case '/':
                state = ps->nextstate;
                ps->nextstate = PREF_PARSE_INIT;
                break;
            case '*':       /* stay in this state */
                break;
            default:
                state = PREF_PARSE_COMMENT_BLOCK;
            }
            break;

        /* string escape sequence parsing */
        case PREF_PARSE_ESC_SEQUENCE:
            /* not necessary to resize buffer here since we should be writing
             * only one character and the resize check would have been done
             * for us in the previous state */
            switch (c) {
            case '\"':
            case '\\':
                break;
            case 'r':
                c = '\r';
                break;
            case 'n':
                c = '\n';
                break;
            default:
                NS_WARNING("preserving unexpected JS escape sequence");
                /* grow line buffer if necessary... */
                if (ps->lbcur == ps->lbend && !pref_GrowBuf(ps))
                    return PR_FALSE; /* out of memory */
                *ps->lbcur++ = '\\'; /* preserve the escape sequence */
                break;
            }
            *ps->lbcur++ = c;
            state = PREF_PARSE_QUOTED_STRING;
            break;

        /* function open and close parsing */
        case PREF_PARSE_UNTIL_OPEN_PAREN:
            /* tolerate only whitespace and embedded comments */
            if (c == '(')
                state = PREF_PARSE_UNTIL_NAME;
            else if (c == '/') {
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;
        case PREF_PARSE_UNTIL_CLOSE_PAREN:
            /* tolerate only whitespace and embedded comments  */
            if (c == ')')
                state = PREF_PARSE_UNTIL_SEMICOLON;
            else if (c == '/') {
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;

        /* function terminator ';' parsing */
        case PREF_PARSE_UNTIL_SEMICOLON:
            /* tolerate only whitespace and embedded comments */
            if (c == ';') {
                if (!pref_DoCallback(ps))
                    return PR_FALSE;
                state = PREF_PARSE_INIT;
            }
            else if (c == '/') {
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                NS_WARNING("malformed pref file");
                return PR_FALSE;
            }
            break;

        /* eol parsing */
        case PREF_PARSE_UNTIL_EOL:
            /* need to handle mac, unix, or dos line endings.
             * PREF_PARSE_INIT will eat the next \n in case
             * we have \r\n. */
            if (c == '\r' || c == '\n' || c == 0x1A) {
                state = ps->nextstate;
                ps->nextstate = PREF_PARSE_INIT; /* reset next state */
            }
            break;
        }
    }
    ps->state = state;
    return PR_TRUE;
}

#ifdef TEST_PREFREAD

static void
pref_reader(void       *closure, 
            const char *pref,
            PrefValue   val,
            PrefType    type,
            PRBool      defPref)
{
    printf("%spref(\"%s\", ", defPref ? "" : "user_", pref);
    switch (type) {
    case PREF_STRING:
        printf("\"%s\");\n", val.stringVal);
        break;
    case PREF_INT:
        printf("%i);\n", val.intVal);
        break;
    case PREF_BOOL:
        printf("%s);\n", val.boolVal == PR_FALSE ? "false" : "true");
        break;
    }
}

int
main(int argc, char **argv)
{
    PrefParseState ps;
    char buf[4096];     /* i/o buffer */
    FILE *fp;
    int n;

    if (argc == 1) {
        printf("usage: prefread file.js\n");
        return -1;
    }

    fp = fopen(argv[1], "r");
    if (!fp) {
        printf("failed to open file\n");
        return -1;
    }

    PREF_InitParseState(&ps, pref_reader, NULL);

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        PREF_ParseBuf(&ps, buf, n);

    PREF_FinalizeParseState(&ps);

    fclose(fp);
    return 0;
}

#endif /* TEST_PREFREAD */
