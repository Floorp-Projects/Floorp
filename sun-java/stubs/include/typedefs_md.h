/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

#if defined(HAVE_INT64) && !defined(HAVE_INT64_T)
typedef int64 int64_t;
#endif

#if !defined(HAVE_INT64) && defined(HAVE_INT64_T)
/*
** On BSDI, for some reason, they define long long's for these types
** even though they aren't actually 64 bits wide!
*/
#define int64_t int64
#endif

#ifndef HAVE_UINT_T
#ifndef XP_MAC
typedef unsigned int uint_t;
#else
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
#endif

#if defined(XP_PC) && !defined(XP_OS2)
typedef long int32_t;
#endif

PR_END_EXTERN_C

#endif /* sun_java_typedefs_md_h___ */
