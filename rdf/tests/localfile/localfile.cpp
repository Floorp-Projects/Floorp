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

#include <io.h>
#include "nsIContent.h"
#include "nsIInputStream.h"
#include "nsINetService.h"
#include "nsINetService.h"
#include "nsIOutputStream.h"
#include "nsIPostToServer.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsLayoutCID.h" // for NS_NAMESPACEMANAGER_CID
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFCID.h"
#include "nsRepository.h"
#include "plevent.h"
#include "plstr.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define PARSER_DLL "raptorhtmlpars.dll"
#define RDF_DLL    "rdf.dll"
#define LAYOUT_DLL "raptorhtml.dll"
#endif

////////////////////////////////////////////////////////////////////////
// CIDs

// netlib
static NS_DEFINE_CID(kNetServiceCID,            NS_NETSERVICE_CID);

// rdf
static NS_DEFINE_CID(kRDFBookMarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFHTMLDocumentCID,       NS_RDFHTMLDOCUMENT_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFDataBaseCID,           NS_RDFDATABASE_CID);
static NS_DEFINE_CID(kRDFSimpleContentSinkCID,  NS_RDFSIMPLECONTENTSINK_CID);
static NS_DEFINE_CID(kRDFStreamDataSourceCID,   NS_RDFSTREAMDATASOURCE_CID);
static NS_DEFINE_CID(kRDFTreeDocumentCID,       NS_RDFTREEDOCUMENT_CID);

// parser
static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

// layout
static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);


////////////////////////////////////////////////////////////////////////
// IIDs

//NS_DEFINE_IID(kIPostToServerIID,       NS_IPOSTTOSERVER_IID);
NS_DEFINE_IID(kIOutputStreamIID,       NS_IOUTPUTSTREAM_IID);
NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
NS_DEFINE_IID(kIRDFXMLSourceIID,       NS_IRDFXMLSOURCE_IID);

static nsresult
SetupRegistry(void)
{
    nsRepository::RegisterFactory(kNetServiceCID,            NETLIB_DLL, PR_FALSE, PR_FALSE);

    nsRepository::RegisterFactory(kRDFBookMarkDataSourceCID, RDF_DLL,    PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kRDFHTMLDocumentCID,       RDF_DLL,    PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kRDFInMemoryDataSourceCID, RDF_DLL,    PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kRDFServiceCID,            RDF_DLL,    PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kRDFSimpleContentSinkCID,  RDF_DLL,    PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kRDFDataBaseCID,           RDF_DLL,    PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kRDFStreamDataSourceCID,   RDF_DLL,    PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kRDFTreeDocumentCID,       RDF_DLL,    PR_FALSE, PR_FALSE);

    nsRepository::RegisterFactory(kParserCID,                PARSER_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kWellFormedDTDCID,         PARSER_DLL, PR_FALSE, PR_FALSE);

    nsRepository::RegisterFactory(kNameSpaceManagerCID,      LAYOUT_DLL, PR_FALSE, PR_FALSE);

    return NS_OK;
}


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
    NS_IMETHOD Write(const char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aWriteCount) {
        write(1, aBuf + aOffset, aCount);
        *aWriteCount = aCount;
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(ConsoleOutputStreamImpl, kIOutputStreamIID);


////////////////////////////////////////////////////////////////////////

int
main(int argc, char** argv)
{
    nsresult rv;

    if (argc < 2) {
        fprintf(stderr, "usage: %s [url]\n", argv[0]);
        return 1;
    }

    PL_InitializeEventsLib("");
    PLEventQueue* mainQueue = PL_GetMainEventQueue();

    SetupRegistry();

    nsIRDFDataSource* ds        = nsnull;
    nsIRDFService* service      = nsnull;
    nsIRDFResource* theHomePage = nsnull;
    nsIRDFResource* NC_title    = nsnull;
    nsIRDFLiteral* theTitle     = nsnull;
    PRInt32 i;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFStreamDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &ds)))
        goto done;

    if (NS_FAILED(rv = ds->Init(argv[1])))
        goto done;

    // XXX This is really gross. I need to figure out the right way to do it...
    for (i = 0; i < 1000000; ++i) {
		PLEvent* event = PL_GetEvent(mainQueue);
        PL_HandleEvent(event);
    }

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &service)))
        goto done;

    if (NS_FAILED(rv = service->GetResource("http://home.netscape.com", &theHomePage)))
        goto done;

    if (NS_FAILED(rv = service->GetResource("http://rdf.nescape.com/NC#title", &NC_title)))
        goto done;

    if (NS_FAILED(rv = service->GetLiteral(nsAutoString("Netscape's Home Page"), &theTitle)))
        goto done;

    if (NS_FAILED(rv = ds->Assert(theHomePage, NC_title, theTitle, PR_TRUE)))
        goto done;

    if (NS_FAILED(rv = ds->Flush()))
        goto done;

done:
    NS_IF_RELEASE(theTitle);
    NS_IF_RELEASE(NC_title);
    NS_IF_RELEASE(theHomePage);
    NS_IF_RELEASE(ds);
    if (service) {
        nsServiceManager::ReleaseService(kRDFServiceCID, service);
        service = nsnull;
    }
    return (NS_FAILED(rv) ? 1 : 0);
}






