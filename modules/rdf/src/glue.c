/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file contains the glue code that links the RDF module with
 * ugly navigator stuff including Netlib for getURL, preferences for
 * finding out where bookmarks.htm and lclstore.mcf are located.  By
 * changing this glue file, one should be able to use the rest of the
 * RDF library with something else (like the server).  

 * For more information on this file, contact rjc or guha 
 * For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "rdf-int.h"
#include "glue.h"
#include "remstore.h"
#include "rdfparse.h"
#include "es2mcf.h"
#include "mcff2mcf.h"
#include "nlcstore.h"
#include "autoupdt.h"
#include "ht.h"
#ifdef NU_CACHE
#include "CacheStubs.h"
#endif

/* external routines */
extern	MWContext	*FE_GetRDFContext(void);
extern	char		*gDefaultNavcntr;
extern	RDF		gNCDB;

/* pointer to the mocha thread */
extern	PRThread	*mozilla_thread;


/* globals */
char			*profileDirURL = NULL;
char			*gLocalStoreURL = NULL;
char			*gBookmarkURL = NULL;
char			*gGlobalHistoryURL = NULL;
void			*timerID = NULL;
char                    *gRLForbiddenDomains = NULL;



unsigned int
rdf_write_ready(NET_StreamClass *stream)
{
	return MAX_WRITE_READY;
}



void
rdf_complete(NET_StreamClass *stream)
{
  RDFFile f = (RDFFile)stream->data_object;
  if (strcmp(f->url, gNavCntrUrl) == 0) {
    if (f->resourceCount == 0) {
      parseNextRDFXMLBlob(stream, gDefaultNavcntr, strlen(gDefaultNavcntr));
    } else {
      RDF_Resource browser = RDF_GetResource(NULL, "netscape:browser", 1);

      RDF_Resource updateID = RDF_GetResource(NULL, "updateID", 1);
      char* id = RDF_GetSlotValue(gNCDB, browser, updateID,
                                  RDF_STRING_TYPE, false, true);

      RDF_Resource updateFrom = RDF_GetResource(NULL, "updateURL", 1);
      char* uf = RDF_GetSlotValue(gNCDB, browser, updateFrom,
                                  RDF_STRING_TYPE, false, true);

      RDF_Resource fileSize = RDF_GetResource(NULL, "fileSize", 1);
      char* fs = RDF_GetSlotValue(gNCDB, browser, fileSize,
                                  RDF_STRING_TYPE, false, true);
      uint32 fSize;
      if (fs == NULL) {
        fSize = 3000;
      } else {
        sscanf("%lu", fs, &fSize);
        freeMem(fs);
      }
      if ((uf != NULL) && (id != NULL)) {
#ifdef MOZ_SMARTUPDATE
        AutoUpdateConnnection autoupdt;
        autoupdt = AutoUpdate_Setup(FE_GetRDFContext(), 
                                    id, uf, fSize, 
                                    "http://warp/u/raman/docs/js/download.html");
        autoupdate_Resume(autoupdt);
#endif /* MOZ_SMARTUPDATE */
        freeMem(uf);
        freeMem(id);
      } 

      /* A temporary hack to demo AutoUpdate on windows */
#ifndef MOZ_SMARTUPDATE
#ifndef XP_MAC
      /*
      {
        AutoUpdate_LoadMainScript(FE_GetRDFContext(),
                                  "http://warp/u/raman/docs/js/download.html");
      }
      */
#endif /* !XP_MAC */
#endif /* MOZ_SMARTUPDATE */

    } 
  }
  if (f) {
    freeMem(f->line);
    freeMem(f->currentSlot);
    freeMem(f->holdOver);
    freeNamespaces(f) ;
	f->line = NULL;
	f->currentSlot = NULL;
	f->holdOver = NULL;
  }
}



void
rdf_abort(NET_StreamClass *stream, int status)
{
  RDFFile f = (RDFFile)stream->data_object;
  if (strcmp(f->url, gNavCntrUrl) == 0) {
    parseNextRDFXMLBlob(stream, gDefaultNavcntr, strlen(gDefaultNavcntr));
  }
    
  if (f) {
     f->locked = false;
     gcRDFFile (f);
    freeMem(f->line);
    freeMem(f->currentSlot);
    freeMem(f->holdOver);
    freeNamespaces(f) ;
  }
}



#ifdef MOZILLA_CLIENT

