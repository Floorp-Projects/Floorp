/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsdirectoryviewer__h____
#define nsdirectoryviewer__h____

#include "nsNetUtil.h"
#include "nsIStreamListener.h"
#include "nsIContentViewer.h"
#include "nsIHTTPIndex.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFLiteral.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsITimer.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"
#include "nsIDirIndexListener.h"
#include "nsIFTPChannel.h"
#include "nsCycleCollectionParticipant.h"

class nsDirectoryViewerFactory : public nsIDocumentLoaderFactory
{
public:
    nsDirectoryViewerFactory();

    // nsISupports interface
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOCUMENTLOADERFACTORY

protected:
    virtual ~nsDirectoryViewerFactory();
};

class nsHTTPIndex : public nsIHTTPIndex,
                    public nsIRDFDataSource,
                    public nsIStreamListener,
                    public nsIDirIndexListener,
                    public nsIInterfaceRequestor,
                    public nsIFTPEventSink
{
private:

    // note: these are NOT statics due to the native of nsHTTPIndex
    // where it may or may not be treated as a singleton

    nsCOMPtr<nsIRDFResource>     kNC_Child;
    nsCOMPtr<nsIRDFResource>     kNC_Comment;
    nsCOMPtr<nsIRDFResource>     kNC_Loading;
    nsCOMPtr<nsIRDFResource>     kNC_URL;
    nsCOMPtr<nsIRDFResource>     kNC_Description;
    nsCOMPtr<nsIRDFResource>     kNC_ContentLength;
    nsCOMPtr<nsIRDFResource>     kNC_LastModified;
    nsCOMPtr<nsIRDFResource>     kNC_ContentType;
    nsCOMPtr<nsIRDFResource>     kNC_FileType;
    nsCOMPtr<nsIRDFResource>     kNC_IsContainer;
    nsCOMPtr<nsIRDFLiteral>      kTrueLiteral;
    nsCOMPtr<nsIRDFLiteral>      kFalseLiteral;

    nsCOMPtr<nsIRDFService>      mDirRDF;

protected:
    // We grab a reference to the content viewer container (which
    // indirectly owns us) so that we can insert ourselves as a global
    // in the script context _after_ the XUL doc has been embedded into
    // content viewer. We'll know that this has happened once we receive
    // an OnStartRequest() notification

    nsCOMPtr<nsIRDFDataSource>   mInner;
    nsCOMPtr<nsISupportsArray>   mConnectionList;
    nsCOMPtr<nsISupportsArray>   mNodeList;
    nsCOMPtr<nsITimer>           mTimer;
    nsCOMPtr<nsIDirIndexParser>  mParser;
    nsCString mBaseURL;
    nsCString                    mEncoding;
    bool                         mBindToGlobalObject;
    nsIInterfaceRequestor*       mRequestor; // WEAK
    nsCOMPtr<nsIRDFResource>     mDirectory;

    nsHTTPIndex(nsIInterfaceRequestor* aRequestor);
    nsresult CommonInit(void);
    nsresult Init(nsIURI* aBaseURL);
    void        GetDestination(nsIRDFResource* r, nsXPIDLCString& dest);
    bool        isWellknownContainerURI(nsIRDFResource *r);
    nsresult    AddElement(nsIRDFResource *parent, nsIRDFResource *prop,
                           nsIRDFNode *child);

    static void FireTimer(nsITimer* aTimer, void* aClosure);

    virtual ~nsHTTPIndex();

public:
    nsHTTPIndex();
    nsresult Init(void);

    static nsresult Create(nsIURI* aBaseURI, nsIInterfaceRequestor* aContainer,
                           nsIHTTPIndex** aResult);

    // nsIHTTPIndex interface
    NS_DECL_NSIHTTPINDEX

    // NSIRDFDataSource interface
    NS_DECL_NSIRDFDATASOURCE

    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    
    NS_DECL_NSIDIRINDEXLISTENER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIFTPEVENTSINK

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsHTTPIndex, nsIHTTPIndex)
};

// {82776710-5690-11d3-BE36-00104BDE6048}
#define NS_DIRECTORYVIEWERFACTORY_CID \
{ 0x82776710, 0x5690, 0x11d3, { 0xbe, 0x36, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

#endif // nsdirectoryviewer__h____
