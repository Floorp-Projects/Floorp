/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include "nsIRobotSink.h"
#include "nsIRobotSinkObserver.h"
#include "nsIParser.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIComponentManager.h"
#include "nsParserCIID.h"

static NS_DEFINE_IID(kIRobotSinkObserverIID, NS_IROBOTSINKOBSERVER_IID);

class RobotSinkObserver : public nsIRobotSinkObserver {
public:
  RobotSinkObserver() {
    NS_INIT_REFCNT();
  }

  virtual ~RobotSinkObserver() {
  }

  NS_DECL_ISUPPORTS

  NS_IMETHOD ProcessLink(const nsString& aURLSpec);
  NS_IMETHOD VerifyDirectory (const char * verify_dir);
    
};

static nsVoidArray * g_workList;
static nsVoidArray * g_duplicateList;
static int g_iProcessed;
static int g_iMaxProcess = 5000;
static PRBool g_bHitTop;
static PRBool g_bReadyForNextUrl;

NS_IMPL_ISUPPORTS(RobotSinkObserver, kIRobotSinkObserverIID);

NS_IMETHODIMP RobotSinkObserver::VerifyDirectory(const char * verify_dir)
{
   return NS_OK;
}

NS_IMETHODIMP RobotSinkObserver::ProcessLink(const nsString& aURLSpec)
{
  if (!g_bHitTop) {
     
     nsAutoString str;
     // Geez this is ugly. temporary hack to only process html files
     str.Truncate();
     nsString(aURLSpec).Right(str,1);
     if (!str.EqualsWithConversion("/"))
     {
        str.Truncate();
        nsString(aURLSpec).Right(str,4);
        if (!str.EqualsWithConversion("html"))
        {
           str.Truncate();
           nsString(aURLSpec).Right(str,3);
           if (!str.EqualsWithConversion("htm"))
              return NS_OK;
        }
     }
     PRInt32 nCount = g_duplicateList->Count();
     if (nCount > 0)
     {
        for (PRInt32 n = 0; n < nCount; n++)
        {
           nsString * pstr = (nsString *)g_duplicateList->ElementAt(n);
           if (pstr->Equals(aURLSpec)) {
              fputs ("Robot: (duplicate '",stdout);
              fputs (aURLSpec,stdout);
              fputs ("')\n",stdout);
              return NS_OK;
           }
        }
     }
     g_duplicateList->AppendElement(new nsString(aURLSpec));
     str.Truncate();
     nsString(aURLSpec).Left(str,5);
     if (str.EqualsWithConversion("http:")) {
        g_iProcessed++;
        if (g_iProcessed == (g_iMaxProcess > 0 ? g_iMaxProcess-1 : 0))
           g_bHitTop = PR_TRUE;
        g_workList->AppendElement(new nsString(aURLSpec));
     }
     else {
        fputs ("Robot: (cannot process URL types '",stdout);
        fputs (aURLSpec,stdout);
        fputs ("')\n",stdout);
     }
  }
  return NS_OK;
}

extern "C" NS_EXPORT void SetVerificationDirectory(char * verify_dir);

class CStreamListener:  public nsIDocumentLoaderObserver
{
public:
  CStreamListener() {
    NS_INIT_REFCNT();

  }

  virtual ~CStreamListener() {
  }

  NS_DECL_ISUPPORTS

  // nsIDocumentLoaderObserver
  NS_DECL_NSIDOCUMENTLOADEROBSERVER
};

// document loader observer implementation
NS_IMETHODIMP
CStreamListener::OnStartDocumentLoad(nsIDocumentLoader* loader, 
                                     nsIURI* aURL, 
                                     const char* aCommand)
{
  return NS_OK;
}

