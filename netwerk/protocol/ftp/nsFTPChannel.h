/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
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
 *   Doug Turner <dougt@meer.net> (original author)
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsFTPChannel_h___
#define nsFTPChannel_h___

#include "nsBaseChannel.h"

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
#include "nsIStreamListener.h"
#include "nsIFTPChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxyInfo.h"
#include "nsIProxiedChannel.h"
#include "nsIResumableChannel.h"
#include "nsHashPropertyBag.h"
#include "nsFtpProtocolHandler.h"

class nsFtpChannel : public nsBaseChannel,
                     public nsIFTPChannel,
                     public nsIUploadChannel,
                     public nsIResumableChannel,
                     public nsIProxiedChannel
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIUPLOADCHANNEL
    NS_DECL_NSIRESUMABLECHANNEL
    NS_DECL_NSIPROXIEDCHANNEL
    
    nsFtpChannel(nsIURI *uri, nsIProxyInfo *pi)
        : mProxyInfo(pi)
        , mStartPos(0)
        , mResumeRequested(PR_FALSE)
        , mLastModifiedTime(0)
    {
        SetURI(uri);
    }

    nsIProxyInfo *ProxyInfo() {
        return mProxyInfo;
    }

    // Were we asked to resume a download?
    bool ResumeRequested() { return mResumeRequested; }

    // Download from this byte offset
    PRUint64 StartPos() { return mStartPos; }

    // ID of the entity to resume downloading
    const nsCString &EntityID() {
        return mEntityID;
    }
    void SetEntityID(const nsCSubstring &entityID) {
        mEntityID = entityID;
    }

    NS_IMETHODIMP GetLastModifiedTime(PRTime* lastModifiedTime) {
        *lastModifiedTime = mLastModifiedTime;
        return NS_OK;
    }

    NS_IMETHODIMP SetLastModifiedTime(PRTime lastModifiedTime) {
        mLastModifiedTime = lastModifiedTime;
        return NS_OK;
    }

    // Data stream to upload
    nsIInputStream *UploadStream() {
        return mUploadStream;
    }

    // Helper function for getting the nsIFTPEventSink.
    void GetFTPEventSink(nsCOMPtr<nsIFTPEventSink> &aResult);

protected:
    virtual ~nsFtpChannel() {}
    virtual nsresult OpenContentStream(bool async, nsIInputStream **result,
                                       nsIChannel** channel);
    virtual bool GetStatusArg(nsresult status, nsString &statusArg);
    virtual void OnCallbacksChanged();

private:
    nsCOMPtr<nsIProxyInfo>    mProxyInfo; 
    nsCOMPtr<nsIFTPEventSink> mFTPEventSink;
    nsCOMPtr<nsIInputStream>  mUploadStream;
    PRUint64                  mStartPos;
    nsCString                 mEntityID;
    bool                      mResumeRequested;
    PRTime                    mLastModifiedTime;
};

#endif /* nsFTPChannel_h___ */
