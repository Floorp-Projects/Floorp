/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*** crawler.c ****************************************************/
/*   description:		implementation for the html crawler.      */
  

 /********************************************************************

  15.Jan.98 mchung
	The crawler sometimes has the annoying behavior of thrashing when the cache is getting
	full. When the available space is less than a threshold (sizeSlop) crawling will stop,
	but the situation arises where we are outside the threshold and each file that comes
	in is larger. Netlib will pull down the file and immediately remove it because it would
	exceed the cache size. There are a couple of strategies for fixing this: increase the
	sizeSlop when a file is not cached, so we cross the threshold sooner, or stop caching
	after a certain number of files have not been cached, or some combination of these. 
	In any case, crawling should _not_ stop if no cache has been specified.

	The approach I am taking for now is to increase the sizeSlop (which lowers the threshold
	for the cache to be considered full) by SIZE_SLOP every time a file fails to be cached.
	This would alleviate the thrashing problem but would result in the threshold being
	lowered prematurely if the crawler encounters several large files which would exceed the cache,
	before the cache is nearly full, but the remainder of the files are "normal" size. The
	appropriate strategy really depends on how accurately the max cache size reflects the total
	content size (if not accurate we need to be more aggressive about lowering the threshold), 
	and how close to the cache limit we need to go.

	To do
	(maybe) One service the crawler might provide is informing its client that a url has
	changed. For a page this could also mean that one or more of its images or resources
	has changed. In order to do this it would be necessary to track which images and resources
	belong to which page(s) (or crawl them right after the page), find the url in the cache 
	and store their last modified date and content length before calling netlib to get the url.
	On return if the last modified date is later or the content length is different, the
	url has changed.

	(definitely) i18n of the parser

  $Revision: 1.2 $
  $Date: 1998/05/19 00:53:22 $

 *********************************************************************/

#include "xp.h"
#include "xp_str.h"
#include "xpassert.h"
#include "prio.h"
#include "prmem.h"
#include "plhash.h"
#include "prprf.h"
#include "robotxt.h"
#include "pagescan.h"
#include "crawler.h"

/* #define CRAWLERTEST */

#ifdef CRAWLERTEST
#include "fe_proto.h" /* FE_GetNetHelpContext */
#include "prinrval.h"
#include "mkextcac.h"
#endif

#define SIZE_SLOP ((uint32)2000)

typedef uint8 CRAWL_Status;
#define CRAWL_STOPPED			((CRAWL_Status)0x00)
#define CRAWL_STOP_REQUESTED	((CRAWL_Status)0x01)
#define CRAWL_RUNNING			((CRAWL_Status)0x02)

typedef uint8 CRAWL_CrawlerItemType;
#define CRAWLER_ITEM_TYPE_PAGE		((CRAWL_CrawlerItemType)0x00)
#define CRAWLER_ITEM_TYPE_IMAGE		((CRAWL_CrawlerItemType)0x01)
#define CRAWLER_ITEM_TYPE_RESOURCE	((CRAWL_CrawlerItemType)0x02)

typedef uint8 CRAWL_LinkStatus;

#define NEW_LINK		((CRAWL_LinkStatus)0x01)
#define REPLACED_LINK	((CRAWL_LinkStatus)0x02)
#define OLD_LINK		((CRAWL_LinkStatus)0x03)

typedef struct _CRAWL_LinkInfoStruc {
	time_t lastModifiedDate;
	CRAWL_LinkStatus status;
} CRAWL_LinkInfoStruc;

/*
 * Typedef for function callback (used internally)
 */
 typedef void
(*CRAWL_ProcessItemFunc)(CRAWL_Crawler crawler, char *url, CRAWL_RobotControl control, CRAWL_CrawlerItemType type);

typedef struct _CRAWL_ItemTable {
	CRAWL_ItemList items;
	uint16 count;
} CRAWL_ItemTable;

typedef struct _CRAWL_CrawlerStruct {
	char *siteName;
	char *siteHost;
	uint8 depth; /* how many levels to crawl */
	uint32 sizeSlop; /* size in bytes of the amount of space left in the cache for the cache
						to be considered full */

	CRAWL_ItemTable *linkedPagesTable; /* has entry for each depth containing all the page URLs */
	CRAWL_ItemTable *linkedImagesTable; /* has entry for each depth containing all the image URLs*/
	CRAWL_ItemTable *linkedResourcesTable; /* has entry for each depth containing all the resource URLs*/

	PLHashTable *pagesParsed; /* key is a url, value is last modified time */
	PLHashTable *imagesCached; /* key is a url, value is last modified time */
	PLHashTable *resourcesCached; /* key is a url, value is last modified time */

	CRAWL_Status status; /* is the crawler running? */
	CRAWL_Error error;

	uint8 currentDepth; /* starts at 1 */
	CRAWL_CrawlerItemType currentType; /* which type of item we're working on */
	uint16 itemIndex; /* which item in the table we're working on */
	CRAWL_ItemList requiredResources;
	uint16 numRequiredResources;

	/* determines what items crawler is allowed or disallowed to crawl at a given site.
	   key is a site name, value is RobotControl */
	PLHashTable *robotControlTable; 

	CRAWL_ItemList keys; /* keeps track of the hashtable keys (so they can be freed) */
	uint16 numKeys;
	uint16 sizeKeys;

	PRBool stayInSite;
	PRBool manageCache; /* maintain a file which lists the cached items so links unreferenced in the next update may be removed */
	MWContext *context; /* dummy context */
	ExtCacheDBInfo *cache; /* external cache */
	CRAWL_PostProcessItemFn postProcessItemFn;
	void *postProcessItemData;
	CRAWL_ExitFn exitFn;
	void *exitData;
} CRAWL_CrawlerStruct;

typedef struct _Crawl_DoProcessItemRecordStruct {
	CRAWL_Crawler crawler;
	CRAWL_CrawlerItemType type; /* which type of item we're working on */
	char *url;
	CRAWL_RobotControl control;
	CRAWL_ProcessItemFunc func;
} Crawl_DoProcessItemRecordStruct;

typedef Crawl_DoProcessItemRecordStruct *Crawl_DoProcessItemRecord;

extern void crawl_stringToLower(char *str);
extern int crawl_appendStringList(char ***list, uint16 *len, uint16 *size, char *str);

/* prototypes */
static PRBool crawl_hostEquals(char *pagehost, char *sitehost);
static void crawl_destroyItemTable(CRAWL_ItemTable *table);
static int crawl_appendToItemList(CRAWL_ItemList *list1, 
								   uint16 *numList1,
								   const CRAWL_ItemList list2, 
								   uint16 numList2);
static int crawl_insertInItemList(CRAWL_ItemList *list1, 
								   uint16 *numList1,
								   const CRAWL_ItemList list2, 
								   uint16 numList2,
								   uint16 pos);
static int crawl_destroyRobotControl(PLHashEntry *he, int i, void *arg);
static int crawl_destroyLinkInfo(PLHashEntry *he, int i, void *arg);
static PRBool crawl_cacheNearlyFull(CRAWL_Crawler crawler);
static void crawl_processLinkWithRobotControl(CRAWL_Crawler crawler, 
											   char *url, 
											   CRAWL_RobotControl control, 
											   CRAWL_CrawlerItemType type);
static int crawl_addCacheTableEntry(PLHashTable *ht, const char *key, time_t lastModifiedDate);
static void crawl_executePostProcessItemFn(CRAWL_Crawler crawler, URL_Struct *URL_s, PRBool isCached);
static void crawl_nonpage_exit(URL_Struct *URL_s, 
									   int status, 
									   MWContext *window_id, 
									   CRAWL_CrawlerItemType type);
