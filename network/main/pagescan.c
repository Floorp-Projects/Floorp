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
/*** pagescan.h ***************************************************/
/*   description:	implementation of page scanning. Adds links   */
/*                  in a page to one of three lists depending on  */
/*                  the tag and its parameters: html links,       */
/*                  images, or resources.                         */
  

 /********************************************************************

	Here's a list of the tags currently handled:

	Links:
	<A HREF=?>
	<AREA HREF=?>
	<FRAME SRC=?>
	<LAYER SRC=?>

	Images:
	<IMG SRC=?>
	<BODY BACKGROUND=?>
	<LAYER BACKGROUND=?>

	Resources:
	<EMBED SRC=?>

	Special:
	<SCRIPT ARCHIVE=? SRC=?>
	<APPLET CODEBASE=? ARCHIVE=? CODE=?>
	<META HTTP-EQUIV="refresh" CONTENT=?>
	<META NAME="robots" CONTENT="NOINDEX,NOFOLLOW">
	<LINK REL="stylesheet" TYPE="text/javascript" HREF=?>
	<STYLE TYPE="text/javascript" SRC=?>
	<STYLE TYPE="text/css" SRC=?>
	<STYLE SRC=?>


  $Revision: 1.3 $
  $Date: 1998/05/22 23:38:14 $

 *********************************************************************/

#include "xp.h"
#include "xp_str.h"
#include "ntypes.h" /* for MWContext */
#include "prmem.h"
#include "net.h"
#include "pa_tags.h" /* lib/libparse */
#include "pagescan.h"
#include "htmparse.h"

typedef uint8 CRAWL_LinkContext;
#define LINK_CONTEXT_HREF 1
#define LINK_CONTEXT_FRAME 2
#define LINK_CONTEXT_LAYER 3

typedef uint8 CRAWL_TagError;
#define CRAWL_TAG_NO_ERR		0
#define CRAWL_TAG_NO_MEMORY		1
#define CRAWL_TAG_SYNTAX_ERR	2

extern PRBool crawl_startsWith (char *pattern, char *uuid);
extern PRBool crawl_endsWith (char *pattern, char *uuid);
extern int crawl_appendStringList(char ***list, uint16 *len, uint16 *size, char *str);

typedef struct _CRAWL_PageInfoStruct {
	char *url;
	char *siteURL;
	char *baseURL;
	URL_Struct *url_s;

	PRBool dontIndex;	/* don't index (cache) this page */
	PRBool dontFollow; /* don't follow links in this page (effectively causes page not to be parsed) */

	CRAWL_ItemList images;
	CRAWL_ItemList links;
	CRAWL_ItemList resources; /* applets */
	CRAWL_ItemList requiredResources; /* script src, and text/javascript or text/css stylesheets */
	CRAWL_ItemList frames;
	CRAWL_ItemList layers;

	uint16 numImages;
	uint16 numLinks;
	uint16 numResources;
	uint16 numRequiredResources;
	uint16 numFrames;
	uint16 numLayers;

	uint16 sizeImages;
	uint16 sizeLinks;
	uint16 sizeResources;
	uint16 sizeRequiredResources;
	uint16 sizeFrames;
	uint16 sizeLayers;

	time_t lastModified;

	void *page_owner;
	CRAWL_ScanPageStatusFunc scan_complete_func;
} CRAWL_PageInfoStruct;

/* prototypes */
static char* crawl_makeAbsoluteURL(CRAWL_PageInfo pageInfo, char *relURL);
static CRAWL_TagError crawl_addPageLink(CRAWL_PageInfo pageInfo, char *link, CRAWL_LinkContext context);
static CRAWL_TagError crawl_addPageImage(CRAWL_PageInfo pageInfo, char *image);
static CRAWL_TagError crawl_addPageResource(CRAWL_PageInfo pageInfo, char *resource);
static CRAWL_TagError crawl_addPageRequiredResource(CRAWL_PageInfo pageInfo, char *resource);
static CRAWL_TagError crawl_addPageFrame(CRAWL_PageInfo pageInfo, char *frame);
static CRAWL_TagError crawl_addPageLayer(CRAWL_PageInfo pageInfo, char *layer);
static CRAWL_TagError crawl_addPageApplet(CRAWL_PageInfo pageInfo, char *codebase, char *archive, char *code);
static CRAWL_TagError crawl_addPageMetaRefresh(CRAWL_PageInfo page, char *content);
static CRAWL_TagError crawl_processURL(CRAWL_PageInfo page, char *url, CRAWL_LinkContext context);
static void crawl_get_page_url_exit(URL_Struct *URL_s, int status, MWContext *window_id);
int crawl_processToken(CRAWL_ParseObj obj, PRBool isTag, void *data);

