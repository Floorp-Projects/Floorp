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

  This is a test script that uses the nsStreamDataSource class to read
  a file from the local file system (using a "file:" URL) and write it
  back. The idea is that the RDF that gets written out should be
  equivalent (but not necessarily identical) to the RDF that was read
  in.

  Currently, the nsFileSpec stuff is _really_ sensitive to Win32 file
  names: be sure to use a file URL that looks like
  "file://G:/tmp/foo.rdf"; that is, two forward-slashes after the
  "file:", a drive letter specification with a colon (not a pipe), and
  forward-slashes in the path.

 */

#ifndef XP_UNIX
#include <io.h>
#endif

#include "nsIContent.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsIOutputStream.h"
#include "nsIPostToServer.h"
#include "nsIRDFXMLDataSource.h"
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
#include "plevent.h"
#include "plstr.h"
#include "rdf.h"

#if defined(XP_PC)
#define DOM_DLL    "jsdom.dll"
#define LAYOUT_DLL "gkhtml.dll"
#define NETLIB_DLL "netlib.dll"
#define PARSER_DLL "gkparser.dll"
#define RDF_DLL    "rdf.dll"
#define XPCOM_DLL  "xpcom32.dll"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#define DOM_DLL    "libjsdom"MOZ_DLL_SUFFIX
#define LAYOUT_DLL "libgklayout"MOZ_DLL_SUFFIX
#define NETLIB_DLL "libnecko"MOZ_DLL_SUFFIX
#define PARSER_DLL "libtmlpars"MOZ_DLL_SUFFIX
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

// netlib
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

// rdf
static NS_DEFINE_CID(kRDFBookMarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,        NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,      NS_RDFXMLDATASOURCE_CID);

// parser
static NS_DEFINE_CID(kParserCID,                NS_PARSER_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

// layout
static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);

// dom
static NS_DEFINE_IID(kScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID,     NS_EVENTQUEUESERVICE_CID);


////////////////////////////////////////////////////////////////////////
// IIDs

NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
NS_DEFINE_IID(kIOutputStreamIID,       NS_IOUTPUTSTREAM_IID);
NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
NS_DEFINE_IID(kIRDFXMLDataSourceIID,   NS_IRDFXMLDATASOURCE_IID);

static nsresult
SetupRegistry(void)
{
    // netlib
    nsComponentManager::RegisterComponent(kIOServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);

    // parser
    nsComponentManager::RegisterComponent(kParserCID,                NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kWellFormedDTDCID,         NULL, NULL, PARSER_DLL, PR_FALSE, PR_FALSE);

    // layout
    nsComponentManager::RegisterComponent(kNameSpaceManagerCID,      NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);

    // dom
    nsComponentManager::RegisterComponent(kScriptNameSetRegistryCID, NULL, NULL, DOM_DLL,    PR_FALSE, PR_FALSE);

    // xpcom
    nsComponentManager::RegisterComponent(kEventQueueServiceCID,     NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

int
main(int argc, char** argv)
{
    nsresult rv;

    if (argc < 2) {
        fprintf(stderr, "usage: %s [url]\n", argv[0]);
        return 1;
    }

    SetupRegistry();

    nsIEventQueueService* theEventQueueService = nsnull;
    nsIEventQueue* mainQueue      = nsnull;
    nsIRDFService* theRDFService = nsnull;
    nsIRDFXMLDataSource* ds      = nsnull;
    nsIRDFResource* theHomePage  = nsnull;
    nsIRDFResource* NC_title     = nsnull;
    nsIRDFLiteral* theTitle      = nsnull;

    // Get netlib off the floor...
    if (NS_FAILED(rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                                    kIEventQueueServiceIID,
                                                    (nsISupports**) &theEventQueueService)))
        goto done;

    if (NS_FAILED(rv = theEventQueueService->CreateThreadEventQueue()))
        goto done;

    if (NS_FAILED(rv = theEventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                                                 &mainQueue)))
        goto done;

		NS_IF_RELEASE(mainQueue);

    // Create a stream data source and initialize it on argv[1], which
    // is hopefully a "file:" URL. (Actually, we can do _any_ kind of
    // URL, but only a "file:" URL will be written back to disk.)
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                    nsnull,
                                                    kIRDFXMLDataSourceIID,
                                                    (void**) &ds)))
        goto done;

    if (NS_FAILED(rv = ds->SetSynchronous(PR_TRUE)))
        goto done;

    // Okay, this should load the XML file...
    if (NS_FAILED(rv = ds->Init(argv[1])))
        goto done;

    // Now take the graph and munge it a little bit...
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &theRDFService)))
        goto done;

    if (NS_FAILED(rv = theRDFService->GetResource("http://home.netscape.com", &theHomePage)))
        goto done;

    if (NS_FAILED(rv = theRDFService->GetResource(NC_NAMESPACE_URI "title", &NC_title)))
        goto done;

    if (NS_FAILED(rv = theRDFService->GetLiteral(nsAutoString("Netscape's Home Page"), &theTitle)))
        goto done;

    if (NS_FAILED(rv = ds->Assert(theHomePage, NC_title, theTitle, PR_TRUE)))
        goto done;

    // And finally, write it back out.
    if (NS_FAILED(rv = ds->Flush()))
        goto done;

done:
    NS_IF_RELEASE(theTitle);
    NS_IF_RELEASE(NC_title);
    NS_IF_RELEASE(theHomePage);
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