#ifdef	XP_MAC
PR_PUBLIC_API(NET_StreamClass *)
#else
PUBLIC NET_StreamClass *
#endif

rdf_Converter(FO_Present_Types format_out, void *client_data,
		URL_Struct *urls, MWContext *cx)
{
	RDFFile			rdfFile;
	MKStreamWriteFunc	writeFunc = NULL;

	if ((urls == NULL) || (urls->fe_data == NULL))
	{
		return(NULL);
	}

	/* determine appropriate write function to use */
	rdfFile = urls->fe_data;
	switch(rdfFile->fileType)
	{
		case	ES_RT:
		case	FTP_RT:
		writeFunc = (MKStreamWriteFunc)parseNextESFTPBlob;
		break;

		case	RDF_MCF:
		writeFunc = (MKStreamWriteFunc)parseNextMCFBlob;
		break;

		default:
		writeFunc = (MKStreamWriteFunc)parseNextRDFXMLBlob;
		break;
	}

	/* allocate stream data block */
	return NET_NewStream("RDF", writeFunc, 
			(MKStreamCompleteFunc)rdf_complete,
			(MKStreamAbortFunc)rdf_abort, 
			(MKStreamWriteReadyFunc)rdf_write_ready,
			urls->fe_data, cx);
}



void
rdf_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx)
{
	RDFFile		f;
	char		*navCenterURL = NULL;

	if ((status < 0) && (urls != NULL))
	{
		/* if unable to read in navcntr.rdf file, create some default views */
		if ((f = (RDFFile) urls->fe_data) != NULL)
		{
			if (strcmp(f->url, gNavCntrUrl) == 0)
			{
				parseNextRDFXMLBlobInt(f, gDefaultNavcntr,
						strlen(gDefaultNavcntr));
			}
		}
	}

	if (urls != NULL)
	{
		if ((f = (RDFFile) urls->fe_data) != NULL)
		{
			htLoadComplete(cx, urls, f->url, status);
		}
	}

	NET_FreeURLStruct (urls);
}


/*
int
rdfRetrievalType (RDFFile f)
{
	URL_Struct		*urls;
	char			*url;
	int			type;

	url = f->url;
	if (f->localp)
	{
		urls = NET_CreateURLStruct(url,  NET_CACHE_ONLY_RELOAD);
		if ((urls != NULL) && (NET_IsURLInDiskCache(urls) || NET_IsURLInMemCache(urls)))
		{
			type = NET_DONT_RELOAD;
		}
		else
		{
			type = NET_NORMAL_RELOAD;
		}
		if (urls != NULL)	NET_FreeURLStruct(urls);
	}
	else
	{
		type = NET_NORMAL_RELOAD;
	}
	return(type);
}

*/



typedef	struct	{
	ETEvent			ce;
	char			*url;
	URL_Struct		*urls;
	int			method;
	MWContext		*cx;
	Net_GetUrlExitFunc	*exitFunc;
} MozillaEvent_rdf_GetURL;



PR_STATIC_CALLBACK(void *)
rdf_HandleEvent_GetURL(MozillaEvent_rdf_GetURL *event)
{
	if (event->url != NULL)
	{
		htLoadBegins(event->urls, event->url);
	}
	NET_GetURL(event->urls, event->method, event->cx, event->exitFunc);
	return(NULL);
}



PR_STATIC_CALLBACK(void)
rdf_DisposeEvent_GetURL(MozillaEvent_rdf_GetURL *event)
{
	if (event->url != NULL)
	{
		freeMem(event->url);
		event->url = NULL;
	}
	XP_FREE(event);
}



