/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Furman, fur@netscape.com
 */

#ifndef _nsCacheEntryChannel_h_
#define _nsCacheEntryChannel_h_

#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsCachedNetData.h"
#include "nsILoadGroup.h"

class nsIStreamListener;

// A proxy for an nsIChannel, useful when only a few nsIChannel
// methods must be overridden
class nsChannelProxy : public nsIChannel {

public:
    NS_FORWARD_NSICHANNEL(mChannel->)
    NS_FORWARD_NSIREQUEST(mChannel->)

protected:
    nsChannelProxy(nsIChannel* aChannel):mChannel(aChannel) {};
    virtual ~nsChannelProxy() {};
    nsCOMPtr<nsIChannel>      mChannel;
};

// Override several nsIChannel methods so that they interact with the cache manager
class nsCacheEntryChannel : public nsChannelProxy {

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD GetTransferOffset(PRUint32 *aStartPosition);
    NS_IMETHOD SetTransferOffset(PRUint32 aStartPosition);
    NS_IMETHOD GetTransferCount(PRInt32 *aReadCount);
    NS_IMETHOD SetTransferCount(PRInt32 aReadCount);
    NS_IMETHOD OpenOutputStream(nsIOutputStream* *aOutputStream);
    NS_IMETHOD OpenInputStream(nsIInputStream* *aInputStream);
    NS_IMETHOD AsyncRead(nsIStreamListener *aListener, nsISupports *aContext);
    NS_IMETHOD AsyncWrite(nsIInputStream *aFromStream,
                          nsIStreamObserver *aObserver, nsISupports *aContext);
    NS_IMETHOD GetLoadAttributes(nsLoadFlags *aLoadAttributes);
    NS_IMETHOD SetLoadAttributes(nsLoadFlags aLoadAttributes);
    NS_IMETHOD GetLoadGroup(nsILoadGroup* *aLoadGroup);
    NS_IMETHOD GetURI(nsIURI * *aURI);
    NS_IMETHOD GetOriginalURI(nsIURI * *aURI);

protected:
    nsCacheEntryChannel(nsCachedNetData* aCacheEntry, nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
    virtual ~nsCacheEntryChannel();

    friend class nsCachedNetData;

private:
    nsCOMPtr<nsCachedNetData>    mCacheEntry;
    nsCOMPtr<nsILoadGroup>       mLoadGroup;
};

#endif // _nsCacheEntryChannel_h_
