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

/*
** File:        plerror.h
** Description: Simple routine to print translate the calling thread's
**              error numbers and print them.
*/

#if defined(PLERROR_H)
#else
#define PLERROR_H

#include "prio.h"
#include "prtypes.h"

PR_BEGIN_EXTERN_C
/*
** Print the messages to "syserr" prepending 'msg' if not NULL.
*/
PR_EXTERN(void) PL_PrintError(const char *msg);

/*
** Print the messages to specified output file prepending 'msg' if not NULL.
*/
PR_EXTERN(void) PL_FPrintError(PRFileDesc *output, const char *msg);

PR_END_EXTERN_C

#endif /* defined(PLERROR_H) */

/* plerror.h */