static void crawl_cache_image_exit(URL_Struct *URL_s, int status, MWContext *window_id);
static void crawl_cache_resource_exit(URL_Struct *URL_s, int status, MWContext *window_id);
static void crawl_processItemWithRobotControl(CRAWL_Crawler crawler, 
											   char *url, 
											   CRAWL_RobotControl control, 
											   CRAWL_CrawlerItemType type);
static void crawl_processNextLink(CRAWL_Crawler crawler);
static void crawl_scanPageComplete(void *data, CRAWL_PageInfo pageInfo);
static PRBool crawl_isCrawlableURL(char *url);
static Crawl_DoProcessItemRecord crawl_makeDoProcessItemRecord(CRAWL_Crawler crawler, 
																char *url, 
																CRAWL_RobotControl control, 
																CRAWL_CrawlerItemType type, 
																CRAWL_ProcessItemFunc func);
static void crawl_doProcessItem(void *data);
static void crawl_processLink(CRAWL_Crawler crawler, 
							   PLHashTable *ht, 
							   char *url, 
							   CRAWL_ProcessItemFunc func, 
							   CRAWL_CrawlerItemType type);
static int crawl_writeCachedLinks(PLHashEntry *he, int i, void *arg);
static int crawl_writeCachedImages(PLHashEntry *he, int i, void *arg);
static int crawl_writeCachedResources(PLHashEntry *he, int i, void *arg);
static char* crawl_makeCacheInfoFilename(CRAWL_Crawler crawler);
static void crawl_writeCacheList(CRAWL_Crawler crawler);
static int crawl_processCacheInfoEntry(CRAWL_Crawler crawler, char *line, PLHashTable *ht);
static int crawl_processCacheInfoLine(CRAWL_Crawler crawler, char *line);
static void crawl_removeDanglingLinksFromCache(CRAWL_Crawler crawler);
static int crawl_updateCrawlerErrors(PLHashEntry *he, int i, void *arg);
static void crawl_crawlerFinish(CRAWL_Crawler crawler);
static void crawl_outOfMemory(CRAWL_Crawler crawler);
#ifdef CRAWLERTEST
void testCrawler(char *name, char *inURL, uint8 depth, uint32 maxSize, PRBool stayInSite);
static void myPostProcessFn(CRAWL_Crawler crawler, URL_Struct *url_s, PRBool isCached, void *data);
static void myExitFn(CRAWL_Crawler crawler, void *data);
#endif

PR_IMPLEMENT(PRBool) CRAWL_IsStopped(CRAWL_Crawler crawler) {
	return((crawler->status == CRAWL_STOPPED) ? PR_TRUE : PR_FALSE);
}

PR_IMPLEMENT(CRAWL_Error) CRAWL_GetError(CRAWL_Crawler crawler) {
	return(crawler->error);
}

/* returns true if the host names are the same.

   The arguments are assumed to be already converted to lower case.

   If the hostname of the page is a substring of the hostname of the site, or
   vice versa, return true. For example, w3 and w3.mcom.com

   This would fail if for example an intranet had a server called netscape. Then any
   pages www.netscape.com would be considered to be on that server.

   If the domain names are the same, return true. The domain name is extracted by taking the
   substring after the first dot. The domain name must contain another dot (www.yahoo.com and
   search.yahoo.com are considered equivalent, but foo.org and bar.org are not).

   This fails to compare id addresses to host names
*/
static PRBool 
crawl_hostEquals(char *pagehost, char *sitehost) {
	if ((PL_strstr(sitehost, pagehost) != NULL) || (PL_strstr(pagehost, sitehost) != NULL))
		return PR_TRUE;
	else {
		char *pageDomain = PL_strchr(pagehost, '.');
		char *siteDomain = PL_strchr(sitehost, '.');
		if ((pageDomain != NULL) && (siteDomain != NULL)) {
			char *pageDomainType = PL_strchr(pageDomain+1, '.');
			char *siteDomainType = PL_strchr(siteDomain+1, '.');
			if ((pageDomainType != NULL) &&
				(siteDomainType != NULL) &&
				(PL_strcmp(pageDomain+1, siteDomain+1) == 0)) {
				return PR_TRUE;
			}
		}
	}
	return PR_FALSE;
}

static void 
crawl_destroyItemTable(CRAWL_ItemTable *table) {
	uint16 count;
	for (count = 0; count < table->count; count++) {
		char *item = *(table->items + count);
		if (item != NULL) PR_Free(item);
	}
	if (table->items != NULL) PR_Free(table->items);
}

/* appends list2 to the end of list1. Returns -1 if no memory */
static int 
crawl_appendToItemList(CRAWL_ItemList *list1, 
								 uint16 *numList1,
								 const CRAWL_ItemList list2, 
								 uint16 numList2) {
	/* this memory freed in CRAWL_DestroyCrawler */
	CRAWL_ItemList newList = (CRAWL_ItemList)PR_Malloc(sizeof(CRAWL_Item) * (*numList1 + numList2));
	CRAWL_ItemList old = *list1;
	if (newList == NULL) return -1;
	memcpy((char*)newList, (char*)*list1, sizeof(CRAWL_Item) * (*numList1)); /* copy first list */
	memcpy((char*)(newList + *numList1), (char*)list2, sizeof(CRAWL_Item) * numList2);
	*list1 = newList;
	*numList1 += numList2;
	if (old != NULL) PR_Free(old);
	return 0;
}

/* inserts list2 at (zero-indexed) position in list1. Returns -1 if no memory. */
static int 
crawl_insertInItemList(CRAWL_ItemList *list1, 
								 uint16 *numList1,
								 const CRAWL_ItemList list2, 
								 uint16 numList2,
								 uint16 pos) {
	/* this memory freed in CRAWL_DestroyCrawler */
	CRAWL_ItemList newList = (CRAWL_ItemList)PR_Malloc(sizeof(CRAWL_Item) * (*numList1 + numList2));
	CRAWL_ItemList old = *list1;
	if (newList == NULL) return -1;
	memcpy((char*)newList, (char*)*list1, sizeof(CRAWL_Item) * pos); /* copy first list up to pos */
	memcpy((char*)(newList + pos), (char*)list2, sizeof(CRAWL_Item) * numList2); /* copy second list */
	memcpy((char*)(newList + pos + numList2), (char*)(*list1 + pos), sizeof(CRAWL_Item) * (*numList1 - pos));
	*list1 = newList;
	*numList1 += numList2;
	if (old != NULL) PR_Free(old);
	return 0;
}

