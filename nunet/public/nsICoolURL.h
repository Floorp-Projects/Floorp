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
