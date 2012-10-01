/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* 
 * shexp.c: shell-like wildcard match routines
 *
 * See shexp.h for public documentation.
 */

#include "seccomon.h"
#include "portreg.h"

/* ----------------------------- shexp_valid ------------------------------ */


static int 
_valid_subexp(const char *exp, char stop1, char stop2) 
{
    register int x;
    int nsc = 0;     /* Number of special characters */
    int np;          /* Number of pipe characters in union */
    int tld = 0;     /* Number of tilde characters */

    for (x = 0; exp[x] && (exp[x] != stop1) && (exp[x] != stop2); ++x) {
        switch(exp[x]) {
	case '~':
            if(tld)                 /* at most one exclusion */
                return INVALID_SXP;
            if (stop1)              /* no exclusions within unions */
                return INVALID_SXP;
            if (!exp[x+1])          /* exclusion cannot be last character */
                return INVALID_SXP;
            if (!x)                 /* exclusion cannot be first character */
                return INVALID_SXP;
            ++tld;
	    /* fall through */
	case '*':
	case '?':
	case '$':
            ++nsc;
            break;
	case '[':
            ++nsc;
            if((!exp[++x]) || (exp[x] == ']'))
                return INVALID_SXP;
            for(; exp[x] && (exp[x] != ']'); ++x) {
                if(exp[x] == '\\' && !exp[++x])
                    return INVALID_SXP;
            }
            if(!exp[x])
                return INVALID_SXP;
            break;
	case '(':
            ++nsc;
	    if (stop1)			/* no nested unions */
		return INVALID_SXP;
            np = -1;
            do {
                int t = _valid_subexp(&exp[++x], ')', '|');
                if(t == 0 || t == INVALID_SXP)
                    return INVALID_SXP;
                x+=t;
		if(!exp[x])
		    return INVALID_SXP;
		++np;
            } while (exp[x] == '|' );
	    if(np < 1)  /* must be at least one pipe */
		return INVALID_SXP;
            break;
	case ')':
	case '|':
	case ']':
            return INVALID_SXP;
	case '\\':
	    ++nsc;
            if(!exp[++x])
                return INVALID_SXP;
            break;
	default:
            break;
        }
    }
    if((!stop1) && (!nsc)) /* must be at least one special character */
        return NON_SXP;
    return ((exp[x] == stop1 || exp[x] == stop2) ? x : INVALID_SXP);
}

int 
PORT_RegExpValid(const char *exp) 
{
    int x;

    x = _valid_subexp(exp, '\0', '\0');
    return (x < 0 ? x : VALID_SXP);
}


/* ----------------------------- shexp_match ----------------------------- */


#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

static int 
_shexp_match(const char *str, const char *exp, PRBool case_insensitive,
             unsigned int level);

/* Count characters until we reach a NUL character or either of the 
 * two delimiter characters, stop1 or stop2.  If we encounter a bracketed 
 * expression, look only for NUL or ']' inside it.  Do not look for stop1 
 * or stop2 inside it. Return ABORTED if bracketed expression is unterminated.
 * Handle all escaping.
 * Return index in input string of first stop found, or ABORTED if not found.
 * If "dest" is non-NULL, copy counted characters to it and NUL terminate.
 */
static int 
_scan_and_copy(const char *exp, char stop1, char stop2, char *dest)
{
    register int sx;     /* source index */
    register char cc;

    for (sx = 0; (cc = exp[sx]) && cc != stop1 && cc != stop2; sx++) {
	if (cc == '\\') {
	    if (!exp[++sx])
		return ABORTED; /* should be impossible */
	} else if (cc == '[') {
	    while ((cc = exp[++sx]) && cc != ']') {
		if(cc == '\\' && !exp[++sx])
		    return ABORTED;
	    }
	    if (!cc) 
		return ABORTED; /* should be impossible */
	}
    }
    if (dest && sx) {
	/* Copy all but the closing delimiter. */
	memcpy(dest, exp, sx);
	dest[sx] = 0;
    }
    return cc ? sx : ABORTED; /* index of closing delimiter */
}

/* On input, exp[0] is the opening parenthesis of a union.
 * See if any of the alternatives in the union matches as a pattern.
 * The strategy is to take each of the alternatives, in turn, and append
 * the rest of the expression (after the closing ')' that marks the end of 
 * this union) to that alternative, and then see if the resultant expression
 * matches the input string.  Repeat this until some alternative matches, 
 * or we have an abort. 
 */
static int 
_handle_union(const char *str, const char *exp, PRBool case_insensitive,
              unsigned int level) 
{
    register int sx;     /* source index */
    int cp;              /* source index of closing parenthesis */
    int count;
    int ret   = NOMATCH;
    char *e2;

    /* Find the closing parenthesis that ends this union in the expression */
    cp = _scan_and_copy(exp, ')', '\0', NULL);
    if (cp == ABORTED || cp < 4) /* must be at least "(a|b" before ')' */
    	return ABORTED;
    ++cp;                /* now index of char after closing parenthesis */
    e2 = (char *) PORT_Alloc(1 + strlen(exp));
    if (!e2)
    	return ABORTED;
    for (sx = 1; ; ++sx) {
	/* Here, exp[sx] is one character past the preceding '(' or '|'. */
	/* Copy everything up to the next delimiter to e2 */
	count = _scan_and_copy(exp + sx, ')', '|', e2);
	if (count == ABORTED || !count) {
	    ret = ABORTED;
	    break;
	}
	sx += count;
	/* Append everything after closing parenthesis to e2. This is safe. */
	strcpy(e2+count, exp+cp);
        ret = _shexp_match(str, e2, case_insensitive, level + 1);
	if (ret != NOMATCH || !exp[sx] || exp[sx] == ')')
            break;
    }
    PORT_Free(e2);
    if (sx < 2)
    	ret = ABORTED;
    return ret;
}

