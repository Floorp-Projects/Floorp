/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIComponentManager.h"
#include "prprf.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "plstr.h"
#include "nsCOMArray.h"
#include "nsIComponentRegistrar.h"
#include <algorithm>

namespace TestPageLoad {

int getStrLine(const char *src, char *str, int ind, int max);
nsresult auxLoad(char *uriBuf);
//----------------------------------------------------------------------


#define RETURN_IF_FAILED(rv, ret, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(">>> %s failed: rv=%x\n", step, static_cast<uint32_t>(rv)); \
        return ret;\
    } \
    PR_END_MACRO

static nsCString globalStream;
//static char urlBuf[256];
static nsCOMPtr<nsIURI> baseURI;
static nsCOMArray<nsIURI> uriList;

//Temp, should remove:
static int numStart=0;
static int numFound=0;

static int32_t gKeepRunning = 0;


//--------writer fun----------------------

static NS_METHOD streamParse (nsIInputStream* in,
                              void* closure,
                              const char* fromRawSegment,
                              uint32_t toOffset,
                              uint32_t count,
                              uint32_t *writeCount) {

  char parseBuf[2048], loc[2048], lineBuf[2048];
  char *loc_t, *loc_t2;
  int i = 0;
  const char *tmp;

  if(!globalStream.IsEmpty()) {
    globalStream.Append(fromRawSegment);
    tmp = globalStream.get();
    //printf("\n>>NOW:\n^^^^^\n%s\n^^^^^^^^^^^^^^", tmp);
  } else {
    tmp = fromRawSegment;
  }

  while(i < (int)count) {
    i = getStrLine(tmp, lineBuf, i, count);
    if(i < 0) {
      *writeCount = count;
      return NS_OK;
    }
    parseBuf[0]='\0';
    if((loc_t=PL_strcasestr(lineBuf, "img"))!= nullptr 
       || (loc_t=PL_strcasestr(lineBuf, "script"))!=nullptr) {
      loc_t2=PL_strcasestr(loc_t, "src");
      if(loc_t2!=nullptr) {
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
    if((loc_t=PL_strcasestr(lineBuf, "link"))!= nullptr) { 
       loc_t2=PL_strcasestr(loc_t, "href");
      if(loc_t2!=nullptr) {
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
    if((loc_t=PL_strcasestr(lineBuf, "background"))!=nullptr) {
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

NS_IMPL_ISUPPORTS(MyListener,
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
    if (--gKeepRunning == 0)
      QuitPumpingEvents();
    return NS_OK;
}

NS_IMETHODIMP
MyListener::OnDataAvailable(nsIRequest *req, nsISupports *ctxt,
                            nsIInputStream *stream,
                            uint64_t offset, uint32_t count)
{
    //printf(">>> OnDataAvailable [count=%u]\n", count);
    nsresult rv = NS_ERROR_FAILURE;
    uint32_t bytesRead=0;
    char buf[1024];

    if(ctxt == nullptr) {
      bytesRead=0;
      rv = stream->ReadSegments(streamParse, nullptr, count, &bytesRead);
    } else {
      while (count) {
        uint32_t amount = std::min<uint32_t>(count, sizeof(buf));
        rv = stream->Read(buf, amount, &bytesRead);  
        count -= bytesRead;
      }
    }

    if (NS_FAILED(rv)) {
      printf(">>> stream->Read failed with rv=%x\n",
             static_cast<uint32_t>(rv));
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
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK

    MyNotifications() { }
    virtual ~MyNotifications() {}
};

NS_IMPL_ISUPPORTS(MyNotifications,
                  nsIInterfaceRequestor,
                  nsIProgressEventSink)

NS_IMETHODIMP
MyNotifications::GetInterface(const nsIID &iid, void **result)
{
    return QueryInterface(iid, result);
}

NS_IMETHODIMP
MyNotifications::OnStatus(nsIRequest *req, nsISupports *ctx,
                          nsresult status, const char16_t *statusText)
{
    //printf("status: %x\n", status);
    return NS_OK;
}

NS_IMETHODIMP
MyNotifications::OnProgress(nsIRequest *req, nsISupports *ctx,
                            uint64_t progress, uint64_t progressMax)
{
    // char buf[100];
    // PR_snprintf(buf, sizeof(buf), "%llu/%llu\n", progress, progressMax);
    // printf("%s", buf);
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
  globalStream.Assign('\0');
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
    bool equal;
    for(int32_t i = 0; i < uriList.Count(); i++) {
      uri->Equals(uriList[i], &equal);
      if(equal) {
        printf("(duplicate, canceling) %s\n",uriBuf); 
        return NS_OK;
      }
    }
    printf("\n");
    uriList.AppendObject(uri);
    rv = NS_NewChannel(getter_AddRefs(chan), uri, nullptr, nullptr, callbacks);
    RETURN_IF_FAILED(rv, rv, "NS_NewChannel");

    gKeepRunning++;
    rv = chan->AsyncOpen(listener, myBool);
    RETURN_IF_FAILED(rv, rv, "AsyncOpen");

    return NS_OK;

}

//---------Buffer writer fun---------

} // namespace

using namespace TestPageLoad;

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
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);

        PRTime start, finish;

        printf("Loading necko ... \n");
        nsCOMPtr<nsIChannel> chan;
        nsCOMPtr<nsIStreamListener> listener = new MyListener();
        nsCOMPtr<nsIInterfaceRequestor> callbacks = new MyNotifications();

        rv = NS_NewURI(getter_AddRefs(baseURI), argv[1]);
        RETURN_IF_FAILED(rv, -1, "NS_NewURI");

        rv = NS_NewChannel(getter_AddRefs(chan), baseURI, nullptr, nullptr, callbacks);
        RETURN_IF_FAILED(rv, -1, "NS_OpenURI");
        gKeepRunning++;

        //TIMER STARTED-----------------------
        printf("Starting clock ... \n");
        start = PR_Now();
        rv = chan->AsyncOpen(listener, nullptr);
        RETURN_IF_FAILED(rv, -1, "AsyncOpen");

        PumpEvents();

        finish = PR_Now();
        uint32_t totalTime32 = uint32_t(finish - start);

        printf("\n\n--------------------\nAll done:\nnum found:%d\nnum start:%d\n", numFound, numStart);

        printf("\n\n>>PageLoadTime>>%u>>\n\n", totalTime32);
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}
