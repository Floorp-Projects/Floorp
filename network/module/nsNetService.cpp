/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsRepository.h"
#include "nsITimer.h"
#include "nsNetService.h"
#include "nsNetStream.h"
#include "nsNetFile.h"
extern "C" {
#include "mkutils.h"
#include "mkgeturl.h"
#include "mktrace.h"
#include "mkstream.h"
#include "cvchunk.h"
}; /* end of extern "C" */

#include "netcache.h"
#include "cookies.h"
#include "plstr.h"

#include "nsString.h"
#include "nsIProtocolConnection.h"
#include "nsINetContainerApplication.h"

#ifdef XP_PC
#include <windows.h>
static HINSTANCE g_hInst = NULL;
#endif


// Declare the nsFile struct here so it's state is initialized before
// we initialize netlib.
#ifdef NS_NET_FILE

#include "nsNetFile.h"
#ifdef XP_MAC
static nsNetFileInit* netFileInit = nsnull;
#else
static nsNetFileInit netFileInit;
#endif

#endif // NS_NET_FILE
// End nsFile specific

/* XXX: Legacy definitions... */
// Global count of active urls from mkgeturl.c
extern "C" int NET_TotalNumberOfProcessingURLs;

MWContext *new_stub_context(URL_Struct *URL_s);
void free_stub_context(MWContext *window_id);
static void bam_exit_routine(URL_Struct *URL_s, int status, MWContext *window_id);

#if defined(XP_WIN) && !defined(NETLIB_THREAD)
nsresult PerformNastyWindowsAsyncDNSHack(URL_Struct* URL_s, nsIURL* aURL);
#endif /* XP_WIN && !NETLIB_THREAD */

char *mangleResourceIntoFileURL(const char* aResourceFileName) ;
extern nsIStreamListener* ns_NewStreamListenerProxy(nsIStreamListener* aListener);

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

}; /* end of extern "C" */

static NS_DEFINE_IID(kIProtocolConnectionIID,  NS_IPROTOCOLCONNECTION_IID);


nsNetlibService::nsNetlibService(nsINetContainerApplication *aContainerApp)
{
    NS_INIT_REFCNT();

    /*
      m_stubContext = new_stub_context();
    */
    mPollingTimer = nsnull;
    mContainer    = nsnull;
    
    mNetlibThread = new nsNetlibThread();
    if (nsnull != mNetlibThread) {
        mNetlibThread->Start();
    }

    if (NULL != aContainerApp) {
        XP_AppCodeName = NULL;
        XP_AppVersion  = NULL;
        XP_AppName     = NULL;
        XP_AppLanguage = NULL;
        XP_AppPlatform = NULL;

        SetContainerApplication(aContainerApp);
    }
    else {
        // XXX: Where should the defaults really come from
        XP_AppCodeName = PL_strdup("Mozilla");
        XP_AppVersion  = PL_strdup("5.0 Netscape/5.0 (Windows;I;x86;en)");
        XP_AppName     = PL_strdup("Netscape");

        /* 
         * XXX: Some of these should come from resources and/or 
         * platform-specific code. 
         */
        XP_AppLanguage = PL_strdup("en");
#ifdef XP_WIN
        XP_AppPlatform = PL_strdup("Win32");
#elif defined(XP_MAC)
        XP_AppPlatform = PL_strdup("MacPPC");
#elif defined(XP_UNIX)
        /* XXX: Need to differentiate between various Unisys */
        XP_AppPlatform = PL_strdup("Unix");
#endif
    }
}


NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
NS_IMPL_THREADSAFE_ISUPPORTS(nsNetlibService,kINetServiceIID);


nsNetlibService::~nsNetlibService()
{
    TRACEMSG(("nsNetlibService is being destroyed...\n"));

    /*
      if (NULL != m_stubContext) {
        free_stub_context((MWContext *)m_stubContext);
        m_stubContext = NULL;
      }
    */

    NS_IF_RELEASE(mPollingTimer);
    NS_IF_RELEASE(mContainer);

    if (nsnull != mNetlibThread) {
        mNetlibThread->Stop();
        delete mNetlibThread;
    }
}



