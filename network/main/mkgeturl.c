/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
/* mkgeturl.c
 *  This file implements the main calling api for netlib.
 *    Includes NET_InitNetlib, NET_Shutdown, NET_GetURL, and NET_ProcessNet
 *
 * Designed and originally implemented by Lou Montulli '94
 * Additions/Changes by Judson Valeski, Gagan Saksena 1997.
 */
#if defined(CookieManagement)
/* #define TRUST_LABELS 1 */
#endif

#include "rosetta.h"
#include "xp_error.h"
#include "mkutils.h"
#include "mkgeturl.h"
#include "shist.h"
#include "mkparse.h"
#include "mkformat.h"
#include "mkstream.h"
#include "mkpadpac.h"
#include "netcache.h"
#include "mkprefs.h"
#ifdef NU_CACHE
#include "CacheStubs.h"
#endif
#include "nslocks.h"
#include "cookies.h"
#include "httpauth.h"
#include "cnetinit.h"
 
#if 0  /* NO PROTOCOLS HERE */
#include "mkhttp.h"
#include "mkftp.h"
#include "mkgopher.h"
#include "mkremote.h"
#include "mknews.h"
#include "mkpop3.h"
#include "mkdaturl.h"
#include "mkfile.h"
#include "mksmtp.h"
#include "mkcache.h"
#include "mkmemcac.h"
#include "mkmailbx.h"
#include "mkcertld.h"
#include "mkmarimb.h"
#include "libmocha.h"
#include "mkmocha.h"
#endif /* 0 */

#ifdef XP_MAC
#ifdef OLD_MOZ_MAIL_NEWS
#ifdef MOZ_SECURITY
#include "mkcertld.h"
#endif /* MOZ_SECURITY */
#include "mkmailbx.h"
#include "mknews.h"
#include "mknewsgr.h"
#include "mkpop3.h"
#include "mkimap4.h"
#include "mkldap.h"
#endif /* OLD_MOZ_MAIL_NEWS */
#endif /* XP_MAC */

#include "glhist.h"
#include "mkautocf.h"
#include "mkabook.h"
#include "mkhelp.h"
#include "ssl.h"

#ifdef MOZ_LDAP
#include "mkldap.h"
#endif

#ifdef OLD_MOZ_MAIL_NEWS
#include "mkimap4.h"
#endif

#include "mktcp.h"  /* for NET_InGetHostByName semaphore */
#ifdef MOZILLA_CLIENT
#include "secnav.h"
#include "libevent.h"
#include "jscompat.h"
#include "jspubtd.h"
#endif

#include "libi18n.h"

#include "np.h"
#include "prefapi.h"
#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#else
#include "private/prpriv.h"
#endif

#include "sslerr.h"
#include "merrors.h"
#include "prefetch.h"

#ifdef XP_OS2
#include "os2sock.h"
#endif
			 
#ifdef EDITOR
#include "edt.h"
#endif

#ifdef XP_UNIX
#include "prnetdb.h"
#else
#define PRHostEnt struct hostent
#endif

#include "xplocale.h"

#ifdef WEBFONTS
#include "nf.h"
#endif /* WEBFONTS */

/* plugins:  */
#include "np.h"

void net_ReleaseContext(MWContext *context);

#if defined(NETLIB_THREAD)
void net_CallExitRoutineProxy(Net_GetUrlExitFunc* exit_routine,
                              URL_Struct*         URL_s,
                              int                 status,
                              FO_Present_Types    format_out,
                              MWContext*          window_id);
#else
#define net_CallExitRoutineProxy net_CallExitRoutine
#endif /* !NETLIB_THREAD */

#include "timing.h"

#ifdef TRUST_LABELS
#include "mkaccess.h"		/* change to trust.h for phase 2 */
#endif

#if defined(SMOOTH_PROGRESS)
#include "progress.h"
#endif

/* for XP_GetString() */
#include "xpgetstr.h"
extern int MK_CONNECTION_REFUSED;
extern int MK_CONNECTION_TIMED_OUT;
extern int MK_MALFORMED_URL_ERROR;
extern int MK_OUT_OF_MEMORY;
extern int MK_RELATIVE_TIMEBOMB_MESSAGE;
extern int MK_RELATIVE_TIMEBOMB_WARNING_MESSAGE;
extern int MK_TIMEBOMB_MESSAGE;
extern int MK_TIMEBOMB_URL_PROHIBIT;
extern int MK_TIMEBOMB_WARNING_MESSAGE;
extern int MK_UNABLE_TO_CONNECT;
extern int MK_UNABLE_TO_CREATE_SOCKET;
extern int MK_UNABLE_TO_LOCATE_HOST;
extern int MK_UNABLE_TO_LOCATE_PROXY;
extern int XP_CHECKING_SERVER__FORCE_RELOAD;
extern int XP_OBJECT_HAS_EXPIRED;
extern int XP_CHECKING_SERVER_CACHE_ENTRY;
extern int XP_CHECKING_SERVER__LASTMOD_MISS;
extern int XP_ALERT_CONNECTION_LESSZERO;
extern int XP_ALERT_INTERRUPT_WINDOW;
extern int XP_ALERT_URLS_LESSZERO;
extern int XP_CONFIRM_REPOST_FORMDATA;
extern int XP_CONSULT_SYS_ADMIN;
extern int XP_HTML_MISSING_REPLYDATA;
extern int XP_SOCKS_NS_ENV_VAR;
extern int XP_SOME_HOSTS_UNREACHABLE;
extern int XP_THIS_IS_DNS_VERSION;
extern int XP_THIS_IS_YP_VERSION;
extern int XP_UNKNOWN_HOST;
extern int XP_UNKNOWN_HOSTS;
extern int XP_UNKNOWN_HTTP_PROXY;
extern int XP_UNKNOWN_SOCKS_HOST;
extern int XP_URLS_WAITING_FOR_AN_OPEN_SOCKET;
extern int XP_URLS_WAITING_FOR_FEWER_ACTIVE_URLS;
extern int XP_CONNECTIONS_OPEN;
extern int XP_ACTIVE_URLS;
extern int XP_USING_PREVIOUSLY_CACHED_COPY_INSTEAD;
extern int XP_BAD_AUTOCONFIG_NO_FAILOVER;

extern int XP_SOCK_CON_SOCK_PROTOCOL;                   
extern int XP_URL_NOT_FOUND_IN_CACHE;                   
extern int XP_PARTIAL_CACHE_ENTRY;
extern int XP_MSG_UNKNOWN ;
extern int XP_EDIT_NEW_DOC_URL;
extern int XP_EDIT_NEW_DOC_NAME;
extern int XP_CONFIRM_MAILTO_POST_1;
extern int XP_CONFIRM_MAILTO_POST_2;

extern int XP_ALERT_OFFLINE_MODE_SELECTED;

extern void NET_InitMailtoProtocol(void);

#if defined(JAVA) || defined(NS_MT_SUPPORTED)
#include "nslocks.h"
#include "prlog.h"
#include <stdarg.h>

PRMonitor* libnet_asyncIO;

#if defined(SOLARIS)
extern int gethostname(char *, int);
#endif /* SOLARIS */

#endif /* JAVA || NS_MT_SUPPORTED */

#define LIBNET_UNLOCK_AND_RETURN(value) \
	do { int rv = (value); LIBNET_UNLOCK(); return rv; } while (0)

/*
** Don't ever forget about this!!!
**
** This is ALL superseeded by the setting in /ns/modules/libpref/src/init/all.js
** lock for timebomb.use_timebomb
 
** Define TIMEBOMB_ON for beta builds.
** Undef TIMEBOMB_ON for release builds.
*/
#define TIMEBOMB_ON
/* #undef TIMEBOMB_ON  */
/*
** After this date all hell breaks loose
*/
#define TIME_BOMB_TIME          888739203       /* 3/01/98 + 3 secs */
#ifdef XP_OS2
#define TIME_BOMB_WARNING_TIME  869976003       /* 7/21/97 + 3 secs */
#else
#define TIME_BOMB_WARNING_TIME  857203203       /* 3/01/97 + 3 secs */
#endif

#ifndef XP_UNIX
#ifdef NADA_VERSION
HG73787
#else
HG72987
#endif
#endif

#ifdef EDITOR
/* 
 * Strings used for new, empty document creation
 * Kluge we use to load a new document is resource with one space.
 * These strings are loaded from XP_MSG.I on first GetURL call
 * (this allows changing "Untitled" for other languages)
*/
PUBLIC char *XP_NEW_DOC_URL = NULL; /* "about:editfilenew" */
/* 
 * This is the name we want to appear as the URL address
 * You can load a new doc using this - it will be changed 
 *  to XP_NEW_DOC_URL. 
 * When done, XP_NEW_DOC_URL will be changed to XP_NEW_DOC_NAME
*/
PUBLIC char *XP_NEW_DOC_NAME = NULL; /* "Untitled" */
#endif

#ifdef PROFILE
#pragma profile on
#endif

/* forward decl's */
PRIVATE void add_slash_to_URL (URL_Struct *URL_s);
PRIVATE Bool override_proxy (CONST char * URL);
HG52422

PRIVATE void net_FreeURLAllHeaders(URL_Struct * URL_s);

PRIVATE NET_TimeBombActive = FALSE;

typedef struct _WaitingURLStruct {
	int                 type;
	URL_Struct         *URL_s;
	FO_Present_Types    format_out;
    MWContext          *window_id;
    Net_GetUrlExitFunc *exit_routine;
} WaitingURLStruct;

/* Pointers to proxy servers
 * They are pointers to a host if
 * a proxy environment variable
 * was found or they are zero if not.
 */
PRIVATE char * MKftp_proxy=0;
PRIVATE char * MKgopher_proxy=0;
PRIVATE char * MKhttp_proxy=0;
HG62630
PRIVATE char * MKwais_proxy=0;
PRIVATE char * MKnews_proxy=0;
PRIVATE char * MKno_proxy=0;
PRIVATE char * MKproxy_ac_url=0;
PRIVATE NET_ProxyStyle MKproxy_style = PROXY_STYLE_UNSET;

PRIVATE char * MKglobal_config_url = 0;

PRIVATE XP_List * net_EntryList=0;

#ifdef TRUST_LABELS
/* This list contains all the URL_Struct s that are created */
PRIVATE XP_List * net_URL_sList=0;
#endif

MODULE_PRIVATE CacheUseEnum NET_CacheUseMethod=CU_CHECK_PER_SESSION;

#ifdef XP_WIN16
#define MAX_NUMBER_OF_PROCESSING_URLS 8
#else
#define MAX_NUMBER_OF_PROCESSING_URLS 15
#endif

#define INITIAL_MAX_ALL_HEADERS 10
#define MULT_ALL_HEADER_COUNT   2
#define MAX_ALL_HEADER_COUNT    16000

MODULE_PRIVATE time_t NET_StartupTime=0;

PRIVATE XP_Bool get_url_disabled = FALSE;

MODULE_PRIVATE PRBool NET_ProxyAcLoaded = PR_FALSE;
MODULE_PRIVATE XP_Bool NET_GlobalAcLoaded = FALSE;
MODULE_PRIVATE int NET_TotalNumberOfOpenConnections=0;
MODULE_PRIVATE int NET_MaxNumberOfOpenConnections=100;
MODULE_PRIVATE int NET_MaxNumberOfOpenConnectionsPerContext=4;
MODULE_PRIVATE int NET_TotalNumberOfProcessingURLs=0;
PRIVATE XP_List  * net_waiting_for_actives_url_list=0;
PRIVATE XP_List  * net_waiting_for_connection_url_list=0;

typedef struct NETExitCallbackNode {
	struct NETExitCallbackNode *next;
	void *arg;
	void (* func)(void *arg);
} NETExitCallbackNode;

PRIVATE NETExitCallbackNode *NETExitCallbackHead = NULL;

/* forward decl */
PRIVATE void net_cleanup_reg_protocol_impls(void);
PRIVATE void NET_InitTotallyRandomStuffPeopleAddedProtocols(void);


PUBLIC void
NET_DisableGetURL(void)
{
	get_url_disabled = TRUE;
}

/* fix Mac warning for missing prototype */
PUBLIC void
NET_AddExitCallback( void (* func)(void *arg), void *arg);

PUBLIC void
NET_AddExitCallback( void (* func)(void *arg), void *arg)
{
	NETExitCallbackNode *node;
	
	/* alloc node */
	node = PR_Malloc(sizeof(NETExitCallbackNode));
	if ( node != NULL ) {
		/* fill it in */
		node->func = func;
		node->arg = arg;
		/* link it into the list */
		node->next = NETExitCallbackHead;
		NETExitCallbackHead = node;
	}
	
	return;
}

PRIVATE void
NET_ProcessExitCallbacks(void)
{
	NETExitCallbackNode *node, *freenode;

	node = NETExitCallbackHead;
	
	while ( node != NULL ) {
		(* node->func)(node->arg);
		freenode = node;
		node = node->next;
		PR_Free(freenode);
	}
	
	NETExitCallbackHead = NULL;
	
	return;
}

/*  Gather manual proxy information from the prefapi. Called from 
   SetupPrefs and SelectProxyStyle */
PRIVATE void
NET_UpdateManualProxyInfo(const char * prefChanged) {

	XP_Bool bSetupAll=FALSE;
	char * proxy = NULL;
	PRInt32 iPort=0;
	char text[MAXHOSTNAMELEN + 8];

	if (!prefChanged)
		bSetupAll = TRUE;

	if (bSetupAll || !PL_strcmp(prefChanged, pref_proxyFtpServer) || 
		!PL_strcmp(prefChanged, pref_proxyFtpPort)) {
		if( (PREF_OK == PREF_CopyCharPref(pref_proxyFtpServer,&proxy))
            && proxy && *proxy) {
            if ( (PREF_OK == PREF_GetIntPref(pref_proxyFtpPort,&iPort)) ) {
			    sprintf(text,"%s:%d", proxy, iPort);
		        StrAllocCopy(MKftp_proxy, text);
			    iPort=0;
            } else {
                FREE_AND_CLEAR(MKftp_proxy);
            }
		}
		else 
			FREE_AND_CLEAR(MKftp_proxy);
	}
	if (proxy) FREE_AND_CLEAR(proxy);

	if (bSetupAll || !PL_strcmp(prefChanged, pref_proxyGopherServer) || 
		!PL_strcmp(prefChanged, pref_proxyGopherPort)) {
		if( (PREF_OK == PREF_CopyCharPref(pref_proxyGopherServer,&proxy)) 
            && proxy && *proxy) {
            if ( (PREF_OK == PREF_GetIntPref(pref_proxyGopherPort,&iPort)) ) {
			    sprintf(text,"%s:%d", proxy, iPort);  
		        StrAllocCopy(MKgopher_proxy, text);
			    iPort=0;
            } else {
                FREE_AND_CLEAR(MKgopher_proxy);
            }
		}
		else 
			FREE_AND_CLEAR(MKgopher_proxy);
	}
	if (proxy) FREE_AND_CLEAR(proxy);

	if (bSetupAll || !PL_strcmp(prefChanged, pref_proxyHttpServer) || 
		!PL_strcmp(prefChanged, pref_proxyHttpPort)) {
		if( (PREF_OK == PREF_CopyCharPref(pref_proxyHttpServer,&proxy)) 
            && proxy && *proxy) {
            if ( (PREF_OK == PREF_GetIntPref(pref_proxyHttpPort,&iPort)) ) {
			    sprintf(text,"%s:%d", proxy, iPort);
                /* Don't change the proxy info unless necessary. */
                if (PL_strcmp(MKhttp_proxy, text))
                    StrAllocCopy(MKhttp_proxy, text);
			    iPort=0;
            } else {
                FREE_AND_CLEAR(MKhttp_proxy);
            }
		}
		else 
			FREE_AND_CLEAR(MKhttp_proxy);
	}
	if (proxy) FREE_AND_CLEAR(proxy);

	HG24324
	if (proxy) FREE_AND_CLEAR(proxy);

	if (bSetupAll || !PL_strcmp(prefChanged, pref_proxyNewsServer) || 
		!PL_strcmp(prefChanged, pref_proxyNewsPort)) {
		if( (PREF_OK == PREF_CopyCharPref(pref_proxyNewsServer,&proxy))
            && proxy && *proxy) {
            if ( (PREF_OK == PREF_GetIntPref(pref_proxyNewsPort,&iPort)) ) {
			    sprintf(text,"%s:%d", proxy, iPort);  
		        StrAllocCopy(MKnews_proxy, text);
			    iPort=0;
            } else {
                FREE_AND_CLEAR(MKnews_proxy);
            }
		}
		else 
			FREE_AND_CLEAR(MKnews_proxy);
	}
	if (proxy) FREE_AND_CLEAR(proxy);

	if (bSetupAll || !PL_strcmp(prefChanged, pref_proxyWaisServer) || 
		!PL_strcmp(prefChanged, pref_proxyWaisPort)) {
		if( (PREF_OK == PREF_CopyCharPref(pref_proxyWaisServer,&proxy))
            && proxy && *proxy) {
            if ( (PREF_OK == PREF_GetIntPref(pref_proxyWaisPort,&iPort)) ) {
			    sprintf(text,"%s:%d", proxy, iPort);  
		        StrAllocCopy(MKwais_proxy, text);
			    iPort=0;
            } else {
                FREE_AND_CLEAR(MKwais_proxy);
            }
		}
		else 
			FREE_AND_CLEAR(MKwais_proxy);
	}
	if (proxy) FREE_AND_CLEAR(proxy);

	if (bSetupAll || !PL_strcmp(prefChanged, pref_socksServer) || 
		!PL_strcmp(prefChanged, pref_socksPort)) {
		if ( (PREF_OK == PREF_CopyCharPref(pref_socksServer,&proxy)) 
            && proxy && *proxy) {
            if ( (PREF_OK == PREF_GetIntPref(pref_socksPort,&iPort)) ) {
			    PR_snprintf(text, sizeof(text), "%s:%d", proxy, iPort);  
			    NET_SetSocksHost(text);
            } else {
                NET_SetSocksHost(NULL); /* NULL is ok */
            }
		}
		else {
			NET_SetSocksHost(proxy); /* NULL is ok */
		}
		iPort=0;
	}
	if (proxy) FREE_AND_CLEAR(proxy);

	if (bSetupAll || !PL_strcmp(prefChanged, pref_proxyNoProxiesOn)) {
		if( (PREF_OK == PREF_CopyCharPref(pref_proxyNoProxiesOn,&proxy))
            && proxy && *proxy) {
		    StrAllocCopy(MKno_proxy, proxy);
        } else {
            FREE_AND_CLEAR(MKno_proxy);
        }
	}
	if (proxy) FREE_AND_CLEAR(proxy);
	return;
}

PUBLIC NET_ProxyStyle
NET_GetProxyStyle(void)
{
	return MKproxy_style;
}

/* These are used in netlib only for exposing the MKproxy_ac_url 
 * to proxy autodiscovery stuff in mkpadpac.c */
MODULE_PRIVATE const char *
net_GetPACUrl(void) {
	return MKproxy_ac_url;
}

MODULE_PRIVATE void
net_SetPACUrl(char *u) {
	/* Do a straight assignment, no checking. */
	MKproxy_ac_url=u;
}

PUBLIC void
NET_SelectProxyStyle(NET_ProxyStyle style)
{
 /* The NET_ProxyStyle enum is defined in net.h.
	PROXY_STYLE_UNSET               = 0             -- ?
	PROXY_STYLE_MANUAL              = 1             -- old style proxies
	PROXY_STYLE_AUTOMATIC   = 2             -- new style proxies
	PROXY_STYLE_NONE                = 3             -- no proxies, direct connection
 */     
	char * proxy=0;

	if (MKproxy_style != style) {
		NET_ProxyAcLoaded = FALSE;
	}

	MKproxy_style = style;

	if (MKproxy_style == PROXY_STYLE_MANUAL) {
		/* Get the manual info */
		NET_UpdateManualProxyInfo(NULL);
	} 
	
	if (MKproxy_style == PROXY_STYLE_AUTOMATIC) {
		/* Get the autoconfig url */
		if ( (PREF_OK == PREF_CopyCharPref(pref_proxyACUrl,&proxy))
            && proxy && *proxy) {
			StrAllocCopy(MKproxy_ac_url, proxy);
			NET_ProxyAcLoaded = FALSE;
		}
		if (proxy) FREE_AND_CLEAR(proxy);
	}

	/* If we're not in automatic or manual style, clear un-needed internal info */
	if (MKproxy_style != PROXY_STYLE_AUTOMATIC) {
		FREE_AND_CLEAR(MKproxy_ac_url);
		NET_ProxyAcLoaded = FALSE;
	}

	/* If we're not in manual (proxies) clear the un-needed internal info. */
	if (MKproxy_style != PROXY_STYLE_MANUAL) {
		FREE_AND_CLEAR(MKftp_proxy);
		FREE_AND_CLEAR(MKgopher_proxy);
		FREE_AND_CLEAR(MKhttp_proxy);
		HG63454
		FREE_AND_CLEAR(MKwais_proxy);
		FREE_AND_CLEAR(MKnews_proxy);
		FREE_AND_CLEAR(MKno_proxy);

		NET_SetSocksHost(NULL);
	} 
}

/* this function loads the proxy values from the various preferences and
sets up the internal cached values...It is called during Init and whenever
a proxy pref is changed...If no specific pref is passed, it sets them all
up */

