/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#ifndef _SECCOMON_H_
#define _SECCOMON_H_

#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "prmem.h"
#include "plarena.h"
#include "prtypes.h"
#include "prlog.h"
#include "plstr.h"
#include "prio.h"

#if defined(XP_CPLUSPLUS)
# define SEC_BEGIN_PROTOS extern "C" {
# define SEC_END_PROTOS }
#else
# define SEC_BEGIN_PROTOS
# define SEC_END_PROTOS
#endif

#include "secstubt.h"
#include "secstubs.h"

#define PORT_Strlen(s) 	strlen(s)
#define PORT_Strcasecmp PL_strcasecmp
#define PORT_Memset 	memset
#define PORT_Strcmp 	strcmp
#define PORT_Assert PR_ASSERT
#define PORT_Memcpy 	memcpy
#define PORT_Strchr 	strchr

SEC_BEGIN_PROTOS
void *
PORT_Alloc(size_t bytes);

void *
PORT_Realloc(void *oldptr, size_t bytes);

void *
PORT_ZAlloc(size_t bytes);

void
PORT_Free(void *ptr);

void
PORT_ZFree(void *ptr, size_t len);

PRArenaPool *
PORT_NewArena(unsigned long chunksize);

void *
PORT_ArenaAlloc(PRArenaPool *arena, size_t size);

void *
PORT_ArenaZAlloc(PRArenaPool *arena, size_t size);

void
PORT_FreeArena(PRArenaPool *arena, PRBool zero);

void *
PORT_ArenaGrow(PRArenaPool *arena, void *ptr, size_t oldsize, size_t newsize);

void *
PORT_ArenaMark(PRArenaPool *arena);

void
PORT_ArenaRelease(PRArenaPool *arena, void *mark);
SEC_END_PROTOS

#endif /* _SECCOMON_H_ */
