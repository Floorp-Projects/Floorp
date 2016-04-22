/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "prefread.h"
#include "nsString.h"
#include "nsUTF8Utils.h"

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
    PREF_PARSE_HEX_ESCAPE,
    PREF_PARSE_UTF16_LOW_SURROGATE,
    PREF_PARSE_UNTIL_OPEN_PAREN,
    PREF_PARSE_UNTIL_CLOSE_PAREN,
    PREF_PARSE_UNTIL_SEMICOLON,
    PREF_PARSE_UNTIL_EOL
};

#define UTF16_ESC_NUM_DIGITS    4
#define HEX_ESC_NUM_DIGITS      2
#define BITS_PER_HEX_DIGIT      4

static const char kUserPref[] = "user_pref";
static const char kPref[] = "pref";
static const char kPrefSticky[] = "sticky_pref";
static const char kTrue[] = "true";
static const char kFalse[] = "false";

/**
 * pref_GrowBuf
 * 
 * this function will increase the size of the buffer owned
 * by the given pref parse state.  We currently use a simple
 * doubling algorithm, but the only hard requirement is that
 * it increase the buffer by at least the size of the ps->esctmp
 * buffer used for escape processing (currently 6 bytes).
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
 * @return false if insufficient memory.
 */
static bool
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
        return false;

    ps->lbcur = ps->lb + curPos;
    ps->lbend = ps->lb + bufLen;
    ps->vb    = ps->lb + valPos;

    return true;
}

/**
 * Report an error or a warning.  If not specified, just dump to stderr.
 */
