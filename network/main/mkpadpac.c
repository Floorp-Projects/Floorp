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
/* mkpadpac.c -- Proxy auto-discovery utility function implementation.
   Created: Judson Valeski, 01.15.1997
 */

#include "prmem.h"
#include "mkpadpac.h"
#include "netutils.h"
#include "mkgeturl.h" /* for NET_GetProxyStyle() */
#include "prefapi.h" /* for js prefs stuff */

/* Global pad variables */
PUBLIC PRBool foundPADPAC=PR_FALSE;
PUBLIC char *MK_padPacURL=NULL;
PUBLIC PRBool MK_PadEnabled=PR_TRUE;

MODULE_PRIVATE void net_UsePadPac(PRBool useIt) {
	if(useIt) {
		net_PadPacURLPrefChanged(NULL, NULL);
	} else {
		MK_PadEnabled=FALSE;
		NET_ProxyAcLoaded=FALSE;
		PR_FREEIF(MK_padPacURL);
		MK_padPacURL=NULL;
		net_SetPACUrl(NULL);
	}
}

/* Return whether we're currently using a pac file via proxy autodiscovery. */
PUBLIC PRBool NET_UsingPadPac(void) {
	return (MK_PadEnabled && foundPADPAC && (NET_GetProxyStyle() == PROXY_STYLE_NONE) );
}

/* Set the MK_padPacURL varialbe to point to the string passed in. */
PUBLIC PRBool NET_SetPadPacURL(char * u) {
	char *url=NULL;
	if(!u || *u == '\0')
		return FALSE;
	StrAllocCopy(url, u);
	if(url && *url) {
		char *host=NET_ParseURL(url, GET_HOST_PART);
		if(host	&& *host && (sizeof(host) <= MAXHOSTNAMELEN) ) {
			if(MK_padPacURL)
				PR_Free(MK_padPacURL);
			MK_padPacURL=url;
			return TRUE;
		} else {
			PR_Free(url);
			PR_FREEIF(host);
		}
	} else {
		PR_FREEIF(url);
	}
	return FALSE;
}

/* Pref for proxy autodiscovery url */
MODULE_PRIVATE int PR_CALLBACK 
net_PadPacURLPrefChanged(const char *pref, void *data) {
	char s[128];
    int len = sizeof(s);
	memset(s, 0, len);

    if ( (PREF_OK == PREF_GetCharPref(pref_padPacURL, s, &len)) ) {
	    NET_SetPadPacURL(s);
    }
    return PREF_NOERROR;
}

/* Pref for allowing (or not) proxy autodiscovery. */
MODULE_PRIVATE int PR_CALLBACK 
net_EnablePadPrefChanged(const char *pref, void *data) {
	PRBool x = FALSE;
    if ( (PREF_OK == PREF_GetBoolPref(pref_enablePad, &x)) ) {
	    MK_PadEnabled=x;
    }
	return PREF_NOERROR;
}

/* called from mkgeturl.c, NET_InitNetLib(). 
 * Initializes the pad variables and registers pad callbacks */
PUBLIC void
NET_RegisterPadPrefCallbacks(void) {
	PRBool x = FALSE;
    char s[128];
    int len=sizeof(s);
	memset(s, 0, len);

    if ( (PREF_OK == PREF_GetBoolPref(pref_enablePad, &x)) ) {
	    MK_PadEnabled=x;
    } else {
        MK_PadEnabled=x;
    }
	PREF_RegisterCallback(pref_enablePad, net_EnablePadPrefChanged, NULL);

    if ( (PREF_OK == PREF_GetCharPref(pref_padPacURL, s, &len)) ) {
	    NET_SetPadPacURL(s);
    }
	PREF_RegisterCallback(pref_padPacURL, net_PadPacURLPrefChanged, NULL);
}