/****************************************************************************************/
/* accessors																			*/
/****************************************************************************************/

PRBool crawl_pageCanBeIndexed(CRAWL_PageInfo pageInfo) {
	return ((pageInfo->dontIndex) ? PR_FALSE : PR_TRUE);
}

char* crawl_getPageURL(CRAWL_PageInfo pageInfo) {
	return pageInfo->url;
}

/* returns the last modified date given by the server */
time_t crawl_getPageLastModified(CRAWL_PageInfo pageInfo) {
	return pageInfo->lastModified;
}

URL_Struct* crawl_getPageURL_Struct(CRAWL_PageInfo pageInfo) {
	return pageInfo->url_s;
}

CRAWL_ItemList crawl_getPageLinks(CRAWL_PageInfo pageInfo) {
	return pageInfo->links;
}

uint16 crawl_getPageLinkCount(CRAWL_PageInfo pageInfo) {
	return pageInfo->numLinks;
}

/* 
	makes an absolute URL from relURL using the base url if it exists, otherwise uses the
	page's url as a base.
*/
static char* 
crawl_makeAbsoluteURL(CRAWL_PageInfo pageInfo, char *relURL) {
	if ((pageInfo->baseURL != NULL) && (*pageInfo->baseURL != '\0')) 
		return NET_MakeAbsoluteURL(pageInfo->baseURL, relURL);
	else return NET_MakeAbsoluteURL(pageInfo->url_s->address, relURL);
}

static CRAWL_TagError
crawl_addPageLink(CRAWL_PageInfo pageInfo, char *link, CRAWL_LinkContext context) {
	char *fullURL = crawl_makeAbsoluteURL(pageInfo, link);
	if (fullURL == NULL) return(CRAWL_TAG_NO_MEMORY);
	switch (context) {
	case LINK_CONTEXT_HREF:
		if (crawl_appendStringList(&pageInfo->links, &pageInfo->numLinks, &pageInfo->sizeLinks, fullURL) != 0)
			return CRAWL_TAG_NO_MEMORY;
		break;
	case LINK_CONTEXT_FRAME:
		if (crawl_appendStringList(&pageInfo->frames, &pageInfo->numFrames, &pageInfo->sizeFrames, fullURL) != 0)
			return CRAWL_TAG_NO_MEMORY;
		break;
	case LINK_CONTEXT_LAYER:
		if (crawl_appendStringList(&pageInfo->layers, &pageInfo->numLayers, &pageInfo->sizeLayers, fullURL) != 0)
			return CRAWL_TAG_NO_MEMORY;
		break;
	default:
		break;
	}
	return CRAWL_TAG_NO_ERR;
}

CRAWL_ItemList crawl_getPageImages(CRAWL_PageInfo pageInfo) {
	return pageInfo->images;
}

uint16 crawl_getPageImageCount(CRAWL_PageInfo pageInfo) {
	return pageInfo->numImages;
}

static CRAWL_TagError
crawl_addPageImage(CRAWL_PageInfo pageInfo, char *image) {
	char *fullURL = crawl_makeAbsoluteURL(pageInfo, image);
	if (fullURL == NULL) return(CRAWL_TAG_NO_MEMORY);
	if (crawl_appendStringList(&pageInfo->images, &pageInfo->numImages, &pageInfo->sizeImages, fullURL) != 0)
		return(CRAWL_TAG_NO_MEMORY);
	else return(CRAWL_TAG_NO_ERR);
}

CRAWL_ItemList crawl_getPageResources(CRAWL_PageInfo pageInfo) {
	return pageInfo->resources;
}

