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

#ifndef nsIStreamListener_h___
#define nsIStreamListener_h___

#include "nsIStreamObserver.h"

/* forward declaration */
class nsIInputStream;
class nsString;
class nsIURL;

struct nsStreamBindingInfo {
    PRBool      seekable;
    /* ...more... */
};

/* 45d234d0-c6c9-11d1-bea2-00805f8a66dc */
#define NS_ISTREAMLISTENER_IID   \
{ 0x45d234d0, 0xc6c9, 0x11d1, \
  {0xbe, 0xa2, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/**
 * The nsIStreamListener interface provides the necessary notifications 
 * during both synchronous and asynchronous URL loading.  
 * This is a preliminary interface which <B>will</B> change over time!
 * <BR><BR>
 * An instance of this interface is passed to nsINetService::Open(...) to
 * allow the client to receive status and notifications during the loading 
 * of the URL. 
 * <BR><BR>
 * Over time this interface will provide the same functionality as the 
 * IBindStatusCallback interface in the MS INET-SDK...
 */
class nsIStreamListener : public nsIStreamObserver {
public:
    /**
     * Return information regarding the current URL load.<BR>
     * The info structure that is passed in is filled out and returned
     * to the caller. 
     * 
     * This method is currently not called.  
     */
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) = 0;

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

#endif /* nsIStreamListener_h___ */
