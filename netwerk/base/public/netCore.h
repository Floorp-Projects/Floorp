/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __netCore_h__
#define __netCore_h__

#include "nsError.h"

 /* networking error codes */

// NET RANGE:   1 -20
// FTP RANGE:   21-30
// HTTP RANGE:  31-40
// DNS RANGE:   41-50 
// SOCKET RANGE 51-60
// CACHE RANGE: 61-70

// XXX Why can't we put all Netwerk error codes in one file to help avoid collisions?

#define NS_ERROR_ALREADY_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 11)

#define NS_ERROR_NOT_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 12)

/* see nsISocketTransportService.idl for other errors */

#define NS_ERROR_IN_PROGRESS \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 15)

#define NS_ERROR_OFFLINE \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 16)

#define NS_ERROR_PORT_ACCESS_NOT_ALLOWED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 19)

#undef NS_NET
#ifdef _IMPL_NS_NET
#if defined(XP_PC) && !defined(XP_OS2)
#define NS_NET _declspec(dllexport)
#else  /* !XP_PC */
#define NS_NET
#endif /* !XP_PC */
#else  /* !_IMPL_NS_NET */
#if defined(XP_PC) && !defined(XP_OS2)
#define NS_NET _declspec(dllimport)
#else  /* !XP_PC */
#define NS_NET
#endif /* !XP_PC */
#endif /* !_IMPL_NS_NET */

// Where most necko status messages come from:
#define NECKO_MSGS_URL  "chrome://necko/locale/necko.properties"

#endif // __netCore_h__