uint16 crawl_getPageResourceCount(CRAWL_PageInfo pageInfo) {
	return pageInfo->numResources;
}

static CRAWL_TagError
crawl_addPageResource(CRAWL_PageInfo pageInfo, char *resource) {
	char *fullURL = crawl_makeAbsoluteURL(pageInfo, resource);
	if (fullURL == NULL) return(CRAWL_TAG_NO_MEMORY);
	if (crawl_appendStringList(&pageInfo->resources, &pageInfo->numResources, &pageInfo->sizeResources, fullURL) != 0)
		return(CRAWL_TAG_NO_MEMORY);
	else return(CRAWL_TAG_NO_ERR);
}

CRAWL_ItemList crawl_getPageRequiredResources(CRAWL_PageInfo pageInfo) {
	return pageInfo->requiredResources;
}

uint16 crawl_getPageRequiredResourceCount(CRAWL_PageInfo pageInfo) {
	return pageInfo->numRequiredResources;
}

static CRAWL_TagError
crawl_addPageRequiredResource(CRAWL_PageInfo pageInfo, char *resource) {
	char *fullURL = crawl_makeAbsoluteURL(pageInfo, resource);
	if (fullURL == NULL) return(CRAWL_TAG_NO_MEMORY);
	if (crawl_appendStringList(&pageInfo->requiredResources, &pageInfo->numRequiredResources, &pageInfo->sizeRequiredResources, fullURL) != 0)
		return(CRAWL_TAG_NO_MEMORY);
	else return(CRAWL_TAG_NO_ERR);
}

CRAWL_ItemList crawl_getPageFrames(CRAWL_PageInfo pageInfo) {
	return pageInfo->frames;
}

uint16 crawl_getPageFrameCount(CRAWL_PageInfo pageInfo) {
	return pageInfo->numFrames;
}

static CRAWL_TagError
crawl_addPageFrame(CRAWL_PageInfo pageInfo, char *frame) {
	char *fullURL = crawl_makeAbsoluteURL(pageInfo, frame);
	if (fullURL == NULL) return(CRAWL_TAG_NO_MEMORY);
	if (crawl_appendStringList(&pageInfo->frames, &pageInfo->numFrames, &pageInfo->sizeFrames, fullURL) != 0)
		return(CRAWL_TAG_NO_MEMORY);
	else return(CRAWL_TAG_NO_ERR);
}

CRAWL_ItemList crawl_getPageLayers(CRAWL_PageInfo pageInfo) {
	return pageInfo->layers;
}

uint16 crawl_getPageLayerCount(CRAWL_PageInfo pageInfo) {
	return pageInfo->numLayers;
}

static CRAWL_TagError
crawl_addPageLayer(CRAWL_PageInfo pageInfo, char *layer) {
	char *fullURL = crawl_makeAbsoluteURL(pageInfo, layer);
	if (fullURL == NULL) return(CRAWL_TAG_NO_MEMORY);
	if (crawl_appendStringList(&pageInfo->layers, &pageInfo->numLayers, &pageInfo->sizeLayers, fullURL) != 0)
		return(CRAWL_TAG_NO_MEMORY);
	else return(CRAWL_TAG_NO_ERR);
}

/* process the codebase, archive, and code attributes of an APPLET tag.
*/
static CRAWL_TagError
crawl_addPageApplet(CRAWL_PageInfo pageInfo, char *codebase, char *archive, char *code) {
	char *fullCodebase;
	if (codebase != NULL) {
		fullCodebase = crawl_makeAbsoluteURL(pageInfo, codebase);
		if (fullCodebase == NULL) return(CRAWL_TAG_NO_MEMORY);
	}
	if (archive != NULL) {
		char *fullArchive;
		if (fullCodebase != NULL) {
			fullArchive = NET_MakeAbsoluteURL(fullCodebase, archive);
		} else {
			fullArchive = crawl_makeAbsoluteURL(pageInfo, archive);
		}
		if (fullArchive == NULL) return(CRAWL_TAG_NO_MEMORY);

		if (crawl_addPageResource(pageInfo, fullArchive) == CRAWL_TAG_NO_MEMORY) return(CRAWL_TAG_NO_MEMORY);
	}
	if (code != NULL) {
		char *fullCode;
		/* FIXME can code still be specified with fully qualified package names? */
		if (fullCodebase != NULL) fullCode = NET_MakeAbsoluteURL(fullCodebase, code);
		else fullCode = crawl_makeAbsoluteURL(pageInfo, code);
		if (fullCode == NULL) return(CRAWL_TAG_NO_MEMORY);
		if (crawl_addPageResource(pageInfo, fullCode) == CRAWL_TAG_NO_MEMORY) return(CRAWL_TAG_NO_MEMORY);
	} else return(CRAWL_TAG_SYNTAX_ERR);

	return(CRAWL_TAG_NO_ERR);
}

