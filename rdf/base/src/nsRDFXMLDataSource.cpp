/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  A data source that can read itself from and write itself to an
  RDF/XML stream.

  For more information on the RDF/XML syntax,
  see http://www.w3.org/TR/REC-rdf-syntax/.

  This code is based on the final W3C Recommendation,
  http://www.w3.org/TR/1999/REC-rdf-syntax-19990222.


  TO DO
  -----

  1) Right now, the only kind of stream data sources that are _really_
     writable are "file:" URIs. (In fact, _all_ "file:" URIs are
     writable, modulo file system permissions; this may lead to some
     surprising behavior.) Eventually, it'd be great if we could open
     an arbitrary nsIOutputStream on *any* URL, and Netlib could just
     do the magic.

  2) Implement a more terse output for "typed" nodes; that is, instead
     of "RDF:Description type='ns:foo'", just output "ns:foo".

  3) When re-serializing, we "cheat" for Descriptions that talk about
     inline resources (i.e.., using the `ID' attribute specified in
     [6.21]). Instead of writing an `ID="foo"' for the first instance,
     and then `about="#foo"' for each subsequent instance, we just
     _always_ write `about="#foo"'.

     We do this so that we can handle the case where an RDF container
     has been assigned arbitrary properties: the spec says we can't
     dangle the attributes directly off the container, so we need to
     refer to it. Of course, with a little cleverness, we could fix
     this. But who cares?

  4) When re-serializing containers. We have to cheat on some
     containers, and use an illegal "about=" construct. We do this to
     handle containers that have been assigned URIs outside of the
     local document.

 */

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIDTD.h"
#include "nsIRDFPurgeableDataSource.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIParser.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSink.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsLayoutCID.h" // for NS_NAMESPACEMANAGER_CID.
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFBaseDataSources.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "prio.h"
#include "prthread.h"
#include "rdf.h"
#include "rdfutil.h"
#include "prlog.h"

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIDTDIID,               NS_IDTD_IID);
static NS_DEFINE_IID(kIInputStreamIID,       NS_IINPUTSTREAM_IID);
static NS_DEFINE_IID(kIParserIID,            NS_IPARSER_IID);
static NS_DEFINE_IID(kIStreamListenerIID,    NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,        NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

//----------------------------------------------------------------------
//
// ProxyStream
//
//   A silly little stub class that lets us do blocking parses
//

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
    NS_IMETHOD Available(PRUint32 *aLength) {
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

    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("ReadSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {
        NS_NOTREACHED("GetNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD GetObserver(nsIInputStreamObserver * *aObserver) {
        NS_NOTREACHED("GetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD SetObserver(nsIInputStreamObserver * aObserver) {
        NS_NOTREACHED("SetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    // Implementation
    void SetBuffer(const char* aBuffer, PRUint32 aSize) {
        mBuffer = aBuffer;
        mSize = aSize;
        mIndex = 0;
    }
};

NS_IMPL_ISUPPORTS(ProxyStream, kIInputStreamIID);

//----------------------------------------------------------------------
//
// RDFXMLDataSourceImpl
//

class RDFXMLDataSourceImpl : public nsIRDFDataSource,
                             public nsIRDFRemoteDataSource,
                             public nsIRDFXMLSink,
                             public nsIRDFXMLSource,
                             public nsIStreamListener
{
protected:
    struct NameSpaceMap {
        nsString      URI;
        nsIAtom*      Prefix;
        NameSpaceMap* Next;
    };

    enum LoadState {
        eLoadState_Unloaded,
        eLoadState_Pending,
        eLoadState_Loading,
        eLoadState_Loaded
    };

    nsIRDFDataSource*   mInner;         // OWNER
    PRPackedBool        mIsWritable;    // true if the document can be written back
    PRPackedBool        mIsDirty;       // true if the document should be written back
    LoadState           mLoadState;     // what we're doing now
    nsVoidArray         mObservers;     // OWNER
    NameSpaceMap*       mNameSpaces;
    nsCOMPtr<nsIURI>    mURL;
    nsCOMPtr<nsIStreamListener> mParser;
    char*               mURLSpec;

    // pseudo-constants
    static PRInt32 gRefCnt;
    static nsIRDFContainerUtils* gRDFC;
    static nsIRDFService*  gRDFService;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_nextVal;
    static nsIRDFResource* kRDF_Bag;
    static nsIRDFResource* kRDF_Seq;
    static nsIRDFResource* kRDF_Alt;

    nsresult Init();
    RDFXMLDataSourceImpl(void);
    virtual ~RDFXMLDataSourceImpl(void);

    friend nsresult
    NS_NewRDFXMLDataSource(nsIRDFDataSource** aResult);

    inline PRBool IsLoading() {
        return (mLoadState == eLoadState_Pending) || 
               (mLoadState == eLoadState_Loading);
    }

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource
    NS_IMETHOD GetURI(char* *uri);

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

    NS_IMETHOD Assert(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget,
                      PRBool tv);

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target);

    NS_IMETHOD Change(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aOldTarget,
                      nsIRDFNode* aNewTarget);

    NS_IMETHOD Move(nsIRDFResource* aOldSource,
                    nsIRDFResource* aNewSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aTarget);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion) {
        return mInner->HasAssertion(source, property, target, tv, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* aObserver) {
        return mInner->AddObserver(aObserver);
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver) {
        return mInner->RemoveObserver(aObserver);
    }

    NS_IMETHOD HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *_retval) {
        return mInner->HasArcIn(aNode, aArc, _retval);
    }

    NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval) {
        return mInner->HasArcOut(aSource, aArc, _retval);
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

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands) {
        return mInner->GetAllCommands(source, commands);
    }

    NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                              nsISimpleEnumerator/*<nsIRDFResource>*/** commands) {
        return mInner->GetAllCmds(source, commands);
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

    // nsIRDFRemoteDataSource interface
    NS_DECL_NSIRDFREMOTEDATASOURCE

    // nsIRDFXMLSink interface
    NS_DECL_NSIRDFXMLSINK

    // nsIRDFXMLSource interface
    NS_DECL_NSIRDFXMLSOURCE

    // nsIStreamObserver
    NS_DECL_NSISTREAMOBSERVER

    // nsIStreamListener
    NS_DECL_NSISTREAMLISTENER

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

    PRBool
    IsContainerProperty(nsIRDFResource* aProperty);

    nsresult
    SerializeDescription(nsIOutputStream* aStream,
                         nsIRDFResource* aResource);

    nsresult
    SerializeMember(nsIOutputStream* aStream,
                    nsIRDFResource* aContainer,
                    nsIRDFNode* aMember);

    nsresult
    SerializeContainer(nsIOutputStream* aStream,
                       nsIRDFResource* aContainer);

    nsresult
    SerializePrologue(nsIOutputStream* aStream);

    nsresult
    SerializeEpilogue(nsIOutputStream* aStream);

    PRBool
    IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType);
};

PRInt32         RDFXMLDataSourceImpl::gRefCnt = 0;
nsIRDFService*  RDFXMLDataSourceImpl::gRDFService;
nsIRDFContainerUtils* RDFXMLDataSourceImpl::gRDFC;
nsIRDFResource* RDFXMLDataSourceImpl::kRDF_instanceOf;
nsIRDFResource* RDFXMLDataSourceImpl::kRDF_nextVal;
nsIRDFResource* RDFXMLDataSourceImpl::kRDF_Bag;
nsIRDFResource* RDFXMLDataSourceImpl::kRDF_Seq;
nsIRDFResource* RDFXMLDataSourceImpl::kRDF_Alt;

//----------------------------------------------------------------------

nsresult
NS_NewRDFXMLDataSource(nsIRDFDataSource** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    RDFXMLDataSourceImpl* datasource = new RDFXMLDataSourceImpl();
    if (! datasource)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = datasource->Init();

    if (NS_FAILED(rv)) {
        delete datasource;
        return rv;
    }

    NS_ADDREF(datasource);
    *aResult = datasource;
    return NS_OK;
}


RDFXMLDataSourceImpl::RDFXMLDataSourceImpl(void)
    : mInner(nsnull),
      mIsWritable(PR_TRUE),
      mIsDirty(PR_FALSE),
      mLoadState(eLoadState_Unloaded),
      mNameSpaces(nsnull),
      mURLSpec(nsnull)
{
    NS_INIT_REFCNT();

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFXMLDataSource");
#endif
}


nsresult
RDFXMLDataSourceImpl::Init()
{
    nsresult rv;
    rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFDataSource),
                                            (void**) &mInner);
    if (NS_FAILED(rv)) return rv;

    // Initialize the name space stuff to know about any "standard"
    // namespaces that we want to look the same in all the RDF/XML we
    // generate.
    //
    // XXX this is a bit of a hack, because technically, the document
    // should've defined the RDF namespace to be _something_, and we
    // should just look at _that_ and use it. Oh well.
    nsIAtom* rdfPrefix = NS_NewAtom("RDF");
    AddNameSpace(rdfPrefix, NS_ConvertASCIItoUCS2(RDF_NAMESPACE_URI));
    NS_IF_RELEASE(rdfPrefix);

    if (gRefCnt++ == 0) {
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          NS_REINTERPRET_CAST(nsISupports**, &gRDFService));

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_FAILED(rv)) return rv;

        rv = gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
        rv = gRDFService->GetResource(RDF_NAMESPACE_URI "nextVal",    &kRDF_nextVal);
        rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Bag",        &kRDF_Bag);
        rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Seq",        &kRDF_Seq);
        rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Alt",        &kRDF_Alt);

        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          NS_GET_IID(nsIRDFContainerUtils),
                                          (nsISupports**) &gRDFC);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get container utils");
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