NS_IMETHODIMP
CStreamListener::OnEndDocumentLoad(nsIDocumentLoader* loader,
                                   nsIChannel* channel,
                                   nsresult aStatus)
{
  fputs("done.\n",stdout);
  g_bReadyForNextUrl = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
CStreamListener::OnStartURLLoad(nsIDocumentLoader* loader,
                                nsIChannel* channel)
{
  return NS_OK;
}

NS_IMETHODIMP
CStreamListener::OnProgressURLLoad(nsIDocumentLoader* loader,
                                   nsIChannel* channel,
                                   PRUint32 aProgress, 
                                   PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
CStreamListener::OnStatusURLLoad(nsIDocumentLoader* loader,
                                 nsIChannel* channel,
                                 nsString& aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
CStreamListener::OnEndURLLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel,
                              nsresult aStatus)
{
  return NS_OK;
}


nsresult CStreamListener::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  return NS_ERROR_NOT_IMPLEMENTED;      // never called
}

NS_IMPL_ADDREF(CStreamListener)
NS_IMPL_RELEASE(CStreamListener)

extern "C" NS_EXPORT void DumpVectorRecord(void);
//----------------------------------------------------------------------
extern "C" NS_EXPORT int DebugRobot(
   nsVoidArray * workList, 
   nsIDocShell * docShell, 
   int iMaxLoads, 
   char * verify_dir,
   void (*yieldProc )(const char *)
   )
{
  int iCount = 1;
  CStreamListener * pl = new CStreamListener; 
  NS_ADDREF(pl);

  if (nsnull==workList)
     return -1;
  g_iMaxProcess = iMaxLoads;
  g_iProcessed = 0;
  g_bHitTop = PR_FALSE;
  g_duplicateList = new nsVoidArray();
  RobotSinkObserver* myObserver = new RobotSinkObserver();
  NS_ADDREF(myObserver);
  g_workList = workList;

/*
  nsIDTDDebug * pIDTDDebug;
  nsresult rval = NS_NewDTDDebug(&pIDTDDebug);
  if (NS_OK != rval) {
    fputs("Cannot create parser debugger.\n", stdout);
    NS_RELEASE(myObserver);
    return -1;
  }
  pIDTDDebug->SetVerificationDirectory(verify_dir);
*/

  for (;;) {
    PRInt32 n = g_workList->Count();
    if (0 == n) {
      break;
    }
    nsString* urlName = (nsString*) g_workList->ElementAt(n - 1);
    g_workList->RemoveElementAt(n - 1);

    // Create url
    nsIURI* url;
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIURI *uri = nsnull;
    char *uriStr = urlName->ToNewCString();
    if (!uriStr) return NS_ERROR_OUT_OF_MEMORY;
    rv = service->NewURI(uriStr, nsnull, &uri);
    nsCRT::free(uriStr);
    if (NS_FAILED(rv)) return rv;

    rv = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&url);
    NS_RELEASE(uri);
    if (NS_OK != rv) {
      printf("invalid URL: '");
      fputs(*urlName, stdout);
      printf("'\n");
      NS_RELEASE(myObserver);
      return -1;
    }

    char str_num[25];
    sprintf (str_num,"%d",iCount++);
    fputs ("Robot: parsing(",stdout);
    fputs (str_num,stdout);
    fputs (") ",stdout);
    fputs (*urlName,stdout);
    fputs ("...",stdout);

    delete urlName;

    nsIParser* parser;

    static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
    static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

    rv = nsComponentManager::CreateInstance(kCParserCID, 
                                      nsnull, 
                                      kCParserIID, 
                                      (void **)&parser);

    if (NS_OK != rv) {
      printf("can't make parser\n");
      NS_RELEASE(myObserver);
      return -1;
    }

    nsIRobotSink* sink;
    rv = NS_NewRobotSink(&sink);
    if (NS_OK != rv) {
      printf("can't make parser\n");
      NS_RELEASE(myObserver);
      return -1;
    }
    sink->Init(url);
    sink->AddObserver(myObserver);

    parser->SetContentSink(sink);
    g_bReadyForNextUrl = PR_FALSE;  

    parser->Parse(url, nsnull,PR_TRUE);/* XXX hook up stream listener here! */
    while (!g_bReadyForNextUrl) {
      if (yieldProc != NULL) {
        char* spec;
        (void)url->GetSpec(&spec);
        (*yieldProc)(spec);
        nsCRT::free(spec);
      }
    }
    g_bReadyForNextUrl = PR_FALSE;
    if (docShell) {
      nsIDocumentLoader *docLoader;

      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
      webShell->GetDocumentLoader(docLoader);
      if (docLoader) {
        docLoader->AddObserver(pl);
        NS_RELEASE(docLoader);
      }
      char* spec;
      (void)url->GetSpec(&spec);
      nsAutoString theSpec; theSpec.AssignWithConversion(spec);
      nsCRT::free(spec);
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
      webNav->LoadURI(theSpec.GetUnicode());/* XXX hook up stream listener here! */
      while (!g_bReadyForNextUrl) {
        if (yieldProc != NULL) {
          (void)url->GetSpec(&spec);
          (*yieldProc)(spec);
          nsCRT::free(spec);
        }
      }
    }  

    NS_RELEASE(sink);
    NS_RELEASE(parser);
    NS_RELEASE(url);
  }

  fputs ("Robot completed.\n", stdout);

  NS_RELEASE(pl);
  NS_RELEASE(myObserver);

//  pIDTDDebug->DumpVectorRecord();
  //NS_RELEASE(pIDTDDebug);

  return 0;
}
