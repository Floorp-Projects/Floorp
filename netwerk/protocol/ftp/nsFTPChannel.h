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
#include "nsIChannelWithDivertableParentListener.h"
#include "nsIFTPChannel.h"
#include "nsIForcePendingChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxyInfo.h"
#include "nsIProxiedChannel.h"
#include "nsIResumableChannel.h"

class nsIURI;
using mozilla::net::ADivertableParentChannel;

class nsFtpChannel final : public nsBaseChannel,
                           public nsIFTPChannel,
                           public nsIUploadChannel,
                           public nsIResumableChannel,
                           public nsIProxiedChannel,
                           public nsIForcePendingChannel,
                           public nsIChannelWithDivertableParentListener
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIUPLOADCHANNEL
    NS_DECL_NSIRESUMABLECHANNEL
    NS_DECL_NSIPROXIEDCHANNEL
    NS_DECL_NSICHANNELWITHDIVERTABLEPARENTLISTENER

    nsFtpChannel(nsIURI *uri, nsIProxyInfo *pi)
        : mProxyInfo(pi)
        , mStartPos(0)
        , mResumeRequested(false)
        , mLastModifiedTime(0)
        , mForcePending(false)
    {
        SetURI(uri);
    }

    void UpdateURI(nsIURI *aURI) {
        MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread(), "Not thread-safe.");
        mURI = aURI;
    }

    nsIProxyInfo *ProxyInfo() {
        return mProxyInfo;
    }

    void SetProxyInfo(nsIProxyInfo *pi)
    {
        mProxyInfo = pi;
    }

    NS_IMETHOD IsPending(bool *result) override;

    // This is a short-cut to calling nsIRequest::IsPending().
    // Overrides Pending in nsBaseChannel.
    bool Pending() const override;

    // Were we asked to resume a download?
    bool ResumeRequested() { return mResumeRequested; }

    // Download from this byte offset
    uint64_t StartPos() { return mStartPos; }

    // ID of the entity to resume downloading
    const nsCString &EntityID() {
        return mEntityID;
    }
    void SetEntityID(const nsACString& entityID) {
        mEntityID = entityID;
    }

    NS_IMETHOD GetLastModifiedTime(PRTime* lastModifiedTime) override {
        *lastModifiedTime = mLastModifiedTime;
        return NS_OK;
    }

    NS_IMETHOD SetLastModifiedTime(PRTime lastModifiedTime) override {
        mLastModifiedTime = lastModifiedTime;
        return NS_OK;
    }

    // Data stream to upload
    nsIInputStream *UploadStream() {
        return mUploadStream;
    }

    // Helper function for getting the nsIFTPEventSink.
    void GetFTPEventSink(nsCOMPtr<nsIFTPEventSink> &aResult);

    NS_IMETHOD Suspend() override;
    NS_IMETHOD Resume() override;

public:
    NS_IMETHOD ForcePending(bool aForcePending) override;

protected:
    virtual ~nsFtpChannel() = default;
    virtual nsresult OpenContentStream(bool async, nsIInputStream **result,
                                       nsIChannel** channel) override;
    virtual bool GetStatusArg(nsresult status, nsString &statusArg) override;
    virtual void OnCallbacksChanged() override;

private:
    nsCOMPtr<nsIProxyInfo>           mProxyInfo;
    nsCOMPtr<nsIFTPEventSink>        mFTPEventSink;
    nsCOMPtr<nsIInputStream>         mUploadStream;
    uint64_t                         mStartPos;
    nsCString                        mEntityID;
    bool                             mResumeRequested;
    PRTime                           mLastModifiedTime;
    bool                             mForcePending;
    RefPtr<ADivertableParentChannel> mParentChannel;

    // Current suspension depth for this channel object
    uint32_t                          mSuspendCount;
};

#endif /* nsFTPChannel_h___ */
