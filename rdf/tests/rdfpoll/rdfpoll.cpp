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

  A simple test program that reads in RDF/XML into an in-memory data
  source, then periodically updates it. It prints the messages
  indicating assert and unassert activity to the console.

  The program takes two parameters: the URL from which to read, and
  the poll-interval at which to re-load the .

 */

#include <stdlib.h>
#include "nsCOMPtr.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#ifndef NECKO
#include "nsINetService.h"
NS_DEFINE_IID(kIOutputStreamIID,       NS_IOUTPUTSTREAM_IID);
#else
#include "nsIIOService.h"
#endif // NECKO
#include "nsIGenericFactory.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsDOMCID.h"    // for NS_SCRIPT_NAMESET_REGISTRY_CID
#include "nsLayoutCID.h" // for NS_NAMESPACEMANAGER_CID
#include "nsRDFCID.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "prthread.h"
#include "plevent.h"
#include "plstr.h"
#include "nsParserCIID.h"

#if defined(XP_PC)
#define DOM_DLL    "jsdom.dll"
#define LAYOUT_DLL "raptorhtml.dll"
#define NETLIB_DLL "netlib.dll"
#define PARSER_DLL "raptorhtmlpars.dll"
#define RDF_DLL    "rdf.dll"
#define XPCOM_DLL  "xpcom32.dll"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#define DOM_DLL    "libjsdom"MOZ_DLL_SUFFIX
#define LAYOUT_DLL "libraptorhtml"MOZ_DLL_SUFFIX
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define PARSER_DLL "libraptorhtmlpars"MOZ_DLL_SUFFIX
#define RDF_DLL    "librdf"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#elif defined(XP_MAC)
#define DOM_DLL    "DOM_DLL"
#define LAYOUT_DLL "LAYOUT_DLL"
#define NETLIB_DLL "NETLIB_DLL"
#define PARSER_DLL "PARSER_DLL"
#define RDF_DLL    "RDF_DLL"
#define XPCOM_DLL  "XPCOM_DLL"
#endif


////////////////////////////////////////////////////////////////////////
// CIDs

// rdf
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,  NS_RDFXMLDATASOURCE_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);

////////////////////////////////////////////////////////////////////////
// IIDs

NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);

#include "nsIAllocator.h" // for the CID

