/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsIComponentManager.h"
#include "nsITimer.h"
#include "nsNetService.h"
#include "nsNetStream.h"
#include "nsNetFile.h"
#include "nsStubContext.h"
#include "prefapi.h"
#include "mkprefs.h"
extern "C" {
#include "mkutils.h"
#include "mkgeturl.h"
#include "mktrace.h"
#include "mkstream.h"
#include "cvchunk.h"
#include "httpurl.h"
} /* end of extern "C" */

#include "netcache.h"
#include "cookies.h"
#include "plstr.h"

#include "nsString.h"
#include "nsINetlibURL.h"
#include "nsIProtocolConnection.h"
#include "nsINetlibURL.h"
#include "nsIProtocolURLFactory.h"
#include "nsIProtocol.h"
#include "nsIURLGroup.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsCRT.h"
#include "nsSocketTransport.h"

#include "nsIChromeRegistry.h"

#ifdef XP_PC
#include <windows.h>
static HINSTANCE g_hInst = NULL;
#endif

/*
** Define TIMEBOMB_ON for beta builds.
** Undef TIMEBOMB_ON for release builds.
*/
/*#define TIMEBOMB_ON*/
#undef TIMEBOMB_ON

/*
** After this date all hell breaks loose
*/
#ifdef TIMEBOMB_ON
#define TIME_BOMB_TIME          917856001	/* 2/01/98 + 1 secs */
#define TIME_BOMB_WARNING_TIME  915955201	/* 1/10/98 + 1 secs */
#else
#define TIME_BOMB_TIME          -1
#define TIME_BOMB_WARNING_TIME  -1
#endif


// Declare the nsFile struct here so it's state is initialized before
// we initialize netlib.
#ifdef NS_NET_FILE

#include "nsNetFile.h"
#if defined(XP_MAC) || defined(XP_UNIX)
static nsNetFileInit* netFileInit = nsnull;
#else
static nsNetFileInit netFileInit;
#endif

#endif // NS_NET_FILE
// End nsFile specific

/* XXX: Legacy definitions... */
// Global count of active urls from mkgeturl.c
extern "C" int NET_TotalNumberOfProcessingURLs;

extern "C" HTTP_Version DEFAULT_VERSION;

extern "C" void net_AddrefContext(MWContext* window_id);
static void bam_exit_routine(URL_Struct *URL_s, int status, MWContext *window_id);

#if defined(XP_WIN) && !defined(NETLIB_THREAD)
nsresult PerformNastyWindowsAsyncDNSHack(URL_Struct* URL_s, nsIURL* aURL);
#endif /* XP_WIN && !NETLIB_THREAD */

char *mangleResourceIntoFileURL(const char* aResourceFileName);

extern nsIStreamListener* ns_NewStreamListenerProxy(nsIStreamListener* aListener, PLEventQueue* aEventQ);

extern "C" {
#if defined(XP_WIN) || defined(XP_OS2)
extern char *XP_AppCodeName;
extern char *XP_AppVersion;
extern char *XP_AppName;
extern char *XP_AppLanguage;
extern char *XP_AppPlatform;
#else
extern const char *XP_AppCodeName;
extern const char *XP_AppVersion;
extern const char *XP_AppName;
extern const char *XP_AppLanguage;
extern const char *XP_AppPlatform;
#endif

} /* end of extern "C" */

static NS_DEFINE_IID(kINetlibURLIID,  NS_INETLIBURL_IID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

// Chrome registry service for handling of chrome URLs
static NS_DEFINE_IID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);
static NS_DEFINE_IID(kIChromeRegistryIID, NS_ICHROMEREGISTRY_IID);
nsIChromeRegistry* nsNetlibService::gChromeRegistry = nsnull;
int nsNetlibService::gRefCnt = 0;

nsNetlibService::nsNetlibService()
{
  nsresult rv;

    NS_INIT_REFCNT();

  /*
   * Cache the EventQueueService...
   */
  // XXX: What if this fails?
  mEventQService = nsnull;
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **)&mEventQService);

    /*
      m_stubContext = new_stub_context();
    */
    mPollingTimer = nsnull;
    
    mNetlibThread = new nsNetlibThread();
    if (nsnull != mNetlibThread) {
        mNetlibThread->Start();
    }

    /* Setup our default prefs. Eventually these will come out of a default
     * all.js file, but, for now each module needs to address their own 
     * default settings. */

    PREF_SetDefaultIntPref(pref_proxyType, 3);
    PREF_SetDefaultCharPref(pref_proxyACUrl, "");
    PREF_SetDefaultCharPref(pref_socksServer, "");
    PREF_SetDefaultIntPref(pref_socksPort, 0);
    PREF_SetDefaultCharPref(pref_proxyFtpServer, "");
    PREF_SetDefaultIntPref(pref_proxyFtpPort, 0);
    PREF_SetDefaultCharPref(pref_proxyGopherServer, "");
    PREF_SetDefaultIntPref(pref_proxyGopherPort, 0);
    PREF_SetDefaultCharPref(pref_proxyHttpServer, "");
    PREF_SetDefaultIntPref(pref_proxyHttpPort, 0);
    PREF_SetDefaultCharPref(pref_proxyNewsServer, "");
    PREF_SetDefaultIntPref(pref_proxyNewsPort, 0);
    PREF_SetDefaultCharPref(pref_proxyWaisServer, "");
    PREF_SetDefaultIntPref(pref_proxyWaisPort, 0);
    PREF_SetDefaultCharPref(pref_proxyNoProxiesOn, "");
    PREF_SetDefaultCharPref(pref_padPacURL, "");
    PREF_SetDefaultCharPref(pref_scriptName, "");

    PREF_SetDefaultIntPref("timebomb.expiration_time",TIME_BOMB_TIME);
    PREF_SetDefaultIntPref("timebomb.warning_time",   TIME_BOMB_WARNING_TIME);

    // XXX: Where should the defaults really come from
    XP_AppCodeName = PL_strdup("Mozilla");
    XP_AppName     = PL_strdup("Netscape");

    /* 
     * XXX: Some of these should come from resources and/or 
     * platform-specific code. 
     */
    XP_AppLanguage = PL_strdup("en");
