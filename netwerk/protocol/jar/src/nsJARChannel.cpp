/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998,2000 Netscape Communications Corporation. All Rights
 * Reserved.
 *
 */

#include "nsJARChannel.h"
#include "nsJARProtocolHandler.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"

#include "nsScriptSecurityManager.h"
#include "nsIAggregatePrincipal.h"
#include "nsIFileURL.h"
#include "nsIJAR.h"

static NS_DEFINE_CID(kScriptSecurityManagerCID, NS_SCRIPTSECURITYMANAGER_CID);
static NS_DEFINE_CID(kInputStreamChannelCID, NS_INPUTSTREAMCHANNEL_CID);

//-----------------------------------------------------------------------------

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=nsJarProtocol:5
//
static PRLogModuleInfo *gJarProtocolLog = nsnull;
#endif

#define LOG(args)     PR_LOG(gJarProtocolLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gJarProtocolLog, 4)

//-----------------------------------------------------------------------------
// nsJARInputThunk
//
// this class allows us to do some extra work on the stream transport thread.
//-----------------------------------------------------------------------------

class nsJARInputThunk : public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

    nsJARInputThunk(nsIFile *jarFile, const nsACString &jarEntry,
                    nsIZipReaderCache *jarCache)
        : mJarCache(jarCache)
        , mJarFile(jarFile)
        , mJarEntry(jarEntry)
        , mContentLength(-1)
    {
        NS_ASSERTION(mJarFile, "no jar file");
    }
    virtual ~nsJARInputThunk() {}

    void GetJarReader(nsIZipReader **result)
    {
        NS_IF_ADDREF(*result = mJarReader);
    }

    PRInt32 GetContentLength()
    {
        return mContentLength;
    }

private:
    nsresult EnsureJarStream();

    nsCOMPtr<nsIZipReaderCache> mJarCache;
    nsCOMPtr<nsIZipReader>      mJarReader;
    nsCOMPtr<nsIFile>           mJarFile;
    nsCOMPtr<nsIInputStream>    mJarStream;
    nsCString                   mJarEntry;
    PRInt32                     mContentLength;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJARInputThunk, nsIInputStream)

nsresult
nsJARInputThunk::EnsureJarStream()
{
    if (mJarStream)
        return NS_OK;

    nsresult rv;
    
    rv = mJarCache->GetZip(mJarFile, getter_AddRefs(mJarReader));
    if (NS_FAILED(rv)) return rv;

    rv = mJarReader->GetInputStream(mJarEntry.get(),
                                    getter_AddRefs(mJarStream));
    if (NS_FAILED(rv)) return rv;

    // ask the zip entry for the content length
    nsCOMPtr<nsIZipEntry> entry;
    mJarReader->GetEntry(mJarEntry.get(), getter_AddRefs(entry));
    if (entry)
        entry->GetRealSize((PRUint32 *) &mContentLength);

    return NS_OK;
}

NS_IMETHODIMP
nsJARInputThunk::Close()
{
    if (mJarStream)
        return mJarStream->Close();

    return NS_OK;
}

NS_IMETHODIMP
nsJARInputThunk::Available(PRUint32 *avail)
{
    nsresult rv = EnsureJarStream();
    if (NS_FAILED(rv)) return rv;

    return mJarStream->Available(avail);
}

NS_IMETHODIMP
nsJARInputThunk::Read(char *buf, PRUint32 count, PRUint32 *countRead)
{
    nsresult rv = EnsureJarStream();
    if (NS_FAILED(rv)) return rv;

    return mJarStream->Read(buf, count, countRead);
}