PUBLIC void
NET_SetupPrefs(const char * prefChanged)
{
	XP_Bool bSetupAll=FALSE;
	char * proxy = NULL;

	/* if prefChanged is null the following PL_strcmp's are never executed because the
	   || statement never gets that far because when prefChanged is null bSetupAll is
	   set to true and is always checked first in the if statement. Don't set prefChanged
	   to "" if it is null because the UpdateManualProxyInfo below needs null not "" if
	   prefChanged was originally null. */
	if (!prefChanged)
		bSetupAll = TRUE;

	if (bSetupAll || !PL_strcmp(prefChanged, pref_dnsExpiration)) {
		PRInt32 n;
        if ( (PREF_OK != PREF_GetIntPref(pref_dnsExpiration,&n)) ) {
            n = DEF_DNS_EXPIRATION;
        }
		NET_SetDNSExpirationPref(n);
	}
	
	if (bSetupAll || !PL_strcmp(prefChanged, pref_browserPrefetch)) {
		PRInt32 n;
        if ( (PREF_OK != PREF_GetIntPref(pref_browserPrefetch,&n)) ) {
            n = 0;
        }
    	PRE_Enable((PRUint8)n);
	}
		
	if (bSetupAll || !PL_strcmp(prefChanged,pref_browserCacheMemSize)) {
		PRInt32 nMemCache;
        if ( (PREF_OK != PREF_GetIntPref(pref_browserCacheMemSize,&nMemCache)) ) {
            nMemCache = DEF_MEM_CACHE_SIZE;
        }
		NET_SetMemoryCacheSize(nMemCache * 1024);
	}

	if (bSetupAll || !PL_strcmp(prefChanged,pref_browserCacheDiskSize)) {
		PRInt32 nDiskCache;
        if ( (PREF_OK != PREF_GetIntPref(pref_browserCacheDiskSize,&nDiskCache)) ) {
            nDiskCache = DEF_DISK_CACHE_SIZE;
        }
	    NET_SetDiskCacheSize(nDiskCache * 1024);
	}

	if (bSetupAll || !PL_strcmp(prefChanged, pref_browserCacheDocFreq)) {
		PRInt32 nDocReqFreq;
        if ( (PREF_OK != PREF_GetIntPref(pref_browserCacheDocFreq,&nDocReqFreq)) ) {
            nDocReqFreq = DEF_CHECK_DOC_FREQ;
        }
		NET_SetCacheUseMethod((CacheUseEnum)nDocReqFreq);
	}
	
	HG42422
#ifdef OLD_MOZ_MAIL_NEWS
	if (bSetupAll || !PL_strcmp(prefChanged,pref_mailAllowSignInUName)) {
		XP_Bool prefBool;
        if ( (PREF_OK != PREF_GetBoolPref(pref_mailAllowSignInUName,&prefBool)) ) {
            prefBool = DEF_ALLOW_AT_SIGN_UNAME;
        }
		NET_SetAllowAtSignInMailUserName (prefBool);
	}
#endif /* OLD_MOZ_MAIL_NEWS */
	
	if (bSetupAll || !PL_strcmp(prefChanged,pref_proxyACUrl)) {
		if ( (PREF_OK == PREF_CopyCharPref(pref_proxyACUrl,&proxy))
            && proxy && *proxy) {
			StrAllocCopy(MKproxy_ac_url, proxy);
			NET_ProxyAcLoaded = FALSE;
		}
		else 
			FREE_AND_CLEAR(MKproxy_ac_url);
	}
	if (proxy) FREE_AND_CLEAR(proxy);
	
	NET_UpdateManualProxyInfo(prefChanged);

	if (bSetupAll || !PL_strcmp(prefChanged, pref_proxyType)) {
		PRInt32 iType;
        if ( (PREF_OK != PREF_GetIntPref(pref_proxyType,&iType)) ) {
            iType = DEF_PROXY_TYPE;
        }
	    NET_SelectProxyStyle((NET_ProxyStyle)iType);
	}
}

/* register preference callbacks for netlib prefs
*/

MODULE_PRIVATE int PR_CALLBACK NET_PrefChangedFunc(const char *pref, void *data) {
	NET_SetupPrefs(pref);
	return TRUE;
} 

#ifdef NS_NET_FILE
extern PRBool NET_InitFilesAndDirs(void);
#endif
/* finish the init of 'netlib'.  inits cookies, cache, history
 *
 */
PUBLIC void 
NET_FinishInitNetLib()
{
#ifdef NS_NET_FILE
    /* Initialize all the directories and files that netlib needs. */
    if (!NET_InitFilesAndDirs())
        ; /* do something. */
#endif


#ifdef MOZILLA_CLIENT
    NET_CacheInit();
    NET_ReadCookies("");
#endif /* MOZILLA_CLIENT */

	NET_RegisterEnableUrlMatchCallback();
	NET_SetupPrefs(NULL);  /* setup initial proxy, socks, dnsExpiration and cache preference info */

    PREF_RegisterCallback("network.proxy",NET_PrefChangedFunc,NULL);
#ifndef NU_CACHE
	PREF_RegisterCallback("browser.cache",NET_PrefChangedFunc,NULL); 
#endif
	PREF_RegisterCallback("network.hosts.socks_server",NET_PrefChangedFunc,NULL);
	PREF_RegisterCallback("network.hosts.socks_serverport",NET_PrefChangedFunc,NULL);
	PREF_RegisterCallback("network.dnsCacheExpiration",NET_DNSExpirationPrefChanged,NULL);
	NET_RegisterCookiePrefCallbacks(); /* simply inits cookie info in mkaccess.c */
									   /* and registers the callbacks */
	/* inits the proxy autodiscovery vars and registers their callbacks. */
	NET_RegisterPadPrefCallbacks();
	PREF_RegisterCallback("mail.allow_at_sign_in_user_name", NET_PrefChangedFunc, NULL);

#ifdef XP_UNIX
	{
		char *ac_url = getenv("AUTOCONF_URL");
		if (ac_url) {
			NET_SetProxyServer(PROXY_AUTOCONF_URL, ac_url);
			NET_SelectProxyStyle(PROXY_STYLE_AUTOMATIC);
		}
	}
#endif

}

/* initialize the netlibrary and set the socket buffer size
 */
PUBLIC int
NET_InitNetLib(int socket_buffer_size, int max_number_of_connections)
{
    int status;

#if defined(DEBUG)
        if (NETLIB==NULL)
           NETLIB = PR_NewLogModule("NETLIB");
#endif

	TRACEMSG(("Initializing Network library"));

	NET_StartupTime = time(NULL);

	/* seed the random generator
	 */
	XP_SRANDOM((unsigned int) time(NULL));

	if(max_number_of_connections < 1)
		max_number_of_connections = 1;

#ifdef XP_UNIX
	signal(SIGPIPE, SIG_IGN);
#endif
	

    status = NET_ChangeSocketBufferSize(socket_buffer_size);

	NET_ChangeMaxNumberOfConnections(max_number_of_connections);
	net_waiting_for_actives_url_list = XP_ListNew();
	net_waiting_for_connection_url_list = XP_ListNew();

    net_EntryList = XP_ListNew();
#ifdef TRUST_LABELS
	net_URL_sList = XP_ListNew();
#endif

    NET_TotalNumberOfProcessingURLs=0; /* reset */

#if defined(JAVA) || defined(NS_MT_SUPPORTED)
    libnet_asyncIO = PR_NewNamedMonitor("libnet");
#endif
 
	/* initialize protocol modules 
	 *
	 * eventually this should be done dynamically,
	 * but first we need to make URL types be dynamically
	 * registered, and make URL parsing dynamic as well.
	 *
	 * Currently, registering them in the most used order is 
	 * very slightly more efficient
	 */

	NET_ClientProtocolInitialize();

	/* @@@@ these need to move soon */
	NET_InitNFSProtocol();
	NET_InitURNProtocol();
	NET_InitWAISProtocol();
#ifdef SMART_MAIL
	NET_InitPop3Protocol(); 
#endif
	NET_InitTotallyRandomStuffPeopleAddedProtocols();

#if defined(OLD_MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
	NET_InitMailtoProtocol();  /* has a stub for OLD_MOZ_MAIL_NEWS */
#endif /* OLD_MOZ_MAIL_NEWS || MOZ_MAIL_COMPOSE */
#ifdef OLD_MOZ_MAIL_NEWS    
	NET_InitNewsProtocol();
	NET_InitMailboxProtocol();
	NET_InitMsgSearchProtocol();
	 
	NET_InitLDAPProtocol();
#ifdef MOZ_SECURITY
	NET_InitCertLdapProtocol();
#endif /* MOZ_SECURITY */
	NET_InitAddressBookProtocol();
	NET_InitIMAP4Protocol();
#endif /* OLD_MOZ_MAIL_NEWS */

    return(status);
}

/* set the maximum allowable number of open connections at any one
 * time regardless of the context
 */
PUBLIC void
NET_ChangeMaxNumberOfConnections(int max_number_of_connections)
{

	/* if already equal return */
	if(NET_MaxNumberOfOpenConnections == max_number_of_connections)
		return;

	if(max_number_of_connections < 1)
	    max_number_of_connections = 1;

	/* make sure that # conns per context is less than or equal
	 * to this Max
	 */
	if(NET_MaxNumberOfOpenConnectionsPerContext > max_number_of_connections)
	    NET_MaxNumberOfOpenConnectionsPerContext = max_number_of_connections;

	NET_MaxNumberOfOpenConnections = max_number_of_connections;

	/* close any open connections to prevent deadlock
	 */
	net_cleanup_reg_protocol_impls();
}

/* set the maximum allowable number of open connections at any one
 * time Per context
 */
PUBLIC void
NET_ChangeMaxNumberOfConnectionsPerContext(int max_number_of_connections)
{

	/* if already equal return */
	if(NET_MaxNumberOfOpenConnectionsPerContext == max_number_of_connections)
		return;

	/* gate the number of max connections to be within 1 and 4
	 */
	if(max_number_of_connections < 1)
		max_number_of_connections = 1;

	if(max_number_of_connections > 6)
		max_number_of_connections = 6;

	NET_MaxNumberOfOpenConnectionsPerContext = max_number_of_connections;

	/* close any open connections to prevent deadlock
	 */
/* @@@@	NET_CleanupFTP(); */
#ifdef MOZILLA_CLIENT
#ifdef OLD_MOZ_MAIL_NEWS    
/* @@@@	NET_CleanupNews(); */
#endif /* OLD_MOZ_MAIL_NEWS */   
#endif /* MOZILLA_CLIENT */

}

/* check the date and set off the timebomb if necessary
 *
 * Calls FE_Alert with a message and then disables
 * all network calls to hosts not in our domains,
 * except for FTP connects.
 */
PUBLIC Bool
NET_CheckForTimeBomb(MWContext *context)
{
	static Bool done = FALSE;
	time_t cur_time;
	time_t timebomb_time = 0;
	time_t timebomb_warning_time = 0;
	
	int32 relative_timebomb_days = 0;
	int32 relative_timebomb_warning_days = 0;
	time_t relative_timebomb_start_date = 0;
	char *relative_timebomb_prefs_name;
	time_t relative_timebomb_time = 0;
	time_t relative_timebomb_warning_time = 0;
	Bool have_relative_timebomb = FALSE;
	Bool just_started_relative_timebomb = FALSE;

	if(done)
		return(NET_TimeBombActive);

	/*
	 * Check if any timebomb is enabled
	 */
	PREF_GetConfigInt("timebomb.expiration_time",(PRInt32 *)&timebomb_time);
	PREF_GetConfigInt("timebomb.relative_timebomb_days",
								(PRInt32 *)&relative_timebomb_days);

	if (relative_timebomb_days >= 1)
		have_relative_timebomb = TRUE;

	done = TRUE;

	if ((timebomb_time < 1) && (have_relative_timebomb == FALSE))
		return FALSE;

	cur_time  = XP_TIME();

	/*
	 * check the absolute timebomb
	 */
	TRACEMSG(("current time is: %ld %s", cur_time, ctime(&cur_time)));
	TRACEMSG(("Timebomb time is: %ld %s", timebomb_time,ctime(&timebomb_time)));
	if ((timebomb_time >= 1) && (cur_time > timebomb_time))
	  {
		char * alert = NET_ExplainErrorDetails(MK_TIMEBOMB_MESSAGE);

		FE_Alert(context, alert);
		FREE(alert);

		NET_TimeBombActive = TRUE;

		return(TRUE);
	  }

	/*
	 * check the relative timebomb
	 * (note: we store the start date in prefs using a "secret" name )
	 */
	if (have_relative_timebomb) {
		TRACEMSG(("Relative timebomb days = %ld", relative_timebomb_days));
		relative_timebomb_prefs_name = NULL;
		PREF_CopyConfigString("timebomb.relative_timebomb_secret_name",
								&relative_timebomb_prefs_name);
		if (relative_timebomb_prefs_name == NULL)
			relative_timebomb_prefs_name = 
								PL_strdup("general.bproxy_cert_digest");
		relative_timebomb_start_date = 0;
		PREF_GetIntPref(relative_timebomb_prefs_name,
								(PRInt32 *)&relative_timebomb_start_date);

		if (relative_timebomb_start_date < 1)
		{
			/* if no value must be first time so set start date */
			just_started_relative_timebomb = TRUE;
			relative_timebomb_start_date = cur_time;
			PREF_SetIntPref(relative_timebomb_prefs_name,
							(int32)relative_timebomb_start_date);
		}
		PR_Free(relative_timebomb_prefs_name);
		relative_timebomb_time = relative_timebomb_start_date +
								(relative_timebomb_days * 24 * 60 * 60);
		if (cur_time > relative_timebomb_time)
		{
			char *alert = NET_ExplainErrorDetails(MK_RELATIVE_TIMEBOMB_MESSAGE);

			FE_Alert(context, alert);
			FREE(alert);

			NET_TimeBombActive = TRUE;

			return(TRUE);
		}
	}

	/*
	 * check the absolute timebomb warning
	 */
	PREF_GetConfigInt("timebomb.warning_time",(PRInt32 *)&timebomb_warning_time);
	TRACEMSG(("Timebomb warning time is: %ld %s", timebomb_warning_time,
								ctime(&timebomb_warning_time)));
	if ((timebomb_warning_time >= 1) && (cur_time > timebomb_warning_time))
	  {
		char * alert = NET_ExplainErrorDetails( MK_TIMEBOMB_WARNING_MESSAGE,
										INTL_ctime(context, &timebomb_time));
		FE_Alert(context, alert);
		FREE(alert);

		NET_TimeBombActive = FALSE;

		return(FALSE);
	  }

	/*
	 * check the relative timebomb warning
	 * also: on first use warn user this is timebombed 
	 */
	if (have_relative_timebomb) {
		relative_timebomb_warning_days = 0;
		PREF_GetConfigInt("timebomb.relative_timebomb_warning_days",
								(PRInt32 *)&relative_timebomb_warning_days);
		TRACEMSG(("Relative timebomb warning days = %ld", 
									relative_timebomb_warning_days));
		if (relative_timebomb_warning_days >= 1)
			relative_timebomb_warning_time = relative_timebomb_start_date +
								(relative_timebomb_warning_days * 24 * 60 * 60);
		else
			relative_timebomb_warning_time = cur_time;
		if (just_started_relative_timebomb ||
				(cur_time > relative_timebomb_warning_time))
		{
			char * alert = NET_ExplainErrorDetails(
						MK_RELATIVE_TIMEBOMB_WARNING_MESSAGE,
						INTL_ctime(context, &relative_timebomb_time));
			FE_Alert(context, alert);
			FREE(alert);

			NET_TimeBombActive = FALSE;

			return(FALSE);
		}
	}

	return(FALSE);
}

/* set the way the cache should be used
 */
PUBLIC void
NET_SetCacheUseMethod(CacheUseEnum method)
{
    NET_CacheUseMethod = method;
}

MODULE_PRIVATE void
net_CallExitRoutine(Net_GetUrlExitFunc *exit_routine,
					URL_Struct                 *URL_s,
					int                 status,
					FO_Present_Types        format_out,
					MWContext          *window_id)
{
#ifdef EDITOR
    /* History_entry * his; */
    /* Change all references to "about:editfilenew" into "file:///Untitled" */
    /* Do this for both Browser and Editor windows */
    if( /* EDT_IS_EDITOR(window_id) && */
	URL_s && URL_s->address &&
	0 == PL_strcmp(URL_s->address, XP_NEW_DOC_URL) )
    {
	PR_Free(URL_s->address);
	URL_s->address = PL_strdup(XP_NEW_DOC_NAME);
       
	/* Not sure if this is the best place to do this,
	 * but it needs to go someplace!        
	*/
	LO_SetBaseURL(window_id, XP_NEW_DOC_NAME);

	/* Setting context title to NULL will trigger
	 * Windows front end to use the URL address and set 
	 * doc and window title to "Untitled"
	 * Do all front ends do this?
	 * If not, they will have to use EDT_IS_NEW_DOCUMENT
	 *  to test for new doc and update title (in exit_routine)
	*/
	if ( window_id->title ) {
	    PR_Free(window_id->title);
	    window_id->title = NULL;
	}
	/* Note: We replace "about:editfilenew" with "file:///Untitled"
	 * in SHIST_AddDocument(), so we don't need to mess with
	 * History data here.
	*/

	/* Set  window_id->is_new_document flag appropriately 
	 * This flag allows quicker response for often-called 
	 * status queries at front end
	*/
	if ( EDT_IS_EDITOR(window_id) ) {
	    EDT_NEW_DOCUMENT(window_id, TRUE);
	}
    }
#endif /* EDITOR */
#if defined(XP_WIN) || defined (XP_MAC) || defined (XP_OS2)
	FE_URLEcho(URL_s, status, window_id);
#endif /* XP_WIN/MAC/OS2 */

	if (!URL_s->load_background)
	  FE_EnableClicking(window_id);

#ifdef MOZILLA_CLIENT
	if(URL_s->refresh_url && status != MK_INTERRUPTED)
		FE_SetRefreshURLTimer(window_id, URL_s);
#endif /* MOZILLA_CLIENT */

    if (URL_s->pre_exit_fn)
	  {
		Net_GetUrlExitFunc *per = URL_s->pre_exit_fn;
		URL_s->pre_exit_fn = 0;
		(*per) (URL_s, status, window_id);
	  }
    /* byrd:  plugin specific stuff, where we've added our own exit routine:  */
	if ((URL_s->owner_data != NULL) && 
		(URL_s->owner_id == 0x0000BAC0)){
			NPL_URLExit(URL_s,status,window_id);
	}

	if ( exit_routine != NULL ) {
#if defined(SingleSignon)
		if (status<0 && URL_s->error_msg != NULL) {
		    SI_RemoveUser(URL_s->address, NULL, TRUE);
		}
#endif
		(*exit_routine) (URL_s, status, window_id);
	}
}

PRIVATE XP_Bool
net_does_url_require_socket_limit(int urltype)
{
	if( urltype == HTTP_TYPE_URL
	HG64398
	|| urltype == FTP_TYPE_URL
	|| urltype == NEWS_TYPE_URL
	|| urltype == GOPHER_TYPE_URL)
		return TRUE;

	return(FALSE);
}


PRIVATE XP_Bool
net_is_one_url_allowed_to_run(MWContext *context, int url_type)
{
    /* put a limit on the total number of open connections
     */
    if(NET_TotalNumberOfOpenConnections < NET_MaxNumberOfOpenConnectionsPerContext
	|| !net_does_url_require_socket_limit(url_type))
      {
		return(TRUE);
	  }
	else if(NET_TotalNumberOfOpenConnections >= NET_MaxNumberOfOpenConnections)
	  {
		return(FALSE);
	  }
	else
	  {
	XP_List * list_ptr;
	ActiveEntry * tmpEntry;
	int32 cur_win_id = FE_GetContextID(context);
	int real_number_of_connections = 0;


		/* run through the list of running URLs and check
		 * for open connections only for this context.
		 * This will allow for the Total number of open
		 * connections to only apply to each window not
		 * to the whole program
		 */
	list_ptr = net_EntryList;
	while((tmpEntry = (ActiveEntry *) XP_ListNextObject(list_ptr)) != NULL)
	  {
		if(cur_win_id == FE_GetContextID(tmpEntry->window_id)
				&& net_does_url_require_socket_limit(url_type))
			  {
				real_number_of_connections++;
		  }
	  } /* end while */

		/* pause it if the number of connections per context
		 * is higher than the per context limit or
		 */
	if(real_number_of_connections >= NET_MaxNumberOfOpenConnectionsPerContext)

		  {
			return(FALSE);
		  }
      }

	return(TRUE);
}
 
static int
net_push_url_on_wait_queue(int                 url_type,
						   URL_Struct         *URL_s,
						   FO_Present_Types    format_out,
						   MWContext          *context,
						   Net_GetUrlExitFunc  exit_routine)
{

	WaitingURLStruct * wus = PR_NEW(WaitingURLStruct);

	TRACEMSG(("Pushing URL on wait queue with %d open connections",
					 NET_TotalNumberOfOpenConnections ));
	if(!wus)
	  {
		net_CallExitRoutineProxy(exit_routine,
							URL_s,
							MK_OUT_OF_MEMORY,
							format_out,
							context);
        net_ReleaseContext(wus->window_id);
		return(MK_OUT_OF_MEMORY);
	  }

	wus->type         = url_type;
	wus->URL_s        = URL_s;
	wus->format_out   = format_out;
	wus->window_id    = context;
	wus->exit_routine = exit_routine;

	/* add "text/ *" to beginning of list so it gets processed first */
	if (CLEAR_CACHE_BIT(format_out) == FO_INTERNAL_IMAGE)
		/* low priority */
		XP_ListAddObjectToEnd(net_waiting_for_connection_url_list, wus);
	else
		/* higher priority */
		XP_ListAddObject(net_waiting_for_connection_url_list, wus);

	return(0);
}

/* returns a malloc'd string that has a bunch of netlib
 * status info in it.
 *
 * please free the pointer when done.
 */
