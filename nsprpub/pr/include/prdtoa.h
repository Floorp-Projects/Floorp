/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#ifndef prdtoa_h___
#define prdtoa_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/*
** PR_strtod() returns as a double-precision floating-point number
** the  value represented by the character string pointed to by
** s00. The string is scanned up to the first unrecognized
** character.
**a
** If the value of se is not (char **)NULL, a  pointer  to
** the  character terminating the scan is returned in the location pointed
** to by se. If no number can be formed, se is set to s00, and
** zero is returned.
*/
#if defined(HAVE_WATCOM_BUG_1)
/* this is a hack to circumvent a bug in the Watcom C/C++ 11.0 compiler
** When Watcom fixes the bug, remove the special case for Win16
*/
PRFloat64 __pascal __loadds __export
#else
PR_EXTERN(PRFloat64)
#endif
PR_strtod(const char *s00, char **se);

/*
** PR_cnvtf()
** conversion routines for floating point
** prcsn - number of digits of precision to generate floating
** point value.
*/
PR_EXTERN(void) PR_cnvtf(char *buf, PRIntn bufsz, PRIntn prcsn, PRFloat64 fval);

/*
** PR_dtoa() converts double to a string.
**
** ARGUMENTS:
** If rve is not null, *rve is set to point to the end of the return value.
** If d is +-Infinity or NaN, then *decpt is set to 9999.
**
** mode:
**     0 ==> shortest string that yields d when read in
**           and rounded to nearest.
*/
PR_EXTERN(PRStatus) PR_dtoa(PRFloat64 d, PRIntn mode, PRIntn ndigits,
	PRIntn *decpt, PRIntn *sign, char **rve, char *buf, PRSize bufsize);

PR_END_EXTERN_C

#endif /* prdtoa_h___ */
