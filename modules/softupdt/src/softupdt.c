/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "softupdt.h"
#include "su_instl.h"
#include "su_folderspec.h"
#define NEW_FE_CONTEXT_FUNCS
#include "net.h"
#include "zig.h"
#include "libevent.h"
#include "prefapi.h"
#include "prprf.h"
#include "mkutils.h"
#include "fe_proto.h"
#include "prthread.h"
#include "xpgetstr.h"
#include "prmon.h"
#include "pw_public.h"
#include "NSReg.h"
#include "VerReg.h"
#include "libi18n.h"

#include "jvmmgr.h"

extern int MK_OUT_OF_MEMORY;

/* xp_string defines */
extern int SU_LOW_DISK_SPACE_WARNING;
extern int SU_NOT_ENOUGH_SPACE;
extern int SU_NOT_A_JAR_FILE;
extern int SU_SECURITY_CHECK;
extern int SU_INSTALL_FILE_HEADER;
extern int SU_INSTALL_FILE_MISSING;
extern int SU_PROGRESS_DOWNLOAD_TITLE;
extern int SU_PROGRESS_DOWNLOAD_LINE1;
extern int REGPACK_PROGRESS_TITLE;
extern int REGPACK_PROGRESS_LINE1;
extern int REGPACK_PROGRESS_LINE2;


extern uint32 FE_DiskSpaceAvailable (MWContext *context, const char *lpszPath );


/* structs */

/* su_DownloadStream
 * keeps track of all SU specific data needed for the stream
 */ 
typedef struct su_DownloadStream_struct {
    XP_File         fFile;
    char *          fJarFile;	/* xpURL location of the downloaded file */
    URL_Struct *    fURL;
    MWContext *     fContext;
    SoftUpdateCompletionFunction fCompletion;
    void *          fCompletionClosure;
    int32           fFlags;     /* download flags */
    pw_ptr          progress ;
} su_DownloadStream;

/* su_URLFeData
 * passes data between the trigger and the stream
 */
typedef struct su_URLFeData_struct {
	SoftUpdateCompletionFunction fCompletion;
	void * fCompletionClosure;
    int32 fFlags;    /* download flags */
} su_URLFeData;

static char * EncodeSoftUpJSArgs(const char * fileName, XP_Bool silent, XP_Bool force, const char* charset);

/* Stream callbacks */
int su_HandleProcess (void *streamData, const char *buffer, int32 buffLen);
void su_HandleComplete (void *streamData);
void su_HandleAbort (void *streamData, int reason);
unsigned int su_HandleWriteReady (void * streamData );

/* Completion routine for stream handler. Deletes su_DownloadStream */
void su_CompleteSoftwareUpdate(MWContext * context,  
								SoftUpdateCompletionFunction f,
								void * completionClosure,
								int result,
								su_DownloadStream* realStream);

void su_NetExitProc(URL_Struct* url, int result, MWContext * context);
void su_HandleCompleteJavaScript (su_DownloadStream* realStream);

int su_GetLastRegPackTime(int32 *lastRegPackTime);
int su_SetLastRegPackTime(int32 lastRegPackTime);

XP_Bool su_RegPackTime(void);
void su_RegPackCallback(void *userData, int32 bytes, int32 totalBytes);
int su_PackRegistry();

/* Handles cancel of progress dialog by user */
void cancelProgressDlg(void * closure) 
{
	su_DownloadStream* realStream = (su_DownloadStream*)closure;
	if (realStream->fContext) {
		XP_InterruptContext(realStream->fContext);
	}
}


/* Completion routine for SU_StartSoftwareUpdate */

void su_CompleteSoftwareUpdate(MWContext * context, 
								SoftUpdateCompletionFunction f,
								void * completionClosure,
								int result,
								su_DownloadStream* realStream)
{
	/* Notify the trigger */
	if ( f != NULL )
		f(result, completionClosure);

	/* Clean up */
	if (realStream)
	{
		if ( realStream->fJarFile )
		{
			result = XP_FileRemove( realStream->fJarFile, xpURL );
			XP_FREE( realStream->fJarFile );
		}
		XP_FREE( realStream);
	}
}

/* a struct to hold the arguments */
struct su_startCallback_
{
	char * url;
	char * name;
	SoftUpdateCompletionFunction f;
	void * completionClosure;
    int32 flags;
	MWContext * context;
} ;

typedef struct su_startCallback_ su_startCallback;

XP_Bool            QInsert( su_startCallback * Item );
su_startCallback * QGetItem(void);
void               SU_InitMonitor(void);
void               SU_DestroyMonitor(void);

/*
 * This struct represents one entry in a queue setup to hold download 
 * requests if there are more than one.
 */
typedef struct su_QItem
{
	struct su_QItem   * next;
	struct su_QItem   * prev;
	su_startCallback  * QItem;
} su_QItem;

typedef struct su_UninstallContext_struct	{
    REGENUM         state;
    XP_Bool         bShared;
} su_UninstallContext;

XP_Bool     DnLoadInProgress = FALSE;
PRMonitor * su_monitor = NULL;

#ifdef XP_MAC
#pragma export on
#endif

void SU_InitMonitor(void)
{
	su_monitor = PR_NewMonitor();
	XP_ASSERT( su_monitor != NULL );
}


void SU_DestroyMonitor(void)
{
	if ( su_monitor != NULL ) 
	{
		PR_DestroyMonitor(su_monitor);
		su_monitor = NULL;
	}
}

#ifdef XP_MAC
#pragma export reset
#endif

/*
 * Queue maintanence is done here 
 */
su_QItem  * Qhead = NULL;
su_QItem  * Qtail = NULL;

XP_Bool QInsert( su_startCallback * Item )
{
	su_QItem *p = XP_ALLOC( sizeof (su_QItem));

	if (p == NULL)
		return FALSE;

	p->QItem = Item;
	p->next = Qhead;
	p->prev = NULL;
	if (Qhead != NULL)
		Qhead->prev = p;
	Qhead = p;
	if (Qtail == NULL) /* First Item inserted in Q? */
		Qtail = p;

	return TRUE;
}