PR_IMPLEMENT(CRAWL_Crawler) 
CRAWL_MakeCrawler(MWContext *context, 
						  char *siteName, 
						  uint8 depth, 
						  PRBool stayInSite,
						  PRBool manageCache,
						  ExtCacheDBInfo *cache,
						  CRAWL_PostProcessItemFn postProcessItemFn,
						  void *postProcessItemData,
						  CRAWL_ExitFn exitFn,
						  void *exitData) {
	CRAWL_Crawler crawler;
	if (depth < 1) return NULL;
	crawler = PR_NEWZAP(CRAWL_CrawlerStruct);
	if (crawler == NULL) return NULL;
	crawler->siteName = PL_strdup(siteName);
	crawl_stringToLower(crawler->siteName);
	crawler->siteHost = NET_ParseURL(crawler->siteName, GET_PROTOCOL_PART | GET_HOST_PART);
	crawler->depth = depth;
	crawler->sizeSlop = SIZE_SLOP;
	crawler->stayInSite = stayInSite;
	crawler->manageCache = manageCache;
	/* this memory freed in CRAWL_DestroyCrawler */
	crawler->linkedPagesTable = (CRAWL_ItemTable*)PR_Calloc(depth+1, sizeof(CRAWL_ItemTable));
	crawler->linkedImagesTable = (CRAWL_ItemTable*)PR_Calloc(depth+1, sizeof(CRAWL_ItemTable));
	crawler->linkedResourcesTable = (CRAWL_ItemTable*)PR_Calloc(depth+1, sizeof(CRAWL_ItemTable));
	if (crawler->linkedPagesTable == NULL ||
		crawler->linkedImagesTable == NULL ||
		crawler->linkedResourcesTable == NULL)
		return NULL;
	crawler->pagesParsed = PL_NewHashTable(100, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL); 
	crawler->imagesCached = PL_NewHashTable(100, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL); 
	crawler->resourcesCached = PL_NewHashTable(100, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL); 
	crawler->robotControlTable = PL_NewHashTable(50, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL); 
	crawler->context = context;
	crawler->cache = cache;
	crawler->status = CRAWL_STOPPED;
	crawler->postProcessItemFn = postProcessItemFn;
	crawler->postProcessItemData = postProcessItemData;
	crawler->exitFn = exitFn;
	crawler->exitData = exitData;
	return crawler;
}

/* an enumerator function for the robotControlTable hashtable - maybe this should be moved to an allocator op */
static int 
crawl_destroyRobotControl(PLHashEntry *he, int i, void *arg) {
#if defined(XP_MAC)
#pragma unused(i, arg)
#endif
	CRAWL_DestroyRobotControl((CRAWL_RobotControl)he->value);
	return HT_ENUMERATE_NEXT;
}

/* an enumerator function for the cache tables */
static int
crawl_destroyLinkInfo(PLHashEntry *he, int i, void *arg) {
#if defined(XP_MAC)
#pragma unused(i, arg)
#endif
	if (he->value != NULL) PR_Free(he->value);
	return HT_ENUMERATE_NEXT;
}

PR_IMPLEMENT(void) 
CRAWL_DestroyCrawler(CRAWL_Crawler crawler) {
	int i;
	if (crawler->siteName != NULL) PR_DELETE(crawler->siteName);
	if (crawler->siteHost != NULL) PR_DELETE(crawler->siteHost);
	PL_HashTableEnumerateEntries(crawler->pagesParsed, crawl_destroyLinkInfo, NULL);
	PL_HashTableEnumerateEntries(crawler->imagesCached, crawl_destroyLinkInfo, NULL);
	PL_HashTableEnumerateEntries(crawler->resourcesCached, crawl_destroyLinkInfo, NULL);
	PL_HashTableEnumerateEntries(crawler->robotControlTable, crawl_destroyRobotControl, NULL);
	for (i = 0; i < crawler->depth; i++) {
		crawl_destroyItemTable(crawler->linkedPagesTable + i);
		crawl_destroyItemTable(crawler->linkedImagesTable + i);
		crawl_destroyItemTable(crawler->linkedResourcesTable + i);
	}
	PR_DELETE(crawler->linkedPagesTable);
	PR_DELETE(crawler->linkedImagesTable);
	PR_DELETE(crawler->linkedResourcesTable);
	PL_HashTableDestroy(crawler->pagesParsed);
	PL_HashTableDestroy(crawler->imagesCached);
	PL_HashTableDestroy(crawler->resourcesCached);
	PL_HashTableDestroy(crawler->robotControlTable);
	for (i = 0; i < crawler->numKeys; i++) {
		PR_Free(crawler->keys[i]); /* these were created with PL_strdup so use PR_Free */
	}
	if (crawler->keys != NULL) PR_DELETE(crawler->keys);
	PR_DELETE(crawler);
}

/* returns true if the cache is almost full (libnet insures that DiskCacheSize won't exceed MaxSize)
*/
static PRBool 
crawl_cacheNearlyFull(CRAWL_Crawler crawler) {
	if ((crawler->cache != NULL) &&
		(crawler->cache->MaxSize - crawler->cache->DiskCacheSize <= crawler->sizeSlop))
		return PR_TRUE;
	else return PR_FALSE;
}

/* error handling for no memory, for situations where we want to exit right away. */
static void 
crawl_outOfMemory(CRAWL_Crawler crawler) {
	crawler->error |= CRAWL_NO_MEMORY;
	crawl_crawlerFinish(crawler);
}

/* scans a page if robots.txt allows it */
static void 
crawl_processLinkWithRobotControl(CRAWL_Crawler crawler, 
									char *url, 
									CRAWL_RobotControl control, 
									CRAWL_CrawlerItemType type) {
	PR_ASSERT(type == CRAWLER_ITEM_TYPE_PAGE);
	if (CRAWL_GetRobotControl(control, url) == CRAWL_ROBOT_DISALLOWED) {
		crawl_processNextLink(crawler);
	} else {
		CRAWL_PageInfo pageInfo = crawl_makePage(crawler->siteName, url, crawler->cache);
		if (pageInfo != NULL) {
			crawl_scanPage(pageInfo, crawler->context, crawl_scanPageComplete, crawler);
		} else crawl_outOfMemory(crawler);
	}
}

/* add an entry to the table of items cached with the last modified time */
static int
crawl_addCacheTableEntry(PLHashTable *ht, const char *key, time_t lastModifiedDate) {
	CRAWL_LinkInfoStruc *info = PR_NEWZAP(CRAWL_LinkInfoStruc);
	if (info == NULL) return -1;
	info->lastModifiedDate = lastModifiedDate;
	info->status = NEW_LINK;
	PL_HashTableAdd(ht, key, (void*)info);
	return 0;
}

static void crawl_executePostProcessItemFn(CRAWL_Crawler crawler, URL_Struct *URL_s, PRBool isCached) {
	if (!isCached) crawler->sizeSlop += SIZE_SLOP; /* lower the threshold for cache fullness */
	if (crawler->postProcessItemFn != NULL) 
		(crawler->postProcessItemFn)(crawler, URL_s, isCached, crawler->postProcessItemData);
}

/* common completion code for images and resources */
static void
crawl_nonpage_exit(URL_Struct *URL_s, int status, MWContext *window_id, CRAWL_CrawlerItemType type) {
#if defined(XP_MAC)
#pragma unused(window_id)
#endif	
	int err = 0;
	CRAWL_Crawler crawler = (CRAWL_Crawler)URL_s->owner_data;
	PLHashTable *table = NULL;
	switch(type) {
	case CRAWLER_ITEM_TYPE_IMAGE:
		table = crawler->imagesCached;
		break;
	case CRAWLER_ITEM_TYPE_RESOURCE:
		table = crawler->resourcesCached;
		break;
	default:
		break;
	}
	PR_ASSERT(table != NULL);

	if (URL_s->server_status >= 400) crawler->error |= CRAWL_SERVER_ERR;
	/* add to the images cached if we are in fact caching and the cache_file is set */
	if ((status >= 0) && ((crawler->cache == NULL) || (URL_s->cache_file != NULL))) {
		char *url = PL_strdup(URL_s->address);
		if (url == NULL) {
			crawl_outOfMemory(crawler);
			return;
		}
		err = crawl_addCacheTableEntry(table, url, URL_s->last_modified);
		crawl_executePostProcessItemFn(crawler, URL_s, PR_TRUE);
		if (err == 0)
			err = crawl_appendStringList(&crawler->keys, &crawler->numKeys, &crawler->sizeKeys, url);
	} else {
		crawl_executePostProcessItemFn(crawler, URL_s, PR_FALSE);
	}

	if (status != MK_CHANGING_CONTEXT)
		NET_FreeURLStruct(URL_s);

	if (err == 0) crawl_processNextLink(crawler);
	else crawl_outOfMemory(crawler); /* alert! assumes any error code returned means out of memory */
}

