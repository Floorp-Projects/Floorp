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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


  Logging
  -------

  To turn on logging for this module, set

    NSPR_LOG_MODULES=nsRDFXMLDataSource:5

 */

#include "nsIFileStreams.h"
#include "nsIOutputStream.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIDTD.h"
#include "nsIRDFPurgeableDataSource.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLParser.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIRDFXMLSink.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsRDFCID.h"
#include "nsRDFBaseDataSources.h"
#include "nsCOMArray.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "prio.h"
#include "prthread.h"
#include "rdf.h"
#include "rdfutil.h"
#include "prlog.h"
#include "nsNameSpaceMap.h"
#include "nsCRT.h"

//----------------------------------------------------------------------

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);

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
        PRUint32 readCount = PR_MIN(aCount, (mSize-mIndex));
        
        memcpy(aBuf, mBuffer+mIndex, readCount);
        mIndex += readCount;
        
        *aReadCount = readCount;
        
        return NS_OK;
    }

    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
        PRUint32 readCount = PR_MIN(count, (mSize-mIndex));

        *_retval = 0;
        nsresult rv = writer (this, closure, mBuffer+mIndex, mIndex, readCount, _retval);
        if (NS_SUCCEEDED(rv))
            mIndex += *_retval;

        // do not propogate errors returned from writer!
        return NS_OK;
    }

    NS_IMETHOD IsNonBlocking(PRBool *aNonBlocking) {
        *aNonBlocking = PR_TRUE;
        return NS_OK;
    }

    // Implementation
    void SetBuffer(const char* aBuffer, PRUint32 aSize) {
        mBuffer = aBuffer;
        mSize = aSize;
        mIndex = 0;
    }
};

NS_IMPL_ISUPPORTS1(ProxyStream, nsIInputStream)

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
    nsCOMArray<nsIRDFXMLSinkObserver> mObservers;
    nsCOMPtr<nsIURI>    mURL;
    nsCOMPtr<nsIStreamListener> mListener;
    nsNameSpaceMap      mNameSpaces;

    // pseudo-constants
    static PRInt32 gRefCnt;
    static nsIRDFService* gRDFService;

    nsresult Init();
    RDFXMLDataSourceImpl(void);
    virtual ~RDFXMLDataSourceImpl(void);
    nsresult rdfXMLFlush(nsIURI *aURI);

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

    NS_IMETHOD BeginUpdateBatch() {
        return mInner->BeginUpdateBatch();
    }

    NS_IMETHOD EndUpdateBatch() {
        return mInner->EndUpdateBatch();
    }

    // nsIRDFRemoteDataSource interface
    NS_DECL_NSIRDFREMOTEDATASOURCE

    // nsIRDFXMLSink interface
    NS_DECL_NSIRDFXMLSINK

    // nsIRDFXMLSource interface
    NS_DECL_NSIRDFXMLSOURCE

    // nsIRequestObserver
    NS_DECL_NSIREQUESTOBSERVER

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

protected:
    nsresult
    BlockingParse(nsIURI* aURL, nsIStreamListener* aConsumer);
};

PRInt32         RDFXMLDataSourceImpl::gRefCnt = 0;
nsIRDFService*  RDFXMLDataSourceImpl::gRDFService;