su_startCallback *QGetItem(void)
{
	su_QItem *Qtemp = Qtail;

	if (Qtemp != NULL)
	{
		su_startCallback *p = NULL;
	
		Qtail = Qtemp->prev;
	
		if (Qtail == NULL) /* Last Item deleted from Q? */
			Qhead = NULL;
		else
			Qtail->next = NULL;
	
		p = Qtemp->QItem;
		XP_FREE(Qtemp);
		return p;
	}
	return NULL;
}

 
int TestFreeSpacePriorToPassing(char* file)
{
	XP_StatStruct 	st;
	int	            status = 0;
	uint32			availSpace = 0;

	char* tempFolder;
	
	
	status = XP_Stat( file, &st,  xpURL );
	
	if (status != 0)
	{
	   return status;
	}
	    
	    
	tempFolder	=	XP_TempDirName();
	
	if(tempFolder == NULL)
	{
		return su_ErrInternalError;
	}
	
	availSpace  = FE_DiskSpaceAvailable (NULL, tempFolder );

	/* 
	   At best, a zip jar file will produce 50 percent compresssion.  Let's
	   make sure we have this. 
	*/
	
    if ( availSpace > (st.st_size * 1.5) ) 
	{
		return 0;
	}
	else
	{
		return su_DiskSpaceError;
	}
}

/* 
 * timer callback to start the network download of a JAR file
 */
PRIVATE void
su_FE_timer_callback( void * data)
{
	su_startCallback * c;
	URL_Struct * urlS = NULL;
	su_URLFeData * fe_data = NULL;
	XP_Bool  errFound = TRUE;

	c = (su_startCallback*)data;

	if (c->context == NULL)
	{
		su_CompleteSoftwareUpdate(c->context, c->f, c->completionClosure, su_ErrInternalError, NULL);
		goto done;
	}

 	if (c->url == NULL)
	{
		su_CompleteSoftwareUpdate(c->context, c->f, c->completionClosure, su_ErrInvalidArgs, NULL);
		goto done;
	}
    
    urlS = NET_CreateURLStruct(c->url, NET_DONT_RELOAD);
	if (urlS == NULL)
    {
		su_CompleteSoftwareUpdate(c->context, c->f, c->completionClosure, MK_OUT_OF_MEMORY, NULL);
		goto done;
	}

    /* fe_data holds the arguments that need to be passed
     * to our download stream
     */
	fe_data = XP_ALLOC(sizeof(su_URLFeData));

	if (fe_data == NULL)
	{
		su_CompleteSoftwareUpdate(c->context, c->f, c->completionClosure, MK_OUT_OF_MEMORY, NULL);
		NET_FreeURLStruct(urlS);
		goto done;
	}

	fe_data->fCompletion = c->f;
	fe_data->fCompletionClosure = c->completionClosure;
    fe_data->fFlags = c->flags;

	urlS->fe_data = fe_data;
	urlS->must_cache = TRUE;	/* This is needed for partial caching */

	errFound = FALSE;
	NET_GetURL(urlS, FO_CACHE_AND_SOFTUPDATE, c->context, su_NetExitProc);

done:
	if (errFound) 
	{
		/* pops next item from queue or sets download flag to false */
                su_NetExitProc(NULL, 0, NULL);
	}
	if (c->url)
		XP_FREE( c->url);
	if (c->name)
		XP_FREE( c->name);
	XP_FREE( c );
}

/* NET_GetURL exit procedure */
void su_NetExitProc(URL_Struct* url, int result, MWContext * context)
{
	su_startCallback * c;
    
	if (result == MK_UNABLE_TO_CONVERT)
	{
		PR_EnterMonitor(su_monitor);
		DnLoadInProgress = FALSE;
		
		if (context)
		{
			FE_Alert(context, XP_GetString(SU_NOT_ENOUGH_SPACE));
		}
		PR_ExitMonitor(su_monitor);
	} 
	else if (result != MK_CHANGING_CONTEXT)
	{
	    PR_EnterMonitor(su_monitor);
	    if ((c = QGetItem()) != NULL)
	    {
		    FE_SetTimeout( su_FE_timer_callback, c, 1 );
	    }
	    else
	    {
		    DnLoadInProgress = FALSE;
	    }
	    PR_ExitMonitor(su_monitor);
    }
}

#ifdef XP_MAC
#pragma export on
#endif


XP_Bool SU_IsUpdateEnabled(void)
{
    XP_Bool enabled = FALSE;
    PREF_GetBoolPref( AUTOUPDATE_ENABLE_PREF, &enabled);

    return (enabled);
}


/* SU_StartSoftwareUpdate
 * Main public interface to software update
 */
PUBLIC XP_Bool 
SU_StartSoftwareUpdate(MWContext * context, 
						const char * url, 
						const char * name, 
						SoftUpdateCompletionFunction f,
						void * completionClosure,
                        int32 flags)
{
	URL_Struct * urlS = NULL;
	su_URLFeData * fe_data = NULL;
	XP_Bool enabled;

	/* Better safe than sorry */
	PREF_GetBoolPref( AUTOUPDATE_ENABLE_PREF, &enabled);
	if (!enabled)
		return FALSE;

	/* if we do not have a context, create one */

	if ( context == NULL )
	{
		return FALSE;
	}

	/* Need to process this on a timer because netlib is not reentrant */
	{
		su_startCallback * varHolder;
		varHolder = XP_ALLOC( sizeof (su_startCallback));
		if (varHolder == NULL)
		{
			su_CompleteSoftwareUpdate(context, f, completionClosure, MK_OUT_OF_MEMORY, NULL);
			return FALSE;
		}
		varHolder->url = url ? XP_STRDUP( url ) : NULL;
		varHolder->name = name ? XP_STRDUP( name ) : NULL;
		varHolder->f = f;
		varHolder->context = context;
		varHolder->completionClosure = completionClosure;
		varHolder->flags = flags;

		PR_EnterMonitor(su_monitor);
		if ( DnLoadInProgress )
		{
			if (!QInsert( varHolder ))
			{       /* cleanup */
				su_CompleteSoftwareUpdate(context, f, completionClosure, MK_OUT_OF_MEMORY, NULL);
				if (varHolder->url)
					XP_FREE( varHolder->url);
				if (varHolder->name)
					XP_FREE( varHolder->name);
				XP_FREE( varHolder );
				return FALSE;
			}
		}
		else
		{
			DnLoadInProgress = TRUE;
			FE_SetTimeout( su_FE_timer_callback, varHolder, 1 );
		}
		PR_ExitMonitor(su_monitor);
		return TRUE;
	}
}