#ifdef XP_WIN
    XP_AppPlatform = PL_strdup("Win95");
#elif defined(XP_MAC)
    XP_AppPlatform = PL_strdup("MacPPC");
#elif defined(XP_UNIX)
    /* XXX: Need to differentiate between various Unisys */
    XP_AppPlatform = PL_strdup("Unix");
#endif

    /* XXXXX TEMPORARY TESTING HACK XXXXX */
    char buf[64];
    char *ver = PR_GetEnv("NG_REQUEST_VER");
    /* Build up the appversion. */
    sprintf(buf, "%s [%s] (%s; I)",
        (ver ? ver : "5.0"), 
        XP_AppLanguage, 
        XP_AppPlatform);
    if (XP_AppVersion)
        PR_Free((char *)XP_AppVersion);
    XP_AppVersion = PL_strdup(buf);

    mProtocols = new nsHashtable();
    PR_ASSERT(mProtocols);

    // Create the chrome registry.  If it fails, that's ok.
    gRefCnt++;
    
    if (gRefCnt == 1)
    {
        gChromeRegistry = nsnull;
        if (NS_FAILED(nsServiceManager::GetService(kChromeRegistryCID,
                                      kIChromeRegistryIID,
                                      (nsISupports **)&gChromeRegistry))) {
            gChromeRegistry = nsnull;
        }
        else gChromeRegistry->Init(); // Load the chrome registry
    }
}


static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsNetlibService,kINetServiceIID);


nsNetlibService::~nsNetlibService()
{
    TRACEMSG(("nsNetlibService is being destroyed...\n"));

    gRefCnt--;
    if (gRefCnt == 0)
    {
        NS_IF_RELEASE(gChromeRegistry);
        gChromeRegistry = nsnull;
    }

    /*
      if (NULL != m_stubContext) {
        free_stub_context((MWContext *)m_stubContext);
        m_stubContext = NULL;
      }
    */

    NS_IF_RELEASE(mPollingTimer);

    if (nsnull != mNetlibThread) {
        mNetlibThread->Stop();
        delete mNetlibThread;
    }

    if (nsnull != mEventQService) {
      nsServiceManager::ReleaseService(kEventQueueServiceCID, mEventQService);
      mEventQService = nsnull;
    }

    delete mProtocols;
}



void nsNetlibService::SetupURLStruct(nsIURL *aUrl, URL_Struct *aURL_s) 
{
  nsresult rv;
  nsILoadAttribs* loadAttribs;
  rv = aUrl->GetLoadAttribs(&loadAttribs);


  /* If this url has load attributes, setup the underlying url struct
   * accordingly. */
  if (loadAttribs) {
    nsURLLoadType loadType;
    nsURLReloadType reloadType;
    PRUint32 localIP;
    char* byteRangeHeader=NULL;

    NS_VERIFY_THREADSAFE_INTERFACE(loadAttribs);

    rv = loadAttribs->GetReloadType(&reloadType);
    if (NS_FAILED(rv)) {
      reloadType = nsURLReload;
    }
    if ((reloadType == nsURLReloadBypassProxy) ||
        (reloadType == nsURLReloadBypassCacheAndProxy)) {
      PRBool bBypassProxy;

      rv = loadAttribs->GetBypassProxy(&bBypassProxy);
      if (NS_FAILED(rv)) {
        bBypassProxy = PR_FALSE;
      }
      aURL_s->bypassProxy = bBypassProxy;
    }
    /* Set the NET_ReloadMethod to correspond with what we've 
     * been asked to do.
     *
     * 0 = nsURLReload (normal)
     * 1 = nsURLReloadBypassCache
     * 2 = nsURLReloadBypassProxy
     * 3 = nsURLReloadBypassCacheAndProxy
     */
    if ((reloadType == nsURLReloadBypassCache) ||
        (reloadType == nsURLReloadBypassCacheAndProxy)) {
      aURL_s->force_reload = NET_SUPER_RELOAD;
    } else {
      aURL_s->force_reload = NET_NORMAL_RELOAD;
    }

    rv = loadAttribs->GetLoadType(&loadType);
    if (NS_FAILED(rv)) {
      loadType = nsURLLoadNormal;
    }
    if (loadType == nsURLLoadBackground) {
      aURL_s->load_background = PR_TRUE;
    } else {
      aURL_s->load_background = PR_FALSE;
    }

    rv = loadAttribs->GetLocalIP(&localIP);
    if (NS_FAILED(rv)) {
      localIP = 0;
    }
    aURL_s->localIP = localIP;

    rv = loadAttribs->GetByteRangeHeader(&byteRangeHeader);
    if (NS_FAILED(rv)) 
    {
      byteRangeHeader = NULL;
    }
    else
    {
        aURL_s->range_header = byteRangeHeader;
    }

    NS_RELEASE(loadAttribs);
  }
}

nsresult nsNetlibService::OpenStream(nsIURL *aUrl,
                                     nsIStreamListener *aConsumer)
{
    URL_Struct *URL_s;
    nsConnectionInfo *pConn;
    nsINetlibURL *netlibURL;
    nsresult result;
    nsIStreamListener* consumer;
    PLEventQueue* evQueue = nsnull;

    if ((NULL == aConsumer) || (NULL == aUrl)) {
        return NS_FALSE;
    }

#if defined(NETLIB_THREAD)
    if (nsnull != mEventQService) {
      mEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &evQueue);
    }
    if (nsnull == evQueue) {
      return NS_FALSE;
    }

    consumer = ns_NewStreamListenerProxy(aConsumer, evQueue);
    if (nsnull == consumer) {
        return NS_FALSE;
    }
#else
    consumer = aConsumer;
