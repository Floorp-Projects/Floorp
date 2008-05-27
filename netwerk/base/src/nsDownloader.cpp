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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include "nsDownloader.h"
#include "nsICachingChannel.h"
#include "nsIInputStream.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsNetUtil.h"

// XXX this code is ripped from profile/src/nsProfile.cpp and is further
//     duplicated in uriloader/exthandler.  this should probably be moved
//     into xpcom or some other shared library.
#include <stdlib.h>
#define TABLE_SIZE 36
static const char table[] =
    { 'a','b','c','d','e','f','g','h','i','j',
      'k','l','m','n','o','p','q','r','s','t',
      'u','v','w','x','y','z','0','1','2','3',
      '4','5','6','7','8','9' };
static void
MakeRandomString(char *buf, PRInt32 bufLen)
{
    // turn PR_Now() into milliseconds since epoch
    // and salt rand with that.
    double fpTime;
    LL_L2D(fpTime, PR_Now());
    srand((uint)(fpTime * 1e-6 + 0.5));   // use 1e-6, granularity of PR_Now() on the mac is seconds

    PRInt32 i;
    for (i=0;i<bufLen;i++) {
        *buf++ = table[rand()%TABLE_SIZE];
    }
    *buf = 0;
}
// XXX

nsDownloader::~nsDownloader()
{
    if (mLocation && mLocationIsTemp) {
        // release the sink first since it may still hold an open file
        // descriptor to mLocation.  this needs to happen before the
        // file can be removed otherwise the Remove call will fail.
        if (mSink) {
            mSink->Close();
            mSink = nsnull;
        }

        nsresult rv = mLocation->Remove(PR_FALSE);
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
            rv = caching->SetCacheAsFile(PR_TRUE);
    }
    if (NS_FAILED(rv)) {
        // OK, we will need to stream the data to disk ourselves.  Make
        // sure mLocation exists.
        if (!mLocation) {
            rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mLocation));
            if (NS_FAILED(rv)) return rv;

            char buf[13];
            MakeRandomString(buf, 8);
            memcpy(buf+8, ".tmp", 5);
            rv = mLocation->AppendNative(nsDependentCString(buf, 12));
            if (NS_FAILED(rv)) return rv;

            rv = mLocation->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
            if (NS_FAILED(rv)) return rv;

            mLocationIsTemp = PR_TRUE;
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
        mSink = nsnull;
    }

    mObserver->OnDownloadComplete(this, request, ctxt, status, mLocation);
    mObserver = nsnull;

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
