/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "nsWildCard.h"
#include "nsCRT.h"
#include "plstr.h"
#include "prmem.h"

/* ----------------------------- shexp_valid ------------------------------ */


static int 
_valid_subexp(PRUnichar *expr, PRUnichar stop) 
{
    register int x,y,t;
    int nsc,np,tld;

    x=0;nsc=0;tld=0;

    while(expr[x] && (expr[x] != stop)) {
        switch(expr[x]) {
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
            if((!expr[++x]) || (expr[x] == ']'))
                return INVALID_SXP;
            for(++x;expr[x] && (expr[x] != ']');++x)
                if(expr[x] == '\\')
                    if(!expr[++x])
                        return INVALID_SXP;
            if(!expr[x])
                return INVALID_SXP;
            break;
          case '(':
            ++nsc;np = 0;
            while(1) {
                if(expr[++x] == ')')
                    return INVALID_SXP;
                for(y=x;(expr[y]) && (expr[y] != '|') && (expr[y] != ')');++y)
                    if(expr[y] == '\\')
                        if(!expr[++y])
                            return INVALID_SXP;
                if(!expr[y])
                    return INVALID_SXP;
                if(expr[y] == '|')
                    ++np;
                t = _valid_subexp(&expr[x],expr[y]);
                if(t == INVALID_SXP)
                    return INVALID_SXP;
                x+=t;
                if(expr[x] == ')') {
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
            if(!expr[++x])
                return INVALID_SXP;
          default:
            break;
        }
        ++x;
    }
    if((!stop) && (!nsc))
        return NON_SXP;
    return ((expr[x] == stop) ? x : INVALID_SXP);
}

int 
NS_WildCardValid(PRUnichar *expr) 
{
    int x;

    x = _valid_subexp(expr, '\0');
    return (x < 0 ? x : VALID_SXP);
}


/* ----------------------------- shexp_match ----------------------------- */


#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

static int _shexp_match(const PRUnichar *str, const PRUnichar *expr,
                        PRBool case_insensitive);

static int 
_handle_union(const PRUnichar *str, const PRUnichar *expr,
              PRBool case_insensitive) 
{
    PRUnichar *e2 = (PRUnichar *) PR_Malloc(sizeof(PRUnichar)*nsCRT::strlen(expr));
    register int t,p2,p1 = 1;
    int cp;

    while(1) {
        for(cp=1;expr[cp] != ')';cp++)
            if(expr[cp] == '\\')
                ++cp;
        for(p2 = 0;(expr[p1] != '|') && (p1 != cp);p1++,p2++) {
            if(expr[p1] == '\\')
                e2[p2++] = expr[p1++];
            e2[p2] = expr[p1];
        }
        for (t=cp+1; ((e2[p2] = expr[t]) != 0); ++t,++p2) {}
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


static int 
_shexp_match(const PRUnichar *str, const PRUnichar *expr,
             PRBool case_insensitive) 
{
    register int x,y;
    int ret,neg;

    ret = 0;
    for(x=0,y=0;expr[y];++y,++x) {
        if((!str[x]) && (expr[y] != '(') && (expr[y] != '$') && (expr[y] != '*'))
            ret = ABORTED;
        else {
            switch(expr[y]) {
              case '$':
                if( (str[x]) )
                    ret = NOMATCH;
                else
                    --x;             /* we don't want loop to increment x */
                break;
              case '*':
                while(expr[++y] == '*'){}
                if(!expr[y])
                    return MATCH;
                while(str[x]) {
                    switch(_shexp_match(&str[x++],&expr[y], case_insensitive)) {
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
                if((expr[y] == '$') && (expr[y+1] == '\0') && (!str[x]))
                    return MATCH;
                else
                    ret = ABORTED;
                break;
              case '[':
              	neg = ((expr[++y] == '^') && (expr[y+1] != ']'));
                if (neg)
                    ++y;
                
                if ((isalnum(expr[y])) && (expr[y+1] == '-') && 
                   (isalnum(expr[y+2])) && (expr[y+3] == ']'))
                    {
                        int start = expr[y], end = expr[y+2];
                        
                        /* Droolproofing for pinheads not included */
                        if(neg ^ ((str[x] < start) || (str[x] > end))) {
                            ret = NOMATCH;
                            break;
                        }
                        y+=3;
                    }
                else {
                    int matched;
                    
                    for (matched=0;expr[y] != ']';y++)
                        matched |= (str[x] == expr[y]);
                    if (neg ^ (!matched))
                        ret = NOMATCH;
                }
                break;
              case '(':
                return _handle_union(&str[x],&expr[y], case_insensitive);
                break;
              case '?':
                break;
              case '\\':
                ++y;
              default:
				if(case_insensitive)
				  {
                    if(toupper(str[x]) != toupper(expr[y]))
                        ret = NOMATCH;
				  }
				else
				  {
                    if(str[x] != expr[y])
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

int 
NS_WildCardMatch(const PRUnichar *str, const PRUnichar *xp,
                 PRBool case_insensitive) {
    register int x;
    PRUnichar *expr = nsCRT::strdup(xp);

	if(!expr)
		return 1;

    for(x=nsCRT::strlen(expr)-1;x;--x) {
        if((expr[x] == '~') && (expr[x-1] != '\\')) {
            expr[x] = '\0';
            if(_shexp_match(str,&expr[++x], case_insensitive) == MATCH)
                goto punt;
            break;
        }
    }
    if(_shexp_match(str,expr, PR_FALSE) == MATCH) {
        PR_Free(expr);
        return 0;
    }

  punt:
    PR_Free(expr);
    return 1;
}