static nsresult
SetupRegistry(void)
{
    // netlib
#ifndef NECKO
    static NS_DEFINE_CID(kNetServiceCID,            NS_NETSERVICE_CID);
    nsComponentManager::RegisterComponent(kNetServiceCID,            NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
#else
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    nsComponentManager::RegisterComponent(kIOServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);
#endif // NECKO

    // parser
    static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID);
    static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

    nsComponentManager::RegisterComponent(kParserCID,                NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kWellFormedDTDCID,         NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);

    // layout
    static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);

    nsComponentManager::RegisterComponent(kNameSpaceManagerCID,      NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);

    // xpcom
    static NS_DEFINE_CID(kAllocatorCID, NS_ALLOCATOR_CID);
    static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
    nsComponentManager::RegisterComponent(kEventQueueServiceCID,     NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kEventQueueCID,            NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kGenericFactoryCID,        NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kAllocatorCID,             NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

class Observer : public nsIRDFObserver
{
public:
    Observer();
    virtual ~Observer() {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget);

    NS_IMETHOD OnUnassert(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget);

    NS_IMETHOD OnChange(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aOldTarget,
                        nsIRDFNode* aNewTarget);

    NS_IMETHOD OnMove(nsIRDFResource* aOldSource,
                      nsIRDFResource* aNewSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget);
};

Observer::Observer()
{
    NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(Observer, nsIRDFObserver::GetIID());

static nsresult
rdf_WriteOp(const char* aOp,
            nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget)
{
    nsresult rv;

    nsXPIDLCString source;
    rv = aSource->GetValue(getter_Copies(source));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString property;
    rv = aProperty->GetValue(getter_Copies(property));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    printf("%.8s [%s]\n", aOp, (const char*) source);
    printf("       --[%s]--\n", (const char*) property);

    if ((resource = do_QueryInterface(aTarget)) != nsnull) {
        nsXPIDLCString target;
        rv = resource->GetValue(getter_Copies(target));
        if (NS_FAILED(rv)) return rv;

        printf("       ->[%s]\n", (const char*) target);
    }
    else if ((literal = do_QueryInterface(aTarget)) != nsnull) {
        nsXPIDLString target;
        rv = literal->GetValue(getter_Copies(target));
        if (NS_FAILED(rv)) return rv;

        char* p = nsAutoString(target).ToNewCString();
        if (! p)
            return NS_ERROR_OUT_OF_MEMORY;
        
        printf("       ->\"%s\"\n", p);

        nsCRT::free(p);
    }

    printf("\n");
    return NS_OK;
}

NS_IMETHODIMP
Observer::OnAssert(nsIRDFResource* aSource,
                   nsIRDFResource* aProperty,
                   nsIRDFNode* aTarget)
{
    return rdf_WriteOp("assert", aSource, aProperty, aTarget);
}


NS_IMETHODIMP
Observer::OnUnassert(nsIRDFResource* aSource,
                     nsIRDFResource* aProperty,
                     nsIRDFNode* aTarget)
{
    return rdf_WriteOp("unassert", aSource, aProperty, aTarget);
}


NS_IMETHODIMP
Observer::OnChange(nsIRDFResource* aSource,
                   nsIRDFResource* aProperty,
                   nsIRDFNode* aOldTarget,
                   nsIRDFNode* aNewTarget)
{
    nsresult rv;
    rv = rdf_WriteOp("chg-from", aSource, aProperty, aOldTarget);
    if (NS_FAILED(rv)) return rv;

    rv = rdf_WriteOp("chg-to", aSource, aProperty, aNewTarget);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
Observer::OnMove(nsIRDFResource* aOldSource,
                 nsIRDFResource* aNewSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget)
{
    nsresult rv;
    rv = rdf_WriteOp("mv-from", aOldSource, aProperty, aTarget);
    if (NS_FAILED(rv)) return rv;

    rv = rdf_WriteOp("mv-to", aNewSource, aProperty, aTarget);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

int
main(int argc, char** argv)
{
    nsresult rv;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <url> [<poll-interval>]\n", argv[0]);
        return 1;
    }

    SetupRegistry();

    // Get netlib off the floor...
    NS_WITH_SERVICE(nsIEventQueueService, theEventQueueService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = theEventQueueService->CreateThreadEventQueue();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create thread event queue");
    if (NS_FAILED(rv)) return rv;

    // Create a stream data source and initialize it on argv[1], which
    // is hopefully a "file:" URL. (Actually, we can do _any_ kind of
    // URL, but only a "file:" URL will be written back to disk.)
    nsCOMPtr<nsIRDFDataSource> ds;
    rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                            nsnull,
                                            nsIRDFDataSource::GetIID(),
                                            getter_AddRefs(ds));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create RDF/XML data source");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFRemoteDataSource> remote
        = do_QueryInterface(ds);

    if (! remote)
        return NS_ERROR_UNEXPECTED;

    rv = remote->Init(argv[1]);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to initialize data source");
    if (NS_FAILED(rv)) return rv;

    // The do_QI() on the pointer is a hack to make sure that the new
    // object gets AddRef()-ed.
    nsCOMPtr<nsIRDFObserver> observer = do_QueryInterface(new Observer);
    if (! observer)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = ds->AddObserver(observer);
    if (NS_FAILED(rv)) return rv;

    while (1) {
        // Okay, this should load the XML file...
        rv = remote->Refresh(PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to open datasource");
        if (NS_FAILED(rv)) return rv;

        if (argc <= 2)
            break;

        PRInt32 pollinterval = atol(argv[2]);
        if (! pollinterval)
            break;

        PR_Sleep(PR_SecondsToInterval(pollinterval));
    }

    return NS_OK;
}
