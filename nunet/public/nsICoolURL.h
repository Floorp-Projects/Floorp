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

#ifndef _nsICoolURL_h_
#define _nsICoolURL_h_

/* 
    The nsICoolURL class is an interface to the URL behaviour. 
    This follows the URL spec at-
    
         http://www.w3.org/Addressing/URL/url-spec.txt
    
    For the purpose of this class, here is the most elaborate form of a URL
    and its corresponding parts-
    
         ftp://username:password@hostname:portnumber/pathname
         \ /   \               / \      / \        /\       /
          -     ---------------   ------   --------  -------
          |            |             |        |         |
          |            |             |        |        Path
          |            |             |       Port         
          |            |            Host
          |         PreHost            
        Scheme

    The URL class extends the URI behaviour by providing a mechanism 
    for consumers to retreive (and on protocol specific levels put) 
	documents defined by the URI.

	Functions missing from this file are the stream listener portions
	for asynch operations. TODO. 

	Please send me a mail before you add anything to this file. 
	Thanks!- Gagan Saksena

 */

#include "nsIURI.h"
#include "nsIInputStream.h"
#include "nsIProtocolInstance.h"

class nsICoolURL : public nsIURI
{

public:
    
    //Core action functions
    /* 
        Note: The GetStream function also opens a connection using 
        the available information in the URL. This is the same as 
        calling OpenInputStream on the connection returned from 
        OpenProtocolInstance. Note that this stream doesn't contain any 
        header information either. 
    */
    NS_IMETHOD          GetStream(nsIInputStream* *o_InputStream) = 0;

#if 0 //Will go away...
    /*
        The GetDocument function BLOCKS till the data is made available
        or an error condition is encountered. The return type is the overall
        success status which depends on the protocol implementation. 
        Its just a convenience function that internally sets up a temp 
		stream in memory and buffers everything. Note that this 
		mechanism strips off the headers and only the raw data is 
		copied to the passed string.

        TODO - return status? 
    */
    NS_IMETHOD          GetDocument(const char* *o_Data) = 0;
#endif

    /* 
        The OpenProtocolInstance method sets up the connection as decided by the 
        protocol implementation. This may then be used to get various 
        connection specific details like the input and the output streams 
        associated with the connection, or the header information by querying
        on the connection type which will be protocol specific.
    */
    NS_IMETHOD          OpenProtocolInstance(nsIProtocolInstance* *o_ProtocolInstance) = 0;

#ifdef DEBUG
    NS_IMETHOD          DebugString(const char* *o_URLString) const=0;
#endif //DEBUG

    static const nsIID& GetIID() { 
        // {6DA32F00-BD64-11d2-B00F-006097BFC036}
        static const nsIID NS_ICOOLURL_IID = 
        { 0x6da32f00, 0xbd64, 0x11d2, { 0xb0, 0xf, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };
        return NS_ICOOLURL_IID; 
    };
};

// Should change to NS_METHOD CreateURL(nsICoolURL* *o_URL, const char* i_URL)
// and be placed where?
extern NS_NET
    nsICoolURL* CreateURL(const char* i_URL);


#endif /* _nsICoolURL_h_ */
