/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*

  A data source that can read itself from and write itself to an
  RDF/XML stream.

  TO DO
  -----

  1) Right now, the only kind of stream data sources that are writable
     are "file:" URIs. (In fact, <em>all</em> "file:" URIs are
     writable, modulo flie system permissions; this may lead to some
     surprising behavior.) Eventually, it'd be great if we could open
     an arbitrary nsIOutputStream on *any* URL, and Netlib could just
     do the magic.

  2) We need a way to decide whether or not the stream should be read
     with a blocking read or an asynchronous read. This issue is
     particularly important for consumers that expect to be able to
     Init() a data source and then have it be 100% ready for use
     (e.g., the regsitry). Does this warrant a new interface? Right
     now, we just assume that "file:" and "resource:" URIs should be
     read synchronously.

 */

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIDTD.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsIOutputStream.h"
#include "nsIParser.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsLayoutCID.h" // for NS_NAMESPACEMANAGER_CID.
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsVoidArray.h"
#include "plstr.h"
#include "prio.h"
#include "prthread.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDTDIID,               NS_IDTD_IID);
static NS_DEFINE_IID(kIInputStreamIID,       NS_IINPUTSTREAM_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,  NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIOutputStreamIID,      NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kIParserIID,            NS_IPARSER_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,     NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,    NS_IRDFCONTENTSINK_IID);
static NS_DEFINE_IID(kIRDFServiceIID,        NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFXMLDataSourceIID,  NS_IRDFXMLDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFXMLSourceIID,      NS_IRDFXMLSOURCE_IID);
static NS_DEFINE_IID(kIStreamListenerIID,    NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,        NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

////////////////////////////////////////////////////////////////////////
// FileOutputStreamImpl

class FileOutputStreamImpl : public nsIOutputStream
{
private:
    nsOutputFileStream mStream;

public:
    FileOutputStreamImpl(const nsFilePath& path)
        : mStream(path)
    {
        NS_INIT_REFCNT();
    }

    virtual ~FileOutputStreamImpl(void) {
        Close();
    }

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIBaseStream interface
    NS_IMETHOD Close(void) {
        mStream.close();
        return NS_OK;
    }

    // nsIOutputStream interface
    NS_IMETHOD Write(const char* aBuf,
                     PRUint32 aOffset,
                     PRUint32 aCount,
                     PRUint32 *aWriteCount)
    {
        PRInt32 written = mStream.write(aBuf + aOffset, aCount);
        if (written == -1) {
            *aWriteCount = 0;
            return NS_ERROR_FAILURE; // XXX right error code?
        }
        else {
            *aWriteCount = written;
            return NS_OK;
        }
    }
};

NS_IMPL_ISUPPORTS(FileOutputStreamImpl, kIOutputStreamIID);

////////////////////////////////////////////////////////////////////////

class ProxyStream : public nsIInputStream
{
private:
    const char* mBuffer;
    PRUint32    mSize;
    PRUint32    mIndex;

public:
    ProxyStream(void) : mBuffer(nsnull)
    {
        NS_INIT_REFCNT();
    }

    virtual ~ProxyStream(void) {
    }

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIBaseStream
    NS_IMETHOD Close(void) {
        return NS_OK;
    }

    // nsIInputStream
    NS_IMETHOD GetLength(PRUint32 *aLength) {
        *aLength = mSize - mIndex;
        return NS_OK;
    }
    
    NS_IMETHOD Read(char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aReadCount) {
        PRUint32 readCount = 0;
        aBuf += aOffset;
        while (mIndex < mSize && aCount > 0) {
            *aBuf = mBuffer[mIndex];
            aBuf++;
            mIndex++;
            readCount++;
            aCount--;
        }
        *aReadCount = readCount;
        return NS_OK;
    }

    // Implementation
    void SetBuffer(const char* aBuffer, PRUint32 aSize) {
        mBuffer = aBuffer;
        mSize = aSize;
        mIndex = 0;
    }
};

NS_IMPL_ISUPPORTS(ProxyStream, kIInputStreamIID);

////////////////////////////////////////////////////////////////////////
// RDFXMLDataSourceImpl

class RDFXMLDataSourceImpl : public nsIRDFXMLDataSource,
                             public nsIRDFXMLSource
{
protected:
    nsIRDFDataSource* mInner;
    PRBool            mIsSynchronous; // true if the document should be loaded synchronously
    PRBool            mIsWritable;    // true if the document can be written back
    PRBool            mIsDirty;       // true if the document should be written back
    nsVoidArray       mObservers;
    char**            mNamedDataSourceURIs;
    PRInt32           mNumNamedDataSourceURIs;
    nsIURL**          mCSSStyleSheetURLs;
    PRInt32           mNumCSSStyleSheetURLs;
    nsIRDFResource*   mRootResource;
    PRBool            mIsLoading; // true while the document is loading

public:
    RDFXMLDataSourceImpl(void);
    virtual ~RDFXMLDataSourceImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(const char* *uri) const {
        return mInner->GetURI(uri);
    }

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source) {
        return mInner->GetSource(property, target, tv, source);
    }

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources) {
        return mInner->GetSources(property, target, tv, sources);
    }

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target) {
        return mInner->GetTarget(source, property, tv, target);
    }

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets) {
        return mInner->GetTargets(source, property, tv, targets);
    }

    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv);

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion) {
        return mInner->HasAssertion(source, property, target, tv, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* n) {
        return mInner->AddObserver(n);
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n) {
        return mInner->RemoveObserver(n);
    }

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels) {
        return mInner->ArcLabelsIn(node, labels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels) {
        return mInner->ArcLabelsOut(source, labels);
    }

    NS_IMETHOD Flush(void);

    NS_IMETHOD IsCommandEnabled(const char* aCommand,
                                nsIRDFResource* aCommandTarget,
                                PRBool* aResult) {
        return mInner->IsCommandEnabled(aCommand, aCommandTarget, aResult);
    }

    NS_IMETHOD DoCommand(const char* aCommand,
                         nsIRDFResource* aCommandTarget) {
        // XXX Uh oh, this could cause problems wrt. the "dirty" flag
        // if it changes the in-memory store's internal state.
        return mInner->DoCommand(aCommand, aCommandTarget);
    }

    // nsIRDFXMLDataSource interface
    NS_IMETHOD SetSynchronous(PRBool aIsSynchronous);
    NS_IMETHOD SetReadOnly(PRBool aIsReadOnly);
    NS_IMETHOD BeginLoad(void);
    NS_IMETHOD Interrupt(void);
    NS_IMETHOD Resume(void);
    NS_IMETHOD EndLoad(void);
    NS_IMETHOD SetRootResource(nsIRDFResource* aResource);
    NS_IMETHOD GetRootResource(nsIRDFResource** aResource);
    NS_IMETHOD AddCSSStyleSheetURL(nsIURL* aStyleSheetURL);
    NS_IMETHOD GetCSSStyleSheetURLs(nsIURL*** aStyleSheetURLs, PRInt32* aCount);
    NS_IMETHOD AddNamedDataSourceURI(const char* aNamedDataSourceURI);
    NS_IMETHOD GetNamedDataSourceURIs(const char* const** aNamedDataSourceURIs, PRInt32* aCount);
    NS_IMETHOD AddXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver);
    NS_IMETHOD RemoveXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver);

    // nsIRDFXMLSource interface
    NS_IMETHOD Serialize(nsIOutputStream* aStream);

    // Implementation methods
};


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFXMLDataSource(nsIRDFXMLDataSource** result)
{
    RDFXMLDataSourceImpl* ds = new RDFXMLDataSourceImpl();
    if (! ds)
        return NS_ERROR_NULL_POINTER;

    *result = ds;
    NS_ADDREF(*result);
    return NS_OK;
}


