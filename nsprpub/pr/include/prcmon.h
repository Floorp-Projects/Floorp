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

#ifndef prcmon_h___
#define prcmon_h___

/*
** Interface to cached monitors. Cached monitors use an address to find a
** given PR monitor. In this way a monitor can be associated with another
** object without preallocating a monitor for all objects.
**
** A hash table is used to quickly map addresses to individual monitors
** and the system automatically grows the hash table as needed.
**
** Cache monitors are about 5 times slower to use than uncached monitors.
*/
#include "prmon.h"
#include "prinrval.h"

PR_BEGIN_EXTERN_C

/**
** Like PR_EnterMonitor except use the "address" to find a monitor in the
** monitor cache. If successful, returns the PRMonitor now associated
** with "address". Note that you must PR_CExitMonitor the address to
** release the monitor cache entry (otherwise the monitor cache will fill
** up). This call will return NULL if the monitor cache needs to be
** expanded and the system is out of memory.
*/
PR_EXTERN(PRMonitor*) PR_CEnterMonitor(void *address);

/*
** Like PR_ExitMonitor except use the "address" to find a monitor in the
** monitor cache.
*/
PR_EXTERN(PRStatus) PR_CExitMonitor(void *address);

/*
** Like PR_Wait except use the "address" to find a monitor in the
** monitor cache.
*/
PR_EXTERN(PRStatus) PR_CWait(void *address, PRIntervalTime timeout);

/*
** Like PR_Notify except use the "address" to find a monitor in the
** monitor cache.
*/
PR_EXTERN(PRStatus) PR_CNotify(void *address);

/*
** Like PR_NotifyAll except use the "address" to find a monitor in the
** monitor cache.
*/
PR_EXTERN(PRStatus) PR_CNotifyAll(void *address);

PR_END_EXTERN_C

#endif /* prcmon_h___ */