/* exit routine for NET_GetURL for images */
static void
crawl_cache_image_exit(URL_Struct *URL_s, int status, MWContext *window_id)
{
	crawl_nonpage_exit(URL_s, status, window_id, CRAWLER_ITEM_TYPE_IMAGE);
}

/* exit routine for NET_GetURL for resources */
static void
crawl_cache_resource_exit(URL_Struct *URL_s, int status, MWContext *window_id)
{
	crawl_nonpage_exit(URL_s, status, window_id, CRAWLER_ITEM_TYPE_RESOURCE);
}

/* caches an image or resource if robots.txt allows it */
static 
void crawl_processItemWithRobotControl(CRAWL_Crawler crawler, 
									char *url, 
									CRAWL_RobotControl control, 
									CRAWL_CrawlerItemType type) {
	if (CRAWL_GetRobotControl(control, url) == CRAWL_ROBOT_DISALLOWED) {
		crawl_processNextLink(crawler);
	} else if (crawler->cache != NULL) {
		URL_Struct *url_s;
		url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD);
		if (url_s == NULL) crawl_outOfMemory(crawler);
		url_s->load_background = PR_TRUE;
		url_s->SARCache = crawler->cache;
		url_s->owner_data = crawler;
		switch (type) {
		case CRAWLER_ITEM_TYPE_IMAGE:
			NET_GetURL(url_s, FO_CACHE_AND_CRAWL_RESOURCE, crawler->context, crawl_cache_image_exit);
			break;
		case CRAWLER_ITEM_TYPE_RESOURCE:
			NET_GetURL(url_s, FO_CACHE_AND_CRAWL_RESOURCE, crawler->context, crawl_cache_resource_exit);
			break;
		}
	}
}

/* Process the next link in the table at the current depth.
   Pages at the previous depth are scanned, and then images and resources at the current depth
   are cached.
*/
#ifndef DEFER_RESOURCE_SCAN

static
void crawl_processNextLink(CRAWL_Crawler crawler) {
	PRBool allDone = PR_FALSE;
	PLHashTable *completedTable;
	CRAWL_ProcessItemFunc func = NULL;
	static uint16 requiredIndex = 0;

	/* parse all the pages at the previous depth, (this includes also frames and layers for
	   the most recently scanned page) and then process the images and resources
	   at the current depth.
	*/
	if (crawler->currentDepth <= crawler->depth) {
		CRAWL_ItemTable *table;
		switch (crawler->currentType) {
		case CRAWLER_ITEM_TYPE_PAGE:
			/* if the previous page had any required resources, cache them now. */
			if (crawler->requiredResources != NULL) {
				PR_LogPrint(("required resources"));
				if (requiredIndex < crawler->numRequiredResources) {
					crawl_processLink(crawler, 
									crawler->resourcesCached, 
									*(crawler->requiredResources + requiredIndex++), 
									crawl_processItemWithRobotControl,
									CRAWLER_ITEM_TYPE_RESOURCE);
					return; /* wait for callback */
				} else {
					uint16 i;
					for (i = 0; i < crawler->numRequiredResources; i++) {
						PR_Free(*(crawler->requiredResources + i));
					}
					requiredIndex = crawler->numRequiredResources = 0;
					PR_DELETE(crawler->requiredResources);
				}
			}
			/* process the pages at the previous level */
			table = crawler->linkedPagesTable + crawler->currentDepth - 1;
			func = crawl_processLinkWithRobotControl;
			completedTable = crawler->pagesParsed;
			if (crawler->itemIndex == table->count) { /* no more items */
				/* done with the pages, now do the images */
				PR_LogPrint(("finished pages"));
				func = crawl_processItemWithRobotControl;
				crawler->currentType = CRAWLER_ITEM_TYPE_IMAGE;
				completedTable = crawler->imagesCached;
				table = crawler->linkedImagesTable + crawler->currentDepth;
				crawler->itemIndex = 0;
			}
			break;
		case CRAWLER_ITEM_TYPE_IMAGE: 
			table = crawler->linkedImagesTable + crawler->currentDepth;
			func = crawl_processItemWithRobotControl;
			completedTable = crawler->imagesCached;
			if (crawler->itemIndex == table->count) { /* no more items */
				PR_LogPrint(("finished images"));
				/* done with the images, now do the resources */
				func = crawl_processItemWithRobotControl;
				crawler->currentType = CRAWLER_ITEM_TYPE_RESOURCE;
				completedTable = crawler->resourcesCached;
				table = crawler->linkedResourcesTable + crawler->currentDepth;
				crawler->itemIndex = 0;
			}
			break;
		case CRAWLER_ITEM_TYPE_RESOURCE: 
			table = crawler->linkedResourcesTable + crawler->currentDepth;
			func = crawl_processItemWithRobotControl;
			completedTable = crawler->resourcesCached;
			if (crawler->itemIndex == table->count) { /* no more items */
				PR_LogPrint(("finished resources"));
				if (crawler->currentDepth == crawler->depth) {
					allDone = PR_TRUE;
					break;
				}
				/* done with the resources, now go to next level */
				func = crawl_processLinkWithRobotControl;
				crawler->currentType = CRAWLER_ITEM_TYPE_PAGE;
				completedTable = crawler->pagesParsed;
				crawler->currentDepth++;
				PR_LogPrint(("depth = %d", crawler->currentDepth));
				crawler->itemIndex = 0;
				table = crawler->linkedPagesTable + crawler->currentDepth - 1;
			}
			break;
		}
		if (!allDone) {
			if (table->count == crawler->itemIndex) crawl_processNextLink(crawler); /* new table is empty */
			else {
				crawl_processLink(crawler, 
								completedTable, 
								*(table->items + (crawler->itemIndex++)), 
								func, 
								crawler->currentType);
			}
		} else {
			crawl_crawlerFinish(crawler);
		}
	}
}

#else

