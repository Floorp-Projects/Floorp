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
/* mkautocf.h: Proxy auto-config parser and evaluator
 * Ari Luotonen */

#ifndef MK_PROXY_AUTO_CONFIG
#define MK_PROXY_AUTO_CONFIG


#include "xp_mcom.h"
#include "xp.h"

/*
 * Called by netlib to get a single string of "host:port" format,
 * given the XP_List containing opaque proxy config objects.
 *
 * This function will return an address to a proxy that is (to its
 * knowledge) up and running.  Netlib can later inform this module
 * using the function pacf_proxy_is_down() that a proxy is down
 * and should not be called for a few minutes.
 *
 * Returns FALSE if everything has failed, and an error should be
 * displayed to the user.
 *
 * Returns TRUE if there is hope.
 * If *ret is NULL, a direct connection should be attempted.
 * If *ret is not null, it is the proxy address to use.
 *
 */
MODULE_PRIVATE Bool
pacf_get_proxy_addr(MWContext *context, char *list,
		    char **  ret_proxy_addr,
		    u_long * ret_socks_addr,
		    short *  ret_socks_port);


MODULE_PRIVATE char *pacf_find_proxies_for_url(MWContext *context, 
					       URL_Struct *URL_s);


/*
 * A stream constructor function for application/x-ns-proxy-autoconfig.
 *
 *
 *
 */
MODULE_PRIVATE NET_StreamClass *
NET_ProxyAutoConfig(int fmt, void *data_obj, URL_Struct *URL_s, MWContext *w);


/*
 * Called by mkgeturl.c to originally retrieve, and re-retrieve
 * the proxy autoconfig file.
 *
 * autoconfig_url is the URL pointing to the autoconfig.
 *
 * The rest of the parameters are what was passed to NET_GetURL(),
 * and when the autoconfig load finishes NET_GetURL() will be called
 * with those exact same parameters.
 *
 * This is because the proxy config is loaded when NET_GetURL() is
 * called for the very first time, and the actual request must be put
 * on hold when the proxy config is being loaded.
 *
 * When called from the explicit proxy config RE-load function
 * NET_ReloadProxyConfig, the four last parameters are all zero,
 * and no request gets restarted.
 *
 */
MODULE_PRIVATE int NET_LoadProxyConfig(char *autoconf_url,
				       URL_Struct *URL_s,
				       FO_Present_Types output_format,
				       MWContext *window_id,
				       Net_GetUrlExitFunc *exit_routine);

/*
 * NET_GetNoProxyFailover
 * Returns TRUE if we're not allowing proxy failover.
 */
MODULE_PRIVATE Bool NET_GetNoProxyFailover(void);

#endif /* ! MK_PROXY_AUTO_CONFIG */

