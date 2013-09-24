/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFTPChannel_h___
#define nsFTPChannel_h___

#include "nsBaseChannel.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIFTPChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxyInfo.h"
#include "nsIProxiedChannel.h"
#include "nsIResumableChannel.h"

class nsIURI;

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
        , mResumeRequested(false)
        , mLastModifiedTime(0)
    {
        SetURI(uri);
    }

    nsIProxyInfo *ProxyInfo() {
        return mProxyInfo;
    }

    void SetProxyInfo(nsIProxyInfo *pi)
    {
        mProxyInfo = pi;
    }

    // Were we asked to resume a download?
    bool ResumeRequested() { return mResumeRequested; }

    // Download from this byte offset
    uint64_t StartPos() { return mStartPos; }

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
    uint64_t                  mStartPos;
    nsCString                 mEntityID;
    bool                      mResumeRequested;
    PRTime                    mLastModifiedTime;
};

#endif /* nsFTPChannel_h___ */