PUBLIC char *
NET_PrintNetlibStatus()
{
	char small_buf[128];
    XP_List * list_ptr;
	ActiveEntry *tmpEntry;
	char *rv=0;

	LIBNET_LOCK();
	list_ptr = net_EntryList;

	sprintf(small_buf, XP_GetString( XP_URLS_WAITING_FOR_AN_OPEN_SOCKET ),
						XP_ListCount(net_waiting_for_connection_url_list),
						NET_MaxNumberOfOpenConnectionsPerContext);
    StrAllocCat(rv, small_buf);

	sprintf(small_buf, XP_GetString( XP_URLS_WAITING_FOR_FEWER_ACTIVE_URLS ),
					XP_ListCount(net_waiting_for_actives_url_list));
    StrAllocCat(rv, small_buf);

	sprintf(small_buf, XP_GetString( XP_CONNECTIONS_OPEN ),
										NET_TotalNumberOfOpenConnections);
    StrAllocCat(rv, small_buf);

	sprintf(small_buf, XP_GetString( XP_ACTIVE_URLS ),
	NET_TotalNumberOfProcessingURLs);
    StrAllocCat(rv, small_buf);

    while((tmpEntry = (ActiveEntry *)XP_ListNextObject(list_ptr)) != 0)
	 {
	   sprintf(small_buf, "------------------------------------\nURL:");
       StrAllocCat(rv, small_buf);
       StrAllocCat(rv, tmpEntry->URL_s->address);
       StrAllocCat(rv, "\n");

	   sprintf(small_buf, XP_GetString(XP_SOCK_CON_SOCK_PROTOCOL),
					tmpEntry->socket, tmpEntry->con_sock, tmpEntry->protocol);
       StrAllocCat(rv, small_buf);
	 }

#if defined(DEBUG) && defined(JAVA)
	{
		static int loggingOn = 0;
		NETLIB->level = (loggingOn ? PR_LOG_NONE : PR_LOG_DEBUG);
		loggingOn = !loggingOn;
	}
#endif /*  defined(DEBUG) && defined(JAVA) */

	LIBNET_UNLOCK();
	return(rv);
}

#define RELEASE_PREFERRED_URLS TRUE
#define RELEASE_PREFERRED_OR_NON_PREFERRED FALSE
#define RELEASE_PREFETCH_URLS TRUE
#define DONT_RELEASE_PREFETCH_URLS FALSE
PRIVATE void
net_release_urls_for_processing(XP_Bool release_prefered, XP_Bool release_prefetch)
{
	XP_List * list_ptr = net_waiting_for_connection_url_list;
    WaitingURLStruct * wus;

	while((wus = (WaitingURLStruct *)
						XP_ListNextObject(list_ptr)) != NULL)
	  {

		if(!release_prefered
		   || ( (wus->format_out != FO_INTERNAL_IMAGE
			    && wus->format_out != FO_CACHE_AND_INTERNAL_IMAGE)
		  ) )
		  {
			/* if the type passed in NOT Prefetch_priority then only
			 * release URL's that are NOT Prefetch_priority
			 */
			if(release_prefetch || wus->URL_s->priority != Prefetch_priority)
			{
				XP_ListRemoveObject(net_waiting_for_connection_url_list, wus);

				/* change prefetch to active prefetch to allow it to load */
				if(wus->URL_s->priority == Prefetch_priority)
					wus->URL_s->priority = CurrentlyPrefetching_priority;

				NET_GetURL(wus->URL_s,
						   wus->format_out,
						   wus->window_id,
						   wus->exit_routine);
				FREE(wus);
				break;
			}
		  }
	  }
}

PRIVATE void
net_release_prefetch_urls_for_processing(void)
{
	/* for now release just one url at a time to give the best possibility
	 * of individual URL's completeing before the user interrupts things
	 */
	net_release_urls_for_processing(RELEASE_PREFERRED_OR_NON_PREFERRED, 
									RELEASE_PREFETCH_URLS);

}

/*
 * Must be called under the cover of the LIBNET_LOCK!
 * 
 * The was_background parameter is used to determine when to call
 * FE_AllConnectionsComplete. It should only be called if we've just
 * completed processing a URL that's *not* a background loading URL.
 * A background URL is one that loads without UI activity e.g. no
 * status bar and thermo activity.
 * This ensures that looping animations don't call 
 * FE_AllConnectionsComplete on every loop cycle.
*/
PRIVATE void
net_CheckForWaitingURL(MWContext * window_id, int protocol, Bool was_background)
{
#ifdef NSPR
	PR_ASSERT(LIBNET_IS_LOCKED());
#endif

	/* decrement here since this function is called
	 * after every exit_routine
	 */
	NET_TotalNumberOfProcessingURLs--;

    if(NET_TotalNumberOfProcessingURLs < 0)
      {
	FE_Alert(window_id, XP_GetString(XP_ALERT_URLS_LESSZERO));
	NET_TotalNumberOfProcessingURLs = 0;
      }

	if(NET_TotalNumberOfOpenConnections < 0)
	  {
		FE_Alert(window_id, XP_GetString(XP_ALERT_CONNECTION_LESSZERO));
	    NET_TotalNumberOfOpenConnections = 0;
	  }

	TRACEMSG(("In: net_CheckForWaitingURL with %d connection waiting URL's, %d active waiting URL's and %d open connections",
	      XP_ListCount(net_waiting_for_connection_url_list),
	      XP_ListCount(net_waiting_for_actives_url_list),
	      NET_TotalNumberOfOpenConnections));

	/* release preferred streams first */
	net_release_urls_for_processing(RELEASE_PREFERRED_URLS,
									DONT_RELEASE_PREFETCH_URLS);

	/* release non-prefered streams */
	net_release_urls_for_processing(RELEASE_PREFERRED_OR_NON_PREFERRED, 
									DONT_RELEASE_PREFETCH_URLS);


    if(!NET_AreThereActiveConnectionsForWindow(window_id))
	  {
		/* if there are prefetch URL's in the queue then we
		 * arn't quite done yet
		 */
		net_release_prefetch_urls_for_processing();

		if(!was_background)
		{
			ET_SendLoadEvent(window_id, EVENT_XFER_DONE, 
					 NULL, NULL, 0,
					 FALSE);
			FE_AllConnectionsComplete(window_id);
		}
	  }

}

PRIVATE int
net_AbortWaitingURL(MWContext * window_id, Bool all, XP_List *list)
{
	XP_List * list_ptr;                     /* Can't initialize here as list might be null */
	XP_List * prev_list_ptr = list;
	XP_List * tmp_list_ptr;
	WaitingURLStruct * wus;
    int32 cur_win_id=0;
	int number_killed = 0;

	/* make sure we actually got a list of things to Abort */
	if (!list)
		return(number_killed);
	
	/* Now initialize the list pointer */
	list_ptr = list->next;
	
	if(!all && window_id)
		cur_win_id = FE_GetContextID(window_id);

	while(list_ptr)
	  {
		wus = (WaitingURLStruct *)list_ptr->object;

		if(all || (window_id && cur_win_id == FE_GetContextID(wus->window_id)))
		  {
			TRACEMSG(("killing waiting URL"));

			/* call exit routine since we are done */
			net_CallExitRoutineProxy(wus->exit_routine,
								wus->URL_s,
								MK_INTERRUPTED,
								wus->format_out,
								wus->window_id);

			number_killed += 1;

			tmp_list_ptr = list_ptr;
			list_ptr = list_ptr->next;

			/* remove the element from the list
			 *
			 * we have to use the function since it does
			 * funky doubly linked list stuff.
			 */
			XP_ListRemoveObject(list, wus);
            net_ReleaseContext(wus->window_id);
			FREE(wus);
		  }
		else
		  {
			prev_list_ptr = list_ptr;
			list_ptr = list_ptr->next;
		  }
	  }

	return(number_killed);
}


/* Helper function for NET_SanityCheckDNS() */
PRIVATE Bool
net_IsHostResolvable (CONST char *hostname, MWContext *context)
{
#ifdef XP_UNIX
	int rv;
	PRHostEnt hpbuf;
	char dbbuf[PR_NETDB_BUF_SIZE];
    if (PR_GetHostByName(hostname, dbbuf, sizeof(dbbuf), &hpbuf) == PR_FAILURE)
	    rv = FALSE;
	else
	    rv = TRUE;
	return rv;
#else
	return(FALSE);
#endif
}

/* Accomodate non-dns OS's like SunOS 4.1.3 */
#if defined(__sun) && !(defined(__svr4__) || defined(__SVR4))
#define MOZ_MAY_HAVE_DNS
#endif

#ifdef MOZ_MAY_HAVE_DNS
/* From xfe/dns-stub.o or xfe/nis-stub.o, depending.  Gag. */
extern int fe_HaveDNS;
#endif


PUBLIC void
NET_SanityCheckDNS (MWContext *context)
{
#ifdef XP_UNIX

  char *proxy = MKhttp_proxy;
  char *socks = NET_SocksHostName;
  char *test_host_1 = "home.netscape.com";
  char *test_host_2 = "home6.netscape.com";
  char *test_host_3 = "internic.net";
  char *message;
#ifdef MOZ_MAY_HAVE_DNS
  char temp[1000];
#endif

  static Bool done;
  if (done) return;
  done = TRUE;

  message = (char *) PR_Malloc (3000);
  if (! message)
	return;
  *message = 0;

  if (proxy)
	proxy = PL_strdup (proxy);
  if (socks)
	socks = PL_strdup (socks);

  /* Strip off everything after last colon. */
  {
    char *s;
    if (proxy && (s = PL_strrchr (proxy, ':')))
	  *s = 0;
    if (socks && (s = PL_strrchr (socks, ':')))
	  *s = 0;
  }

  if (proxy)
	proxy = XP_StripLine (proxy); /* in case it was "hostname:  80" */
  if (socks)
	socks = XP_StripLine (socks);

  /* If the hosts are specified as IP numbers, don't try and resolve them.
     (Yes, hostnames can't begin with digits.) */
  if (proxy && proxy[0] >= '0' && proxy[0] <= '9')
    PR_Free (proxy), proxy = 0;
  if (socks && socks[0] >= '0' && socks[0] <= '9')
    PR_Free (socks), socks = 0;

  if (proxy && *proxy)
    {
      /* If there is an HTTP proxy, then we shouldn't try to resolve any
	 other hosts at all, because they might legitimately be unresolvable.
	 The HTTP proxy will do all lookups.
       */
      if (!net_IsHostResolvable (proxy, context))
	{
		  sprintf(message, XP_GetString(XP_UNKNOWN_HTTP_PROXY), proxy);
	}
    }
  else
    {
      /* There is not an HTTP proxy specified.  So check the other hosts.
       */

      if (socks && *socks && !net_IsHostResolvable (socks, context))
	{
		  sprintf(message, XP_GetString(XP_UNKNOWN_SOCKS_HOST), socks);
#ifdef XP_UNIX
			PL_strcat (message, XP_GetString(XP_SOCKS_NS_ENV_VAR));
		  /* Only Unix has the $SOCKS_NS environment variable. */
#endif /* XP_UNIX */
			PL_strcat (message, XP_GetString(XP_CONSULT_SYS_ADMIN));
	}
      else
	{
	  /* At this point, we're not using a proxy, and either we're not
	     using a SOCKS host or we're using a resolvable SOCKS host.
	     So that means that all the usual host names should be resolvable,
	     and the world is broken if they're not.
	   */
	  char *losers [10];
	  int loser_count = 0;

#ifdef XP_UNIX
	  char local [255], *local2;
	  PRHostEnt *hent, hpbuf;
		  char dbbuf[PR_NETDB_BUF_SIZE];

	  if (gethostname (local, sizeof (local) - 1))
	    local [0] = 0;

	  /* gethostname() and gethostbyname() often return different data -
	     on many systems, the former is basename, the latter is FQDN. */
          local2 = 0;
	  if (local &&
	      (PR_GetHostByName (local, dbbuf, sizeof(dbbuf), &hpbuf) == PR_SUCCESS)) { 
	  hent = &hpbuf;
	  if (PL_strcmp (local, hent->h_name))
	    local2 = PL_strdup (hent->h_name);
      }
	  if (local && *local && !net_IsHostResolvable (local, context))
	    losers [loser_count++] = local;
	  if (local2 && *local2 && !net_IsHostResolvable (local2, context))
	    losers [loser_count++] = local2;
#endif /* XP_UNIX */

	  if (!net_IsHostResolvable (test_host_1, context))
	    losers [loser_count++] = test_host_1;
	  if (!net_IsHostResolvable (test_host_2, context))
	    losers [loser_count++] = test_host_2;
	  if (!net_IsHostResolvable (test_host_3, context))
	    losers [loser_count++] = test_host_3;

	  if (loser_count > 0)
	    {
	      if (loser_count > 1)
		{
		  int i;
		  sprintf(message, XP_GetString(XP_UNKNOWN_HOSTS));
		  for (i = 0; i < loser_count; i++)
		    {
		      PL_strcat (message, "                    ");
		      PL_strcat (message, losers [i]);
		      PL_strcat (message, "\n");
		    }
		}
	      else
		{
				  sprintf(message, XP_GetString(XP_UNKNOWN_HOST), losers[0]);
		}
	      PL_strcat (message, XP_GetString(XP_SOME_HOSTS_UNREACHABLE));
#ifdef XP_UNIX
# ifdef MOZ_MAY_HAVE_DNS       /* compiled on SunOS 4.1.3 */
			  if (fe_HaveDNS)
				/* Don't talk about $SOCKS_NS in the YP/NIS version. */
# endif
				PL_strcat (message, XP_GetString(XP_SOCKS_NS_ENV_VAR));

#ifdef MOZ_MAY_HAVE_DNS        /* compiled on SunOS 4.1.3 */
			  assert (XP_AppName);
			  if (fe_HaveDNS)
				{
					sprintf(temp, XP_GetString(XP_THIS_IS_DNS_VERSION),
						XP_AppName);
				}
			  else
				{
					sprintf(temp, XP_GetString(XP_THIS_IS_YP_VERSION),
						XP_AppName);
				}
			  PL_strcat(message, temp);
# endif /*  SunOS 4.* */
#endif /* XP_UNIX */
	      PL_strcat (message, XP_GetString(XP_CONSULT_SYS_ADMIN));
	    }

#ifdef XP_UNIX
	  if (local2) PR_Free (local2);
#endif /* XP_UNIX */
	}
    }

  if (proxy) PR_Free (proxy);
  if (socks) PR_Free (socks);

  if (*message)
    FE_Alert (context, message);
  PR_Free (message);
#endif /* XP_UNIX full function wrap */
}


/* shutdown the netlibrary, cancel all
 * conections and free all
 * memory
 */
PUBLIC void
NET_ShutdownNetLib(void)
{
    ActiveEntry *tmpEntry;

/* #ifdef XP_WIN */
#if defined(JAVA) || defined(NS_MT_SUPPORTED)
	if (libnet_asyncIO) LIBNET_LOCK();
#else
    LIBNET_LOCK();
#endif

    if(net_waiting_for_actives_url_list) {
	    net_AbortWaitingURL(0, TRUE, net_waiting_for_actives_url_list);
	XP_ListDestroy(net_waiting_for_actives_url_list);
	net_waiting_for_actives_url_list = 0;
    }

    if(net_waiting_for_connection_url_list) {
	net_AbortWaitingURL(0, TRUE, net_waiting_for_connection_url_list);
	XP_ListDestroy(net_waiting_for_connection_url_list);
	net_waiting_for_connection_url_list = 0;
    }

    /* run through list of
     * connections
     */
    while((tmpEntry = (ActiveEntry *)XP_ListRemoveTopObject(net_EntryList)) != 0)
      {

		if(tmpEntry->proto_impl)
		{
			(*tmpEntry->proto_impl->interrupt)(tmpEntry);
		}
		else
		{
			PR_ASSERT(0);
		}

     	/* XP_OS2_FIX IBM-MAS: limit length of output to keep from blowing trace buffer! */
        /* TRACEMSG does sprintf upto the length of the buffer, thus passing %s is ok */
    	TRACEMSG(("End of transfer, entry (soc=%d, con=%d) being removed from list with %d status: %s",
                  tmpEntry->socket, tmpEntry->con_sock, tmpEntry->status, 
                  (tmpEntry->URL_s->address ? tmpEntry->URL_s->address : "none")));

	 	/* call exit routine since we know we are done */
	 	net_CallExitRoutineProxy(tmpEntry->exit_routine,
							 tmpEntry->URL_s,
							 tmpEntry->status,
							 tmpEntry->format_out,
							 tmpEntry->window_id);
        net_ReleaseContext(tmpEntry->window_id);
	 	PR_Free(tmpEntry);  /* free the no longer active entry */
      }

	XP_ListDestroy(net_EntryList);
	net_EntryList = 0;

#ifdef TRUST_LABELS
	/* if there are any URL_s on their list delete them, then delete the list */
	if ( net_URL_sList ) { 
		URL_Struct *tmpURLs;
		XP_List *list_ptr = net_URL_sList;
		while((tmpURLs = (URL_Struct *) XP_ListNextObject(list_ptr)) != NULL) {
			list_ptr = list_ptr->next;		/* get the next object before the current object gets deleted */
			NET_FreeURLStruct( tmpURLs );	/* this will remove it from the list */
		}
		XP_ListDestroy( net_URL_sList );
		net_URL_sList = 0;
	}
#endif

    /* free any memory in the protocol modules
     */
	net_cleanup_reg_protocol_impls();

#ifdef MOZILLA_CLIENT
	NET_SaveCookies("");
#if defined(SingleSignon)
        SI_SaveSignonData("");
#endif
	GH_SaveGlobalHistory();
#endif /* MOZILLA_CLIENT */

	/* purge existing cache files */

    /* free memory in the tcp routines
     */
    NET_CleanupTCP();

#ifdef MOZILLA_CLIENT
    NET_CleanupCache();
	NET_SetMemoryCacheSize(0); /* free memory cache */

#endif /* MOZILLA_CLIENT */

#ifdef XP_UNIX
	NET_CleanupFileFormat(NULL);
#else
	NET_CleanupFileFormat();
#endif

/* #ifdef XP_WIN */
#if defined(JAVA) || defined(NS_MT_SUPPORTED)
    if (libnet_asyncIO) LIBNET_UNLOCK();
#else
    LIBNET_UNLOCK();
#endif
}

static PRBool warn_on_mailto_post = PR_TRUE;

extern void
NET_WarnOnMailtoPost(PRBool warn)
{
	warn_on_mailto_post = warn;
	return;
}

/* use a small static array for speed of traversal.
 * We rely on the fact that we dont expect ton's of 
 * protocol implementations to be dynamically added
 * so a static size is managable for now.
 *
 * URL types should really be dynamic so that we dont have
 * to define types in net.h
 */
#define MAX_NUMBER_OF_PROTOCOL_IMPLS LAST_URL_TYPE

typedef struct {
	NET_ProtoImpl *impl;
	int            url_type;
} net_ProtoImplAndTypeAssoc;

PRIVATE net_ProtoImplAndTypeAssoc net_proto_impls[MAX_NUMBER_OF_PROTOCOL_IMPLS];
PRIVATE net_number_of_proto_impls = 0;

/* registers a protocol impelementation for a particular url_type
 * see NET_URL_Type() for types
 */
PR_IMPLEMENT(void)
 NET_RegisterProtocolImplementation(NET_ProtoImpl *impl, int for_url_type)
{

	if(!impl || for_url_type < 0 || for_url_type > LAST_URL_TYPE)
	{
		PR_ASSERT(0);
		return;
	}

	net_proto_impls[net_number_of_proto_impls].impl = impl;
	net_proto_impls[net_number_of_proto_impls].url_type = for_url_type;
	
	net_number_of_proto_impls++;
}

/* get a handle to a protocol implemenation
 */
NET_ProtoImpl *
net_get_protocol_impl(int for_url_type)
{
	int count=0;

	/* if we ever get around to doing dynamic protocol loading
	 * this would be a good place to plug it in.
	 * just load a DLL with the implementation and
	 * return the handle.
	 * The integer URL_TYPE would need to be replaced with
	 * strings or some other identifier so that it can all be
	 * handled dynamically
	 */

	for(; count < net_number_of_proto_impls; count++)
	{
		if(net_proto_impls[count].url_type == for_url_type)
			return net_proto_impls[count].impl;
	}

/*	PR_ASSERT(0);  *//* should always find one */
	return NULL; 
}

void
net_cleanup_reg_protocol_impls(void)
{
	int count=0;

	for(; count < net_number_of_proto_impls; count++)
	{
        (*net_proto_impls[count].impl->cleanup)();
	}

}

/* register and begin a transfer.
 *
 * returns negative if the transfer errored or finished during the call
 * otherwise returns 0 or greater.
 *
 * A URL, an output format, a window context pointer, and a callback routine
 * should be passed in.
 */