/* New stream callback */
/* creates the stream, and a opens up a temporary file */
NET_StreamClass * SU_NewStream (int format_out, void * registration,
								URL_Struct * request, MWContext *context)
{
	su_DownloadStream * streamData = NULL;
	su_URLFeData * fe_data = NULL;
	NET_StreamClass * stream = NULL;
	SoftUpdateCompletionFunction completion = NULL;
	void * completionClosure = NULL;
    int32 flags = 0;
	short result = 0;
	XP_Bool	isJar;
    pw_ptr prg = NULL;
	MWContext *fContext;
/*  char path[256]; */

	/* Initialize the stream data by data passed in the URL*/
	fe_data = (su_URLFeData *) request->fe_data;

	if ( fe_data != NULL )
	{
		completion = fe_data->fCompletion;
		completionClosure = fe_data->fCompletionClosure;
        flags = fe_data->fFlags;
    }

	/* Make sure that we are loading a Java archive 
	 Strictly, we should accept only APPLICATION_JAVAARCHIVE,\
	but I know that many servers will be misconfigured 
	so we'll try to deal with text/plain, and octet-stream
	So, the logic is:
	Anything but HTML is OK. Netlib will only trigger us
	with the correct MIME type, and when we are using triggers
	this is the right thing to do. HTML is usually an error message
	from the server.
	*/
	if (request->content_type)
	{
		if ( XP_STRCMP( APPLICATION_JAVAARCHIVE, request->content_type) == 0) /* Exact match */
			isJar = TRUE;
		else if ( XP_STRCMP( TEXT_HTML, request->content_type) == 0)
			isJar = FALSE;
		else
			isJar = TRUE;
	}
	else
		isJar = TRUE;	/* Assume we have JAR if no content type */

	/* If we got the wrong MIME type... */
	if (isJar == FALSE)
	{
		if (context)
			FE_Alert(context, XP_GetString(SU_NOT_A_JAR_FILE));
		goto fail;
	}
	
	/* Create all the structs */	
	streamData = XP_CALLOC(sizeof(su_DownloadStream), 1);
	
	if (streamData == NULL)
	{
		result = MK_OUT_OF_MEMORY;
		goto fail;
	}
    
    prg = PW_Create(context, pwStandard);
	fContext = PW_CreateProgressContext();
    PW_AssociateWindowWithContext(fContext, prg);
	fContext->url = request->address;
	NET_SetNewContext(request, fContext, su_NetExitProc); 
    
	stream = NET_NewStream ("SmartUpdate", 
			su_HandleProcess,
			su_HandleComplete, 
			su_HandleAbort, 
			su_HandleWriteReady, 
			streamData,
			fContext);

	if (stream == NULL)
	{
		result = MK_OUT_OF_MEMORY;
		goto fail;
	}

	streamData->fURL = request;
	streamData->fContext = fContext;
	streamData->fCompletion = completion;
	streamData->fCompletionClosure = completionClosure;
    streamData->fFlags = flags;
   	streamData->progress = prg;

	if (request->fe_data)
	{
		XP_FREE( request->fe_data);
		request->fe_data = NULL;
	}
	/* Here we jump through few hoops to get the file name in xpURL kind */
	streamData->fJarFile = WH_TempName( xpURL, NULL );

	if (streamData->fJarFile == NULL)
	{
		result = su_ErrInternalError;
		goto fail;
	}
	streamData->fFile = XP_FileOpen( streamData->fJarFile, xpURL, XP_FILE_WRITE_BIN );
	
	if ( streamData->fFile == 0)
	{
		result = su_ErrInternalError;
		goto fail;
	}
    
    PW_SetCancelCallback(streamData->progress, cancelProgressDlg, streamData);
    PW_SetWindowTitle(streamData->progress, XP_GetString(SU_PROGRESS_DOWNLOAD_TITLE));
/*
    PR_snprintf(path, 256, XP_GetString(SU_PROGRESS_DOWNLOAD_LINE1), streamData->fURL->address);
    PW_SetLine1(streamData->progress, path);
*/
    PW_SetLine1(streamData->progress, streamData->fURL->address);
    PW_SetLine2(streamData->progress, NULL);
#ifndef XP_UNIX
	PW_Show(prg);
#endif
  
/* return the stream */
	return stream;

fail:
	if (stream != NULL)
		XP_FREE( stream );
	su_CompleteSoftwareUpdate( context, completion, completionClosure, result, streamData );
	return NULL;
}

#ifdef XP_MAC
#pragma export reset
#endif

/* su_HandleProcess
 * stream method, writes to disk
 */
int su_HandleProcess (void *streamData, const char *buffer, int32 buffLen)
{
	return XP_FileWrite( buffer, buffLen, ((su_DownloadStream*)streamData)->fFile );
}

/* su_HandleAbort
 * Clean up
 */
void su_HandleAbort (void *streamData, int reason)
{
	su_DownloadStream* realStream = (su_DownloadStream*)streamData;
	
	/* Close the files */
	if (realStream->fFile)
		XP_FileClose(realStream->fFile);
#ifndef XP_UNIX
    PW_Hide((pw_ptr)(realStream->progress));
#endif
	/* Report the result */
	su_CompleteSoftwareUpdate(realStream->fContext, 
					realStream->fCompletion, realStream->fCompletionClosure, reason, realStream);
}

