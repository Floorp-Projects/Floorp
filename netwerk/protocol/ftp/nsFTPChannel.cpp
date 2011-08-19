/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sts=4 sw=4 et cin: */
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

#include "nsFTPChannel.h"
#include "nsFtpConnectionThread.h"  // defines nsFtpState

#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsReadableUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStreamConverterService.h"
#include "nsISocketTransport.h"
#include "nsURLHelper.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

////////////// this needs to move to nspr
static inline PRUint32
PRTimeToSeconds(PRTime t_usec)
{
    return PRUint32(t_usec / PR_USEC_PER_SEC);
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())
////////////// end


// There are two transport connections established for an 
// ftp connection. One is used for the command channel , and
// the other for the data channel. The command channel is the first
// connection made and is used to negotiate the second, data, channel.
// The data channel is driven by the command channel and is either
// initiated by the server (PORT command) or by the client (PASV command).
// Client initiation is the most common case and is attempted first.

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED4(nsFtpChannel,
                             nsBaseChannel,
                             nsIUploadChannel,
                             nsIResumableChannel,
                             nsIFTPChannel,
                             nsIProxiedChannel)

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpChannel::SetUploadStream(nsIInputStream *stream,
                              const nsACString &contentType,
                              PRInt32 contentLength)
{
    NS_ENSURE_TRUE(!IsPending(), NS_ERROR_IN_PROGRESS);

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
nsFtpChannel::ResumeAt(PRUint64 aStartPos, const nsACString& aEntityID)
{
    NS_ENSURE_TRUE(!IsPending(), NS_ERROR_IN_PROGRESS);
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
nsFtpChannel::OpenContentStream(PRBool async, nsIInputStream **result,
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

PRBool
nsFtpChannel::GetStatusArg(nsresult status, nsString &statusArg)
{
    nsCAutoString host;
    URI()->GetHost(host);
    CopyUTF8toUTF16(host, statusArg);
    return PR_TRUE;
}

void
nsFtpChannel::OnCallbacksChanged()
{
    mFTPEventSink = nsnull;
}

//-----------------------------------------------------------------------------

namespace {

class FTPEventSinkProxy : public nsIFTPEventSink
{
public:
    FTPEventSinkProxy(nsIFTPEventSink* aTarget)
        : mTarget(aTarget)
        , mTargetThread(do_GetCurrentThread())
    { }
        
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFTPEVENTSINK

    class OnFTPControlLogRunnable : public nsRunnable
    {
    public:
        OnFTPControlLogRunnable(nsIFTPEventSink* aTarget,
                                PRBool aServer,
                                const char* aMessage)
            : mTarget(aTarget)
            , mServer(aServer)
            , mMessage(aMessage)
        { }

        NS_DECL_NSIRUNNABLE

    private:
        nsCOMPtr<nsIFTPEventSink> mTarget;
        PRBool mServer;
        nsCString mMessage;
    };

private:
    nsCOMPtr<nsIFTPEventSink> mTarget;
    nsCOMPtr<nsIThread> mTargetThread;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(FTPEventSinkProxy, nsIFTPEventSink)

NS_IMETHODIMP
FTPEventSinkProxy::OnFTPControlLog(PRBool aServer, const char* aMsg)
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

} // anonymous namespace

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