/* process a url specified in a META refresh tag */
static CRAWL_TagError
crawl_addPageMetaRefresh(CRAWL_PageInfo page, char *content) {
	/* look for the url= */
	char *url = PL_strcasestr(content, "url=");
	if (url != NULL) {
		url += 4; /* go past the url= */
		url = strtok(url, " '\"");
		return(crawl_addPageLink(page, url, LINK_CONTEXT_HREF));
	} else return CRAWL_TAG_SYNTAX_ERR;
}

CRAWL_PageInfo crawl_makePage(char *siteURL, char *pageURL, ExtCacheDBInfo *cache) {
	CRAWL_PageInfo pageInfo = PR_NEWZAP(CRAWL_PageInfoStruct);
	if (pageInfo == NULL) return NULL;
	pageInfo->url = pageURL;
	pageInfo->siteURL = siteURL;
	pageInfo->url = NET_MakeAbsoluteURL(siteURL, pageURL); /* complete a partial url if necessary */
	if (pageInfo->url != NULL) {
		pageInfo->url_s = NET_CreateURLStruct(pageInfo->url, NET_NORMAL_RELOAD); /* freed in the exit function */
		if (pageInfo->url_s != NULL) {
			pageInfo->url_s->load_background = PR_TRUE;
			pageInfo->url_s->SARCache = cache;
			pageInfo->url_s->owner_data = pageInfo; /* so we can recover the CRAWL_PageInfo in the converter */
		} else {
			PR_Free(pageInfo->url);
			PR_Free(pageInfo);
			return NULL;
		}
	} else {
		PR_Free(pageInfo);
		return NULL;
	}
	return pageInfo;
}

void crawl_destroyPage(CRAWL_PageInfo page) {
	if (page->images != NULL) {
		PR_Free(page->images);
		page->sizeImages = 0;
	}
	if (page->links != NULL) {
		PR_Free(page->links);
		page->sizeLinks = 0;
	}
	if (page->resources != NULL) {
		PR_Free(page->resources);
		page->sizeResources = 0;
	}
	if (page->requiredResources != NULL) {
		PR_Free(page->requiredResources);
		page->sizeRequiredResources = 0;
	}
	if (page->frames != NULL) {
		PR_Free(page->frames);
		page->sizeFrames = 0;
	}
	if (page->layers != NULL) {
		PR_Free(page->layers);
		page->sizeLayers = 0;
	}
	/* if (page->url_s != NULL) NET_FreeURLStruct(page->url_s); */
	if (page->url != NULL) PR_Free(page->url);
	if (page->baseURL != NULL) PR_Free(page->baseURL);
	PR_Free(page);
}

/* 
	adds the url to the appropriate list (link, image, or resource).
	FIXME ALERT! still need to deal with file extensions - those which are alphanumeric are resources.
*/
static CRAWL_TagError 
crawl_processURL(CRAWL_PageInfo page, char *url, CRAWL_LinkContext context) {
	/* NET_TO_LOWER(url); */
	if (crawl_startsWith("mailto:", url)) return(CRAWL_TAG_NO_ERR);
	else if (crawl_startsWith("mailbox:", url)) return(CRAWL_TAG_NO_ERR);
	else if (crawl_startsWith("news:", url)) return(CRAWL_TAG_NO_ERR);
	else if (crawl_startsWith("javascript:", url)) return(CRAWL_TAG_NO_ERR);
	
	if (crawl_endsWith(".jpg", url) || 
		crawl_endsWith(".jpeg", url) || 
		crawl_endsWith(".gif", url) || 
		crawl_endsWith(".xbm", url)) {
		crawl_addPageImage(page, url);
		return(CRAWL_TAG_NO_ERR);
	} else return(crawl_addPageLink(page, url, context));
}