RDFXMLDataSourceImpl::~RDFXMLDataSourceImpl(void)
{
    nsresult rv;

    // Unregister first so that nobody else tries to get us.
    rv = gRDFService->UnregisterDataSource(this);

    // Now flush contents
    rv = Flush();

    // Release RDF/XML sink observers
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLSinkObserver* obs = NS_STATIC_CAST(nsIRDFXMLSinkObserver*, mObservers[i]);
        NS_RELEASE(obs);
    }

    if (mURLSpec)
        PL_strfree(mURLSpec);

    while (mNameSpaces) {
        NameSpaceMap* doomed = mNameSpaces;
        mNameSpaces = mNameSpaces->Next;

        NS_IF_RELEASE(doomed->Prefix);
        delete doomed;
    }

    NS_RELEASE(mInner);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kRDF_Bag);
        NS_IF_RELEASE(kRDF_Seq);
        NS_IF_RELEASE(kRDF_Alt);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_nextVal);

        if (gRDFC) {
            nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFC);
            gRDFC = nsnull;
        }

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }
    }
}


NS_IMPL_ISUPPORTS6(RDFXMLDataSourceImpl,
                   nsIRDFDataSource,
                   nsIRDFRemoteDataSource,
                   nsIRDFXMLSink,
                   nsIRDFXMLSource,
                   nsIStreamObserver,
                   nsIStreamListener);


