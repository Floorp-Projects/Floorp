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

  1) Right now, the only kind of stream data sources that are _really_
     writable are "file:" URIs. (In fact, <em>all</em> "file:" URIs
     are writable, modulo flie system permissions; this may lead to
     some surprising behavior.) Eventually, it'd be great if we could
     open an arbitrary nsIOutputStream on *any* URL, and Netlib could
     just do the magic.

  2) Implement a more terse output for "typed" nodes; that is, instead
     of "RDF:Description RDF:type='ns:foo'", just output "ns:foo".

  3) There is a lot of code that calls rdf_PossiblyMakeRelative() and
     then calls rdf_EscapeAmpersands(). Is this really just one operation?

  4) Maybe keep the docURI around for writing.

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
#include "rdfutil.h"
#include "prlog.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDTDIID,               NS_IDTD_IID);
static NS_DEFINE_IID(kIInputStreamIID,       NS_IINPUTSTREAM_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,  NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIParserIID,            NS_IPARSER_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,    NS_IRDFCONTENTSINK_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,     NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,        NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,       NS_IRDFRESOURCE_IID);
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
// Vocabulary stuff
#include "rdf.h"
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, nextVal);

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
    
    NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
        PRUint32 readCount = 0;
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
    struct NameSpaceMap {
        nsString      URI;
        nsIAtom*      Prefix;
        NameSpaceMap* Next;
    };

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
    NameSpaceMap*     mNameSpaces;

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

    NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor) {
        return mInner->GetAllResources(aCursor);
    }

    NS_IMETHOD Flush(void);

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands) {
        return mInner->GetAllCommands(source, commands);
    }

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments) {
        return mInner->IsCommandEnabled(aSources, aCommand, aArguments);
    }

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments) {
        // XXX Uh oh, this could cause problems wrt. the "dirty" flag
        // if it changes the in-memory store's internal state.
        return mInner->DoCommand(aSources, aCommand, aArguments);
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
    NS_IMETHOD AddNameSpace(nsIAtom* aPrefix, const nsString& aURI);
    NS_IMETHOD AddXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver);
    NS_IMETHOD RemoveXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver);

    // nsIRDFXMLSource interface
    NS_IMETHOD Serialize(nsIOutputStream* aStream);

    // Implementation methods
    PRBool
    MakeQName(nsIRDFResource* aResource,
              nsString& property,
              nsString& nameSpacePrefix,
              nsString& nameSpaceURI);

    nsresult
    SerializeAssertion(nsIOutputStream* aStream,
                       nsIRDFResource* aResource,
                       nsIRDFResource* aProperty,
                       nsIRDFNode* aValue);

    nsresult
    SerializeProperty(nsIOutputStream* aStream,
                      nsIRDFResource* aResource,
                      nsIRDFResource* aProperty);

    nsresult
    SerializeDescription(nsIOutputStream* aStream,
                         nsIRDFResource* aResource);

    nsresult
    SerializeMember(nsIOutputStream* aStream,
                    nsIRDFResource* aContainer,
                    nsIRDFResource* aProperty);

    nsresult
    SerializeContainer(nsIOutputStream* aStream,
                       nsIRDFResource* aContainer);

    nsresult
    SerializePrologue(nsIOutputStream* aStream);

    nsresult
    SerializeEpilogue(nsIOutputStream* aStream);
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
    : mInner(nsnull),
      mIsSynchronous(PR_FALSE),
      mIsWritable(PR_TRUE),
      mIsDirty(PR_FALSE),
      mNamedDataSourceURIs(nsnull),
      mNumNamedDataSourceURIs(0),
      mCSSStyleSheetURLs(nsnull),
      mNumCSSStyleSheetURLs(0),
      mRootResource(nsnull),
      mIsLoading(PR_FALSE),
      mNameSpaces(nsnull)
{
    nsresult rv;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mInner)))
        PR_ASSERT(0);

    NS_INIT_REFCNT();

    // Initialize the name space stuff to know about any "standard"
    // namespaces that we want to look the same in all the RDF/XML we
    // generate.
    //
    // XXX this is a bit of a hack, because technically, the document
    // should've defined the RDF namespace to be _something_, and we
    // should just look at _that_ and use it. Oh well.
    AddNameSpace(NS_NewAtom("RDF"), RDF_NAMESPACE_URI);
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

    while (mNumNamedDataSourceURIs-- > 0) {
        delete mNamedDataSourceURIs[mNumNamedDataSourceURIs];
	}

    delete mNamedDataSourceURIs;

    while (mNumCSSStyleSheetURLs-- > 0) {
        NS_RELEASE(mCSSStyleSheetURLs[mNumCSSStyleSheetURLs]);
	}

    delete mCSSStyleSheetURLs;

    while (mNameSpaces) {
        NameSpaceMap* doomed = mNameSpaces;
        mNameSpaces = mNameSpaces->Next;

        NS_IF_RELEASE(doomed->Prefix);
        delete doomed;
    }

    NS_IF_RELEASE(mRootResource);
    NS_RELEASE(mInner);
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
    if (NS_FAILED(rv = NS_OpenURL(aURL, &in, nsnull /* XXX aConsumer */))) {
        NS_ERROR("unable to open blocking stream");
        return rv;
    }

    NS_ASSERTION(in != nsnull, "no input stream");
    if (! in) return NS_ERROR_FAILURE;

    rv = NS_ERROR_OUT_OF_MEMORY;
    ProxyStream* proxy = new ProxyStream();
    if (! proxy)
        goto done;

    // XXX shouldn't netlib be doing this???
    aConsumer->OnStartBinding(aURL, "text/rdf");
    while (PR_TRUE) {
        char buf[1024];
        PRUint32 readCount;

        if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
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

    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    (void**) &ns)))
        goto done;

    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFContentSinkCID,
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

    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kParserCID,
                                                    nsnull,
                                                    kIParserIID,
                                                    (void**) &parser)))
        goto done;

    parser->SetContentSink(sink);

    // XXX this should eventually be kRDFDTDCID (oh boy, that's a
    // pretty identifier). The RDF DTD will be a much more
    // RDF-resilient parser.
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kWellFormedDTDCID,
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
    nsFileSpec path(url);

    nsOutputFileStream out(path);
    if (! out.is_open())
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIOutputStream> outIStream = out.GetIStream();
    if (NS_FAILED(rv = Serialize(outIStream)))
        goto done;

    mIsDirty = PR_FALSE;

