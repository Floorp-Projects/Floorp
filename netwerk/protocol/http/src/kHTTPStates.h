/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