unsigned int su_HandleWriteReady (void * streamData )
{
	return USHRT_MAX;	/* Returning -1 causes errors when loading local file */
}

/* su_HandleComplete
 * Clean up
 */
void su_HandleComplete (void *streamData)
{

	su_DownloadStream* realStream = (su_DownloadStream*)streamData;

	if (realStream->fFile)
		XP_FileClose(realStream->fFile);
#ifndef XP_UNIX
    PW_Hide((pw_ptr)(realStream->progress));
#endif
	su_HandleCompleteJavaScript( realStream );
}

/* This is called when Mocha is done with evaluation */
static void
su_mocha_eval_exit_fn(void * data, char * str, size_t len, char * wysiwyg_url,
		 char * base_href, Bool valid)
{
	short result = 0;
	su_DownloadStream* realStream = (su_DownloadStream*) data;
	MWContext *context = realStream->fContext;
	
	if (valid == FALSE)
		result = su_ErrBadScript;

	if (str)
		XP_FREE( str );

	su_CompleteSoftwareUpdate( context, 
					realStream->fCompletion, realStream->fCompletionClosure, 0, realStream );

	FE_DestroyWindow(context);
}
/* su_ReadFileIntoBuffer
 * given a file name, reads it into buffer
 * returns an error code
 */
static short su_ReadFileIntoBuffer(char * fileName, void ** buffer, unsigned long * bufferSize)
{
	XP_File file;
	XP_StatStruct st;
	short result = 0;

	if ( XP_Stat( fileName, &st,  xpURL ) != 0 )
	{
		result = su_ErrInternalError;
		goto fail;
	}
	*bufferSize = st.st_size;

	*buffer = XP_ALLOC( st.st_size );
	if (*buffer == NULL)
	{
		result = MK_OUT_OF_MEMORY;
		goto fail;
	}
	file = XP_FileOpen( fileName, xpURL, XP_FILE_READ_BIN );
	if ( file == 0 )
	{
		result = su_ErrInternalError;
		goto fail;
	}
	if ( XP_FileRead( *buffer, *bufferSize, file ) != *bufferSize )
	{
		result = su_ErrInternalError;
		XP_FileClose( file );
		goto fail;
	}
	XP_FileClose( file );
	return result;

fail:
	if (*buffer != NULL)
		XP_FREE( * buffer);
	*buffer = NULL;
	return result;
}

/* Encodes the args in the format
 * MOCHA_CONTEXT_PREFIX<File>CR<silent>CR<force>
 * Booleans are encoded as T or F.
 * DecodeSoftUpJSArgs is in lm_supdt.c
 */
static char *
EncodeSoftUpJSArgs(const char * fileName, XP_Bool force, XP_Bool silent,const char* charset )
{
    char * s;
    int32 length;
    if (fileName == NULL)
        return NULL;

    length = 	XP_STRLEN(MOCHA_CONTEXT_PREFIX) +
    			XP_STRLEN(fileName) + 
            	6 + 							 /* 2 booleans and 3 CR and one NULL*/
            	XP_STRLEN(charset); 



    s = XP_ALLOC(length);
    if (s != NULL)
    {
        int32 tempLen;
        
        s[0] = 0;
        XP_STRCAT(s, MOCHA_CONTEXT_PREFIX);
        XP_STRCAT(s, fileName);                 	/* Prefix and Name */
        
        tempLen = XP_STRLEN(s);						/* CR */
        s[tempLen] = CR;
        s[tempLen + 1] = silent ? 'T' : 'F';		/* Silent */
        s[tempLen + 2] = CR;						/* CR */
        s[tempLen + 3] = force ? 'T' : 'F';			/* Force */
		s[tempLen + 4] = CR;						/* CR */
		s[tempLen + 5] = 0;	
    	XP_STRCAT(&(s[tempLen + 5]), charset);		/* charset */
    }
    return s;
}

/* su_HandleCompleteJavaScript
 * Reads in the mocha script out of the Jar file
 * Executes the script
 */