RDFXMLDataSourceImpl::RDFXMLDataSourceImpl(void)
    : mIsSynchronous(PR_FALSE),
      mIsWritable(PR_TRUE),
      mIsDirty(PR_FALSE),
      mNamedDataSourceURIs(nsnull),
      mNumNamedDataSourceURIs(0),
      mCSSStyleSheetURLs(nsnull),
      mNumCSSStyleSheetURLs(0),
      mIsLoading(PR_FALSE)
{
    nsresult rv;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mInner)))
        PR_ASSERT(0);

    NS_INIT_REFCNT();
}


RDFXMLDataSourceImpl::~RDFXMLDataSourceImpl(void)
{
    nsIRDFService* rdfService;
    if (NS_SUCCEEDED(nsServiceManager::GetService(kRDFServiceCID,
                                                  kIRDFServiceIID,
                                                  (nsISupports**) &rdfService))) {
        rdfService->UnregisterDataSource(this);
        nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
    }

    Flush();

    while (--mNumNamedDataSourceURIs >= 0)
        delete mNamedDataSourceURIs[mNumNamedDataSourceURIs];

    delete mNamedDataSourceURIs;

    while (--mNumCSSStyleSheetURLs >= 0)
        NS_RELEASE(mCSSStyleSheetURLs[mNumCSSStyleSheetURLs]);

    delete mCSSStyleSheetURLs;
}