/* 
	Process a tag, adding urls to the lists of links, images, or resources.
	The parser calls this routine whenever a tag or between-tag data has been read. 
*/
int crawl_processToken(CRAWL_ParseObj obj, PRBool isTag, void *data) {
	CRAWL_TagError err = CRAWL_TAG_NO_ERR;
	CRAWL_PageInfo page = (CRAWL_PageInfo)data;
	if (isTag) {
		CRAWL_Tag tag = CRAWL_GetTagParsed(obj);
		char *name = CRAWL_GetTagName(tag);
		char *att1, *att2, *att3;
		if (CRAWL_IsEndTag(tag)) return PARSE_GET_NEXT_TOKEN;
		switch (CRAWL_GetTagToken(tag)) {
		case P_ANCHOR:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_HREF); /* <A HREF=?> */
			if (att1 != NULL) err = crawl_processURL(page, att1, LINK_CONTEXT_HREF);
			break;
		case P_AREA:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_HREF); /* <AREA HREF=?> */
			if (att1 != NULL) err = crawl_processURL(page, att1, LINK_CONTEXT_HREF);
			break;
		case P_BASE:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_HREF);
			if (att1 != NULL) {
				page->baseURL = NET_MakeAbsoluteURL(page->url_s->address, att1); /* PL_strdup(att1); */
				if (page->baseURL == NULL) err = CRAWL_TAG_NO_MEMORY;
			}
			break;
		case P_IMAGE:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_SRC); /* <IMG SRC=?> */
			if (att1 != NULL) err = crawl_addPageImage(page, att1);
			break;
		case P_BODY:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_BACKGROUND); /* <BODY BACKGROUND=?> */
			if (att1 != NULL) err = crawl_addPageImage(page, att1);
			break;
		case P_GRID_CELL:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_SRC); /* <FRAME SRC=?> */
			if (att1 != NULL) err = crawl_processURL(page, att1, LINK_CONTEXT_FRAME);
			break;
		case P_LAYER:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_SRC); /* <LAYER SRC=?> */
			if (att1 != NULL) err = crawl_processURL(page, att1, LINK_CONTEXT_LAYER);
			att2 = CRAWL_GetAttributeValue(tag, "background"); /* <LAYER BACKGROUND=?> */
			if ((att2 != NULL) && (err == CRAWL_TAG_NO_ERR)) err = crawl_addPageImage(page, att2);
			break;
		case P_EMBED:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_SRC); /* <EMBED SRC=?> */
			if (att1 != NULL) err = crawl_addPageResource(page, att1);
			break;
		case P_SCRIPT:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_ARCHIVE);
			if (att1 != NULL) err = crawl_addPageRequiredResource(page, att1);
			att2 = CRAWL_GetAttributeValue(tag, PARAM_SRC);
			if ((att2 != NULL) && (err == CRAWL_TAG_NO_ERR)) err = crawl_addPageRequiredResource(page, att2);
			break;
		case P_JAVA_APPLET:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_CODEBASE);
			att2 = CRAWL_GetAttributeValue(tag, PARAM_ARCHIVE);
			att3 = CRAWL_GetAttributeValue(tag, PARAM_CODE);
			err = crawl_addPageApplet(page, att1, att2, att3);
			break;
		case P_META:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_HTTP_EQUIV);
			if ((att1 != NULL) && (PL_strcasecmp(att1, "refresh") == 0)) {
				att2 = CRAWL_GetAttributeValue(tag, PARAM_CONTENT);
				if (att2 != NULL) err = crawl_addPageMetaRefresh(page, att2);
			} else {
			/* robots meta tag: we take NOINDEX to mean don't cache (the document is already
			   cached so we need to remove it from the cache. NOFOLLOW means don't follow the
			   links, which we'll do by aborting the parsing of this page.
			   See http://info.webcrawler.com/mak/projects/robots/exclusion.html
			*/
				att1 = CRAWL_GetAttributeValue(tag, PARAM_NAME);
				if ((att1 != NULL) && (PL_strcasecmp(att1, "robots") == 0)) {
					att2 = CRAWL_GetAttributeValue(tag, PARAM_CONTENT);
					if (att2 != NULL) {
						if (PL_strcasestr(att2, "noindex")) {
							page->dontIndex = PR_TRUE;
						}
						if (PL_strcasestr(att2, "nofollow")) {
							page->dontFollow = PR_TRUE;
							return PARSE_STOP;
						}
					}
				}
			}
			break;
		case P_LINK:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_REL);
			if ((att1 != NULL) && (PL_strcasecmp(att1, "stylesheet") == 0)) {
				att2 = CRAWL_GetAttributeValue(tag, PARAM_HREF);
				if (att2 != NULL) {
					att3 = CRAWL_GetAttributeValue(tag, PARAM_TYPE);
					if ((att3 != NULL) && ((PL_strcasecmp(att3, "text/javascript") == 0) || (PL_strcasecmp(att3, "text/css") == 0))) {
						err = crawl_addPageRequiredResource(page, att2);
					} else err = crawl_addPageResource(page, att2);
				}
			}
			break;
		case P_STYLE:
			att1 = CRAWL_GetAttributeValue(tag, PARAM_SRC);
			if (att1 != NULL) {
				att2 = CRAWL_GetAttributeValue(tag, PARAM_TYPE);
				if ((att2 != NULL) && ((PL_strcasecmp(att2, "text/javascript") == 0) || (PL_strcasecmp(att2, "text/css") == 0))) {
						err = crawl_addPageRequiredResource(page, att1);
				} else err = crawl_addPageResource(page, att1);
			}
			break;
		default:
			break;
		}
	}
	if (err == CRAWL_TAG_NO_MEMORY) return PARSE_OUT_OF_MEMORY;
	else return PARSE_GET_NEXT_TOKEN;
}

