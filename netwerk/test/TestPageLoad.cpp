/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestCommon.h"
#include "nsNetUtil.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIComponentManager.h"
#include "prprf.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsTimer.h"
#include "prlong.h"
#include "plstr.h"
#include "nsSupportsArray.h"
#include "nsReadableUtils.h"
#include "nsIComponentRegistrar.h"
int getStrLine(const char *src, char *str, int ind, int max);
nsresult auxLoad(char *uriBuf);
//----------------------------------------------------------------------


#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(">>> %s failed: rv=%x\n", step, rv); \
        return rv;\
    } \
    PR_END_MACRO

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static nsIEventQueue* gEventQ = nsnull;
static PRBool gKeepRunning = PR_FALSE;
static nsCString globalStream;
//static char urlBuf[256];
static nsCOMPtr<nsIURI> baseURI;
static nsCOMPtr<nsISupportsArray> uriList;

//Temp, should remove:
static int numStart=0;
static int numFound=0;


//--------writer fun----------------------

static NS_METHOD streamParse (nsIInputStream* in,
                              void* closure,
                              const char* fromRawSegment,
                              PRUint32 toOffset,
                              PRUint32 count,
                              PRUint32 *writeCount) {

  char parseBuf[2048], loc[2048], lineBuf[2048];
  char *loc_t, *loc_t2;
  int i = 0;
  char *tmp;

  if(!globalStream.IsEmpty()) {
    globalStream.Append(fromRawSegment);
    tmp = ToNewCString(globalStream);
    //printf("\n>>NOW:\n^^^^^\n%s\n^^^^^^^^^^^^^^", tmp);
  } else {
    tmp = (char *)fromRawSegment;
  }

  while(i < (int)count) {
    i = getStrLine(tmp, lineBuf, i, count);
    if(i < 0) {
      *writeCount = count;
      return NS_OK;
    }
    parseBuf[0]='\0';
    if((loc_t=PL_strcasestr(lineBuf, "img"))!= NULL 
       || (loc_t=PL_strcasestr(lineBuf, "script"))!=NULL) {
      loc_t2=PL_strcasestr(loc_t, "src");
      if(loc_t2!=NULL) {
        loc_t2+=3;
        strcpy(loc, loc_t2);
        sscanf(loc, "=\"%[^\"]", parseBuf);
        if(parseBuf[0]=='\0')
          sscanf(loc, "=%s", parseBuf);         
        if(parseBuf[0]!='\0'){
          numFound++;
          auxLoad(parseBuf);
        }     
      }
    }

    /***NEED BETTER CHECK FOR STYLESHEETS
    if((loc_t=PL_strcasestr(lineBuf, "link"))!= NULL) { 
       loc_t2=PL_strcasestr(loc_t, "href");
      if(loc_t2!=NULL) {
        loc_t2+=4;
        strcpy(loc, loc_t2);
        //printf("%s\n", loc);
        sscanf(loc, "=\"%[^\"]", parseBuf);
        if(parseBuf[0]!='\0'){
          //printf("%s\n", parseBuf);
          numFound++;
          auxLoad(parseBuf);
        }     
      }
    }
    */
    if((loc_t=PL_strcasestr(lineBuf, "background"))!=NULL) {
      loc_t+=10;
      strcpy(loc, loc_t);
      sscanf(loc, "=\"%[^\"]", parseBuf);
      if(parseBuf[0]!='\0') {
        numFound++;
        auxLoad(parseBuf);
      }
    }
    i++;

  }
  *writeCount = count;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIStreamListener implementation
//-----------------------------------------------------------------------------

class MyListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    MyListener() { }
    virtual ~MyListener() {}
};

NS_IMPL_ISUPPORTS2(MyListener,
                   nsIRequestObserver,
                   nsIStreamListener)

