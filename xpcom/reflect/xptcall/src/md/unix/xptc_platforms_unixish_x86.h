/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Platform specific #defines to be shared by the various platforms sharing 
*  the unixish_86 code
*/

/*
*  The goal here is to clearly define the binary compatibility parameters for
*  the platforms that will use this code. Rather than switch at compile time
*  based on the compiler that happens to be in use we are forcing implementors
*  to make a conscious decision.
*
*  For some of these platforms the community may choose to have more than one
*  binary model in effect. In that case I suggest that there be explicit sub
*  defines for that platform specifying 'TYPE1', 'TYPE2', etc. The decision on
*  which 'TYPE' to use would be triggered by a setting passed through from the 
*  config system.
*
*  For example we might end up with something like:
*
* #elif defined(NTO) 
* #  if defined(TYPE1)
* #    define CFRONT_STYLE_THIS_ADJUST
* #  elif defined(TYPE1) 
* #    define THUNK_BASED_THIS_ADJUST
* #  else
* #    error "need TYPE1 or TYPE2 for NTO"
* #  endif
* #elif defined(__BEOS__) 
*
*  and so on....
*
*/

#if defined(LINUX)
#define THUNK_BASED_THIS_ADJUST

#elif defined(__FreeBSD__) 
/* System versions of gcc on FreeBSD don't use thunks.  On 3.x, the system
 * compiler is gcc 2.7.2.3, which doesn't use thunks by default.  On 4.x and
 * 5.x, /usr/src/contrib/gcc/config/freebsd.h explicitly undef's
 * DEFAULT_VTABLE_THUNKS.  (The one exception is a brief period (September
 * 1999 - Jan 2000) during 4.0-CURRENT, after egcs was merged --
 * this was changed before 4.0-RELEASE, but we can handle it anyway.)
 *
 * Versions of gcc from the ports collection (/usr/ports/lang/egcs),
 * however, have DEFAULT_VTABLE_THUNKS #defined to 1, at least
 * in all ports collections since the 2.95 merge.  (Supporting optional
 * compilers from FreeBSD 3.2 or earlier seems unnecessary).
 *
 * The easiest way to distinguish the ports collection gcc from the system
 * gcc is that the system gcc defines __FreeBSD_cc_version.  This variable
 * can also identify the period of time that 4.0-CURRENT used thunks.
 */
#if defined(__FreeBSD_cc_version) && \
    (__FreeBSD_cc_version < 400002 || __FreeBSD_cc_version > 400003)
#define CFRONT_STYLE_THIS_ADJUST
#else
#define THUNK_BASED_THIS_ADJUST
#endif

#elif defined(__NetBSD__) 
#define THUNK_BASED_THIS_ADJUST

#elif defined(__OpenBSD__) 
/* OpenBSD instroduces GCC 2.95.x in late May 1999 */
#include <sys/param.h>
#if OpenBSD <= 199905
#define THUNK_BASED_THIS_ADJUST
#else
#define CFRONT_STYLE_THIS_ADJUST
#endif

#elif defined(__bsdi__) 
#include <sys/param.h>
#if _BSDI_VERSION >= 199910
/* BSDI/4.1 ships with egcs, ergo thunk-based */
#define THUNK_BASED_THIS_ADJUST
#else
#define CFRONT_STYLE_THIS_ADJUST
#endif

#elif defined(NTO) 
#define CFRONT_STYLE_THIS_ADJUST

#elif defined(__BEOS__) 
#define CFRONT_STYLE_THIS_ADJUST

#elif defined(__sun__)
#define CFRONT_STYLE_THIS_ADJUST

#else
#error "need a platform define if using unixish x86 code"
#endif

/***************************************************************************/

#if !defined(THUNK_BASED_THIS_ADJUST) && !defined(CFRONT_STYLE_THIS_ADJUST)
#error "need to define 'this' adjust scheme"    
#endif

#if defined(THUNK_BASED_THIS_ADJUST) && defined(CFRONT_STYLE_THIS_ADJUST)
#error "need to define only ONE 'this' adjust scheme"    
#endif