/* exit routine for NET_GetURL */
static void
crawl_get_page_url_exit(URL_Struct *URL_s, int status, MWContext *window_id)
{
#if defined(XP_MAC)
#pragma unused(window_id)
#endif
	if(status != MK_CHANGING_CONTEXT)
		NET_FreeURLStruct(URL_s);
}

void crawl_scanPage(CRAWL_PageInfo page, MWContext *context, CRAWL_ScanPageStatusFunc complete_func, void *data) {

	page->page_owner = data;
	page->scan_complete_func = complete_func;
	NET_GetURL(page->url_s, FO_CACHE_AND_CRAWL_PAGE, context, crawl_get_page_url_exit);
}

/****************************************************************************************/
/* stream routines																		*/
/****************************************************************************************/

typedef struct {
	CRAWL_ParseObj parse_obj;
	CRAWL_PageInfo page;
} crawl_page_scan_stream;

/* prototypes */
PRIVATE int crawl_PageScanConvPut(NET_StreamClass *stream, char *s, int32 l);
PRIVATE int crawl_PageScanConvWriteReady(NET_StreamClass *stream);
PRIVATE void crawl_PageScanConvComplete(NET_StreamClass *stream);
PRIVATE void crawl_PageScanConvAbort(NET_StreamClass *stream, int status);

PRIVATE int
crawl_PageScanConvPut(NET_StreamClass *stream, char *s, int32 l)
{
	crawl_page_scan_stream *obj=stream->data_object;
	int status;

	PR_ASSERT(obj->parse_obj != NULL);
	PR_ASSERT(obj->page != NULL);

	if (!obj->page->dontFollow) /* no directive to cache without parsing */
		status = CRAWL_ParserPut(obj->parse_obj, s, l, crawl_processToken, obj->page);
	else status = CRAWL_PARSE_NO_ERROR;
	
	switch (status) {
	case CRAWL_PARSE_NO_ERROR:
		return(0);
	case CRAWL_PARSE_ERROR:
		obj->page->dontFollow = PR_TRUE;
		return(0);
	case CRAWL_PARSE_TERMINATE:
		obj->page->dontFollow = PR_TRUE;
		return(0);
	case CRAWL_PARSE_OUT_OF_MEMORY:
		return(MK_UNABLE_TO_CONVERT);
	default:
		PR_ASSERT(0);
		break;
	}
	return(status);
}