/* returns 1 if val is in range from start..end, case insensitive. */
static int
_is_char_in_range(int start, int end, int val)
{
    char map[256];
    memset(map, 0, sizeof map);
    while (start <= end)
	map[tolower(start++)] = 1;
    return map[tolower(val)];
}

static int 
_shexp_match(const char *str, const char *exp, PRBool case_insensitive, 
             unsigned int level) 
{
    register int x;   /* input string index */
    register int y;   /* expression index */
    int ret,neg;

    if (level > 20)      /* Don't let the stack get too deep. */
    	return ABORTED;
    for(x = 0, y = 0; exp[y]; ++y, ++x) {
        if((!str[x]) && (exp[y] != '$') && (exp[y] != '*')) {
            return NOMATCH;
	}
	switch(exp[y]) {
	case '$':
	    if(str[x])
		return NOMATCH;
	    --x;                 /* we don't want loop to increment x */
	    break;
	case '*':
	    while(exp[++y] == '*'){}
	    if(!exp[y])
		return MATCH;
	    while(str[x]) {
	        ret = _shexp_match(&str[x++], &exp[y], case_insensitive, 
				   level + 1);
		switch(ret) {
		case NOMATCH:
		    continue;
		case ABORTED:
		    return ABORTED;
		default:
		    return MATCH;
		}
	    }
	    if((exp[y] == '$') && (exp[y+1] == '\0') && (!str[x]))
		return MATCH;
	    else
		return NOMATCH;
	case '[': {
	    int start, end = 0, i;
	    neg = ((exp[++y] == '^') && (exp[y+1] != ']'));
	    if (neg)
		++y;
	    i = y;
	    start = (unsigned char)(exp[i++]);
	    if (start == '\\')
	    	start = (unsigned char)(exp[i++]);
	    if (isalnum(start) && exp[i++] == '-') {
		end = (unsigned char)(exp[i++]); 
		if (end == '\\')
		    end = (unsigned char)(exp[i++]);
	    }
	    if (isalnum(end) && exp[i] == ']') {
		/* This is a range form: a-b */
		int val   = (unsigned char)(str[x]);
		if (end < start) { /* swap them */
		    start ^= end;
		    end ^= start;
		    start ^= end;
		}
		if (case_insensitive && isalpha(val)) {
		    val = _is_char_in_range(start, end, val);
		    if (neg == val)
			return NOMATCH;
		} else if (neg != ((val < start) || (val > end))) {
		    return NOMATCH;
		}
		y = i;
	    } else {
		/* Not range form */
		int matched = 0;
		for (; exp[y] != ']'; y++) {
		    if (exp[y] == '\\')
			++y;
		    if(case_insensitive) {
			matched |= (toupper(str[x]) == toupper(exp[y]));
		    } else {
			matched |= (str[x] == exp[y]);
		    }
		}
		if (neg == matched)
		    return NOMATCH;
	    }
	}
	break;
	case '(':
	    if (!exp[y+1])
	    	return ABORTED;
	    return _handle_union(&str[x], &exp[y], case_insensitive, level);
	case '?':
	    break;
	case '|':
	case ']':
	case ')':
	    return ABORTED;
	case '\\':
	    ++y;
	    /* fall through */
	default:
	    if(case_insensitive) {
		if(toupper(str[x]) != toupper(exp[y]))
		    return NOMATCH;
	    } else {
		if(str[x] != exp[y])
		    return NOMATCH;
	    }
	    break;
	}
    }
    return (str[x] ? NOMATCH : MATCH);
}

static int 
port_RegExpMatch(const char *str, const char *xp, PRBool case_insensitive) 
{
    char *exp = 0;
    int x, ret = MATCH;

    if (!strchr(xp, '~'))
    	return _shexp_match(str, xp, case_insensitive, 0);

    exp = PORT_Strdup(xp);
    if(!exp)
	return NOMATCH;

    x = _scan_and_copy(exp, '~', '\0', NULL);
    if (x != ABORTED && exp[x] == '~') {
	exp[x++] = '\0';
	ret = _shexp_match(str, &exp[x], case_insensitive, 0);
	switch (ret) {
	case NOMATCH: ret = MATCH;   break;
	case MATCH:   ret = NOMATCH; break;
	default:                     break;
        }
    }
    if (ret == MATCH)
	ret = _shexp_match(str, exp, case_insensitive, 0);

    PORT_Free(exp);
    return ret;
}


/* ------------------------------ shexp_cmp ------------------------------- */

int 
PORT_RegExpSearch(const char *str, const char *exp)
{
    switch(PORT_RegExpValid(exp)) 
	  {
        case INVALID_SXP:
            return -1;
        case NON_SXP:
            return (strcmp(exp,str) ? 1 : 0);
        default:
            return port_RegExpMatch(str, exp, PR_FALSE);
      }
}

int
PORT_RegExpCaseSearch(const char *str, const char *exp)
{
    switch(PORT_RegExpValid(exp))
      {
        case INVALID_SXP:
            return -1;
        case NON_SXP:
            return (PORT_Strcasecmp(exp,str) ? 1 : 0);
        default:
            return port_RegExpMatch(str, exp, PR_TRUE);
      }
}