NS_IMETHODIMP
MyListener::OnStartRequest(nsIRequest *req, nsISupports *ctxt)
{
  //printf(">>> OnStartRequest\n");
    numStart++;
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnStopRequest(nsIRequest *req, nsISupports *ctxt, nsresult status)
{
    //printf(">>> OnStopRequest status=%x\n", status);
    gKeepRunning--;
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnDataAvailable(nsIRequest *req, nsISupports *ctxt,
                            nsIInputStream *stream,
                            PRUint32 offset, PRUint32 count)
{
    //printf(">>> OnDataAvailable [count=%u]\n", count);
    nsresult rv = NS_ERROR_FAILURE;
    PRUint32 bytesRead=0;
    char buf[1024];

    if(ctxt == nsnull) {
      bytesRead=0;
      rv = stream->ReadSegments(streamParse, &offset, count, &bytesRead);
    } else {
      while (count) {
        PRUint32 amount = PR_MIN(count, sizeof(buf));
        rv = stream->Read(buf, amount, &bytesRead);  
        count -= bytesRead;
      }
    }

    if (NS_FAILED(rv)) {
      printf(">>> stream->Read failed with rv=%x\n", rv);
      return rv;
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// NotificationCallbacks implementation
//-----------------------------------------------------------------------------

class MyNotifications : public nsIInterfaceRequestor
                      , public nsIProgressEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK

    MyNotifications() { }
    virtual ~MyNotifications() {}
};

NS_IMPL_THREADSAFE_ISUPPORTS2(MyNotifications,
                              nsIInterfaceRequestor,
                              nsIProgressEventSink)

NS_IMETHODIMP
MyNotifications::GetInterface(const nsIID &iid, void **result)
{
    return QueryInterface(iid, result);
}

NS_IMETHODIMP
MyNotifications::OnStatus(nsIRequest *req, nsISupports *ctx,
                          nsresult status, const PRUnichar *statusText)
{
    //printf("status: %x\n", status);
    return NS_OK;
}

NS_IMETHODIMP
MyNotifications::OnProgress(nsIRequest *req, nsISupports *ctx,
                            PRUint32 progress, PRUint32 progressMax)
{
    //printf("progress: %u/%u\n", progress, progressMax);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// main, etc..
//-----------------------------------------------------------------------------

//---------getStrLine Helper function---------------
//Finds a newline in src starting at ind. Puts the
//line in str (must be big enough). Returns the index
//of the newline, or -1 if at end of string. If reaches 
//end of string ('\0'), then will copy contents to 
//globalStream. 
int getStrLine(const char *src, char *str, int ind, int max) {
  char c = src[ind];
  int i=0;
  globalStream.AssignLiteral("\0");
  while(c!='\n' && c!='\0' && i<max) {
    str[i] = src[ind];
    i++; ind++;
    c = src[ind];
  }
  str[i]='\0';
  if(i==max || c=='\0') {
    globalStream.Assign(str);
    //printf("\nCarryover (%d|%d):\n------------\n%s\n-------\n",i,max,str);
    return -1;
  }
  return ind;
}

//----------AUX LOAD-----------
nsresult auxLoad(char *uriBuf)
{
    nsresult rv;

    nsCOMPtr<nsISupportsPRBool> myBool = do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);

    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> chan;
    nsCOMPtr<nsIStreamListener> listener = new MyListener();
    nsCOMPtr<nsIInterfaceRequestor> callbacks = new MyNotifications();

    printf("Getting: %s", uriBuf);

    //If relative link
    if(strncmp(uriBuf, "http:", 5)) {
      //Relative link
      rv = NS_NewURI(getter_AddRefs(uri), uriBuf, baseURI);
      if (NS_FAILED(rv)) return(rv);
    } else {
      //Absolute link, no base needed
      rv = NS_NewURI(getter_AddRefs(uri), uriBuf);
      if (NS_FAILED(rv)) return(rv);
    }

    //Compare to see if exists
    PRUint32 num;
    uriList->Count(&num);
    PRBool equal;
    nsCOMPtr<nsIURI> uriTmp;
    for(PRUint32 i = 0; i < num; i++) {
      uriList->GetElementAt(i, getter_AddRefs(uriTmp));
      uri->Equals(uriTmp, &equal);
      if(equal) {
        printf("(duplicate, canceling) %s\n",uriBuf); 
        return NS_OK;
      }
    }
    printf("\n");
    uriList->AppendElement(uri);
    rv = NS_NewChannel(getter_AddRefs(chan), uri, nsnull, nsnull, callbacks);
    RETURN_IF_FAILED(rv, "NS_NewChannel");

    gKeepRunning++;
    rv = chan->AsyncOpen(listener, myBool);
    RETURN_IF_FAILED(rv, "AsyncOpen");

    return NS_OK;

}

//---------Buffer writer fun---------


//---------MAIN-----------

int main(int argc, char **argv)
{ 
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv;

    if (argc == 1) {
        printf("usage: TestPageLoad <url>\n");
        return -1;
    }
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nsnull);

        PRTime start, finish;

        rv = NS_NewISupportsArray(getter_AddRefs(uriList));
        RETURN_IF_FAILED(rv, "NS_NewISupportsArray");

        // Create the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> eqs =
                 do_GetService(kEventQueueServiceCID, &rv);
        RETURN_IF_FAILED(rv, "do_GetService(EventQueueService)");

        rv = eqs->CreateMonitoredThreadEventQueue();
        RETURN_IF_FAILED(rv, "CreateMonitoredThreadEventQueue");

        rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
        RETURN_IF_FAILED(rv, "GetThreadEventQueue");

        printf("Loading necko ... \n");
        nsCOMPtr<nsIChannel> chan;
        nsCOMPtr<nsIStreamListener> listener = new MyListener();
        nsCOMPtr<nsIInterfaceRequestor> callbacks = new MyNotifications();

        rv = NS_NewURI(getter_AddRefs(baseURI), argv[1]);
        RETURN_IF_FAILED(rv, "NS_NewURI");

        rv = NS_NewChannel(getter_AddRefs(chan), baseURI, nsnull, nsnull, callbacks);
        RETURN_IF_FAILED(rv, "NS_OpenURI");
        gKeepRunning++;

        //TIMER STARTED-----------------------
        printf("Starting clock ... \n");
        start = PR_Now();
        rv = chan->AsyncOpen(listener, nsnull);
        RETURN_IF_FAILED(rv, "AsyncOpen");

        while (gKeepRunning) {
            gEventQ->ProcessPendingEvents();
        }

        finish = PR_Now();
        PRUint32 totalTime32;
        PRUint64 totalTime64;
        LL_SUB(totalTime64, finish, start);
        LL_L2UI(totalTime32, totalTime64);

        printf("\n\n--------------------\nAll done:\nnum found:%d\nnum start:%d\n", numFound, numStart);

        printf("\n\n>>PageLoadTime>>%u>>\n\n", totalTime32);
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}