PUBLIC int
NET_GetURL (URL_Struct *URL_s,
	    FO_Present_Types output_format,
	    MWContext *window_id,
	    Net_GetUrlExitFunc* exit_routine)
{
    int          status=MK_MALFORMED_URL_ERROR;
    int          pacf_status=TRUE;
    int          type;
    int          cache_method=0;
    ActiveEntry *this_entry=0;  /* a new entry */
	char        *new_address;
	int processcallbacks = 0;
	Bool confirm;
	Bool load_background;
	char *confirmstring;
	
	if ( get_url_disabled ) {
		/* So that external URL's can't be loaded */
		return -1;
	}

	TRACEMSG(("Entering NET_GetURL"));
	LIBNET_LOCK();


#ifdef XP_WIN
	/* this runs a timer to periodically call the netlib
	 * so that we still get events even when OnIdle is never
	 * called
	 */
	NET_SetNetlibSlowKickTimer(TRUE);
#endif

	PR_ASSERT (URL_s && URL_s->address);
#ifdef MOZILLA_CLIENT
	if ( URL_s->mailto_post && warn_on_mailto_post) {
		confirmstring = NULL;
		confirm = FALSE;
		
		StrAllocCopy(confirmstring, XP_GetString(XP_CONFIRM_MAILTO_POST_1));
		StrAllocCat(confirmstring, XP_GetString(XP_CONFIRM_MAILTO_POST_2));
		
		if ( confirmstring ) {
			confirm = FE_Confirm(window_id, confirmstring);
			PR_Free(confirmstring);
		}
		
		if ( !confirm ) {
			/* abort url
			 */
			net_CallExitRoutineProxy(exit_routine,
								URL_s,
								MK_INTERRUPTED,
								output_format,
								window_id);
            net_ReleaseContext(window_id);
			LIBNET_UNLOCK_AND_RETURN(MK_INTERRUPTED);
		}
	}
#endif /* MOZILLA_CLIENT */     
#ifdef EDITOR
    /* First time here - get our strings out of XP_MSG */
    if (XP_NEW_DOC_URL == NULL) {
		StrAllocCopy(XP_NEW_DOC_URL, XP_GetString(XP_EDIT_NEW_DOC_URL));
    }
    if (XP_NEW_DOC_NAME == NULL) {
		StrAllocCopy(XP_NEW_DOC_NAME, XP_GetString(XP_EDIT_NEW_DOC_NAME));
    }

    if( !window_id->is_new_document ) {
        /* Test for "Untitled" URL */
        if( 0 == PL_strcmp(URL_s->address, XP_NEW_DOC_NAME) ) {
    	    /* Change request to load "file:///Untitled" into "about:editfilenew"  */
	        PR_Free(URL_s->address);
	        URL_s->address = PL_strdup(XP_NEW_DOC_URL);
	        /* Set flag so FE can quickly detect new doc */
	        window_id->is_new_document = TRUE;
        }
        else if( 0 == PL_strcmp(URL_s->address, XP_NEW_DOC_URL) ) {
		    window_id->is_new_document = TRUE;
        }
#if 0
        /* This is bad! We run through here when loading images,
           which shares the context, 
           so we shouldn't clear the new document flag */
        else {
            /* Be sure this is FALSE if not a new document */
            window_id->is_new_document = FALSE;
        }
#endif
    }

    /* Hack to allow special URL for new Editor doc 
     *   and still allow filtering of non-editable doc types
     * All editor GetUrl calls should use FO_EDIT or FO_CACHE_AND_EDIT
     * When we know we have a new document, we change it to FO_CACHE_AND_PRESENT
     *   so it can load the "about:" url without complaining
    */
    if( EDT_IS_EDITOR(window_id) && window_id->is_new_document && 
        (output_format == FO_EDIT || output_format == FO_CACHE_AND_EDIT) ){
        output_format = FO_CACHE_AND_PRESENT;
    }
#endif
#ifdef MOZILLA_CLIENT

		/* Hack. Proxy AutoDiscovery. If we want to use proxy autodiscovery
	     * simply point the MKproxy_ac_url to the MK_padpacURL and use all 
	     * all the same logic. */
		if(!MKproxy_ac_url && NET_UsingPadPac()) {
			NET_SetNoProxyFailover();
			MKproxy_ac_url=MK_padPacURL;
		}

	/* Initialize global/auto proxy codepath. This gets called for every 
	 * NET_GetURL() call. */
	if (
#ifdef MOZ_OFFLINE
		!NET_IsOffline() &&
#endif /* MOZ_OFFLINE */
		/* Do we even have auto config urls set? If not, don't bother
		 * continuing. See mkautocf.c for more info. */
		(MKglobal_config_url || MKproxy_ac_url)
		&& (CLEAR_CACHE_BIT(output_format) == FO_PRESENT)
		&& (!(type = NET_URL_Type(URL_s->address))
		 || type == HTTP_TYPE_URL
		 HG77355
		 || type == GOPHER_TYPE_URL
		 || type == FTP_TYPE_URL
		 || type == WAIS_TYPE_URL
		 || type == URN_TYPE_URL
		 || type == NFS_TYPE_URL
		 || type == POP3_TYPE_URL
		 || (type == NEWS_TYPE_URL && !PL_strncasecmp(URL_s->address, "snews:", 6)))
		 ) {
		int status=-1;
		/* Figure out which auto config we're dealing (global or pac file
		 * with and load it.
		 * If the status is other than -1, the load started and the 
		 * originally requested url will be loaded by the pre_exit_fn()
		 * that is in the proxy auto config URL_Struct. */
		if(MKglobal_config_url && !NET_GlobalAcLoaded) {
			NET_GlobalAcLoaded=TRUE;
			status=NET_LoadProxyConfig(MKglobal_config_url,
										URL_s,
										output_format,
										window_id,
										exit_routine);
		} 
		/* else we know we're dealing with a proxyACL (the other 
		 * possibility in the above if statement) so see if it's 
		 * loaded or not. Also make sure we want to use it (i.e. style is
		 * automatic or if we want to use a proxy autodiscovery url). */
		else if(!NET_ProxyAcLoaded
				&& (   (MKproxy_style == PROXY_STYLE_AUTOMATIC) 
					|| !MKproxy_style
					|| NET_UsingPadPac() ) ) {
			NET_ProxyAcLoaded=TRUE;
			status=NET_LoadProxyConfig(MKproxy_ac_url,
										URL_s,
										output_format,
										window_id,
										exit_routine);
		}
		if(status != -1)
			LIBNET_UNLOCK_AND_RETURN(status);
	} /* End big proxy if */

#endif  /* MOZILLA_CLIENT */

	load_background = URL_s->load_background;

	/* kill leading and trailing spaces in the URL address
	 */
	new_address = XP_StripLine(URL_s->address);
	if(new_address != URL_s->address)
	  {
		memmove(URL_s->address, new_address, PL_strlen(new_address)+1);
	  }

	/* get the protocol type
	 */
    type = NET_URL_Type(URL_s->address);

	if (URL_s->method == URL_HEAD_METHOD &&
		type != HTTP_TYPE_URL HG42421 && type != FILE_TYPE_URL) {
	  /* We can only do HEAD on http connections. */
	  net_CallExitRoutineProxy(exit_routine,
						  URL_s,
						  MK_MALFORMED_URL_ERROR, /* Is this right? ### */
						  output_format,
						  window_id);
	  /* increment since it will get decremented */
	  NET_TotalNumberOfProcessingURLs++;
	  net_CheckForWaitingURL(window_id, 0, load_background);

      net_ReleaseContext(window_id);
	  LIBNET_UNLOCK_AND_RETURN(MK_MALFORMED_URL_ERROR);
	}

    /* XP_OS2_FIX IBM-MAS:limit length of output to keep from blowing trace buffer! */
   TRACEMSG(("Called NET_GetURL with FO: %d URL %-.1900s --", output_format, URL_s->address));
   TRACEMSG(("with method: %d, and post headers: %s", URL_s->method,
            URL_s->post_headers ? URL_s->post_headers : "none"));

	/* if this URL is for prefetching, put it on the wait queue until
	 * everything else is done
	 */
	if(URL_s->priority == Prefetch_priority)
	{
		LIBNET_UNLOCK_AND_RETURN(net_push_url_on_wait_queue(type,
															URL_s,
															output_format,
															window_id,
															exit_routine));
	}

#if defined(SMOOTH_PROGRESS)
    PM_StartBinding(window_id, URL_s);
#endif

	/* put a limit on the total number of active urls
	 */
    if((NET_TotalNumberOfProcessingURLs >= MAX_NUMBER_OF_PROCESSING_URLS) && 
       ((output_format & FO_ONLY_FROM_CACHE) == 0))
	  {
	LIBNET_UNLOCK_AND_RETURN(net_push_url_on_wait_queue(type,
															URL_s,
															output_format,
															window_id,
															exit_routine));
	  }

	/* if we get this far then we should add the URL
	 * to the number of processing URL's
	 */
	NET_TotalNumberOfProcessingURLs++;

	if(type == VIEW_SOURCE_TYPE_URL)
	  {
		/* this is a view-source: URL
		 * strip off the front stuff 
		 */
		char *new_address=0;
		/* the colon is guarenteed to be there */
		StrAllocCopy(new_address, PL_strchr(URL_s->address, ':')+1);
		FREE(URL_s->address);
		URL_s->address = new_address;
		URL_s->allow_content_change = TRUE;

		type = NET_URL_Type(URL_s->address);

		/* remap the format out for the fo_present type
		 */
		if(CLEAR_CACHE_BIT(output_format) == FO_PRESENT)
			output_format = FO_VIEW_SOURCE;
	  }

	if(type == MARIMBA_TYPE_URL)
	  {
		/* If this a castanet URL, and Netcaster is not installed, try http://
		 * Otherwise, we let the castanet protocol handler do its thing.
		 */
		if( ! FE_IsNetcasterInstalled() )
		{
			char *new_address;
			/* Replace castanet:// with http:// */
			/* Allocate space for 'http' + the rest of the string */
			new_address = PR_Malloc(PL_strlen(PL_strchr(URL_s->address, ':'))+5);

			PR_ASSERT(new_address);
			*new_address=0;
			PL_strcat(new_address,"http");
			PL_strcat(new_address,PL_strchr(URL_s->address, ':'));

			FREE(URL_s->address);
			URL_s->address = new_address;

			type = NET_URL_Type(URL_s->address);
		}
	  }

	if(type == NETHELP_TYPE_URL)
	  {
		/* this is a nethelp: URL
		 * separate into its component parts, the mapping file in URL_s->address,
		 * and the topic in URL_s->fe_data
		 */

		if (NET_ParseNetHelpURL(URL_s) == MK_OUT_OF_MEMORY) {
			LIBNET_UNLOCK_AND_RETURN(MK_OUT_OF_MEMORY);
		}
		
		type = NET_URL_Type(URL_s->address);

		output_format = FO_LOAD_HTML_HELP_MAP_FILE;
	  }

#ifdef MOZILLA_CLIENT
	if(
#if defined(MOZ_LITE) && defined(XP_UNIX)
	   (type == NEWS_TYPE_URL && FE_AlternateNewsReader(URL_s->address)) ||
#endif
#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2)
	   FE_UseExternalProtocolModule(window_id, output_format, URL_s, exit_routine) ||
#endif
	   NPL_HandleURL(window_id, output_format, URL_s, exit_routine))
      {
		/* don't call the exit routine since the
		 * External protocol module will call it
		 */
	net_CheckForWaitingURL(window_id, 0, load_background);
 
	LIBNET_UNLOCK_AND_RETURN(-1); /* all done */
      }

	if(NET_TimeBombActive)
	  {
		/* Timebomb has gone off!!
		 *
		 * limit URL's to FTP and anything in our domains
		 */
		if(type != FTP_TYPE_URL
			&& type != ABOUT_TYPE_URL
			 && type != FILE_TYPE_URL
			  && type != MAILBOX_TYPE_URL
			   && type != IMAP_TYPE_URL
			   && type != POP3_TYPE_URL
			    && type != MOCHA_TYPE_URL
			     && type != DATA_TYPE_URL
			  && type != HTML_DIALOG_HANDLER_TYPE_URL
				   && type != HTML_PANEL_HANDLER_TYPE_URL
					HG87237

			  && !PL_strcasestr(URL_s->address, "mcom.com")
			       &&  !PL_strcasestr(URL_s->address, "netscape.com"))
		  {
			char * alert = NET_ExplainErrorDetails(MK_TIMEBOMB_URL_PROHIBIT);

			FE_Alert(window_id, alert);
			FREE(alert);

		/* we need at least an address
		 * if we don't have it exit this routine
	     */
		net_CallExitRoutineProxy(exit_routine,
			    URL_s,
			    MK_TIMEBOMB_URL_PROHIBIT,
							output_format,
			    window_id);

			net_CheckForWaitingURL(window_id, 0, load_background);

            net_ReleaseContext(window_id);
		LIBNET_UNLOCK_AND_RETURN(MK_TIMEBOMB_URL_PROHIBIT);
		  }
	  }

	/* put up security dialog boxes to tell the user
	 * about transitions
	 *
	 * only do this for FO_PRESENT objects and objects
	 * not coming out of history
	 * and also make sure that the object is not being asked
	 * for twice via the "304 use local copy" method
	 */
	if(output_format == FO_CACHE_AND_PRESENT
		&& !URL_s->history_num
		 && !URL_s->use_local_copy)
	  {
		Bool continue_loading_url = TRUE;
		History_entry * h = SHIST_GetCurrent(&window_id->hist);

		/* this is some protection against the "mail document"
		 * feature popping up a dialog warning about an insecure
		 * post.
		 * If the type is MAILTO and the first post header
		 * begins with "Content-type" then it is a post from
		 * inside a form, otherwise it is just a normal MAILTO
		 *
		 * And, of course, anything not a MAILTO link will fall
		 * into the if as well.
		 */
		if(type != MAILTO_TYPE_URL
			|| !URL_s->post_headers
			 || !PL_strncmp("Content-type", URL_s->post_headers, 12))
		  {
		    if(h HG52423)
		      {
				/* if this is not a secure transaction */
			    if(HG76373
					&& !(type == NEWS_TYPE_URL &&
						 toupper(*URL_s->address) == 'S') )
			      {
				    if(URL_s->method == URL_POST_METHOD)
				      {
					    continue_loading_url = HG72388
				      }
				    else if(!URL_s->redirecting_url
							 && type != MAILTO_TYPE_URL
								&& type != ABOUT_TYPE_URL
							       && type != MOCHA_TYPE_URL)
				      {
					    /* don't put up in case of redirect or mocha
					     */
					    continue_loading_url = HG52223
				      }
			      }
		      }
		    else
		      {
				/* put up a dialog for all insecure posts
				 * except news and mail posts
				 */
			    if(URL_s->method == URL_POST_METHOD
					HG53535
					 && type != INTERNAL_NEWS_TYPE_URL
					 && type != NEWS_TYPE_URL
					  && type != HTML_DIALOG_HANDLER_TYPE_URL
					   && type != HTML_PANEL_HANDLER_TYPE_URL
					    && type != MAILTO_TYPE_URL)
		  {
				    continue_loading_url = (Bool)SECNAV_SecurityDialog(window_id, SD_INSECURE_POST_FROM_INSECURE_DOC);
		  }
		      }

            if(!continue_loading_url)
		      {
			    /* abort url
		 */
		net_CallExitRoutineProxy(exit_routine,
									URL_s,
									MK_INTERRUPTED,
									output_format,
									window_id);
		net_CheckForWaitingURL(window_id, 0, load_background);

        net_ReleaseContext(window_id);
		LIBNET_UNLOCK_AND_RETURN(MK_INTERRUPTED);
		      }
		  }
      }
#endif /* MOZILLA_CLIENT */

	/*
	 * if the string is not a valid URL we are just going
     * to punt, so we may as well try it as http
	 */
	if(type==0)
	{
		char *munged;

#define INT_SEARCH_URL "http://cgi.netscape.com/cgi-bin/url_search.cgi?search="
#define INT_SEARCH_URL_TYPE HTTP_TYPE_URL

		if(!(munged = (char*) PR_Malloc(PL_strlen(URL_s->address)+20)))
		{
		    net_CallExitRoutineProxy(exit_routine,
								URL_s,
								MK_OUT_OF_MEMORY,
								output_format,
								window_id);
			
            net_ReleaseContext(window_id);
			LIBNET_UNLOCK_AND_RETURN(MK_OUT_OF_MEMORY);
		}       

#ifdef EDITOR
	/* Some platforms resize a window while creating it. An example is
	 * Windows in Communicator 4.0. On these platforms,
	 * the Composer View Document Source window can end up here with a
	 * bizarre (and potentially localized) URL of "View Document Source".
	 * If we allow this URL to be resolved, it will cause a search engine
	 * to be triggered.
	 * Fortunately, edit_view_source_hack is TRUE iff the context is
	 * a Composer View Document Source window.
	 */
	if ( window_id->edit_view_source_hack ){
	    net_CallExitRoutineProxy(exit_routine,
							    URL_s,
							    MK_INTERRUPTED,
							    output_format,
							    window_id);
        net_ReleaseContext(window_id);
			LIBNET_UNLOCK_AND_RETURN(MK_INTERRUPTED);
	}
#endif

		/* if it starts with a question mark or has a space it's
		 * a search URL
		 */
		if(*URL_s->address == '?' || PL_strchr(URL_s->address, ' '))
		  {
			/* URL contains spaces.  Treat it as search text. */
			char *escaped;

			if(*URL_s->address == '?')
				escaped = NET_Escape(URL_s->address+1, URL_XPALPHAS);
			else
				escaped = NET_Escape(URL_s->address, URL_XPALPHAS);

			if(escaped)
			  {
				char *pUrl;
				FREE(munged);
				if ( (PREF_OK == PREF_CopyCharPref(pref_searchUrl,&pUrl))
                    && pUrl) {
					munged = PR_smprintf("%s%s", pUrl, escaped);
					FREE(escaped);
					PR_Free(pUrl);
				}
			    type = INT_SEARCH_URL_TYPE;
			  }
		  }
		else
		  {
			if(*URL_s->address == '/')
			  {
			    PL_strcpy(munged, "file:");
			    type = FILE_TYPE_URL;
		      }
		    else if(!PL_strncasecmp(URL_s->address, "ftp", 3))
		      {
			    PL_strcpy(munged, "ftp://");
			    type = FTP_TYPE_URL;
		      }
		    else if(!PL_strncasecmp(URL_s->address, "gopher", 6))
		      {
			    PL_strcpy(munged, "gopher://");
			    type = GOPHER_TYPE_URL;
		      }
		    else if(!PL_strncasecmp(URL_s->address, "news", 4)
				    || !PL_strncasecmp(URL_s->address, "nntp", 4))
		      {
			    PL_strcpy(munged, "news://");
			    type = NEWS_TYPE_URL;
		      }
			else
		      {
			    PL_strcpy(munged, "http://");
			    type = HTTP_TYPE_URL;
		      }
    
		    PL_strcat(munged, URL_s->address);
		  }

		PR_Free(URL_s->address);
		URL_s->address = munged;

#ifdef MOZILLA_CLIENT
		/*      Try the external protocol handlers again, since with the
		 *              newly appended protocol, they might work this time.
		 */
		if(
	#if defined(XP_WIN) || defined(XP_MAC)
		   FE_UseExternalProtocolModule(window_id, output_format, URL_s, exit_routine) ||
	#endif
		   NPL_HandleURL(window_id, output_format, URL_s, exit_routine))
	      {
			/* don't call the exit routine since the
			 * External protocol module will call it
			 */
		net_CheckForWaitingURL(window_id, 0, load_background);
 
		LIBNET_UNLOCK_AND_RETURN(-1); /* all done */
	      }
#endif /* MOZILLA_CLIENT */
	}

    if(type == HTTP_TYPE_URL || type == FILE_TYPE_URL ||
		HG73277 || type == GOPHER_TYPE_URL
#ifdef JAVA
#if 0
/* DHIREN */ /* Kludge because castanet URLs don't work when there's no Java */
		|| type == MARIMBA_TYPE_URL
#endif
#endif
		)
	add_slash_to_URL(URL_s);

#ifdef MOZILLA_CLIENT
	/*
	 * If this is a resize-reload OR a view-source URL, try to use the
	 * URL's wysiwyg: counterpart.
	 */
	URL_s->resize_reload = (URL_s->force_reload == NET_RESIZE_RELOAD);
	if(URL_s->wysiwyg_url &&
	   (URL_s->resize_reload == TRUE ||
		CLEAR_CACHE_BIT(output_format) == FO_VIEW_SOURCE))
	  {
		StrAllocCopy(URL_s->address, URL_s->wysiwyg_url);
		type = WYSIWYG_TYPE_URL;
	  }

	/* the FE's were screwing up the use of force_reload
     * to get around a bug in the scroll to named anchor
	 * code.  So I gave them an enum NET_RESIZE_RELOAD
	 * that they could use instead.  NET_RESIZE_RELOAD
	 * should be treated just like NET_DONT_RELOAD within
	 * the netlib
	 */
	if((URL_s->force_reload == NET_RESIZE_RELOAD) ||
       (URL_s->force_reload == NET_CACHE_ONLY_RELOAD))
		URL_s->force_reload = NET_DONT_RELOAD;

	/* Before looking in the cache, make sure we check the
	 * special cases to know if
	 * we're allowed to use modified content.
	 * Two cases so far: a view-source URL, and
	 * any non-internal URL.  Non-internal URLs are
	 * the result of link clicks, drag-and-drop, etc.
	 */
	if ((CLEAR_CACHE_BIT(output_format) == FO_VIEW_SOURCE) ||
		(!URL_s->internal_url &&
		 (CLEAR_CACHE_BIT(output_format) != FO_MAIL_TO)))
		URL_s->allow_content_change = TRUE;
    /* check for the url in the cache
     */
	if (type != FILE_TYPE_URL)
		cache_method = NET_FindURLInCache(URL_s, window_id);

	if (!cache_method)
	  {
        TIMING_MESSAGE(("cache,%.64s,%08x,not found", URL_s->address, window_id));

		/* cache testing stuff */
		if(NET_IsCacheTraceOn())
		  {
			char *buf = 0;
			StrAllocCopy(buf, XP_GetString(XP_URL_NOT_FOUND_IN_CACHE));
			StrAllocCat(buf, URL_s->address);
			FE_Alert(window_id, buf);
			FREE(buf);
		  }

		/* if wysiwyg, there must be a cache entry or we retry the real url */
		if(type == WYSIWYG_TYPE_URL)
		  {
			const char *real_url = LM_SkipWysiwygURLPrefix(URL_s->address);

			/* XXX can't use StrAllocCopy because it frees dest first */
			if (real_url && (new_address = PL_strdup(real_url)) != NULL)
			  {
				/* cache miss: we must clear this flag so scripts rerun */
				URL_s->resize_reload = FALSE;

				PR_Free(URL_s->address);
				URL_s->address = new_address;
				FREE_AND_CLEAR(URL_s->wysiwyg_url);
				/* 
		 		* Since the call to NET_GetURL will increment the
		 		* following counter again, we decrement it so as not
		 		* to overcount.
		 		*/
				NET_TotalNumberOfProcessingURLs--;
				LIBNET_UNLOCK_AND_RETURN(
							NET_GetURL(URL_s, output_format, window_id, exit_routine)
						);
			  }
		  }
	  }

    /* if cache_method is non zero then we have this URL cached.  Use
     * the bogus cache url type
	 *
	 * This nasty bit of logic figures out if we really
	 * need to reload it or if we can use the cached copy
     */
    else
      {
		if(URL_s->use_local_copy || output_format & FO_ONLY_FROM_CACHE)
		  {
	    /* the cached file is valid so use it unilaterally
	     */
        TIMING_MESSAGE(("cache,%.64s,%08x,valid", URL_s->address, window_id));
	    type = cache_method;
		  }
		else if(URL_s->real_content_length > URL_s->content_length)
		  {
			/* cache testing stuff */
			if(NET_IsCacheTraceOn())
			  {
				char *buf = 0;
				StrAllocCopy(buf, XP_GetString(XP_PARTIAL_CACHE_ENTRY));
				StrAllocCat(buf, URL_s->address);
				FE_Alert(window_id, buf);
				FREE(buf);
			  }

#ifdef NU_CACHE
            URL_s->cache_object = 0;
#else
	    URL_s->memory_copy = 0;
#endif
	    cache_method = 0;
	    TRACEMSG(("Getting the rest of a partial cache file"));
        TIMING_MESSAGE(("cache,%.64s,%08x,partial", URL_s->address, window_id));
		  }
	else if (URL_s->force_reload != NET_DONT_RELOAD)
	  {
	    TRACEMSG(("Force reload flag set.  Checking server to see if modified"));
        TIMING_MESSAGE(("cache,%.64s,%08x,forced reload",
                        URL_s->address, window_id));

	    /* cache testing stuff */
	    if(NET_IsCacheTraceOn())
	      {
		char *buf = 0;
		StrAllocCopy(buf, XP_GetString(XP_CHECKING_SERVER__FORCE_RELOAD));
		StrAllocCat(buf, URL_s->address);
		FE_Alert(window_id, buf);
		FREE(buf);
	      }

	    /* strip the cache file and
	     * memory pointer
	     */
	    if (type == WYSIWYG_TYPE_URL)
		type = cache_method;
	    else
	      {
		FREE_AND_CLEAR(URL_s->cache_file);
#ifdef NU_CACHE
                URL_s->cache_object = 0;
#else
		URL_s->memory_copy = 0;
#endif
		cache_method = 0;
	      }
	  }
	else if(URL_s->expires)         
	  {
	    time_t cur_time = time(NULL);

	    if(cur_time > URL_s->expires)
	      {
		FREE_AND_CLEAR(URL_s->cache_file);
#ifdef NU_CACHE
                URL_s->cache_object = 0;
#else
		URL_s->memory_copy = 0;
#endif
		URL_s->expires = 0;  /* remove cache reference */
		cache_method = 0;

        TIMING_MESSAGE(("cache,%.64s,%08x,expired",
                        URL_s->address, window_id));

		/* cache testing stuff */
		if(NET_IsCacheTraceOn())
		  {
		    char *buf = 0;
		    StrAllocCopy(buf, XP_GetString(XP_OBJECT_HAS_EXPIRED));
		    StrAllocCat(buf, URL_s->address);
		    FE_Alert(window_id, buf);
		    FREE(buf);
		  }
	      }
			else
			  {
			/* the cached file is valid so use it unilaterally
			 */
        TIMING_MESSAGE(("cache,%.64s,%08x,valid",
                        URL_s->address, window_id));

		type = cache_method;
			  }
		  }
		else if((NET_CacheUseMethod == CU_NEVER_CHECK || URL_s->history_num)
				  && !URL_s->expires)
		  {
        TIMING_MESSAGE(("cache,%.64s,%08x,valid",
                        URL_s->address, window_id));

	    type = cache_method;
		  }
		else if(NET_CacheUseMethod == CU_CHECK_ALL_THE_TIME
			    && CLEAR_CACHE_BIT(output_format) == FO_PRESENT
				  && (type == HTTP_TYPE_URL HG73738)
					  && !URL_s->expires)
		  {
			/* cache testing stuff */
			if(NET_IsCacheTraceOn())
			  {
				char *buf = 0;
				StrAllocCopy(buf, XP_GetString(XP_CHECKING_SERVER_CACHE_ENTRY));
				StrAllocCat(buf, URL_s->address);
				FE_Alert(window_id, buf);
				FREE(buf);
			  }

			 /* if it's an HTTP URL and its not an Internal image
			  * and it's not being asked for with ONLY FROM CACHE
			  * and it's not coming out of the history
			  * and it doesn't have an expiration date...
			  * FORCE Reload it
			  */
        TIMING_MESSAGE(("cache,%.64s,%08x,forced reload:non-history http",
                        URL_s->address, window_id));

	    TRACEMSG(("Non history http file found. Force reloading it "));
	    /* strip the cache file and
	     * memory pointer
	     */
	    FREE_AND_CLEAR(URL_s->cache_file);
#ifdef NU_CACHE
            URL_s->cache_object = 0;
#else
	    URL_s->memory_copy = 0;
#endif
	    cache_method = 0;

		  }
		else if(!URL_s->last_modified
				&& CLEAR_CACHE_BIT(output_format) == FO_PRESENT
				&& (type == HTTP_TYPE_URL HG62522) 
#ifdef MOZ_OFFLINE            
	    && !NET_IsOffline() 
#endif /* MOZ_OFFLINE */
	    )  /* *X* check for is offline */
		  {
            TIMING_MESSAGE(("cache,%.64s,%08x,forced reload:non-history cgi",
                            URL_s->address, window_id));

			TRACEMSG(("Non history cgi script found (probably)."
					  " Force reloading it "));
			/* cache testing stuff */
			if(NET_IsCacheTraceOn())
			  {
				char *buf = 0;
				StrAllocCopy(buf, XP_GetString(XP_CHECKING_SERVER__LASTMOD_MISS));
				StrAllocCat(buf, URL_s->address);
				FE_Alert(window_id, buf);
				FREE(buf);
			  }

	    /* strip the cache file and
	     * memory pointer
	     */
	    FREE_AND_CLEAR(URL_s->cache_file);
#ifdef NU_CACHE
            URL_s->cache_object = 0;
#else
	    URL_s->memory_copy = 0;
#endif
		URL_s->expires = 0;  /* remove cache reference */
	    cache_method = 0;
		  }
		else
		  {
		    /* the cached file is valid so use it unilaterally
		     */
        TIMING_MESSAGE(("cache,%.64s,%08x,valid", URL_s->address, window_id));

	    type = cache_method;
		  }
      }
#endif /* MOZILLA_CLIENT */

	/*
	 * If the object should only come out of the cache
	 * we should abort now if there is not a cache object
	 */
    if(output_format & FO_ONLY_FROM_CACHE)
      {
		TRACEMSG(("object called with ONLY_FROM_CACHE format out"));

		if(!cache_method)
		  {
			net_CallExitRoutineProxy(exit_routine,
								URL_s,
								MK_OBJECT_NOT_IN_CACHE,
								output_format,
								window_id);
			net_CheckForWaitingURL(window_id, type, load_background);

            net_ReleaseContext(window_id);
			LIBNET_UNLOCK_AND_RETURN(MK_OBJECT_NOT_IN_CACHE);
		  }
		else
		  {
			/* remove the ONLY_FROM_CACHE designation and replace
			 * it with CACHE_AND.. so that we go through the cacheing
			 * module again so that we can memory cache disk objects
			 * and do the right thing in general
			 */
			output_format = CLEAR_PRESENT_BIT(output_format,FO_ONLY_FROM_CACHE);
			output_format = SET_PRESENT_BIT(output_format,FO_CACHE_ONLY);
		  }
      }

    /* put a limit on the total number of open connections
     */
	if(!net_is_one_url_allowed_to_run(window_id, type))
	  {
		NET_TotalNumberOfProcessingURLs--; /* waiting not processing */
		LIBNET_UNLOCK_AND_RETURN(net_push_url_on_wait_queue(type,
															URL_s,
															output_format,
															window_id,
															exit_routine));
	  }

    /* start a new entry */
    this_entry = PR_NEW(ActiveEntry);
    if(!this_entry)
	  {
	net_CallExitRoutineProxy(exit_routine,
							URL_s,
							MK_OUT_OF_MEMORY,
							output_format,
							window_id);

        net_ReleaseContext(window_id);
		LIBNET_UNLOCK_AND_RETURN(MK_OUT_OF_MEMORY);
	  }

    /* init new entry */
    memset(this_entry, 0, sizeof(ActiveEntry));
    this_entry->URL_s        = URL_s;
    this_entry->socket       = NULL;
    this_entry->con_sock     = NULL;
    this_entry->exit_routine = exit_routine;
    this_entry->window_id    = window_id;
	this_entry->protocol     = type;

    /* set the format out for the entry
     */
    this_entry->format_out = output_format;

	/* set it busy */
	this_entry->busy = TRUE;

	/* add it to the processing list now
	 */
    XP_ListAddObjectToEnd(net_EntryList, this_entry);

    /* Set the timer to call NET_ProcessNet after we
     * have submitted a network request.
     * BUG #123826
     */
#if defined(XP_UNIX)
	/* temporarily use busy poll to ease transition */
	NET_SetCallNetlibAllTheTime(window_id, "mkgeturl");
#endif



	/* this will protect against multiple posts unknown to the
	 * user
	 *
	 * check if the method is post and it came from the
	 * history and it isn't coming from the cache
	 */
	if(URL_s->method == URL_POST_METHOD
		&& !URL_s->expires
		 && !cache_method
		  && URL_s->history_num)
	  {
		if(URL_s->force_reload != NET_DONT_RELOAD)
		  {
			if(!FE_Confirm(window_id, XP_GetString(XP_CONFIRM_REPOST_FORMDATA)))
			  {
				XP_ListRemoveObject(net_EntryList, this_entry);
				net_CallExitRoutineProxy(exit_routine,
									URL_s,
									MK_INTERRUPTED,
									output_format,
									window_id);
				/* will get decremented */
				net_CheckForWaitingURL(window_id, this_entry->protocol, 
									   load_background);

                net_ReleaseContext(window_id);
				PR_Free(this_entry);  /* not needed any more */

				LIBNET_UNLOCK_AND_RETURN(MK_INTERRUPTED);
			  }
			/* otherwise fall through and repost
			 */
		  }
		else
		  {
			NET_StreamClass * stream;
			char buffer[1000];

			StrAllocCopy(URL_s->content_type, TEXT_HTML);
			stream =  NET_StreamBuilder(CLEAR_CACHE_BIT(output_format),
										URL_s,
										window_id);
			if(stream)
			  {
				PL_strcpy(buffer, XP_GetString(XP_HTML_MISSING_REPLYDATA));
				(*stream->put_block)(stream,
									 buffer,
									 PL_strlen(buffer));
				(*stream->complete)(stream);
			  }

			XP_ListRemoveObject(net_EntryList, this_entry);

			net_CallExitRoutineProxy(exit_routine,
								URL_s,
								MK_INTERRUPTED,
								output_format,
								window_id);
			net_CheckForWaitingURL(window_id, this_entry->protocol,
								   load_background);

            net_ReleaseContext(window_id);
			PR_Free(this_entry);  /* not needed any more */

			LIBNET_UNLOCK_AND_RETURN(MK_INTERRUPTED);
		  }
	  }

redo_load_switch:   /* come here on file/ftp retry */

	/* This code path handles the case when a pac/global file has been loaded
	 * and we now want to use it in for a request. If we find a proxy/socks
	 * server to use, we call NET_HTTPLoad here, skipping the below switch.
	 * See mkautocf.c for more info. */
	pacf_status = TRUE;
	if ((NET_ProxyAcLoaded || NET_GlobalAcLoaded) 
		&& (this_entry->protocol == HTTP_TYPE_URL
		 HG62363
		 || this_entry->protocol == GOPHER_TYPE_URL
		 || this_entry->protocol == FTP_TYPE_URL
		 || this_entry->protocol == WAIS_TYPE_URL
		 || this_entry->protocol == URN_TYPE_URL
		 || this_entry->protocol == NFS_TYPE_URL
		 || (this_entry->protocol == NEWS_TYPE_URL 
			 && !PL_strncasecmp(URL_s->address, "snews:", 6)
			)
		)
		&& ((this_entry->proxy_conf =
		     pacf_find_proxies_for_url(window_id, URL_s)) != NULL)
		&& (pacf_status = pacf_get_proxy_addr(window_id,
							 this_entry->proxy_conf,
							 &this_entry->proxy_addr,
							 &this_entry->socks_host,
							 &this_entry->socks_port)) != 0
		&& this_entry->proxy_addr
		)
	  {
		TRACEMSG(("PAC returned \"%s\" for \"%s\".", this_entry->proxy_conf, URL_s->address));
		HG24242

		/* Everything else except SNEWS gets loaded by HTTP loader,
		 * including HTTPS. */
		if (this_entry->protocol != NEWS_TYPE_URL) {
			this_entry->proto_impl = net_get_protocol_impl(HTTP_TYPE_URL);
		} else {
			this_entry->proto_impl = net_get_protocol_impl(NEWS_TYPE_URL);
		}

		if(this_entry->proto_impl)
			status = (*this_entry->proto_impl->init)(this_entry);
	  }
	else if ( pacf_status == FALSE && NET_GetNoProxyFailover() == TRUE ) 
	  {
		status = MK_UNABLE_TO_LOCATE_PROXY;
		FE_Alert(window_id, XP_GetString(XP_BAD_AUTOCONFIG_NO_FAILOVER));
      } 
	else
	  {
        char *proxy_address = NULL;
        /* If this url struct indicates that it doesn't want to use a proxy,
         * skip it. */
        if (!this_entry->URL_s->bypassProxy) {
            proxy_address = NET_FindProxyHostForUrl(type, this_entry->URL_s->address);
        }
  		  if(proxy_address)
			{
				this_entry->protocol = HTTP_TYPE_URL;
				this_entry->proxy_addr = proxy_address;
			}
			
		  this_entry->proto_impl = net_get_protocol_impl(this_entry->protocol);

		  if(this_entry->proto_impl)
		  {
			status = (*this_entry->proto_impl->init)(this_entry);
		  }
		  else
		  {
				/* bad url */
				URL_s->error_msg =
					NET_ExplainErrorDetails(MK_MALFORMED_URL_ERROR,
											this_entry->URL_s->address);
				status = MK_MALFORMED_URL_ERROR;
				this_entry->status = MK_MALFORMED_URL_ERROR;
		  }
	}

	/* the entry is no longer busy
	 */
	this_entry->busy = FALSE;

#ifdef MOZILLA_CLIENT
    /* OH NASTY A GOTO!
     * If we went into load file and what we really wanted to do
     * was use FTP, LoadFile will return MK_USE_FTP_INSTEAD so
     * we will go back up to the beginning of the switch and
     * use ftp instead
     */
    if(status == MK_USE_FTP_INSTEAD)
      {
		type = FTP_TYPE_URL;
	    this_entry->protocol = type; /* change protocol designator */
		this_entry->status = 0; /* reset */
		goto redo_load_switch;
      }
	else 
#endif /* MOZILLA_CLIENT */
	if(this_entry->status == MK_USE_COPY_FROM_CACHE)
	  {
	    TRACEMSG(("304 Not modified recieved using cached entry\n"));

#ifdef MOZILLA_CLIENT
	    NET_RefreshCacheFileExpiration(this_entry->URL_s);
#endif /* MOZILLA_CLIENT */

		/* turn off force reload by telling it to use local copy
		 */
		this_entry->URL_s->use_local_copy = TRUE;

	    /* restart the transfer
	     */
		XP_ListRemoveObject(net_EntryList, this_entry);
	    status = NET_GetURL(this_entry->URL_s,
				   this_entry->format_out,
				   this_entry->window_id,
				   this_entry->exit_routine);

		net_CheckForWaitingURL(this_entry->window_id,
							   this_entry->protocol,
							   this_entry->URL_s->load_background);

		PR_Free(this_entry);
		LIBNET_UNLOCK_AND_RETURN(0);
	  }
	else if(this_entry->status == MK_DO_REDIRECT)
	  {
		/* this redirect should just call GetURL again
		 */
		int status;
		XP_ListRemoveObject(net_EntryList, this_entry);
		status =NET_GetURL(this_entry->URL_s, 
						   this_entry->format_out,
						   this_entry->window_id,
						   this_entry->exit_routine);
		net_CheckForWaitingURL(this_entry->window_id, 
							   this_entry->protocol,
							   load_background);
		PR_Free(this_entry);  /* not needed any more */
		LIBNET_UNLOCK_AND_RETURN(0);
	  }
    else if(this_entry->status == MK_TOO_MANY_OPEN_FILES)
      {
		/* Queue this URL so it gets tried again
		 */
		XP_ListRemoveObject(net_EntryList, this_entry);
		status = net_push_url_on_wait_queue(
						NET_URL_Type(this_entry->URL_s->address),
						this_entry->URL_s,
						this_entry->format_out,
						this_entry->window_id,
						this_entry->exit_routine);
		NET_TotalNumberOfProcessingURLs--;
		PR_Free(this_entry);
		LIBNET_UNLOCK_AND_RETURN(status);
      }
    else if(this_entry->status == MK_OFFLINE)
      {
		/*  Stop the stars and put up a nice message
		 */
		XP_ListRemoveObject(net_EntryList, this_entry);
		{
			if (window_id->type != MWContextMail && window_id->type != MWContextMailMsg &&
				(CLEAR_CACHE_BIT(output_format) != FO_INTERNAL_IMAGE) &&
				(CLEAR_CACHE_BIT(output_format) != FO_EMBED) &&
				(CLEAR_CACHE_BIT(output_format) != FO_PLUGIN) &&
				(CLEAR_CACHE_BIT(output_format) != FO_DO_JAVA))
			{
				/* We only display the message for top-level items (i.e., not for
				   inline images, plugins, or java classes */
				   
				char * alert = PL_strdup(XP_GetString(XP_ALERT_OFFLINE_MODE_SELECTED));
				FE_Alert(window_id, alert);
				FREE(alert);
			}
		}
		
		net_CallExitRoutineProxy(this_entry->exit_routine,
							this_entry->URL_s,
							this_entry->status,
							this_entry->format_out,
							this_entry->window_id);


		/* Check for WaitingURL decrements TotalNumberofProcessingURLS */
		net_CheckForWaitingURL(this_entry->window_id, 
							   this_entry->protocol,
							   load_background);

        net_ReleaseContext(this_entry->window_id);
		PR_Free(this_entry);
		LIBNET_UNLOCK_AND_RETURN(MK_OFFLINE);
      }

    if(status < 0)  /* check for finished state */
      {

        /* XP_OS2_FIX IBM-MAS: limit length of output string to keep from blowing trace buffer */
      TRACEMSG(("End of transfer, entry (soc=%d, con=%d) being removed from list with %d status: %-.1900s",
             this_entry->socket, this_entry->con_sock, this_entry->status,
             this_entry->URL_s->address));

		XP_ListRemoveObject(net_EntryList, this_entry);

		net_CallExitRoutineProxy(this_entry->exit_routine,
							this_entry->URL_s,
							this_entry->status,
							this_entry->format_out,
							this_entry->window_id);
		net_CheckForWaitingURL(this_entry->window_id, 
							   this_entry->protocol,
							   load_background);
        net_ReleaseContext(this_entry->window_id);
		PR_Free(this_entry);  /* not needed any more */
      }

    TRACEMSG(("Leaving GetURL with %d items in list",
			  XP_ListCount(net_EntryList)));

	/* XXX - hack for chromeless windows - jsw 10/27/95 */
	LIBNET_UNLOCK();
	if ( processcallbacks ) {
		NET_ProcessExitCallbacks();
	}
	
	return status;

#ifdef NOTDEF /* this is the real code here */
    LIBNET_UNLOCK_AND_RETURN(status);
#endif
}

