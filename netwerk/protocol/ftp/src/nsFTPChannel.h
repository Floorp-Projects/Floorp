/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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

// ftp implementation header

#ifndef nsFTPChannel_h___
#define nsFTPChannel_h___

#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIProtocolHandler.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsFtpConnectionThread.h"
#include "netCore.h"
#include "nsXPIDLString.h"
#include "nsIStreamListener.h"
#include "nsAutoLock.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIFTPChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxyInfo.h"
#include "nsIResumableChannel.h"
#include "nsIResumableEntityID.h"
#include "nsIDirectoryListing.h"

#include "nsICacheService.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICacheListener.h"
#include "nsICacheSession.h"

#define FTP_COMMAND_CHANNEL_SEG_SIZE 64
#define FTP_COMMAND_CHANNEL_SEG_COUNT 8

#define FTP_DATA_CHANNEL_SEG_SIZE  (4*1024)
#define FTP_DATA_CHANNEL_SEG_COUNT 8

#define FTP_CACHE_CONTROL_CONNECTION 1

class nsFTPChannel : public nsIFTPChannel,
                     public nsIUploadChannel,
                     public nsIInterfaceRequestor,
                     public nsIProgressEventSink,
                     public nsIStreamListener, 
                     public nsICacheListener,
                     public nsIResumableChannel,
                     public nsIDirectoryListing
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIUPLOADCHANNEL
    NS_DECL_NSIFTPCHANNEL
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSICACHELISTENER
    NS_DECL_NSIRESUMABLECHANNEL
    NS_DECL_NSIDIRECTORYLISTING
    
    // nsFTPChannel methods:
    nsFTPChannel();
    virtual ~nsFTPChannel();
    
    // initializes the channel. 
    nsresult Init(nsIURI* uri,
                  nsIProxyInfo* proxyInfo,
                  nsICacheSession* session);

    nsresult SetupState(PRUint32 startPos, nsIResumableEntityID* entityID);
    nsresult GenerateCacheKey(nsACString &cacheKey);
    
    nsresult AsyncOpenAt(nsIStreamListener *listener, nsISupports *ctxt,
                         PRUint64 startPos, nsIResumableEntityID* entityID);

protected:
    nsCOMPtr<nsIURI>                mOriginalURI;
    nsCOMPtr<nsIURI>                mURL;
    
    nsCOMPtr<nsIInputStream>        mUploadStream;

    // various callback interfaces
    nsCOMPtr<nsIProgressEventSink>  mEventSink;
    nsCOMPtr<nsIPrompt>             mPrompter;
    nsCOMPtr<nsIFTPEventSink>       mFTPEventSink;
    nsCOMPtr<nsIAuthPrompt>         mAuthPrompter;
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

    PRBool                          mIsPending;
    PRUint32                        mLoadFlags;
    PRUint32                        mListFormat;

    PRUint32                        mSourceOffset;
    PRInt32                         mAmount;
    nsCOMPtr<nsILoadGroup>          mLoadGroup;
    nsCString                       mContentType;
    nsCString                       mContentCharset;
    PRInt32                         mContentLength;
    nsCOMPtr<nsISupports>           mOwner;

    nsCOMPtr<nsIStreamListener>     mListener;

    nsFtpState*                     mFTPState;   

    nsCString                       mHost;
    PRLock*                         mLock;
    nsCOMPtr<nsISupports>           mUserContext;
    nsresult                        mStatus;
    PRPackedBool                    mCanceled;

    nsCOMPtr<nsIIOService>          mIOService;

    nsCOMPtr<nsICacheSession>         mCacheSession;
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsCOMPtr<nsIProxyInfo>            mProxyInfo;
    nsCOMPtr<nsIResumableEntityID>    mEntityID;
    PRUint64                          mStartPos;
};

#endif /* nsFTPChannel_h___ */