static nsresult
rdf_BlockingParse(nsIURI* aURL, nsIStreamListener* aConsumer)
{
    nsresult rv;

    // XXX I really hate the way that we're spoon-feeding this stuff
    // to the parser: it seems like this is something that netlib
    // should be able to do by itself.

    nsCOMPtr<nsIChannel> channel;
    // Null LoadGroup ?
    rv = NS_OpenURI(getter_AddRefs(channel), aURL, nsnull);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* in;
    PRUint32 sourceOffset = 0;
    rv = channel->OpenInputStream(&in);

    // If we couldn't open the channel, then just return.
    if (NS_FAILED(rv)) return NS_OK;

    NS_ASSERTION(in != nsnull, "no input stream");
    if (! in) return NS_ERROR_FAILURE;

    rv = NS_ERROR_OUT_OF_MEMORY;
    ProxyStream* proxy = new ProxyStream();
    if (! proxy)
        goto done;

    aConsumer->OnStartRequest(channel, nsnull);
    while (PR_TRUE) {
        char buf[1024];
        PRUint32 readCount;

        if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
            break; // error

        if (readCount == 0)
            break; // eof

        proxy->SetBuffer(buf, readCount);

        rv = aConsumer->OnDataAvailable(channel, nsnull, proxy, sourceOffset, readCount);
        sourceOffset += readCount;
        if (NS_FAILED(rv))
            break;
    }

    aConsumer->OnStopRequest(channel, nsnull, rv, nsnull);

	// don't leak proxy!
	proxy->Close();
	delete proxy;
	proxy = nsnull;

done:
    NS_RELEASE(in);
    return rv;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::GetLoaded(PRBool* _result)
{
    *_result = (mLoadState == eLoadState_Loaded);
    return NS_OK;
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

    rv = NS_NewURI(getter_AddRefs(mURL), uri);
    if (NS_FAILED(rv)) return rv;

    // XXX this is a hack: any "file:" URI is considered writable. All
    // others are considered read-only.
    char* realURL;
    mURL->GetSpec(&realURL);
    if ((PL_strncmp(realURL, kFileURIPrefix, sizeof(kFileURIPrefix) - 1) != 0) &&
        (PL_strncmp(realURL, kResourceURIPrefix, sizeof(kResourceURIPrefix) - 1) != 0)) {
        mIsWritable = PR_FALSE;
    }

    // XXX Keep a 'cached' copy of the URL; opening it may cause the
    // spec to be re-written.
    if (mURLSpec)
        PL_strfree(mURLSpec);

    mURLSpec = PL_strdup(realURL);
    nsCRT::free(realURL);

    rv = gRDFService->RegisterDataSource(this, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::GetURI(char* *aURI)
{
    *aURI = nsnull;
    if (mURLSpec) {
        // XXX We don't use the mURL, because it might get re-written
        // when it's actually opened.
        *aURI = nsXPIDLCString::Copy(mURLSpec);
        if (! *aURI)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Assert(nsIRDFResource* aSource,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aTarget,
                             PRBool aTruthValue)
{
    // We don't accept assertions unless we're writable (except in the
    // case that we're actually _reading_ the datasource in).
    nsresult rv;

    if (IsLoading()) {
        PRBool hasAssertion = PR_FALSE;

        nsCOMPtr<nsIRDFPurgeableDataSource> gcable = do_QueryInterface(mInner);
        if (gcable) {
            rv = gcable->Mark(aSource, aProperty, aTarget, aTruthValue, &hasAssertion);
            if (NS_FAILED(rv)) return rv;
        }

        rv = NS_RDF_ASSERTION_ACCEPTED;

        if (! hasAssertion) {
            rv = mInner->Assert(aSource, aProperty, aTarget, aTruthValue);

            if (NS_SUCCEEDED(rv) && gcable) {
                // Now mark the new assertion, so it doesn't get
                // removed when we sweep. Ignore rv, because we want
                // to return what mInner->Assert() gave us.
                PRBool didMark;
                (void) gcable->Mark(aSource, aProperty, aTarget, aTruthValue, &didMark);
            }

            if (NS_FAILED(rv)) return rv;
        }

        return rv;
    }
    else if (mIsWritable) {
        rv = mInner->Assert(aSource, aProperty, aTarget, aTruthValue);

        if (rv == NS_RDF_ASSERTION_ACCEPTED)
            mIsDirty = PR_TRUE;

        return rv;
    }
    else {
        return NS_RDF_ASSERTION_REJECTED;
    }
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::Unassert(nsIRDFResource* source,
                               nsIRDFResource* property,
                               nsIRDFNode* target)
{
    // We don't accept assertions unless we're writable (except in the
    // case that we're actually _reading_ the datasource in).
    nsresult rv;

    if (IsLoading() || mIsWritable) {
        rv = mInner->Unassert(source, property, target);
        if (!IsLoading() && rv == NS_RDF_ASSERTION_ACCEPTED)
            mIsDirty = PR_TRUE;
    }
    else {
        rv = NS_RDF_ASSERTION_REJECTED;
    }

    return rv;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Change(nsIRDFResource* aSource,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aOldTarget,
                             nsIRDFNode* aNewTarget)
{
    nsresult rv;

    if (IsLoading() || mIsWritable) {
        rv = mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);

        if (!IsLoading() && rv == NS_RDF_ASSERTION_ACCEPTED)
            mIsDirty = PR_TRUE;
    }
    else {
        rv = NS_RDF_ASSERTION_REJECTED;
    }

    return rv;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Move(nsIRDFResource* aOldSource,
                           nsIRDFResource* aNewSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode* aTarget)
{
    nsresult rv;

    if (IsLoading() || mIsWritable) {
        rv = mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
        if (!IsLoading() && rv == NS_RDF_ASSERTION_ACCEPTED)
            mIsDirty = PR_TRUE;
    }
    else {
        rv = NS_RDF_ASSERTION_REJECTED;
    }

    return rv;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Flush(void)
{
    if (!mIsWritable || !mIsDirty)
        return NS_OK;

    NS_PRECONDITION(mURLSpec != nsnull, "not initialized");
    if (! mURLSpec)
        return NS_ERROR_NOT_INITIALIZED;

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml[%p] flush(%s)", this, mURLSpec));

    nsresult rv;

    // XXX Replace this with channels someday soon...
    nsFileURL url(mURLSpec, PR_TRUE);
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

//----------------------------------------------------------------------
//
// nsIRDFXMLDataSource methods
//

NS_IMETHODIMP
RDFXMLDataSourceImpl::GetReadOnly(PRBool* aIsReadOnly)
{
    *aIsReadOnly = !mIsWritable;
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
RDFXMLDataSourceImpl::Refresh(PRBool aBlocking)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml[%p] refresh(%s) %sblocking", this, mURLSpec, (aBlocking ? "" : "non")));

    // If an asynchronous load is already pending, then just let it do
    // the honors.
    if (IsLoading()) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml[%p] refresh(%s) a load was pending", this, mURLSpec));

        if (aBlocking) {
            NS_WARNING("blocking load requested when async load pending");
            return NS_ERROR_FAILURE;
        }
        else {
            return NS_OK;
        }
    }

    nsresult rv;

    nsCOMPtr<nsIRDFContentSink> sink;
    rv = nsComponentManager::CreateInstance(kRDFContentSinkCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFContentSink),
                                            getter_AddRefs(sink));
    if (NS_FAILED(rv)) return rv;

    rv = sink->Init(mURL);
    if (NS_FAILED(rv)) return rv;

    // We set the content sink's data source directly to our in-memory
    // store. This allows the initial content to be generated "directly".
    rv = sink->SetDataSource(this);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser;
    rv = nsComponentManager::CreateInstance(kParserCID,
                                            nsnull,
                                            kIParserIID,
                                            getter_AddRefs(parser));
    if (NS_FAILED(rv)) return rv;

    nsAutoString charset(NS_LITERAL_STRING("UTF-8"));
    parser->SetDocumentCharset(charset, kCharsetFromDocTypeDefault);
    parser->SetContentSink(sink);

    rv = parser->QueryInterface(NS_GET_IID(nsIStreamListener), getter_AddRefs(mParser));
    if (NS_FAILED(rv)) return rv;

    rv = parser->Parse(mURL);
    if (NS_FAILED(rv)) return rv;

    if (aBlocking) {
        rv = rdf_BlockingParse(mURL, this);

        mParser = nsnull; // release the parser

        if (NS_FAILED(rv)) return rv;
    }
    else {
        // Null LoadGroup ?
        rv = NS_OpenURI(this, nsnull, mURL, nsnull);
        if (NS_FAILED(rv)) return rv;

        // So we don't try to issue two asynchronous loads at once.
        mLoadState = eLoadState_Pending;
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::BeginLoad(void)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml[%p] begin-load(%s)", this, mURLSpec));

    mLoadState = eLoadState_Loading;
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLSinkObserver* obs = (nsIRDFXMLSinkObserver*) mObservers[i];
        obs->OnBeginLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Interrupt(void)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml[%p] interrupt(%s)", this, mURLSpec));

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLSinkObserver* obs = (nsIRDFXMLSinkObserver*) mObservers[i];
        obs->OnInterrupt(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Resume(void)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml[%p] resume(%s)", this, mURLSpec));

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLSinkObserver* obs = (nsIRDFXMLSinkObserver*) mObservers[i];
        obs->OnResume(this);
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::EndLoad(void)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml[%p] end-load(%s)", this, mURLSpec));

    mLoadState = eLoadState_Loaded;

    // Clear out any unmarked assertions from the datasource.
    nsCOMPtr<nsIRDFPurgeableDataSource> gcable = do_QueryInterface(mInner);
    if (gcable) {
        gcable->Sweep();
    }

    // Notify load observers
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFXMLSinkObserver* obs = (nsIRDFXMLSinkObserver*) mObservers[i];
        obs->OnEndLoad(this);
    }
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
RDFXMLDataSourceImpl::AddXMLSinkObserver(nsIRDFXMLSinkObserver* aObserver)
{
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(aObserver);
    mObservers.AppendElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::RemoveXMLSinkObserver(nsIRDFXMLSinkObserver* aObserver)
{
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (mObservers.RemoveElement(aObserver)) {
        // RemoveElement() returns PR_TRUE if it was actually there...
        NS_RELEASE(aObserver);
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIStreamObserver
//

NS_IMETHODIMP
RDFXMLDataSourceImpl::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
    return mParser->OnStartRequest(channel, ctxt);
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::OnStopRequest(nsIChannel *channel,
                                    nsISupports *ctxt,
                                    nsresult status,
                                    const PRUnichar *errorMsg)
{
    if (NS_FAILED(status)) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            nsIRDFXMLSinkObserver* obs = (nsIRDFXMLSinkObserver*) mObservers[i];
            (void) obs->OnError(this, status, errorMsg);
        }
    }

    nsresult rv;
    rv = mParser->OnStopRequest(channel, ctxt, status, errorMsg);

    mParser = nsnull; // release the parser

    return rv;
}

//----------------------------------------------------------------------
//
// nsIStreamListener
//

NS_IMETHODIMP
RDFXMLDataSourceImpl::OnDataAvailable(nsIChannel *channel,
                                      nsISupports *ctxt,
                                      nsIInputStream *inStr,
                                      PRUint32 sourceOffset,
                                      PRUint32 count)
{
    return mParser->OnDataAvailable(channel, ctxt, inStr, sourceOffset, count);
}

//----------------------------------------------------------------------
//
// nsIRDFXMLSource methods
//

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
    nsresult rv;
    char* utf8 = s.ToNewUTF8String();
    if (! utf8)
        return NS_ERROR_OUT_OF_MEMORY;

    // Be sure to grab the UTF-8 encoded string's length, because it
    // may be longer than the length of the unencoded string.
    PRInt32 len = PL_strlen(utf8);
    rv = rdf_BlockingWrite(stream, utf8, len);

    nsCRT::free(utf8);
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
    nsXPIDLCString s;
    resource->GetValue(getter_Copies(s));
    nsAutoString uri = NS_ConvertUTF8toUCS2(s);

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

    // Okay, so we don't have it in our map. Try to make one up. This
    // is very bogus.
    PRInt32 i = uri.RFindChar('#'); // first try a '#'
    if (i == -1) {
        i = uri.RFindChar('/');
        if (i == -1) {
            // Okay, just punt and assume there is _no_ namespace on
            // this thing...
            nameSpaceURI.Truncate();
            nameSpacePrefix.Truncate();
            property = uri;
            return PR_TRUE;
        }
    }

    // Take whatever is to the right of the '#' and call it the
    // property.
    property.Truncate();
    uri.Right(property, uri.Length() - (i + 1));

    // Truncate the namespace URI down to the string up to and
    // including the '#'.
    nameSpaceURI = uri;
    nameSpaceURI.Truncate(i + 1);

    // Just generate a random prefix
    static PRInt32 gPrefixID = 0;
    nameSpacePrefix = NS_LITERAL_STRING("NS");
    nameSpacePrefix.AppendInt(++gPrefixID, 10);
    return PR_FALSE;
}


PRBool
RDFXMLDataSourceImpl::IsContainerProperty(nsIRDFResource* aProperty)
{
    // Return `true' if the property is an internal property related
    // to being a container.
    if (aProperty == kRDF_instanceOf)
        return PR_TRUE;

    if (aProperty == kRDF_nextVal)
        return PR_TRUE;

    PRBool isOrdinal = PR_FALSE;
    gRDFC->IsOrdinalProperty(aProperty, &isOrdinal);
    if (isOrdinal)
        return PR_TRUE;

    return PR_FALSE;
} 


// convert '<' and '>' into '&lt;' and '&gt', respectively.
static void
rdf_EscapeAngleBrackets(nsString& s)
{
    PRInt32 i;
    while ((i = s.FindChar('<')) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("lt;"), i + 1);
    }

    while ((i = s.FindChar('>')) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("gt;"), i + 1);
    }
}

static void
rdf_EscapeAmpersands(nsString& s)
{
    PRInt32 i = 0;
    while ((i = s.FindChar('&', PR_FALSE,i)) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("amp;"), i + 1);
        i += 4;
    }
}

static void
rdf_EscapeQuotes(nsString& s)
{
    PRInt32 i = 0;
    while ((i = s.FindChar('"', PR_FALSE, i)) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("quot;"), i + 1);
        i += 5;
    }
}

static void
rdf_EscapeAttributeValue(nsString& s)
{
    rdf_EscapeAmpersands(s);
    rdf_EscapeAngleBrackets(s);
    rdf_EscapeQuotes(s);
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
        tag.AppendWithConversion(':');
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

    if (NS_SUCCEEDED(aValue->QueryInterface(NS_GET_IID(nsIRDFResource), (void**) &resource))) {
        nsXPIDLCString s;
        resource->GetValue(getter_Copies(s));

        nsXPIDLCString docURI;

        nsAutoString uri = NS_ConvertUTF8toUCS2(s);
        rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mURLSpec), uri);
        rdf_EscapeAttributeValue(uri);

static const char kRDFResource1[] = " resource=\"";
static const char kRDFResource2[] = "\"/>\n";
        rdf_BlockingWrite(aStream, kRDFResource1, sizeof(kRDFResource1) - 1);
        rdf_BlockingWrite(aStream, uri);
        rdf_BlockingWrite(aStream, kRDFResource2, sizeof(kRDFResource2) - 1);

        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(aValue->QueryInterface(NS_GET_IID(nsIRDFLiteral), (void**) &literal))) {
        nsXPIDLString value;
        literal->GetValue(getter_Copies(value));
        nsAutoString s((const PRUnichar*) value);

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

    nsCOMPtr<nsISimpleEnumerator> assertions;
    if (NS_FAILED(rv = mInner->GetTargets(aResource, aProperty, PR_TRUE, getter_AddRefs(assertions))))
        return rv;

    while (1) {
        PRBool hasMore;
        rv = assertions->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsIRDFNode* value;
        rv = assertions->GetNext((nsISupports**) &value);
        if (NS_FAILED(rv)) return rv;

        rv = SerializeAssertion(aStream, aResource, aProperty, value);
        NS_RELEASE(value);

        if (NS_FAILED(rv))
            break;
    }

    return NS_OK;
}


nsresult
RDFXMLDataSourceImpl::SerializeDescription(nsIOutputStream* aStream,
                                           nsIRDFResource* aResource)
{
static const char kRDFDescriptionAbout[] = "  <RDF:Description about=\"";
static const char kRDFDescriptionID[]    = "  <RDF:Description ID=\"";
static const char kRDFDescription2[] = "\">\n";
static const char kRDFDescription3[] = "  </RDF:Description>\n";

    nsresult rv;

    // XXX Look for an "RDF:type" property: if one exists, then output
    // as a "typed node" instead of the more verbose "RDF:Description
    // type='...'".

    nsXPIDLCString s;
    rv = aResource->GetValue(getter_Copies(s));
    if (NS_FAILED(rv)) return rv;

    nsAutoString uri = NS_ConvertUTF8toUCS2(s);
    rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mURLSpec), uri);
    rdf_EscapeAttributeValue(uri);

    if (uri[0] == PRUnichar('#')) {
        uri.Cut(0, 1);
        rdf_BlockingWrite(aStream, kRDFDescriptionID, sizeof(kRDFDescriptionID) - 1);
    }
    else {
        rdf_BlockingWrite(aStream, kRDFDescriptionAbout, sizeof(kRDFDescriptionAbout) - 1);
    }

    rdf_BlockingWrite(aStream, uri);
    rdf_BlockingWrite(aStream, kRDFDescription2, sizeof(kRDFDescription2) - 1);

    nsCOMPtr<nsISimpleEnumerator> arcs;
    rv = mInner->ArcLabelsOut(aResource, getter_AddRefs(arcs));
    if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsISupportsArray> array;
	rv = NS_NewISupportsArray(getter_AddRefs(array));
	if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
        NS_ASSERTION(property != nsnull, "not an nsIRDFResource!");
        if (! property)
            continue;

        // have we already seen this resource?  If so, don't write it out again
        if (array->IndexOf(property) >= 0)
            continue;

        array->AppendElement(property);

        if (! IsContainerProperty(property)) {
            // Ignore properties that pertain to containers; we may be
            // called from SerializeContainer() if the container
            // resource has been assigned non-container properties.
            rv = SerializeProperty(aStream, aResource, property);
        }

        if (NS_FAILED(rv))
            break;
    }

    rdf_BlockingWrite(aStream, kRDFDescription3, sizeof(kRDFDescription3) - 1);
    return NS_OK;
}

nsresult
RDFXMLDataSourceImpl::SerializeMember(nsIOutputStream* aStream,
                                      nsIRDFResource* aContainer,
                                      nsIRDFNode* aMember)
{
    nsresult rv;

    // If it's a resource, then output a "<RDF:li resource=... />"
    // tag, because we'll be dumping the resource separately. (We
    // iterate thru all the resources in the datasource,
    // remember?) Otherwise, output the literal value.

    nsIRDFResource* resource = nsnull;
    nsIRDFLiteral* literal = nsnull;

    if (NS_SUCCEEDED(rv = aMember->QueryInterface(NS_GET_IID(nsIRDFResource), (void**) &resource))) {
        nsXPIDLCString s;
        if (NS_SUCCEEDED(rv = resource->GetValue( getter_Copies(s) ))) {
static const char kRDFLIResource1[] = "    <RDF:li resource=\"";
static const char kRDFLIResource2[] = "\"/>\n";

            nsAutoString uri = NS_ConvertUTF8toUCS2(s);
            rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mURLSpec), uri);
            rdf_EscapeAttributeValue(uri);

            rdf_BlockingWrite(aStream, kRDFLIResource1, sizeof(kRDFLIResource1) - 1);
            rdf_BlockingWrite(aStream, uri);
            rdf_BlockingWrite(aStream, kRDFLIResource2, sizeof(kRDFLIResource2) - 1);
        }
        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(rv = aMember->QueryInterface(NS_GET_IID(nsIRDFLiteral), (void**) &literal))) {
        nsXPIDLString value;
        if (NS_SUCCEEDED(rv = literal->GetValue( getter_Copies(value) ))) {
static const char kRDFLILiteral1[] = "    <RDF:li>";
static const char kRDFLILiteral2[] = "</RDF:li>\n";

            rdf_BlockingWrite(aStream, kRDFLILiteral1, sizeof(kRDFLILiteral1) - 1);
            rdf_BlockingWrite(aStream, nsAutoString(NS_STATIC_CAST(const PRUnichar*, value)));
            rdf_BlockingWrite(aStream, kRDFLILiteral2, sizeof(kRDFLILiteral2) - 1);
        }
        NS_RELEASE(literal);
    }
    else {
        NS_ASSERTION(PR_FALSE, "uhh -- it's not a literal or a resource?");
    }
    return NS_OK;
}