/* this thing is a hack that will allow http streams
   waiting on passwd auth to resume XXX */
PUBLIC void NET_ResumeWithAuth (void *closure)
{
    XP_List * iter = NULL;
    ActiveEntry * tmpEntry = NULL;
    NET_AuthClosure *auth_closure = (NET_AuthClosure *) closure;

    /* check to see if our entry is in the list */
    if (XP_ListIsEmpty(net_EntryList)) {
      TRACEMSG (("NET_ResumeWithAuth: no list! we're busted ..."));
      return;
    }
    iter = net_EntryList;

    while ((tmpEntry = (ActiveEntry *) XP_ListNextObject(iter))) {
      if (tmpEntry == (ActiveEntry *) auth_closure->_private) {
        break;
      }
    }

    if (!tmpEntry)
        return;

    if (auth_closure && auth_closure->pass && *auth_closure->pass) {

      TRACEMSG (("NET_ResumeWithAuth: auth succeeded: %s", tmpEntry->URL_s->address));

      /* now try it again */
      if (tmpEntry->proto_impl->resume)
          tmpEntry->proto_impl->resume (tmpEntry, auth_closure, PR_TRUE);

    } else {
      TRACEMSG (("NET_ResumeWithAuth(): auth failed: %s", tmpEntry->URL_s->address));
      if (tmpEntry->proto_impl->resume)
          tmpEntry->proto_impl->resume (tmpEntry, auth_closure, PR_FALSE);
    }

    /* now free the closure struct */
    PR_Free(auth_closure);
    return;
}

/* process_net is called from the client's main event loop and
 * causes connections to be read and processed.  Multiple
 * connections can be processed simultaneously.
 */
