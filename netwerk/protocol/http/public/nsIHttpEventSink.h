/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef _nsIHTTPEventSink_h_
#define _nsIHTTPEventSink_h_

#include "nsISupports.h"

/* 
    The nsIHTTPEventSink class is the interface thru which the
    HTTP handler reports specific http events back to a client. 

    -Gagan Saksena 02/25/99
*/
class nsIHTTPEventSink : public nsISupports
{

public:
    
    /*
        OnAwaitingInput gets called when the handler is waiting on some info from the 
        app. Ex. username password dialogs, etc. You must at the very least call resume
        on the connection for this to continue.
    */
    NS_IMETHOD      OnAwaitingInput(nsISupports* i_Context) = 0;

    NS_IMETHOD      OnHeadersAvailable(nsISupports* i_Context) = 0;
    
    NS_IMETHOD      OnProgress(nsISupports* i_Context, 
                            PRUint32 i_Progress, 
                            PRUint32 i_ProgressMax) = 0;
    
    // OnRedirect gets fired only if you have set FollowRedirects on the handler!
    NS_IMETHOD      OnRedirect(nsISupports* i_Context, 
                               nsIURI* i_NewLocation) =0;

    static const nsIID& GetIID() { 
        // {E4F981C0-098F-11d3-B01A-006097BFC036}
        static const nsIID NS_IHTTPEventSink_IID = 
        { 0xe4f981c0, 0x98f, 0x11d3, { 0xb0, 0x1a, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };
        return NS_IHTTPEventSink_IID; 
    };

};

#endif /* _nsIHTTPEventSink_h_ */