void su_HandleCompleteJavaScript (su_DownloadStream* realStream)
{
	short result = 0;
	void * buffer = NULL;
	unsigned long bufferSize;
	char * installerJarName = NULL;
	char * installerFileNameURL = NULL;
	char * codebase = NULL;
	int32  urlLen;
	unsigned long fileNameLength;
  	char * charset = NULL;
	unsigned long charsetLen;
	ZIG * jarData = NULL;
	char s[255];
	char * jsScope = NULL;
	Chrome chrome;
	MWContext * context;
	JSPrincipals * principals = NULL;
  	ETEvalStuff * stuff = NULL;
	char * jarCharset = NULL;
	unsigned long jarCharsetLen;
	XP_Bool		didConfirm;
	
#if 0
	/* hold off on this change -- may create security holes 
	   see later in this function for more detail */
    char * nativeJar;
#endif

	
	result = TestFreeSpacePriorToPassing(realStream->fJarFile);
/*	
	if (result != 0)
	{
		PR_snprintf(s, 255, XP_GetString(SU_NOT_ENOUGH_SPACE), installerJarName);
		FE_Alert(realStream->fContext, s);
		result = su_DiskSpaceError;
		goto fail;
	}
*/

	if (result != 0)
	{
		PR_snprintf(s, 255, XP_GetString(SU_LOW_DISK_SPACE_WARNING), installerJarName);
		didConfirm = FE_Confirm (realStream->fContext, s);
		
		if (didConfirm)
		{
			/* the user wants to bail */
			result = su_DiskSpaceError;
			goto fail;
		}
		else
		{
			/* the user is a daredevil */
			result = 0;
		}
	}
	
	

	/* Initialize the JAR file */

	jarData = SOB_new();
	if ( jarData == NULL)
	{
		result = MK_OUT_OF_MEMORY;
		goto fail;
	}

	SOB_set_context (jarData, realStream->fContext);

	result = SOB_pass_archive( ZIG_F_GUESS, 
						realStream->fJarFile, 
						realStream->fURL->address, 
						jarData);

	if (result < 0)
	{
        char *errMsg = SOB_get_error(result); /* do not free, bug 157209 */
		PR_snprintf(s, 255, XP_GetString(SU_SECURITY_CHECK), (errMsg?errMsg:"") );
		FE_Alert(realStream->fContext, s);

		result = su_JarError;
		goto fail;
	}

	/* Get the installer file name */
	result = SOB_get_metainfo( jarData, NULL, INSTALLER_HEADER, (void**)&installerJarName, &fileNameLength);

	if (result < 0)
	{
		FE_Alert(realStream->fContext, XP_GetString(SU_INSTALL_FILE_HEADER));

		result = su_JarError;
		goto fail;
	}
	
	installerJarName[fileNameLength] = 0;	/* Terminate the string */

	/* Need a temporary file name that is the xpURL form */
	installerFileNameURL = WH_TempName( xpURL, NULL );

	if ( installerFileNameURL == NULL)
	{
		result = su_JarError;
		goto fail;
	}

    /* extract the character set info, if any */
    	
	result = SOB_get_metainfo( jarData, installerJarName, CHARSET_HEADER, (void**)&charset, &charsetLen);

	if ((result < 0) || (INTL_CharSetNameToID(charset) == CS_UNKNOWN))
	{
		if (charset)
		{
			/*
			 * if you hit this assert, that means that your jar
			 * file has a bad charset name in it
			 */
			XP_ASSERT(0);
			XP_FREE(charset);
		}
		charset = XP_STRDUP((const char *) INTL_CsidToCharsetNamePt(FE_DefaultDocCharSetID(realStream->fContext)));
	}
	
	
	
    /* extract the character set info for the Jar File, if any */
    	
	result = SOB_get_metainfo( jarData, NULL, CHARSET_HEADER, (void**)&jarCharset, &jarCharsetLen);

	if ((result < 0) || (INTL_CharSetNameToID(jarCharset) == CS_UNKNOWN))
	{
		if (jarCharset)
		{
			/*
			 * if you hit this assert, that means that your jar
			 * file has a bad charset name in it
			 */
			XP_ASSERT(0);
			XP_FREE(jarCharset);
		}
		
		jarCharset = XP_STRDUP((const char *) INTL_CsidToCharsetNamePt(FE_DefaultDocCharSetID(realStream->fContext)));
	}
	

	/* Extract the script out */

	result = SOB_verified_extract( jarData, installerJarName, installerFileNameURL);
	
	if (result == ZIG_ERR_DISK)
	{
		PR_snprintf(s, 255, XP_GetString(SU_NOT_ENOUGH_SPACE), installerJarName);
		FE_Alert(realStream->fContext, s);
		result = su_DiskSpaceError;
		goto fail;
	}
	else if (result < 0)
	{
		PR_snprintf(s, 255, XP_GetString(SU_INSTALL_FILE_MISSING), installerJarName);
		FE_Alert(realStream->fContext, s);
		result = su_JarError;
		goto fail;
	}
	
	/* Read the file in */
	result = su_ReadFileIntoBuffer( installerFileNameURL, &buffer, &bufferSize);
	XP_FileRemove( installerFileNameURL, xpURL);

	if (result < 0)
	{
		result = su_JarError;
		goto fail;
	}

	/* Temporary hack to pass the zig data */
	/* close the archive */
	
	SOB_destroy( jarData);
	jarData = NULL;

    /* add installer .JAR to the classpath */
#if 0
    /* hold off on this change -- may create security holes if we switch
     * to the Sun JVM. Wouldn't want to offer this functionality and then
     * take it away.
     *   This fix doesn't work, by the way. The temp name MUST have a .jar
     * or .zip extension or else Java tries to treat them as directories.
     * we must first guarantee the correct extension.
     */
    nativeJar = WH_FileName( realStream->fJarFile, xpURL );
    if ( nativeJar != NULL ) {
        LJ_AddToClassPath( nativeJar );
        XP_FREE( nativeJar );
    }
#endif

	/* For security reasons, installer JavaScript has to execute inside a
	special context. This context is created by ET_EvaluateBuffer as a JS object
	of type SoftUpdate. the arguments to the object are passed in the string,
    jsScope
	*/
	jsScope = EncodeSoftUpJSArgs(realStream->fJarFile, 
                                 ((realStream->fFlags & FORCE_INSTALL) != 0),
                                 ((realStream->fFlags & SILENT_INSTALL) != 0),
                                 jarCharset);
    if (jsScope == NULL)
        goto fail;

	XP_BZERO(&chrome, sizeof(Chrome));
	chrome.location_is_chrome = TRUE;
#ifndef XP_MAC
    chrome.type = MWContextDialog;
#else
    /* MacFE doesn't respect chrome.is_modal for MWContextDialog anymore.
       We need a different solution for int'l content encoding on macs    */
    chrome.type = MWContextBrowser;
#endif
	chrome.l_hint = -3000;
	chrome.t_hint = -3000;
	context = FE_MakeNewWindow(realStream->fContext, NULL, NULL, &chrome);
	if (context == NULL)
		goto fail;
	urlLen = XP_STRLEN(realStream->fURL->address);
	codebase = XP_ALLOC( urlLen + XP_STRLEN(installerJarName) + 2 );
        if (codebase == NULL) { 
	    goto fail;
	}
	XP_STRCPY( codebase, realStream->fURL->address );
	codebase[urlLen] = '/';
	XP_STRCPY( codebase+urlLen+1, installerJarName );

	principals = LM_NewJSPrincipals(realStream->fURL, installerJarName, codebase);
	if (principals == NULL)
		goto fail;

	ET_StartSoftUpdate(context, codebase);

	/* Execute the mocha script, result will be reported in the callback */
	realStream->fContext = context;

	stuff = (ETEvalStuff *) XP_NEW_ZAP(ETEvalStuff);
        if (!stuff) {
	    FE_DestroyWindow(context);
	    goto fail;
	}

	stuff->len = bufferSize;
	stuff->line_no = 1;
	stuff->scope_to = jsScope;
	stuff->want_result = JS_TRUE;
	stuff->data = realStream;
	stuff->version = JSVERSION_DEFAULT;
	stuff->principals = principals;
/* FIX	stuff->charset = charset; */

	ET_EvaluateScript(context, buffer, stuff, su_mocha_eval_exit_fn);

	goto done;

fail:
	if (jarData != NULL)
		SOB_destroy( jarData);
	su_CompleteSoftwareUpdate( realStream->fContext, 
					realStream->fCompletion, realStream->fCompletionClosure, result, realStream );
	/* drop through */
done:
	
    /* Don't free the following, they are freed in the mocha event destructor!
     
    XP_FREEIF(jsScope);
    XP_FREEIF(codebase); 
    XP_FREEIF(charset);
    
    */
	
	XP_FREEIF( installerFileNameURL );
    XP_FREEIF( installerJarName );
    XP_FREEIF( jarCharset );
	
	/* Should we purge stuff from the disk cache here? */
}

