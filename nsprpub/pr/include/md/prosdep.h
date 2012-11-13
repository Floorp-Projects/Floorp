/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef prosdep_h___
#define prosdep_h___

/*
** Get OS specific header information
*/
#include "prtypes.h"

PR_BEGIN_EXTERN_C

#ifdef XP_PC

#include "md/_pcos.h"
#ifdef WINNT
#include "md/_winnt.h"
#include "md/_win32_errors.h"
#elif defined(WIN95) || defined(WINCE)
#include "md/_win95.h"
#include "md/_win32_errors.h"
#elif defined(OS2)
#include "md/_os2.h"
#include "md/_os2_errors.h"
#else
#error unknown Windows platform
#endif

#elif defined(XP_UNIX)

#if defined(AIX)
#include "md/_aix.h"

#elif defined(FREEBSD)
#include "md/_freebsd.h"

#elif defined(NETBSD)
#include "md/_netbsd.h"

#elif defined(OPENBSD)
#include "md/_openbsd.h"

#elif defined(BSDI)
#include "md/_bsdi.h"

#elif defined(HPUX)
#include "md/_hpux.h"

#elif defined(IRIX)
#include "md/_irix.h"

#elif defined(LINUX) || defined(__GNU__) || defined(__GLIBC__)
#include "md/_linux.h"

#elif defined(OSF1)
#include "md/_osf1.h"

#elif defined(DARWIN)
#include "md/_darwin.h"

#elif defined(SOLARIS)
#include "md/_solaris.h"

#elif defined(SCO)
#include "md/_scoos.h"

#elif defined(UNIXWARE)
#include "md/_unixware.h"

#elif defined(DGUX)
#include "md/_dgux.h"

#elif defined(QNX)
#include "md/_qnx.h"

#elif defined(NTO)
#include "md/_nto.h"

#elif defined(RISCOS)
#include "md/_riscos.h"

#elif defined(SYMBIAN)
#include "md/_symbian.h"

#else
#error unknown Unix flavor

#endif

#include "md/_unixos.h"
#include "md/_unix_errors.h"

#elif defined(XP_BEOS)

#include "md/_beos.h"
#include "md/_unix_errors.h"

#else

#error "The platform is not BeOS, Unix, Windows, or Mac"

#endif

#ifdef _PR_PTHREADS
#include "md/_pth.h"
#endif

PR_END_EXTERN_C

#endif /* prosdep_h___ */