static const char kFileURIPrefix[] = "file:";
static const char kResourceURIPrefix[] = "resource:";


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
      mLoadState(eLoadState_Unloaded)
{
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

    if (gRefCnt++ == 0) {
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          NS_REINTERPRET_CAST(nsISupports**, &gRDFService));

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
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
    mObservers.Clear();

    NS_RELEASE(mInner);

    if (--gRefCnt == 0) {
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
                   nsIRequestObserver,
                   nsIStreamListener)


nsresult
RDFXMLDataSourceImpl::BlockingParse(nsIURI* aURL, nsIStreamListener* aConsumer)
{
    nsresult rv;

    // XXX I really hate the way that we're spoon-feeding this stuff
    // to the parser: it seems like this is something that netlib
    // should be able to do by itself.
    
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIRequest> request;

    // Null LoadGroup ?
    rv = NS_NewChannel(getter_AddRefs(channel), aURL, nsnull);
    if (NS_FAILED(rv)) return rv;
    nsIInputStream* in;
    PRUint32 sourceOffset = 0;
    rv = channel->Open(&in);

    // Report success if the file doesn't exist, but propagate other errors.
    if (rv == NS_ERROR_FILE_NOT_FOUND) return NS_OK;
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(in != nsnull, "no input stream");
    if (! in) return NS_ERROR_FAILURE;

    rv = NS_ERROR_OUT_OF_MEMORY;
    ProxyStream* proxy = new ProxyStream();
    if (! proxy)
        goto done;

    // Notify load observers
    PRInt32 i;
    for (i = mObservers.Count() - 1; i >= 0; --i) {
        // Make sure to hold a strong reference to the observer so
        // that it doesn't go away in this call if it removes itself
        // as an observer
        nsCOMPtr<nsIRDFXMLSinkObserver> obs = mObservers[i];

        if (obs) {
            obs->OnBeginLoad(this);
        }
    }

    request = do_QueryInterface(channel);

    aConsumer->OnStartRequest(request, nsnull);
    while (PR_TRUE) {
        char buf[4096];
        PRUint32 readCount;

        if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
            break; // error

        if (readCount == 0)
            break; // eof

        proxy->SetBuffer(buf, readCount);

        rv = aConsumer->OnDataAvailable(request, nsnull, proxy, sourceOffset, readCount);
        sourceOffset += readCount;
        if (NS_FAILED(rv))
            break;
    }

    aConsumer->OnStopRequest(channel, nsnull, rv);

    // Notify load observers
    for (i = mObservers.Count() - 1; i >= 0; --i) {
        // Make sure to hold a strong reference to the observer so
        // that it doesn't go away in this call if it removes itself
        // as an observer
        nsCOMPtr<nsIRDFXMLSinkObserver> obs = mObservers[i];

        if (obs) {
            if (NS_FAILED(rv))
                obs->OnError(this, rv, nsnull);

            obs->OnEndLoad(this);
        }
    }

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
    NS_PRECONDITION(mInner != nsnull, "not initialized");
    if (! mInner)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    rv = NS_NewURI(getter_AddRefs(mURL), nsDependentCString(uri));
    if (NS_FAILED(rv)) return rv;

    // XXX this is a hack: any "file:" URI is considered writable. All
    // others are considered read-only.
    if ((PL_strncmp(uri, kFileURIPrefix, sizeof(kFileURIPrefix) - 1) != 0) &&
        (PL_strncmp(uri, kResourceURIPrefix, sizeof(kResourceURIPrefix) - 1) != 0)) {
        mIsWritable = PR_FALSE;
    }

    rv = gRDFService->RegisterDataSource(this, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::GetURI(char* *aURI)
{
    *aURI = nsnull;
    if (!mURL) {
        return NS_OK;
    }
    
    nsCAutoString spec;
    mURL->GetSpec(spec);
    *aURI = ToNewCString(spec);
    if (!*aURI) {
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


nsresult
RDFXMLDataSourceImpl::rdfXMLFlush(nsIURI *aURI)
{

    nsresult rv;

    {
        // Quick and dirty check to see if we're in XPCOM shutdown. If
        // we are, we're screwed: it's too late to serialize because
        // many of the services that we'll need to acquire to properly
        // write the file will be unaquirable.
        nsCOMPtr<nsIRDFService> dummy = do_GetService(kRDFServiceCID, &rv);
        if (NS_FAILED(rv)) {
            NS_WARNING("unable to Flush() diry datasource during XPCOM shutdown");
            return rv;
        }
    }

    // Is it a file? If so, we can write to it. Some day, it'd be nice
    // if we didn't care what kind of stream this was...
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI);
    
    if (fileURL) {
        nsCOMPtr<nsIFile> file;
        fileURL->GetFile(getter_AddRefs(file));
        if (file) {
            // if file doesn't exist, create it
            (void)file->Create(nsIFile::NORMAL_FILE_TYPE, 0666);

            nsCOMPtr<nsIOutputStream> out;
            rv = NS_NewLocalFileOutputStream(getter_AddRefs(out), file);
            nsCOMPtr<nsIOutputStream> bufferedOut;
            if (out)
                NS_NewBufferedOutputStream(getter_AddRefs(bufferedOut), out, 4096);
            if (bufferedOut) {
                rv = Serialize(bufferedOut);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::FlushTo(const char *aURI)
{
    NS_PRECONDITION(aURI != nsnull, "not initialized");
    if (!aURI)
        return NS_ERROR_NULL_POINTER;

    // XXX this is a hack: any "file:" URI is considered writable. All
    // others are considered read-only.
    if ((PL_strncmp(aURI, kFileURIPrefix, sizeof(kFileURIPrefix) - 1) != 0) &&
        (PL_strncmp(aURI, kResourceURIPrefix, sizeof(kResourceURIPrefix) - 1) != 0))
    {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsCOMPtr<nsIURI>  url;
    nsresult rv = NS_NewURI(getter_AddRefs(url), aURI);
    if (NS_FAILED(rv))
      return rv;
    rv = rdfXMLFlush(url);
    return rv;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::Flush(void)
{
    if (!mIsWritable || !mIsDirty)
        return NS_OK;

    // while it is not fatal if mURL is not set,
    // indicate failure since we can't flush back to an unknown origin
    if (! mURL)
        return NS_ERROR_NOT_INITIALIZED;

#ifdef PR_LOGGING
    nsCAutoString spec;
    mURL->GetSpec(spec);
    PR_LOG(gLog, PR_LOG_NOTICE,
           ("rdfxml[%p] flush(%s)", this, spec.get()));
#endif

    nsresult rv;
    if (NS_SUCCEEDED(rv = rdfXMLFlush(mURL)))
    {
      mIsDirty = PR_FALSE;
    }
    return rv;
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

#include "nsITimelineService.h"

NS_IMETHODIMP
RDFXMLDataSourceImpl::Refresh(PRBool aBlocking)
{
#ifdef PR_LOGGING
    nsCAutoString spec;
    if (mURL) {
        mURL->GetSpec(spec);
    }
    PR_LOG(gLog, PR_LOG_NOTICE,
           ("rdfxml[%p] refresh(%s) %sblocking", this, spec.get(), (aBlocking ? "" : "non")));
#endif
    
    // If an asynchronous load is already pending, then just let it do
    // the honors.
    if (IsLoading()) {
        PR_LOG(gLog, PR_LOG_NOTICE,
               ("rdfxml[%p] refresh(%s) a load was pending", this, spec.get()));

        if (aBlocking) {
            NS_WARNING("blocking load requested when async load pending");
            return NS_ERROR_FAILURE;
        }
        else {
            return NS_OK;
        }
    }

    if (! mURL)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIRDFXMLParser> parser = do_CreateInstance("@mozilla.org/rdf/xml-parser;1");
    if (! parser)
        return NS_ERROR_FAILURE;

    nsresult rv = parser->ParseAsync(this, mURL, getter_AddRefs(mListener));
    if (NS_FAILED(rv)) return rv;

    if (aBlocking) {
        NS_TIMELINE_START_TIMER("rdf blocking parse");
        rv = BlockingParse(mURL, this);
        NS_TIMELINE_STOP_TIMER("rdf blocking parse");
        NS_TIMELINE_MARK_TIMER("rdf blocking parse");

        mListener = nsnull; // release the parser

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
#ifdef PR_LOGGING
    nsCAutoString spec;
    if (mURL) {
        mURL->GetSpec(spec);
    }
    PR_LOG(gLog, PR_LOG_NOTICE,
           ("rdfxml[%p] begin-load(%s)", this, spec.get()));
#endif
    
    mLoadState = eLoadState_Loading;
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        // Make sure to hold a strong reference to the observer so
        // that it doesn't go away in this call if it removes itself
        // as an observer
        nsCOMPtr<nsIRDFXMLSinkObserver> obs = mObservers[i];

        if (obs) {
            obs->OnBeginLoad(this);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Interrupt(void)
{
#ifdef PR_LOGGING
    nsCAutoString spec;
    if (mURL) {
        mURL->GetSpec(spec);
    }
    PR_LOG(gLog, PR_LOG_NOTICE,
           ("rdfxml[%p] interrupt(%s)", this, spec.get()));
#endif
    
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        // Make sure to hold a strong reference to the observer so
        // that it doesn't go away in this call if it removes itself
        // as an observer
        nsCOMPtr<nsIRDFXMLSinkObserver> obs = mObservers[i];

        if (obs) {
            obs->OnInterrupt(this);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::Resume(void)
{
#ifdef PR_LOGGING
    nsCAutoString spec;
    if (mURL) {
        mURL->GetSpec(spec);
    }
    PR_LOG(gLog, PR_LOG_NOTICE,
           ("rdfxml[%p] resume(%s)", this, spec.get()));
#endif
    
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        // Make sure to hold a strong reference to the observer so
        // that it doesn't go away in this call if it removes itself
        // as an observer
        nsCOMPtr<nsIRDFXMLSinkObserver> obs = mObservers[i];

        if (obs) {
            obs->OnResume(this);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::EndLoad(void)
{
#ifdef PR_LOGGING
    nsCAutoString spec;
    if (mURL) {
        mURL->GetSpec(spec);
    }
    PR_LOG(gLog, PR_LOG_NOTICE,
           ("rdfxml[%p] end-load(%s)", this, spec.get()));
#endif
    
    mLoadState = eLoadState_Loaded;

    // Clear out any unmarked assertions from the datasource.
    nsCOMPtr<nsIRDFPurgeableDataSource> gcable = do_QueryInterface(mInner);
    if (gcable) {
        gcable->Sweep();
    }

    // Notify load observers
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        // Make sure to hold a strong reference to the observer so
        // that it doesn't go away in this call if it removes itself
        // as an observer
        nsIRDFXMLSinkObserver* obs = mObservers[i];

        if (obs) {
            obs->OnEndLoad(this);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::AddNameSpace(nsIAtom* aPrefix, const nsString& aURI)
{
    mNameSpaces.Put(aURI, aPrefix);
    return NS_OK;
}


NS_IMETHODIMP
RDFXMLDataSourceImpl::AddXMLSinkObserver(nsIRDFXMLSinkObserver* aObserver)
{
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    mObservers.AppendObject(aObserver);
    return NS_OK;
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::RemoveXMLSinkObserver(nsIRDFXMLSinkObserver* aObserver)
{
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    mObservers.RemoveObject(aObserver);

    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIRequestObserver
//

NS_IMETHODIMP
RDFXMLDataSourceImpl::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    return mListener->OnStartRequest(request, ctxt);
}

NS_IMETHODIMP
RDFXMLDataSourceImpl::OnStopRequest(nsIRequest *request,
                                    nsISupports *ctxt,
                                    nsresult status)
{
    if (NS_FAILED(status)) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            // Make sure to hold a strong reference to the observer so
            // that it doesn't go away in this call if it removes
            // itself as an observer
            nsCOMPtr<nsIRDFXMLSinkObserver> obs = mObservers[i];

            if (obs) {
                obs->OnError(this, status, nsnull);
            }
        }
    }

    nsresult rv;
    rv = mListener->OnStopRequest(request, ctxt, status);

    mListener = nsnull; // release the parser

    return rv;
}

//----------------------------------------------------------------------
//
// nsIStreamListener
//

NS_IMETHODIMP
RDFXMLDataSourceImpl::OnDataAvailable(nsIRequest *request,
                                      nsISupports *ctxt,
                                      nsIInputStream *inStr,
                                      PRUint32 sourceOffset,
                                      PRUint32 count)
{
    return mListener->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
}

//----------------------------------------------------------------------
//
// nsIRDFXMLSource
//

NS_IMETHODIMP
RDFXMLDataSourceImpl::Serialize(nsIOutputStream* aStream)
{
    nsresult rv;
    nsCOMPtr<nsIRDFXMLSerializer> serializer
        = do_CreateInstance("@mozilla.org/rdf/xml-serializer;1", &rv);

    if (! serializer)
        return rv;

    rv = serializer->Init(this);
    if (NS_FAILED(rv)) return rv;

    // Add any namespace information that we picked up when reading
    // the RDF/XML
    nsNameSpaceMap::const_iterator last = mNameSpaces.last();
    for (nsNameSpaceMap::const_iterator iter = mNameSpaces.first(); iter != last; ++iter)
        serializer->AddNameSpace(iter->mPrefix, iter->mURI);

    // Serialize!
    nsCOMPtr<nsIRDFXMLSource> source = do_QueryInterface(serializer);
    if (! source)
        return NS_ERROR_FAILURE;

    return source->Serialize(aStream);
}
