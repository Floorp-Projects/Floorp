/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "cvjscfg.h"
#include "cvsimple.h"
#include "prefapi.h"
#include "mkutils.h"
#include "netutils.h"
#include "xpgetstr.h"
#include "jsapi.h"
#include "fe_proto.h"
#include "net_xp_file.h"

static XP_Bool                  m_GettingConfigFile = FALSE;
static XP_Bool                  m_FindProxyInJSC = FALSE;
static XP_Bool                  m_TimesUp = FALSE;

extern int XP_CONFIG_BLAST_WARNING;
extern int XP_RECEIVING_PROXY_AUTOCFG;
extern int XP_AUTOADMIN_MISSING;
extern int XP_BAD_AUTOCONFIG_NO_FAILOVER;
extern int XP_GLOBAL_EVEN_SAVED_IS_BAD;
extern int XP_CONF_LOAD_FAILED_USE_PREV;

extern void NET_DisableGetURL(void);

PRIVATE int jsc_try_failover(MWContext*);
PRIVATE XP_Bool jsc_check_for_find_proxy(void);
extern XP_Bool NET_InitPacfContext(void);

PRIVATE void AutoAdminRefreshCallback(void *closure)
{
	NET_DownloadAutoAdminCfgFile();
}


/*
 * pref_DownloadDone
 * Sets 'm_GettingConfigFile' to FALSE, and registers a timeout for
 * jsc file polling.
 */
PRIVATE void
pref_DownloadDone(URL_Struct* URL_s, int status, MWContext* window_id)
{
	int32 minutes = 0;
 
	/* Might have been cancelled by a timeout */
	if ( m_GettingConfigFile == FALSE ) {
		return;
	}

	m_GettingConfigFile = FALSE;

	if (URL_s->server_status == 0) {
		jsc_try_failover(window_id);
		m_FindProxyInJSC = jsc_check_for_find_proxy();
	} 

    if (PREF_OK ==  PREF_GetConfigInt("autoadmin.refresh_interval", &minutes)) {
	    if ( minutes > 0 ) {
		    FE_SetTimeout((TimeoutCallbackFunction) AutoAdminRefreshCallback,
						    NULL, minutes * 60 * 1000);
	    }
    }
}


/*
 * AutoAdminTimeoutCallback
 */
PRIVATE void
AutoAdminTimeoutCallback(void *closure)
{
	m_TimesUp = TRUE;
}


/*
 * PREF_DownloadConfigFile
 * Get the global config file and execute it.
 * Called at startup, and at the interval specified by
 * "autoadmin.refresh_interval".
 * The first time we get this file, we need to loop over FEU_StayingAlive().
 * After that, the main event loop will be happening, so we don't loop
 * ourselves, or we'd freeze up the browser.
 */
PUBLIC void
NET_DownloadAutoAdminCfgFile()
{
#if 0 /* Used to be MODULAR_NETLIB */ 
	static XP_Bool first_time = TRUE;
	XP_Bool append_email;
	char* email_addr;
	char* url = NULL;
    MWContext* context;

#ifdef MOZ_OFFLINE
	if ( NET_IsOffline() ) return;
#endif

	if(first_time)
		context = FE_GetInitContext();
	else
		context = XP_FindSomeContext();

	if ( first_time && (NET_InitPacfContext() == FALSE) ) return;

    if (PREF_OK != PREF_CopyConfigString("autoadmin.global_config_url", &url)) {
        return;
    }
 
	if (( url == NULL ) || (!(*url))) {
		return;
	}
 
  	if ( !PREF_IsAutoAdminEnabled() ) {
		XP_Bool failover = FALSE;
        if (PREF_OK != PREF_GetConfigBool("autoadmin.failover_to_cached", &failover)) {
            return;
        }
		if ( failover == FALSE ) {
			NET_DisableGetURL();
		}
		FE_Alert(context, XP_GetString(XP_AUTOADMIN_MISSING));
		return;
	}

    if (PREF_OK == PREF_GetConfigBool("autoadmin.append_emailaddr", &append_email)) {
         if ( append_email ) {
            StrAllocCat(url, "?");
            if (PREF_OK == PREF_CopyCharPref("mail.identity.useremail", &email_addr)) {
                if ( email_addr ) {
                    StrAllocCat(url, email_addr);
                    XP_FREE(email_addr);
                }
            }
        }
    }
 
	if ( url && *url ) {
		URL_Struct* url_s = NET_CreateURLStruct(url, NET_SUPER_RELOAD);
 
		NET_GetURL(url_s, FO_JAVASCRIPT_CONFIG, context, pref_DownloadDone);

		m_GettingConfigFile = TRUE;

		if ( first_time ) {
			int32 seconds;
			first_time = FALSE;
			/* Timeout so we don't hang here */
            if (PREF_OK == PREF_GetConfigInt("autoadmin.timeout", &seconds)) {
			    if ( seconds > 0 ) {
				    FE_SetTimeout((TimeoutCallbackFunction) AutoAdminTimeoutCallback,
						    NULL, seconds * 1000);
			    }
            }
			while ( m_GettingConfigFile && !m_TimesUp ) {
				FEU_StayingAlive();
			}
			if ( m_TimesUp ) {
				/* Give up, cause jsc download to fail */
				pref_DownloadDone(url_s, 0, context);
			}
		}
	}
#endif
}
 