/* this version traverses the tree like Netcaster 1.0: all the pages at all the depths, then all the
   images at all the depths, and then all the resources at all the depths.
*/
static
void crawl_processNextLink(CRAWL_Crawler crawler) {
	PRBool allDone = PR_FALSE;
	PLHashTable *completedTable;
	CRAWL_ProcessItemFunc func = NULL;
	static uint16 requiredIndex = 0;

	/* parse all the pages at the previous depth, (this includes also frames and layers for
	   the most recently scanned page) and then process the images and resources
	   at the current depth.
	*/
	if (crawler->currentDepth <= crawler->depth) {
		CRAWL_ItemTable *table;
		switch (crawler->currentType) {
		case CRAWLER_ITEM_TYPE_PAGE:
			/* if the previous page had any required resources, cache them now. */
			if (crawler->requiredResources != NULL) {
				PR_LogPrint(("required resources"));
				if (requiredIndex < crawler->numRequiredResources) {
					crawl_processLink(crawler, 
									crawler->resourcesCached, 
									*(crawler->requiredResources + requiredIndex++), 
									crawl_processItemWithRobotControl,
									CRAWLER_ITEM_TYPE_RESOURCE);
					return; /* wait for callback */
				} else {
					uint16 i;
					for (i = 0; i < crawler->numRequiredResources; i++) {
						PR_Free(*(crawler->requiredResources + i));
					}
					requiredIndex = crawler->numRequiredResources = 0;
					PR_DELETE(crawler->requiredResources);
				}
			}
			/* process the pages at the previous level */
			table = crawler->linkedPagesTable + crawler->currentDepth - 1;
			func = crawl_processLinkWithRobotControl;
			completedTable = crawler->pagesParsed;
			if (crawler->itemIndex == table->count) { /* no more items */
				/* done with the pages at this level, now go to next level */
				if (crawler->currentDepth < crawler->depth) {
					crawler->currentDepth++;
					PR_LogPrint(("depth = %d", crawler->currentDepth));
					crawler->itemIndex = 0;
				} else {
					/* done with pages, now do images */
					crawler->currentDepth = 1;
					crawler->itemIndex = 0;
					func = crawl_processItemWithRobotControl;
					crawler->currentType = CRAWLER_ITEM_TYPE_IMAGE;
					completedTable = crawler->imagesCached;
					table = crawler->linkedImagesTable + crawler->currentDepth;
				}
			}
			break;
		case CRAWLER_ITEM_TYPE_IMAGE: 
			table = crawler->linkedImagesTable + crawler->currentDepth;
			func = crawl_processItemWithRobotControl;
			completedTable = crawler->imagesCached;
			if (crawler->itemIndex == table->count) { /* no more items */
				if (crawler->currentDepth < crawler->depth) {
					crawler->currentDepth++;
					PR_LogPrint(("depth = %d", crawler->currentDepth));
					crawler->itemIndex = 0;
				} else {
					/* done with the images, now do the resources */
					crawler->currentDepth = 1;
					crawler->itemIndex = 0;
					func = crawl_processItemWithRobotControl;
					crawler->currentType = CRAWLER_ITEM_TYPE_RESOURCE;
					completedTable = crawler->resourcesCached;
					table = crawler->linkedResourcesTable + crawler->currentDepth;
					crawler->itemIndex = 0;
				}
			}
			break;
		case CRAWLER_ITEM_TYPE_RESOURCE: 
			table = crawler->linkedResourcesTable + crawler->currentDepth;
			func = crawl_processItemWithRobotControl;
			completedTable = crawler->resourcesCached;
			if (crawler->itemIndex == table->count) { /* no more items */
				if (crawler->currentDepth < crawler->depth) {
					crawler->currentDepth++;
					PR_LogPrint(("depth = %d", crawler->currentDepth));
					crawler->itemIndex = 0;
				} else {
					allDone = PR_TRUE;
					break;
				}
			}
			break;
		}
		if (!allDone) {
			if (table->count == crawler->itemIndex) crawl_processNextLink(crawler); /* new table is empty */
			else {
				crawl_processLink(crawler, 
								completedTable, 
								*(table->items + (crawler->itemIndex++)), 
								func, 
								crawler->currentType);
			}
		} else {
			crawl_crawlerFinish(crawler);
		}
	}
}
#endif

/* adds links from the page just parsed to the appropriate table, and continue.
   This is a completion routine for the page scan.
*/
static
void crawl_scanPageComplete(void *data, CRAWL_PageInfo pageInfo) {
	int err = 0;
	CRAWL_Crawler crawler = (CRAWL_Crawler)data;
	URL_Struct *url_s = crawl_getPageURL_Struct(pageInfo);
	char *url = PL_strdup(crawl_getPageURL(pageInfo));

	if (url == NULL) crawl_outOfMemory(crawler);

	if (url_s->server_status >= 400) crawler->error |= CRAWL_SERVER_ERR;

	/* add url to pages parsed only if it was actually cached - this might mean that
	   we would parse the url again if encountered, but this should only be encountered 
	   when the cache is full, and we're about to quit.
	*/
	if (crawl_pageCanBeIndexed(pageInfo)) { /* no meta robots tag directing us not to index, i.e. cache */
		if ((crawler->cache == NULL) || (url_s->cache_file != NULL)) { /* was cached, or not cache specified */
			err = crawl_addCacheTableEntry(crawler->pagesParsed, url, crawl_getPageLastModified(pageInfo));
			crawl_executePostProcessItemFn(crawler, url_s, PR_TRUE);
			if (err == 0)
				err = crawl_appendStringList(&crawler->keys, &crawler->numKeys, &crawler->sizeKeys, url);
		} else { /* wasn't cached */
			crawl_executePostProcessItemFn(crawler, url_s, PR_FALSE);
		}
	} else { /* obey meta robots tag and remove from cache. */
		NET_RemoveURLFromCache(url_s);
	    if (crawler->postProcessItemFn != NULL) 
			(crawler->postProcessItemFn)(crawler, url_s, PR_FALSE, crawler->postProcessItemData);
	}

	if ((crawl_getPageLinks(pageInfo) != NULL) && (err == 0)) {
		/* add links to pages at depth */
		err = crawl_appendToItemList(&(crawler->linkedPagesTable + crawler->currentDepth)->items,
									&(crawler->linkedPagesTable + crawler->currentDepth)->count,
									crawl_getPageLinks(pageInfo),
									crawl_getPageLinkCount(pageInfo));
	}
	if ((crawl_getPageImages(pageInfo) != NULL) && (err == 0)) {
		/* add images to images at depth */
		err = crawl_appendToItemList(&(crawler->linkedImagesTable + crawler->currentDepth)->items,
									&(crawler->linkedImagesTable + crawler->currentDepth)->count,
									crawl_getPageImages(pageInfo),
									crawl_getPageImageCount(pageInfo));
	}
	if ((crawl_getPageResources(pageInfo) != NULL) && (err == 0)) {
		/* add resources to resources at depth */
		err = crawl_appendToItemList(&(crawler->linkedResourcesTable + crawler->currentDepth)->items,
									&(crawler->linkedResourcesTable + crawler->currentDepth)->count,
									crawl_getPageResources(pageInfo),
									crawl_getPageResourceCount(pageInfo));
	}
	if ((crawl_getPageFrames(pageInfo) != NULL) && (err == 0)) {
		/* add frames to pages currently being processed (next link will be a frame) */
		err = crawl_insertInItemList(&(crawler->linkedPagesTable + crawler->currentDepth - 1)->items,
									&(crawler->linkedPagesTable + crawler->currentDepth - 1)->count,
									crawl_getPageFrames(pageInfo),
									crawl_getPageFrameCount(pageInfo),
									crawler->itemIndex);
	}
	if ((crawl_getPageLayers(pageInfo) != NULL) && (err == 0)){
		/* add layers to pages currently being processed */
		err = crawl_insertInItemList(&(crawler->linkedPagesTable + crawler->currentDepth - 1)->items,
									&(crawler->linkedPagesTable + crawler->currentDepth - 1)->count,
									crawl_getPageLayers(pageInfo),
									crawl_getPageLayerCount(pageInfo),
									crawler->itemIndex);
	}
	if ((crawl_getPageRequiredResources(pageInfo) != NULL) && (err == 0)) {
		err = crawl_appendToItemList(&crawler->requiredResources,
									&crawler->numRequiredResources,
									crawl_getPageRequiredResources(pageInfo),
									crawl_getPageRequiredResourceCount(pageInfo));
	}
	/* crawler->currentPage = pageInfo; */
	if (err == 0) crawl_processNextLink(crawler);
	else crawl_outOfMemory(crawler);
}

