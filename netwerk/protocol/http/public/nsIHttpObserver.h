/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIHttpObserver_h___
#define nsIHttpObserver_h___

// XXX maybe we should inherit from this:
#include "nsIStreamListener.h"  // same error codes
#include "nscore.h"

class nsIUrl;

// XXX regenerate:
#define NS_IHTTPOBSERVER_IID                         \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x85, 0x6a, 0x00, 0x60, 0xb8, 0x11, 0x9d, 0x7a} \
}

class nsIHttpObserver : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTTPOBSERVER_IID);

    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @return The return value is currently ignored.  In the future it may be
     * used to cancel the URL load..
     */
    NS_IMETHOD OnStartBinding(nsIUrl* aURL, const char *aContentType) = 0;

    /**
     * Return information regarding the current URL load.<BR>
     * The info structure that is passed in is filled out and returned
     * to the caller. 
     * 
     * This method is currently not called.  
     */
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) = 0;

    /**
     * Notify the observer that progress as occurred for the URL load.<BR>
     */
    NS_IMETHOD OnProgress(nsIUrl* aURL, PRUint32 aProgress, PRUint32 aProgressMax) = 0;

    /**
     * Notify the observer with a status message for the URL load.<BR>
     */
    NS_IMETHOD OnStatus(nsIUrl* aURL, const PRUnichar* aMsg) = 0;

    /**
     * Notify the observer that the URL has finished loading.  This method is 
     * called once when the networking library has finished processing the 
     * URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
     * 
     * This method is called regardless of whether the URL loaded successfully.<BR><BR>
     * 
     * @param status    Status code for the URL load.
     * @param msg   A text string describing the error.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD OnStopBinding(nsIUrl* aURL, nsresult aStatus, const PRUnichar* aMsg) = 0;

};

#endif /* nsIIHttpObserver_h___ */
