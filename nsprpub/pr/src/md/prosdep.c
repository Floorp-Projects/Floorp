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

#include "prbit.h"
#include "prsystem.h"

#ifdef XP_UNIX
#include <unistd.h>
#endif
#ifdef SUNOS4
#include "md/sunos4.h"
#endif
#ifdef _WIN32
#include <windows.h>
#endif 
#ifdef XP_BEOS
#include <OS.h>
#endif

PRInt32 _pr_pageShift;
PRInt32 _pr_pageSize;

/*
** Get system page size
*/
static void GetPageSize(void)
{
	PRInt32 pageSize;

    /* Get page size */
#ifdef XP_UNIX
#if defined SUNOS4 || defined LINUX || defined BSDI || defined AIX \
        || defined FREEBSD || defined NETBSD || defined OPENBSD \
        || defined RHAPSODY || defined NEXTSTEP
    _pr_pageSize = getpagesize();
#elif defined(HPUX)
    /* I have no idea. Don't get me started. --Rob */
    _pr_pageSize = sysconf(_SC_PAGE_SIZE);
#else
    _pr_pageSize = sysconf(_SC_PAGESIZE);
#endif
#endif /* XP_UNIX */

#ifdef XP_MAC
    _pr_pageSize = 4096;
#endif /* XP_MAC */

#ifdef XP_BEOS
    _pr_pageSize = B_PAGE_SIZE;
#endif

#ifdef XP_PC
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    _pr_pageSize = info.dwPageSize;
#else
    _pr_pageSize = 4096;
#endif
#endif /* XP_PC */

	pageSize = _pr_pageSize;
	PR_CEILING_LOG2(_pr_pageShift, pageSize);
}

PR_IMPLEMENT(PRInt32) PR_GetPageShift(void)
{
    if (!_pr_pageSize) {
	GetPageSize();
    }
    return _pr_pageShift;
}

PR_IMPLEMENT(PRInt32) PR_GetPageSize(void)
{
    if (!_pr_pageSize) {
	GetPageSize();
    }
    return _pr_pageSize;
}