#endif

    /* Create the nsConnectionInfo object... */
    pConn = new nsConnectionInfo(aUrl, NULL, consumer);
    if (NULL == pConn) {
        return NS_FALSE;
    }
    NS_ADDREF(pConn);

    /* 
     * XXX: Rewrite the resource: URL into a file: URL
     */
    const char* protocol;
    result = aUrl->GetProtocol(&protocol);
    NS_ASSERTION(result == NS_OK, "deal with this");

    // Deal with chrome URLS
    if ((PL_strcmp(protocol, "chrome") == 0) &&
        gChromeRegistry != nsnull) {        
	  
      if (NS_FAILED(result = gChromeRegistry->ConvertChromeURL(aUrl))) {
          NS_ERROR("Unable to convert chrome URL.");
          return result;
      }
      
      result = aUrl->GetProtocol(&protocol);
      NS_ASSERTION(result == NS_OK, "deal with this");
	}

    if (PL_strcmp(protocol, "resource") == 0) {
      char* fileName;
      const char* file;
      result = aUrl->GetFile(&file);
      NS_ASSERTION(result == NS_OK, "deal with this");
      fileName = mangleResourceIntoFileURL(file);
      aUrl->SetSpec(fileName);
      PR_Free(fileName);
    } 
	
    /* Create the URLStruct... */

    const char* spec = NULL;
    result = aUrl->GetSpec(&spec);
    NS_ASSERTION(result == NS_OK, "deal with this");
    URL_s = NET_CreateURLStruct(spec, NET_NORMAL_RELOAD);
    if (NULL == URL_s) {
        NS_RELEASE(pConn);
        return NS_FALSE;
    }

    SetupURLStruct(aUrl, URL_s);

    /*
     * Attach the Data Consumer to the URL_Struct.
     *
     * Both the Data Consumer and the URL_Struct are freed in the 
     * bam_exit_routine(...)
     * 
     * The Reference count on pConn is already 1 so no AddRef() is necessary.
     */
    URL_s->fe_data = pConn;
    
    /*
     * Attach the Event Queue to use when proxying data back to the thread
     * which initiated the URL load...
     *
     * Currently, this information is needed to marshall the call to the URL
     * exit_routine(...) on the correct thread...
     */
    URL_s->owner_data = evQueue;

    /*
     * Give the protocol a chance to initialize any URL_Struct fields...
     *
     * XXX:  Currently the return value form InitializeURLInfo(...) is 
     *       ignored...  Should the connection abort if it fails?
     */
    result = aUrl->QueryInterface(kINetlibURLIID, (void**)&netlibURL);
    if (NS_OK == result) {
        netlibURL->SetURLInfo(URL_s);
        NS_RELEASE(netlibURL);
    }
    
    /* Create a new Context and set its reference count to one.. */
    MWContext *stubContext = new_stub_context(URL_s);
    net_AddrefContext(stubContext);

    /* Check for timebomb...*/
#ifdef TIMEBOMB_ON
    NET_CheckForTimeBomb(stubContext);
#endif /* TIMEBOMB_ON */

    /* Start the URL load... */
    NET_GetURL (URL_s,                      /* URL_Struct      */
                FO_CACHE_AND_NGLAYOUT,      /* FO_Present_type */
                (MWContext *)stubContext,   /* MWContext       */
                bam_exit_routine);          /* Exit routine... */

    /* Remember, the URL_s may have been freed ! */

    /* 
     * Start the network timer to call NET_PollSockets(...) until the
     * URL has been completely loaded...
     */
    SchedulePollingTimer();
    return NS_OK;
}


nsresult nsNetlibService::OpenBlockingStream(nsIURL *aUrl, 
                                             nsIStreamListener *aConsumer,
                                             nsIInputStream **aNewStream)
{
    URL_Struct *URL_s;
    nsConnectionInfo *pConn;
    nsNetlibStream *pBlockingStream;
    nsINetlibURL *netlibURL;
    nsIStreamListener* consumer = nsnull;
    PLEventQueue* evQueue = nsnull;
    nsresult result;

    if (NULL == aNewStream) {
        return NS_FALSE;
    }

    if (NULL != aUrl) {
#if defined(NETLIB_THREAD)
        if (nsnull != mEventQService) {
          mEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &evQueue);
        }
        if (nsnull == evQueue) {
          goto loser;
        }

        if (nsnull != aConsumer) {
          consumer = ns_NewStreamListenerProxy(aConsumer, evQueue);
            if (nsnull == consumer) {
                goto loser;
            }
        }
#else
        consumer = aConsumer;
#endif

        /* Create the blocking stream... */
        pBlockingStream = new nsBlockingStream();

        if (NULL == pBlockingStream) {
            goto loser;
        }
        /*
         * AddRef the new stream in anticipation of returning it... This will
         * keep it alive :-)
         */
        NS_ADDREF(pBlockingStream);

        /* Create the nsConnectionInfo object... */
        pConn = new nsConnectionInfo(aUrl, pBlockingStream, consumer);
        if (NULL == pConn) {
            NS_RELEASE(pBlockingStream);
            goto loser;
        }
        NS_ADDREF(pConn);

        /* 
         * XXX: Rewrite the resource: URL into a file: URL
         */
        const char* protocol;
        result = aUrl->GetProtocol(&protocol);
        NS_ASSERTION(result == NS_OK, "deal with this");

        // Deal with chrome URLS
        if ((PL_strcmp(protocol, "chrome") == 0) &&
            gChromeRegistry != nsnull) {        
	  
          if (NS_FAILED(result = gChromeRegistry->ConvertChromeURL(aUrl))) {
              NS_ERROR("Unable to convert chrome URL.");
              return result;
          }
      
          result = aUrl->GetProtocol(&protocol);
          NS_ASSERTION(result == NS_OK, "deal with this");
	    }

        if (PL_strcmp(protocol, "resource") == 0) {
            char* fileName;
            const char* file;
            result = aUrl->GetFile(&file);
            NS_ASSERTION(result == NS_OK, "deal with this");
            fileName = mangleResourceIntoFileURL(file);
            aUrl->SetSpec(fileName);
            PR_Free(fileName);
        } 

        /* Create the URLStruct... */

        const char* spec = NULL;
        result = aUrl->GetSpec(&spec);
        NS_ASSERTION(result == NS_OK, "deal with this");
        URL_s = NET_CreateURLStruct(spec, NET_NORMAL_RELOAD);
        if (NULL == URL_s) {
            NS_RELEASE(pBlockingStream);
            NS_RELEASE(pConn);
            goto loser;
        }

        SetupURLStruct(aUrl, URL_s);

#if defined(XP_WIN) && !defined(NETLIB_THREAD)
        /*
         * When opening a blocking HTTP stream, perform a synchronous DNS 
         * lookup now, to avoid netlib from doing an async lookup later
         * and causing a deadlock!
         */
        result = PerformNastyWindowsAsyncDNSHack(URL_s, aUrl);
        if (NS_OK != result) {
            NET_FreeURLStruct(URL_s);
            NS_RELEASE(pBlockingStream);
            NS_RELEASE(pConn);
            goto loser;
        }
#endif /* XP_WIN && !NETLIB_THREAD */

        /*
         * Attach the ConnectionInfo object to the URL_Struct.
         *
         * Both the ConnectionInfo and the URL_Struct are freed in the 
         * bam_exit_routine(...)
         * The Reference count on pConn is already 1 so no AddRef() is 
         * necessary.
         */
        URL_s->fe_data = pConn;

        /*
         * Attach the Event Queue to use when proxying data back to the thread
         * which initiated the URL load...
         *
         * Currently, this information is needed to marshall the call to the URL
         * exit_routine(...) on the correct thread...
         */
        URL_s->owner_data = evQueue;

        /*
         * Give the protocol a chance to initialize any URL_Struct fields...
         *
         * XXX:  Currently the return value form InitializeURLInfo(...) is 
         *       ignored...  Should the connection abort if it fails?
         */
         result = aUrl->QueryInterface(kINetlibURLIID, (void**)&netlibURL);
         if (NS_OK == result) {
             netlibURL->SetURLInfo(URL_s);
             NS_RELEASE(netlibURL);
         }

/*        printf("+++ Loading %s\n", aUrl); */

      /* Create a new Context and set its reference count to one.. */
        MWContext *stubContext = new_stub_context(URL_s);
        net_AddrefContext(stubContext);

        /* Check for timebomb...*/
#ifdef TIMEBOMB_ON
        NET_CheckForTimeBomb(stubContext);
#endif /* TIMEBOMB_ON */

        /* Start the URL load... */
        if (0 > NET_GetURL (URL_s,                      /* URL_Struct      */
                    FO_CACHE_AND_NGLAYOUT,      /* FO_Present_type */
                    (MWContext *)  stubContext, /* MWContext       */
                    bam_exit_routine))          /* Exit routine... */
        {
            //Not releasing anything since bam_exit_routine will still be called.
            goto loser;
        }
        /* Remember, the URL_s may have been freed ! */

        /* 
         * Start the network timer to call NET_PollSockets(...) until the
         * URL has been completely loaded...
         */
        SchedulePollingTimer();

        *aNewStream = pBlockingStream;
        return NS_OK;
    }