/* returns false if the url is empty or contains any entities */
static
PRBool crawl_isCrawlableURL(char *url) {
	char *amp, *semicolon;
	if (*url == '\0') return PR_FALSE;
	amp = PL_strchr(url, '&');
	if (amp != NULL) {
		semicolon = PL_strchr(amp, ';');
		if (semicolon != NULL) return PR_FALSE; /* don't crawl any url with entities */
	}
	return PR_TRUE;
}

/* callback structure for the robots.txt parser, freed in robotxt.c */
static Crawl_DoProcessItemRecord 
crawl_makeDoProcessItemRecord(CRAWL_Crawler crawler, char *url, CRAWL_RobotControl control, CRAWL_CrawlerItemType type, CRAWL_ProcessItemFunc func) {
	Crawl_DoProcessItemRecord rec;
	rec = (Crawl_DoProcessItemRecord)PR_Malloc(sizeof(Crawl_DoProcessItemRecordStruct));
	rec->crawler = crawler;
	rec->control = control;
	rec->url = url;
	rec->type = type;
	rec->func = func;
	return rec;
}

/* callback for the robots.txt parser */
static void 
crawl_doProcessItem(void *data) {
	Crawl_DoProcessItemRecord rec = (Crawl_DoProcessItemRecord)data;
	rec->func(rec->crawler, rec->url, rec->control, rec->type);
}

/* processes a link (page, image, or resource).
*/
static void 
crawl_processLink(CRAWL_Crawler crawler, 
					PLHashTable *ht, 
					char *url, 
					CRAWL_ProcessItemFunc func, 
					CRAWL_CrawlerItemType type) {
	CRAWL_RobotControl control;
	char *siteURL;
	PLHashNumber keyHash;
    PLHashEntry *he, **hep;

	if (crawler->status == CRAWL_STOP_REQUESTED) {
		crawler->error |= CRAWL_INTERRUPTED;
		crawl_crawlerFinish(crawler); /* stop update */
		return;
	}

	if (crawl_cacheNearlyFull(crawler)) {
		PR_LogPrint(("crawl_processLink: cache is full, stopping"));
		crawler->error |= CRAWL_CACHE_FULL;
		crawl_crawlerFinish(crawler); /* stop update */
		return;
	}
	
	if (!crawl_isCrawlableURL(url)) {
		crawl_processNextLink(crawler);
		return;
	}

	/* check if already processed - use raw lookup because value can be 0 */
	keyHash = (*ht->keyHash)(url);
	hep = PL_HashTableRawLookup(ht, keyHash, url);
	if ((he = *hep) != 0) {
		crawl_processNextLink(crawler);
		return;
	}

	siteURL = NET_ParseURL(url, GET_PROTOCOL_PART | GET_HOST_PART); /* PR_Malloc'd */
	crawl_stringToLower(siteURL);

	if (crawler->stayInSite && !crawl_hostEquals(siteURL, crawler->siteHost)) {
		PR_Free(siteURL);
		crawl_processNextLink(crawler); /* skip this item */
		return;
	}

	/* get robot directives for this site, creating if it doesn't exist */
	control = PL_HashTableLookup(crawler->robotControlTable, siteURL);
	if (control == NULL) {
		control = CRAWL_MakeRobotControl(crawler->context, siteURL);
		if (control != NULL) {
			Crawl_DoProcessItemRecord rec;
			PL_HashTableAdd(crawler->robotControlTable, siteURL, control);
			/* keep a separate list of the hosts around so we can free them later on */
			if (crawl_appendStringList(&crawler->keys, &crawler->numKeys, &crawler->sizeKeys, siteURL) < 0) {
				crawl_outOfMemory(crawler);
				return;
			}
			/* if we successfully issue request for robots.txt, return (link processed in callback),
			 , otherwise process it now. */
			rec = crawl_makeDoProcessItemRecord(crawler, url, control, type, func);
			if (rec == NULL) {
				crawl_outOfMemory(crawler);
				return;
			}
			if (CRAWL_ReadRobotControlFile(control, crawl_doProcessItem, rec, PR_TRUE)) return; /* wait for the callback */
		} else { 
			PR_Free(siteURL);
			crawl_outOfMemory(crawler);
			return;
		}
	} else PR_Free(siteURL); /* we found a robot control */

	if (control != NULL) {
		func(crawler, url, control, type);
	}
}

/* starts crawling from the url specified */
PR_IMPLEMENT(void) 
CRAWL_StartCrawler(CRAWL_Crawler crawler, char *url) {
	crawler->currentDepth = 1;
	crawler->status = CRAWL_RUNNING;
	/* just assume it's a page for now. The crawler converter won't attempt to
	   parse it if the mime type is not text/html. */
	crawl_processLink(crawler, 
											crawler->pagesParsed, 
											url, 
											crawl_processLinkWithRobotControl, 
											CRAWLER_ITEM_TYPE_PAGE);
}

/* stops crawling safely */
PR_IMPLEMENT(void) 
CRAWL_StopCrawler(CRAWL_Crawler crawler) {
	crawler->status = CRAWL_STOP_REQUESTED;
}

/****************************************************************************************/
/* cache management using .dat file														*/
/****************************************************************************************/

static int 
crawl_writeCachedLinks(PLHashEntry *he, int i, void *arg) {
#if defined(XP_MAC)
#pragma unused(i)
#endif	
	PRFileDesc *fd = (PRFileDesc *)arg;
	PR_fprintf(fd, "L>%s/%lu\n", he->key, he->value);
	return HT_ENUMERATE_NEXT;
}

static int 
crawl_writeCachedImages(PLHashEntry *he, int i, void *arg) {
#if defined(XP_MAC)
#pragma unused(i)
#endif	
	PRFileDesc *fd = (PRFileDesc *)arg;
	PR_fprintf(fd, "I>%s/%lu\n", he->key, he->value);
	return HT_ENUMERATE_NEXT;
}

static int 
crawl_writeCachedResources(PLHashEntry *he, int i, void *arg) {
#if defined(XP_MAC)
#pragma unused(i)
#endif	
	PRFileDesc *fd = (PRFileDesc *)arg;
	PR_fprintf(fd, "R>%s/%lu\n", he->key, he->value);
	return HT_ENUMERATE_NEXT;
}

/* result is malloc'd on Windows, not on Mac or UNIX.
   crawler->cache is assumed to be non-null. */
static char*
crawl_makeCacheInfoFilename(CRAWL_Crawler crawler) {
#if defined(XP_MAC)
	OSErr ConvertMacPathToUnixPath(const char *macPath, char **unixPath);
#endif	
	char *tmp = NULL, *tmpName, *dot, *filename;

	tmp = (char *)PR_MALLOC(PL_strlen(crawler->cache->filename) + 5); /* +5 for .dat and null termination */
	PL_strcpy(tmp, crawler->cache->filename);
	if (tmp == NULL) return NULL;
	dot = PL_strchr(tmp, '.');
	if (dot != NULL) *dot = '\0';
	PL_strcat(tmp, ".dat");
	tmpName = WH_FileName(tmp, xpSARCache);
#ifndef XP_MAC
	filename = WH_FilePlatformName(tmpName);
#else
	/* unfortunately PR_Open doesn't like the output of WH_FileName, we have to
	   convert it to a Unix path, or use the XP_File routines.
	*/
	/* filename = tmpName; */
	ConvertMacPathToUnixPath(tmpName, &filename);
#endif
	PR_Free(tmp);
	return filename;
}

