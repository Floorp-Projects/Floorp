/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef _nsHTTPEnums_h_
#define _nsHTTPEnums_h_

typedef enum {
    HM_DELETE, 
    HM_GET,
    HM_HEAD,
    HM_INDEX,
    HM_LINK,
    HM_OPTIONS,
    HM_POST,
    HM_PUT,
    HM_PATCH,
    HM_TRACE,
    HM_UNLINK,
    TOTAL_NUMBER_OF_METHODS // Always the last
} HTTPMethod;

/*
    HTTP States.
*/
typedef enum 
{
    // The initial stage when a connection is set up
    // For pipelining we repeat from here
    HS_IDLE, 
    // A temporarily suspended state. 
    HS_SUSPENDED, 
    // The request has been built. Now just waiting for Open-- final call that connects.
    HS_WAITING_FOR_OPEN, 
    // Encountered a problem or a situation that requires user input.
    HS_WAITING_FOR_USER_INPUT,
    // Waiting for a connection to go thru.
    HS_WAITING_FOR_TRANSPORT,
    // Connected and now waiting for response.
    HS_WAITING_FOR_RESPONSE,
    // Done
    HS_DONE
} HTTPState;

typedef enum {
    HTTP_ZERO_NINE, // HTTP/0.9
    HTTP_ONE_ZERO,  // HTTP/1.0
    HTTP_ONE_ONE    // HTTP/1.1
} HTTPVersion;

#endif //_knsHTTPEnumss_h_