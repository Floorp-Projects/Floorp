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

/* mkpadpac.h -- Proxy auto-discovery documentation.
   Created: Judson Valeski, 01.15.1998

   Proxy auto-discovery (pad) allows the client to search for and use a 
   pre-determined proxy auto-configuration (pac) file. The file it looks up 
   and parses is named in the users prefs file (network.padPacURL). The feature
   can be completely disabled by setting the network.enablePad preference to 
   false.

   Holding to tradition, I've piggy-backed pad onto the current pac file 
   implementation. If pad is enabled, the MKproxy_ac_url variable (as defined in
   mkautocf.c/h) is set to point at the MK_padPacURL. This means that all the 
   current pac file logic is used once a pac file is located by pad. A pad pac 
   file is located by asyncronously searching for the host in the pad pac url 
   in mkconect.c>NET_findAddress(). If the host is found then MKproxy_ac_url is
   set to MK_padPacURL (this happens in mkgeturl.c>NET_getURL()) and we treat the 
   pad pac file just like a pac file. The only difference is that when there's 
   a problem with the pad pac file, we failover silently to a direct connection, 
   no error messages get to the user. All the silent failover happens in 
   mkautocf.c, except for one case in mkhttp.c which handles the case when there
   is a problem actually loading the pac file itself.
 */

#ifndef MKPADPAC_H
#define MKPADPAC_H

#include "xp.h"

/* Pad js pref names */
#define pref_padPacURL "network.padPacURL"
#define pref_enablePad "network.enablePad"

/* Global pad variables */
extern XP_Bool foundPADPAC;
extern char *MK_padPacURL;
extern XP_Bool MK_PadEnabled;

/* ***** FUNCTION PROTOTYPES */

/* Setup internal variables to use or not use the proxy 
 * autodiscovery feature. Actually not that simple. */
MODULE_PRIVATE void net_UsePadPac(XP_Bool useIt);

/* Return whether we're currently using a pac file via proxy autodiscovery. 
 * This function takes three items into consideration:
 * 1. Is padpac enabled.
 * 2. Was the padpac host found.
 * 3. Is the user's proxy style pref set to NONE (i.e. the user is set to go
 *    direct connection, not proxy, or auto proxy.
 * 
 * If all these are true, then we're using a padpac file.
 */
PUBLIC XP_Bool NET_UsingPadPac(void);

/* Return whether or not we are currently in the process of loading
 * a proxy autoconfig url. */
PUBLIC XP_Bool NET_LoadingPac(void);

/* Set the MK_padPacURL varialbe to point to the url passed in,
 * after checking the url for size limits. 
 * Returns TRUE if successful, FALSE otherwise. */
PUBLIC XP_Bool NET_SetPadPacURL(char * url);

/* Called by js prefs when the padpac url changes. */
MODULE_PRIVATE int PR_CALLBACK net_PadPacURLPrefChanged(const char *pref, void *data);

/* Called by js prefs when the enable padpac pref changes. */
MODULE_PRIVATE int PR_CALLBACK net_EnablePadPrefChanged(const char *pref, void *data);

/* Registers the above callbacks with js prefs. */
PUBLIC void NET_RegisterPadPrefCallbacks(void);

/* ***** END FUNCTION PROTOTYPES */

#endif /* MKPADPAC_H */
