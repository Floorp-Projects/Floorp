/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsIProtocolInstance_h_
#define _nsIProtocolInstance_h_

#include "nsISupports.h"
class nsIInputStream;

/* 
    The nsIProtocolInstance class is a common interface to all protocol
	functionality. This base interface will support at getting the input
	stream and the outputstream for the "connection" 

	There are still issues on what is the most common interface for the
	extreme cases of an interactive protocol like HTTP vs. events based
	one like SMTP/IMAP. This interface will probably go thru some 
	redesigning.

	-Gagan Saksena 03/15/99
*/

class nsIProtocolInstance : public nsISupports
{

public:
    
    /* 
        The GetInputStream function. Note that this function is not a const
		and calling it may result in a state change for the protocol instance.
		Its upto the protocol to reflect an already open input stream through
		the return methods. 
    */
    NS_IMETHOD          GetInputStream( nsIInputStream* *o_Stream) = 0;

    /* 
        The interrupt function for stopping an active instance.
    */
    NS_IMETHOD          Interrupt(void) = 0;

    /*
        The load function that makes the actual attempt to starts the 
        retrieval process. Depending upon the protocol's implementation
        this function may open the network connection.
    */
    NS_IMETHOD          Load(void) = 0;


    static const nsIID& GetIID() { 
        // {3594D180-CB85-11d2-A1BA-444553540000}
		static const nsIID NS_IProtocolInstance_IID = 
            { 0x3594d180, 0xcb85, 0x11d2, { 0xa1, 0xba, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0 } };
		return NS_IProtocolInstance_IID; 
	};

protected:

    /*
        Protocol's check for connection status. Once Load is called, this 
        should return true. TODO THINK

    NS_IMETHOD_(PRBool) IsConnected(void) = 0;
    */


};

//Possible errors
#define NS_ERROR_ALREADY_CONNECTED NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 150);

#endif /* _nsIProtocolInstance_h_ */