void nsNetlibService::SetupURLStruct(nsIURL *aUrl, URL_Struct *aURL_s) {
    nsresult result;
    NET_ReloadMethod reloadType;
    nsILoadAttribs* loadAttribs = aUrl->GetLoadAttribs();
    PRInt32 type = aUrl->GetReloadType();

    /* Set the NET_ReloadMethod to correspond with what we've 
     * been asked to do.
     *
     * 0 = nsReload (normal)
     * 1 = nsReloadBypassCache
     * 2 = nsReloadBypassProxy
     * 3 = nsReloadBypassCacheAndProxy
     */
    if (type == 1 || type == 3) {
        reloadType = NET_SUPER_RELOAD;
    } else {
        reloadType = NET_NORMAL_RELOAD;
    }


    /* If this url has load attributes, setup the underlying url struct
     * accordingly. */
    if (loadAttribs) {
        PRUint32 localIP = 0;

        NS_VERIFY_THREADSAFE_INTERFACE(loadAttribs);
        if (type == 2 || type == 3) {
            result = loadAttribs->GetBypassProxy((int *)&(aURL_s->bypassProxy));
            if (result != NS_OK)
                aURL_s->bypassProxy = FALSE;
        }

        result = loadAttribs->GetLocalIP(&localIP);
        if (result != NS_OK)
            localIP = 0;
        aURL_s->localIP = localIP;
        NS_RELEASE(loadAttribs);
    }
}

