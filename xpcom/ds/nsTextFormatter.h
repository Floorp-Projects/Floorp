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

#ifndef nsTextFormatter_h___
#define nsTextFormatter_h___

/*
** API for PR printf like routines. Supports the following formats
**	%d - decimal
**	%u - unsigned decimal
**	%x - unsigned hex
**	%X - unsigned uppercase hex
**	%o - unsigned octal
**	%hd, %hu, %hx, %hX, %ho - 16-bit versions of above
**	%ld, %lu, %lx, %lX, %lo - 32-bit versions of above
**	%lld, %llu, %llx, %llX, %llo - 64 bit versions of above
**	%s - utf8 string
**	%S - PRUnichar string
**	%c - character
**	%p - pointer (deals with machine dependent pointer size)
**	%f - float
**	%g - float
*/
#include "prtypes.h"
#include "prio.h"
#include <stdio.h>
#include <stdarg.h>
#include "nscore.h"
#include "nsAString.h"


class NS_COM nsTextFormatter {

public:

/*
** sprintf into a fixed size buffer. Guarantees that a NUL is at the end
** of the buffer. Returns the length of the written output, NOT including
** the NUL, or (PRUint32)-1 if an error occurs.
*/
static PRUint32  snprintf(PRUnichar *out, PRUint32 outlen, const PRUnichar *fmt, ...);

/*
** sprintf into a PR_MALLOC'd buffer. Return a pointer to the malloc'd
** buffer on success, NULL on failure. Call "smprintf_free" to release
** the memory returned.
*/
static PRUnichar*  smprintf(const PRUnichar *fmt, ...);


static PRUint32 ssprintf(nsAString& out, const PRUnichar* fmt, ...);
/*
** Free the memory allocated, for the caller, by smprintf
*/
static void smprintf_free(PRUnichar *mem);

/*
** "append" sprintf into a PR_MALLOC'd buffer. "last" is the last value of
** the PR_MALLOC'd buffer. sprintf will append data to the end of last,
** growing it as necessary using realloc. If last is NULL, PR_sprintf_append
** will allocate the initial string. The return value is the new value of
** last for subsequent calls, or NULL if there is a malloc failure.
*/
static PRUnichar*  sprintf_append(PRUnichar *last, const PRUnichar *fmt, ...);

/*
** va_list forms of the above.
*/
static PRUint32  vsnprintf(PRUnichar *out, PRUint32 outlen, const PRUnichar *fmt, va_list ap);
static PRUnichar*  vsmprintf(const PRUnichar *fmt, va_list ap);
static PRUint32    vssprintf(nsAString& out, const PRUnichar *fmt, va_list ap);
static PRUnichar*  vsprintf_append(PRUnichar *last, const PRUnichar *fmt, va_list ap);

#ifdef DEBUG
static PRBool SelfTest();
#endif


};

#endif /* nsTextFormatter_h___ */
