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

/*	The PrefetchURLStruct which holds the URL struct
	and the corresponding prefetch value.
*/
typedef struct _PrefetchURLStruct {
	double 			prevalue;
	URL_Struct*		URL_s;
} PrefetchURLStruct;

PRIVATE XP_List* prefetch_list = 0;

#define USER_SETTING 0.0 /* TODO Change this to read off of the prefs.js file */

MODULE_PRIVATE void pre_FreePrefetchURLStruct(PrefetchURLStruct *pus);
MODULE_PRIVATE Bool pre_OKToPrefetch(char* url);
MODULE_PRIVATE void pre_Finished(URL_Struct* url_struct, int status, MWContext* context);

PRIVATE XP_Bool pre_enabled = TRUE;
PRIVATE int pre_LockNormalizeAndSort(); 
PRIVATE void pre_ProcessList(MWContext* context); 

/*	Constructs a URL_Struct from the the specified URL to the prefetch_list based
	on the value of the pre subtag. 
*/
PUBLIC void
PRE_AddToList(MWContext* context, char* url, double value) 
{
	/* Construct a new URL_Struct with this url, and Prefetch priority */
	URL_Struct* urls;
	PrefetchURLStruct *pus = PR_NEW(PrefetchURLStruct); 

	if (!pre_enabled || !value || !pus || !pre_OKToPrefetch(url))
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
	
	pus->prevalue = value;
	pus->URL_s = urls;

	XP_ListAddObjectToEnd(prefetch_list, pus);
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
	LO_TabFocusData tfd;
	int found ;

	if (!pre_enabled)
		return;

	tfd.pElement		= NULL;
	tfd.mapAreaIndex	= 0;		/* 0 means no focus, start with index 1. */
	tfd.pAnchor			= NULL;
	
	do 
	{
		if ((found = LO_getNextTabableElement(
			context, 
			&tfd, 
			1)) != 0)
		{
			if (tfd.pElement && 
					(tfd.pElement->lo_any.type == LO_TEXT ||	
						tfd.pElement->lo_any.type == LO_IMAGE))	/* Image Maps */
			{
				PRE_AddToList(context, (char*)tfd.pAnchor->anchor, tfd.pAnchor->prevalue);
			}
		}
	}
	while (found != 0);

	pre_ProcessList(context);
	XP_ListDestroy(prefetch_list); 
	prefetch_list = NULL;
}

/*	Returns bool to indicate if its OK to prefetch the specified URL.
	we don't prefetch mailto:, file:, etc. 
*/
PRIVATE Bool
pre_OKToPrefetch(char* url)
{
	int type;
	if (url == NULL)
		return FALSE;

	/* Add new logic here */
	/* (skip about:/mailto:/telnet:/gopher:/ etc.) */
	type = NET_URL_Type(url);
	if (type && (
		(type == HTTP_TYPE_URL) ||
		(type == GOPHER_TYPE_URL)))
		return TRUE;

	return FALSE;
}

/*	Lock the prefetch_list and normalize the item values.
*/
PRIVATE int 
pre_LockNormalizeAndSort() 
{
	int count;
	double sum=0;
	PrefetchURLStruct* pus = NULL;
	XP_List* pList = prefetch_list;

	count = XP_ListCount(prefetch_list);
	if (0 == count)
		return 0;

	while((pus = (PrefetchURLStruct*)XP_ListNextObject(pList)))
		sum += pus->prevalue;
	
	if (sum != 0)
	{
		XP_List* pList = prefetch_list;
		while((pus = (PrefetchURLStruct*)XP_ListNextObject(pList)))
			pus->prevalue = pus->prevalue/sum;
	}
	else 
		return -1;
	return count;
}

MODULE_PRIVATE void 
pre_FreePrefetchURLStruct(PrefetchURLStruct *pus)
{
    if(pus) 
      {
        FREEIF(pus->URL_s);
        PR_Free(pus);
      }
}

PRIVATE void
pre_Finished(URL_Struct* url_struct, int status, MWContext* context)
{
	/* this should change to update the colors of 
	the prefetched links */
	PR_Free(url_struct);
	url_struct = 0;
}

PRIVATE void
pre_ProcessList(MWContext* context) 
{
	int count = XP_ListCount(prefetch_list);
	if (XP_ListCount(prefetch_list)>0) 
	{
		/* Normalize the prefetch-list based on the values */
		if (pre_LockNormalizeAndSort() > 0) 
		{
			/* Invoke NET_GetURL on the prefetch_list */
			int i;
			for (i=0; i<count; i++) {
				PrefetchURLStruct* pusTop;
				PrefetchURLStruct* pus;
				XP_List* pList = prefetch_list;

				while((pus = (PrefetchURLStruct*)XP_ListNextObject(pList)))
				{
					if (pusTop) 
					{
						if (pusTop->prevalue < pus->prevalue)
							pusTop = pus;
					}
					else
						pusTop = pus;

				}
				if (pusTop->prevalue > USER_SETTING)
				{
					NET_GetURL (pus->URL_s,
						FO_CACHE_ONLY,
						context,
						pre_Finished); 
				}
				XP_ListRemoveObject(prefetch_list, pusTop);
			}
		}
	}
}

/* Enable or disable the prefetching, called from NET_SetupPrefs in mkgeturl.c */
PUBLIC void
PRE_Enable(XP_Bool enabled)
{
	pre_enabled = enabled;
}
