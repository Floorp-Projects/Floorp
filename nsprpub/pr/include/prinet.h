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
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * File:		prinet.h
 * Description:
 *     Header file used to find the system header files for socket support.
 *     This file serves the following purposes:
 *     - A cross-platform, "get-everything" socket header file.  On
 *       Unix, socket support is scattered in several header files,
 *       while Windows and Mac have a "get-everything" socket header
 *       file.
 *     - NSPR needs the following macro definitions and function
 *       prototype declarations from these header files:
 *           AF_INET
 *           INADDR_ANY, INADDR_LOOPBACK, INADDR_BROADCAST
 *           ntohl(), ntohs(), htonl(), ntons().
 *       NSPR does not define its own versions of these macros and
 *       functions.  It simply uses the native versions, which have
 *       the same names on all supported platforms.
 *     This file is intended to be included by nspr20 public header
 *     files, such as prio.h.  One should not include this file directly.
 */

#ifndef prinet_h__
#define prinet_h__

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#ifdef LINUX
#undef __STRICT_ANSI__
#define __STRICT_ANSI__
#endif
#include <sys/types.h>
#include <sys/socket.h>		/* AF_INET */
#include <netinet/in.h>         /* INADDR_ANY, ..., ntohl(), ... */
#ifdef XP_OS2
#include <sys/ioctl.h>
#endif
#ifdef XP_UNIX
#ifdef AIX
/*
 * On AIX 4.3, the header <arpa/inet.h> refers to struct
 * ether_addr and struct sockaddr_dl that are not declared.
 * The following struct declarations eliminate the compiler
 * warnings.
 */
struct ether_addr;
struct sockaddr_dl;
#endif /* AIX */
#include <arpa/inet.h>
#endif /* XP_UNIX */
#include <netdb.h>

#if defined(FREEBSD) || defined(BSDI) || defined(QNX)
#include <rpc/types.h> /* the only place that defines INADDR_LOOPBACK */
#endif

/*
 * OS/2 hack.  For some reason INADDR_LOOPBACK is not defined in the
 * socket headers.
 */
#if defined(OS2) && !defined(INADDR_LOOPBACK)
#define INADDR_LOOPBACK 0x7f000001
#endif

/*
 * Prototypes of ntohl() etc. are declared in <machine/endian.h>
 * on these platforms.
 */
#if defined(BSDI) || defined(OSF1)
#include <machine/endian.h>
#endif

#elif defined(WIN32)

/* Do not include any system header files. */

#elif defined(WIN16)

#include <winsock.h>

#elif defined(XP_MAC)

#include "macsocket.h"

#else

#error Unknown platform

#endif

#endif /* prinet_h__ */
