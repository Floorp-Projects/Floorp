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

XP_BEGIN_PROTOS

extern void   NET_CleanupCache (char * filename);
extern int    NET_FindURLInCache(URL_Struct * URL_s, MWContext *ctxt);
extern void   NET_RefreshCacheFileExpiration(URL_Struct * URL_s);

extern void NET_CacheInit(void);

extern void NET_InitMemCacProtocol(void);


extern XP_Bool NET_IsCacheTraceOn(void);

/* remove a URL from the cache
 */
extern void NET_RemoveURLFromCache(URL_Struct *URL_s);

/* create an HTML stream and push a bunch of HTML about
 * the cache
 */
extern void NET_DisplayCacheInfoAsHTML(ActiveEntry * cur_entry);

/* return TRUE if the URL is in the cache and
 * is a partial cache file
 */ 
extern XP_Bool NET_IsPartialCacheFile(URL_Struct *URL_s);

/* encapsulated access to the first object in cache_database */
extern int NET_FirstCacheObject(DBT *key, DBT *data);

/* encapsulated access to the next object in the cache_database */
extern int NET_NextCacheObject(DBT *key, DBT *data);

/* Max size for displaying in the cache browser */
extern int32 NET_GetMaxDiskCacheSize();

extern void
NET_OpenExtCacheFAT(MWContext *ctxt, char * cache_name, char * instructions);

extern void
CACHE_CloseAllOpenSARCache();

extern void
CACHE_OpenAllSARCache();

/* create an HTML stream and push a bunch of HTML about
 * the memory cache
 */
extern void
NET_DisplayMemCacheInfoAsHTML(ActiveEntry * cur_entry);

extern int
NET_FindURLInMemCache(URL_Struct * URL_s, MWContext *ctxt);

/* lookup routine
 *
 * builds a key and looks for it in
 * the database.  Returns an access
 * method and sets a filename in the
 * URL struct if found
 */
extern int NET_FindURLInExtCache(URL_Struct * URL_s, MWContext *ctxt);

XP_END_PROTOS

#endif /* NETCACHE_H */
