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

PR_BEGIN_EXTERN_C

#if (defined(__sun) && !defined(SOLARIS2_6)) || defined(HPUX9) || defined(HPUX10_10) || defined(XP_PC) || defined(AIX) || defined(OSF1) || defined(XP_MAC) || defined(SONY) || defined(SNI) || defined(UNIXWARE) || defined(LINUX)
typedef int16 int16_t;
typedef int32 int32_t;
#endif

#if !defined(IRIX6_2) && !defined(IRIX6_3) && !defined(SOLARIS2_6) && !defined(HPUX10_20) && !defined(HPUX10_30) && !defined(HPUX11)
typedef uint16 uint16_t;
#ifndef	_UINT32_T
#define	_UINT32_T
typedef uint32 uint32_t;
#endif /* _UINT32_T */
#endif /* !defined(IRIX6_2) && !defined(IRIX6_3) && !defined(SOLARIS2_6) */

typedef prword_t uintVP_t; /* unsigned that is same size as a void pointer */

#if !defined(BSDI) && !defined(IRIX6_2) && !defined(IRIX6_3) && !defined(LINUX2_0) && !defined(MKLINUX) && !defined(SOLARIS2_6) && !defined(HPUX10_20) && !defined(HPUX10_30) && !defined(HPUX11)
typedef int64 int64_t;
#else
/*
** On BSDI, for some damn reason, they define long long's for these types
** even though they aren't actually 64 bits wide!
*/
#define int64_t int64
#endif

#if defined(XP_PC) || (defined(__sun) && !defined(SVR4)) || defined(HPUX) || defined(LINUX) || defined(BSDI)  /* || defined(XP_MAC) */
typedef unsigned int uint_t;
#elif defined(XP_MAC)
	/* we have to push/pop to avoid breaking existing projects that
		have "treat warnings as errors" on. This is two, two, TWO hacks in one! (pinkerton) */
	#pragma warning_errors off
	#ifndef __OPENTRANSPORT__
	typedef unsigned long uint_t;	/* this is already declared in OpenTransport.h, but we don't want to drag in
										all the other defines present there. This is a bad solution, but the
										least bad given the alternatives. */
	#endif /* __OPENTRANSPORT__ */
	#pragma warning_errors reset
#endif

#if defined(XP_PC)
typedef long int32_t;
#endif

PR_END_EXTERN_C

#endif /* sun_java_typedefs_md_h___ */
