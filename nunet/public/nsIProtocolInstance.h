/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
