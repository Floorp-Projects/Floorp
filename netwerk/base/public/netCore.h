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
#include "nsError.h"

 /* networking error codes */

#define NS_ERROR_ALREADY_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 1)

#define NS_ERROR_NOT_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 2)

#define NS_ERROR_CONNECTION_REFUSED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 3)

#define NS_ERROR_DNS_DOES_NOT_EXIST \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 4)

#define NS_ERROR_TCP_TIMEOUT \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 5)

 