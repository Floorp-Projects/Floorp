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
/*** crawler.h ****************************************************/
/*   description:		html crawler                              */


 /********************************************************************

	The crawler scans html pages and the links in those pages to a specified 
	depth in a breadth first manner, optionally caching them in an external cache. 
	Items are crawled sequentially - an item must finish being crawled or cached 
	before the next item is crawled. Multiple instances of the crawler may be running 
	at the same time.

	Depth = 1 means that only the initial page, and any images and resources that 
	it contains, are cached.

	Depth = n means that the crawler will crawl to n levels deep. Assuming the 
	maximum cache size is sufficient, the crawler will cache images and resources 
	for each page it encounters. Normally, resources are cached after all the pages 
	for a given level have been processed, but some resources are considered "required", 
	meaning they will be cached immediately after the page containing them has been 
	processed. An example of a "required" resource is a stylesheet.
 
	The crawler obeys the robots.txt directives on a site, which may allow or deny access 
	to specific urls or directories. The robots.txt file is by convention at the top level
	of a site.

	The type of links that are crawled are determined in pagescan.c.
	The parsing code is in htmparse.c
	The robots.txt parser is in robotxt.c

  $Revision: 1.1 $
  $Date: 1998/04/30 20:53:21 $

 *********************************************************************/

#ifndef crawler_h___
#define crawler_h___

#include "ntypes.h"		/* for MWContext */
#include "prtypes.h"	/* for PRBool */
#include "net.h"		/* for ExtCacheDBInfo, URL_Struct */

/* Error codes */
typedef PRUint16 CRAWL_Error;

#define CRAWL_CACHE_FULL		((CRAWL_Error)0x0001)
#define CRAWL_NO_MEMORY			((CRAWL_Error)0x0002)
#define CRAWL_SERVER_ERR		((CRAWL_Error)0x0004)
#define CRAWL_INTERRUPTED		((CRAWL_Error)0x0008)

/* these error codes indicate if and how the cache has been updated and are only
   set if CRAWL_MakeCrawler was called with manageCache set to true. Note that replaced
   links may not have been reported as such if the server does not provide a last
   modified date.
*/
#define CRAWL_NEW_LINK			((CRAWL_Error)0x0010)
#define CRAWL_REPLACED_LINK		((CRAWL_Error)0x0020)
#define CRAWL_REMOVED_LINK		((CRAWL_Error)0x0040)

/* Most of the APIs require a reference to CRAWL_Crawler, which is created by CRAWL_MakeCrawler. */
typedef struct _CRAWL_CrawlerStruct *CRAWL_Crawler;

/*
 * Typedef for a callback executed when an item has been processed.
 */
 typedef void
(PR_CALLBACK *CRAWL_PostProcessItemFn)(CRAWL_Crawler crawler, URL_Struct *url_s, PRBool isCached, void *data);

/*
 * Typedef for a callback executed when the crawler is done.
 */
 typedef void
(PR_CALLBACK *CRAWL_ExitFn)(CRAWL_Crawler crawler, void *data);

/****************************************************************************************/
/* public API																			*/
/****************************************************************************************/

NSPR_BEGIN_EXTERN_C

/* 
	Creates a crawler which may be used for one crawling request. Subsequent requests
	to crawl urls should use a separate crawler instance. Returns NULL if not enough
	memory is available, or the depth is less than 1.

  Parameters:
	context - needed by netlib (the crawler does not check this parameter)
	siteName - url of the site
	stayInSite - whether to restrict crawling to the site named.
	manageCache - whether to maintain a local file describing the cache contents. 
		If true, the crawler uses the file to remove dangling links from the cache
		the next time it is invoked with the same cache. This is not guaranteed to
		work correctly if another crawling instance uses the same cache simultaneously.
	cache - the external cache. This may be NULL if the crawled items do not need
		to be put in an external cache.
	postProcessItemFn - a function which is called after each item has been handled
		by netlib. This may be NULL.
	postProcessItemData - this data is supplied as a parameter to the postProcessItemFn
		and is opaque to the crawler. This may be NULL.
	exitFn - a function which is called when the crawler is done or has terminated
		prematurely (because the cache is full, or no memory is available). This may be NULL.
	exitData - this data is supplied as a parameter to the exitFn and is opaque to
		the crawler. This may be NULL.
*/
PR_EXTERN(CRAWL_Crawler) 
CRAWL_MakeCrawler(MWContext *context, 
										 char *siteName, 
										 uint8 depth, 
										 PRBool stayInSite,
										 PRBool manageCache,
										 ExtCacheDBInfo *cache,
										 CRAWL_PostProcessItemFn postProcessItemFn,
										 void *postProcessItemData,
										 CRAWL_ExitFn exitFn,
										 void *exitData);

/* 
	Destroys the crawler and all memory associated with it. The crawler instance should not be
	used after calling this function.
*/
PR_EXTERN(void) 
CRAWL_DestroyCrawler(CRAWL_Crawler crawler);

/* 
	Starts crawling from the url. If its content type is text/html, links may be traversed. This function
	returns as soon as the first network request is issued. 
*/
PR_EXTERN(void) 
CRAWL_StartCrawler(CRAWL_Crawler crawler, char *url);

/* 
	Stops crawling at the next link. This function returns immediately and cannot fail. 
*/
PR_EXTERN(void) 
CRAWL_StopCrawler(CRAWL_Crawler crawler);

/* 
	Returns the crawler error code. This function returns immediately and cannot fail. 
*/
PR_EXTERN(CRAWL_Error) 
CRAWL_GetError(CRAWL_Crawler crawler);

/* 
	Returns true if the crawler has stopped, which is the case before and after crawling. Returns
	immediately and cannot fail.
*/
PR_EXTERN(PRBool) 
CRAWL_IsStopped(CRAWL_Crawler crawler);

NSPR_END_EXTERN_C

/* 
	Stream function for crawling resources. Resources are not parsed, but the crawler checks the
	content length to see if the cache would be exceeded.
*/
PUBLIC NET_StreamClass*
CRAWL_CrawlerResourceConverter(int format_out,
								void *data_object,
								URL_Struct *URL_s,
								MWContext  *window_id);

#endif /* crawler_h___ */