int
rdf_GetURL (MWContext *cx, int method, Net_GetUrlExitFunc *exit_routine, RDFFile rdfFile)
{
	MozillaEvent_rdf_GetURL		*event;
	URL_Struct      		*urls = NULL;
        char				*url;

#ifdef DEBUG_gagan
        return 0;
#endif

    if (cx == NULL)  return 0;
        if (rdfFile->refreshingp && rdfFile->updateURL) {
          url = rdfFile->updateURL;
        } else {
          url = rdfFile->url;
        }
        if (strcmp(url, gNavCntrUrl) == 0) {
          urls = NET_CreateURLStruct(url,  NET_CACHE_ONLY_RELOAD);
#ifdef NU_CACHE
          if (!CacheManager_Contains(url)) {
#else
          if (NET_IsURLInDiskCache(urls) || NET_IsURLInMemCache(urls)) {
          } else {
#endif
            NET_FreeURLStruct(urls);
            urls = NULL;
          }
        }
	if (!urls) 
          urls = NET_CreateURLStruct(url, (rdfFile->refreshingp ? 
                                           NET_SUPER_RELOAD : NET_NORMAL_RELOAD));
	if (urls == NULL) return 0;
	urls->fe_data = rdfFile;
	if (method) urls->method = method;

	if (PR_CurrentThread() == mozilla_thread)
	{
		htLoadBegins(urls, url);
		NET_GetURL(urls, FO_CACHE_AND_RDF, cx, rdf_GetUrlExitFunc);
	}
	else
	{
		/* send event to Mozilla thread */
		
		if (mozilla_event_queue == NULL)	return(0);
		event = PR_NEW(MozillaEvent_rdf_GetURL);
		if (event == NULL)	return(0);
		PR_InitEvent(&(event->ce.event), cx,
			(PRHandleEventProc)rdf_HandleEvent_GetURL,
			(PRDestroyEventProc)rdf_DisposeEvent_GetURL);
		event->url = copyString(url);
		event->urls = urls;
		event->method = FO_CACHE_AND_RDF;
		event->cx = cx;
		event->exitFunc = rdf_GetUrlExitFunc;
		PR_PostEvent(mozilla_event_queue, &(event->ce.event));
	}
	return 1;
}
#endif /* MOZILLA_CLIENT */



void
possiblyRereadRDFFiles (void* data)
{
	possiblyRefreshRDFFiles();
/*	timerID = FE_SetTimeout(possiblyRereadRDFFiles, NULL, 1000 * 60 * 10); 
	once every 10 minutes
	diabled for legal reasons.*/
}



void
RDFglueInitialize()
{
#ifdef MOZILLA_CLIENT

	timerID = FE_SetTimeout(possiblyRereadRDFFiles, NULL, 1000 * 60 * 10); /* once every 10 minutes */
	if (gRLForbiddenDomains != NULL)
	{
		freeMem(gRLForbiddenDomains);
		gRLForbiddenDomains = NULL;
	}
	if (PREF_CopyCharPref("browser.relatedLinksDisabledForDomains", &gRLForbiddenDomains) != PREF_OK)
	{
		gRLForbiddenDomains = NULL;
	}

#endif /* MOZILLA_CLIENT */
}



void
RDFglueExit (void)
{
#ifdef MOZILLA_CLIENT

	if (timerID != NULL)
	{
		/* commented out as the timer's window has already been destroyed */

		/* FE_ClearTimeout(timerID); */
		timerID = NULL;
	}

#endif /* MOZILLA_CLIENT */
}


void *
gRDFMWContext(RDFT db)
{
#ifndef MOZILLA_CLIENT
   return NULL;
#else
	void		*cx;
        RDFL            rdf = NULL;
        
        if (db) rdf = db->rdf;
        
        while (rdf) {
          if (rdf->rdf->context) return (rdf->rdf->context);
          rdf = rdf->next;
        }

	cx = (void *)FE_GetRDFContext();
	return(cx);
#endif
}



/* 
 * beginReadingRDFFile is called whenever we need to read something of
 * the net (or local drive). The url of the file to be read is at
 * file->url.  As the bits are read in (and it can take the bits in
 * any sized chunks) it should call parseNextRDFBlob(file, nextBlock,
 * blobSize) when its done, it should call void finishRDFParse
 * (RDFFile f) to abort, it should call void abortRDFParse (RDFFile f)
 * [which will undo all that has been read from that file] 
 */

void
beginReadingRDFFile (RDFFile file)
{
	char		*url;
	int		method = 0;

#ifndef MOZILLA_CLIENT

   /* If standalone, we just use  to open the file */
   NET_StreamClass stream;
   PRFileDesc *fd;
   PRFileInfo fi;
   PRBool bSuccess = FALSE;
    
   url = file->url;
   fd = CallPROpenUsingFileURL(url, PR_RDONLY, 0);
   if(fd)
   {
      if(PR_GetOpenFileInfo(fd, &fi) == PR_SUCCESS)
      {
         char* buf = malloc(fi.size);
         if(PR_Read(fd, buf, fi.size))
         {
            stream.data_object = file;
            if(parseNextRDFXMLBlob (&stream, buf, fi.size))
               bSuccess = TRUE;
         }
         free(buf);
      }
      PR_Close(fd);
   }
   if(bSuccess == TRUE)
      rdf_complete(&stream);
#else

	url = file->url;
	if (file->fileType == ES_RT)	method = URL_INDEX_METHOD;
	rdf_GetURL (gRDFMWContext(file->db), method, NULL, file);

#endif
}



/* Returns a new string with inURL unescaped. */
/* We return a new string because NET_UnEscape unescapes */
/* string in place */
char *
unescapeURL(char *inURL)
{
	char *escapedPath = copyString(inURL);

#ifdef MOZILLA_CLIENT
#ifdef XP_WIN
	replacePipeWithColon(escapedPath);
#endif

	NET_UnEscape(escapedPath);
#endif

	return (escapedPath);
}



/* Given a file URL of form "file:///", return substring */
/* that can be used as a path for PR_Open. */
/* NOTE: This routine DOESN'T allocate a new string */


char *
convertFileURLToNSPRCopaceticPath(char* inURL)
{
#ifdef	XP_WIN
	if (startsWith("file://", inURL))	return (inURL + 8);
	else if (startsWith("mailbox:/", inURL))	return (inURL + 9);
	else if (startsWith("IMAP:/", inURL))	return (inURL + 6);
	else return (inURL);
#else
	/* For Mac & Unix, need preceeding '/' so that NSPR */
	/* interprets path as full path */
	if (startsWith("file://", inURL))	return (inURL + 7);
	else if (startsWith("mailbox:/", inURL))	return (inURL + 8);
	else if (startsWith("IMAP:/", inURL))	return (inURL + 5);
	else return (inURL);
#endif
}

char* MCDepFileURL (char* url) {
	char* furl;  
	int32 len;   
	char* baz = "\\";
	int32 n = 0; 
	furl = convertFileURLToNSPRCopaceticPath(unescapeURL(url));
	len = strlen(furl);
#ifdef XP_WIN
	while (n < len) {
		if (furl[n] == '/') furl[n] = baz[0];
		n++;
	}
#endif
	return furl;
}

PRFileDesc *
CallPROpenUsingFileURL(char *fileURL, PRIntn flags, PRIntn mode)
{
	PRFileDesc* result = NULL;
	const char *path;

	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_Open(path, flags, mode);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



PRDir *
CallPROpenDirUsingFileURL(char *fileURL)
{
	PRDir* result = NULL;
	const char *path;
	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_OpenDir(path);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int32
CallPRWriteAccessFileUsingFileURL(char *fileURL)
{
	int32 result = -1;
	const char *path;
	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_Access(path, PR_ACCESS_WRITE_OK);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int32
CallPRDeleteFileUsingFileURL(char *fileURL)
{
	int32 result = -1;
	const char *path;
	char *escapedPath = unescapeURL(fileURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_Delete(path);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int
CallPR_RmDirUsingFileURL(char *dirURL)
{
	int32 result=-1;
	const char *path;

	char *escapedPath = unescapeURL(dirURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_RmDir(path);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



int32
CallPRMkDirUsingFileURL(char *dirURL, int mode)
{
	int32 result=-1;
	const char *path;

	char *escapedPath = unescapeURL(dirURL);
	path = convertFileURLToNSPRCopaceticPath(escapedPath);

	if (path != NULL)	{
		result = PR_MkDir(path,mode);
	}

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}



#ifdef MOZILLA_CLIENT
DB *
CallDBOpenUsingFileURL(char *fileURL, int flags,int mode, DBTYPE type, const void *openinfo)
{
	DB *result;
	char *path;
	char *escapedPath;

        if (fileURL == NULL) return NULL;

	escapedPath = unescapeURL(fileURL);

#ifdef XP_MAC
	path = WH_FilePlatformName(convertFileURLToNSPRCopaceticPath(fileURL));
	XP_ASSERT(path != NULL);
#else
	
	path = convertFileURLToNSPRCopaceticPath(escapedPath);
#endif
	result = dbopen(path, flags, mode, type, openinfo);
#ifdef XP_MAC
	XP_FREE(path);
#endif

	if (escapedPath != NULL)	freeMem(escapedPath);

	return result;
}
#else
#if defined(XP_WIN) && defined(DEBUG)
/* Some XP functions that are implemented in winfe
 * in the client.
 */
void XP_AssertAtLine( char *pFileName, int iLine )
{
   fprintf(stderr, "assert: line %d, file %s%c\n", __LINE__, pFileName, 7);
}

char* NOT_NULL(const char* x)
{
   return (char*)x;
}
#endif
#endif