loser:
    *aNewStream = NULL;
    return NS_FALSE;
}

static NS_DEFINE_IID(kIProtocolConnectionIID, NS_IPROTOCOLCONNECTION_IID);
static NS_DEFINE_IID(kINetLibUrlIID, NS_INETLIBURL_IID);

NS_IMETHODIMP
nsNetlibService::InterruptStream(nsIURL* aURL)
{
  nsINetlibURL * pNetUrl = nsnull;

  nsresult rv = NS_OK;

  if (nsnull == aURL) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  rv = aURL->QueryInterface(kINetLibUrlIID, (void**)&pNetUrl);
   if (NS_SUCCEEDED(rv)) {
     URL_Struct* URL_s;
     int status = -1;

     pNetUrl->GetURLInfo(&URL_s);
     if (nsnull != URL_s) {
       status = NET_InterruptStream(URL_s);
     }
     NS_RELEASE(pNetUrl);

     if (status != 0) {
       rv = NS_ERROR_FAILURE;
     }
   }

done:
  return rv;
}


NS_IMETHODIMP
nsNetlibService::GetCookieString(nsIURL *aURL, nsString& aCookie)
{
    // XXX How safe is it to create a stub context without a URL_Struct?
    MWContext *stubContext = new_stub_context(nsnull);
    
    const char *spec = NULL;
    nsresult result = aURL->GetSpec(&spec);
    NS_ASSERTION(result == NS_OK, "deal with this");
    char *cookie = NET_GetCookie(stubContext, (char *)spec);

    if (nsnull != cookie) {
        aCookie.SetString(cookie);
        PR_FREEIF(cookie);
    }
    else {
        aCookie.SetString("");
    }

    free_stub_context(stubContext);
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SetCookieString(nsIURL *aURL, const nsString& aCookie)
{
    // XXX How safe is it to create a stub context without a URL_Struct?
    MWContext *stubContext = new_stub_context(nsnull);
    
    const char *spec = NULL;
    nsresult result = aURL->GetSpec(&spec);
    NS_ASSERTION(result == NS_OK, "deal with this");
    char *cookie = aCookie.ToNewCString();

    NET_SetCookieString(stubContext, (char *)spec, cookie);

    delete []cookie;
    free_stub_context(stubContext);
    return NS_OK;
}

#ifdef SingleSignon
NS_IMETHODIMP
nsNetlibService::SI_DisplaySignonInfoAsHTML(){
    ::SI_DisplaySignonInfoAsHTML(NULL);
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SI_RememberSignonData
       (char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt) {
   ::SI_RememberSignonData(URLName, name_array, value_array, type_array, value_cnt);
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SI_RestoreSignonData
        (char* URLName, char* name, char** value) {
    ::SI_RestoreSignonData(URLName, name, value);
    return NS_OK;
}

#endif

#ifdef CookieManagement
NS_IMETHODIMP
nsNetlibService::NET_DisplayCookieInfoAsHTML(){
    ::NET_DisplayCookieInfoAsHTML(NULL);
    return NS_OK;
}

#ifdef PrivacySiteInfo
NS_IMETHODIMP
nsNetlibService::NET_DisplayCookieInfoOfSiteAsHTML(char * URLName){
    ::NET_DisplayCookieInfoOfSiteAsHTML(NULL, URLName);
    return NS_OK;
}
NS_IMETHODIMP
nsNetlibService::NET_CookiePermission(char* URLName, PRInt32* permission){
    *permission = ::NET_CookiePermission(URLName);
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::NET_CookieCount(char* URLName, PRInt32* count){
    *count = ::NET_CookieCount(URLName);
    return NS_OK;
}

#endif
#endif

NS_IMETHODIMP
nsNetlibService::NET_AnonymizeCookies(){
#ifdef CookieManagement
    ::NET_AnonymizeCookies();
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::NET_UnanonymizeCookies(){
#ifdef CookieManagement
    ::NET_UnanonymizeCookies();
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SI_AnonymizeSignons(){
#ifdef SingleSignon
    ::SI_AnonymizeSignons();
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SI_UnanonymizeSignons(){
#ifdef SingleSignon
    ::SI_UnanonymizeSignons();
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::GetProxyHTTP(nsString& aProxyHTTP) {
    char *proxy = nsnull;
    PRInt32 port;
    char outBuf[MAXHOSTNAMELEN + 8];
    *outBuf = '\0';

    if ( PREF_OK != PREF_CopyCharPref(pref_proxyHttpServer,&proxy) ) {
        return NS_FALSE;
    }

    if ( PREF_OK != PREF_GetIntPref(pref_proxyHttpPort,&port) ) {
        PR_FREEIF(proxy);
        return NS_FALSE;
    }

    sprintf(outBuf,"%s:%d", proxy, port);
    PR_FREEIF(proxy);
    aProxyHTTP.SetString(outBuf);

    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SetProxyHTTP(nsString& aProxyHTTP) {
    nsresult rv = NS_OK;
    PRInt32 port;
    nsString nsSPort;
    char *csPort = nsnull;
    char *proxy = nsnull;
    nsString nsSProxy;
    PRInt32 colonIdx;
    PRUnichar colon = ':';

    if (aProxyHTTP.Length() < 1) {
        NET_SelectProxyStyle(PROXY_STYLE_NONE);
        return NS_OK;
    }

    if ( (colonIdx = aProxyHTTP.Find(colon)) < 0 )
        return NS_FALSE;

    aProxyHTTP.Left(nsSProxy, colonIdx);
    aProxyHTTP.Mid(nsSPort, colonIdx+1, aProxyHTTP.Length() - colonIdx);

    proxy = nsSProxy.ToNewCString();
    if (!proxy)
        return NS_FALSE;
    csPort = nsSPort.ToNewCString();
    if (!csPort) {
        delete[] proxy;
        return NS_FALSE;
    }

    port = atoi(csPort);

    if ( PREF_OK != PREF_SetCharPref(pref_proxyHttpServer, proxy) ) {
        rv = NS_FALSE;
    }
    if ( PREF_OK != PREF_SetIntPref(pref_proxyHttpPort, port) ) {
        rv = NS_FALSE;
    }
    delete[] proxy;
    delete[] csPort;

    NET_SelectProxyStyle(PROXY_STYLE_MANUAL);

    return rv;
}

NS_IMETHODIMP
nsNetlibService::GetHTTPOneOne(PRBool& aOneOne) {
    aOneOne = (ONE_POINT_ONE == DEFAULT_VERSION);
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SetHTTPOneOne(PRBool aSendOneOne) {
    if (aSendOneOne)
        DEFAULT_VERSION = ONE_POINT_ONE;
    else
        DEFAULT_VERSION = ONE_POINT_O;
    return NS_OK;
}

NS_IMETHODIMP    
nsNetlibService::GetAppCodeName(nsString& aAppCodeName)
{
  aAppCodeName.SetString(XP_AppCodeName);
  return NS_OK;
}
 
NS_IMETHODIMP
nsNetlibService::GetAppVersion(nsString& aAppVersion)
{
  aAppVersion.SetString(XP_AppVersion);
  return NS_OK;
}
 
NS_IMETHODIMP
nsNetlibService::GetAppName(nsString& aAppName)
{
  aAppName.SetString(XP_AppName);
  return NS_OK;
}
 
NS_IMETHODIMP
nsNetlibService::GetLanguage(nsString& aLanguage)
{
  aLanguage.SetString(XP_AppLanguage);
  return NS_OK;
}
 
NS_IMETHODIMP    
nsNetlibService::GetPlatform(nsString& aPlatform)
{
  aPlatform.SetString(XP_AppPlatform);
  return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::GetUserAgent(nsString& aUA)
{
    char buf[64];
    PR_snprintf(buf, 64, "%.100s/%.90s", XP_AppCodeName, XP_AppVersion);
    aUA.SetString(buf);
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::SetCustomUserAgent(nsString aCustom)
{
    PRInt32 inIdx;
#ifdef emacs_knew_how_to_pattern_match_better_oh_well
    PRUnichar junkChar = '[';
#endif
    PRUnichar inChar = ']';

    if (!XP_AppVersion || (0 >= aCustom.Length()) )
        return NS_FALSE;

    nsString newAppVersion = XP_AppVersion;

    inIdx = newAppVersion.Find(inChar);
    if (0 > inIdx) {
        newAppVersion.Insert(aCustom, inIdx + 1);
    }

    PR_Free((char *)XP_AppVersion);
    XP_AppVersion = newAppVersion.ToNewCString();
    return NS_OK;

}

////////////////////////////////////////////////////////////////////////////////

class nsStringHashKey : public nsHashKey 
{
public:
    nsStringHashKey(const nsString& aName)
        : mName(aName)
    {
    }

    virtual ~nsStringHashKey(void)
    {
    }

    virtual PRUint32 HashValue(void) const;
    virtual PRBool Equals(const nsHashKey *aKey) const;
    virtual nsHashKey *Clone(void) const;

protected:
    const nsString& mName;
};

PRUint32 nsStringHashKey::HashValue(void) const
{
    return nsCRT::HashValue(mName);
}

PRBool nsStringHashKey::Equals(const nsHashKey *aKey) const
{
    const nsStringHashKey* other = (const nsStringHashKey*)aKey;
    return mName.EqualsIgnoreCase(other->mName);
}

nsHashKey *nsStringHashKey::Clone(void) const
{
    return new nsStringHashKey(mName);
}

struct nsProtocolPair {
    nsIProtocolURLFactory*      mProtocolURLFactory;
    nsIProtocol*                mProtocol;
};

NS_IMETHODIMP 
nsNetlibService::RegisterProtocol(const nsString& aName, 
                                  nsIProtocolURLFactory* aProtocolURLFactory,
                                  nsIProtocol* aProtocol)
{
    nsStringHashKey* key = new nsStringHashKey(aName);
    if (key == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(aProtocolURLFactory);
    NS_IF_ADDREF(aProtocol);    // XXX remove IF
    nsProtocolPair* pair = new nsProtocolPair;
    if (pair == NULL) {
        delete key;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    pair->mProtocolURLFactory = aProtocolURLFactory;
    pair->mProtocol = aProtocol;
    mProtocols->Put(key, pair);
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::UnregisterProtocol(const nsString& aName)
{
    nsStringHashKey key(aName);
    nsProtocolPair* pair = (nsProtocolPair*)mProtocols->Remove(&key);
    NS_RELEASE(pair->mProtocolURLFactory);
    NS_IF_RELEASE(pair->mProtocol);     // XXX remove IF
    delete pair;
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::GetProtocol(const nsString& aName,
                             nsIProtocolURLFactory* *aProtocolURLFactory,
                             nsIProtocol* *aProtocol)
{
    nsStringHashKey key(aName);
    nsProtocolPair* pair = (nsProtocolPair*)mProtocols->Get(&key);
    if (pair == NULL)
        return NS_ERROR_FAILURE;
    if (aProtocolURLFactory)
        *aProtocolURLFactory = pair->mProtocolURLFactory;
    if (aProtocol)
        *aProtocol = pair->mProtocol;
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::CreateURL(nsIURL* *aURL, 
                           const nsString& aSpec,
                           const nsIURL* aContextURL,
                           nsISupports* aContainer,
                           nsIURLGroup* aGroup)
{
    nsAutoString protoName;
    PRInt32 pos = aSpec.Find(':');
    if (pos >= 0) {
        aSpec.Left(protoName, pos);
    }
    else if (aContextURL) {
        const char* str;
        aContextURL->GetProtocol(&str);
        protoName = str;
    }
    else {
        protoName = "http";
    }
    nsIProtocolURLFactory* protocolURLFactory;
    nsresult err = GetProtocol(protoName, &protocolURLFactory, NULL);
    if (err != NS_OK) return err;
    
    return protocolURLFactory->CreateURL(aURL, aSpec, aContextURL, aContainer, aGroup);
}

NS_IMETHODIMP nsNetlibService::CreateSocketTransport(nsITransport **aTransport, PRUint32 aPortToUse, const char * aHostName)
{
	nsSocketTransport *pNSSocketTransport = NULL;

	NS_DEFINE_IID(kITransportIID, NS_ITRANSPORT_IID);

	pNSSocketTransport = new nsSocketTransport(aPortToUse, aHostName);
	if (pNSSocketTransport->QueryInterface(kITransportIID, (void**)aTransport) != NS_OK) {
    // then we're trying get a interface other than nsISupports and
    // nsITransport
    return NS_ERROR_FAILURE;
	}
 
	return NS_OK;
}

NS_IMETHODIMP nsNetlibService::CreateFileSocketTransport(nsITransport **aTransport, const char * aFileName)
{
	NS_PRECONDITION(aFileName, "You need to specify the file name of the file you wish to create a socket for");

	nsSocketTransport *pNSSocketTransport = NULL;

	NS_DEFINE_IID(kITransportIID, NS_ITRANSPORT_IID);

	pNSSocketTransport = new nsSocketTransport(aFileName);
	if (pNSSocketTransport->QueryInterface(kITransportIID, (void**)aTransport) != NS_OK) {
    // then we're trying get a interface other than nsISupports and
    // nsITransport
    return NS_ERROR_FAILURE;
	}
 
	return NS_OK;
}


NS_IMETHODIMP
nsNetlibService::AreThereActiveConnections()
{
    if (NET_AreThereActiveConnections() == PR_TRUE)
        return 1;
    else
        return 0;   
}


static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                          const nsString& aSpec,
                          const nsIURL* aURL,
                          nsISupports* aContainer,
                          nsIURLGroup* aGroup)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
        return NS_ERROR_NULL_POINTER;
    }

    nsINetService *inet = nsnull;
    nsresult rv = nsServiceManager::GetService(kNetServiceCID,
                                               kINetServiceIID,
                                               (nsISupports **)&inet);
    if (rv != NS_OK) return rv;

    rv = inet->CreateURL(aInstancePtrResult, aSpec, aURL, aContainer, aGroup);

    nsServiceManager::ReleaseService(kNetServiceCID, inet);
    return rv;
}

NS_NET nsresult NS_MakeAbsoluteURL(nsIURL* aURL,
                                   const nsString& aBaseURL,
                                   const nsString& aSpec,
                                   nsString& aResult)
{
    nsIURL* base = nsnull;
    if (0 < aBaseURL.Length()) {
        nsresult err = NS_NewURL(&base, aBaseURL);
        if (err != NS_OK) return err;
    }
    else {
        const char* str;
        aURL->GetSpec(&str);
        nsresult err = NS_NewURL(&base, str);
        if (err != NS_OK) return err;
    }
    nsIURL* url = nsnull;
    nsresult err = NS_NewURL(&url, aSpec, base);
    if (err != NS_OK) goto done;

    PRUnichar* str;
    err = url->ToString(&str);
    if (err) goto done;
    aResult = str;
    delete []str;

  done:
    NS_IF_RELEASE(url);
    NS_IF_RELEASE(base);
    return err;
}

NS_NET nsresult NS_OpenURL(nsIURL* aURL, nsIStreamListener* aConsumer)
{
  nsresult rv;
  nsIURLGroup* group = nsnull;

  rv = aURL->GetURLGroup(&group);
  if (NS_SUCCEEDED(rv)) {
    if (nsnull != group) {
      rv = group->OpenStream(aURL, aConsumer);
      NS_RELEASE(group);
    } else {
      nsINetService *inet = nsnull;

      rv = nsServiceManager::GetService(kNetServiceCID,
                                        kINetServiceIID,
                                        (nsISupports **)&inet);
      if (NS_SUCCEEDED(rv)) {
        rv = inet->OpenStream(aURL, aConsumer);
        nsServiceManager::ReleaseService(kNetServiceCID, inet);
      }
    }
  }
  return rv;
}

NS_NET nsresult NS_OpenURL(nsIURL* aURL, nsIInputStream* *aNewStream,
                           nsIStreamListener* aConsumer)
{
    nsINetService *inet = nsnull;
    nsresult rv = nsServiceManager::GetService(kNetServiceCID,
                                               kINetServiceIID,
                                               (nsISupports **)&inet);
    if (rv != NS_OK) return rv;

    rv = inet->OpenBlockingStream(aURL, aConsumer, aNewStream);

    nsServiceManager::ReleaseService(kNetServiceCID, inet);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

void nsNetlibService::SchedulePollingTimer()
{
#if !defined(NETLIB_THREAD)
    // If a timer is already active, then do not create another...
    if (nsnull == mPollingTimer) {
        if (NS_OK == NS_NewTimer(&mPollingTimer)) {
            mPollingTimer->Init(nsNetlibService::NetPollSocketsCallback, this, 1000 / 50);
        }
    }
#endif /* ! NETLIB_THREAD */
}


void nsNetlibService::CleanupPollingTimer(nsITimer* aTimer) 
{
#if !defined(NETLIB_THREAD)
    NS_PRECONDITION((aTimer == mPollingTimer), "Unknown Timer...");

    NS_RELEASE(mPollingTimer);
#endif /* ! NETLIB_THREAD */
}


#if !defined(NETLIB_THREAD)
void nsNetlibService::NetPollSocketsCallback(nsITimer* aTimer, void* aClosure)
{
    nsNetlibService* inet = (nsNetlibService*)aClosure;

    NS_PRECONDITION((nsnull != inet), "Null pointer");
    if (nsnull != inet) {
        (void) NET_PollSockets();

        inet->CleanupPollingTimer(aTimer);
        // Keep scheduling callbacks as long as there are URLs to process...
///        if (0 < NET_TotalNumberOfProcessingURLs) {
            inet->SchedulePollingTimer();
///        }
    }
}

#if defined(XP_WIN)
/*
 * This routine is used to avoid a (hopefully temporary) problem which netlib
 * has when processing blocking HTTP streams.  
 * 
 * When ASYNC_DNS is enabled, all DNS lookups go through the main message pump,
 * unfortunately, this is not possible when processing a blocking stream, so 
 * we deadlock :-(
 *
 * To avoid this deadlock, we synchronously resolve the hostname and store it
 * in the little known IPAddressString field of the URL_Struct.  This prevents
 * netlib from doing an Async DNS lookup later...
 */
nsresult PerformNastyWindowsAsyncDNSHack(URL_Struct *URL_s, nsIURL* aURL)
{
    PRHostEnt hpbuf;
    char dbbuf[PR_NETDB_BUF_SIZE];
    PRStatus err;
    nsresult rv = NS_OK;

    /* Only attempt to resolve the hostname for HTTP URLs... */
    if (0 == PL_strcasecmp("http", aURL->GetProtocol())) {
        /* Perform a synchronous DNS lookup... */
        err = PR_GetHostByName(aURL->GetHost(), dbbuf, sizeof(dbbuf),  &hpbuf);
        if (PR_SUCCESS == err) {
            int a,b,c,d;
            unsigned char *pc;

            pc = (unsigned char *) hpbuf.h_addr_list[0];

            a = (int) *pc;
            b = (int) *(++pc);
            c = (int) *(++pc);
            d = (int) *(++pc);
            URL_s->IPAddressString = PR_smprintf("%d.%d.%d.%d", a,b,c,d);
        } else {
            /* 
             * If we fail to resolve a host on a HTTP connection, then 
             * abort the connection to prevent a deadlock...
             */
            rv = NS_ERROR_FAILURE;
        }
    }
    return rv;
}
#endif /* XP_WIN */
#endif /* !NETLIB_THREAD */


// This is global so it can be accessed by the NetFactory (nsNetFactory.cpp)
nsNetlibService *gNetlibService = nsnull;

//
// Class to manage static initialization of the Netlib DLL...
//
struct nsNetlibInit {
  nsNetlibInit() {
    gNetlibService = nsnull;
#if !defined(NETLIB_THREAD)
    /*
     * The netlib thread cannot be created within a static constructor
     * since a runtime library lock is being held by the mozilla thread
     * which results in a deadlock when the netlib thread is started...
     */
    (void) NS_InitINetService();
#endif /* !NETLIB_THREAD */
  }

  ~nsNetlibInit() {
#if !defined(NETLIB_THREAD)
    NS_ShutdownINetService();
#endif /* !NETLIB_THREAD */
    gNetlibService = nsnull;
  }
};

#if defined(XP_MAC) || defined(XP_UNIX)
static nsNetlibInit* netlibInit = nsnull;
#else
static nsNetlibInit netlibInit;
#endif

extern "C" {
/*
 * Factory for creating instance of the NetlibService...
 */
NS_NET nsresult NS_NewINetService(nsINetService** aInstancePtrResult,
                                  nsISupports* aOuter)
{
    if (nsnull != aOuter) {
        return NS_ERROR_NO_AGGREGATION;
    }

#if defined(XP_MAC) || defined(XP_UNIX)
    // Perform static initialization...
    if (nsnull == netlibInit) {
        netlibInit = new nsNetlibInit;
    }
#endif /* XP_MAC */ // XXX on the mac this never gets shutdown

    // The Netlib Service is created by the nsNetlibInit class...
    if (nsnull == gNetlibService) {
        (void) NS_InitINetService();
///     return NS_ERROR_OUT_OF_MEMORY;
    }

    return gNetlibService->QueryInterface(kINetServiceIID, (void**)aInstancePtrResult);
}

extern "C" NS_NET nsresult
NS_InitializeHttpURLFactory(nsINetService* inet);

NS_NET nsresult NS_InitINetService()
{
    /* XXX: For now only allow a single instance of the Netlib Service */
    if (nsnull == gNetlibService) {
        gNetlibService = new nsNetlibService();
        if (nsnull == gNetlibService) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        // XXX temporarily, until we have pluggable protocols...
        NS_InitializeHttpURLFactory(gNetlibService);
    }

    NS_ADDREF(gNetlibService);
    return NS_OK;
}


NS_NET nsresult NS_ShutdownINetService()
{
    return NS_OK;
}

} /* extern "C" */

/*
 * This is the generic exit routine for all URLs loaded via the new
 * BAM APIs...
 */
static void bam_exit_routine(URL_Struct *URL_s, int status, MWContext *window_id)
{
    TRACEMSG(("bam_exit_routine was called...\n"));

    if (NULL != URL_s) {
        nsConnectionInfo *pConn = (nsConnectionInfo *)URL_s->fe_data;

#ifdef NOISY
        printf("+++ Finished loading %s\n", URL_s->address);
#endif
        PR_ASSERT(pConn);

        /* Release the ConnectionInfo object held in the URL_Struct. */
        if (nsnull != pConn) {
            /* 
             * Normally, the stream is closed when the connection has been
             * completed.  However, if the URL exit proc was called directly
             * by NET_GetURL(...), then the stream may still be around...
             */
            if (nsnull != pConn->pNetStream) {
                pConn->pNetStream->Close();
                NS_RELEASE(pConn->pNetStream);
            }

            /* 
             * If the connection is still marked as active, then
             * notify the Data Consumer that the Binding has completed... 
             * Since the connection is still active, the stream was never
             * closed (or possibly created).  So, the binding has failed...
             */
            if ((nsConnectionActive == pConn->mStatus) && 
                (nsnull != pConn->pConsumer)) {
                nsAutoString status;
                pConn->pConsumer->OnStopBinding(pConn->pURL, NS_BINDING_FAILED, status);
                NS_RELEASE(pConn->pConsumer);
            }

            /* Release the nsConnectionInfo object hanging off of the fe_data */
            URL_s->fe_data = NULL;
            NS_RELEASE(pConn);
        }

        /* Delete the URL_Struct... */
        NET_FreeURLStruct(URL_s);
    }
}

/*
 * Ugly hack to free contexts
 */

extern "C" void net_AddrefContext(MWContext *context)
{
  if (nsnull != context) {
    context->ref_count++;
  }
}

extern "C" void net_ReleaseContext(MWContext *context)
{
 if (context) {
   NS_PRECONDITION((0 < context->ref_count), "Context reference count is bad.");
   if ((0 == --context->ref_count) && (context->modular_data)) {
     free_stub_context(context);
   } else {
     TRACEMSG(("net_ReleaseContext: not releasing non-modular context"));
   }
 }
}


/*
 * Rewrite "resource://" URLs into file: URLs with the path of the 
 * executable prepended to the file path...
 */
char *mangleResourceIntoFileURL(const char* aResourceFileName) 
{
  // XXX For now, resources are not in jar files 
  // Find base path name to the resource file
  char* resourceBase;

#ifdef XP_PC
  // XXX For now, all resources are relative to the .exe file
  resourceBase = (char *)PR_Malloc(_MAX_PATH);;
  DWORD mfnLen = GetModuleFileName(g_hInst, resourceBase, _MAX_PATH);
  // Truncate the executable name from the rest of the path...
  char* cp = strrchr(resourceBase, '\\');
  if (nsnull != cp) {
    *cp = '\0';
  }
  // Change the first ':' into a '|'
  cp = PL_strchr(resourceBase, ':');
  if (nsnull != cp) {
      *cp = '|';
  }
#endif /* XP_PC */

#ifdef XP_UNIX

//
// Obtain the resource: url base from the environment variable
//
// MOZILLA_FIVE_HOME
//
// Which is the standard place where mozilla stores global (ie, not
// user specific) data
//

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024 // A good guess, i suppose
#endif

#define MOZILLA_FIVE_HOME "MOZILLA_FIVE_HOME"

    static char * nsUnixMozillaHomePath = nsnull;

    if (nsnull == nsUnixMozillaHomePath)
    {
      nsUnixMozillaHomePath = PR_GetEnv(MOZILLA_FIVE_HOME);
    }
    if (nsnull == nsUnixMozillaHomePath)
    {
      static char homepath[MAXPATHLEN];
      FILE* pp;
      if (!(pp = popen("pwd", "r"))) {
#ifdef DEBUG
        printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 1\n");
#endif
        return(nsnull);
      }
      if (fgets(homepath, MAXPATHLEN, pp)) {
        homepath[PL_strlen(homepath)-1] = 0;
      }
      else {
#ifdef DEBUG
        printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 2\n");
#endif
        pclose(pp);
        return(nsnull);
      }
      pclose(pp);
      nsUnixMozillaHomePath = homepath;
    }

	resourceBase = XP_STRDUP(nsUnixMozillaHomePath);
#ifdef DEBUG
    {
        static PRBool firstTime = PR_TRUE;
        if (firstTime) {
            firstTime = PR_FALSE;
            printf("Using '%s' as the resource: base\n", resourceBase);
        }
    }
#endif

#endif /* XP_UNIX */

#ifdef XP_MAC
	resourceBase = XP_STRDUP("usr/local/netscape/bin");
#endif /* XP_MAC */

  // Join base path to resource name
  if (aResourceFileName[0] == '/') {
    aResourceFileName++;
  }
  PRInt32 baseLen = PL_strlen(resourceBase);
  PRInt32 resLen = PL_strlen(aResourceFileName);
  PRInt32 totalLen = 8 + baseLen + 1 + resLen + 1;
  char* fileName = (char *)PR_Malloc(totalLen);
  PR_snprintf(fileName, totalLen, "file:///%s/%s", resourceBase, aResourceFileName);

#ifdef XP_PC
  // Change any backslashes into foreward slashes...
  while ((cp = PL_strchr(fileName, '\\')) != 0) {
    *cp = '/';
    cp++;
  }
#endif /* XP_PC */

  PR_Free(resourceBase);

  return fileName;
}




#ifdef XP_PC

BOOL WINAPI DllMain(HINSTANCE hDllInst,
                    DWORD fdwReason,
                    LPVOID lpvReserved)
{
    BOOL bResult = TRUE;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
          {
            // save our instance
            g_hInst = hDllInst;
          }
          break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
  }

  return (bResult);
}


#endif /* XP_PC */
