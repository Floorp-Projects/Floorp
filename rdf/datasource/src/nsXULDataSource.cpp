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
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFXMLDataSource.h"
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

static NS_DEFINE_IID(kIRDFXMLDataSourceIID,  NS_IRDFXMLDATASOURCE_IID);
static NS_DEFINE_IID(kXULContentSinkCID,	 NS_XULCONTENTSINK_CID);

////////////////////////////////////////////////////////////////////////
// Vocabulary stuff
#include "rdf.h"
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, nextVal);


////////////////////////////////////////////////////////////////////////
// XULDataSourceImpl

class XULDataSourceImpl : public nsIRDFXMLDataSource
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
    XULDataSourceImpl(void);
    virtual ~XULDataSourceImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(char* *uri) {
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
                          nsISimpleEnumerator** sources) {
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
                          nsISimpleEnumerator** targets) {
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
                           nsISimpleEnumerator** labels) {
        return mInner->ArcLabelsIn(node, labels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsISimpleEnumerator** labels) {
        return mInner->ArcLabelsOut(source, labels);
    }

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult) {
        return mInner->GetAllResources(aResult);
    }

    NS_IMETHOD Flush(void);

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands) {
        return mInner->GetAllCommands(source, commands);
    }

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult) {
        return mInner->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
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
};


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXULDataSource(nsIRDFXMLDataSource** result)
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
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
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
		iid.Equals(kIRDFDataSourceIID) ||
		iid.Equals(kIRDFXMLDataSourceIID)) {
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

    if (NS_FAILED(rv = rdfService->RegisterDataSource(this, PR_FALSE)))
        goto done;

    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                    nsnull,
                                                    kINameSpaceManagerIID,
                                                    (void**) &ns)))
        goto done;

    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kXULContentSinkCID,
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

////////////////////////////////////////////////////////////////////////
// nsIRDFXMLDataSource methods

NS_IMETHODIMP
XULDataSourceImpl::SetSynchronous(PRBool aIsSynchronous)
{
    mIsSynchronous = aIsSynchronous;
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::SetReadOnly(PRBool aIsReadOnly)
{
    if (mIsWritable && aIsReadOnly)
        mIsWritable = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::BeginLoad(void)
{
    mIsLoading = PR_TRUE;
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnBeginLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::Interrupt(void)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnInterrupt(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::Resume(void)
{
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnResume(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::EndLoad(void)
{
    mIsLoading = PR_FALSE;
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLDataSourceObserver* obs = (nsIRDFXMLDataSourceObserver*) mObservers[i];
        obs->OnEndLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::SetRootResource(nsIRDFResource* aResource)
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
XULDataSourceImpl::GetRootResource(nsIRDFResource** aResource)
{
    NS_ADDREF(mRootResource);
    *aResource = mRootResource;
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::AddCSSStyleSheetURL(nsIURL* aCSSStyleSheetURL)
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
XULDataSourceImpl::GetCSSStyleSheetURLs(nsIURL*** aCSSStyleSheetURLs, PRInt32* aCount)
{
    *aCSSStyleSheetURLs = mCSSStyleSheetURLs;
    *aCount = mNumCSSStyleSheetURLs;
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::AddNamedDataSourceURI(const char* aNamedDataSourceURI)
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
XULDataSourceImpl::GetNamedDataSourceURIs(const char* const** aNamedDataSourceURIs, PRInt32* aCount)
{
    *aNamedDataSourceURIs = mNamedDataSourceURIs;
    *aCount = mNumNamedDataSourceURIs;
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::AddNameSpace(nsIAtom* aPrefix, const nsString& aURI)
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

    NS_ADDREF(aPrefix);
    entry->Prefix = aPrefix;
    entry->URI = aURI;
    entry->Next = mNameSpaces;
    mNameSpaces = entry;
    return NS_OK;
}


NS_IMETHODIMP
XULDataSourceImpl::AddXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver)
{
    mObservers.AppendElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP
XULDataSourceImpl::RemoveXMLStreamObserver(nsIRDFXMLDataSourceObserver* aObserver)
{
    mObservers.RemoveElement(aObserver);
    return NS_OK;
}


