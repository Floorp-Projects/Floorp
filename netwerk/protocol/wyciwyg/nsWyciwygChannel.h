/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsWyciwygChannel_h___
#define nsWyciwygChannel_h___

#include "nsWyciwygProtocolHandler.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsIWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsICacheListener.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIURI.h"

extern PRLogModuleInfo * gWyciwygLog;

//-----------------------------------------------------------------------------

class nsWyciwygChannel: public nsIWyciwygChannel,
                        public nsIStreamListener,
                        public nsICacheListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIWYCIWYGCHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICACHELISTENER

    // nsWyciwygChannel methods:
    nsWyciwygChannel();
    virtual ~nsWyciwygChannel();

    nsresult Init(nsIURI *uri);

protected:
    nsresult ReadFromCache();
    nsresult OpenCacheEntry(const nsACString & aCacheKey, nsCacheAccessMode aWriteAccess, PRBool * aDelayFlag = nsnull);

    void WriteCharsetAndSourceToCache(PRInt32 aSource,
                                      const nsCString& aCharset);

    void NotifyListener();
       
    nsresult                            mStatus;
    PRPackedBool                        mIsPending;
    PRPackedBool                        mNeedToWriteCharset;
    PRInt32                             mCharsetSource;
    nsCString                           mCharset;
    PRInt32                             mContentLength;
    PRUint32                            mLoadFlags;
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsISupports>               mListenerContext;

    // reuse as much of this channel implementation as we can
    nsCOMPtr<nsIInputStreamPump>        mPump;
    
    // Cache related stuff    
    nsCOMPtr<nsICacheEntryDescriptor>   mCacheEntry;
    nsCOMPtr<nsIOutputStream>           mCacheOutputStream;
    nsCOMPtr<nsIInputStream>            mCacheInputStream;

    nsCOMPtr<nsISupports>               mSecurityInfo;
};

#endif /* nsWyciwygChannel_h___ */
