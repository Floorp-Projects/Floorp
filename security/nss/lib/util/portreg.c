/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* 
 * shexp.c: shell-like wildcard match routines
 *
 *
 * See shexp.h for public documentation.
 *
 */

#include "seccomon.h"
#include "portreg.h"

/* ----------------------------- shexp_valid ------------------------------ */


static int 
_valid_subexp(const char *exp, char stop) 
{
    register int x,y,t;
    int nsc,np,tld;

    x=0;nsc=0;tld=0;

    while(exp[x] && (exp[x] != stop)) {
        switch(exp[x]) {
          case '~':
            if(tld) return INVALID_SXP;
            else ++tld;
          case '*':
          case '?':
          case '^':
          case '$':
            ++nsc;
            break;
          case '[':
            ++nsc;
            if((!exp[++x]) || (exp[x] == ']'))
                return INVALID_SXP;
            for(++x;exp[x] && (exp[x] != ']');++x)
                if(exp[x] == '\\')
                    if(!exp[++x])
                        return INVALID_SXP;
            if(!exp[x])
                return INVALID_SXP;
            break;
          case '(':
            ++nsc;np = 0;
            while(1) {
                if(exp[++x] == ')')
                    return INVALID_SXP;
                for(y=x;(exp[y]) && (exp[y] != '|') && (exp[y] != ')');++y)
                    if(exp[y] == '\\')
                        if(!exp[++y])
                            return INVALID_SXP;
                if(!exp[y])
                    return INVALID_SXP;
                if(exp[y] == '|')
                    ++np;
                t = _valid_subexp(&exp[x],exp[y]);
                if(t == INVALID_SXP)
                    return INVALID_SXP;
                x+=t;
                if(exp[x] == ')') {
                    if(!np)
                        return INVALID_SXP;
                    break;
                }
            }
            break;
          case ')':
          case ']':
            return INVALID_SXP;
          case '\\':
            if(!exp[++x])
                return INVALID_SXP;
          default:
            break;
        }
        ++x;
    }
    if((!stop) && (!nsc))
        return NON_SXP;
    return ((exp[x] == stop) ? x : INVALID_SXP);
}

int 
PORT_RegExpValid(const char *exp) 
{
    int x;

    x = _valid_subexp(exp, '\0');
    return (x < 0 ? x : VALID_SXP);
}


/* ----------------------------- shexp_match ----------------------------- */


#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

static int _shexp_match(const char *str, const char *exp, PRBool case_insensitive);

static int 
_handle_union(const char *str, const char *exp, PRBool case_insensitive) 
{
    char *e2 = (char *) PORT_Alloc(sizeof(char)*strlen(exp));
    register int t,p2,p1 = 1;
    int cp;

    while(1) {
        for(cp=1;exp[cp] != ')';cp++)
            if(exp[cp] == '\\')
                ++cp;
        for(p2 = 0;(exp[p1] != '|') && (p1 != cp);p1++,p2++) {
            if(exp[p1] == '\\')
                e2[p2++] = exp[p1++];
            e2[p2] = exp[p1];
        }
        for (t=cp+1; ((e2[p2] = exp[t]) != 0); ++t,++p2) {}
        if(_shexp_match(str,e2, case_insensitive) == MATCH) {
            PORT_Free(e2);
            return MATCH;
        }
        if(p1 == cp) {
            PORT_Free(e2);
            return NOMATCH;
        }
        else ++p1;
    }
}


static int 
_shexp_match(const char *str, const char *exp, PRBool case_insensitive) 
{
    register int x,y;
    int ret,neg;

    ret = 0;
    for(x=0,y=0;exp[y];++y,++x) {
        if((!str[x]) && (exp[y] != '(') && (exp[y] != '$') && (exp[y] != '*'))
            ret = ABORTED;
        else {
            switch(exp[y]) {
              case '$':
                if( (str[x]) )
                    ret = NOMATCH;
                else
                    --x;             /* we don't want loop to increment x */
                break;
              case '*':
                while(exp[++y] == '*'){}
                if(!exp[y])
                    return MATCH;
                while(str[x]) {
                    switch(_shexp_match(&str[x++],&exp[y], case_insensitive)) {
                    case NOMATCH:
                        continue;
                    case ABORTED:
                        ret = ABORTED;
                        break;
                    default:
                        return MATCH;
                    }
                    break;
                }
                if((exp[y] == '$') && (exp[y+1] == '\0') && (!str[x]))
                    return MATCH;
                else
                    ret = ABORTED;
                break;
              case '[':
              	neg = ((exp[++y] == '^') && (exp[y+1] != ']'));
                if (neg)
                    ++y;
                
                if ((isalnum(exp[y])) && (exp[y+1] == '-') && 
                   (isalnum(exp[y+2])) && (exp[y+3] == ']'))
                    {
                        int start = exp[y], end = exp[y+2];
                        
                        /* no safeguards here */
                        if(neg ^ ((str[x] < start) || (str[x] > end))) {
                            ret = NOMATCH;
                            break;
                        }
                        y+=3;
                    }
                else {
                    int matched;
                    
                    for (matched=0;exp[y] != ']';y++)
                        matched |= (str[x] == exp[y]);
                    if (neg ^ (!matched))
                        ret = NOMATCH;
                }
                break;
              case '(':
                return _handle_union(&str[x],&exp[y], case_insensitive);
                break;
              case '?':
                break;
              case '\\':
                ++y;
              default:
		if(case_insensitive)
		  {
                    if(toupper(str[x]) != toupper(exp[y]))
                        ret = NOMATCH;
		  }
		else
		  {
                    if(str[x] != exp[y])
                        ret = NOMATCH;
		  }
                break;
            }
        }
        if(ret)
            break;
    }
    return (ret ? ret : (str[x] ? NOMATCH : MATCH));
}

static int 
port_RegExpMatch(const char *str, const char *xp, PRBool case_insensitive) {
    register int x;
    char *exp = 0;

    exp = PORT_Strdup(xp);

    if(!exp)
	return 1;

    for(x=strlen(exp)-1;x;--x) {
        if((exp[x] == '~') && (exp[x-1] != '\\')) {
            exp[x] = '\0';
            if(_shexp_match(str,&exp[++x], case_insensitive) == MATCH)
                goto punt;
            break;
        }
    }
    if(_shexp_match(str,exp, PR_FALSE) == MATCH) {
        PORT_Free(exp);
        return 0;
    }

  punt:
    PORT_Free(exp);
    return 1;
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
            return (strcmp(exp,str) ? 1 : 0);
        default:
            return port_RegExpMatch(str, exp, PR_TRUE);
      }
}

