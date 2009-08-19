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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Rob McCool  (original author)
 *      Nelson Bolyard <nelson@bolyard.me>
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
 * shexp.h: Defines and prototypes for shell exp. match routines
 * 
 * This routine will match a string with a shell expression. The expressions
 * accepted are based loosely on the expressions accepted by zsh.
 * 
 * o * matches anything
 * o ? matches one character
 * o \ will escape a special character
 * o $ matches the end of the string
 * Bracketed expressions:
 * o [abc] matches one occurence of a, b, or c.  
 * o [^abc] matches any character except a, b, or c.
 *     To be matched between [ and ], these characters must be escaped: \ ]
 *     No other characters need be escaped between brackets. 
 *     Unnecessary escaping is permitted.
 * o [a-z] matches any character between a and z, inclusive.
 *     The two range-definition characters must be alphanumeric ASCII.
 *     If one is upper case and the other is lower case, then the ASCII
 *     non-alphanumeric characters between Z and a will also be in range.
 * o [^a-z] matches any character except those between a and z, inclusive.
 *     These forms cannot be combined, e.g [a-gp-z] does not work.
 * o Exclusions:
 *   As a top level, outter-most expression only, the expression
 *   foo~bar will match the expression foo, provided it does not also 
 *     match the expression bar.  Either expression or both may be a union.
 *     Except between brackets, any unescaped ~ is an exclusion. 
 *     At most one exclusion is permitted.
 *     Exclusions cannot be nested (contain other exclusions).
 *     example: *~abc will match any string except abc
 * o Unions:
 *   (foo|bar) will match either the expression foo, or the expression bar.
 *     At least one '|' separator is required.  More are permitted.
 *     Expressions inside unions may not include unions or exclusions.
 *     Inside a union, to be matched and not treated as a special character,
 *     these characters must be escaped: \ ( | ) [ ~ except when they occur
 *     inside a bracketed expression, where only \ and ] require escaping.
 *
 * The public interface to these routines is documented below.
 * 
 */
 
#ifndef SHEXP_H
#define SHEXP_H

#include "utilrename.h"
/*
 * Requires that the macro MALLOC be set to a "safe" malloc that will 
 * exit if no memory is available. 
 */


/* --------------------------- Public routines ---------------------------- */


/*
 * shexp_valid takes a shell expression exp as input. It returns:
 * 
 *  NON_SXP      if exp is a standard string
 *  INVALID_SXP  if exp is a shell expression, but invalid
 *  VALID_SXP    if exp is a valid shell expression
 */

#define NON_SXP -1
#define INVALID_SXP -2
#define VALID_SXP 1

SEC_BEGIN_PROTOS

extern int PORT_RegExpValid(const char *exp);

extern int PORT_RegExpSearch(const char *str, const char *exp);

/* same as above but uses case insensitive search */
extern int PORT_RegExpCaseSearch(const char *str, const char *exp);

SEC_END_PROTOS

#endif
