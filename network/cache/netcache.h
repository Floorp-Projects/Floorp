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

#ifndef NETCACHE_H
#define NETCACHE_H

#include "mkgeturl.h"

PR_BEGIN_EXTERN_C

/* Initialization */
extern void     NET_CacheInit(void);
/* Shutdown */
extern void     NET_CleanupCache(void); 

/* create an HTML stream and push a bunch of HTML about
 * the cache
 */
extern void     NET_DisplayCacheInfoAsHTML(ActiveEntry * cur_entry);

/* create an HTML stream and push a bunch of HTML about
 * the memory cache - TODO consolidate these two into one function! - Gagan
 */
extern void     NET_DisplayMemCacheInfoAsHTML(ActiveEntry * cur_entry);

extern int      NET_FindURLInCache(URL_Struct * URL_s, MWContext *ctxt);

#ifdef NU_CACHE
extern void     NET_InitNuCacheProtocol(void);
#else
extern void     NET_InitMemCacProtocol(void);
#endif

/* Will go away later */
extern PRBool   NET_IsCacheTraceOn(void);

/* return TRUE if the URL is in the cache and
 * is a partial cache file - TODO- only used in HTTP- cleanup - Gagan 
 */ 
extern PRBool   NET_IsPartialCacheFile(URL_Struct *URL_s);

#ifdef NU_CACHE
/* return TRUE if the URL exists somewhere in the cache */
extern PRBool   NET_IsURLInCache(const URL_Struct *URL_s);
#endif

/* Update cache entry on a 304 return*/
extern void     NET_RefreshCacheFileExpiration(URL_Struct * URL_s);

/* remove a URL from the cache */
extern void     NET_RemoveURLFromCache(URL_Struct *URL_s);

/* Removed in NU_Cache, SAR should go through new api. - Gagan */
#ifndef NU_CACHE
extern void     NET_OpenExtCacheFAT(MWContext *ctxt, char * cache_name, char * instructions);
extern void     CACHE_CloseAllOpenSARCache();
extern void     CACHE_OpenAllSARCache();
extern int      NET_FindURLInMemCache(URL_Struct * URL_s, MWContext *ctxt);
/* lookup routine
 *
 * builds a key and looks for it in the database.  Returns an access
 * method and sets a filename in the URL struct if found
 */
extern int      NET_FindURLInExtCache(URL_Struct * URL_s, MWContext *ctxt);
#endif

PR_END_EXTERN_C

#endif /* NETCACHE_H */