/* Writes list of all links, images, and resources that were cached by the crawler.
   crawler->cache is assumed to be non-null.
*/
static void 
crawl_writeCacheList(CRAWL_Crawler crawler) {
	PRFileDesc *fd;
	char *filename = crawl_makeCacheInfoFilename(crawler);
	if (filename != NULL) {
		PR_Delete(filename);
		fd = PR_Open(filename,  PR_CREATE_FILE | PR_RDWR, 0644);
		if(fd == NULL) return;
		/* write here */
		PL_HashTableEnumerateEntries(crawler->pagesParsed, crawl_writeCachedLinks, fd);
		PL_HashTableEnumerateEntries(crawler->imagesCached, crawl_writeCachedImages, fd);
		PL_HashTableEnumerateEntries(crawler->resourcesCached, crawl_writeCachedResources, fd);
		PR_Close(fd);
#ifdef XP_WIN
		PR_Free(filename); /* WH_FilePlatformName malloc's on Win but not Mac and X */
#endif
	} else crawler->error |= CRAWL_NO_MEMORY;
}

/* if the cache item specified in the line does not exist in the table, remove it from the cache 
   returns -1 if no memory, 0 for no error. */
static int 
crawl_processCacheInfoEntry(CRAWL_Crawler crawler, char *line, PLHashTable *ht) {
	PLHashNumber keyHash;
    PLHashEntry *he, **hep;
	char old;
	char *slash = PL_strrchr(line, '/');
	char *gt = PL_strchr(line, '>');
	if ((slash != NULL) && (gt != NULL)) {
		char *url = gt + 1;
		char *date = slash + 1;
		old = *slash;
		*slash = '\0'; /* temporarily null terminate url */
		/* check if exists - use raw lookup because value can be 0 */
		keyHash = (*ht->keyHash)(url);
		hep = PL_HashTableRawLookup(ht, keyHash, url);
		if ((he = *hep) == 0) {
			URL_Struct *url_s = NET_CreateURLStruct(url, NET_DONT_RELOAD);
			if (url_s != NULL) {
				url_s->SARCache = crawler->cache;
				NET_RemoveURLFromCache(url_s);
				crawler->error |= CRAWL_REMOVED_LINK;
				NET_FreeURLStruct(url_s);
			} else {
				crawler->error |= CRAWL_NO_MEMORY;
				return(-1);
			}
		} else {
			/* there is an entry in the table so check the modified date */
			char *end = NULL;
			CRAWL_LinkInfoStruc *info = (CRAWL_LinkInfoStruc*)he->value;
			time_t oldDate = strtoul(date, &end, 10);
			if (info->lastModifiedDate > oldDate) {
				info->status = REPLACED_LINK;
			} else {
				info->status = OLD_LINK; /* could either be old or a new one with no last modified date reported */
			}
		}
		*slash = old;
	}
	return(0);
}

/* returns -1 on error, 0 for no error */
static int 
crawl_processCacheInfoLine(CRAWL_Crawler crawler, char *line) {
	if (line != NULL) {
		switch (*line) {
		case 'L':
			return(crawl_processCacheInfoEntry(crawler, line, crawler->pagesParsed));
		case 'I':
			return(crawl_processCacheInfoEntry(crawler, line, crawler->imagesCached));
		case 'R':
			return(crawl_processCacheInfoEntry(crawler, line, crawler->resourcesCached));
		default:
			return(-1);
		}
	}
	return(-1);
}

#define CACHE_INFO_BUF_SIZE 10
/* Reads the existing cache info file and for each entry, does a lookup in the appropriate table
   of pages, images, or resources cached. If not found, removes the file from the cache.
*/
static void 
crawl_removeDanglingLinksFromCache(CRAWL_Crawler crawler) {
	static char buf[CACHE_INFO_BUF_SIZE];
	char *line = NULL;
	char *eol;
	int32 n = 0, status;
	char *filename = crawl_makeCacheInfoFilename(crawler);
	if (filename != NULL) {
		PRFileDesc *fd;
		fd = PR_Open(filename,  PR_RDONLY, 0644);
		if (fd == NULL) return;
		while ((status = PR_Read(fd, buf, CACHE_INFO_BUF_SIZE)) > 0) {
			while (n < status) {
				if ((eol = PL_strchr(buf + n, '\n')) == NULL) {
					/* no end of line detected so add to line and continue */
					if (line == NULL) line = (char *)PR_CALLOC(status+1);
					else line = (char *)PR_REALLOC(line, PL_strlen(line) + status + 1);
					if (line == NULL) {
						PR_Close(fd);
						return;
					}
					PL_strcat(line, buf + n, status);
					n += status;
				} else {
					/* end of line detected so copy line up to there */
					int32 len = eol - (buf + n);
					if (line == NULL) line = (char *)PR_CALLOC(len + 1);
					else line = (char *)PR_REALLOC(line, PL_strlen(line) + len + 1);
					if (line == NULL) {
						PR_Close(fd);
						return;
					}
					PL_strcat(line, buf + n, len);
					if (crawl_processCacheInfoLine(crawler, line) != 0) {
						PR_Close(fd); /* abort on bad data */
						return;
					}
					PR_Free(line);
					line = NULL;
					n += (len + 1);
				}
			}
			n = 0;
		}
		PR_Close(fd);
	} else crawler->error |= CRAWL_NO_MEMORY;
}

static int
crawl_updateCrawlerErrors(PLHashEntry *he, int i, void *arg) {
#if defined(XP_MAC)
#pragma unused(i)
#endif	
	CRAWL_LinkInfoStruc *info = (CRAWL_LinkInfoStruc *)he->value;
	CRAWL_Crawler crawler = (CRAWL_Crawler)arg;
	switch (info->status) {
	case NEW_LINK:
		crawler->error |= CRAWL_NEW_LINK;
		break;
	case REPLACED_LINK:
		crawler->error |= CRAWL_REPLACED_LINK;
		break;
	case OLD_LINK:
	default:
		break;
	}
	return HT_ENUMERATE_NEXT;
}

/* called when we're done processing all the items */
static void 
crawl_crawlerFinish(CRAWL_Crawler crawler) {
	/* remove old cache items */
	if (crawler->manageCache && 
		(crawler->cache != NULL) && 
		((crawler->error & CRAWL_NO_MEMORY) == 0) &&
		(crawler->pagesParsed->nentries > 0)) {
		crawl_removeDanglingLinksFromCache(crawler);
		PL_HashTableEnumerateEntries(crawler->pagesParsed, crawl_updateCrawlerErrors, (void*)crawler);
		PL_HashTableEnumerateEntries(crawler->imagesCached, crawl_updateCrawlerErrors, (void*)crawler);
		PL_HashTableEnumerateEntries(crawler->resourcesCached, crawl_updateCrawlerErrors, (void*)crawler);
		crawl_writeCacheList(crawler);
	}
	crawler->status = CRAWL_STOPPED;
	crawler->sizeSlop = SIZE_SLOP; /* reset, in case someone decides to use this crawler again (although docs say not to use again) */
	if (crawler->exitFn != NULL) (crawler->exitFn)(crawler, crawler->exitData);
}

/****************************************************************************************/
/* stream routines for images and resources												*/
/****************************************************************************************/

/* prototypes */
PRIVATE int crawl_ResourceConvPut(NET_StreamClass *stream, char *s, int32 l);
PRIVATE int crawl_ResourceConvWriteReady(NET_StreamClass *stream);
PRIVATE void crawl_ResourceConvComplete(NET_StreamClass *stream);
PRIVATE void crawl_ResourceConvAbort(NET_StreamClass *stream, int status);

PRIVATE int
crawl_ResourceConvPut(NET_StreamClass *stream, char *s, int32 l)
{	
#if defined(XP_MAC)
#pragma unused(stream, s, l)
#endif	
	return(0);
}