nsresult
RDFXMLDataSourceImpl::SerializeContainer(nsIOutputStream* aStream,
                                         nsIRDFResource* aContainer)
{
    nsresult rv;
    nsAutoString tag;

    // Decide if it's a sequence, bag, or alternation, and print the
    // appropriate tag-open sequence

    if (IsA(mInner, aContainer, kRDF_Bag)) {
        tag = NS_LITERAL_STRING("RDF:Bag");
    }
    else if (IsA(mInner, aContainer, kRDF_Seq)) {
        tag = NS_LITERAL_STRING("RDF:Seq");
    }
    else if (IsA(mInner, aContainer, kRDF_Alt)) {
        tag = NS_LITERAL_STRING("RDF:Alt");
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

    nsXPIDLCString s;
    if (NS_SUCCEEDED(aContainer->GetValue( getter_Copies(s) ))) {
        nsAutoString uri = NS_ConvertUTF8toUCS2(s);
        rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mURLSpec), uri);

        rdf_EscapeAttributeValue(uri);

        if (uri.First() == PRUnichar('#')) {
            // Okay, it's actually identified as an element in the
            // current document, not trying to decorate some absolute
            // URI. We can use the 'ID=' attribute...
            static const char kIDEquals[] = " ID=\"";

            uri.Cut(0, 1); // chop the '#'
            rdf_BlockingWrite(aStream, kIDEquals, sizeof(kIDEquals) - 1);
        }
        else {
            // We need to cheat and spit out an illegal 'about=' on
            // the sequence. 
            static const char kAboutEquals[] = " about=\"";
            rdf_BlockingWrite(aStream, kAboutEquals, sizeof(kAboutEquals) - 1);
        }

        rdf_BlockingWrite(aStream, uri);
        rdf_BlockingWrite(aStream, "\"", 1);
    }

    rdf_BlockingWrite(aStream, ">\n", 2);

    // First iterate through each of the ordinal elements (the RDF/XML
    // syntax doesn't allow us to place properties on RDF container
    // elements).
    nsCOMPtr<nsISimpleEnumerator> elements;
    rv = NS_NewContainerEnumerator(mInner, aContainer, getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasMore;
        rv = elements->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = elements->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> element = do_QueryInterface(isupports);
        NS_ASSERTION(element != nsnull, "not an nsIRDFNode");
        if (! element)
            continue;

        rv = SerializeMember(aStream, aContainer, element);
        if (NS_FAILED(rv)) return rv;
    }

    // close the container tag
    rdf_BlockingWrite(aStream, "  </", 4);
    rdf_BlockingWrite(aStream, tag);
    rdf_BlockingWrite(aStream, ">\n", 2);


    // Now, we iterate through _all_ of the arcs, in case someone has
    // applied properties to the bag itself. These'll be placed in a
    // separate RDF:Description element.
    nsCOMPtr<nsISimpleEnumerator> arcs;
    rv = mInner->ArcLabelsOut(aContainer, getter_AddRefs(arcs));
    if (NS_FAILED(rv)) return rv;

    PRBool wroteDescription = PR_FALSE;
    while (! wroteDescription) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsIRDFResource* property;
        rv = arcs->GetNext((nsISupports**) &property);
        if (NS_FAILED(rv)) return rv;

        // If it's a membership property, then output a "LI"
        // tag. Otherwise, output a property.
        if (! IsContainerProperty(property)) {
            rv = SerializeDescription(aStream, aContainer);
            wroteDescription = PR_TRUE;
        }

        NS_RELEASE(property);
        if (NS_FAILED(rv))
            break;
    }

    return NS_OK;
}


