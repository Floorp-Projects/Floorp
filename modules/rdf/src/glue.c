/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* external routines */
extern	MWContext	*FE_GetRDFContext(void);


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
  if (f) {
    freeMem(f->line);
    freeMem(f->currentSlot);
    freeMem(f->holdOver);
    freeNamespaces(f) ;
  }
}



void
rdf_abort(NET_StreamClass *stream, int status)
{
  RDFFile f = (RDFFile)stream->data_object;
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
	char		*navCenterURL = NULL;

	if ((status < 0) && (urls != NULL))
	{
		if ((cx != NULL) && (urls->error_msg != NULL))
		{
			FE_Alert(cx, urls->error_msg);
		}

		/* if unable to read in navcntr.rdf file, create some default views */
		
		PREF_CopyCharPref("browser.NavCenter", &navCenterURL);
		if (navCenterURL != NULL)
		{
			if (urls->address != NULL)
			{
				if (!strcmp(urls->address, navCenterURL))
				{
					remoteStoreAdd(gRemoteStore, gNavCenter->RDF_BookmarkFolderCategory,
						gCoreVocab->RDF_parent, gNavCenter->RDF_Top, RDF_RESOURCE_TYPE, 1);
					remoteStoreAdd(gRemoteStore, gNavCenter->RDF_History, gCoreVocab->RDF_parent,
						gNavCenter->RDF_Top, RDF_RESOURCE_TYPE, 1);
					remoteStoreAdd(gRemoteStore, gNavCenter->RDF_LocalFiles, gCoreVocab->RDF_parent,
						gNavCenter->RDF_Top, RDF_RESOURCE_TYPE, 1);
					remoteStoreAdd(gRemoteStore, gNavCenter->RDF_Search, gCoreVocab->RDF_parent,
						gNavCenter->RDF_Top, RDF_RESOURCE_TYPE, 1);
				}
			}
			freeMem(navCenterURL);
		}
	}
	NET_FreeURLStruct (urls);
}



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



int
rdf_GetURL (MWContext *cx,  int method, Net_GetUrlExitFunc *exit_routine, RDFFile rdfFile)
{
	URL_Struct                      *urls;

	if (cx == NULL)  return 0;
	urls = NET_CreateURLStruct(rdfFile->url, (NET_ReloadMethod)rdfRetrievalType(rdfFile));
	if (urls == NULL) return 0;
        /*	urls->use_local_copy = rdfFile->localp;*/
	urls->fe_data = rdfFile;
	if (method) urls->method = method;
/*
	if (rdfFile->localp) {
          NET_GetURLQuick(urls, FO_CACHE_AND_RDF, cx, rdf_GetUrlExitFunc);
        } else {
          NET_GetURLQuick(urls, FO_CACHE_AND_RDF, cx, rdf_GetUrlExitFunc);
        }
*/
	NET_GetURL(urls, FO_CACHE_AND_RDF, cx, rdf_GetUrlExitFunc);
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
gRDFMWContext()
{
#ifndef MOZILLA_CLIENT
   return NULL;
#else
	void		*cx;

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

   /* If standalone, we just use NSPR to open the file */
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
	rdf_GetURL (gRDFMWContext(), method, NULL, file);

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
