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
};
#include "netcache.h"
#include "cookies.h"
#include "plstr.h"

#include "nsString.h"
#include "nsIProtocolConnection.h"
#include "nsINetContainerApplication.h"


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

#if defined(XP_WIN)
nsresult PerformNastyWindowsAsyncDNSHack(URL_Struct* URL_s, nsIURL* aURL);
#endif /* XP_WIN */

extern "C" {
#include "fileurl.h"
#include "httpurl.h"
#include "ftpurl.h"
#include "abouturl.h"
#include "gophurl.h"
#include "fileurl.h"
#include "remoturl.h"
#include "netcache.h"

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

extern "C" void RL_Init();

PUBLIC NET_StreamClass * 
NET_NGLayoutConverter(FO_Present_Types format_out,
                      void *converter_obj,
                      URL_Struct  *URL_s,
                      MWContext   *context);
};

/*
 * Initialize our protocols
 */

extern "C" void NET_ClientProtocolInitialize()
{
    NET_InitFileProtocol();
    NET_InitHTTPProtocol();
    NET_InitMemCacProtocol();
    NET_InitFTPProtocol();
    NET_InitAboutProtocol();
    NET_InitGopherProtocol();
    NET_InitRemoteProtocol();
}

static NS_DEFINE_IID(kIProtocolConnectionIID,  NS_IPROTOCOLCONNECTION_IID);
static NS_DEFINE_IID(kINetContainerApplicationIID,  NS_INETCONTAINERAPPLICATION_IID);

