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

  A simple test program that reads in RDF/XML into an in-memory data
  source, then periodically updates it. It prints the messages
  indicating assert and unassert activity to the console.

  The program takes two parameters: the URL from which to read, and
  the poll-interval at which to re-load the .

 */

#include <stdlib.h>
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "prthread.h"
#include "plevent.h"
#include "plstr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNetCID.h"
#include "nsCRT.h"

////////////////////////////////////////////////////////////////////////
// CIDs

// rdf
static NS_DEFINE_CID(kRDFXMLDataSourceCID,  NS_RDFXMLDATASOURCE_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

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

    nsXPIDLCString source;
    rv = aSource->GetValue(getter_Copies(source));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString property;
    rv = aProperty->GetValue(getter_Copies(property));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;
    nsCOMPtr<nsIRDFDate> date;
    nsCOMPtr<nsIRDFInt> number;

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

        char* p = ToNewCString(target);
        if (! p)
            return NS_ERROR_OUT_OF_MEMORY;
        
        printf("       ->\"%s\"\n", p);

        nsCRT::free(p);
    }
    else if ((date = do_QueryInterface(aTarget)) != nsnull) {
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
    else if ((number = do_QueryInterface(aTarget)) != nsnull) {
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

    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "NS_InitXPCOM2 failed\n");
        return 1;
    }

    // Get netlib off the floor...
    nsCOMPtr<nsIEventQueueService> theEventQueueService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = theEventQueueService->CreateThreadEventQueue();
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create thread event queue");
    if (NS_FAILED(rv)) return rv;

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
