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
#ifndef sun_java_typedefs_md_h___
#define sun_java_typedefs_md_h___

#include "prtypes.h"
#include "nspr_md.h"

/* Some platforms need this to define int32_t */
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif

PR_BEGIN_EXTERN_C

#ifndef HAVE_INT16_T
typedef int16 int16_t;
#endif

#ifndef HAVE_INT32_T
typedef int32 int32_t;
#endif

#ifndef HAVE_UINT16_T
typedef uint16 uint16_t;
#ifndef	_UINT32_T
#define	_UINT32_T
typedef uint32 uint32_t;
#endif
#endif

typedef prword_t uintVP_t; /* unsigned that is same size as a void pointer */

#if !defined(BSDI) && !defined(IRIX6_2) && !defined(IRIX6_3) && !defined(LINUX) && !defined(SOLARIS2_6) && !defined(HPUX10_20) && !defined(HPUX10_30) && !defined(HPUX11) && !defined(RHAPSODY) && !defined(NETBSD) && !defined(AIX) && !defined(HPUX)
typedef int64 int64_t;
#else
/*
** On BSDI, for some reason, they define long long's for these types
** even though they aren't actually 64 bits wide!
*/
#define int64_t int64
#endif

#if defined(XP_PC) || (defined(__sun) && !defined(SVR4)) || defined(HPUX) || defined(LINUX) || defined(BSDI)  /* || defined(XP_MAC) */
typedef unsigned int uint_t;
#elif defined(XP_MAC)
/* we have to push/pop to avoid breaking existing projects that
** have "treat warnings as errors" on. This is two, two, TWO hacks in one! (pinkerton)
*/
#pragma warning_errors off
#ifndef __OPENTRANSPORT__
typedef unsigned long uint_t;	/* this is already declared in OpenTransport.h, but we don't want to drag in
					all the other defines present there. This is a bad solution, but the
					least bad given the alternatives. */
#endif /* __OPENTRANSPORT__ */
#pragma warning_errors reset
#endif

#if defined(XP_PC) && !defined(XP_OS2)
typedef long int32_t;
#endif

PR_END_EXTERN_C

#endif /* sun_java_typedefs_md_h___ */