/* XXX: Move JavaGetBoolPref to lj_init.c.
 * Delete IsJavaSecurityEnabled and IsJavaSecurityDefaultTo30Enabled functions
 * XXX: Cache all security preferences while we are running on mozilla
 *      thread. Hack for 4.0 
 */
#define MAX_PREF 20

struct {
  char *pref_name;
  XP_Bool value;
} cachePref[MAX_PREF];

static int free_idx=0;
static XP_Bool locked = FALSE;

static void AddPrefToCache(char *pref_name, XP_Bool value) 
{
  if (!pref_name)
      return;

  if (free_idx >= MAX_PREF) {
      XP_ASSERT(FALSE); /* Implement dynamic growth of preferences */
      return;
  }

  cachePref[free_idx].pref_name = XP_STRDUP(pref_name);
  cachePref[free_idx].value = value;
  free_idx++;
}

static XP_Bool GetPreference(char *pref_name, XP_Bool *pref_value) 
{
  int idx = 0;
  *pref_value = FALSE;

  if (!pref_name) 
      return FALSE;

  for (; idx < free_idx; idx++) {
    if (XP_STRCMP(cachePref[idx].pref_name, pref_name) == 0) {
        *pref_value = cachePref[idx].value;
        locked = TRUE;
        return TRUE;
    }
  }

  if (locked) {
    XP_ASSERT(FALSE); /* Implement dynamic growth of preferences */
    return FALSE;
  }
      
  if (PREF_GetBoolPref(pref_name, pref_value) >=0) {
      AddPrefToCache(pref_name, *pref_value);
      return TRUE;
  }

  return FALSE;
}

#ifdef XP_MAC
#pragma export on
#endif

int PR_CALLBACK JavaGetBoolPref(char * pref_name) 
{
	XP_Bool pref;
        int ret_val;
        GetPreference(pref_name, &pref);
        ret_val = pref;
	return ret_val;
}

int PR_CALLBACK IsJavaSecurityEnabled() 
{
	XP_Bool pref;
        int ret_val;
	GetPreference("signed.applets.codebase_principal_support", &pref);
        ret_val = !pref;
	return ret_val;
}

int PR_CALLBACK IsJavaSecurityDefaultTo30Enabled() 
{
	return JavaGetBoolPref("signed.applets.local_classes_have_30_powers");
}

#ifdef XP_MAC
#pragma export reset
#endif

int su_UninstallDeleteFile(char *fileNamePlatform)
{
	char * fileName;
	int32 err;
	XP_StatStruct statinfo;
          
	fileName = XP_PlatformFileToURL(fileNamePlatform);
    if (fileName != NULL)
	{
		char * temp = XP_STRDUP(&fileName[7]);
		XP_FREEIF(fileName);
		fileName = temp;
		if (fileName)
		{
			err = XP_Stat(fileName, &statinfo, xpURL);
			if (err != -1)
			{
				if ( XP_STAT_READONLY( statinfo ) )
				{
					err = -1;
				}
				else if (!S_ISDIR(statinfo.st_mode))
				{
					err = XP_FileRemove ( fileName, xpURL );
					if (err != 0)
                    {
#ifdef XP_PC
/* REMIND  need to move function to generic XP file */
						su_DeleteOldFileLater( (char*)fileNamePlatform );
#endif
					}

				}
                else
                {
                    err = -1;
                }
			}
			else
			{
				err = -1;
			}
    		
        }
		else
		{
			err = -1;
		}
	}   

	
	XP_FREEIF(fileName);
	return err;
}


REGERR su_UninstallProcessItem(char *component_path)
{
    int refcount;
    int err;
    char filepath[MAXREGPATHLEN];

    err = VR_GetPath(component_path, sizeof(filepath), filepath);
    if ( err == REGERR_OK )
    {
        err = VR_GetRefCount(component_path, &refcount);  
        if ( err == REGERR_OK )
        {
            --refcount;
            if (refcount > 0)
                err = VR_SetRefCount(component_path, refcount);  
            else 
            {
                err = VR_Remove(component_path);
                err = su_UninstallDeleteFile(filepath);
            }
        }
        else
        {
            /* delete node and file */
            err = VR_Remove(component_path);
            err = su_UninstallDeleteFile(filepath);
        }
    }
    return err;
}

