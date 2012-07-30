/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDownloader.h"
#include "nsICachingChannel.h"
#include "nsIInputStream.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNetUtil.h"
#include "nsCRTGlue.h"

nsDownloader::~nsDownloader()
{
    if (mLocation && mLocationIsTemp) {
        // release the sink first since it may still hold an open file
        // descriptor to mLocation.  this needs to happen before the
        // file can be removed otherwise the Remove call will fail.
        if (mSink) {
            mSink->Close();
            mSink = nullptr;
        }

        nsresult rv = mLocation->Remove(false);
        if (NS_FAILED(rv))
            NS_ERROR("unable to remove temp file");
    }
}

NS_IMPL_ISUPPORTS3(nsDownloader,
                   nsIDownloader,
                   nsIStreamListener,
                   nsIRequestObserver)

NS_IMETHODIMP
nsDownloader::Init(nsIDownloadObserver *observer, nsIFile *location)
{
    mObserver = observer;
    mLocation = location;
    return NS_OK;
}

NS_IMETHODIMP 
nsDownloader::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!mLocation) {
        nsCOMPtr<nsICachingChannel> caching = do_QueryInterface(request, &rv);
        if (NS_SUCCEEDED(rv))
            rv = caching->SetCacheAsFile(true);
    }
    if (NS_FAILED(rv)) {
        // OK, we will need to stream the data to disk ourselves.  Make
        // sure mLocation exists.
        if (!mLocation) {
            rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mLocation));
            if (NS_FAILED(rv)) return rv;

            char buf[13];
            NS_MakeRandomString(buf, 8);
            memcpy(buf+8, ".tmp", 5);
            rv = mLocation->AppendNative(nsDependentCString(buf, 12));
            if (NS_FAILED(rv)) return rv;

            rv = mLocation->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
            if (NS_FAILED(rv)) return rv;

            mLocationIsTemp = true;
        }
        
        rv = NS_NewLocalFileOutputStream(getter_AddRefs(mSink), mLocation);
        if (NS_FAILED(rv)) return rv;

        // we could wrap this output stream with a buffered output stream,
        // but it shouldn't be necessary since we will be writing large
        // chunks given to us via OnDataAvailable.
    }
    return rv;
}

NS_IMETHODIMP 
nsDownloader::OnStopRequest(nsIRequest  *request,
                            nsISupports *ctxt,
                            nsresult     status)
{
    if (!mSink && NS_SUCCEEDED(status)) {
        nsCOMPtr<nsICachingChannel> caching = do_QueryInterface(request, &status);
        if (NS_SUCCEEDED(status)) {
            status = caching->GetCacheFile(getter_AddRefs(mLocation));
            if (NS_SUCCEEDED(status)) {
                NS_ASSERTION(mLocation, "success without a cache file");
                // ok, then we need to hold a reference to the cache token in
                // order to ensure that the cache file remains valid until we
                // get destroyed.
                caching->GetCacheToken(getter_AddRefs(mCacheToken));
            }
        }
    }
    else if (mSink) {
        mSink->Close();
        mSink = nullptr;
    }

    mObserver->OnDownloadComplete(this, request, ctxt, status, mLocation);
    mObserver = nullptr;

    return NS_OK;
}

NS_METHOD
nsDownloader::ConsumeData(nsIInputStream* in,
                          void* closure,
                          const char* fromRawSegment,
                          PRUint32 toOffset,
                          PRUint32 count,
                          PRUint32 *writeCount)
{
    nsDownloader *self = (nsDownloader *) closure;
    if (self->mSink)
        return self->mSink->Write(fromRawSegment, count, writeCount);

    *writeCount = count;
    return NS_OK;
}

NS_IMETHODIMP 
nsDownloader::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, 
                              nsIInputStream *inStr, 
                              PRUint32 sourceOffset, PRUint32 count)
{
    PRUint32 n;  
    return inStr->ReadSegments(ConsumeData, this, count, &n);
}
