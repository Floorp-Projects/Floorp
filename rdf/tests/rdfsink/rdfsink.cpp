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

#include "nsXPCOM.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsIOutputStream.h"
#include "nsIPostToServer.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "plevent.h"
#include "plstr.h"

////////////////////////////////////////////////////////////////////////
// CIDs

// rdf
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,  NS_RDFXMLDATASOURCE_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////
// IIDs

NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
NS_DEFINE_IID(kIRDFXMLDataSourceIID,   NS_IRDFXMLDATASOURCE_IID);
NS_DEFINE_IID(kIRDFXMLSourceIID,       NS_IRDFXMLSOURCE_IID);

////////////////////////////////////////////////////////////////////////

class ConsoleOutputStreamImpl : public nsIOutputStream
{
public:
    ConsoleOutputStreamImpl(void) {}
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

    NS_IMETHOD Flush() {
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
        fprintf(stderr, "usage: %s [url]\n", argv[0]);
        return 1;
    }

    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "NS_InitXPCOM2 failed\n");
        return 1;
    }

    nsIEventQueueService* theEventQueueService = nsnull;
    nsIEventQueue* mainQueue      = nsnull;
    nsIRDFService* theRDFService = nsnull;
    nsIRDFXMLDataSource* ds      = nsnull;
    nsIOutputStream* out         = nsnull;
    nsIRDFXMLSource* source      = nsnull;

    // Get netlib off the floor...
    if (NS_FAILED(rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                                    kIEventQueueServiceIID,
                                                    (nsISupports**) &theEventQueueService))) {
        NS_ERROR("unable to get event queue service");
        goto done;
    }

    if (NS_FAILED(rv = theEventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                                                 &mainQueue))) {
        NS_ERROR("unable to get event queue for current thread");
        goto done;
    }

		NS_IF_RELEASE(mainQueue);

    // Create a stream data source and initialize it on argv[1], which
    // is hopefully a "file:" URL. (Actually, we can do _any_ kind of
    // URL, but only a "file:" URL will be written back to disk.)
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                          nsnull,
                                                          kIRDFXMLDataSourceIID,
                                                          (void**) &ds))) {
        NS_ERROR("unable to create RDF/XML data source");
        goto done;
    }

    if (NS_FAILED(rv = ds->SetSynchronous(PR_TRUE))) {
        NS_ERROR("unable to mark data source as synchronous");
        goto done;
    }

    // Okay, this should load the XML file...
    if (NS_FAILED(rv = ds->Init(argv[1]))) {
        NS_ERROR("unable to initialize data source");
        goto done;
    }

    // And finally, write it back out.
    if ((out = new ConsoleOutputStreamImpl()) == nsnull) {
        NS_ERROR("unable to create console output stream");
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }

    NS_ADDREF(out);

    if (NS_FAILED(rv = ds->QueryInterface(kIRDFXMLSourceIID, (void**) &source))) {
        NS_ERROR("unable to RDF/XML interface");
        goto done;
    }

    if (NS_FAILED(rv = source->Serialize(out))) {
        NS_ERROR("error serializing");
        goto done;
    }

done:
    NS_IF_RELEASE(out);
    NS_IF_RELEASE(ds);
    if (theRDFService) {
        nsServiceManager::ReleaseService(kRDFServiceCID, theRDFService);
        theRDFService = nsnull;
    }
    if (theEventQueueService) {
        nsServiceManager::ReleaseService(kEventQueueServiceCID, theEventQueueService);
        theEventQueueService = nsnull;
    }
    return (NS_FAILED(rv) ? 1 : 0);
}