#ifdef XP_MAC
#pragma export on
#endif
int32 SU_Uninstall(char *regPackageName)
{
    REGERR status = REGERR_FAIL;
    char pathbuf[MAXREGPATHLEN+1] = {0};
    char sharedfilebuf[MAXREGPATHLEN+1] = {0};
    REGENUM state = 0;
    int32 length;
    int32 err;

    if (regPackageName == NULL)
        return REGERR_PARAM;

    if (pathbuf == NULL)
        return REGERR_PARAM;

    /* Get next path from Registry */
    status = VR_Enum( regPackageName, &state, pathbuf, MAXREGPATHLEN );

    /* if we got a good path */
    while (status == REGERR_OK)
    {
        char component_path[2*MAXREGPATHLEN+1] = {0};
        XP_STRCAT(component_path, regPackageName);
        length = XP_STRLEN(regPackageName);
        if (component_path[length - 1] != '/')
            XP_STRCAT(component_path, "/");
        XP_STRCAT(component_path, pathbuf);
        err = su_UninstallProcessItem(component_path);
        status = VR_Enum( regPackageName, &state, pathbuf, MAXREGPATHLEN );  
    }
    
    err = VR_Remove(regPackageName);

    state = 0;
    status = VR_UninstallEnumSharedFiles( regPackageName, &state, sharedfilebuf, MAXREGPATHLEN );
    while (status == REGERR_OK)
    {
        err = su_UninstallProcessItem(sharedfilebuf);
        err = VR_UninstallDeleteFileFromList(regPackageName, sharedfilebuf);
        status = VR_UninstallEnumSharedFiles( regPackageName, &state, sharedfilebuf, MAXREGPATHLEN );
    }

    err = VR_UninstallDeleteSharedFilesKey(regPackageName);
    err = VR_UninstallDestroy(regPackageName);
    return err;
}


int32 SU_EnumUninstall(void** context, char* userPackageName,
                                    int32 len1, char*regPackageName, int32 len2)
{
    REGERR status = REGERR_FAIL;
    int err;
    su_UninstallContext* pcontext = (su_UninstallContext*)*context;
       
    if ( *context == NULL )
    {
        *context = XP_ALLOC( sizeof (su_UninstallContext));
        if (*context == NULL)
	    {
		    err = MK_OUT_OF_MEMORY;
		    return err;
	    }

        pcontext = (su_UninstallContext*)*context;
        pcontext->state = 0;
        pcontext->bShared = FALSE;
    }

    if ( !pcontext->bShared )
    {
        status = VR_EnumUninstall(&pcontext->state, 
                                  userPackageName, len1, 
                                  regPackageName, len2, pcontext->bShared); 
        
        if (status != REGERR_OK)
        {
            pcontext->bShared = TRUE;
            pcontext->state = 0;
            status = REGERR_OK;
        }
    } 

    if ( pcontext->bShared )
    {
        status = VR_EnumUninstall(&pcontext->state, 
                                  userPackageName, len1, 
                                  regPackageName, len2, pcontext->bShared); 

        if ( status != REGERR_OK )
        {
            XP_FREE( pcontext );
            *context = NULL;
        }
    }

    return status;
}

PUBLIC int SU_Startup()
{
    HREG reg;
    REGERR  err = REGERR_OK;
    char    *curPath = NULL;
	
    curPath = FE_GetDirectoryPath(eCommunicatorFolder);
    if (!curPath) {
        return REGERR_MEMORY;
    }

    VR_SetRegDirectory(curPath);
    XP_FREE(curPath);

    NR_StartupRegistry();   /* startup the registry; if already started, this will essentially be a noop */
    SU_InitMonitor();

	/* The profile manager will set the name of the current user once one is selected */
	
    /* check to see that we have a valid registry */
    if (REGERR_OK == NR_RegOpen("", &reg))
    {
	    XP_StatStruct stat;
 		XP_Bool removeFromList;
	    RKEY key;
	    REGENUM state;

        /* perform scheduled file deletions and replacements (PC only) */
        if (REGERR_OK ==  NR_RegGetKey(reg, ROOTKEY_PRIVATE,
            REG_DELETE_LIST_KEY,&key))
        {
            char *urlFile;
            char *pFile;
            char buf[MAXREGNAMELEN];

            state = 0;
            while (REGERR_OK == NR_RegEnumEntries(reg, key, &state,
                buf, sizeof(buf), NULL ))
            {
                urlFile = XP_PlatformFileToURL(buf);
                if ( urlFile == NULL)
                    continue;
                pFile = urlFile+7;

                removeFromList = FALSE;
                if (0 == XP_FileRemove(pFile, xpURL)) {
                    /* file was successfully deleted */
                    removeFromList = TRUE;
                }
                else if (XP_Stat(pFile, &stat, xpURL) != 0) {
                    /* file doesn't appear to exist */
                    removeFromList = TRUE;
                }

                if (removeFromList) {
                    err = NR_RegDeleteEntry( reg, key, buf );
                    /* must reset state or enum will stop on deleted entry */
                    if ( err == REGERR_OK )
                        state = 0;
                }

                XP_FREEIF(urlFile);
            }
            /* delete list node if empty */
			state = 0;
            if (REGERR_NOMORE == NR_RegEnumEntries( reg, key, &state, buf, 
                sizeof(buf), NULL ))
            {
                NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_DELETE_LIST_KEY);
            }
        }

        /* replace files if any listed */
        if (REGERR_OK ==  NR_RegGetKey(reg, ROOTKEY_PRIVATE,
            REG_REPLACE_LIST_KEY, &key))
        {
            char tmpfile[MAXREGNAMELEN];
            char target[MAXREGNAMELEN];

            state = 0;
            while (REGERR_OK == NR_RegEnumEntries(reg, key, &state,
                tmpfile, sizeof(tmpfile), NULL ))
            {
                removeFromList = FALSE;
                if (XP_Stat(tmpfile, &stat, xpURL) != 0) 
                {
                    /* new file is gone! */
                    removeFromList = TRUE;
                }
                else if ( REGERR_OK != NR_RegGetEntryString( reg, key, 
                    tmpfile, target, sizeof(target) ) )
                {
                    /* can't read target filename, corruption? */
                    removeFromList = TRUE;
                }
                else {
                    if (XP_Stat(target, &stat, xpURL) == 0) {
                        /* need to delete old file first */
                        XP_FileRemove( target, xpURL );
                    }
                    if (0 == XP_FileRename(tmpfile, xpURL, target, xpURL)) {
                        removeFromList = TRUE;
                    }
                }

                if (removeFromList) {
                    err = NR_RegDeleteEntry( reg, key, tmpfile );
                    /* must reset state or enum will stop on deleted entry */
                    if ( err == REGERR_OK )
                        state = 0;
                }
            }
            /* delete list node if empty */
            state = 0;
            if (REGERR_NOMORE == NR_RegEnumEntries(reg, key, &state, tmpfile, 
                sizeof(tmpfile), NULL )) 
            {
                NR_RegDeleteKey(reg, ROOTKEY_PRIVATE, REG_REPLACE_LIST_KEY);
            }
        }
        NR_RegClose(reg);
    }


    return err;
}

