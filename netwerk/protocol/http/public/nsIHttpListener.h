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

#ifndef nsIHttpListener_h___
#define nsIHttpListener_h___

// XXX maybe we should inherit from this:
#include "nsIStreamListener.h"  // same error codes
#include "nscore.h"

class nsIUrl;

// XXX regenerate:
#define NS_IHTTPLISTENER_IID                         \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x85, 0x6a, 0x00, 0x60, 0xb8, 0x11, 0x9d, 0x7a} \
}

class nsIHttpListener : public nsIHttpObserver
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTTPLISTENER_IID);

    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param pIStream  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the stream.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                               PRUint32 aLength) = 0;

};

#endif /* nsIIHttpListener_h___ */
