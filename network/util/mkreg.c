/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* *
 * 
 *
 * shexp.c: shell-like wildcard match routines
 *
 * See shexp.h for public documentation.
 *
 * Rob McCool
 * 
 */

#include "mkreg.h"
#include "plstr.h"
#include "prmem.h"

/* ----------------------------- shexp_valid ------------------------------ */


PRIVATE int 
_valid_subexp(char *exp, char stop) 
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

PUBLIC int 
NET_RegExpValid(char *exp) 
{
    int x;

    x = _valid_subexp(exp, '\0');
    return (x < 0 ? x : VALID_SXP);
}


/* ----------------------------- shexp_match ----------------------------- */


#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

PRIVATE int _shexp_match(char *str, char *exp, Bool case_insensitive);

PRIVATE int 
_handle_union(char *str, char *exp, Bool case_insensitive) 
{
    char *e2 = (char *) PR_Malloc(sizeof(char)*strlen(exp));
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
            PR_Free(e2);
            return MATCH;
        }
        if(p1 == cp) {
            PR_Free(e2);
            return NOMATCH;
        }
        else ++p1;
    }
}


PRIVATE int 
_shexp_match(char *str, char *exp, Bool case_insensitive) 
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
                        
                        /* Droolproofing for pinheads not included */
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

PUBLIC int 
NET_RegExpMatch(char *str, char *xp, Bool case_insensitive) {
    register int x;
    char *exp = PL_strdup(xp);

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
    if(_shexp_match(str,exp, FALSE) == MATCH) {
        PR_Free(exp);
        return 0;
    }

  punt:
    PR_Free(exp);
    return 1;
}


/* ------------------------------ shexp_cmp ------------------------------- */

PUBLIC int 
NET_RegExpSearch(char *str, char *exp)
{
    switch(NET_RegExpValid(exp)) 
	  {
        case INVALID_SXP:
            return -1;
        case NON_SXP:
            return (strcmp(exp,str) ? 1 : 0);
        default:
            return NET_RegExpMatch(str, exp, FALSE);
      }
}

PUBLIC int
NET_RegExpCaseSearch(char *str, char *exp)
{
    switch(NET_RegExpValid(exp))
      {
        case INVALID_SXP:
            return -1;
        case NON_SXP:
            return (strcmp(exp,str) ? 1 : 0);
        default:
            return NET_RegExpMatch(str, exp, TRUE);
      }
}

