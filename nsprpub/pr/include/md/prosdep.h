/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions requiored by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

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
#elif defined(WIN95)
#include "md/_win95.h"
#include "md/_win32_errors.h"
#elif defined(WIN16)
#include "md/_win16.h"
#elif defined(OS2)
#include "md/_os2.h"
#include "md/_os2_errors.h"
#else
#error unknown Windows platform
#endif

#elif defined XP_MAC

#include "_macos.h"

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

#elif defined(LINUX)
#include "md/_linux.h"

#elif defined(OSF1)
#include "md/_osf1.h"

#elif defined(DARWIN)
#include "md/_darwin.h"

#elif defined(NEXTSTEP)
#include "md/_nextstep.h"

#elif defined(SOLARIS)
#include "md/_solaris.h"

#elif defined(SUNOS4)
#include "md/_sunos4.h"

#elif defined(SNI)
#include "md/_reliantunix.h"

#elif defined(SONY)
#include "md/_sony.h"

#elif defined(NEC)
#include "md/_nec.h"

#elif defined(SCO)
#include "md/_scoos.h"

#elif defined(UNIXWARE)
#include "md/_unixware.h"

#elif defined(NCR)
#include "md/_ncr.h"

#elif defined(DGUX)
#include "md/_dgux.h"

#elif defined(QNX)
#include "md/_qnx.h"

#elif defined(VMS)
#include "md/_openvms.h"

#elif defined(NTO)
#include "md/_nto.h"

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