PUBLIC int NET_ProcessNet (PRFileDesc *ready_fd,  int fd_type) {
    ActiveEntry * tmpEntry;
    XP_List * list_item;
    int rv= -1;
    Bool load_background;

#ifdef XP_OS2_FIX
    /* assume no local files in net_EntryList.
     * see "Thrash Optomization", below */
    int localfiles = 0;
#endif

    TRACEMSG(("Entering ProcessNet!  ready_fd: %d", ready_fd));
    LIBNET_LOCK();

    if(XP_ListIsEmpty(net_EntryList)) {
        TRACEMSG(("Invalid call to NET_ProcessNet with fd: %d - No active entries\n", ready_fd));

#ifdef MOZILLA_CLIENT
        if (fd_type == NET_EVERYTIME_TYPE) {
            MWContext *c = XP_FindContextOfType(0, MWContextBrowser);
            if (!c) c = XP_FindContextOfType(0, MWContextMail);
            if (!c) c = XP_FindContextOfType(0, MWContextNews);
            if (!c) c = XP_FindContextOfType(0, MWContextMessageComposition);
            if (!c) c = XP_FindContextOfType(0, MWContextMailMsg);
            if (!c) c = XP_FindContextOfType(0, MWContextPane);

            if (c) {
                if(NET_IsCallNetlibAllTheTimeSet(c, NULL)) {	
                    NET_ClearCallNetlibAllTheTime(c, "mkgeturl");
                }
                NET_SetNetlibSlowKickTimer(FALSE);
            }
        }
#endif /* MOZILLA_CLIENT */

        LIBNET_UNLOCK_AND_RETURN(0); /* no entries to process */
    } /* XP_ListIsEmpty(net_EntryList) */

    if(NET_InGetHostByName) {
        TRACEMSG(("call to processnet while doing gethostbyname call"));
        PR_ASSERT(0);
        LIBNET_UNLOCK_AND_RETURN(1);
    }

    /* if NULL is passed into ProcessNet use select to
     * figure out if a socket is ready */
    if(ready_fd == NULL) {
        /* try and find a socket ready for reading */
#define MAX_SIMULTANIOUS_SOCKETS 100
        /* should never have more than MAX sockets */
        PRPollDesc poll_desc_array[MAX_SIMULTANIOUS_SOCKETS];
        unsigned int fd_set_size=0;

        /* reorder the list so that we get a round robin effect */
        XP_ListMoveTopToBottom(net_EntryList);

        fd_type = NET_SOCKET_FD;

        /* process one socket ready for reading */
        list_item = net_EntryList;
        while((tmpEntry = (ActiveEntry *) XP_ListNextObject(list_item)) != 0)
        {
            if(tmpEntry->busy)
                continue;

            if(!tmpEntry->local_file && !tmpEntry->memory_file)
            {
                if (tmpEntry->socket != NULL) {
                    if(tmpEntry->con_sock) {
                        poll_desc_array[fd_set_size].fd = tmpEntry->con_sock;
                        poll_desc_array[fd_set_size].in_flags = PR_POLL_READ | PR_POLL_EXCEPT | PR_POLL_WRITE; 
                    } else {
                        poll_desc_array[fd_set_size].fd = tmpEntry->socket;
                        poll_desc_array[fd_set_size].in_flags = PR_POLL_READ | PR_POLL_EXCEPT; 
                    }

                    fd_set_size++;
                    PR_ASSERT(fd_set_size < MAX_SIMULTANIOUS_SOCKETS);
                }
            } else if(tmpEntry == 
                        (ActiveEntry *) XP_ListGetObjectNum(net_EntryList, 1))
            {
                /* if this is the very first object in the list
                 * and it's a local file or a memory cache copy
                 * then use this one entry and skip the select call
                 * we won't deadlock because we reorder the list
                 * with every call to net process net. */
                fd_type = NET_LOCAL_FILE_FD;
                ready_fd = tmpEntry->socket;  /* it's actually going to be NULL in this case */

                /* call yeild to let other thread do something */
                PR_Sleep(PR_INTERVAL_NO_WAIT);

                break;  /* exit while() */
            }
        } /* end while() */

        /* in case we set it for the local file type above */
        if(fd_type == NET_SOCKET_FD) {
            int count=0;

            PR_Sleep(PR_INTERVAL_NO_WAIT); /* thread yeild */

            if( PR_Poll(poll_desc_array, fd_set_size, 0) < 1 ) {
                TRACEMSG(("Select returned no active sockets! WASTED CALL TO PROCESS NET"));
                LIBNET_UNLOCK_AND_RETURN(XP_ListIsEmpty(net_EntryList) ? 0 : 1);
            }

            /* process one socket ready for reading,
             * find the first one ready */
            list_item = net_EntryList;
            while((tmpEntry=(ActiveEntry *) XP_ListNextObject(list_item)) != 0)
            {
                if(tmpEntry->busy)
                    continue;

                if(!tmpEntry->local_file && !tmpEntry->memory_file) {
                    /* count should line up to the appropriate socket since
                     * it was added in the same order */
                    PR_ASSERT(poll_desc_array[count].fd == tmpEntry->socket
                                || poll_desc_array[count].fd == tmpEntry->con_sock);
                    if(poll_desc_array[count].out_flags & (PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT))
                    {
                        ready_fd = tmpEntry->socket;
                        break;
                    }
                    count++;
                }
            } /* end while() */

			if(!ready_fd) {
                /* couldn't find the active socket.  Shouldn't ever happen */
                PR_ASSERT(0);
                LIBNET_UNLOCK_AND_RETURN(XP_ListIsEmpty(net_EntryList) ? 0 : 1);
            }
        } /* end if(fd_type == NET_SOCKET_FD) */
    } /* end if(ready_fd == NULL)*/

    list_item = net_EntryList;

    /* process one socket ready for reading
     * find the ready socket in the active entry list */
    while((tmpEntry = (ActiveEntry *) XP_ListNextObject(list_item)) != 0)
    {
        TRACEMSG(("Found item in Active Entry List. sock #%d  con_sock #%d",
                    tmpEntry->socket, tmpEntry->con_sock));

        /* this will activate local and memory files as well since the ready_fd
         * will be zero and will match the NULL socket ID.
         *
         * We won't have starvation because the list is reordered every time
         */
        if(!tmpEntry->busy 
            &&
            (ready_fd == tmpEntry->socket
            || ready_fd == tmpEntry->con_sock) ) {

            tmpEntry->busy = TRUE;

            TRACEMSG(("Item has data ready for read"));

            rv = (*tmpEntry->proto_impl->process)(tmpEntry);

            tmpEntry->busy = FALSE;

            /* check for done status on transfer and call
             * exit routine if done. */
            if (rv < 0) {

                XP_ListRemoveObject(net_EntryList, tmpEntry);

                if(tmpEntry->status == MK_USE_COPY_FROM_CACHE) {
                    TRACEMSG(("304 Not modified recieved using cached entry\n"));
#ifdef MOZILLA_CLIENT
                    NET_RefreshCacheFileExpiration(tmpEntry->URL_s);
#endif /* MOZILLA_CLIENT */

                    /* turn off force reload by telling it to use local copy */
                    tmpEntry->URL_s->use_local_copy = TRUE;

                    /* restart the transfer */
                    NET_GetURL(tmpEntry->URL_s,
                        tmpEntry->format_out,
                        tmpEntry->window_id,
                        tmpEntry->exit_routine);

                    net_CheckForWaitingURL(tmpEntry->window_id,
                                            tmpEntry->protocol,
                                            tmpEntry->URL_s->load_background);
                } else if(tmpEntry->status == MK_DO_REDIRECT) {
                    TRACEMSG(("Doing redirect part in ProcessNet"));

                    /* restart the whole transfer */
                    NET_GetURL(tmpEntry->URL_s,
                        tmpEntry->format_out,
                        tmpEntry->window_id,
                        tmpEntry->exit_routine);

                    net_CheckForWaitingURL(tmpEntry->window_id,
                                            tmpEntry->protocol,
                                            tmpEntry->URL_s->load_background);                  
                } else if(tmpEntry->status == MK_TOO_MANY_OPEN_FILES) {
					/* Queue this URL so it gets tried again */
                    LIBNET_UNLOCK_AND_RETURN(net_push_url_on_wait_queue(
                        NET_URL_Type(tmpEntry->URL_s->address),
                        tmpEntry->URL_s,
                        tmpEntry->format_out,
                        tmpEntry->window_id,
                        tmpEntry->exit_routine));
                }
#ifdef NU_CACHE
                else if(tmpEntry->status < 0
                            && !tmpEntry->URL_s->use_local_copy
                            HG42469
                        && (tmpEntry->status == MK_CONNECTION_REFUSED
                            || tmpEntry->status == MK_CONNECTION_TIMED_OUT
                            || tmpEntry->status == MK_UNABLE_TO_CREATE_SOCKET
                            || tmpEntry->status == MK_UNABLE_TO_LOCATE_HOST
                            || tmpEntry->status == MK_UNABLE_TO_CONNECT)
                            && NET_IsURLInCache(tmpEntry->URL_s))
#else
                else if(tmpEntry->status < 0
                            && !tmpEntry->URL_s->use_local_copy
                            HG42469
                        && (tmpEntry->status == MK_CONNECTION_REFUSED
                            || tmpEntry->status == MK_CONNECTION_TIMED_OUT
                            || tmpEntry->status == MK_UNABLE_TO_CREATE_SOCKET
                            || tmpEntry->status == MK_UNABLE_TO_LOCATE_HOST
                            || tmpEntry->status == MK_UNABLE_TO_CONNECT)
                        && (NET_IsURLInDiskCache(tmpEntry->URL_s)
                            || NET_IsURLInMemCache(tmpEntry->URL_s)))
#endif
                {
                    /* if the status is negative something went wrong
                     * with the load.  If last_modified is set
                     * then we probably have a cache file,
                     * but it might be a broken image so
                     * make sure "use_local_copy" is not
                     * set.
                     *
                     * Only do this when we can't connect to the
                     * server. */

                    /* if we had a cache file and got a load
                     * error, go ahead and use it anyways */

                    /* turn off force reload by telling it to use local copy */
                    tmpEntry->URL_s->use_local_copy = TRUE;

                    if(CLEAR_CACHE_BIT(tmpEntry->format_out) == FO_PRESENT)
                    {
                        StrAllocCat(tmpEntry->URL_s->error_msg,
                        XP_GetString( XP_USING_PREVIOUSLY_CACHED_COPY_INSTEAD ) );

                        FE_Alert(tmpEntry->window_id,
                                tmpEntry->URL_s->error_msg);
                    }

                    FREE_AND_CLEAR(tmpEntry->URL_s->error_msg);

                    /* restart the transfer */
                    NET_GetURL(tmpEntry->URL_s,
                                tmpEntry->format_out,
                                tmpEntry->window_id,
                                tmpEntry->exit_routine);

                    net_CheckForWaitingURL(tmpEntry->window_id,
                                            tmpEntry->protocol,
                                            tmpEntry->URL_s->load_background);
                } else {
                    /* XP_OS2_FIX IBM-MAS: limit size of URL string to 100 to keep from blowing trace message buffer! */
                    TRACEMSG(("End of transfer, entry (soc=%d, con=%d) being removed from list with %d status: %-.1900s",
                            tmpEntry->socket, tmpEntry->con_sock, tmpEntry->status,
                            (tmpEntry->URL_s->address ? tmpEntry->URL_s->address : "")));

                    /* catch out of memory errors at the lowest
                     * level since we don't do it at all the out
                     * of memory condition spots */
                    if(tmpEntry->status == MK_OUT_OF_MEMORY
                            && !tmpEntry->URL_s->error_msg) {
                        tmpEntry->URL_s->error_msg =
                            NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
                    }

                    load_background = tmpEntry->URL_s->load_background;
                    /* run the exit routine */
                    net_CallExitRoutineProxy(tmpEntry->exit_routine,
                                            tmpEntry->URL_s,
                                            tmpEntry->status,
                                            tmpEntry->format_out,
                                            tmpEntry->window_id);

                    net_CheckForWaitingURL(tmpEntry->window_id,
                                            tmpEntry->protocol,
                                            load_background);

#ifdef MILAN
                    /* if the error was caused by a NETWORK DOWN
                     * then interrupt the window.  This
                     * could still be a problem since another
                     * window may be active, but it should get
                     * the same error */
                    if(PR_GetOSError() == XP_ERRNO_ENETDOWN) {
                        NET_SilentInterruptWindow(tmpEntry->window_id);
                    }
#endif /* MILAN */

                    net_ReleaseContext(tmpEntry->window_id);
                } /* end status variable if statements */

                PR_Free(tmpEntry);  /* free the now non active entry */

            } /* end if  rv < 0 */
            
            TRACEMSG(("Leaving process net with %d items in list",
                        XP_ListCount(net_EntryList)));

            LIBNET_UNLOCK_AND_RETURN(XP_ListIsEmpty(net_EntryList) ? 0 : 1); /* all done */

        } /* end if !tmpEntry->busy */

    } /* end while */

    /* the active socket wasn't found in the list :( */
    TRACEMSG(("Invalid call to NET_ProcessNet: Active item with passed in fd: %d not found\n", ready_fd));

    LIBNET_UNLOCK_AND_RETURN(XP_ListIsEmpty(net_EntryList) ? 0 : 1);
}

/*
 *      net_InterruptActiveStream
 *      Kills a single stream given its ActiveEntry*
 *      *       FrontEndCancel -> NetInterruptWindow -> net_InterruptActiveStream
 *      *       LayoutNewDoc -> KillOtherLayouts -> net_InterruptActiveStream
 *
 *      Also removes the entry from the active list and frees it.
 *      Also allows waiting urls into the active list.
 */
PRIVATE int
net_InterruptActiveStream (ActiveEntry *entry, Bool show_warning)
{
	TRACEMSG(("Terminating transfer on port #%d, proto: %d",
								entry->socket, entry->protocol));

	/* remove it from the active list first to prevent
	 * reentrant problem
	 */
	XP_ListRemoveObject(net_EntryList, entry);

	if(entry->proto_impl)
	{
		(*entry->proto_impl->interrupt)(entry);
	}
	else
	{
		PR_ASSERT(0);
	}

    /* XP_OS2_FIX IBM-MAS: limit length of output string to keep from blowing trace buffer */
   TRACEMSG(("End of transfer, entry (soc=%d, con=%d) being removed from list with %d status: %-.1900s",
           entry->socket, entry->con_sock, entry->status, entry->URL_s->address));

	/* call exit routine since we know we are done */
	net_CallExitRoutineProxy(entry->exit_routine,
						entry->URL_s,
						entry->status,
						entry->format_out,
						entry->window_id);

	/* decrement here since we don't call checkForWaitingURL's
	 */
	NET_TotalNumberOfProcessingURLs--;

	if(!NET_AreThereActiveConnectionsForWindow(entry->window_id))
		FE_AllConnectionsComplete(entry->window_id);

	/* free the no longer active entry */
    net_ReleaseContext(entry->window_id);
	PR_Free(entry);

	return 0;
}

/* use a URL struct to find an active entry
 * The netlib lock must be held prior to calling this
 * returns NULL if not found
 */
PRIVATE ActiveEntry *
net_find_ac_from_url(URL_Struct *nurl)
{
    XP_List * iter = net_EntryList;
	ActiveEntry *tmpEntry = NULL, *rv = NULL;

    while ((tmpEntry = (ActiveEntry *) XP_ListNextObject(iter)) != NULL) 
	  {
		if (tmpEntry->URL_s == nurl) 
		{
			rv = tmpEntry;
			break;
		}
      }

	return(rv);
}

PUBLIC Bool
NET_SetActiveEntryBusyStatus(URL_Struct *nurl, Bool set_busy)
{
	ActiveEntry *entry = NULL;

	entry = net_find_ac_from_url(nurl);

	if(entry)
		entry->busy = set_busy;

	return (entry != 0);
}

/*
 *      NET_InterruptStream kills just stream associated with an URL.
 */
PUBLIC int
NET_InterruptStream (URL_Struct *nurl)
{
	/* Find the ActiveEntry structure for this URL */
	ActiveEntry *entryToKill = NULL;
	int status;
	
	TRACEMSG(("Entering NET_InterruptStream"));
	LIBNET_LOCK();

	if(NET_InGetHostByName)
	  {
		TRACEMSG(("call to InterrruptStream while doing gethostbyname call"));
		LIBNET_UNLOCK_AND_RETURN(1);
	  }

	entryToKill = net_find_ac_from_url(nurl);

/*    assert (entryToKill);*/
    /* Kill it & free it */
	if (entryToKill)
	    status = net_InterruptActiveStream (entryToKill, TRUE);
	else
	    status = -1;

	LIBNET_UNLOCK_AND_RETURN(status);
}


/* interrupt a LoadURL action by using a socket number as
 * an index.  NOTE: that this cannot interrupt non-socket
 * based operations such as file or memory cache loads
 *
 * returns -1 on error.
 */
PUBLIC int
NET_InterruptSocket (PRFileDesc *socket)
{
    /* Find the ActiveEntry structure for this URL */
    ActiveEntry *tmpEntry = NULL, *entryToKill = NULL;
    XP_List * iter;
    int status;

    TRACEMSG(("Entering NET_InterruptSocket"));
    LIBNET_LOCK();
    iter = net_EntryList;

    if(NET_InGetHostByName)
      {
	TRACEMSG(("call to InterrruptStream while doing gethostbyname call"));
	LIBNET_UNLOCK_AND_RETURN(1);
      }

    while ((tmpEntry = (ActiveEntry *) XP_ListNextObject(iter)) != NULL) {
	if (tmpEntry->con_sock == socket || tmpEntry->socket == socket) {
	    entryToKill = tmpEntry;
	    break;
	}
    }

	if (entryToKill)
	    status = net_InterruptActiveStream (entryToKill, TRUE);
	else
	    status = -1;

	LIBNET_UNLOCK_AND_RETURN(status);
}

/*
 *  NET_SetNewContext changes the context associated with a URL Struct.
 *      can also change the url exit routine, which causes the old one to be
 *      called with a status of MK_CHANGING_CONTEXT
 */
PUBLIC int
NET_SetNewContext(URL_Struct *URL_s, MWContext * new_context, Net_GetUrlExitFunc *exit_routine)
{
    /* Find the ActiveEntry structure for this URL */
    ActiveEntry *tmpEntry = NULL;
    XP_List * iter;
	MWContext *old_window_id;

	LIBNET_LOCK();
	iter = net_EntryList;

    while ((tmpEntry = (ActiveEntry *) XP_ListNextObject(iter)) != NULL)
	  {
	if (tmpEntry->URL_s == URL_s)
		  {
#if defined(SMOOTH_PROGRESS)
              /* When the URL load gets redirected to another window,
                 we need to notify the progress manager from the old
                 window that the URL need no longer be tracked */
            PM_StopBinding(tmpEntry->window_id, URL_s, -1, NULL);
#endif
			/* call the old exit routine now with the old context */
			/* call with MK_CHANGING_CONTEXT, FE shouldn't free URL_s */
			net_CallExitRoutineProxy(tmpEntry->exit_routine,
								URL_s,
								MK_CHANGING_CONTEXT,
								0,
								tmpEntry->window_id);

			/*      see if we're changing exit routines */
			if(exit_routine != NULL)        {
				/*      okay to change now. */
				tmpEntry->exit_routine = exit_routine;
			}
			old_window_id = tmpEntry->window_id;
	    tmpEntry->window_id = new_context;

#if defined(SMOOTH_PROGRESS)
            /* Inform the progress manager in the new context about
               the URL that it just received */
            PM_StartBinding(tmpEntry->window_id, URL_s);
#endif

#ifdef XP_WIN
			/*
			 *      Windows needs a way to know when a URL switches contexts,
			 *              so that it can keep the NCAPI progress messages
			 *              specific to a URL loading and not specific to the
			 *              context attempting to load.
			 *      So sue me.  GAB 10-16-95
			 */
			FE_UrlChangedContext(URL_s, old_window_id, new_context);
#endif /* XP_WIN */

			/*      if there are no more connections in the old context, we
			 *      need to call AllConnectionsComplete
			 */
		if(!NET_AreThereActiveConnectionsForWindow(old_window_id))
				FE_AllConnectionsComplete(old_window_id);

        net_ReleaseContext(old_window_id);
			LIBNET_UNLOCK();
	    return(0);
	  }
      }

	/* couldn't find it :( */
    PR_ASSERT (0);

	LIBNET_UNLOCK();
    return(-1);
}

/* returns true if the stream is safe for setting up a new
 * context and using a separate window.
 *
 * This will return FALSE in multipart/mixed cases where
 * it is impossible to use separate windows for the
 * same stream.
 */
PUBLIC Bool
NET_IsSafeForNewContext(URL_Struct *URL_s)
{
	if(!URL_s || URL_s->is_active)
		return(FALSE);

	return(TRUE);
}


/* NET_InterruptTransfer
 *
 * Interrupts all transfers in progress that have the same window id as
 * the one passed in.
 */
