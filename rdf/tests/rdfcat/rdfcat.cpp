/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A simple test program that reads in RDF/XML into an in-memory data
  source, then uses the RDF/XML serialization API to write an
  equivalent (but not identical) RDF/XML file back to stdout.

  The program takes a single parameter: the URL from which to read.

 */

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIEventQueueService.h"
#include "nsIGenericFactory.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "plevent.h"
#include "plstr.h"
#include "prio.h"
#include "prthread.h"


////////////////////////////////////////////////////////////////////////
// CIDs

// rdf
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,  NS_RDFXMLDATASOURCE_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);

////////////////////////////////////////////////////////////////////////
// To get the registry initialized!

#include "../../../webshell/tests/viewer/nsSetupRegistry.cpp"

////////////////////////////////////////////////////////////////////////

class ConsoleOutputStreamImpl : public nsIOutputStream
{
public:
    ConsoleOutputStreamImpl(void) { NS_INIT_REFCNT(); }
    virtual ~ConsoleOutputStreamImpl(void) {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIBaseStream interface
    NS_IMETHOD Close(void) {
        return NS_OK;
    }

    // nsIOutputStream interface
    NS_IMETHOD Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount) {
        PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf, aCount);
        *aWriteCount = aCount;
        return NS_OK;
    }

    NS_IMETHOD
    WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteFrom");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    GetNonBlocking(PRBool *aNonBlocking) {
        NS_NOTREACHED("GetNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    SetNonBlocking(PRBool aNonBlocking) {
        NS_NOTREACHED("SetNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    GetObserver(nsIOutputStreamObserver * *aObserver) {
        NS_NOTREACHED("GetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    SetObserver(nsIOutputStreamObserver * aObserver) {
        NS_NOTREACHED("SetObserver");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush(void) {
        PR_Sync(PR_GetSpecialFD(PR_StandardOutput));
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS1(ConsoleOutputStreamImpl, nsIOutputStream)


////////////////////////////////////////////////////////////////////////

int
main(int argc, char** argv)
{
    nsresult rv;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <url> [<poll-interval>]\n", argv[0]);
        return 1;
    }

    NS_SetupRegistry();

    // Get netlib off the floor...
    nsCOMPtr<nsIEventQueueService> theEventQueueService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = theEventQueueService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) {
        fprintf(stderr, "unable to create thread event queue\n");
        return rv;
    }

    // Create a stream data source and initialize it on argv[1], which
    // is hopefully a "file:" URL.
    nsCOMPtr<nsIRDFDataSource> ds;
    rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFDataSource),
                                            getter_AddRefs(ds));

    if (NS_FAILED(rv)) {
        fprintf(stderr, "unable to create RDF/XML data source\n");
        return rv;
    }

    nsCOMPtr<nsIRDFRemoteDataSource> remote
        = do_QueryInterface(ds);

    if (! remote)
        return NS_ERROR_UNEXPECTED;

    rv = remote->Init(argv[1]);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "unable to initialize data source\n");
        return rv;
    }

    // Okay, this should load the XML file...
    rv = remote->Refresh(PR_TRUE);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "unable to open file\n");
        return rv;
    }

    // And this should write it back out. The do_QI() on the pointer
    // is a hack to make sure that the new object gets AddRef()-ed.
    nsCOMPtr<nsIOutputStream> out =
        do_QueryInterface(new ConsoleOutputStreamImpl);

    if (! out) {
        fprintf(stderr, "unable to create console output stream\n");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIRDFXMLSource> source = do_QueryInterface(ds);
    if (! source)
        return NS_ERROR_UNEXPECTED;

    rv = source->Serialize(out);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "error serializing\n");
        return rv;
    }

    return NS_OK;
}