nsresult
RDFXMLDataSourceImpl::SerializePrologue(nsIOutputStream* aStream)
{
static const char kXMLVersion[] = "<?xml version=\"1.0\"?>\n";
static const char kOpenRDF[]  = "<RDF:RDF";
static const char kXMLNS[]    = "\n     xmlns";

    rdf_BlockingWrite(aStream, kXMLVersion, sizeof(kXMLVersion) - 1);

    // global name space declarations
    rdf_BlockingWrite(aStream, kOpenRDF, sizeof(kOpenRDF) - 1);
    for (NameSpaceMap* entry = mNameSpaces; entry != nsnull; entry = entry->Next) {
        rdf_BlockingWrite(aStream, kXMLNS, sizeof(kXMLNS) - 1);

        if (entry->Prefix) {
            rdf_BlockingWrite(aStream, ":", 1);
            nsAutoString prefix;
            entry->Prefix->ToString(prefix);
            rdf_BlockingWrite(aStream, prefix);
        }

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
      // can't use |NS_LITERAL_STRING| here until |rdf_BlockingWrite| is fixed to accept readables
    rdf_BlockingWrite(aStream, NS_ConvertASCIItoUCS2("</RDF:RDF>\n"));
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Serialize(nsIOutputStream* aStream)
{
    nsresult rv;
    nsCOMPtr<nsISimpleEnumerator> resources;

    rv = mInner->GetAllResources(getter_AddRefs(resources));
    if (NS_FAILED(rv)) return rv;

    rv = SerializePrologue(aStream);
    if (NS_FAILED(rv))
        return rv;

    while (1) {
        PRBool hasMore;
        rv = resources->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsIRDFResource* resource;
        rv = resources->GetNext((nsISupports**) &resource);
        if (NS_FAILED(rv))
            break;

        if (IsA(mInner, resource, kRDF_Bag) ||
            IsA(mInner, resource, kRDF_Seq) ||
            IsA(mInner, resource, kRDF_Alt)) {
            rv = SerializeContainer(aStream, resource);
        }
        else {
            rv = SerializeDescription(aStream, resource);
        }
        NS_RELEASE(resource);

        if (NS_FAILED(rv))
            break;
    }

    rv = SerializeEpilogue(aStream);
    return rv;
}


PRBool
RDFXMLDataSourceImpl::IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType)
{
    nsresult rv;

    PRBool result;
    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, aType, PR_TRUE, &result);
    if (NS_FAILED(rv)) return PR_FALSE;

    return result;
}