PRIVATE int
net_InternalInterruptWindow(MWContext * window_id, Bool show_warning)
{
    int starting_list_count;
    ActiveEntry * tmpEntry;
    ActiveEntry * tmpEntry2;
    XP_List * list_item;
    int32 cur_win_id = FE_GetContextID(window_id);
	int number_killed=0;
	XP_Bool call_all_connections_complete = TRUE;

	TRACEMSG(("-------Interrupt Transfer called!"));

	LIBNET_LOCK();
	list_item = net_EntryList;
	starting_list_count = XP_ListCount(net_EntryList);

#ifdef DEBUG
	if(NET_InGetHostByName)
	  {
		TRACEMSG(("call to InterrruptWindow while doing gethostbyname call"));
		LIBNET_UNLOCK_AND_RETURN(1);
	  }
#endif /* DEBUG */

	number_killed = net_AbortWaitingURL(window_id,
										FALSE,
										net_waiting_for_actives_url_list);
	number_killed += net_AbortWaitingURL(window_id,
										FALSE,
										net_waiting_for_connection_url_list);

    /* run through the whole list of connections and
     * interrupt any of them that have a matching window
     * id.
     */
	tmpEntry = (ActiveEntry *) XP_ListNextObject(list_item);

    while(tmpEntry)
      {
	/* advance to the next item NOW in case we free this one */
	tmpEntry2 = (ActiveEntry *) XP_ListNextObject(list_item);

	if(FE_GetContextID(tmpEntry->window_id) == cur_win_id)
	  {
		    if(tmpEntry->busy)
		      {
				if(show_warning)
				  {
				TRACEMSG(("AAAACK, reentrant protection kicking in.!!!"));
#ifdef DEBUG
				FE_Alert(tmpEntry->window_id, XP_GetString(XP_ALERT_INTERRUPT_WINDOW));
#endif /* DEBUG */
				  }
		      }
			else
		      {
				FE_EnableClicking(window_id);
				net_InterruptActiveStream (tmpEntry, show_warning);
				number_killed += 1;

				/* unset call_all_connections_complete here
				 * since InterruptActiveStream will already
				 * call all_connections_complete when it detects
				 * that there are no more active connections
				 * going on for the window.
				 */
				call_all_connections_complete = FALSE;
		      }
	  } /* end if cur_win_id */

		tmpEntry = tmpEntry2;
      } /* end while */

	/* If all_connection_complete was called, FE might have destroyed
	 * the context. So dont call FE_EnableClicking() if
	 * call_all_connections_complete was already called.
	 */
	if (call_all_connections_complete)
		FE_EnableClicking(window_id);

	/* we are sure that all connections are complete
	 * but only call it if we killed one.
	 */
	if(number_killed && call_all_connections_complete)
	FE_AllConnectionsComplete(window_id);


    TRACEMSG(("Leaving Interrupt transfer with %d items in list",
				XP_ListCount(net_EntryList)));

    LIBNET_UNLOCK_AND_RETURN(starting_list_count - XP_ListCount(net_EntryList));

}

/*
 * Interrupts all transfers in progress that have the same window id as
 * the one passed in.
 */
PUBLIC int
NET_InterruptWindow(MWContext * window_id)
{
/* call interruptwindow twice so that things we interrupt that call NET_GetURL
 * will be interrupted as well
 */
	
    int rv = net_InternalInterruptWindow(window_id, TRUE);
    #ifdef XP_MAC
    /* pchen - Fix bug #72831. On Mac, continuously call
     * net_InternalInterruptWindow while
     * NET_AreThereNonBusyActiveConnectionsForWindow returns true
     */
    while(NET_AreThereNonBusyActiveConnectionsForWindow(window_id))
    #endif /* XP_MAC */
    rv += net_InternalInterruptWindow(window_id, TRUE);
    return(rv);
}

/*
 * Silently Interrupts all transfers in progress that have the same
 * window id as the one passed in.
 */
MODULE_PRIVATE int
NET_SilentInterruptWindow(MWContext * window_id)
{
    return(net_InternalInterruptWindow(window_id, FALSE));
}

/* check for any active URL transfers in progress for the given
 * window Id
 *
 * It is possible that there exist URL's on the wait queue that
 * will not be returned by this function.  It is recommended
 * that you call NET_InterruptWindow(MWContext * window_id)
 * when FALSE is returned to make sure that the wait queue is
 * cleared as well.
 * 
 * It is also possible that some URLs will be explicitly marked as
 * "background" URLs that are not identified as active, even though
 * they are.
 */
PUBLIC Bool
NET_AreThereActiveConnectionsForWindow(MWContext * window_id)
{
    ActiveEntry * tmpEntry;
    int32 cur_win_id = FE_GetContextID(window_id);
    XP_List * list_ptr;
    WaitingURLStruct * wus;

	LIBNET_LOCK();

    TRACEMSG(("-------NET_AreThereActiveConnectionsForWindow called!"));

	/* check for connections in the wait queue
	 */
    list_ptr = net_waiting_for_actives_url_list;
    while((wus = (WaitingURLStruct*) XP_ListNextObject(list_ptr)) != NULL)
      {
	if(cur_win_id == FE_GetContextID(wus->window_id) &&
	   !wus->URL_s->load_background)
		  {
			LIBNET_UNLOCK();
			return(TRUE);
		  }
      }

    /* check for connections in the connections wait queue
     */
	list_ptr = net_waiting_for_connection_url_list;
    while((wus = (WaitingURLStruct*) XP_ListNextObject(list_ptr)) != NULL)
      {
	if(cur_win_id == FE_GetContextID(wus->window_id) &&
	   !wus->URL_s->load_background)
	  {
	    LIBNET_UNLOCK();
	    return(TRUE);
	  }
      }

    /* run through the whole list of active connections and
     * return true if any of them have the passed in window id
     */
	list_ptr = net_EntryList;
    while((tmpEntry = (ActiveEntry *) XP_ListNextObject(list_ptr)) != NULL)
      {
	if(cur_win_id == FE_GetContextID(tmpEntry->window_id) &&
	   !tmpEntry->URL_s->load_background)
		  {
			LIBNET_UNLOCK();
	    return(TRUE);
		  }

      } /* end while */

	LIBNET_UNLOCK();
    return(FALSE);
}


PUBLIC Bool
NET_AreThereActiveConnections()
{
    Bool ret_value;
	LIBNET_LOCK();
	ret_value = XP_ListIsEmpty(net_waiting_for_actives_url_list) &&
                XP_ListIsEmpty(net_waiting_for_connection_url_list) &&
                XP_ListIsEmpty(net_EntryList);
	LIBNET_UNLOCK();
    return !ret_value;
}


#ifdef XP_MAC
/* pchen - Fix bug #72831. Same as NET_AreThereActiveConnectionsForWindow
 * except that it will return false if there is an active connection that
 * is busy.
 */
PUBLIC Bool
NET_AreThereNonBusyActiveConnectionsForWindow(MWContext * window_id)
{
    ActiveEntry * tmpEntry;
    int32 cur_win_id = FE_GetContextID(window_id);
    XP_List * list_ptr;
    WaitingURLStruct * wus;

	LIBNET_LOCK();

    TRACEMSG(("-------NET_AreThereActiveConnectionsForWindow called!"));

	/* check for connections in the wait queue
	 */
    list_ptr = net_waiting_for_actives_url_list;
    while((wus = (WaitingURLStruct*) XP_ListNextObject(list_ptr)) != NULL)
      {
	if(cur_win_id == FE_GetContextID(wus->window_id) &&
	   !wus->URL_s->load_background)
		  {
			LIBNET_UNLOCK();
			return(TRUE);
		  }
      }

    /* check for connections in the connections wait queue
     */
	list_ptr = net_waiting_for_connection_url_list;
    while((wus = (WaitingURLStruct*) XP_ListNextObject(list_ptr)) != NULL)
      {
	if(cur_win_id == FE_GetContextID(wus->window_id) &&
	   !wus->URL_s->load_background)
	  {
	    LIBNET_UNLOCK();
	    return(TRUE);
	  }
      }

    /* run through the whole list of active connections and
     * return true if any of them have the passed in window id
     */
	list_ptr = net_EntryList;
    while((tmpEntry = (ActiveEntry *) XP_ListNextObject(list_ptr)) != NULL)
      {
	if(cur_win_id == FE_GetContextID(tmpEntry->window_id) &&
	   !tmpEntry->URL_s->load_background &&
	   !tmpEntry->busy)
		  {
			LIBNET_UNLOCK();
	    return(TRUE);
		  }

      } /* end while */

	LIBNET_UNLOCK();
    return(FALSE);
}
#endif /* XP_MAC */

/* KM - Are there other ActiveEntry structures being processed with the same
	context? */
/* fix Mac warning for missing prototype */
PUBLIC Bool
NET_AreThereActiveConnectionsForWindowWithOtherActiveEntry(ActiveEntry *thisEntry);

PUBLIC Bool
NET_AreThereActiveConnectionsForWindowWithOtherActiveEntry(ActiveEntry *thisEntry)
{
    ActiveEntry * tmpEntry;
    int32 cur_win_id = FE_GetContextID(thisEntry->window_id);
    XP_List * list_ptr;

	LIBNET_LOCK();
	
    /* run through the whole list of active connections and
     * return true if any of them have the passed in window id
     * with a different entry
     */
	list_ptr = net_EntryList;
    while((tmpEntry = (ActiveEntry *) XP_ListNextObject(list_ptr)) != NULL)
      {
	if((cur_win_id == FE_GetContextID(tmpEntry->window_id)) &&
	   (tmpEntry != thisEntry))
		  {
			LIBNET_UNLOCK();
	    return(TRUE);
		  }

      } /* end while */

	LIBNET_UNLOCK();
    return(FALSE);
}


/*
 *  GAB 04-23-96
 *  We're interested in knowing if there are any operations which
 *      will cause activity of the said type, in the said context.
 *  This is in the interests of PE, where they desire a world of
 *      dial on demand (hanging up the phone when we're not doing anything
 *      on the network).
 *
 *  Args:
 *      window_id   The said context to check.
 *                  If NULL, check them all (disregard).
 *      waiting     Include waiting queues in check if TRUE.
 *                  Note that waiting queues do not know thier type
 *                      yet (local, cache, or net), thus type can not
 *                      be correctly checked.  I can only assume that
 *                      we have the technology to mostly decide this
 *                      before the fact, but I see no facility available.
 *      background  Include background activity in the check if TRUE.
 *  Returns:
 *      Bool        TRUE, there is chance of activity of said type.
 *                  FALSE, that particular type of activity is not present.
 *
 */
PUBLIC Bool
NET_HasNetworkActivity(MWContext * window_id, Bool waiting, Bool background)
{
    ActiveEntry * tmpEntry;
    XP_List * list_ptr;
    WaitingURLStruct * wus;

	LIBNET_LOCK();

    TRACEMSG(("-------NET_HasNetworkActivity called!"));

	/* check for connections in the wait queue
	 */
    if(waiting)
      {
	list_ptr = net_waiting_for_actives_url_list;
	while((wus = (WaitingURLStruct*) XP_ListNextObject(list_ptr)) != NULL)
	  {
	    if((window_id == NULL ||
		FE_GetContextID(window_id) ==
		FE_GetContextID(wus->window_id)) &&
		(background || !wus->URL_s->load_background))
		      {
			    LIBNET_UNLOCK();
			    return(TRUE);
		      }
	  }

	/* check for connections in the connections wait queue
	 */
	    list_ptr = net_waiting_for_connection_url_list;
	while((wus = (WaitingURLStruct*) XP_ListNextObject(list_ptr)) != NULL)
	  {
	    if((window_id == NULL ||
		FE_GetContextID(window_id) ==
		FE_GetContextID(wus->window_id)) &&
		(background || !wus->URL_s->load_background))
	      {
		LIBNET_UNLOCK();
		return(TRUE);
	      }
	  }
      }

    /* run through the whole list of active connections
     */
	list_ptr = net_EntryList;
    while((tmpEntry = (ActiveEntry *) XP_ListNextObject(list_ptr)) != NULL)
      {
	if((window_id == NULL ||
	    FE_GetContextID(window_id) ==
	    FE_GetContextID(tmpEntry->window_id)) &&
	    (background || !tmpEntry->URL_s->load_background) &&
	    !tmpEntry->memory_file &&
	    !tmpEntry->local_file)
		  {
			LIBNET_UNLOCK();
	    return(TRUE);
		  }

      } /* end while */

	LIBNET_UNLOCK();
    return(FALSE);
}


/* malloc, init, and set the URL structure used
 * for the network library
 */
PUBLIC URL_Struct * 
NET_CreateURLStruct (CONST char *url, NET_ReloadMethod force_reload)
{
    uint32 all_headers_size;
    URL_Struct * URL_s  = PR_NEW(URL_Struct);

    if(!URL_s)
	  {
		return NULL;
	  }

    /* zap the whole structure */
    memset (URL_s, 0, sizeof (URL_Struct));

    URL_s->SARCache = NULL;

    /* we haven't modified the address at all (since we are just creating it)*/
    URL_s->address_modified=NO;

	URL_s->method = URL_GET_METHOD;

    URL_s->force_reload = force_reload;

	URL_s->load_background = FALSE;

    URL_s->ref_count = 1;

    /* Allocate space for pointers to hold message (http, news etc) headers */
    all_headers_size = 
	INITIAL_MAX_ALL_HEADERS * sizeof (URL_s->all_headers.key[0]);
    URL_s->all_headers.key = (char **) PR_Malloc(all_headers_size);
    if(!URL_s->all_headers.key)
      {
	NET_FreeURLStruct(URL_s);
	return NULL;
      }
    memset (URL_s->all_headers.key, 0, all_headers_size);

    all_headers_size = 
	INITIAL_MAX_ALL_HEADERS * sizeof (URL_s->all_headers.value[0]);
    URL_s->all_headers.value = (char **) PR_Malloc(all_headers_size);
    if(!URL_s->all_headers.value)
      {
	NET_FreeURLStruct(URL_s);
	return NULL;
      }
    memset (URL_s->all_headers.value, 0, all_headers_size);

    URL_s->all_headers.max_index = INITIAL_MAX_ALL_HEADERS;

	URL_s->owner_data = NULL;
	URL_s->owner_id   = 0;

    /* set the passed in value */
    StrAllocCopy(URL_s->address, url);
	if (!URL_s->address) {
		/* out of memory, clean up previously allocated objects */
		NET_FreeURLStruct(URL_s);
		URL_s = NULL;
	}

#ifdef TRUST_LABELS
	/* add the newly minted URL_s struct to the list of the same so
	 * that when cookie to trust label matching occurs I can find the
	 * associated trust list */
	if ( URL_s && net_URL_sList ) {
		LIBNET_LOCK();
		XP_ListAddObjectToEnd( net_URL_sList, URL_s );
		LIBNET_UNLOCK();
	}
#endif

    return(URL_s);
}

/* Increase the size of AllHeaders in URL_struct
 */
PRIVATE Bool 
net_EnlargeURLAllHeaders (URL_Struct * URL_s)
{
    uint32 number_of_entries = 
	URL_s->all_headers.empty_index * MULT_ALL_HEADER_COUNT;
    uint32 realloc_size = 
	number_of_entries * sizeof (URL_s->all_headers.key[0]);
    char **temp;

    if (number_of_entries < MAX_ALL_HEADER_COUNT) {

	temp = (char **) PR_Realloc(URL_s->all_headers.key, realloc_size);
	if (temp) {
	    URL_s->all_headers.key = temp;

	    temp = (char **) PR_Realloc(URL_s->all_headers.value, realloc_size);
	    if (temp) {
		URL_s->all_headers.value = temp;

		URL_s->all_headers.max_index = number_of_entries;
		return(TRUE);
	    }
	}
    }
    net_FreeURLAllHeaders(URL_s);
    return(FALSE);
}


/* Append Key, Value pair into AllHeaders list of URL_struct
 */
PUBLIC Bool 
NET_AddToAllHeaders(URL_Struct * URL_s, char *name, char *value)
{
    char *key_ptr;
    char *value_ptr;

    PR_ASSERT(URL_s);
    PR_ASSERT(name);
    PR_ASSERT(value);

    if ((URL_s->all_headers.empty_index >= URL_s->all_headers.max_index) &&
	(!net_EnlargeURLAllHeaders(URL_s))) {
	return(FALSE);
    }

    key_ptr = URL_s->all_headers.key[URL_s->all_headers.empty_index] =
	PL_strdup(name);
    if (!key_ptr) {
	net_FreeURLAllHeaders(URL_s);
	return(FALSE);
	}

    value_ptr = URL_s->all_headers.value[URL_s->all_headers.empty_index] = 
	PL_strdup(value);
    if (!value_ptr) {
	PR_Free(key_ptr);
	URL_s->all_headers.key[URL_s->all_headers.empty_index] = NULL;
	net_FreeURLAllHeaders(URL_s);
	return(FALSE);
	}
    URL_s->all_headers.empty_index++;
    return(TRUE);
}

/* Set the option IPAddressString field
 * This field overrides the hostname when doing the connect, 
 * and allows Java to specify a spelling of an IP number.
 * Out of memory condition will cause the URL_struct to be destroyed.
 */
PUBLIC int
NET_SetURLIPAddressString (URL_Struct * URL_s, CONST char *ip_string)
{
    PR_ASSERT(URL_s);

    FREEIF(URL_s->IPAddressString);
    URL_s->IPAddressString = NULL;

    /* set the passed in value */
    StrAllocCopy(URL_s->IPAddressString, ip_string);
    return NULL == URL_s->IPAddressString;
}

PUBLIC URL_Struct *
NET_HoldURLStruct (URL_Struct * URL_s)
{
    if(URL_s) {
	PR_ASSERT(URL_s->ref_count > 0);
	URL_s->ref_count++;
    }
    return URL_s;
}


/* free the contents and the URL structure when finished
 */
PUBLIC void
NET_FreeURLStruct (URL_Struct * URL_s)
{
    if(!URL_s)
	return;
    
    /* if someone is holding onto a pointer to us don't die */
    PR_ASSERT(URL_s->ref_count > 0);
    URL_s->ref_count--;
    if(URL_s->ref_count > 0)
	return;

#ifdef TRUST_LABELS
	/* remove URL_s struct from the list of the same */
	if ( URL_s && net_URL_sList ) {
		LIBNET_LOCK();
	   	XP_ListRemoveObject( net_URL_sList, URL_s );
		LIBNET_UNLOCK();
	}
#endif

    FREEIF(URL_s->address);
    FREEIF(URL_s->username);
    FREEIF(URL_s->password);
    FREEIF(URL_s->IPAddressString);
    FREEIF(URL_s->referer);
    FREEIF(URL_s->post_data);
    FREEIF(URL_s->post_headers);
    FREEIF(URL_s->content_type);
    FREEIF(URL_s->content_encoding);
    FREEIF(URL_s->x_mac_type);
    FREEIF(URL_s->x_mac_creator);
	FREEIF(URL_s->charset);
	FREEIF(URL_s->boundary);
    FREEIF(URL_s->redirecting_url);
	FREEIF(URL_s->authenticate);
	FREEIF(URL_s->protection_template);
    FREEIF(URL_s->http_headers);
	FREEIF(URL_s->cache_file);
    FREEIF(URL_s->window_target);
	FREEIF(URL_s->window_chrome);
	FREEIF(URL_s->refresh_url);
	FREEIF(URL_s->wysiwyg_url);
	FREEIF(URL_s->etag);
	FREEIF(URL_s->origin_url);
	FREEIF(URL_s->error_msg);

    HG87376

    /* Free all memory associated with header information */
    net_FreeURLAllHeaders(URL_s);

	if(URL_s->files_to_post)
	  {
		/* free a null terminated array of filenames
		 */
		int i=0;
		for(i=0; URL_s->files_to_post[i] != NULL; i++)
		  {
			FREE(URL_s->files_to_post[i]); /* free the filenames */
		  }
		FREE(URL_s->files_to_post); /* free the array */
	  }

    /* Like files_to_post. */
	if(URL_s->post_to)
	  {
		/* free a null terminated array of filenames
		 */
		int i=0;
		for(i=0; URL_s->post_to[i] != NULL; i++)
		  {
			FREE(URL_s->post_to[i]); /* free the URLs */
		  }
		FREE(URL_s->post_to); /* free the array */
	  }


  /* free the array */
  PR_FREEIF(URL_s->add_crlf);

	 PR_FREEIF(URL_s->page_services_url);

	if (URL_s->privacy_policy_url != (char*)(-1)) {
		PR_FREEIF(URL_s->privacy_policy_url);
	} else {
		URL_s->privacy_policy_url = NULL;
	}

#ifdef TRUST_LABELS
	/* delete the entries and the trust list, if the list was created then there
	 * are entries on it.  */
	if ( URL_s->TrustList != NULL ) {
		/* delete each trust list entry then delete the list */
		TrustLabel *ALabel;
		XP_List *tempList = URL_s->TrustList;
		while ( (ALabel = XP_ListRemoveEndObject( tempList )) != NULL ) {
			TL_Destruct( ALabel );
		}
		XP_ListDestroy( URL_s->TrustList );
		URL_s->TrustList = NULL;
    }
#endif

    PR_Free(URL_s);
}

/* free the contents of AllHeader structure that is part of URL_Struct
 */
PRIVATE void
net_FreeURLAllHeaders (URL_Struct * URL_s)
{
    uint32 i=0;

    /* Free all memory associated with header information */
    for (i = 0; i < URL_s->all_headers.empty_index; i++) {
	if (URL_s->all_headers.key) {
	    FREEIF(URL_s->all_headers.key[i]);
	}
	if (URL_s->all_headers.value) {
	    FREEIF(URL_s->all_headers.value[i]);
	}
    }
    URL_s->all_headers.empty_index = URL_s->all_headers.max_index = 0;

    FREEIF(URL_s->all_headers.key);
    FREEIF(URL_s->all_headers.value);
    URL_s->all_headers.key = URL_s->all_headers.value = NULL;
}


/* if there is no slash in a URL besides the two that specify a host
 * then add one to the end.
 * 
 * this fails with file:// URLs of the form file:///Hard_disk
 * because there are 3 leading slashes already. Fixed for Mac, to fix bug 113706
 * 
 */
