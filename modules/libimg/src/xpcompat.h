/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/*
 * This will probably go away after we stop using 16-bit compilers
 * for Win16.
 */
#ifdef XP_WIN16
#define XP_HUGE __huge
#define XP_HUGE_ALLOC(SIZE) halloc(SIZE,1)
#define XP_HUGE_FREE(SIZE) hfree(SIZE)
#define XP_HUGE_MEMCPY(DEST, SOURCE, LEN) hmemcpy(DEST, SOURCE, LEN)
#else
#define XP_HUGE
#define XP_HUGE_ALLOC(SIZE) malloc(SIZE)
#define XP_HUGE_FREE(SIZE) free(SIZE)
#define XP_HUGE_MEMCPY(DEST, SOURCE, LEN) memcpy(DEST, SOURCE, LEN)
#endif

#define XP_HUGE_CHAR_PTR char XP_HUGE *

/*
 * These will be replaced with their PL_ equivalents.
 */
#define XP_MEMCPY(d, s, n)        memcpy((d), (s), (n))

/* NOTE: XP_MEMMOVE gurantees that overlaps will be properly handled */
#ifdef SUNOS4
#define XP_MEMMOVE(Dest,Src,Len)  bcopy((Src),(Dest),(Len))
#else
#define XP_MEMMOVE(Dest,Src,Len)  memmove((Dest),(Src),(Len))
#endif /* SUNOS4 */

#define XP_MEMSET                  memset
#define XP_BZERO(a,b)	           memset(a,0,b)

/* NOTE: XP_BCOPY gurantees that overlaps will be properly handled */
#ifdef XP_WIN16

	XP_BEGIN_PROTOS
	extern void WIN16_bcopy(char *, char *, unsigned long);
	XP_END_PROTOS

	#define XP_BCOPY(PTR_FROM, PTR_TO, LEN) \
	        		(WIN16_bcopy((char *) (PTR_FROM), (char *)(PTR_TO), (LEN)))

#else

	#define XP_BCOPY(Src,Dest,Len)  XP_MEMMOVE((Dest),(Src),(Len))

#endif /* XP_WIN16 */

#define XP_SPRINTF                	sprintf
#define XP_SAFE_SPRINTF		PR_snprintf
#define XP_MEMCMP                 	memcmp
#define XP_VSPRINTF               	vsprintf

#if !defined(XP_RANDOM) || !defined(XP_SRANDOM)   /* defined in both xp_mcom.h and xp_str.h */
#if defined(HAVE_RANDOM) && !defined(__QNX__)     /* QNX 4.24 _has_ random, but no prototype */
#define XP_RANDOM		random
#define XP_SRANDOM(seed)	srandom((seed))
#else
#define XP_RANDOM		rand
#define XP_SRANDOM(seed)	srand((seed))
#endif
#endif

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

#define BlockAllocCopy(dest, src, src_length) NET_BACopy((char**)&(dest), src, src_length)
#define BlockAllocCat(dest, dest_length, src, src_length)  NET_BACat(&(dest), dest_length, src, src_length)
extern char * NET_BACopy (char **dest, const char *src, size_t src_length);
extern char * NET_BACat  (char **dest, size_t dest_length, const char *src, size_t src_length);

/*
 * Malloc'd string manipulation
 *
 * notice that they are dereferenced by the define!
 */
#define StrAllocCopy(dest, src) NET_SACopy (&(dest), src)
#define StrAllocCat(dest, src)  NET_SACat  (&(dest), src)
extern char * NET_SACopy (char **dest, const char *src);
extern char * NET_SACat  (char **dest, const char *src);

NS_EXPORT void* IL_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs);

NS_EXPORT void IL_ClearTimeout(void *timer_id);

PR_END_EXTERN_C

#endif
