/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * The purpose of this file is to help phase out XP_ library
 * from the image library. In general, XP_ data structures and
 * functions will be replaced with the PR_ or PL_ equivalents.
 * In cases where the PR_ or PL_ equivalents don't yet exist,
 * this file (and its source equivalent) will play the role 
 * of the XP_ library.
 */

#ifndef xpcompat_h___
#define xpcompat_h___

#include "platform.h"
#include "prtypes.h"
#include "nsCom.h"


#define XP_HUGE
#define XP_HUGE_ALLOC(SIZE) malloc(SIZE)
#define XP_HUGE_FREE(SIZE) free(SIZE)
#define XP_HUGE_MEMCPY(DEST, SOURCE, LEN) memcpy(DEST, SOURCE, LEN)


#define XP_HUGE_CHAR_PTR char XP_HUGE *


typedef void
(*TimeoutCallbackFunction) (void * closure);


PR_BEGIN_EXTERN_C

/*
 * XP_GetString is a mechanism for getting localized strings from a 
 * resource file. It needs to be replaced with a PR_ equivalent.
 */
extern char *XP_GetString(int i);

/* 
 * I don't know if qsort will ever be in NSPR, so this might have
 * to stay here indefinitely.  Mac is completely broken and should use
 * mozilla/include/xp_qsort.h, mozilla/lib/xp/xp_qsort.c.
 */
#if defined(XP_MAC_NEVER)
extern void XP_QSORT(void *, size_t, size_t,
                     int (*)(const void *, const void *));
#endif /* XP_MAC */


NS_EXPORT void* IL_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs);

NS_EXPORT void IL_ClearTimeout(void *timer_id);

PR_END_EXTERN_C

#endif
