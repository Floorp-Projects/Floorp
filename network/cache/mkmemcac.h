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

#ifndef MK_MEMORY_CACHE_H

#include "mkgeturl.h"

/* A pointer to this struct is passed as the converter_obj argument to
 * NET_MemCacheConverter.
 */
typedef struct net_MemCacheConverterObject {
    NET_StreamClass *next_stream;
	PRBool         dont_hold_URL_s;
} net_MemCacheConverterObject;

extern NET_StreamClass * 
NET_MemCacheConverter (FO_Present_Types format_out,
                    void       *converter_obj,
                    URL_Struct *URL_s,
                    MWContext  *window_id);

/* remove a URL from the memory cache
 */
extern void
NET_RemoveURLFromMemCache(URL_Struct *URL_s);

/* set or unset a lock on a memory cache object
 */
extern void
NET_ChangeMemCacheLock(URL_Struct *URL_s, PRBool set);

/* Create a wysiwyg cache converter to a copy of the current entry for URL_s.
 */
extern NET_StreamClass *
net_CloneWysiwygMemCacheEntry(MWContext *window_id, URL_Struct *URL_s,
			      uint32 nbytes, const char * wysiwyg_url,
			      const char * base_href);
/* return the first cache object in memory */
extern net_CacheObject * 
NET_FirstMemCacheObject(XP_List* list_ptr);

/* return the next cache object in memory */
extern net_CacheObject * 
NET_NextMemCacheObject(XP_List* list_ptr);

#endif /* MK_MEMORY_CACHE_H */
