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
 * plresolv.h - asynchronous name resolution using DNS 
 */

#ifndef _PLRESOLV_H_
#define _PLRESOLV_H_

/*
** THIS IS WORK IN PROGRESS. DO NOT ATTEMPT TO USE ANY PORTION OF THIS
** API UNTIL THIS MESSAGE NO LONGER EXISTS. IF YOU DO, THEN YOU SURRENDER
** THE RIGHT TO COMPLAIN ABOUT ANY CONTENT.
*/

#if defined(XP_UNIX)

#include <prtypes.h>
#include <prnetdb.h>

NSPR_BEGIN_EXTERN_C

#define PL_RESOLVE_MAXHOSTENTBUF        1024
#define PL_RESOLVE_DEFAULT_TIMEOUT      0

/* Error return codes */
#define PL_RESOLVE_OK            0
#define PL_RESOLVE_EWINIT        1           /* Failed to initialize window */
#define PL_RESOLVE_EMAKE         2           /* Failed to create request */
#define PL_RESOLVE_ELAUNCH       3           /* Error launching Async request */
#define PL_RESOLVE_ETIMEDOUT     4           /* Request timed-out */
#define PL_RESOLVE_EINVAL        5           /* Invalid argument */
#define PL_RESOLVE_EOVERFLOW     6           /* Buffer Overflow */
#define PL_RESOLVE_EUNKNOWN      7           /* berzerk error */

/* ----------- Function Prototypes ----------------*/

PR_EXTERN(PRStatus) PL_ResolveName(
    const char *name, unsigned char *buf,
    PRIntn bufsize, PRIntervalTime timeout,
    PRHostEnt *hostentry, PRIntervalTime *ttl);

PR_EXTERN(PRStatus) PL_ResolveAddr(
    const PRNetAddr *address, unsigned char *buf,
    PRIntn bufsize, PRIntervalTime timeout,
    PRHostEnt *hostentry, PRIntervalTime *ttl);

typedef struct PLResolveStats {
    int re_errors;
    int re_nu_look;
    int re_na_look;
    int re_replies;
    int re_requests;
    int re_resends;
    int re_sent;
    int re_timeouts;
} PLResolveStats;

typedef struct PLResoveInfo {
    PRBool enabled;
    PRUint32 numNameLookups;
    PRUint32 numAddrLookups;
    PRUint32 numLookupsInProgress;
    PLResolveStats stats;
} PLResoveInfo;

PR_EXTERN(void) PL_ResolveInfo(PLResoveInfo *info);

NSPR_END_EXTERN_C

#endif /* defined(XP_UNIX) */

#endif /* _PLRESOLV_H_ */
