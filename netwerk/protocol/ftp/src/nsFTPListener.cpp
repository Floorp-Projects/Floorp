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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFTPListener.h"

NS_IMPL_ISUPPORTS2(nsFTPListener, nsIStreamListener, nsIStreamObserver);

// nsFTPListener methods

nsFTPListener::nsFTPListener(nsIStreamListener *aListener, nsIChannel *aChannel) {
    NS_INIT_REFCNT();
    NS_ASSERTION(aListener && aChannel, "null ptr");
    mListener = aListener;
    mFTPChannel = aChannel;
}

nsFTPListener::~nsFTPListener() {
    mListener = 0;
    mFTPChannel = 0;
}

// nsIStreamObserver methods.
NS_IMETHODIMP
nsFTPListener::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext,
                                      nsresult aStatus, const PRUnichar* aMsg) {
    return mListener->OnStopRequest(mFTPChannel, aContext, aStatus, aMsg);

}

NS_IMETHODIMP
nsFTPListener::OnStartRequest(nsIChannel *aChannel, nsISupports *aContext) {
    return mListener->OnStartRequest(mFTPChannel, aContext);
}


// nsIStreamListener methods
NS_IMETHODIMP
nsFTPListener::OnDataAvailable(nsIChannel* aChannel, nsISupports* aContext,
                               nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                               PRUint32 aLength) {
    return mListener->OnDataAvailable(mFTPChannel, aContext, aInputStream, aSourceOffset, aLength);
}
