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

#ifndef _nsIHTTPEventSink_h_
#define _nsIHTTPEventSink_h_

#include "nsIStreamListener.h"
class nsIString;
/* 
    The nsIHTTPEventSink class is the interface thru which the
    HTTP handler reports specific http events back to a client. 

    -Gagan Saksena 02/25/99
*/
class nsIHTTPEventSink : public nsIStreamListener
{

public:
    
    /*
        OnAwaitingInput gets called when the handler is waiting on some info from the 
        app. Ex. username password dialogs, etc. You must at the very least call resume
        on the connection for this to continue.
    */
    NS_IMETHOD      OnAwaitingInput(nsISupports* i_Context) = 0;

    NS_IMETHOD      OnDataAvailable(nsISupports* i_Context, 
                            nsIInputStream *i_IStream,
                            PRUint32 i_SourceOffset,
                            PRUint32 i_Length) = 0;

    NS_IMETHOD      OnHeadersAvailable(nsISupports* i_Context) = 0;
    
    NS_IMETHOD      OnProgress(nsISupports* i_Context, 
                            PRUint32 i_Progress, 
                            PRUint32 i_ProgressMax) = 0;
    
    // OnRedirect gets fired only if you have set FollowRedirects on the handler!
    NS_IMETHOD      OnRedirect(nsISupports* i_Context, 
                            nsIUrl* i_NewLocation) =0;

    NS_IMETHOD      OnStopBinding(nsISupports* i_Context, 
                            nsresult i_Status, 
                            const nsIString* i_pMsg) = 0;
    
    static const nsIID& GetIID() { 
        // {E4F981C0-098F-11d3-B01A-006097BFC036}
        static const nsIID NS_IHTTPEventSink_IID = 
            { 0xe4f981c0, 0x98f, 0x11d3, { 0xb0, 0x1a, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };
		return NS_IHTTPEventSink_IID; 
	};

};

#endif /* _nsIHTTPEventSink_h_ */
