/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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
 * replace them with the notice and other provisions requiored by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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
