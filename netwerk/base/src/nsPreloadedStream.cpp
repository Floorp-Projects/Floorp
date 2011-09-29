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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
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

#include "nsPreloadedStream.h"
#include "nsIRunnable.h"

#include "nsThreadUtils.h"
#include "nsAlgorithm.h"
#include "prmem.h"
   
namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPreloadedStream,
                              nsIInputStream,
                              nsIAsyncInputStream)

nsPreloadedStream::nsPreloadedStream(nsIAsyncInputStream *aStream,
                                     const char *data, PRUint32 datalen)
    : mStream(aStream),
      mOffset(0),
      mLen(datalen)
{
    mBuf = (char *) moz_xmalloc(datalen);
    memcpy(mBuf, data, datalen);
}

nsPreloadedStream::~nsPreloadedStream()
{
    moz_free(mBuf);
}

NS_IMETHODIMP
nsPreloadedStream::Close()
{
    mLen = 0;
    return mStream->Close();
}


NS_IMETHODIMP
nsPreloadedStream::Available(PRUint32 *_retval NS_OUTPARAM)
{
    PRUint32 avail = 0;
    
    nsresult rv = mStream->Available(&avail);
    if (NS_FAILED(rv))
        return rv;
    *_retval = avail + mLen;
    return NS_OK;
}

NS_IMETHODIMP
nsPreloadedStream::Read(char *aBuf, PRUint32 aCount,
                        PRUint32 *_retval NS_OUTPARAM)
{
    if (!mLen)
        return mStream->Read(aBuf, aCount, _retval);
    
    PRUint32 toRead = NS_MIN(mLen, aCount);
    memcpy(aBuf, mBuf + mOffset, toRead);
    mOffset += toRead;
    mLen -= toRead;
    *_retval = toRead;
    return NS_OK;
}

NS_IMETHODIMP
nsPreloadedStream::ReadSegments(nsWriteSegmentFun aWriter,
                                void *aClosure, PRUint32 aCount,
                                PRUint32 *result)
{
    if (!mLen)
        return mStream->ReadSegments(aWriter, aClosure, aCount, result);

    *result = 0;
    while (mLen > 0 && aCount > 0) {
        PRUint32 toRead = NS_MIN(mLen, aCount);
        PRUint32 didRead = 0;
        nsresult rv;

        rv = aWriter(this, aClosure, mBuf + mOffset, *result, toRead, &didRead);

        if (NS_FAILED(rv))
            return NS_OK;

        *result += didRead;
        mOffset += didRead;
        mLen -= didRead;
        aCount -= didRead;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsPreloadedStream::IsNonBlocking(bool *_retval NS_OUTPARAM)
{
    return mStream->IsNonBlocking(_retval);
}

NS_IMETHODIMP
nsPreloadedStream::CloseWithStatus(nsresult aStatus)
{
    mLen = 0;
    return mStream->CloseWithStatus(aStatus);
}

class RunOnThread : public nsRunnable
{
public:
    RunOnThread(nsIAsyncInputStream *aStream,
                nsIInputStreamCallback *aCallback)
      : mStream(aStream),
        mCallback(aCallback) {}
    
    virtual ~RunOnThread() {}
    
    NS_IMETHOD Run()
    {
        mCallback->OnInputStreamReady(mStream);
        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncInputStream>    mStream;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
};

NS_IMETHODIMP
nsPreloadedStream::AsyncWait(nsIInputStreamCallback *aCallback,
                             PRUint32 aFlags,
                             PRUint32 aRequestedCount,
                             nsIEventTarget *aEventTarget)
{
    if (!mLen)
        return mStream->AsyncWait(aCallback, aFlags, aRequestedCount,
                                  aEventTarget);

    if (!aCallback)
        return NS_OK;

    if (!aEventTarget)
        return aCallback->OnInputStreamReady(this);

    nsCOMPtr<nsIRunnable> event =
        new RunOnThread(this, aCallback);
    return aEventTarget->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
}

} // namespace net
} // namespace mozilla