/*
 * jsc_save
 */
PRIVATE void
jsc_save_config(char* bytes, int32 num_bytes)
{
	XP_File fp;
 
	if ( bytes == NULL || num_bytes <= 0 ) return;
 
	if( (fp = NET_XP_FileOpen("", xpJSConfig, XP_FILE_WRITE)) == NULL ) return;
 
	NET_XP_FileWrite(bytes, num_bytes, fp);
	NET_XP_FileClose(fp);
}


/*
 * jsc_try_failover
 */
PRIVATE int
jsc_try_failover(MWContext* w)
{
	XP_Bool failover;
	char* bytes = NULL;
	int32 num_bytes;
	XP_StatStruct st;
	XP_File fp = NULL;

    if (PREF_OK != PREF_GetConfigBool("autoadmin.failover_to_cached", &failover)) {
        goto failed;
    }

	if ( failover == FALSE ) {
		FE_Alert(w, XP_GetString(XP_BAD_AUTOCONFIG_NO_FAILOVER));
		NET_DisableGetURL();
		return -1;
	}

	if ( !FE_Confirm(w, XP_GetString(XP_CONF_LOAD_FAILED_USE_PREV)) ) return -1;

	if (NET_XP_Stat("", &st, xpJSConfig) == -1) goto failed;
 
	if ( (fp = NET_XP_FileOpen("", xpJSConfig, XP_FILE_READ)) == NULL ) goto failed;
 
	num_bytes = st.st_size;

	if ( num_bytes <= 0 ) goto failed;

	bytes = (char *)XP_ALLOC(num_bytes + 1);

	if ( bytes == NULL ) goto failed;

	if ( (num_bytes = NET_XP_FileRead(bytes, num_bytes, fp)) <= 0 ) goto failed;

	bytes[num_bytes] = '\0';

#ifdef NEW_PREF_ARCH
	if ( !PREF_EvaluateConfigScript(bytes, num_bytes, NULL, TRUE, TRUE, FALSE) )goto failed;
#else
    if ( !PREF_EvaluateConfigScript(bytes, num_bytes, NULL, TRUE, TRUE) )goto failed;
#endif

	NET_XP_FileClose(fp);
	XP_FREEIF(bytes);

	return 0;

failed:
	FE_Alert(w, XP_GetString(XP_GLOBAL_EVEN_SAVED_IS_BAD));
	if ( fp != NULL ) NET_XP_FileClose(fp);
	XP_FREEIF(bytes);
	return -1;
}


/*
 * jsc_check_for_find_proxy
 */
PRIVATE XP_Bool
jsc_check_for_find_proxy(void)
{
	JSObject* globalConfig;
	JSContext* configContext;
	char buf[1024];
	JSBool ok;
	jsval rv;

	if (PREF_OK != PREF_GetConfigContext(&configContext) ||
        PREF_OK != PREF_GetGlobalConfigObject(&globalConfig) ) {
        return FALSE;
    }

	XP_SPRINTF(buf, "typeof(ProxyConfig.FindProxyForURL) == \"function\"");
	ok = JS_EvaluateScript(configContext, globalConfig, buf, strlen(buf), 0, 0, 
						&rv);

	return (ok && JSVAL_IS_BOOLEAN(rv) && JSVAL_TO_BOOLEAN(rv) == JS_TRUE);
}


/*
 * jsc_complete
 */
PRIVATE void
jsc_complete(void* bytes, int32 num_bytes)
{
#ifdef DEBUG_malmer
	extern FILE* real_stderr;

	fwrite(bytes, sizeof(char), num_bytes, real_stderr);
#endif

	if ( bytes ) {
#ifdef NEW_PREF_ARCH
        PREF_EvaluateConfigScript(bytes, num_bytes, NULL, TRUE, TRUE, FALSE);
#else
		PREF_EvaluateConfigScript(bytes, num_bytes, NULL, TRUE, TRUE);
#endif
	} else {
		/* If failover is ok, read the local cached config */
	}

	/* Need to hash and save to disk here */
}


/*
 * NET_JavascriptConfig
 */
MODULE_PRIVATE NET_StreamClass*
NET_JavascriptConfig(int fmt, void* data_obj, URL_Struct* URL_s, MWContext* w)
{
	if ( URL_s == NULL || URL_s->address == NULL ) {
		return NULL;
	}

	NET_Progress(w, XP_GetString(XP_RECEIVING_PROXY_AUTOCFG));

	return NET_SimpleStream(fmt, (void*) jsc_complete, URL_s, w);
}


/*
 * NET_FindProxyInJSC
 */
MODULE_PRIVATE XP_Bool
NET_FindProxyInJSC(void)
{
	return m_FindProxyInJSC;
}


