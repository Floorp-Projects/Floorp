/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIRobotSink.h"
#include "nsIRobotSinkObserver.h"
#include "nsIParser.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h" 
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

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

NS_IMPL_ISUPPORTS1(RobotSinkObserver, nsIRobotSinkObserver)

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

class CStreamListener:  public nsIWebProgressListener,
                        public nsSupportsWeakReference
{
public:
  CStreamListener() {
    NS_INIT_REFCNT();

  }

  virtual ~CStreamListener() {
  }

  NS_DECL_ISUPPORTS

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER
};

// nsIWebProgressListener implementation
NS_IMETHODIMP
CStreamListener::OnStateChange(nsIWebProgress* aWebProgress, 
                   nsIRequest *aRequest, 
                   PRInt32 progressStateFlags, 
                   nsresult aStatus) {
    if (progressStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT)
        if (progressStateFlags & nsIWebProgressListener::STATE_STOP) {
            fputs("done.\n",stdout);
            g_bReadyForNextUrl = PR_TRUE;
        }
    return NS_OK;
}

NS_IMETHODIMP
CStreamListener::OnProgressChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRInt32 aCurSelfProgress,
                                     PRInt32 aMaxSelfProgress,
                                     PRInt32 aCurTotalProgress,
                                     PRInt32 aMaxTotalProgress) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CStreamListener::OnLocationChange(nsIWebProgress* aWebProgress,
                      nsIRequest* aRequest,
                      nsIURI *location) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
CStreamListener::OnStatusChange(nsIWebProgress* aWebProgress,
                    nsIRequest* aRequest,
                    nsresult aStatus,
                    const PRUnichar* aMessage) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
CStreamListener::OnSecurityChange(nsIWebProgress *aWebProgress, 
                      nsIRequest *aRequest, 
                      PRInt32 state) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS2(CStreamListener,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

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
    nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &rv));
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
    static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

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
      nsCOMPtr<nsIWebProgress> progress(do_GetInterface(docShell, &rv));
      if (NS_FAILED(rv)) return rv;

      (void) progress->AddProgressListener(pl);

      char* spec;
      (void)url->GetSpec(&spec);
      nsAutoString theSpec; theSpec.AssignWithConversion(spec);
      nsCRT::free(spec);
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
      webNav->LoadURI(theSpec.get(), nsIWebNavigation::LOAD_FLAGS_NONE);/* XXX hook up stream listener here! */
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