nsresult nsNetlibService::OpenStream(nsIURL *aUrl, 
                                     nsIStreamListener *aConsumer)
{
    URL_Struct *URL_s;
    nsConnectionInfo *pConn;
    nsIProtocolConnection *pProtocol;
    nsresult result;
    NET_ReloadMethod reloadType;
    nsIStreamListener* consumer;

    if ((NULL == aConsumer) || (NULL == aUrl)) {
        return NS_FALSE;
    }

    consumer = ns_NewStreamListenerProxy(aConsumer);
    if (nsnull == consumer) {
        return NS_FALSE;
    }

    /* Create the nsConnectionInfo object... */
    pConn = new nsConnectionInfo(aUrl, NULL, consumer);
    if (NULL == pConn) {
        return NS_FALSE;
    }
    NS_ADDREF(pConn);

    /* We've got a nsConnectionInfo(), now hook it up
     * to the nsISupports of the nsIContentViewerContainer
     */
    pConn->pContainer = aUrl->GetContainer();
    NS_VERIFY_THREADSAFE_INTERFACE(pConn->pContainer);

    
    /* 
     * XXX: Rewrite the resource: URL into a file: URL
     */
    if (PL_strcmp(aUrl->GetProtocol(), "resource") == 0) {
      char* fileName;

      fileName = mangleResourceIntoFileURL(aUrl->GetFile());
      aUrl->Set(fileName);
      PR_Free(fileName);
    } 

    /* Create the URLStruct... */

    URL_s = NET_CreateURLStruct(aUrl->GetSpec(), reloadType);
    if (NULL == URL_s) {
        NS_RELEASE(pConn);
        return NS_FALSE;
    }

    SetupURLStruct(aUrl, URL_s);

    /* 
     * Mark the URL as background loading.  This prevents many
     * client upcall notifications...
     */
    URL_s->load_background = FALSE;

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
     * Give the protocol a chance to initialize any URL_Struct fields...
     *
     * XXX:  Currently the return value form InitializeURLInfo(...) is 
     *       ignored...  Should the connection abort if it fails?
     */
    result = aUrl->QueryInterface(kIProtocolConnectionIID, (void**)&pProtocol);
    if (NS_OK == result) {
        pProtocol->InitializeURLInfo(URL_s);
        NS_RELEASE(pProtocol);
    }
    
    MWContext *stubContext = new_stub_context(URL_s);
    /* Start the URL load... */
/*
    NET_GetURL (URL_s,                      // URL_Struct
                FO_CACHE_AND_NGLAYOUT,      // FO_Present_type
                (MWContext *)m_stubContext, // MWContext
                bam_exit_routine);          // Exit routine...
*/
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
    nsIProtocolConnection *pProtocol;
    nsIStreamListener* consumer = nsnull;
    nsresult result;

    if (NULL == aNewStream) {
        return NS_FALSE;
    }

    if (NULL != aUrl) {
        if (nsnull != aConsumer) {
            consumer = ns_NewStreamListenerProxy(aConsumer);
            if (nsnull == consumer) {
                goto loser;
            }
        }

        /* Create the blocking stream... */
        pBlockingStream = new nsBlockingStream();
        NET_ReloadMethod reloadType;

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

        /* We've got a nsConnectionInfo(), now hook it up
         * to the nsISupports of the nsIContentViewerContainer
         */
        pConn->pContainer = aUrl->GetContainer();
        NS_VERIFY_THREADSAFE_INTERFACE(pConn->pContainer);

        /* 
         * XXX: Rewrite the resource: URL into a file: URL
         */
        if (PL_strcmp(aUrl->GetProtocol(), "resource") == 0) {
            char* fileName;

            fileName = mangleResourceIntoFileURL(aUrl->GetFile());
            aUrl->Set(fileName);
            PR_Free(fileName);
        } 

        /* Create the URLStruct... */

        URL_s = NET_CreateURLStruct(aUrl->GetSpec(), reloadType);
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
         * Mark the URL as background loading.  This prevents many
         * client upcall notifications...
         */
        URL_s->load_background = FALSE;

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
         * Give the protocol a chance to initialize any URL_Struct fields...
         *
         * XXX:  Currently the return value form InitializeURLInfo(...) is 
         *       ignored...  Should the connection abort if it fails?
         */
         result = aUrl->QueryInterface(kIProtocolConnectionIID, (void**)&pProtocol);
         if (NS_OK == result) {
             pProtocol->InitializeURLInfo(URL_s);
             NS_RELEASE(pProtocol);
         }

/*        printf("+++ Loading %s\n", aUrl); */

        MWContext *stubContext = new_stub_context(URL_s);

        /* Start the URL load... */
/*
        NET_GetURL (URL_s,                      // URL_Struct
                    FO_CACHE_AND_NGLAYOUT,      // FO_Present_type
                    (MWContext *)m_stubContext, // MWContext
                    bam_exit_routine);          // Exit routine...
*/
        NET_GetURL (URL_s,                      /* URL_Struct      */
                    FO_CACHE_AND_NGLAYOUT,      /* FO_Present_type */
                    (MWContext *)  stubContext, /* MWContext       */
                    bam_exit_routine);          /* Exit routine... */

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

NS_IMETHODIMP
nsNetlibService::GetContainerApplication(nsINetContainerApplication **aContainer)
{
    NS_LOCK_INSTANCE();
    *aContainer = mContainer;
    NS_IF_ADDREF(*aContainer);
    NS_UNLOCK_INSTANCE();

    return NS_OK;
}

nsresult
nsNetlibService::SetContainerApplication(nsINetContainerApplication *aContainer)
{
    NS_LOCK_INSTANCE();

    NS_IF_RELEASE(mContainer);

    mContainer = aContainer;
    NS_IF_ADDREF(mContainer);
    NS_VERIFY_THREADSAFE_INTERFACE(mContainer);

    if (mContainer) {
        nsAutoString str;

        if (XP_AppCodeName) {
            PR_Free((void *)XP_AppCodeName);
        }
        mContainer->GetAppCodeName(str);
        XP_AppCodeName = str.ToNewCString();
        if (XP_AppVersion) {
            PR_Free((void *)XP_AppVersion);
        }
        mContainer->GetAppVersion(str);
        XP_AppVersion = str.ToNewCString();
        if (XP_AppName) {
            PR_Free((void *)XP_AppName);
        }
        mContainer->GetAppName(str);
        XP_AppName = str.ToNewCString();
        if (XP_AppPlatform) {
            PR_Free((void *)XP_AppPlatform);
        }
        mContainer->GetPlatform(str);
        XP_AppPlatform = str.ToNewCString();
        if (XP_AppLanguage) {
            PR_Free((void *)XP_AppLanguage);
        }
        mContainer->GetLanguage(str);
        XP_AppLanguage = str.ToNewCString();
    }
    
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP
nsNetlibService::GetCookieString(nsIURL *aURL, nsString& aCookie)
{
    // XXX How safe is it to create a stub context without a URL_Struct?
    MWContext *stubContext = new_stub_context(nsnull);
    
    const char *spec = aURL->GetSpec();
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
    
    const char *spec = aURL->GetSpec();
    char *cookie = aCookie.ToNewCString();

    NET_SetCookieString(stubContext, (char *)spec, cookie);

    delete []cookie;
    free_stub_context(stubContext);
    return NS_OK;
}

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
    (void) NS_InitINetService(nsnull);
#endif /* !NETLIB_THREAD */
  }

  ~nsNetlibInit() {
#if !defined(NETLIB_THREAD)
    NS_ShutdownINetService();
#endif /* !NETLIB_THREAD */
    gNetlibService = nsnull;
  }
};

#ifdef XP_MAC
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

#ifdef XP_MAC
    // Perform static initialization...
    if (nsnull == netlibInit) {
        netlibInit = new nsNetlibInit;
    }
#endif /* XP_MAC */ // XXX on the mac this never gets shutdown

    // The Netlib Service is created by the nsNetlibInit class...
    if (nsnull == gNetlibService) {
        (void) NS_InitINetService(nsnull);
///     return NS_ERROR_OUT_OF_MEMORY;
    }

    return gNetlibService->QueryInterface(kINetServiceIID, (void**)aInstancePtrResult);
}

