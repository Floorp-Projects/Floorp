/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDownloader.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
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

NS_IMPL_ISUPPORTS(nsDownloader,
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
    nsresult rv;
    if (!mLocation) {
        nsCOMPtr<nsIFile> location;
        rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(location));
        if (NS_FAILED(rv)) return rv;

        char buf[13];
        NS_MakeRandomString(buf, 8);
        memcpy(buf+8, ".tmp", 5);
        rv = location->AppendNative(nsDependentCString(buf, 12));
        if (NS_FAILED(rv)) return rv;

        rv = location->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
        if (NS_FAILED(rv)) return rv;

        location.swap(mLocation);
        mLocationIsTemp = true;
    }

    rv = NS_NewLocalFileOutputStream(getter_AddRefs(mSink), mLocation);
    if (NS_FAILED(rv)) return rv;

    // we could wrap this output stream with a buffered output stream,
    // but it shouldn't be necessary since we will be writing large
    // chunks given to us via OnDataAvailable.

    return NS_OK;
}

NS_IMETHODIMP 
nsDownloader::OnStopRequest(nsIRequest  *request,
                            nsISupports *ctxt,
                            nsresult     status)
{
    if (mSink) {
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
                          uint32_t toOffset,
                          uint32_t count,
                          uint32_t *writeCount)
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
                              uint64_t sourceOffset, uint32_t count)
{
    uint32_t n;  
    return inStr->ReadSegments(ConsumeData, this, count, &n);
}