PRIVATE int
crawl_PageScanConvWriteReady(NET_StreamClass *stream)
{	
#if defined(XP_MAC)
#pragma unused(stream)
#endif
	return(MAX_WRITE_READY);
}

PRIVATE void
crawl_PageScanConvComplete(NET_StreamClass *stream)
{
	crawl_page_scan_stream *obj=stream->data_object;
	CRAWL_PageInfo page = (CRAWL_PageInfo)obj->page;
	PR_ASSERT(page != NULL);
	if ((page->scan_complete_func != NULL) && (page->page_owner != NULL))
		page->scan_complete_func(page->page_owner, page);
	CRAWL_DestroyParseObj(obj->parse_obj);
	crawl_destroyPage(page);
	obj->page = NULL;
}

PRIVATE void
crawl_PageScanConvAbort(NET_StreamClass *stream, int status)
{	
#if defined(XP_MAC)
#pragma unused(status)
#endif
	crawl_page_scan_stream *obj=stream->data_object;
	/* we got an error (status always negative here) but should call completion function */
	CRAWL_PageInfo page = (CRAWL_PageInfo)obj->page;
	PR_ASSERT(page != NULL);
	if ((page->scan_complete_func != NULL) && (page->page_owner != NULL))
		page->scan_complete_func(page->page_owner, page);

	CRAWL_DestroyParseObj(obj->parse_obj);
	crawl_destroyPage(obj->page);
	obj->page = NULL;
}

PUBLIC NET_StreamClass *
CRAWL_CrawlerConverter(int format_out,
						void *data_object,
						URL_Struct *URL_s,
						MWContext  *window_id)
{
#if defined(XP_MAC)
#pragma unused(format_out, data_object)
#endif
	NET_StreamClass *stream = NULL;

	crawl_page_scan_stream *obj;

	TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

	PR_LogPrint(("CRAWL_CrawlerConverter: %d %s", URL_s->server_status, URL_s->address));

	if (URL_s->SARCache != NULL) {
		/* if the content length would exceed the cache limit, don't convert this */
		if (((uint32)URL_s->content_length >= (URL_s->SARCache->MaxSize - URL_s->SARCache->DiskCacheSize)) &&
			((uint32)URL_s->content_length < BOGUS_CONTENT_LENGTH)) {
			CRAWL_PageInfo page = URL_s->owner_data;
			PR_LogPrint(("not converting %s", URL_s->address));
			if ((page->scan_complete_func != NULL) && (page->page_owner != NULL)) {
				page->scan_complete_func(page->page_owner, page);
				return(NULL);
			}
		}
	}

	stream = PR_NEW(NET_StreamClass);
	if(stream == NULL)
		return(NULL);

	obj = PR_NEW(crawl_page_scan_stream);
	if (obj == NULL)
	  {
		PR_Free(stream);
		return(NULL);
	  }
	obj->parse_obj = CRAWL_MakeParseObj();			/* this object used to parse the page and destroyed 
												in completion or abort function */
	if (obj->parse_obj == NULL) return(NULL);
	obj->page = URL_s->owner_data;				/* this data was set in crawl_makePage */
	obj->page->lastModified = URL_s->last_modified;
	/* if there was a server error, read but don't parse the document */
	if ((URL_s->server_status >= 400) ||
		/* don't attempt to parse non-html */
		((PL_strstr(URL_s->content_type, TEXT_HTML) == NULL) &&
		 (PL_strstr(URL_s->content_type, INTERNAL_PARSER) == NULL))) {
		/* URL_s->dont_cache = PR_FALSE; */
		obj->page->dontFollow = PR_TRUE;
	}

	stream->name           = "Crawler Converter";
	stream->complete       = (MKStreamCompleteFunc) crawl_PageScanConvComplete;
	stream->abort          = (MKStreamAbortFunc) crawl_PageScanConvAbort;
	stream->put_block      = (MKStreamWriteFunc) crawl_PageScanConvPut;
	stream->is_write_ready = (MKStreamWriteReadyFunc) crawl_PageScanConvWriteReady;
	stream->data_object    = obj;  /* document info object */
	stream->window_id      = window_id;
	return(stream);
}