int SU_Shutdown()
{
    if (su_RegPackTime())
    {
        su_PackRegistry();
    }
    SU_DestroyMonitor();
    NR_ShutdownRegistry();

    return 0;
}

int su_GetLastRegPackTime(int32 *lastRegPackTime)
{
    HREG reg;
    REGERR  err = REGERR_OK;
    RKEY key;
    char pathbuf[MAXREGPATHLEN+1] = {0};
    char lrptstr[MAXREGNAMELEN];

    *lastRegPackTime = 0;
    err = NR_RegOpen("", &reg);
    if (REGERR_OK == err)
    {
        err = NR_RegGetKey( reg, ROOTKEY_PRIVATE,  REG_SOFTUPDT_DIR, &key);

        if (err == REGERR_OK)
        {
            err = NR_RegGetEntryString( reg, key, LAST_REGPACK_TIME, lrptstr, sizeof(lrptstr) );
	        if (err == REGERR_OK)
            {
                *lastRegPackTime = XP_ATOI( lrptstr );
            }
        }
        NR_RegClose(reg);
    }
    return err;
}

int su_SetLastRegPackTime(int32 lastRegPackTime)
{
    HREG reg;
    REGERR  err = REGERR_OK;
    RKEY key;
    char pathbuf[MAXREGPATHLEN+1] = {0};
    char lrptstr[MAXREGNAMELEN];

    err = NR_RegOpen("", &reg);
    if (REGERR_OK == err)
    {
        XP_STRCPY(pathbuf, REG_SOFTUPDT_DIR);
        err = NR_RegAddKey( reg, ROOTKEY_PRIVATE,  pathbuf, &key);
        if (err == REGERR_OK)
        {
            *lrptstr = '\0';
	       
	        PR_snprintf(lrptstr, MAXREGNAMELEN, "%d", lastRegPackTime);
	        if ( lrptstr != NULL && *lrptstr != '\0' ) {
                /* Add "LastRegPackTime" */
	            err = NR_RegSetEntryString( reg, key, LAST_REGPACK_TIME, lrptstr);  
            }
                
        }
        NR_RegClose(reg);
    }
    return err;
}

XP_Bool su_RegPackTime()
{
    int32 lastRegPackTime = 0;
    int32 nowSecInt = 0;
    int32 intervalDays = 0;
    int32 intervalSec = 0;
    int32 i;
    int64 bigNumber;
    REGERR err;

    int64 now;
    int64 nowSec;

    LL_I2L(now, 0);
    LL_I2L(nowSec, 0);
    LL_I2L(bigNumber, 1000000);
    
    PREF_GetIntPref("autoupdate.regpack_interval", &intervalDays);
    intervalSec = intervalDays*24*60*60;
    err = su_GetLastRegPackTime(&lastRegPackTime);
  
    #ifdef NSPR20
		now = PR_Now();
	#else
		now = PR_LocalTime();
	#endif
   
    LL_DIV(nowSec, now, bigNumber);
    LL_L2I(nowSecInt, nowSec);

    /* We should not pack the Netscape Client Registry, the first
    time we use Nova, instead we should set the current time in the
    registry, so that the packing can happen at the intervalDays
    set in the preferences */
    if (lastRegPackTime == 0)
    {
        su_SetLastRegPackTime(nowSecInt);
        return FALSE;
    }
    i = nowSecInt - lastRegPackTime;
    if ((i > intervalSec) || (i < 0))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void su_RegPackCallback(void *userData, int32 bytes, int32 totalBytes)
{
    int32 value;
    pw_ptr prg = (pw_ptr)userData;
    char valueStr[256];
    static XP_Bool rangeSet = FALSE;
    
    if (totalBytes)
    {
        if (!rangeSet)
        {
            PW_SetProgressRange(prg, 0, totalBytes);
            PW_SetProgressValue(prg, 0);
            /* Need to do this to force the progressMeter to work */
            value = (int32) (5 * (((double)totalBytes) /((double)100)));
            PW_SetProgressValue(prg, value);
            rangeSet = TRUE;
        }
             
        PR_snprintf(valueStr, 256, XP_GetString(REGPACK_PROGRESS_LINE2), bytes, totalBytes);
        PW_SetProgressValue(prg, bytes);
        PW_SetLine2(prg, valueStr);
     }
}

int su_PackRegistry()
{
    int64   bigNumber;
    int32 nowSecInt = 0;
    REGERR  err = REGERR_OK;
    pw_ptr prg;
   
    int64 now;
    int64 nowSec;

    LL_I2L(now, 0);
    LL_I2L(nowSec, 0);
    LL_I2L(bigNumber, 1000000);
    
    prg = PW_Create(NULL, pwApplicationModal);
    PW_SetWindowTitle(prg, XP_GetString(REGPACK_PROGRESS_TITLE));
    PW_SetLine2(prg, NULL);
  	PW_Show(prg);
    PW_SetLine1(prg, XP_GetString(REGPACK_PROGRESS_LINE1));;
      
    err = VR_PackRegistry((void *)prg, su_RegPackCallback);
    /* We ignore all errors from PackRegistry. Later, we will check for an error
       which is is called if the cancel button of the progress dialog is clicked,
       in which case we will not set the lastRegPackTime.
    */
        #ifdef NSPR20
			now = PR_Now();
		#else
			now = PR_LocalTime();
		#endif
   
        LL_DIV(nowSec, now, bigNumber);
        LL_L2I(nowSecInt, nowSec);
        su_SetLastRegPackTime(nowSecInt);
    
    PW_Destroy(prg);
    
    return err;
}


#ifdef XP_MAC
#pragma export reset
#endif