nsNetlibService::nsNetlibService(nsINetContainerApplication *aContainerApp)
{
    NS_INIT_REFCNT();

    /*
      m_stubContext = new_stub_context();
    */

    /* Initialize netlib with 32 sockets... */
    NET_InitNetLib(0, 32);

    /* Initialize the file extension -> content-type mappings */
    NET_InitFileFormatTypes(nsnull, nsnull);

    NET_FinishInitNetLib();

    NET_RegisterContentTypeConverter("*", FO_CACHE_AND_NGLAYOUT, NULL,
                                     NET_CacheConverter);
    NET_RegisterContentTypeConverter("*", FO_NGLAYOUT, NULL,
                                     NET_NGLayoutConverter);
    NET_RegisterContentTypeConverter(APPLICATION_HTTP_INDEX, FO_NGLAYOUT,
                                    NULL, NET_HTTPIndexFormatToHTMLConverter);

    NET_RegisterUniversalEncodingConverter("chunked",
                                           NULL,
                                           NET_ChunkedDecoderStream);

    mPollingTimer = nsnull;
    RL_Init();
    
    mContainer = aContainerApp;
    NS_IF_ADDREF(mContainer);
    if (NULL != mContainer) {
        nsAutoString str;
        
        mContainer->GetAppCodeName(str);
        XP_AppCodeName = str.ToNewCString();
        mContainer->GetAppVersion(str);
        XP_AppVersion = str.ToNewCString();
        mContainer->GetAppName(str);
        XP_AppName = str.ToNewCString();
        mContainer->GetPlatform(str);
        XP_AppPlatform = str.ToNewCString();
        mContainer->GetLanguage(str);
        XP_AppLanguage = str.ToNewCString();
    }
    else {
        // XXX: Where should the defaults really come from
        XP_AppCodeName = PL_strdup("Mozilla");
        XP_AppVersion = PL_strdup("5.0 Netscape/5.0 (Windows;I;x86;en)");
        XP_AppName = PL_strdup("Netscape");

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
NS_IMPL_ISUPPORTS(nsNetlibService,kINetServiceIID);


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
    NET_ShutdownNetLib();
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
        if (type == 2 || type == 3) {
            result = loadAttribs->GetBypassProxy((int *)&(aURL_s->bypassProxy));
            if (result != NS_OK)
                aURL_s->bypassProxy = FALSE;
        }

        result = loadAttribs->GetLocalIP(&localIP);
        if (result != NS_OK)
            localIP = 0;
        aURL_s->localIP = localIP;
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
    nsILoadAttribs* loadAttribs = nsnull;

    if ((NULL == aConsumer) || (NULL == aUrl)) {
        return NS_FALSE;
    }

    /* Create the nsConnectionInfo object... */
    pConn = new nsConnectionInfo(aUrl, NULL, aConsumer);
    if (NULL == pConn) {
        return NS_FALSE;
    }
    pConn->AddRef();

    /* We've got a nsConnectionInfo(), now hook it up
     * to the nsISupports of the nsIContentViewerContainer
     */
    pConn->pContainer = aUrl->GetContainer();
    if(pConn->pContainer) {
        NS_ADDREF(pConn->pContainer);
    }

    /* Create the URLStruct... */

    URL_s = NET_CreateURLStruct(aUrl->GetSpec(), reloadType);
    if (NULL == URL_s) {
        pConn->Release();
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
    nsresult result;
    nsILoadAttribs* loadAttribs = nsnull;



    if (NULL == aNewStream) {
        return NS_FALSE;
    }

    if (NULL != aUrl) {
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
        pBlockingStream->AddRef();

        /* Create the nsConnectionInfo object... */
        pConn = new nsConnectionInfo(aUrl, pBlockingStream, aConsumer);
        if (NULL == pConn) {
            pBlockingStream->Release();
            goto loser;
        }
        pConn->AddRef();

        /* We've got a nsConnectionInfo(), now hook it up
         * to the nsISupports of the nsIContentViewerContainer
         */
        pConn->pContainer = aUrl->GetContainer();
        if(pConn->pContainer) {
            NS_ADDREF(pConn->pContainer);
        }

        /* Create the URLStruct... */

        URL_s = NET_CreateURLStruct(aUrl->GetSpec(), reloadType);
        if (NULL == URL_s) {
            pBlockingStream->Release();
            pConn->Release();
            goto loser;
        }

        SetupURLStruct(aUrl, URL_s);

#if defined(XP_WIN)
        /*
         * When opening a blocking HTTP stream, perform a synchronous DNS 
         * lookup now, to avoid netlib from doing an async lookup later
         * and causing a deadlock!
         */
        result = PerformNastyWindowsAsyncDNSHack(URL_s, aUrl);
        if (NS_OK != result) {
            NET_FreeURLStruct(URL_s);
            pBlockingStream->Release();
            pConn->Release();
            goto loser;
        }
#endif /* XP_WIN */

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
    *aContainer = mContainer;

    NS_IF_ADDREF(mContainer);
    
    return NS_OK;
}

nsresult
nsNetlibService::SetContainerApplication(nsINetContainerApplication *aContainer)
{
    NS_IF_RELEASE(mContainer);

    mContainer = aContainer;
    
    NS_IF_ADDREF(mContainer);

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
    // If a timer is already active, then do not create another...
    if (nsnull == mPollingTimer) {
        if (NS_OK == NS_NewTimer(&mPollingTimer)) {
            mPollingTimer->Init(nsNetlibService::NetPollSocketsCallback, this, 1000 / 50);
        }
    }
}


void nsNetlibService::CleanupPollingTimer(nsITimer* aTimer) 
{
    NS_PRECONDITION((aTimer == mPollingTimer), "Unknown Timer...");

    NS_RELEASE(mPollingTimer);
}


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

// This is global so it can be accessed by the NetFactory (nsNetFactory.cpp)
nsNetlibService *gNetlibService = nsnull;

//
// Class to manage static initialization of the Netlib DLL...
//
struct nsNetlibInit {
  nsNetlibInit() {
    gNetlibService = nsnull;
    (void) NS_InitINetService(nsnull);
  }

  ~nsNetlibInit() {
    NS_ShutdownINetService();
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
        return NS_ERROR_OUT_OF_MEMORY;
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
                pConn->pNetStream->Release();
                pConn->pNetStream = NULL;
            }

            /* 
             * Notify the Data Consumer that the Binding has completed... 
             * Since the Consumer is still available, the stream was never
             * closed (or possibly created).  So, the binding has failed...
             */
            if (pConn->pConsumer) {
                nsAutoString status;
                pConn->pConsumer->OnStopBinding(pConn->pURL, NS_BINDING_FAILED, status);
                pConn->pConsumer->Release();
                pConn->pConsumer = NULL;
            }

            /* Release the nsConnectionInfo object hanging off of the fe_data */
            URL_s->fe_data = NULL;
            pConn->Release();
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
