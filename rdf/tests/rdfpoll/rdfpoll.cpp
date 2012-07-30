/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A simple test program that reads in RDF/XML into an in-memory data
  source, then periodically updates it. It prints the messages
  indicating assert and unassert activity to the console.

  The program takes two parameters: the URL from which to read, and
  the poll-interval at which to re-load the .

 */

#include <stdlib.h>
#include <stdio.h>
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "prthread.h"
#include "plstr.h"
#include "nsEmbedString.h"
#include "nsNetCID.h"

////////////////////////////////////////////////////////////////////////
// CIDs

// rdf
static NS_DEFINE_CID(kRDFXMLDataSourceCID,  NS_RDFXMLDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// IIDs


#include "nsIMemory.h" // for the CID

////////////////////////////////////////////////////////////////////////

class Observer : public nsIRDFObserver
{
public:
    Observer();
    virtual ~Observer() {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFObserver interface
    NS_DECL_NSIRDFOBSERVER
};

Observer::Observer()
{
}

NS_IMPL_ISUPPORTS1(Observer, nsIRDFObserver)

static nsresult
rdf_WriteOp(const char* aOp,
            nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget)
{
    nsresult rv;

    nsCString source;
    rv = aSource->GetValue(getter_Copies(source));
    if (NS_FAILED(rv)) return rv;

    nsCString property;
    rv = aProperty->GetValue(getter_Copies(property));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;
    nsCOMPtr<nsIRDFDate> date;
    nsCOMPtr<nsIRDFInt> number;

    printf("%.8s [%s]\n", aOp, source.get());
    printf("       --[%s]--\n", property.get());

    if ((resource = do_QueryInterface(aTarget)) != nullptr) {
        nsCString target;
        rv = resource->GetValue(getter_Copies(target));
        if (NS_FAILED(rv)) return rv;

        printf("       ->[%s]\n", target.get());
    }
    else if ((literal = do_QueryInterface(aTarget)) != nullptr) {
        nsString target;
        rv = literal->GetValue(getter_Copies(target));
        if (NS_FAILED(rv)) return rv;

        printf("       ->\"%s\"\n", NS_ConvertUTF16toUTF8(target).get());
    }
    else if ((date = do_QueryInterface(aTarget)) != nullptr) {
        PRTime value;
        date->GetValue(&value);

        PRExplodedTime t;
        PR_ExplodeTime(value, PR_GMTParameters, &t);

        printf("       -> %02d/%02d/%04d %02d:%02d:%02d +%06d\n",
               t.tm_month + 1,
               t.tm_mday,
               t.tm_year,
               t.tm_hour,
               t.tm_min,
               t.tm_sec,
               t.tm_usec);
    }
    else if ((number = do_QueryInterface(aTarget)) != nullptr) {
        PRInt32 value;
        number->GetValue(&value);

        printf("       -> %d\n", value);
    }
    else {
        printf("       -> (unknown node type)\n");
    }

    printf("\n");
    return NS_OK;
}

NS_IMETHODIMP
Observer::OnAssert(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aSource,
                   nsIRDFResource* aProperty,
                   nsIRDFNode* aTarget)
{
    return rdf_WriteOp("assert", aSource, aProperty, aTarget);
}


NS_IMETHODIMP
Observer::OnUnassert(nsIRDFDataSource* aDataSource,
                     nsIRDFResource* aSource,
                     nsIRDFResource* aProperty,
                     nsIRDFNode* aTarget)
{
    return rdf_WriteOp("unassert", aSource, aProperty, aTarget);
}


NS_IMETHODIMP
Observer::OnChange(nsIRDFDataSource* aDataSource,
                   nsIRDFResource* aSource,
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
Observer::OnMove(nsIRDFDataSource* aDataSource,
                 nsIRDFResource* aOldSource,
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

NS_IMETHODIMP
Observer::OnBeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
    return NS_OK;
}

NS_IMETHODIMP
Observer::OnEndUpdateBatch(nsIRDFDataSource* aDataSource)
{
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

    rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "NS_InitXPCOM2 failed\n");
        return 1;
    }

    // Create a stream data source and initialize it on argv[1], which
    // is hopefully a "file:" URL. (Actually, we can do _any_ kind of
    // URL, but only a "file:" URL will be written back to disk.)
    nsCOMPtr<nsIRDFDataSource> ds = do_CreateInstance(kRDFXMLDataSourceCID, &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("unable to create RDF/XML data source");
        return rv;
    }

    nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(ds);
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
        rv = remote->Refresh(true);
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