NS_NET nsresult NS_InitINetService(nsINetContainerApplication *aContainer)
{
    /* XXX: For now only allow a single instance of the Netlib Service */
    if (nsnull == gNetlibService) {
        gNetlibService = new nsNetlibService(aContainer);
        if (nsnull == gNetlibService) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    else {
        gNetlibService->SetContainerApplication(aContainer);
    }

    NS_ADDREF(gNetlibService);
    return NS_OK;
}

NS_NET nsresult NS_ShutdownINetService()
{
    nsNetlibService *service = gNetlibService;

    // Release the container...
    if (nsnull != service) {
        gNetlibService->SetContainerApplication(nsnull);
        NS_RELEASE(service);
    }
    return NS_OK;
}

}; /* extern "C" */

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
        if (pConn) {
            /* 
             * Normally, the stream is closed when the connection has been
             * completed.  However, if the URL exit proc was called directly
             * by NET_GetURL(...), then the stream may still be around...
             */
            if (pConn->pNetStream) {
                pConn->pNetStream->Close();
                NS_RELEASE(pConn->pNetStream);
            }

            /* 
             * Notify the Data Consumer that the Binding has completed... 
             * Since the Consumer is still available, the stream was never
             * closed (or possibly created).  So, the binding has failed...
             */
            if (pConn->pConsumer) {
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

extern "C" void net_ReleaseContext(MWContext *context)
{
    if (context) {
        if (context->modular_data) {
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
  char* cp;

#ifdef XP_PC
  // XXX For now, all resources are relative to the .exe file
  resourceBase = (char *)PR_Malloc(_MAX_PATH);;
  DWORD mfnLen = GetModuleFileName(g_hInst, resourceBase, _MAX_PATH);
  // Truncate the executable name from the rest of the path...
  cp = strrchr(resourceBase, '\\');
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
  // XXX For now, all resources are relative to the current working directory

    FILE *pp;

#define MAXPATHLEN 2000

    resourceBase = (char *)PR_Malloc(MAXPATHLEN);;

    if (!(pp = popen("pwd", "r"))) {
      printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 1\n");
      return(nsnull);
    }
    else {
      if (fgets(resourceBase, MAXPATHLEN, pp)) {
        printf("[%s] %d\n", resourceBase, PL_strlen(resourceBase));
        resourceBase[PL_strlen(resourceBase)-1] = 0;
      }
      else {
       printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 2\n");
       return(nsnull);
      }
   }

   printf("RESOURCE name %s\n", resourceBase);
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
