/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsINetService_h___
#define nsINetService_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"

 /* XXX: This should be moved to ns/xpcom/src/nserror.h */
#define NS_OK    0
#define NS_FALSE 1


/* cfb1a480-c78f-11d1-bea2-00805f8a66dc */
#define NS_INETSERVICE_IID   \
{ 0xcfb1a480, 0xc78f, 0x11d1, \
  {0xbe, 0xa2, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }


/**
 * The nsINetService interface provides an API to the networking service.
 * This is a preliminary interface which <B>will</B> change over time!
 * 
 */
struct nsINetService : public nsISupports 
{
    /**
     * Initiate an asynchronous URL load.<BR><BR>
     *
     * @param aUrl      The URL to load.
     * @param aConsumer An object that will receive notifications during the 
     *                  URL loading.  This parameter cannot be NULL.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */ 
    NS_IMETHOD OpenStream(nsIURL *aUrl, 
                          nsIStreamListener *aConsumer) = 0;

    /**
     * Initiate a synchronous URL load.<BR><BR>
     * 
     * @param aUrl      The URL to load.
     * @param aConsumer An object that will receive notifications during the 
     *                  URL loading.  This parameter can be NULL if 
     *                  notifications are not required.
     * @param aNewStream    An output parameter to recieve the blocking stream
     *                      created for this URL load.
     * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
     */
    NS_IMETHOD OpenBlockingStream(nsIURL *aUrl, 
                                  nsIStreamListener *aConsumer,
                                  nsIInputStream **aNewStream) = 0;
};


/**
 * Create an instance of the INetService
 *
 */
extern "C" NS_NET nsresult NS_NewINetService(nsINetService** aInstancePtrResult,
                                             nsISupports* aOuter);

#endif /* nsINetService_h___ */