PRIVATE int
crawl_ResourceConvWriteReady(NET_StreamClass *stream)
{	
#if defined(XP_MAC)
#pragma unused(stream)
#endif	
	return(MAX_WRITE_READY);
}

PRIVATE void
crawl_ResourceConvComplete(NET_StreamClass *stream)
{	
#if defined(XP_MAC)
#pragma unused(stream)
#endif
	/* do nothing */
}

PRIVATE void
crawl_ResourceConvAbort(NET_StreamClass *stream, int status)
{	
#if defined(XP_MAC)
#pragma unused(stream, status)
#endif
	/* do nothing */
}

/* 
	The reason for using a converter for images and resources is efficiency -
	to prevent netlib from getting a url if we can tell in advance that
	it will exceed the cache size. Otherwise netlib will get the url, determine
	that it has exceeded the cache size and immediately delete it. 

	I haven't done enough testing to determine if this is a big win or not.
*/
PUBLIC NET_StreamClass *
CRAWL_CrawlerResourceConverter(int format_out,
								void *data_object,
								URL_Struct *URL_s,
								MWContext  *window_id)
{
#if defined(XP_MAC)
#pragma unused(format_out)
#endif
	NET_StreamClass *stream = NULL;

	TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

	PR_LogPrint(("CRAWL_CrawlerResourceConverter: %d %s", URL_s->server_status, URL_s->address));

	if (URL_s->SARCache != NULL) {
		/* if the content length would exceed the cache limit, don't convert this */
		if (((uint32)URL_s->content_length >= (URL_s->SARCache->MaxSize - URL_s->SARCache->DiskCacheSize)) &&
			((uint32)URL_s->content_length < BOGUS_CONTENT_LENGTH)) {
				PR_LogPrint(("not converting %s", URL_s->address));
				return(NULL);
		}
	}

	stream = PR_NEW(NET_StreamClass);
	if(stream == NULL)
		return(NULL);

	stream->name           = "Crawler Resource Converter";
	stream->complete       = (MKStreamCompleteFunc) crawl_ResourceConvComplete;
	stream->abort          = (MKStreamAbortFunc) crawl_ResourceConvAbort;
	stream->put_block      = (MKStreamWriteFunc) crawl_ResourceConvPut;
	stream->is_write_ready = (MKStreamWriteReadyFunc) crawl_ResourceConvWriteReady;
	stream->data_object    = data_object;  /* document info object */
	stream->window_id      = window_id;
	return(stream);
}

#ifdef CRAWLERTEST
static void myPostProcessFn(CRAWL_Crawler crawler, URL_Struct *url_s, PRBool isCached, void *data) {
	if (isCached) PR_LogPrint(("%s was cached, content length=%d", url_s->address, url_s->content_length));
	else PR_LogPrint(("%s wasn't cached, content length=%d", url_s->address, url_s->content_length));
	PR_LogPrint(("cache size=%d, size slop=%d", crawler->cache->DiskCacheSize, crawler->sizeSlop));
}

static void myExitFn(CRAWL_Crawler crawler, void *data) {
	char *msg;
	PRIntervalTime startTime = (PRIntervalTime)data;
	msg = PR_smprintf("Crawler finished in %lu milliseconds - cache full=%d", 
				PR_IntervalToMilliseconds(PR_IntervalNow() - startTime),
				(crawler->error & CRAWL_CACHE_FULL));
	FE_Alert(crawler->context, msg);
	PR_smprintf_free(msg);
	CRAWL_DestroyCrawler(crawler);
}

void testCrawler(char *name, char *inURL, uint8 depth, uint32 maxSize, PRBool stayInSite) {
	CRAWL_Crawler crawler;
	char *url = PL_strdup(inURL);
#ifdef XP_MAC
	MWContext *context = XP_FindSomeContext(); /* FE_GetNetHelpContext didn't work with netlib on Mac */
#else
	MWContext *context = FE_GetNetHelpContext();
#endif
	ExtCacheDBInfo *cache = PR_NEWZAP(ExtCacheDBInfo);

	cache->name = "test cache";
	cache->filename = name;
	cache->path = "\\"; /* ignored */
	cache->MaxSize = maxSize;

	cache = CACHE_GetCache(cache);
	crawler = CRAWL_MakeCrawler(context, 
								url, 
								depth, 
								stayInSite, 
								PR_TRUE, 
								cache,
								myPostProcessFn,
								NULL,
								myExitFn,
								(void*)PR_IntervalNow());
	CRAWL_StartCrawler(crawler, url);
}
#endif

#if 0
/****************************************************************************************/
/* test harness code fragment															*/
/****************************************************************************************/

/* 
	Here is a code fragment which may be helpful for testing inside of the client. 
	You can modify NET_GetURL so it will recognize about:crawler and invoke the crawler.
	Also modify net_output_about_url so it returns -1 for about:crawler.
	These functions are defined in ns/lib/libnet/mkgeturl.c

	The following parameters are available
		url - starting url to crawl from (defaults to http://w3.mcom.com/)
		depth - how many levels to crawl (defaults to 1)
		maxsize - maximum cache size in bytes (defaults to 200000)
		stayinsite - if 1, restricts crawling to the site of the initial url (defaults to 1),
			otherwise crawling is unrestricted.
		name - name of the cache (defaults to test.db)

	The parameters are separated by &.

	Example:

	about:crawler?url=http://home&depth=2&stayinsite=0

*/

NET_GetURL (URL_Struct *URL_s,
	    FO_Present_Types output_format,
	    MWContext *window_id,
	    Net_GetUrlExitFunc* exit_routine)
{
	...

		case ABOUT_TYPE_URL:
			...

		  if (URL_s && PL_strncmp(URL_s->address, "about:crawler?", 14) == 0)
		  {
			  uint8 depth = 1;
			  uint32 maxsize = 200000;
			  PRBool stayInSite = PR_TRUE;
			  char temp;
			  char * end;
			  char * item;
			  char * url = "http://w3.mcom.com/";
			  char * name = "test.db";
			  item = PL_strstr(URL_s->address, "url=");
			  if (item != NULL) {
					item += 4;
					end = PL_strchr(item, '&');
					if (end != NULL) {
						temp = *end;
						*end = '\0';
						url = PL_strdup(item);
						*end = temp;
					} else url = PL_strdup(item);
			  }
			  item = PL_strstr(URL_s->address, "name=");
			  if (item != NULL) {
					item += 5;
					end = PL_strchr(item, '&');
					if (end != NULL) {
						temp = *end;
						*end = '\0';
						name = PL_strdup(item);
						*end = temp;
					} else name = PL_strdup(item);
			  }
			  item = PL_strstr(URL_s->address, "depth=");
			  if (item != NULL) {
					item += 6;
					depth = (uint8)strtoul(item, &end, 10);
			  }
			  item = PL_strstr(URL_s->address, "maxsize=");
			  if (item != NULL) {
				  item += 8;
				  maxsize = strtoul(item, &end, 10);
			  }
			  item = PL_strstr(URL_s->address, "stayinsite=");
			  if (item != NULL) {
				  item += 8;
				  if (strtoul(item, &end, 10) == 0) stayInSite = PR_FALSE;
				  else stayInSite = PR_TRUE;
			  }			
				testCrawler(name, url, (uint8)depth, (uint32)maxsize, stayInSite);

				...

			}
	...
}

PRIVATE int net_output_about_url(ActiveEntry * cur_entry)
{
	...
	else if (!PL_strncasecmp(which, "crawler", 7)) 
	{
		return (-1);
	}
}
#endif