done:
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

    NS_IF_RELEASE(mRootResource);
    mRootResource = aResource;
    NS_IF_ADDREF(mRootResource);

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnRootResourceFound(this, mRootResource);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::GetRootResource(nsIRDFResource** aResource)
{
    NS_IF_ADDREF(mRootResource);
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
RDFXMLDataSourceImpl::AddNameSpace(nsIAtom* aPrefix, const nsString& aURI)
{
    NameSpaceMap* entry;

    // ensure that URIs are unique
    for (entry = mNameSpaces; entry != nsnull; entry = entry->Next) {
        if (aURI.Equals(entry->URI))
            return NS_OK;
    }

    // okay, it's a new one: let's add it.
    entry = new NameSpaceMap;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_IF_ADDREF(aPrefix);
    entry->Prefix = aPrefix;
    entry->URI = aURI;
    entry->Next = mNameSpaces;
    mNameSpaces = entry;
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

static nsresult
rdf_BlockingWrite(nsIOutputStream* stream, const char* buf, PRUint32 size)
{
    PRUint32 written = 0;
    PRUint32 remaining = size;
    while (remaining > 0) {
        nsresult rv;
        PRUint32 cb;

        if (NS_FAILED(rv = stream->Write(buf + written, remaining, &cb)))
            return rv;

        written += cb;
        remaining -= cb;
    }
    return NS_OK;
}

static nsresult
rdf_BlockingWrite(nsIOutputStream* stream, const nsString& s)
{
    char buf[256];
    char* p = buf;

    if (s.Length() >= sizeof(buf))
        p = new char[s.Length() + 1];

    nsresult rv = rdf_BlockingWrite(stream, s.ToCString(p, s.Length() + 1), s.Length());

    if (p != buf)
        delete[](p);

    return rv;
}

// This converts a property resource (like
// "http://www.w3.org/TR/WD-rdf-syntax#Description") into a property
// ("Description"), a namespace prefix ("RDF"), and a namespace URI
// ("http://www.w3.org/TR/WD-rdf-syntax#").

PRBool
RDFXMLDataSourceImpl::MakeQName(nsIRDFResource* resource,
                                nsString& property,
                                nsString& nameSpacePrefix,
                                nsString& nameSpaceURI)
{
    const char* s;
    resource->GetValue(&s);
    nsAutoString uri(s);

    for (NameSpaceMap* entry = mNameSpaces; entry != nsnull; entry = entry->Next) {
        if (uri.Find(entry->URI) == 0) {
            nameSpaceURI    = entry->URI;
            if (entry->Prefix) {
                entry->Prefix->ToString(nameSpacePrefix);
            }
            else {
                nameSpacePrefix.Truncate();
            }
            uri.Right(property, uri.Length() - nameSpaceURI.Length());
            return PR_TRUE;
        }
    }

    // Okay, so we don't have it in our map. Try to make one up.
    PRInt32 index = uri.RFind('#'); // first try a '#'
    if (index == -1) {
        index = uri.RFind('/');
        if (index == -1) {
            // Okay, just punt and assume there is _no_ namespace on
            // this thing...
            NS_ASSERTION(PR_FALSE, "couldn't find reasonable namespace prefix");
            nameSpaceURI.Truncate();
            nameSpacePrefix.Truncate();
            property = uri;
            return PR_TRUE;
        }
    }

    // Take whatever is to the right of the '#' and call it the
    // property.
    property.Truncate();
    nameSpaceURI.Right(property, uri.Length() - (index + 1));

    // Truncate the namespace URI down to the string up to and
    // including the '#'.
    nameSpaceURI = uri;
    nameSpaceURI.Truncate(index + 1);

    // Just generate a random prefix
    static PRInt32 gPrefixID = 0;
    nameSpacePrefix = "NS";
    nameSpacePrefix.Append(++gPrefixID, 10);
    return PR_FALSE;
}

// convert '<' and '>' into '&lt;' and '&gt', respectively.
static void
rdf_EscapeAngleBrackets(nsString& s)
{
    PRInt32 index;
    while ((index = s.Find('<')) != -1) {
        s.SetCharAt('&',index);
        s.Insert(nsAutoString("lt;"), index + 1);
    }

    while ((index = s.Find('>')) != -1) {
        s.SetCharAt('&',index);
        s.Insert(nsAutoString("gt;"), index + 1);
    }
}

static void
rdf_EscapeAmpersands(nsString& s)
{
    PRInt32 index = 0;
    while ((index = s.Find('&', index)) != -1) {
        s.SetCharAt('&',index);
        s.Insert(nsAutoString("amp;"), index + 1);
        index += 4;
    }
}

nsresult
RDFXMLDataSourceImpl::SerializeAssertion(nsIOutputStream* aStream,
                                         nsIRDFResource* aResource,
                                         nsIRDFResource* aProperty,
                                         nsIRDFNode* aValue)
{
    nsAutoString property, nameSpacePrefix, nameSpaceURI;
    nsAutoString tag;

    PRBool wasDefinedAtGlobalScope =
        MakeQName(aProperty, property, nameSpacePrefix, nameSpaceURI);

    if (nameSpacePrefix.Length()) {
        tag.Append(nameSpacePrefix);
        tag.Append(':');
    }
    tag.Append(property);

    rdf_BlockingWrite(aStream, "    <", 5);
    rdf_BlockingWrite(aStream, tag);

    if (!wasDefinedAtGlobalScope && nameSpacePrefix.Length()) {
        rdf_BlockingWrite(aStream, " xmlns:", 7);
        rdf_BlockingWrite(aStream, nameSpacePrefix);
        rdf_BlockingWrite(aStream, "=\"", 2);
        rdf_BlockingWrite(aStream, nameSpaceURI);
        rdf_BlockingWrite(aStream, "\"", 1);
    }

    nsIRDFResource* resource;
    nsIRDFLiteral* literal;

    if (NS_SUCCEEDED(aValue->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
        const char* s;
        resource->GetValue(&s);

        const char* docURI;
        mInner->GetURI(&docURI);

        nsAutoString uri(s);
        rdf_PossiblyMakeRelative(docURI, uri);
        rdf_EscapeAmpersands(uri);

static const char kRDFResource1[] = " RDF:resource=\"";
static const char kRDFResource2[] = "\"/>\n";
        rdf_BlockingWrite(aStream, kRDFResource1, sizeof(kRDFResource1) - 1);
        rdf_BlockingWrite(aStream, uri);
        rdf_BlockingWrite(aStream, kRDFResource2, sizeof(kRDFResource2) - 1);

        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(aValue->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
        const PRUnichar* value;
        literal->GetValue(&value);
        nsAutoString s(value);

        rdf_EscapeAmpersands(s); // do these first!
        rdf_EscapeAngleBrackets(s);

        rdf_BlockingWrite(aStream, ">", 1);
        rdf_BlockingWrite(aStream, s);
        rdf_BlockingWrite(aStream, "</", 2);
        rdf_BlockingWrite(aStream, tag);
        rdf_BlockingWrite(aStream, ">\n", 2);

        NS_RELEASE(literal);
    }
    else {
        // XXX it doesn't support nsIRDFResource _or_ nsIRDFLiteral???
        NS_ASSERTION(PR_FALSE, "huh?");
    }

    return NS_OK;
}


nsresult
RDFXMLDataSourceImpl::SerializeProperty(nsIOutputStream* aStream,
                                        nsIRDFResource* aResource,
                                        nsIRDFResource* aProperty)
{
    nsresult rv;

    nsIRDFAssertionCursor* assertions = nsnull;
    if (NS_FAILED(rv = mInner->GetTargets(aResource, aProperty, PR_TRUE, &assertions)))
        return rv;

    while (NS_SUCCEEDED(rv = assertions->Advance())) {
        nsIRDFNode* value;
        if (NS_FAILED(rv = assertions->GetValue(&value)))
            break;

        rv = SerializeAssertion(aStream, aResource, aProperty, value);
        NS_RELEASE(value);

        if (NS_FAILED(rv))
            break;
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        rv = NS_OK;

    NS_RELEASE(assertions);
    return rv;
}


nsresult
RDFXMLDataSourceImpl::SerializeDescription(nsIOutputStream* aStream,
                                           nsIRDFResource* aResource)
{
static const char kRDFDescription1[] = "  <RDF:Description RDF:about=\"";
static const char kRDFDescription2[] = "\">\n";
static const char kRDFDescription3[] = "  </RDF:Description>\n";

    nsresult rv;

    // XXX Look for an "RDF:type" property: if one exists, then output
    // as a "typed node" instead of the more verbose "RDF:Description
    // RDF:type='...'".

    const char* s;
    if (NS_FAILED(rv = aResource->GetValue(&s)))
        return rv;

    const char* docURI;
    mInner->GetURI(&docURI);

    nsAutoString uri(s);
    rdf_PossiblyMakeRelative(docURI, uri);
    rdf_EscapeAmpersands(uri);

    rdf_BlockingWrite(aStream, kRDFDescription1, sizeof(kRDFDescription1) - 1);
    rdf_BlockingWrite(aStream, uri);
    rdf_BlockingWrite(aStream, kRDFDescription2, sizeof(kRDFDescription2) - 1);

    nsIRDFArcsOutCursor* arcs = nsnull;
    if (NS_FAILED(rv = mInner->ArcLabelsOut(aResource, &arcs)))
        return rv;

    while (NS_SUCCEEDED(rv = arcs->Advance())) {
        nsIRDFResource* property;
        if (NS_FAILED(rv = arcs->GetPredicate(&property)))
            break;

        rv = SerializeProperty(aStream, aResource, property);
        NS_RELEASE(property);

        if (NS_FAILED(rv))
            break;
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        rv = NS_OK;

    NS_IF_RELEASE(arcs);
    rdf_BlockingWrite(aStream, kRDFDescription3, sizeof(kRDFDescription3) - 1);

    return rv;
}

nsresult
RDFXMLDataSourceImpl::SerializeMember(nsIOutputStream* aStream,
                                      nsIRDFResource* aContainer,
                                      nsIRDFResource* aProperty)
{
    nsresult rv;

    // We open a cursor rather than just doing GetTarget() because
    // there may for some random reason be two or more elements with
    // the same ordinal value. Okay, I'm paranoid.

    nsIRDFAssertionCursor* cursor;
    if (NS_FAILED(rv = mInner->GetTargets(aContainer, aProperty, PR_TRUE, &cursor)))
        return rv;

    const char* docURI;
    mInner->GetURI(&docURI);

    while (NS_SUCCEEDED(rv = cursor->Advance())) {
        nsIRDFNode* node;

        if (NS_FAILED(rv = cursor->GetObject(&node)))
            break;

        // If it's a resource, then output a "<RDF:li resource=... />"
        // tag, because we'll be dumping the resource separately. (We
        // iterate thru all the resources in the datasource,
        // remember?) Otherwise, output the literal value.

        nsIRDFResource* resource = nsnull;
        nsIRDFLiteral* literal = nsnull;

        if (NS_SUCCEEDED(rv = node->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
            const char* s;
            if (NS_SUCCEEDED(rv = resource->GetValue(&s))) {
static const char kRDFLIResource1[] = "    <RDF:li RDF:resource=\"";
static const char kRDFLIResource2[] = "\"/>\n";

                nsAutoString uri(s);
                rdf_PossiblyMakeRelative(docURI, uri);
                rdf_EscapeAmpersands(uri);

                rdf_BlockingWrite(aStream, kRDFLIResource1, sizeof(kRDFLIResource1) - 1);
                rdf_BlockingWrite(aStream, uri);
                rdf_BlockingWrite(aStream, kRDFLIResource2, sizeof(kRDFLIResource2) - 1);
            }
            NS_RELEASE(resource);
        }
        else if (NS_SUCCEEDED(rv = node->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
            const PRUnichar* value;
            if (NS_SUCCEEDED(rv = literal->GetValue(&value))) {
static const char kRDFLILiteral1[] = "    <RDF:li>";
static const char kRDFLILiteral2[] = "</RDF:li>\n";
                rdf_BlockingWrite(aStream, kRDFLILiteral1, sizeof(kRDFLILiteral1) - 1);
                rdf_BlockingWrite(aStream, value);
                rdf_BlockingWrite(aStream, kRDFLILiteral2, sizeof(kRDFLILiteral2) - 1);
            }
            NS_RELEASE(literal);
        }
        else {
            NS_ASSERTION(PR_FALSE, "uhh -- it's not a literal or a resource?");
        }

        NS_RELEASE(node);
        if (NS_FAILED(rv))
            break;
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        rv = NS_OK;

    NS_RELEASE(cursor);
    return rv;
}


nsresult
RDFXMLDataSourceImpl::SerializeContainer(nsIOutputStream* aStream,
                                         nsIRDFResource* aContainer)
{
static const char kRDFBag[] = "RDF:Bag";
static const char kRDFSeq[] = "RDF:Seq";
static const char kRDFAlt[] = "RDF:Alt";

    nsresult rv;
    const char* tag;

    // Decide if it's a sequence, bag, or alternation, and print the
    // appropriate tag-open sequence

    if (rdf_IsBag(mInner, aContainer)) {
        tag = kRDFBag;
    }
    else if (rdf_IsSeq(mInner, aContainer)) {
        tag = kRDFSeq;
    }
    else if (rdf_IsAlt(mInner, aContainer)) {
        tag = kRDFAlt;
    }
    else {
        NS_ASSERTION(PR_FALSE, "huh? this is _not_ a container.");
        return NS_ERROR_UNEXPECTED;
    }

    rdf_BlockingWrite(aStream, "  <", 3);
    rdf_BlockingWrite(aStream, tag);


    // Unfortunately, we always need to print out the identity of the
    // resource, even if was constructed "anonymously". We need to do
    // this because we never really know who else might be referring
    // to it...

    const char* docURI;
    mInner->GetURI(&docURI);

    const char* s;
    if (NS_SUCCEEDED(aContainer->GetValue(&s))) {
        nsAutoString uri(s);
        rdf_PossiblyMakeRelative(docURI, uri);
        rdf_EscapeAmpersands(uri);
        rdf_BlockingWrite(aStream, " RDF:ID=\"", 9);
        rdf_BlockingWrite(aStream, uri);
        rdf_BlockingWrite(aStream, "\"", 1);
    }

    rdf_BlockingWrite(aStream, ">\n", 2);


    // We iterate through all of the arcs, in case someone has applied
    // properties to the bag itself.

    nsIRDFArcsOutCursor* arcs;
    if (NS_FAILED(rv = mInner->ArcLabelsOut(aContainer, &arcs)))
        return rv;

    while (NS_SUCCEEDED(rv = arcs->Advance())) {
        nsIRDFResource* property;

        if (NS_FAILED(rv = arcs->GetPredicate(&property)))
            break;

        // If it's a membership property, then output a "LI"
        // tag. Otherwise, output a property.
        if (rdf_IsOrdinalProperty(property)) {
            rv = SerializeMember(aStream, aContainer, property);
        }
        else {
            do {
                PRBool eq;

                // don't serialize RDF:instanceOf -- it's implicit in the tag
                if (NS_FAILED(rv = property->EqualsString(kURIRDF_instanceOf, &eq)))
                    break;

                if (eq)
                    break;

                // don't serialize RDF:nextVal -- it's internal state
                if (NS_FAILED(rv = property->EqualsString(kURIRDF_nextVal, &eq)))
                    break;

                if (eq)
                    break;

                rv = SerializeProperty(aStream, aContainer, property);
            } while (0);
        }

        NS_RELEASE(property);
        if (NS_FAILED(rv))
            break;
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        rv = NS_OK;

    NS_RELEASE(arcs);


    // close the container tag
    rdf_BlockingWrite(aStream, "  </", 4);
    rdf_BlockingWrite(aStream, tag);
    rdf_BlockingWrite(aStream, ">\n", 2);

    return rv;
}


nsresult
RDFXMLDataSourceImpl::SerializePrologue(nsIOutputStream* aStream)
{
static const char kXMLVersion[] = "<?xml version=\"1.0\"?>\n";
static const char kOpenRDF[]  = "<RDF:RDF";
static const char kXMLNS[]    = "\n     xmlns:";

    rdf_BlockingWrite(aStream, kXMLVersion, sizeof(kXMLVersion) - 1);

    PRInt32 i;

    // Write out style sheet processing instructions
    for (i = 0; i < mNumCSSStyleSheetURLs; ++i) {
static const char kCSSStyleSheet1[] = "<?xml-stylesheet href=\"";
static const char kCSSStyleSheet2[] = "\" type=\"text/css\"?>\n";

        const char* url;
        mCSSStyleSheetURLs[i]->GetSpec(&url);
        rdf_BlockingWrite(aStream, kCSSStyleSheet1, sizeof(kCSSStyleSheet1) - 1);
        rdf_BlockingWrite(aStream, url);
        rdf_BlockingWrite(aStream, kCSSStyleSheet2, sizeof(kCSSStyleSheet2) - 1);
    }

    // Write out named data source processing instructions
    for (i = 0; i < mNumNamedDataSourceURIs; ++i) {
static const char kNamedDataSource1[] = "<?rdf-datasource href=\"";
static const char kNamedDataSource2[] = "\"?>\n";

        rdf_BlockingWrite(aStream, kNamedDataSource1, sizeof(kNamedDataSource1) - 1);
        rdf_BlockingWrite(aStream, mNamedDataSourceURIs[i]);
        rdf_BlockingWrite(aStream, kNamedDataSource2, sizeof(kNamedDataSource2) - 1);
    }

    // global name space declarations
    rdf_BlockingWrite(aStream, kOpenRDF, sizeof(kOpenRDF) - 1);
    for (NameSpaceMap* entry = mNameSpaces; entry != nsnull; entry = entry->Next) {
        rdf_BlockingWrite(aStream, kXMLNS, sizeof(kXMLNS) - 1);

        nsAutoString prefix;
        entry->Prefix->ToString(prefix);
        rdf_BlockingWrite(aStream, prefix);

        rdf_BlockingWrite(aStream, "=\"", 2);
        rdf_BlockingWrite(aStream, entry->URI);
        rdf_BlockingWrite(aStream, "\"", 1);
    }
    rdf_BlockingWrite(aStream, ">\n", 2);
    return NS_OK;
}


nsresult
RDFXMLDataSourceImpl::SerializeEpilogue(nsIOutputStream* aStream)
{
static const char kCloseRDF[] = "</RDF:RDF>\n";

    rdf_BlockingWrite(aStream, kCloseRDF);
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Serialize(nsIOutputStream* aStream)
{
    nsresult rv;
    nsIRDFResourceCursor* resources = nsnull;

    if (NS_FAILED(rv = mInner->GetAllResources(&resources)))
        goto done;

    if (NS_FAILED(rv = SerializePrologue(aStream)))
        goto done;

    while (NS_SUCCEEDED(rv = resources->Advance())) {
        nsIRDFResource* resource;
        if (NS_FAILED(rv = resources->GetResource(&resource)))
            break;

        if (rdf_IsContainer(mInner, resource)) {
            rv = SerializeContainer(aStream, resource);
        }
        else {
            rv = SerializeDescription(aStream, resource);
        }
        NS_RELEASE(resource);

        if (NS_FAILED(rv))
            break;
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        rv = NS_OK;

    if (NS_FAILED(rv))
        goto done;

    rv = SerializeEpilogue(aStream);

done:
    NS_IF_RELEASE(resources);
    return rv;
}


