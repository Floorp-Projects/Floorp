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
  XUL stream.

  TO DO
  -----

  1) Write code to serialize XUL back.

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
#include "nsIRDFDataSource.h"
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

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDTDIID,               NS_IDTD_IID);
static NS_DEFINE_IID(kIInputStreamIID,       NS_IINPUTSTREAM_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,  NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIOutputStreamIID,      NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kIParserIID,            NS_IPARSER_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,    NS_IRDFCONTENTSINK_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,     NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,        NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,       NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,        NS_IRDFSERVICE_IID);
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
// XULDataSourceImpl

class XULDataSourceImpl : public nsIRDFDataSource
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
    nsID              mContentModelBuilderCID;

public:
    XULDataSourceImpl(void);
    virtual ~XULDataSourceImpl(void);

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
                      PRBool tv) {
        return mInner->Assert(source, property, target, tv);;
    }

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target) {
        return mInner->Unassert(source, property, target);
    }

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

};


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXULDataSource(nsIRDFDataSource** result)
{
    XULDataSourceImpl* ds = new XULDataSourceImpl();
    if (! ds)
        return NS_ERROR_NULL_POINTER;

    *result = ds;
    NS_ADDREF(*result);
    return NS_OK;
}


XULDataSourceImpl::XULDataSourceImpl(void)
    : mIsSynchronous(PR_FALSE),
      mIsWritable(PR_TRUE),
      mIsDirty(PR_FALSE),
      mNamedDataSourceURIs(nsnull),
      mNumNamedDataSourceURIs(0),
      mCSSStyleSheetURLs(nsnull),
      mNumCSSStyleSheetURLs(0),
      mIsLoading(PR_FALSE),
      mNameSpaces(nsnull)
{
    nsresult rv;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mInner)))
        PR_ASSERT(0);

    NS_INIT_REFCNT();
}


XULDataSourceImpl::~XULDataSourceImpl(void)
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

        NS_RELEASE(doomed->Prefix);
        delete doomed;
    }
}


NS_IMPL_ADDREF(XULDataSourceImpl);
NS_IMPL_RELEASE(XULDataSourceImpl);

NS_IMETHODIMP
XULDataSourceImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kISupportsIID) ||
        iid.Equals(kIRDFDataSourceIID)) {
        *result = NS_STATIC_CAST(nsIRDFDataSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}


NS_IMETHODIMP
XULDataSourceImpl::Init(const char* uri)
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

#if 0
    if (NS_FAILED(rv = nsRepository::CreateInstance(kXULContentSinkCID,
                                                    nsnull,
                                                    kIXULContentSinkIID,
                                                    (void**) &sink)))
        goto done;

    if (NS_FAILED(sink->Init(url, ns)))
        goto done;

    // We set the content sink's data source directly to our in-memory
    // store. This allows the initial content to be generated "directly".
    if (NS_FAILED(rv = sink->SetDataSource(this)))
        goto done;
#endif

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

    rv = NS_OpenURL(url, lsnr);

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
XULDataSourceImpl::Flush(void)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