NS_IMETHODIMP
nsJARInputThunk::ReadSegments(nsWriteSegmentFun writer, void *closure,
                              PRUint32 count, PRUint32 *countRead)
{
    // stream transport does only calls Read()
    NS_NOTREACHED("nsJarInputThunk::ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARInputThunk::IsNonBlocking(PRBool *nonBlocking)
{
    *nonBlocking = PR_FALSE;
    return NS_OK;
}

//-----------------------------------------------------------------------------

nsJARChannel::nsJARChannel()
    : mContentLength(-1)
    , mLoadFlags(LOAD_NORMAL)
    , mStatus(NS_OK)
    , mIsPending(PR_FALSE)
    , mJarInput(nsnull)
{
#if defined(PR_LOGGING)
    if (!gJarProtocolLog)
        gJarProtocolLog = PR_NewLogModule("nsJarProtocol");
#endif

    // hold an owning reference to the jar handler
    NS_ADDREF(gJarHandler);
}

nsJARChannel::~nsJARChannel()
{
    // with the exception of certain error cases mJarInput will already be null.
    NS_IF_RELEASE(mJarInput);

    // release owning reference to the jar handler
    nsJARProtocolHandler *handler = gJarHandler;
    NS_RELEASE(handler); // NULL parameter
}

NS_IMPL_ISUPPORTS6(nsJARChannel,
                   nsIRequest,
                   nsIChannel,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIDownloadObserver,
                   nsIJARChannel)

nsresult 
nsJARChannel::Init(nsIURI *uri)
{
    nsresult rv;
    mJarURI = do_QueryInterface(uri, &rv);

#if defined(PR_LOGGING)
    mJarURI->GetSpec(mSpec);
#endif
    return rv;
}

nsresult
nsJARChannel::CreateJarInput()
{
    // important to pass a clone of the file since the nsIFile impl is not
    // necessarily MT-safe
    nsCOMPtr<nsIFile> clonedFile;
    nsresult rv = mJarFile->Clone(getter_AddRefs(clonedFile));
    if (NS_FAILED(rv)) return rv;

    mJarInput = new nsJARInputThunk(clonedFile, mJarEntry, gJarHandler->JarCache());
    if (!mJarInput)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mJarInput);
    return NS_OK;
}

nsresult
nsJARChannel::EnsureJarInput(PRBool blocking)
{
    LOG(("nsJARChannel::EnsureJarInput [this=%x %s]\n", this, mSpec.get()));

    nsresult rv;
    nsCOMPtr<nsIURI> uri;

    rv = mJarURI->GetJARFile(getter_AddRefs(mJarBaseURI));
    if (NS_FAILED(rv)) return rv;

    rv = mJarURI->GetJAREntry(mJarEntry);
    if (NS_FAILED(rv)) return rv;

    // try to get a nsIFile directly from the url, which will often succeed.
    {
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mJarBaseURI);
        if (fileURL)
            fileURL->GetFile(getter_AddRefs(mJarFile));
    }

    if (mJarFile) {
        rv = CreateJarInput();
    }
    else if (blocking) {
        NS_NOTREACHED("need sync downloader");
        rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else {
        // kick off an async download of the base URI...
        rv = NS_NewDownloader(getter_AddRefs(mDownloader),
                              mJarBaseURI, this, nsnull, PR_FALSE,
                              mLoadGroup, mCallbacks, mLoadFlags);
    }
    return rv;

}

//-----------------------------------------------------------------------------
// nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsJARChannel::GetName(nsACString &result)
{
    return mJarURI->GetSpec(result);
}

NS_IMETHODIMP
nsJARChannel::IsPending(PRBool *result)
{
    *result = mIsPending;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetStatus(nsresult *status)
{
    if (mPump && NS_SUCCEEDED(mStatus))
        mPump->GetStatus(status);
    else
        *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::Cancel(nsresult status)
{
    mStatus = status;
    if (mPump)
        return mPump->Cancel(status);

    NS_ASSERTION(!mIsPending, "need to implement cancel when downloading");
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::Suspend()
{
    if (mPump)
        return mPump->Suspend();

    NS_ASSERTION(!mIsPending, "need to implement suspend when downloading");
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::Resume()
{
    if (mPump)
        return mPump->Resume();

    NS_ASSERTION(!mIsPending, "need to implement resume when downloading");
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsJARChannel::GetOriginalURI(nsIURI **aURI)
{
    if (mOriginalURI)
        *aURI = mOriginalURI;
    else
        *aURI = mJarURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetOriginalURI(nsIURI *aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetURI(nsIURI **aURI)
{
    NS_IF_ADDREF(*aURI = mJarURI);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetOwner(nsISupports **result)
{
    nsresult rv;

    if (mOwner) {
        NS_ADDREF(*result = mOwner);
        return NS_OK;
    }

    if (!mJarInput) {
        *result = nsnull;
        return NS_OK;
    }

    //-- Verify signature, if one is present, and set owner accordingly
    nsCOMPtr<nsIZipReader> jarReader;
    mJarInput->GetJarReader(getter_AddRefs(jarReader));
    if (!jarReader)
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIJAR> jar = do_QueryInterface(jarReader, &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsIJAR not supported");
        return rv;
    }

    nsCOMPtr<nsIPrincipal> cert;
    rv = jar->GetCertificatePrincipal(mJarEntry.get(), getter_AddRefs(cert));
    if (NS_FAILED(rv)) return rv;

    if (cert) {
        // Get the codebase principal
        nsCOMPtr<nsIScriptSecurityManager> secMan = 
                 do_GetService(kScriptSecurityManagerCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIPrincipal> codebase;
        rv = secMan->GetCodebasePrincipal(mJarBaseURI, 
                                          getter_AddRefs(codebase));
        if (NS_FAILED(rv)) return rv;
    
        // Join the certificate and the codebase
        nsCOMPtr<nsIAggregatePrincipal> agg = do_QueryInterface(cert, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = agg->SetCodebase(codebase);
        if (NS_FAILED(rv)) return rv;

        mOwner = do_QueryInterface(agg, &rv);
        if (NS_FAILED(rv)) return rv;

        NS_ADDREF(*result = mOwner);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetOwner(nsISupports *aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
    NS_IF_ADDREF(*aCallbacks = mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks)
{
    mCallbacks = aCallbacks;
    mProgressSink = do_GetInterface(mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP 
nsJARChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetContentType(nsACString &result)
{
    nsresult rv;

    if (!mContentType.IsEmpty()) {
        result = mContentType;
        return NS_OK;
    }

    //
    // generate content type and set it
    //
    if (mJarEntry.IsEmpty()) {
        LOG(("mJarEntry is empty!\n"));
        return NS_ERROR_NOT_AVAILABLE;
    }

    const char *ext = nsnull, *fileName = mJarEntry.get();
    PRInt32 len = mJarEntry.Length();
    for (PRInt32 i = len-1; i >= 0; i--) {
        if (fileName[i] == '.') {
            ext = &fileName[i + 1];
            break;
        }
    }
    if (ext) {
        nsIMIMEService *mimeServ = gJarHandler->MimeService();
        if (mimeServ) {
            nsXPIDLCString mimeType;
            rv = mimeServ->GetTypeFromExtension(ext, getter_Copies(mimeType));
            if (NS_SUCCEEDED(rv))
                mContentType = mimeType;
        }
    }
    else
        rv = NS_ERROR_NOT_AVAILABLE;

    if (NS_FAILED(rv) || mContentType.IsEmpty())
        mContentType = NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE);

    result = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetContentType(const nsACString &aContentType)
{
    // mContentCharset is unchanged if not parsed
    NS_ParseContentType(aContentType, mContentType, mContentCharset);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetContentCharset(const nsACString &aContentCharset)
{
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetContentLength(PRInt32 *result)
{
    // if content length is unknown, query mJarInput...
    if (mContentLength < 0 && mJarInput)
        mContentLength = mJarInput->GetContentLength();

    *result = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetContentLength(PRInt32 aContentLength)
{
    // XXX does this really make any sense at all?
    mContentLength = aContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::Open(nsIInputStream **stream)
{
    LOG(("nsJARChannel::Open [this=%x]\n", this));

    NS_ENSURE_TRUE(!mJarInput, NS_ERROR_IN_PROGRESS);
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    nsresult rv = EnsureJarInput(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    if (!mJarInput)
        return NS_ERROR_UNEXPECTED;

    NS_ADDREF(*stream = mJarInput);
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctx)
{
    LOG(("nsJARChannel::AsyncOpen [this=%x]\n", this));

    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    nsresult rv = EnsureJarInput(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    if (mJarInput) {
        // create input stream pump
        rv = NS_NewInputStreamPump(getter_AddRefs(mPump), mJarInput);
        if (NS_FAILED(rv)) return rv;

        rv = mPump->AsyncRead(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    mListener = listener;
    mListenerContext = ctx;
    mIsPending = PR_TRUE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIDownloadObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsJARChannel::OnDownloadComplete(nsIDownloader *downloader,
                                 nsISupports *closure,
                                 nsresult status,
                                 nsIFile *file)
{
    if (NS_SUCCEEDED(status)) {
        mJarFile = file;
    
        nsresult rv = CreateJarInput();
        if (NS_SUCCEEDED(rv)) {
            // create input stream pump
            rv = NS_NewInputStreamPump(getter_AddRefs(mPump), mJarInput);
            if (NS_SUCCEEDED(rv))
                rv = mPump->AsyncRead(this, nsnull);
        }
        status = rv;
    }

    if (NS_FAILED(status)) {
        OnStartRequest(nsnull, nsnull);
        OnStopRequest(nsnull, nsnull, status);
    }

    mDownloader = 0;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsJARChannel::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    LOG(("nsJARChannel::OnStartRequest [this=%x %s]\n", this, mSpec.get()));

    return mListener->OnStartRequest(this, mListenerContext);
}

NS_IMETHODIMP
nsJARChannel::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    LOG(("nsJARChannel::OnStopRequest [this=%x %s status=%x]\n",
        this, mSpec.get(), status));

    if (NS_SUCCEEDED(mStatus))
        mStatus = status;

    if (mListener) {
        mListener->OnStopRequest(this, mListenerContext, status);
        mListener = 0;
        mListenerContext = 0;
    }

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, status);

    mPump = 0;
    NS_IF_RELEASE(mJarInput);
    mIsPending = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                               nsIInputStream *stream,
                               PRUint32 offset, PRUint32 count)
{
#if defined(PR_LOGGING)
    LOG(("nsJARChannel::OnDataAvailable [this=%x %s]\n", this, mSpec.get()));
#endif

    nsresult rv;

    rv = mListener->OnDataAvailable(this, mListenerContext, stream, offset, count);

    // simply report progress here instead of hooking ourselves up as a
    // nsITransportEventSink implementation.
    if (mProgressSink && NS_SUCCEEDED(rv) && !(mLoadFlags & LOAD_BACKGROUND))
        mProgressSink->OnProgress(this, nsnull, offset + count, mContentLength);

    return rv; // let the pump cancel on failure
}