NS_IMPL_ADDREF(RDFXMLDataSourceImpl);
NS_IMPL_RELEASE(RDFXMLDataSourceImpl);

NS_IMETHODIMP
RDFXMLDataSourceImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kISupportsIID) ||
        iid.Equals(kIRDFDataSourceIID) ||
        iid.Equals(kIRDFXMLDataSourceIID)) {
        *result = NS_STATIC_CAST(nsIRDFDataSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIRDFXMLSourceIID)) {
        *result = NS_STATIC_CAST(nsIRDFXMLSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}


static nsresult
rdf_BlockingParse(nsIURL* aURL, nsIStreamListener* aConsumer)
{
    nsresult rv;

    // XXX I really hate the way that we're spoon-feeding this stuff
    // to the parser: it seems like this is something that netlib
    // should be able to do by itself.

    nsIInputStream* in;
    if (NS_FAILED(rv = NS_OpenURL(aURL, &in, nsnull /* XXX aConsumer */)))
        return rv;

    rv = NS_ERROR_OUT_OF_MEMORY;
    ProxyStream* proxy = new ProxyStream();
    if (! proxy)
        goto done;

    // XXX shouldn't netlib be doing this???
    aConsumer->OnStartBinding(aURL, "text/rdf");
    while (PR_TRUE) {
        char buf[1024];
        PRUint32 readCount;

        if (NS_FAILED(rv = in->Read(buf, 0, sizeof(buf), &readCount)))
            break; // error or eof

        if (readCount == 0)
            break; // eof

        proxy->SetBuffer(buf, readCount);
                
        // XXX shouldn't netlib be doing this???
        if (NS_FAILED(rv = aConsumer->OnDataAvailable(aURL, proxy, readCount)))
            break;
    }
    if (rv == NS_BASE_STREAM_EOF) {
        rv = NS_OK;
    }
    // XXX shouldn't netlib be doing this???
    aConsumer->OnStopBinding(aURL, 0, nsnull);

done:
    NS_RELEASE(in);
    return rv;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::Init(const char* uri)
{
static const char kFileURIPrefix[] = "file:";
static const char kResourceURIPrefix[] = "resource:";

    NS_PRECONDITION(mInner != nsnull, "not initialized");
    if (! mInner)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    // XXX this is a hack: any "file:" URI is considered writable. All
    // others are considered read-only.
    if (PL_strncmp(uri, kFileURIPrefix, sizeof(kFileURIPrefix) - 1) != 0)
        mIsWritable = PR_FALSE;

    nsIRDFService* rdfService = nsnull;
    nsINameSpaceManager* ns = nsnull;
    nsIRDFContentSink* sink = nsnull;
    nsIParser* parser       = nsnull;
    nsIDTD* dtd             = nsnull;
    nsIStreamListener* lsnr = nsnull;
    nsIURL* url             = nsnull;

    if (NS_FAILED(rv = NS_NewURL(&url, uri)))
        goto done;

    if (NS_FAILED(rv = mInner->Init(uri)))
        goto done;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &rdfService)))
        goto done;

    if (NS_FAILED(rv = rdfService->RegisterDataSource(this)))
        goto done;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    (void**) &ns)))
        goto done;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFContentSinkCID,
                                                    nsnull,
                                                    kIRDFContentSinkIID,
                                                    (void**) &sink)))
        goto done;

    if (NS_FAILED(sink->Init(url, ns)))
        goto done;

    // We set the content sink's data source directly to our in-memory
    // store. This allows the initial content to be generated "directly".
    if (NS_FAILED(rv = sink->SetDataSource(this)))
        goto done;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kParserCID,
                                                    nsnull,
                                                    kIParserIID,
                                                    (void**) &parser)))
        goto done;

    parser->SetContentSink(sink);

    // XXX this should eventually be kRDFDTDCID (oh boy, that's a
    // pretty identifier). The RDF DTD will be a much more
    // RDF-resilient parser.
    if (NS_FAILED(rv = nsRepository::CreateInstance(kWellFormedDTDCID,
                                                    nsnull,
                                                    kIDTDIID,
                                                    (void**) &dtd)))
        goto done;

    parser->RegisterDTD(dtd);

    if (NS_FAILED(rv = parser->QueryInterface(kIStreamListenerIID, (void**) &lsnr)))
        goto done;

    if (NS_FAILED(parser->Parse(url)))
        goto done;

    // XXX Yet another hack to get the registry stuff
    // bootstrapped. Force "file:" and "resource:" URIs to be loaded
    // by a blocking read. Maybe there needs to be a distinct
    // interface for stream data sources?
    if (mIsSynchronous) {
        rv = rdf_BlockingParse(url, lsnr);
    }
    else {
        rv = NS_OpenURL(url, lsnr);
    }

