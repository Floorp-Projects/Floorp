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
/*** pagescan.h ***************************************************/
/*   description:		page scanning                             */
/*                      - not dependent on the crawler            */
  

 /********************************************************************

  $Revision: 1.1 $
  $Date: 1998/04/30 20:53:32 $

 *********************************************************************/

#ifndef pagescan_h___
#define pagescan_h___
#include "prtypes.h"
#include "ntypes.h" /* for MWContext */
#include "net.h" /* for URLStruct */

/* any content length over this number is assumed to be a bogus content length field. This
   is sometimes used to keep a connection open. If we encounter a content length this size or
   greater we won't automatically reject it because it's larger than the remaining cache size.
*/
#define BOGUS_CONTENT_LENGTH ((uint32)100000000)

typedef char **CRAWL_ItemList;

typedef char *CRAWL_Item;

/* Most APIs require a CRAWL_PageInfo reference, which is created in crawl_makePage. */
typedef struct _CRAWL_PageInfoStruct *CRAWL_PageInfo;
 
/* Prototype describing callback function for when a page is done. */
 typedef void
(*CRAWL_ScanPageStatusFunc)(void *data, CRAWL_PageInfo pageInfo);

/* Creates a page object which will be freed by the completion or abort stream routines (the
   URL_Struct is freed by the exit routine). Returns NULL if there is not enough memory.
   Parameters:

   siteURL - url of the site (used to make absolute url). Note this may be different than the
site that the page is on (in case we are crawling), in which case the pageURL should be absolute.
   pageURL - url of the page.
   cache - external cache. If non-null, the url will be cached here.
 */
CRAWL_PageInfo crawl_makePage(char *siteURL, char *pageURL, ExtCacheDBInfo *db);

/* Deallocates the list structures but not the list content (which should be done by the consumer).
   This is normally called internally by the completion and abort stream routines.
*/
void crawl_destroyPage(CRAWL_PageInfo page);


/* Requests the url from netlib and begins scanning. This function returns after the request 
   to netlib is issued. The callback function is called when page scanning is complete or aborts. 
   It is not guaranteed to be called in an out of memory situation.
*/
void crawl_scanPage(CRAWL_PageInfo page, MWContext *context, CRAWL_ScanPageStatusFunc func, void *data);

/****************************************************************************************/
/* accessors																			*/
/****************************************************************************************/

/* returns true if the page does not have a META tag that specifies that the page should
   not be indexed.
*/
PRBool crawl_pageCanBeIndexed(CRAWL_PageInfo pageInfo);

char* crawl_getPageURL(CRAWL_PageInfo pageInfo);

time_t crawl_getPageLastModified(CRAWL_PageInfo pageInfo);

URL_Struct* crawl_getPageURL_Struct(CRAWL_PageInfo pageInfo);

CRAWL_ItemList crawl_getPageLinks(CRAWL_PageInfo pageInfo);
uint16 crawl_getPageLinkCount(CRAWL_PageInfo pageInfo);

CRAWL_ItemList crawl_getPageImages(CRAWL_PageInfo pageInfo);
uint16 crawl_getPageImageCount(CRAWL_PageInfo pageInfo);

CRAWL_ItemList crawl_getPageResources(CRAWL_PageInfo pageInfo);
uint16 crawl_getPageResourceCount(CRAWL_PageInfo pageInfo);

CRAWL_ItemList crawl_getPageRequiredResources(CRAWL_PageInfo pageInfo);
uint16 crawl_getPageRequiredResourceCount(CRAWL_PageInfo pageInfo);

CRAWL_ItemList crawl_getPageFrames(CRAWL_PageInfo pageInfo);
uint16 crawl_getPageFrameCount(CRAWL_PageInfo pageInfo);

CRAWL_ItemList crawl_getPageLayers(CRAWL_PageInfo pageInfo);
uint16 crawl_getPageLayerCount(CRAWL_PageInfo pageInfo);

/****************************************************************************************/
/* stream function																		*/
/****************************************************************************************/

PUBLIC NET_StreamClass*
CRAWL_CrawlerConverter(int format_out,
					   void *data_object,
					   URL_Struct *URL_s,
					   MWContext  *window_id);
#endif
