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
 * This section typedefs the old 'native' types to the new PR<type>s.
 * These definitions are scheduled to be eliminated at the earliest
 * possible time. The NSPR API is implemented and documented using
 * the new definitions.
 */

#if !defined(PROTYPES_H)
#define PROTYPES_H

typedef PRUintn uintn;
#ifndef _XP_Core_
typedef PRIntn intn;
#endif

/*
 * BeOS already defines the integer types below in its standard
 * header file SupportDefs.h.
 */
#ifdef XP_BEOS

#include <support/SupportDefs.h>

#else /* XP_BEOS */

/* SVR4 typedef of uint is commonly found on UNIX machines. */
#ifdef XP_UNIX
#include <sys/types.h>
#else
typedef PRUintn uint;
#endif

typedef PRUint64 uint64;
#if !defined(XP_MAC) && !defined(_WIN32) && !defined(XP_OS2)
typedef PRUint32 uint32;
#else
typedef unsigned long uint32;
#endif
typedef PRUint16 uint16;
typedef PRUint8 uint8;

/*
 * On AIX 4.3, sys/inttypes.h (which is included by sys/types.h, a very
 * common header file) defines the types int8, int16, int32, and int64.
 * So we don't define these four types here to avoid conflicts in case
 * the code also includes sys/types.h.
 */
#if defined(AIX4_3)
#include <sys/inttypes.h>
#else
typedef PRInt64 int64;

/* /usr/include/model.h on HP-UX defines int8, int16, and int32 */
#if defined(HPUX)
#include <model.h>
#else
#if !defined(WIN32) || !defined(_WINSOCK2API_)  /* defines its own "int32" */
#if !defined(XP_MAC) && !defined(_WIN32) && !defined(XP_OS2)
typedef PRInt32 int32;
#else
typedef long int32;
#endif
#endif
typedef PRInt16 int16;
typedef PRInt8 int8;
#endif /* HPUX */
#endif /* AIX4_3 */

#endif /* XP_BEOS */

typedef PRFloat64 float64;
typedef PRUptrdiff uptrdiff_t;
typedef PRUword uprword_t;
typedef PRWord prword_t;


/* Re: prbit.h */
#define TEST_BIT	PR_TEST_BIT
#define SET_BIT		PR_SET_BIT
#define CLEAR_BIT	PR_CLEAR_BIT

/* Re: prarena.h->plarena.h */
#define PRArena PLArena
#define PRArenaPool PLArenaPool
#define PRArenaStats PLArenaStats
#define PR_ARENA_ALIGN PL_ARENA_ALIGN
#define PR_INIT_ARENA_POOL PL_INIT_ARENA_POOL
#define PR_ARENA_ALLOCATE PL_ARENA_ALLOCATE
#define PR_ARENA_GROW PL_ARENA_GROW
#define PR_ARENA_MARK PL_ARENA_MARK
#define PR_CLEAR_UNUSED PL_CLEAR_UNUSED
#define PR_CLEAR_ARENA PL_CLEAR_ARENA
#define PR_ARENA_RELEASE PL_ARENA_RELEASE
#define PR_COUNT_ARENA PL_COUNT_ARENA
#define PR_ARENA_DESTROY PL_ARENA_DESTROY
#define PR_InitArenaPool PL_InitArenaPool
#define PR_FreeArenaPool PL_FreeArenaPool
#define PR_FinishArenaPool PL_FinishArenaPool
#define PR_CompactArenaPool PL_CompactArenaPool
#define PR_ArenaFinish PL_ArenaFinish
#define PR_ArenaAllocate PL_ArenaAllocate
#define PR_ArenaGrow PL_ArenaGrow
#define PR_ArenaRelease PL_ArenaRelease
#define PR_ArenaCountAllocation PL_ArenaCountAllocation
#define PR_ArenaCountInplaceGrowth PL_ArenaCountInplaceGrowth
#define PR_ArenaCountGrowth PL_ArenaCountGrowth
#define PR_ArenaCountRelease PL_ArenaCountRelease
#define PR_ArenaCountRetract PL_ArenaCountRetract

/* Re: prevent.h->plevent.h */
#define PREvent PLEvent
#define PREventQueue PLEventQueue
#define PR_CreateEventQueue PL_CreateEventQueue
#define PR_DestroyEventQueue PL_DestroyEventQueue
#define PR_GetEventQueueMonitor PL_GetEventQueueMonitor
#define PR_ENTER_EVENT_QUEUE_MONITOR PL_ENTER_EVENT_QUEUE_MONITOR
#define PR_EXIT_EVENT_QUEUE_MONITOR PL_EXIT_EVENT_QUEUE_MONITOR
#define PR_PostEvent PL_PostEvent
#define PR_PostSynchronousEvent PL_PostSynchronousEvent
#define PR_GetEvent PL_GetEvent
#define PR_EventAvailable PL_EventAvailable
#define PREventFunProc PLEventFunProc
#define PR_MapEvents PL_MapEvents
#define PR_RevokeEvents PL_RevokeEvents
#define PR_ProcessPendingEvents PL_ProcessPendingEvents
#define PR_WaitForEvent PL_WaitForEvent
#define PR_EventLoop PL_EventLoop
#define PR_GetEventQueueSelectFD PL_GetEventQueueSelectFD
#define PRHandleEventProc PLHandleEventProc
#define PRDestroyEventProc PLDestroyEventProc
#define PR_InitEvent PL_InitEvent
#define PR_GetEventOwner PL_GetEventOwner
#define PR_HandleEvent PL_HandleEvent
#define PR_DestroyEvent PL_DestroyEvent
#define PR_DequeueEvent PL_DequeueEvent
#define PR_GetMainEventQueue PL_GetMainEventQueue

/* Re: prhash.h->plhash.h */
#define PRHashEntry PLHashEntry
#define PRHashTable PLHashTable
#define PRHashNumber PLHashNumber
#define PRHashFunction PLHashFunction
#define PRHashComparator PLHashComparator
#define PRHashEnumerator PLHashEnumerator
#define PRHashAllocOps PLHashAllocOps
#define PR_NewHashTable PL_NewHashTable
#define PR_HashTableDestroy PL_HashTableDestroy
#define PR_HashTableRawLookup PL_HashTableRawLookup
#define PR_HashTableRawAdd PL_HashTableRawAdd
#define PR_HashTableRawRemove PL_HashTableRawRemove
#define PR_HashTableAdd PL_HashTableAdd
#define PR_HashTableRemove PL_HashTableRemove
#define PR_HashTableEnumerateEntries PL_HashTableEnumerateEntries
#define PR_HashTableLookup PL_HashTableLookup
#define PR_HashTableDump PL_HashTableDump
#define PR_HashString PL_HashString
#define PR_CompareStrings PL_CompareStrings
#define PR_CompareValues PL_CompareValues

#if defined(XP_MAC)
#ifndef TRUE				/* Mac standard is lower case true */
	#define TRUE 1
#endif
#ifndef FALSE				/* Mac standard is lower case false */
	#define FALSE 0
#endif
#endif

#endif /* !defined(PROTYPES_H) */