done:
    NS_IF_RELEASE(lsnr);
    NS_IF_RELEASE(dtd);
    NS_IF_RELEASE(parser);
    NS_IF_RELEASE(sink);
    if (rdfService) {
        nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
        rdfService = nsnull;
    }
    NS_IF_RELEASE(url);
    return rv;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::Assert(nsIRDFResource* source, 
                             nsIRDFResource* property, 
                             nsIRDFNode* target,
                             PRBool tv)
{
    if (!mIsLoading && !mIsWritable)
        return NS_ERROR_FAILURE; // XXX right error code?

    nsresult rv;
    if (NS_SUCCEEDED(rv = mInner->Assert(source, property, target, tv))) {
        if (!mIsLoading)
            mIsDirty = PR_TRUE;
    }

    return rv;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::Unassert(nsIRDFResource* source, 
                               nsIRDFResource* property, 
                               nsIRDFNode* target)
{
    if (!mIsLoading && !mIsWritable)
        return NS_ERROR_FAILURE; // XXX right error code?

    nsresult rv;
    if (NS_SUCCEEDED(rv = mInner->Unassert(source, property, target))) {
        if (!mIsLoading)
            mIsDirty = PR_TRUE;
    }

    return rv;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::Flush(void)
{
    if (!mIsWritable || !mIsDirty)
        return NS_OK;

    nsresult rv;

    const char* uri;
    if (NS_FAILED(rv = mInner->GetURI(&uri)))
        return rv;

    nsFileURL url(uri);
    nsFilePath path(url);

    FileOutputStreamImpl* out = new FileOutputStreamImpl(path);
    if (! out)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = Serialize(out)))
        goto done;

    mIsDirty = PR_FALSE;

done:
    NS_IF_RELEASE(out);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFXMLDataSource methods

NS_IMETHODIMP
RDFXMLDataSourceImpl::SetSynchronous(PRBool aIsSynchronous)
{
    mIsSynchronous = aIsSynchronous;
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::SetReadOnly(PRBool aIsReadOnly)
{
    if (mIsWritable && aIsReadOnly)
        mIsWritable = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::BeginLoad(void)
{
    mIsLoading = PR_TRUE;
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnBeginLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Interrupt(void)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnInterrupt(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Resume(void)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnResume(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::EndLoad(void)
{
    mIsLoading = PR_FALSE;
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnEndLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::SetRootResource(nsIRDFResource* aResource)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(aResource);
    mRootResource = aResource;

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnRootResourceFound(this, mRootResource);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::GetRootResource(nsIRDFResource** aResource)
{
    NS_ADDREF(mRootResource);
    *aResource = mRootResource;
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::AddCSSStyleSheetURL(nsIURL* aCSSStyleSheetURL)
{
    NS_PRECONDITION(aCSSStyleSheetURL != nsnull, "null ptr");
    if (! aCSSStyleSheetURL)
        return NS_ERROR_NULL_POINTER;

    nsIURL** p = new nsIURL*[mNumCSSStyleSheetURLs + 1];
    if (! p)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 i;
    for (i = mNumCSSStyleSheetURLs - 1; i >= 0; --i)
        p[i] = mCSSStyleSheetURLs[i];

    NS_ADDREF(aCSSStyleSheetURL);
    p[mNumCSSStyleSheetURLs] = aCSSStyleSheetURL;

    ++mNumCSSStyleSheetURLs;
    mCSSStyleSheetURLs = p;

    for (i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnCSSStyleSheetAdded(this, aCSSStyleSheetURL);
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::GetCSSStyleSheetURLs(nsIURL*** aCSSStyleSheetURLs, PRInt32* aCount)
{
    *aCSSStyleSheetURLs = mCSSStyleSheetURLs;
    *aCount = mNumCSSStyleSheetURLs;
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::AddNamedDataSourceURI(const char* aNamedDataSourceURI)
{
    NS_PRECONDITION(aNamedDataSourceURI != nsnull, "null ptr");
    if (! aNamedDataSourceURI)
        return NS_ERROR_NULL_POINTER;

    char** p = new char*[mNumNamedDataSourceURIs + 1];
    if (! p)
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 i;
    for (i = mNumNamedDataSourceURIs - 1; i >= 0; --i)
        p[i] = mNamedDataSourceURIs[i];

    PRInt32 len = PL_strlen(aNamedDataSourceURI);
    char* buf = new char[len + 1];
    if (! buf) {
        delete p;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    PL_strcpy(buf, aNamedDataSourceURI);
    p[mNumNamedDataSourceURIs] = buf;

    ++mNumNamedDataSourceURIs;
    mNamedDataSourceURIs = p;

    for (i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnNamedDataSourceAdded(this, aNamedDataSourceURI);
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::GetNamedDataSourceURIs(const char* const** aNamedDataSourceURIs, PRInt32* aCount)
{
    *aNamedDataSourceURIs = mNamedDataSourceURIs;
    *aCount = mNumNamedDataSourceURIs;
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::AddXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver)
{
    mObservers.AppendElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::RemoveXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver)
{
    mObservers.RemoveElement(aObserver);
    return NS_OK;
}




////////////////////////////////////////////////////////////////////////
// nsIRDFXMLSource methods

NS_IMETHODIMP
RDFXMLDataSourceImpl::Serialize(nsIOutputStream* stream)
{
    nsresult rv;
    nsIRDFXMLSource* source;
    if (NS_SUCCEEDED(rv = mInner->QueryInterface(kIRDFXMLSourceIID, (void**) &source))) {
        rv = source->Serialize(stream);
        NS_RELEASE(source);
    }
    return rv;
}

