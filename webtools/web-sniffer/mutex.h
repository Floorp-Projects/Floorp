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
 * Contributor(s): 
 */

#include <thread.h>
#include <synch.h>

#include "main.h"

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
