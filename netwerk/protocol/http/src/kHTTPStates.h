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

#ifndef _kHTTPStates_h_
#define _kHTTPStates_h_

/*
    HTTP States.
*/
typedef enum _HTTPStates
{
    // The initial stage when a connection is set up
    IDLE, 
    // The request has been built. Now just waiting for Open-- final call that connects.
    WAITING_FOR_OPEN, 
    // Encountered a problem or a situation that requires user input.
    WAITING_FOR_USER_INPUT,
    // Waiting for a connection to go thru.
    WAITING_FOR_TRANSPORT,
    // Connected and now waiting for response.
    WAITING_FOR_RESPONSE,
    // Done
    DONE
} HTTPStates;

#endif //_kHTTPStates_h_