PRIVATE void add_slash_to_URL (URL_Struct *URL_s)
{
	char *colon=PL_strchr(URL_s->address,':');  /* must find colon */
	char *slash;

	/* make sure there is a hostname
	 */
#if defined(XP_MAC) /* || defined(XP_WIN) || defined(XP_UNIX) - add when comfortable.*/
	/* should probably be XP at some stage */
	if (PL_strncmp("file://", URL_s->address, 7) == 0)
	{
		 slash = PL_strchr(colon + 4,'/');
	}
	else
#endif
	if(*(colon+1) == '/' && *(colon+2) == '/')
	    slash = PL_strchr(colon+3,'/');
	else
		return;

	if(slash==NULL)
      {
      TRACEMSG(("GetURL: URL %-.1900s changed to ",URL_s->address));

#ifdef MOZILLA_CLIENT
		/* add it now to the Global history since we can't get this
		 * name back :(  this is a bug because we are not sure
		 * that the link will actually work.
		 */
	    GH_UpdateGlobalHistory(URL_s);
#endif /* MOZILLA_CLIENT */

	StrAllocCat(URL_s->address, "/");
	URL_s->address_modified = YES;

	TRACEMSG(("%-.1900s",URL_s->address));
      }
}
 
#ifdef MOZILLA_CLIENT
HG83667

PRIVATE int32
net_HTMLPanelLoad(ActiveEntry *ce)
{
	if(ce->URL_s)
		StrAllocCopy(ce->URL_s->charset, INTL_ResourceCharSet());
#if 0 /* Used to be MODULAR_NETLIB */
	XP_HandleHTMLPanel(ce->URL_s);
#endif
	return -1;
}

PRIVATE int32
net_HTMLDialogLoad(ActiveEntry *ce)
{
	if(ce->URL_s)
		StrAllocCopy(ce->URL_s->charset, INTL_ResourceCharSet());
#if 0 /* Used to be MODULAR_NETLIB */
	XP_HandleHTMLDialog(ce->URL_s);
#endif
	return -1;
}

PRIVATE int32
net_WysiwygLoad(ActiveEntry *ce)
{
	const char *real_url = LM_SkipWysiwygURLPrefix(ce->URL_s->address);
	char *new_address;

	/* XXX can't use StrAllocCopy because it frees dest first */
	if (real_url && (new_address = PL_strdup(real_url)) != NULL)
	{
		PR_Free(ce->URL_s->address);
		ce->URL_s->address = new_address;
		FREE_AND_CLEAR(ce->URL_s->wysiwyg_url);
	}

	/* no need to free real_url */

	ce->status = MK_DO_REDIRECT;
	return MK_DO_REDIRECT;
}

PRIVATE int32
net_ProtoMainStub(ActiveEntry *ce)
{
#ifdef DO_ANNOYING_ASSERTS_IN_STUBS
	PR_ASSERT(0);
#endif
	return -1;
}

PRIVATE void
net_ProtoCleanupStub(void)
{
}

PRIVATE void
net_reg_random_protocol(NET_ProtoInitFunc *LoadRoutine, int type)
{
    NET_ProtoImpl *random_proto_impl;

	random_proto_impl = PR_NEW(NET_ProtoImpl);

	if(!random_proto_impl)
		return;
	
    random_proto_impl->init = LoadRoutine;
    random_proto_impl->process = net_ProtoMainStub;
    random_proto_impl->interrupt = net_ProtoMainStub;
    random_proto_impl->resume = NULL; /* not usually need */
    random_proto_impl->cleanup = net_ProtoCleanupStub;

    NET_RegisterProtocolImplementation(random_proto_impl, type);
}

/* don't you just hate it when people come along and hack this
 * kind of stuff into your code.
 *
 * @@@ clean this up some time
 */
PRIVATE void
NET_InitTotallyRandomStuffPeopleAddedProtocols(void)
{
	HG00484
	net_reg_random_protocol(net_HTMLPanelLoad, HTML_PANEL_HANDLER_TYPE_URL);
	net_reg_random_protocol(net_HTMLDialogLoad, HTML_DIALOG_HANDLER_TYPE_URL);
	net_reg_random_protocol(net_WysiwygLoad, WYSIWYG_TYPE_URL);
}

#endif /* MOZILLA_CLIENT */

/*
 *  Check the no_proxy environment variable to get the list
 *  of hosts for which proxy server is not consulted.
 *
 *  no_proxy is a comma- or space-separated list of machine
 *  or domain names, with optional :port part.  If no :port
 *  part is present, it applies to all ports on that domain.
 *
 *  Example: "netscape.com,some.domain:8001"
 *
 */
PRIVATE Bool override_proxy (CONST char * URL)
{
    char * p = NULL;
    char * host = NULL;
    int port = 0;
    int h_len = 0;
    char *no_proxy = MKno_proxy;

    if(no_proxy==NULL)
	return(FALSE);

    if (!(host = NET_ParseURL(URL, GET_HOST_PART)))
	return NO;

    if (!*host)
      {
	PR_Free(host);
	return NO;
      }

    p = PL_strchr(host, ':');
    if (p)     /* Port specified */
      {
	*p++ = 0;                       /* Chop off port */
	port = atoi(p);
      }
    else
      {                          /* Use default port */
	char * access = NET_ParseURL(URL, GET_PROTOCOL_PART);
	if (access) {
	    if      (!PL_strcmp(access,"http"))    port = 80;
	    else if (!PL_strcmp(access,"gopher"))  port = 70;
	    else if (!PL_strcmp(access,"ftp"))     port = 21;
		PR_Free(access);
	}
      }
    if (!port) port = 80;           /* Default */
    h_len = PL_strlen(host);

    while (*no_proxy) {
	char * end;
	char * colon = NULL;
	int templ_port = 0;
	int t_len;

	while (*no_proxy && (isspace(*no_proxy) || *no_proxy==','))
	no_proxy++;                 /* Skip whitespace and separators */

	end = no_proxy;
	while (*end && !isspace(*end) && *end != ',')   /* Find separator */
		  {
	    if (*end==':') colon = end;                 /* Port number given */
	    end++;
	  }

	if (colon)
		  {
	    templ_port = atoi(colon+1);
	    t_len = colon - no_proxy;
	  }
		else
		  {
	    t_len = end - no_proxy;
	  }

	/* don't worry about case when comparing the requested host to the proxies in the
	   no proxy list, i.e. use PL_strncasecmp */
	if ((!templ_port || templ_port == port)  && (t_len > 0  &&  t_len <= h_len  &&
		!PL_strncasecmp(host + h_len - t_len, no_proxy, t_len))) 
		  {
	    PR_Free(host);
	    return YES;
	  }
	if (*end)
	    no_proxy = end+1;
	else
	    break;
    }

    PR_Free(host);
    return NO;
}

PUBLIC void
NET_SetProxyServer(NET_ProxyType type, const char * org_host_port)
{
	char *host_port = 0;

	if(org_host_port && *org_host_port) {
		host_port = PL_strdup(org_host_port);

		if(!host_port)
			return;

		/* limit the size of host_port to within MAXHOSTNAMELEN */
		if(PL_strlen(host_port) > MAXHOSTNAMELEN)
			host_port[MAXHOSTNAMELEN] = '\0';
	}

	TRACEMSG(("NET_SetProxyServer called with host: %-.1900s",
			  host_port ? host_port : "(null)"));

    switch(type)
	  {
		case PROXY_AUTOCONF_URL:
			if (host_port) {
				if (!MKproxy_ac_url || PL_strcmp(MKproxy_ac_url, org_host_port)) {
					StrAllocCopy(MKproxy_ac_url, org_host_port);
					NET_ProxyAcLoaded = FALSE;
				}
			}
			else {
				if (MKproxy_ac_url) {
					FREE_AND_CLEAR(MKproxy_ac_url);
					NET_ProxyAcLoaded = FALSE;
				}
			}
			break;

		case FTP_PROXY:
			if(host_port)
			StrAllocCopy(MKftp_proxy, host_port);
			else
				FREE_AND_CLEAR(MKftp_proxy);
			break;
		case GOPHER_PROXY:
			if(host_port)
			StrAllocCopy(MKgopher_proxy, host_port);
			else
				FREE_AND_CLEAR(MKgopher_proxy);
			break;
		case HTTP_PROXY:
			if(host_port)
			StrAllocCopy(MKhttp_proxy, host_port);
			else
				FREE_AND_CLEAR(MKhttp_proxy);
			break;
		HG88000
		case NEWS_PROXY:
			if(host_port)
			StrAllocCopy(MKnews_proxy, host_port);
			else
				FREE_AND_CLEAR(MKnews_proxy);
			break;
		case WAIS_PROXY:
			if(host_port)
			StrAllocCopy(MKwais_proxy, host_port);
			else
				FREE_AND_CLEAR(MKno_proxy);
			break;

		case NO_PROXY:
			if(host_port)
			StrAllocCopy(MKno_proxy, org_host_port);
			break;

		default:
			/* ignore */
			break;
	  }

	FREEIF(host_port);
}

/* return a proxy server host and port to the caller or NULL
 *
 * all this proxy bussiness should be rewritten to be generalized
 * and not switch() based [LJM]
 */
MODULE_PRIVATE char *
NET_FindProxyHostForUrl(int url_type, char *url_address)
{

	if(override_proxy(url_address))
		return NULL;

    switch(url_type)
	  {
		case FTP_TYPE_URL:
			return MKftp_proxy;
			break;

		case GOPHER_TYPE_URL:
			return(MKgopher_proxy);
			break;

		case HTTP_TYPE_URL:
		case URN_TYPE_URL:
		case NFS_TYPE_URL:
			return(MKhttp_proxy);
			break;

		HG50027

		case NEWS_TYPE_URL:
		case INTERNAL_NEWS_TYPE_URL:
			return(MKnews_proxy);
			break;

		case WAIS_TYPE_URL:
			return(MKwais_proxy);
			break;

		default:
			/* ignore */
			break;
	  }

	return NULL;
}

/* Force it to reload automatic proxy config.
 * This function simply sets a flag so the config will be 
 * reloaded during the next geturl. It doesn't actually
 * do the reloading here. */
#ifdef MOZILLA_CLIENT
PUBLIC void
NET_ReloadProxyConfig(MWContext *window_id)
{
	/* if we have a global config load it */
	if (MKglobal_config_url)
		  NET_GlobalAcLoaded = FALSE;
	/* else if we have an old style proxy config and we are set on automatic...load it 
	 * , or if we're using the old style proxy config that is actually a proxy autodiscovery
	 * url, reload it also */
    else if( (MKproxy_ac_url &&	(!MKproxy_style || MKproxy_style == PROXY_STYLE_AUTOMATIC))
			|| (NET_UsingPadPac()) )
		  NET_ProxyAcLoaded = FALSE;
}
#endif  /* MOZILLA_CLIENT */


/* Debugging routine prints an URL (and string "header")
 */
#ifdef DEBUG
void TraceURL (URL_Struct *url, char *header)
{
    TRACEMSG(("URL %-.1900s", header));
    if (url->address)           TRACEMSG(("    address          %s", url->address));
    if (url->content_length)    TRACEMSG(("    content_length   %i", url->content_length));
    if (url->content_type)      TRACEMSG(("    content_type     %s", url->content_type));
    if (url->content_encoding)  TRACEMSG(("    content_encoding %s", url->content_encoding));
}
#endif

/* This function's existance was prompted by the front end desiring the ability to put the
   client in kiosk mode. The function removes any potentially sensitive information from the
   background: passwords, caches, histories, cookies. */
/* fix Mac warning of missing prototype */
PUBLIC void
NET_DestroyEvidence();

PUBLIC void
NET_DestroyEvidence()
{
	int32 oldSize = 0;

	/* Handle authorizations */
	NET_RemoveAllAuthorizations();
	
	/* Handle global history */
	GH_ClearGlobalHistory();

	/* Handle Session History */

	/* Handle cookies */
	NET_RemoveAllCookies();

#if defined(SingleSingon)
	/* Handle single signons */
	SI_RemoveAllSignonData();
#endif

	/* Handle disk cache */
	oldSize = NET_GetDiskCacheSize();
	NET_SetDiskCacheSize(0); /* zero it out */
	NET_SetDiskCacheSize(oldSize); /* set it back up */

	/* Handle memory cache */
	oldSize = NET_GetMemoryCacheSize();
	NET_SetMemoryCacheSize(0); /* zero it out */
	NET_SetMemoryCacheSize(oldSize); /* set it back up */
}

#if !defined(OLD_MOZ_MAIL_NEWS) && !defined(MOZ_MAIL_COMPOSE)

/* this whole mess should get moved to the mksmtp.c file
 * where it can share the InitMailtoProtocol function
 * since that is what it is trying to replace
 *
 * for now InitMailto will be duplicated inside the ifdef.
 */

#define CE_URL_S            cur_entry->URL_s
#define CE_STATUS           cur_entry->status

extern void FE_AlternateCompose(
    char * from, char * reply_to, char * to, char * cc, char * bcc,
    char * fcc, char * newsgroups, char * followup_to,
    char * organization, char * subject, char * references,
    char * other_random_headers, char * priority,
    char * attachment, char * newspost_url, char * body);

MODULE_PRIVATE
int32 net_MailtoLoad (ActiveEntry * cur_entry)
{
		char *parms = NET_ParseURL (CE_URL_S->address, GET_SEARCH_PART);
		char *rest = parms;
		char *from = 0;						/* internal only */
		char *reply_to = 0;					/* internal only */
		char *to = 0;
		char *cc = 0;
		char *bcc = 0;
		char *fcc = 0;						/* internal only */
		char *newsgroups = 0;
		char *followup_to = 0;
		char *html_part = 0;				/* internal only */
		char *organization = 0;				/* internal only */
		char *subject = 0;
		char *references = 0;
		char *attachment = 0;				/* internal only */
		char *body = 0;
		char *other_random_headers = 0;		/* unused (for now) */
		char *priority = 0;
		char *newshost = 0;					/* internal only */
		HG72762
		char *newspost_url = 0;
		XP_Bool force_plain_text = FALSE;
		to = NET_ParseURL (CE_URL_S->address, GET_PATH_PART);
		if (rest && *rest == '?')
		  {
 			/* start past the '?' */
			rest++;
			rest = strtok (rest, "&");
			while (rest && *rest)
			  {
				char *token = rest;
				char *value = 0;
				char *eq = PL_strchr (token, '=');
				if (eq)
				  {
					value = eq+1;
					*eq = 0;
				  }
				switch (*token)
				  {
				  case 'A': case 'a':
					if (!PL_strcasecmp (token, "attachment") &&
						CE_URL_S->internal_url)
					  StrAllocCopy (attachment, value);
					break;
				  case 'B': case 'b':
					if (!PL_strcasecmp (token, "bcc"))
					  {
						if (bcc && *bcc)
						  {
							StrAllocCat (bcc, ", ");
							StrAllocCat (bcc, value);
						  }
						else
						  {
							StrAllocCopy (bcc, value);
						  }
					  }
					else if (!PL_strcasecmp (token, "body"))
					  {
						if (body && *body)
						  {
							StrAllocCat (body, "\n");
							StrAllocCat (body, value);
						  }
						else
						  {
							StrAllocCopy (body, value);
						  }
					  }
					break;
				  case 'C': case 'c':
					if (!PL_strcasecmp (token, "cc"))
					  {
						if (cc && *cc)
						  {
							StrAllocCat (cc, ", ");
							StrAllocCat (cc, value);
						  }
						else
						  {
							StrAllocCopy (cc, value);
						  }
					  }
					break;
				  HG16262
				  case 'F': case 'f':
					if (!PL_strcasecmp (token, "followup-to"))
					  StrAllocCopy (followup_to, value);
					else if (!PL_strcasecmp (token, "from") &&
							 CE_URL_S->internal_url)
					  StrAllocCopy (from, value);
					else if (!PL_strcasecmp (token, "force-plain-text") &&
							 CE_URL_S->internal_url)
						force_plain_text = TRUE;
					break;
				  case 'H': case 'h':
					  if (!PL_strcasecmp(token, "html-part") &&
						  CE_URL_S->internal_url) {
						StrAllocCopy(html_part, value);
					  }
				  case 'N': case 'n':
					if (!PL_strcasecmp (token, "newsgroups"))
					  StrAllocCopy (newsgroups, value);
					else if (!PL_strcasecmp (token, "newshost") &&
							 CE_URL_S->internal_url)
					  StrAllocCopy (newshost, value);
					break;
				  case 'O': case 'o':
					if (!PL_strcasecmp (token, "organization") &&
						CE_URL_S->internal_url)
					  StrAllocCopy (organization, value);
					break;
				  case 'R': case 'r':
					if (!PL_strcasecmp (token, "references"))
					  StrAllocCopy (references, value);
					else if (!PL_strcasecmp (token, "reply-to") &&
							 CE_URL_S->internal_url)
					  StrAllocCopy (reply_to, value);
					break;
				  case 'S': case 's':
					if(!PL_strcasecmp (token, "subject"))
					  StrAllocCopy (subject, value);
					HG72661
					break;
				  case 'P': case 'p':
					if (!PL_strcasecmp (token, "priority"))
					  StrAllocCopy (priority, value);
					break;
				  case 'T': case 't':
					if (!PL_strcasecmp (token, "to"))
					  {
						if (to && *to)
						  {
							StrAllocCat (to, ", ");
							StrAllocCat (to, value);
						  }
						else
						  {
							StrAllocCopy (to, value);
						  }
					  }
					break;
				  }
				if (eq)
				  *eq = '='; /* put it back */
				rest = strtok (0, "&");
			  }
		  }

		FREEIF (parms);
		if (to)
		  NET_UnEscape (to);
		if (cc)
		  NET_UnEscape (cc);
		if (subject)
		  NET_UnEscape (subject);
		if (newsgroups)
		  NET_UnEscape (newsgroups);
		if (references)
		  NET_UnEscape (references);
		if (attachment)
		  NET_UnEscape (attachment);
		if (body)
		  NET_UnEscape (body);
		if (newshost)
		  NET_UnEscape (newshost);

		if(newshost)
		  {
			char *prefix = "news://";
			/* char *slash = PL_strrchr (newshost, '/'); -- unused */
			HG32828
			newspost_url = (char *) PR_Malloc (PL_strlen (prefix) +
											  PL_strlen (newshost) + 10);
			if (newspost_url)
			  {
				PL_strcpy (newspost_url, prefix);
				PL_strcat (newspost_url, newshost);
				PL_strcat (newspost_url, "/");
			  }
		  }
		else
		  {
			HG13227
				newspost_url = PL_strdup ("news:");
		  }

#if defined(XP_WIN) || defined(XP_UNIX) /* other FE's add here when Front end code added */
        FE_AlternateCompose(
            from, reply_to, to, cc, bcc, fcc,
			newsgroups, followup_to, organization,
			subject, references, other_random_headers,
			priority, attachment, newspost_url, body );
#endif /* XP_WIN */

		FREEIF(from);
		FREEIF(reply_to);
		FREEIF(to);
		FREEIF(cc);
		FREEIF(bcc);
		FREEIF(fcc);
		FREEIF(newsgroups);
		FREEIF(followup_to);
		FREEIF(html_part);
		FREEIF(organization);
		FREEIF(subject);
		FREEIF(references);
		FREEIF(attachment);
		FREEIF(body);
		FREEIF(other_random_headers);
		FREEIF(newshost);
		FREEIF(priority);
		FREEIF(newspost_url);

		CE_STATUS = MK_NO_DATA;
		return(-1);
}

PRIVATE int32
net_MailtoStub(ActiveEntry *ce)
{
#ifdef DO_ANNOYING_ASSERTS_IN_STUBS
	PR_ASSERT(0);
#endif
	return(0);		/* Well the function definition says it returns SOMETHING! */
}
PRIVATE void
net_CleanupMailtoStub(void)
{
#ifdef DO_ANNOYING_ASSERTS_IN_STUBS
	PR_ASSERT(0);
#endif
}

MODULE_PRIVATE void
NET_InitMailtoProtocol(void)
{
        static NET_ProtoImpl mailto_proto_impl;

        mailto_proto_impl.init = net_MailtoLoad;
        mailto_proto_impl.process = net_MailtoStub;
        mailto_proto_impl.interrupt = net_MailtoStub;
        mailto_proto_impl.resume = NULL;
        mailto_proto_impl.cleanup = net_CleanupMailtoStub;

        NET_RegisterProtocolImplementation(&mailto_proto_impl, MAILTO_TYPE_URL);
}

#endif /* OLD_MOZ_MAIL_NEWS */

#ifdef TRUST_LABELS
/* given a URL search the list of URL_s structures for one that has
 * a non-empty trust list.
 * Return a pointer to the non empty trust last */
XP_List * NET_GetTrustList( char *TargetURL )
{
	XP_List *RetList = NULL;
	XP_List * list_ptr;
	URL_Struct * tmpURLs;
	char *FromURLs = NULL;
	char *FromTarget = NULL;

	if ( TargetURL && PL_strlen( TargetURL ) ) {
		LIBNET_LOCK();
		list_ptr = net_URL_sList;
		while((tmpURLs = (URL_Struct *) XP_ListNextObject(list_ptr)) != NULL) {
			/* first check if this URL_Struct has a trust list.  If it does then do 
			 * the name parsing on both URLs. Check for an empty list because due to
			 * thread switching the list could have been created but no entries added 
			 * to it. */
			if ( tmpURLs->TrustList != NULL && !XP_ListIsEmpty( tmpURLs->TrustList ) ) { 
			/* This entry has a trustList, see if the URS match.  To be safe strip both
			 * URLs down to host/domain/file.  If there is a match 
			 * return the pointer to the trust list */
				char *FromURLs = NET_ParseURL (tmpURLs->address, GET_PATH_PART);
				char *FromTarget = NET_ParseURL (TargetURL, GET_PATH_PART);
				if( PL_strcmp( FromURLs, FromTarget) == 0 ) {
					/* we have a match */
					RetList = tmpURLs->TrustList;
					break;
				}
			}
		} 
		FREEIF( FromURLs ); 
		FREEIF( FromTarget );
		LIBNET_UNLOCK();
	}
	return RetList;
}

#endif


#ifdef PROFILE
#pragma profile off
#endif
