/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __netCore_h__
#define __netCore_h__

#include "nsError.h"

 /* networking error codes */

// NET RANGE:   1 -20
// FTP RANGE:   21-30
// HTTP RANGE:  31-40
// DNS RANGE:   41-50 

#define NS_ERROR_ALREADY_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 11)

#define NS_ERROR_NOT_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 12)

#define NS_ERROR_CONNECTION_REFUSED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 13)

#define NS_ERROR_TCP_TIMEOUT \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 14)

#define NS_ERROR_IN_PROGRESS \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 15)

#undef NS_NET
#ifdef _IMPL_NS_NET
#ifdef XP_PC
#define NS_NET _declspec(dllexport)
#else  /* !XP_PC */
#define NS_NET
#endif /* !XP_PC */
#else  /* !_IMPL_NS_NET */
#ifdef XP_PC
#define NS_NET _declspec(dllimport)
#else  /* !XP_PC */
#define NS_NET
#endif /* !XP_PC */
#endif /* !_IMPL_NS_NET */

#endif // __netCore_h__
