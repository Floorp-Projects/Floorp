/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation..
 * Portions created by Netscape Communications Corporation. are
 * Copyright (C) 1998 Netscape Communications Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * shexp.h: Defines and prototypes for shell exp. match routines
 * 
 *
 * This routine will match a string with a shell expression. The expressions
 * accepted are based loosely on the expressions accepted by zsh.
 * 
 * o * matches anything
 * o ? matches one character
 * o \ will escape a special character
 * o $ matches the end of the string
 * o [abc] matches one occurence of a, b, or c. The only character that needs
 *         to be escaped in this is ], all others are not special.
 * o [a-z] matches any character between a and z
 * o [^az] matches any character except a or z
 * o ~ followed by another shell expression will remove any pattern
 *     matching the shell expression from the match list
 * o (foo|bar) will match either the substring foo, or the substring bar.
 *             These can be shell expressions as well.
 * 
 * The public interface to these routines is documented below.
 * 
 * Rob McCool
 * 
 */
 
#ifndef nsWildCard_h__
#define nsWildCard_h__

#include "prtypes.h"
#include <ctype.h>  /* isalnum */
#include <string.h> /* strlen */

/* --------------------------- Public routines ---------------------------- */


/*
 * NS_WildCardValid takes a shell expression exp as input. It returns:
 * 
 *  NON_SXP      if exp is a standard string
 *  INVALID_SXP  if exp is a shell expression, but invalid
 *  VALID_SXP    if exp is a valid shell expression
 */

#define NON_SXP -1
#define INVALID_SXP -2
#define VALID_SXP 1

extern int NS_WildCardValid(char *expr);


/* return values for the search routines */
#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

/*
 * NS_WildCardMatch
 * 
 * Takes a prevalidated shell expression exp, and a string str.
 *
 * Returns 0 on match and 1 on non-match.
 */

extern int NS_WildCardMatch(char *str, char *expr, PRBool case_insensitive);

/*
 * Same as above, but validates the exp first. 0 on match, 1 on non-match,
 * -1 on invalid exp.
 */

extern int NS_WildCardSearch(char *str, char *expr);

/*
 * Same as above but uses case insensitive search.
 */
extern int NS_WildCardCaseSearch(char *str, char *expr);

#endif /* nsWildCard_h__ */