static void
pref_ReportParseProblem(PrefParseState& ps, const char* aMessage, int aLine, bool aError)
{
    if (ps.reporter) {
        ps.reporter(aMessage, aLine, aError);
    } else {
        printf_stderr("**** Preference parsing %s (line %d) = %s **\n",
                      (aError ? "error" : "warning"), aLine, aMessage);
    }
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
 * @return false to indicate a fatal error.
 */
static bool
pref_DoCallback(PrefParseState *ps)
{
    PrefValue  value;

    switch (ps->vtype) {
    case PrefType::String:
        value.stringVal = ps->vb;
        break;
    case PrefType::Int:
        if ((ps->vb[0] == '-' || ps->vb[0] == '+') && ps->vb[1] == '\0') {
            pref_ReportParseProblem(*ps, "invalid integer value", 0, true);
            NS_WARNING("malformed integer value");
            return false;
        }
        value.intVal = atoi(ps->vb);
        break;
    case PrefType::Bool:
        value.boolVal = (ps->vb == kTrue);
        break;
    default:
        break;
    }
    (*ps->reader)(ps->closure, ps->lb, value, ps->vtype, ps->fdefault,
                  ps->fstickydefault);
    return true;
}

void
PREF_InitParseState(PrefParseState *ps, PrefReader reader,
                    PrefParseErrorReporter reporter, void *closure)
{
    memset(ps, 0, sizeof(*ps));
    ps->reader = reader;
    ps->closure = closure;
    ps->reporter = reporter;
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
 * function-name = "user_pref" | "pref" | "sticky_pref"
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
bool
PREF_ParseBuf(PrefParseState *ps, const char *buf, int bufLen)
{
    const char *end;
    char c;
    char udigit;
    int state;

    // The line number is currently only used for the error/warning reporting.
    int lineNum = 0;

    state = ps->state;
    for (end = buf + bufLen; buf != end; ++buf) {
        c = *buf;
        if (c == '\r' || c == '\n' || c == 0x1A) {
            lineNum ++;
        }

        switch (state) {
        /* initial state */
        case PREF_PARSE_INIT:
            if (ps->lbcur != ps->lb) { /* reset state */
                ps->lbcur = ps->lb;
                ps->vb    = nullptr;
                ps->vtype = PrefType::Invalid;
                ps->fdefault = false;
                ps->fstickydefault = false;
            }
            switch (c) {
            case '/':       /* begin comment block or line? */
                state = PREF_PARSE_COMMENT_MAYBE_START;
                break;
            case '#':       /* accept shell style comments */
                state = PREF_PARSE_UNTIL_EOL;
                break;
            case 'u':       /* indicating user_pref */
            case 's':       /* indicating sticky_pref */
            case 'p':       /* indicating pref */
                if (c == 'u') {
                  ps->smatch = kUserPref;
                } else if (c == 's') {
                  ps->smatch = kPrefSticky;
                } else {
                  ps->smatch = kPref;
                }
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
                pref_ReportParseProblem(*ps, "non-matching string", lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
            }
            break;

        /* quoted string parsing */
        case PREF_PARSE_QUOTED_STRING:
            /* we assume that the initial quote has already been consumed */
            if (ps->lbcur == ps->lbend && !pref_GrowBuf(ps))
                return false; /* out of memory */
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
                ps->fdefault = (ps->smatch == kPref || ps->smatch == kPrefSticky);
                ps->fstickydefault = (ps->smatch == kPrefSticky);
                ps->quotechar = c;
                ps->nextstate = PREF_PARSE_UNTIL_COMMA; /* return here when done */
                state = PREF_PARSE_QUOTED_STRING;
            }
            else if (c == '/') {       /* allow embedded comment */
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                pref_ReportParseProblem(*ps, "need space, comment or quote", lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
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
                pref_ReportParseProblem(*ps, "need space, comment or comma", lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
            }
            break;

        /* value parsing */
        case PREF_PARSE_UNTIL_VALUE:
            /* the pref value type is unknown.  so, we scan for the first
             * character of the value, and determine the type from that. */
            if (c == '\"' || c == '\'') {
                ps->vtype = PrefType::String;
                ps->quotechar = c;
                ps->nextstate = PREF_PARSE_UNTIL_CLOSE_PAREN;
                state = PREF_PARSE_QUOTED_STRING;
            }
            else if (c == 't' || c == 'f') {
                ps->vb = (char *) (c == 't' ? kTrue : kFalse);
                ps->vtype = PrefType::Bool;
                ps->smatch = ps->vb;
                ps->sindex = 1;
                ps->nextstate = PREF_PARSE_UNTIL_CLOSE_PAREN;
                state = PREF_PARSE_MATCH_STRING;
            }
            else if (isdigit(c) || (c == '-') || (c == '+')) {
                ps->vtype = PrefType::Int;
                /* write c to line buffer... */
                if (ps->lbcur == ps->lbend && !pref_GrowBuf(ps))
                    return false; /* out of memory */
                *ps->lbcur++ = c;
                state = PREF_PARSE_INT_VALUE;
            }
            else if (c == '/') {       /* allow embedded comment */
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                pref_ReportParseProblem(*ps, "need value, comment or space", lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
            }
            break;
        case PREF_PARSE_INT_VALUE:
            /* grow line buffer if necessary... */
            if (ps->lbcur == ps->lbend && !pref_GrowBuf(ps))
                return false; /* out of memory */
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
                    pref_ReportParseProblem(*ps, "while parsing integer", lineNum, true);
                    NS_WARNING("malformed pref file");
                    return false;
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
                pref_ReportParseProblem(*ps, "while parsing comment", lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
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
            case '\'':
            case '\\':
                break;
            case 'r':
                c = '\r';
                break;
            case 'n':
                c = '\n';
                break;
            case 'x': /* hex escape -- always interpreted as Latin-1 */
            case 'u': /* UTF16 escape */
                ps->esctmp[0] = c;
                ps->esclen = 1;
                ps->utf16[0] = ps->utf16[1] = 0;
                ps->sindex = (c == 'x' ) ?
                                HEX_ESC_NUM_DIGITS :
                                UTF16_ESC_NUM_DIGITS;
                state = PREF_PARSE_HEX_ESCAPE;
                continue;
            default:
                pref_ReportParseProblem(*ps, "preserving unexpected JS escape sequence",
                                        lineNum, false);
                NS_WARNING("preserving unexpected JS escape sequence");
                /* Invalid escape sequence so we do have to write more than
                 * one character. Grow line buffer if necessary... */
                if ((ps->lbcur+1) == ps->lbend && !pref_GrowBuf(ps))
                    return false; /* out of memory */
                *ps->lbcur++ = '\\'; /* preserve the escape sequence */
                break;
            }
            *ps->lbcur++ = c;
            state = PREF_PARSE_QUOTED_STRING;
            break;

        /* parsing a hex (\xHH) or utf16 escape (\uHHHH) */
        case PREF_PARSE_HEX_ESCAPE:
            if ( c >= '0' && c <= '9' )
                udigit = (c - '0');
            else if ( c >= 'A' && c <= 'F' )
                udigit = (c - 'A') + 10;
            else if ( c >= 'a' && c <= 'f' )
                udigit = (c - 'a') + 10;
            else {
                /* bad escape sequence found, write out broken escape as-is */
                pref_ReportParseProblem(*ps, "preserving invalid or incomplete hex escape",
                                        lineNum, false);
                NS_WARNING("preserving invalid or incomplete hex escape");
                *ps->lbcur++ = '\\';  /* original escape slash */
                if ((ps->lbcur + ps->esclen) >= ps->lbend && !pref_GrowBuf(ps))
                    return false;
                for (int i = 0; i < ps->esclen; ++i)
                    *ps->lbcur++ = ps->esctmp[i];

                /* push the non-hex character back for re-parsing. */
                /* (++buf at the top of the loop keeps this safe)  */
                --buf;
                state = PREF_PARSE_QUOTED_STRING;
                continue;
            }

            /* have a digit */
            ps->esctmp[ps->esclen++] = c; /* preserve it */
            ps->utf16[1] <<= BITS_PER_HEX_DIGIT;
            ps->utf16[1] |= udigit;
            ps->sindex--;
            if (ps->sindex == 0) {
                /* have the full escape. Convert to UTF8 */
                int utf16len = 0;
                if (ps->utf16[0]) {
                    /* already have a high surrogate, this is a two char seq */
                    utf16len = 2;
                }
                else if (0xD800 == (0xFC00 & ps->utf16[1])) {
                    /* a high surrogate, can't convert until we have the low */
                    ps->utf16[0] = ps->utf16[1];
                    ps->utf16[1] = 0;
                    state = PREF_PARSE_UTF16_LOW_SURROGATE;
                    break;
                }
                else {
                    /* a single utf16 character */
                    ps->utf16[0] = ps->utf16[1];
                    utf16len = 1;
                }

                /* actual conversion */
                /* make sure there's room, 6 bytes is max utf8 len (in */
                /* theory; 4 bytes covers the actual utf16 range) */
                if (ps->lbcur+6 >= ps->lbend && !pref_GrowBuf(ps))
                    return false;

                ConvertUTF16toUTF8 converter(ps->lbcur);
                converter.write(ps->utf16, utf16len);
                ps->lbcur += converter.Size();
                state = PREF_PARSE_QUOTED_STRING;
            }
            break;

        /* looking for beginning of utf16 low surrogate */
        case PREF_PARSE_UTF16_LOW_SURROGATE:
            if (ps->sindex == 0 && c == '\\') {
                ++ps->sindex;
            }
            else if (ps->sindex == 1 && c == 'u') {
                /* escape sequence is correct, now parse hex */
                ps->sindex = UTF16_ESC_NUM_DIGITS;
                ps->esctmp[0] = 'u';
                ps->esclen = 1;
                state = PREF_PARSE_HEX_ESCAPE;
            }
            else {
                /* didn't find expected low surrogate. Ignore high surrogate
                 * (it would just get converted to nothing anyway) and start
                 * over with this character */
                 --buf;
                 if (ps->sindex == 1)
                     state = PREF_PARSE_ESC_SEQUENCE;
                 else
                     state = PREF_PARSE_QUOTED_STRING;
                 continue;
            }
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
                pref_ReportParseProblem(*ps, "need space, comment or open parentheses",
                                        lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
            }
            break;
        case PREF_PARSE_UNTIL_CLOSE_PAREN:
            /* tolerate only whitespace and embedded comments  */
            if (c == ')') {
                state = PREF_PARSE_UNTIL_SEMICOLON;
            } else if (c == '/') {
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            } else if (!isspace(c)) {
                pref_ReportParseProblem(*ps, "need space, comment or closing parentheses",
                                        lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
            }
            break;

        /* function terminator ';' parsing */
        case PREF_PARSE_UNTIL_SEMICOLON:
            /* tolerate only whitespace and embedded comments */
            if (c == ';') {
                if (!pref_DoCallback(ps))
                    return false;
                state = PREF_PARSE_INIT;
            }
            else if (c == '/') {
                ps->nextstate = state; /* return here when done with comment */
                state = PREF_PARSE_COMMENT_MAYBE_START;
            }
            else if (!isspace(c)) {
                pref_ReportParseProblem(*ps, "need space, comment or semicolon",
                                        lineNum, true);
                NS_WARNING("malformed pref file");
                return false;
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
    return true;
}

#ifdef TEST_PREFREAD

static void
pref_reader(void       *closure, 
            const char *pref,
            PrefValue   val,
            PrefType    type,
            bool        defPref)
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
        printf("%s);\n", val.boolVal == false ? "false" : "true");
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

    PREF_InitParseState(&ps, pref_reader, nullptr, nullptr);

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        PREF_ParseBuf(&ps, buf, n);

    PREF_FinalizeParseState(&ps);

    fclose(fp);
    return 0;
}

#endif /* TEST_PREFREAD */
