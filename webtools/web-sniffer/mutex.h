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
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): Bruce Robson
 */

#include "plat.h"

#ifdef PLAT_UNIX
#include <thread.h>
#include <synch.h>
#endif

#include "main.h"

#ifdef PLAT_UNIX

#define MUTEX_INIT() \
	if (mutex_init(&mainMutex, USYNC_THREAD, NULL)) \
	{ \
		fprintf(stderr, "mutex_init failed\n"); \
		exit(0); \
		return 1; \
	}

#define MUTEX_LOCK() \
	do \
	{ \
		mutex_lock(&mainMutex); \
	} while (0);

#define MUTEX_UNLOCK() \
	do \
	{ \
		mutex_unlock(&mainMutex); \
	} while (0);

#define DECLARE_MUTEX mutex_t mainMutex

#else /* Windows */

#define MUTEX_INIT() \
        if ((mainMutex = CreateMutex (NULL, FALSE, NULL)) == NULL) \
        { \
                fprintf(stderr, "CreateMutex failed\n"); \
                exit(0); \
                return 1; \
        }

#define MUTEX_LOCK() \
        do \
        { \
                if (WaitForSingleObject(mainMutex, INFINITE) == WAIT_FAILED) \
                { \
                        fprintf(stderr, "WaitForSingleObject(mainMutex) failed\n"); \
                } \
        } while (0);

#define MUTEX_UNLOCK() \
        do \
        { \
                if (ReleaseMutex(mainMutex) == 0) \
                { \
                        fprintf(stderr, "ReleaseMutex failed\n"); \
                } \
        } while (0);

#define DECLARE_MUTEX HANDLE mainMutex = NULL

#endif
