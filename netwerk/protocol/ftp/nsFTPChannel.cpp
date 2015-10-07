/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sts=4 sw=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFTPChannel.h"
#include "nsFtpConnectionThread.h"  // defines nsFtpState

#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"

extern PRLogModuleInfo* gFTPLog;

// There are two transport connections established for an 
// ftp connection. One is used for the command channel , and
// the other for the data channel. The command channel is the first
// connection made and is used to negotiate the second, data, channel.
// The data channel is driven by the command channel and is either
// initiated by the server (PORT command) or by the client (PASV command).
// Client initiation is the most common case and is attempted first.

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED(nsFtpChannel,
                            nsBaseChannel,
                            nsIUploadChannel,
                            nsIResumableChannel,
                            nsIFTPChannel,
                            nsIProxiedChannel,
                            nsIForcePendingChannel)

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpChannel::SetUploadStream(nsIInputStream *stream,
                              const nsACString &contentType,
                              int64_t contentLength)
{
    NS_ENSURE_TRUE(!Pending(), NS_ERROR_IN_PROGRESS);

    mUploadStream = stream;

    // NOTE: contentLength is intentionally ignored here.
 
    return NS_OK;
}

NS_IMETHODIMP
nsFtpChannel::GetUploadStream(nsIInputStream **stream)
{
    NS_ENSURE_ARG_POINTER(stream);
    *stream = mUploadStream;
    NS_IF_ADDREF(*stream);
    return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpChannel::ResumeAt(uint64_t aStartPos, const nsACString& aEntityID)
{
    NS_ENSURE_TRUE(!Pending(), NS_ERROR_IN_PROGRESS);
    mEntityID = aEntityID;
    mStartPos = aStartPos;
    mResumeRequested = (mStartPos || !mEntityID.IsEmpty());
    return NS_OK;
}

NS_IMETHODIMP
nsFtpChannel::GetEntityID(nsACString& entityID)
{
    if (mEntityID.IsEmpty())
      return NS_ERROR_NOT_RESUMABLE;

    entityID = mEntityID;
    return NS_OK;
}

//-----------------------------------------------------------------------------
NS_IMETHODIMP
nsFtpChannel::GetProxyInfo(nsIProxyInfo** aProxyInfo)
{
    *aProxyInfo = ProxyInfo();
    NS_IF_ADDREF(*aProxyInfo);
    return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult
nsFtpChannel::OpenContentStream(bool async, nsIInputStream **result,
                                nsIChannel** channel)
{
    if (!async)
        return NS_ERROR_NOT_IMPLEMENTED;

    nsFtpState *state = new nsFtpState();
    if (!state)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(state);

    nsresult rv = state->Init(this);
    if (NS_FAILED(rv)) {
        NS_RELEASE(state);
        return rv;
    }

    *result = state;
    return NS_OK;
}

bool
nsFtpChannel::GetStatusArg(nsresult status, nsString &statusArg)
{
    nsAutoCString host;
    URI()->GetHost(host);
    CopyUTF8toUTF16(host, statusArg);
    return true;
}

void
nsFtpChannel::OnCallbacksChanged()
{
    mFTPEventSink = nullptr;
}

//-----------------------------------------------------------------------------

namespace {

class FTPEventSinkProxy final : public nsIFTPEventSink
{
    ~FTPEventSinkProxy() {}

public:
    explicit FTPEventSinkProxy(nsIFTPEventSink* aTarget)
        : mTarget(aTarget)
        , mTargetThread(do_GetCurrentThread())
    { }
        
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIFTPEVENTSINK

    class OnFTPControlLogRunnable : public nsRunnable
    {
    public:
        OnFTPControlLogRunnable(nsIFTPEventSink* aTarget,
                                bool aServer,
                                const char* aMessage)
            : mTarget(aTarget)
            , mServer(aServer)
            , mMessage(aMessage)
        { }

        NS_DECL_NSIRUNNABLE

    private:
        nsCOMPtr<nsIFTPEventSink> mTarget;
        bool mServer;
        nsCString mMessage;
    };

private:
    nsCOMPtr<nsIFTPEventSink> mTarget;
    nsCOMPtr<nsIThread> mTargetThread;
};

NS_IMPL_ISUPPORTS(FTPEventSinkProxy, nsIFTPEventSink)

NS_IMETHODIMP
FTPEventSinkProxy::OnFTPControlLog(bool aServer, const char* aMsg)
{
    nsRefPtr<OnFTPControlLogRunnable> r =
        new OnFTPControlLogRunnable(mTarget, aServer, aMsg);
    return mTargetThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
FTPEventSinkProxy::OnFTPControlLogRunnable::Run()
{
    mTarget->OnFTPControlLog(mServer, mMessage.get());
    return NS_OK;
}

} // namespace

void
nsFtpChannel::GetFTPEventSink(nsCOMPtr<nsIFTPEventSink> &aResult)
{
    if (!mFTPEventSink) {
        nsCOMPtr<nsIFTPEventSink> ftpSink;
        GetCallback(ftpSink);
        if (ftpSink) {
            mFTPEventSink = new FTPEventSinkProxy(ftpSink);
        }
    }
    aResult = mFTPEventSink;
}

NS_IMETHODIMP
nsFtpChannel::ForcePending(bool aForcePending)
{
    // Set true here so IsPending will return true.
    // Required for callback diversion from child back to parent. In such cases
    // OnStopRequest can be called in the parent before callbacks are diverted
    // back from the child to the listener in the parent.
    mForcePending = aForcePending;

    return NS_OK;
}

NS_IMETHODIMP
nsFtpChannel::IsPending(bool *result)
{
  *result = Pending();
  return NS_OK;
}

bool
nsFtpChannel::Pending() const
{
  return nsBaseChannel::Pending() || mForcePending;
}
