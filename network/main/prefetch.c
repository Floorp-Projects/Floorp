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
/* designed/implemented by Gagan Saksena */

#include "mkutils.h"
#include "xp.h"
#include "prefetch.h"
#include "net.h"
#include "xp_list.h"
#include "prefapi.h"

PRIVATE XP_List* prefetch_list = 0;

MODULE_PRIVATE Bool pre_OKToPrefetch(char* url);
MODULE_PRIVATE void pre_Finished(URL_Struct* url_struct, int status, MWContext* context);

PRIVATE XP_Bool pre_enabled = TRUE;
PRIVATE void pre_ProcessList(MWContext* context); 

/*	Constructs a URL_Struct from the the specified URL to the prefetch_list based
	on the value of the pre subtag. 
*/
PUBLIC void
PRE_AddToList(MWContext* context, char* url) 
{
	/* Construct a new URL_Struct with this url, and Prefetch priority */
	URL_Struct* urls;
	
	if (!pre_enabled || !pre_OKToPrefetch(url))
		return;

	urls = NET_CreateURLStruct(url, NET_DONT_RELOAD);
	if (!urls)
		return;
	
	urls->priority = Prefetch_priority;
	urls->load_background = TRUE;
					
	if (prefetch_list == NULL)
	{
		prefetch_list = XP_ListNew();
	}
	
	XP_ListAddObjectToEnd(prefetch_list, urls);
}


/*  The main process for each MWContext, should be called after 
	a page is finished loading. This basically extracts all the 
	links on the current page and eliminates the non relevant ones
	and adds the elements to the prefetch_list. If something
	has been added, then it is passed on to NET_GetURL function
	for prefetching and storing in cache. 
*/
PUBLIC void 
PRE_Fetch(MWContext* context)
{
	pre_ProcessList(context);
}

/*	Returns bool to indicate if its OK to prefetch the specified URL.
	we don't prefetch mailto:, file:, etc. 
*/
MODULE_PRIVATE Bool
pre_OKToPrefetch(char* url)
{
	int type;
	if (url == NULL)
		return FALSE;

	/* Skip interpreted cgi-bin's for now... */
    if (PL_strchr(url, '?') ||
        PL_strstr(url, "cgi-bin") ||
        PL_strstr(url, ".cgi") ||
        PL_strstr(url, ".shtml"))
        return FALSE;

	/* approve only HTTP and Gopher for now (skip about:/mailto:/telnet:/gopher:/ etc.) */
    /* TODO add FTP directories only! */
	type = NET_URL_Type(url);
	if (type && (
		(type == HTTP_TYPE_URL) ||
		(type == GOPHER_TYPE_URL)))
		return TRUE;

	return FALSE;
}

MODULE_PRIVATE void
pre_Finished(URL_Struct* url_struct, int status, MWContext* context)
{
	/* this should change to update the colors of 
	the prefetched links */
	if (prefetch_list)
    {
        XP_ListRemoveObject(prefetch_list, url_struct); 

        if (XP_ListCount(prefetch_list) == 0)
        {
            XP_ListDestroy(prefetch_list);
            prefetch_list = 0;
        }
    }
    XP_FREEIF(url_struct);
}

PRIVATE void
pre_ProcessList(MWContext* context) 
{
	if (XP_ListCount(prefetch_list)>0) 
	{
        XP_List* pList = prefetch_list;
        URL_Struct* urls = (URL_Struct*)XP_ListNextObject(pList);
        
        while (urls)
        {
            NET_GetURL(urls,
                FO_CACHE_ONLY,
                context,
                pre_Finished);

            urls = (URL_Struct*)XP_ListNextObject(pList);
        }
	}
}

/* Enable or disable the prefetching, called from NET_SetupPrefs in mkgeturl.c */
PUBLIC void
PRE_Enable(PRUint8 nNumber)
{
    if (nNumber > 0)
    {
	    pre_enabled = TRUE;
        /* TODO - set max number here */
    }
    else
        pre_enabled = FALSE;
}
