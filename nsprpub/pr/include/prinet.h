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
#define INADDR_LOOPBACK gethostid()
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
