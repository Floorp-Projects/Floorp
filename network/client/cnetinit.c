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

#include "xp.h"
#include "mkutils.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "plstr.h"
#include "prmem.h"

#include "fileurl.h"
#include "httpurl.h"
#include "ftpurl.h"
#include "abouturl.h"
#include "gophurl.h"
#include "jsurl.h"
#include "fileurl.h"
#include "remoturl.h"
#include "dataurl.h"
#include "netcache.h"

#if defined(JAVA) && defined(XP_MAC)
#include "marimurl.h"
#endif

/* For about handlers */
#include "il_strm.h"
#include "glhist.h"

PRIVATE void net_InitAboutURLs();

PUBLIC void
NET_ClientProtocolInitialize(void)
{

        NET_InitFileProtocol();
        NET_InitHTTPProtocol();
#ifdef NU_CACHE
        NET_InitNuCacheProtocol();
#else   
        NET_InitMemCacProtocol();
#endif
        NET_InitFTPProtocol();
        NET_InitAboutProtocol();
        NET_InitGopherProtocol();
        NET_InitMochaProtocol();
        NET_InitRemoteProtocol();
        NET_InitDataURLProtocol();

#ifdef JAVA
    NET_InitMarimbaProtocol();
#endif

    net_InitAboutURLs();
}

#ifdef XP_UNIX
extern void FE_ShowMinibuffer(MWContext *);
  
PRBool net_AboutMinibuffer(const char *token,
                           FO_Present_Types format_out,
                           URL_Struct *URL_s,
                           MWContext *window_id)
{
    FE_ShowMinibuffer(window_id);

    return PR_TRUE;
}
#endif

#ifdef WEBFONTS
PRBool net_AboutFonts(const char *token,
                      FO_Present_Types format_out,
                      URL_Struct *URL_s,
                      MWContext *window_id)
{
    NF_AboutFonts(window_id, which);
    return PR_TRUE;
}
#endif /* WEBFONTS */

PRBool net_AboutImageCache(const char *token,
                           FO_Present_Types format_out,
                           URL_Struct *URL_s,
                           MWContext *window_id)
{
    IL_DisplayMemCacheInfoAsHTML(format_out, URL_s, window_id);
    return PR_TRUE;
}

PRBool net_AboutGlobalHistory(const char *token,
                              FO_Present_Types format_out,
                              URL_Struct *URL_s,
                              MWContext *window_id)
{
    NET_DisplayGlobalHistoryInfoAsHTML(window_id, URL_s, format_out);
    
    return PR_TRUE;
}

PRIVATE void net_InitAboutURLs()
{
    NET_RegisterAboutProtocol("image-cache", net_AboutImageCache);
    NET_RegisterAboutProtocol("global", net_AboutGlobalHistory);
#ifdef XP_UNIX
    NET_RegisterAboutProtocol("minibuffer", net_AboutMinibuffer);
#endif
#ifdef WEBFONTS
    NET_RegisterAboutProtocol("fonts", net_AboutFonts);
#endif
}
