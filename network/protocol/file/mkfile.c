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
/*
 * state machine to handle local file and directory URL's
 * and to retreive file cache objects
 *
 * Designed and implemented by Lou Montulli '94
 */

#if defined(CookieManagement)
/* #define TRUST_LABELS 1 */
#endif

#include "xp.h"
#include "net_xp_file.h"
#include "plstr.h"
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "mkstream.h"
#include "mkformat.h"
#include "mkparse.h"
#include "mkfsort.h"
#include "mkfile.h"
#include "merrors.h"
#include "glhist.h"

#include "il_strm.h"            /* Image Lib stream converters. */
#include "il_types.h"
IL_EXTERN(int)
IL_Type(const char *buf, int32 len);

#if defined(XP_WIN) || defined(XP_OS2)
#include "errno.h"
#endif

#include "prefapi.h"

#ifdef TRUST_LABELS
extern void ProcessCookiesAndTrustLabels( ActiveEntry *ce );
#endif 

#if defined(SMOOTH_PROGRESS)
#include "progress.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>  /* for SEEK_SET on some platforms */
#endif

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_ALERT_OUTMEMORY;
extern int XP_PROGRESS_DIRDONE;
extern int XP_PROGRESS_FILEDONE;
extern int XP_PROGRESS_FILEZEROLENGTH;
extern int XP_PROGRESS_READDIR;
extern int XP_PROGRESS_READFILE;
extern int MK_OUT_OF_MEMORY;
extern int MK_UNABLE_TO_LOCATE_FILE;
extern int MK_UNABLE_TO_OPEN_FILE;
extern int MK_ZERO_LENGTH_FILE;
extern int MK_UNABLE_TO_DELETE_FILE;
extern int MK_UNABLE_TO_DELETE_DIRECTORY;
extern int MK_MKDIR_FILE_ALREADY_EXISTS;
extern int MK_COULD_NOT_CREATE_DIRECTORY;
extern int MK_ERROR_WRITING_FILE;
extern int MK_NOT_A_DIRECTORY;
extern int XP_READING_SEGMENT;
extern int XP_TITLE_DIRECTORY_LISTING;
extern int XP_H1_DIRECTORY_LISTING;
extern int XP_UPTO_HIGHER_LEVEL_DIRECTORY;
extern int XP_TITLE_DIRECTORY_OF_ETC;

/* states of the machine
 */
typedef enum {
	NET_CHECK_FILE_TYPE,
	NET_DELETE_FILE,
	NET_MOVE_FILE,
	NET_PUT_FILE,
	NET_MAKE_DIRECTORY,
	NET_FILE_SETUP_STREAM,
	NET_OPEN_FILE,
	NET_SETUP_FILE_STREAM,
	NET_OPEN_DIRECTORY,
	NET_READ_FILE_CHUNK,
	NET_READ_DIRECTORY_CHUNK,
	NET_BEGIN_PRINT_DIRECTORY,
	NET_PRINT_DIRECTORY,
	NET_FILE_DONE,
	NET_FILE_ERROR_DONE,
	NET_FILE_FREE
} net_FileStates;

/* forward decl */
PRIVATE int32 net_ProcessFile (ActiveEntry * cur_entry);

/* allow URL byterange requests for backwards compatibility,
 * but only leave this in for a short while
 */
#define URL_BYTERANGE_METHOD

#define DIR_STRUCT struct dirent

typedef struct _FILEConData {
    XP_File           file_ptr;
    PRDir            *dir_ptr;
    net_FileStates    next_state;
    NET_StreamClass * stream;
    char            * filename;
    Bool           is_dir;
    SortStruct      * sort_base;
    Bool           pause_for_read;
    Bool           is_cache_file;
	Bool		   calling_netlib_all_the_time;
    Bool destroy_graph_progress;  /* do we need to destroy graph progress? */
    int32   original_content_length; /* the content length at the time of
                                      * calling graph progress
                                      */
	char             * byterange_string; 
	int32              range_length;  /* the length of the current byte range */
} FILEConData;

#define CE_WINDOW_ID      cur_entry->window_id
#define CE_STATUS         cur_entry->status
#define CE_SOCK           cur_entry->socket
#define CE_BYTES_RECEIVED cur_entry->bytes_received
#define CE_FORMAT_OUT     cur_entry->format_out
#define CD_DESTROY_GRAPH_PROGRESS   con_data->destroy_graph_progress
#define CD_ORIGINAL_CONTENT_LENGTH  con_data->original_content_length
#define CD_BYTERANGE_STRING         con_data->byterange_string
#define CD_RANGE_LENGTH             con_data->range_length

#define PUTS(s)           (*con_data->stream->put_block) \
                                 (con_data->stream, s, PL_strlen(s))
#define IS_WRITE_READY    (*con_data->stream->is_write_ready) \
                                 (con_data->stream)
#define PUTB(b,l)         (*con_data->stream->put_block) \
                                (con_data->stream, b, l)
#define COMPLETE_STREAM   (*con_data->stream->complete) \
                                 (con_data->stream)
#define ABORT_STREAM(s)   (*con_data->stream->abort) \
                                 (con_data->stream, s)

/* try and open the URL path as a normal file
 * if it fails set the next state to try and open
 * a directory.
 */
PRIVATE int
net_check_file_type (ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *) cur_entry->con_data;
    XP_StatStruct stat_entry;

	TRACEMSG(("Looking for file: %s", con_data->filename));

	/* larubbio added xpSARCache check */
    if(-1 == NET_XP_Stat (con_data->filename, 
				&stat_entry, 
				con_data->is_cache_file ? 
					(cur_entry->URL_s->ext_cache_file ? 
					(cur_entry->URL_s->SARCache ? xpSARCache : xpExtCache) : xpCache) : xpURL))
	  {
		if(cur_entry->URL_s->method == URL_PUT_METHOD)
	  	  {
			con_data->next_state = NET_PUT_FILE;
			return(0);
	  	  }
		else if(cur_entry->URL_s->method == URL_MKDIR_METHOD)
	  	  {
			con_data->next_state = NET_MAKE_DIRECTORY;
			return(0);
	  	  }
		

		cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(
											MK_UNABLE_TO_LOCATE_FILE,
 											*con_data->filename ? con_data->filename : "/");
        return(MK_UNABLE_TO_LOCATE_FILE);
	  }

	if(!con_data->is_cache_file)
	  {
		cur_entry->URL_s->last_modified = stat_entry.st_mtime;
	  }

    if(S_ISDIR(stat_entry.st_mode))
	  {
        /* if the current address doesn't end with a
         * slash, add it now.
         */
        if(cur_entry->URL_s->address[PL_strlen(cur_entry->URL_s->address)-1] != '/')
            StrAllocCat(cur_entry->URL_s->address, "/");

		cur_entry->URL_s->is_directory = TRUE;
        con_data->is_dir = TRUE;
	  }
	else
	  {
        cur_entry->URL_s->content_length = (int32) stat_entry.st_size;
	  }

	if(con_data->is_cache_file)
	  {
		/* if we are looking for a cache file then
		 * we are always looking to do a GET of
		 * the file regardless of the method in
		 * the URL_s
		 */
   		con_data->next_state = NET_FILE_SETUP_STREAM;
	  }
	else switch(cur_entry->URL_s->method)
	  {
		case URL_PUT_METHOD:
			{
			  char * msg;
			  msg = PR_smprintf("Overwrite file %s?", con_data->filename);
			  if(FE_Confirm(CE_WINDOW_ID, msg))
				  con_data->next_state = NET_PUT_FILE;
			  else
				  con_data->next_state = NET_FILE_DONE;
			}
			break;

		case URL_MKDIR_METHOD:
			/* error, can't create a directory over
		 	 * an existing file or directory
		 	 */
			cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(
												MK_MKDIR_FILE_ALREADY_EXISTS,
												con_data->filename);
			return(MK_MKDIR_FILE_ALREADY_EXISTS);
	
		case URL_HEAD_METHOD:
			/* we just need the relavent file info.
			 * it is currently filled into the URL struct
			 */
    		con_data->next_state = NET_FILE_DONE;
			break;

		case URL_MOVE_METHOD:
			con_data->next_state = NET_MOVE_FILE;
			break;

		case URL_DELETE_METHOD:
			con_data->next_state = NET_DELETE_FILE;
			break;
	
		case URL_GET_METHOD:
    		con_data->next_state = NET_FILE_SETUP_STREAM;
			break;

        case URL_INDEX_METHOD:
            if(!cur_entry->URL_s->is_directory)
              {
                cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_NOT_A_DIRECTORY, con_data->filename);
                return(MK_NOT_A_DIRECTORY);
              }
            con_data->next_state = NET_FILE_SETUP_STREAM;
            break;

		default:
			PR_ASSERT(0);
    		con_data->next_state = NET_FILE_DONE;
			break;
	  }
			
    return(0);
}

PRIVATE int
net_delete_file (ActiveEntry * ce)
{
#ifndef LIVEWIRE
	PR_ASSERT(0);
	return(MK_OUT_OF_MEMORY);

#else
	FILEConData * cd = (FILEConData *) ce->con_data;

	TRACEMSG(("Deleteing file: %s", cd->filename));

	if(cd->is_dir)
	  {
        if(0 != XP_RemoveDirectory(cd->filename, xpURL))
          {
            /* error */
            ce->URL_s->error_msg = NET_ExplainErrorDetails(
                                                MK_UNABLE_TO_DELETE_DIRECTORY, 
												cd->filename);
            return(MK_UNABLE_TO_DELETE_DIRECTORY);
          }
	  }
	else
	  {
		if(0 != NET_XP_FileRemove(cd->filename, xpURL))
	  	  {
			/* error */
			ce->URL_s->error_msg = NET_ExplainErrorDetails(
													MK_UNABLE_TO_DELETE_FILE,
													cd->filename);
			return(MK_UNABLE_TO_DELETE_FILE);
	  	  }
	  }

	/* success */
	cd->next_state = NET_FILE_DONE;
	return(0);
#endif /* NOT LIVEWIRE */
}

PRIVATE int
net_move_file (ActiveEntry * ce)
{
#ifndef LIVEWIRE
	PR_ASSERT(0);
	return(MK_OUT_OF_MEMORY);

#else
	FILEConData * cd = (FILEConData *) ce->con_data;
	char *destination;

	PR_ASSERT(ce->URL_s->destination);
	if(!ce->URL_s->destination)
		return(MK_UNABLE_TO_LOCATE_FILE);

	destination = NET_ParseURL(ce->URL_s->destination, GET_PATH_PART);

	if(!destination)
		return(MK_OUT_OF_MEMORY);

	TRACEMSG(("Moving file: %s", cd->filename));

	if(0 != XP_FileRename(cd->filename, xpURL, destination, xpURL))
	  {
	  	/* error! */
		PR_ASSERT(0);
		/* @@@@@ */
		return(MK_UNABLE_TO_LOCATE_FILE);
	  }

	/* success */
	cd->next_state = NET_FILE_DONE;
	return(0);
#endif /* !LIVEWIRE */
}

PRIVATE int
net_put_file (ActiveEntry * ce)
{
#ifndef LIVEWIRE
	PR_ASSERT(0);
	return(MK_OUT_OF_MEMORY);

#else
    FILEConData * cd = (FILEConData *) ce->con_data;
	XP_File fp;
	int status;

	if(!(fp = NET_XP_FileOpen(cd->filename, xpURL, XP_FILE_WRITE_BIN)))
	  {
		/* error */
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_OPEN_FILE, 
													  cd->filename);
		return(MK_UNABLE_TO_OPEN_FILE);
	  }
		
	PR_ASSERT(ce->URL_s->post_data);
	PR_ASSERT(!ce->URL_s->post_data_is_file);

	status = NET_XP_FileWrite(ce->URL_s->post_data, ce->URL_s->post_data_size, fp);

	NET_XP_FileClose(fp);

	if(status < ce->URL_s->post_data_size)
	  {
		/* error */
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_FILE_WRITE_ERROR, 
													  cd->filename);
		return(MK_FILE_WRITE_ERROR);
	  }

	/* success */
	cd->next_state = NET_FILE_DONE;
	return(0);
#endif /* !LIVEWIRE */
}

PRIVATE int
net_make_directory (ActiveEntry * ce)
{
#ifndef LIVEWIRE
	PR_ASSERT(0);
	return(MK_OUT_OF_MEMORY);

#else
	FILEConData * cd = (FILEConData *) ce->con_data;

	if(0 != XP_MakeDirectory(cd->filename, xpURL))
	  {
		/* error */
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
												MK_COULD_NOT_CREATE_DIRECTORY,
                                                cd->filename);
        return(MK_COULD_NOT_CREATE_DIRECTORY);
	  }

	/* success */
	cd->next_state = NET_FILE_DONE;
	return (0);
#endif /* !LIVEWIRE */
}

/* setup the output stream
 */
PRIVATE int
net_file_setup_stream(ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *) cur_entry->con_data;

    if(con_data->is_dir)
      {        
	char * pUrl ;
	int status;
 
	status = PREF_CopyCharPref("protocol.mimefor.file",&pUrl);

	if (status >= 0 && pUrl) {
		StrAllocCopy(cur_entry->URL_s->content_type, pUrl);
		PR_Free(pUrl);
	} else {
 		StrAllocCopy(cur_entry->URL_s->content_type, APPLICATION_HTTP_INDEX);
	}

        con_data->stream = NET_StreamBuilder(CE_FORMAT_OUT, cur_entry->URL_s, CE_WINDOW_ID);

        if(!con_data->stream)
		  {
		    cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
            return(MK_UNABLE_TO_CONVERT);
		  }

#ifdef MOZILLA_CLIENT
		GH_UpdateGlobalHistory(cur_entry->URL_s);
#endif /* MOZILLA_CLIENT */

        con_data->next_state = NET_OPEN_DIRECTORY;
      }
	else
      {
	    /* 
	     * don't open the stream yet for files
	     */
        con_data->next_state = NET_OPEN_FILE;
      }

    return(0);  /* OK */
}

PRIVATE int
net_open_file (ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *) cur_entry->con_data;

    TRACEMSG(("MKFILE: Trying to open: %s\n",con_data->filename));

    if((con_data->file_ptr = NET_XP_FileOpen(con_data->filename, 
								   (con_data->is_cache_file ? 
									 (cur_entry->URL_s->ext_cache_file ? 
									   (cur_entry->URL_s->SARCache ? xpSARCache : xpExtCache) : xpCache) : xpURL),
									     XP_FILE_READ_BIN)) != NULL)
	  {
        con_data->next_state = NET_READ_FILE_CHUNK;
	  }
    else
	  {
		cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_OPEN_FILE, 
													  con_data->filename);

		/* if errno is equal to "Too many open files"
		 * return a special error
		 */
#ifdef EMFILE  /* Mac doesn't have EMFILE. */
		if(errno == EMFILE)
        	return(MK_TOO_MANY_OPEN_FILES);
		else
#endif
        	return(MK_UNABLE_TO_OPEN_FILE);
	  }

#if defined(SMOOTH_PROGRESS)
    PM_Status(cur_entry->window_id,
              cur_entry->URL_s,
              XP_GetString(XP_PROGRESS_READFILE));
#else
    if (!cur_entry->URL_s->load_background)
        NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_READFILE));
#endif

    /* CE_SOCK = XP_Fileno(con_data->file_ptr); */
	CE_SOCK = NULL;
    cur_entry->local_file = TRUE;
    NET_SetFileReadSelect(CE_WINDOW_ID, NET_XP_Fileno(con_data->file_ptr)); 

	/* @@@ now that we have determined that it is a local file
	 * set URL_s->cache_file so that the streams think
	 * that this file came from the cache
	 */
    StrAllocCopy(cur_entry->URL_s->cache_file, con_data->filename);

    con_data->pause_for_read = TRUE;

	con_data->next_state = NET_SETUP_FILE_STREAM;

#if defined(SMOOTH_PROGRESS)
    PM_Progress(cur_entry->window_id,
                cur_entry->URL_s,
                0,
                cur_entry->URL_s->content_length);
#else
    if (!cur_entry->URL_s->load_background) {
        FE_GraphProgressInit(CE_WINDOW_ID, cur_entry->URL_s, cur_entry->URL_s->content_length);
        CD_DESTROY_GRAPH_PROGRESS = TRUE;  /* we will need to destroy it */
    }
#endif
    CD_ORIGINAL_CONTENT_LENGTH = cur_entry->URL_s->content_length;

    return(0);  /* ok */
}


#ifdef MOZILLA_CLIENT
#ifdef OLD_MOZ_MAIL_NEWS
extern int net_InitializeNewsFeData (ActiveEntry * cur_entry);
extern int IMAP_InitializeImapFeData (ActiveEntry * cur_entry);
#endif /* OLD_MOZ_MAIL_NEWS */
#endif /* MOZILLA_CLIENT */

PRIVATE int
net_setup_file_stream (ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *) cur_entry->con_data;
	int32 count;

	/* set up the stream now
	 */
	TRACEMSG(("Trying to look at content type: %s",cur_entry->URL_s->content_type));
    if(!cur_entry->URL_s->content_type)
	  {
		int img_type;
		
	    /* read the first chunk of the file
	     */
        count = NET_XP_FileRead(NET_Socket_Buffer, NET_Socket_Buffer_Size, con_data->file_ptr);
    
        if(!count)
          {
            NET_ClearFileReadSelect(CE_WINDOW_ID, NET_XP_Fileno(con_data->file_ptr));
#if defined(SMOOTH_PROGRESS)
            PM_Status(cur_entry->window_id,
                      cur_entry->URL_s,
                      XP_GetString(XP_PROGRESS_FILEZEROLENGTH));
#else
            NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_FILEZEROLENGTH));
#endif
            NET_XP_FileClose(con_data->file_ptr);
			CE_SOCK = NULL;
            con_data->file_ptr = 0;
            con_data->next_state = NET_FILE_ERROR_DONE;
		    cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_ZERO_LENGTH_FILE, 
												          con_data->filename);
            return(MK_ZERO_LENGTH_FILE);
          }
    
#ifdef MOZILLA_CLIENT
		img_type = IL_Type(NET_Socket_Buffer, count);
		if (img_type && img_type != IL_NOTFOUND)
		  {
			switch(img_type)
			  {
				case IL_GIF:
            		StrAllocCopy(cur_entry->URL_s->content_type, IMAGE_GIF);
					break;
				case IL_XBM:
            		StrAllocCopy(cur_entry->URL_s->content_type, IMAGE_XBM);
					break;
				case IL_JPEG:
            		StrAllocCopy(cur_entry->URL_s->content_type, IMAGE_JPG);
					break;
                		case IL_PNG:
            		StrAllocCopy(cur_entry->URL_s->content_type, IMAGE_PNG);
					break;

				case IL_PPM:
            		StrAllocCopy(cur_entry->URL_s->content_type, IMAGE_PPM);
					break;
				default:
					assert(0);  /* should NEVER get this!! */
					break;
			  }
					
		  }
		else
#endif /* MOZILLA_CLIENT */
		{
#if defined(XP_MAC)
		  	char *	macType, *macEncoding;
		  	Bool	useDefault;

			FE_FileType(cur_entry->URL_s->address, &useDefault, &macType, &macEncoding);
			if (useDefault)
			{
#endif
				NET_cinfo * type = NET_cinfo_find_type(cur_entry->URL_s->address);
				NET_cinfo * enc = NET_cinfo_find_enc(cur_entry->URL_s->address);

#if defined(XP_MAC) 
				if (macType)
					PR_Free(macType);
#endif				

				StrAllocCopy(cur_entry->URL_s->content_type, type->type); 
				StrAllocCopy(cur_entry->URL_s->content_encoding, enc->encoding); 
				
	
				if(type->is_default && NET_ContainsHTML(NET_Socket_Buffer, count))
	            	StrAllocCopy(cur_entry->URL_s->content_type, TEXT_HTML);

#if defined(XP_MAC)
			}
			else
			{
				cur_entry->URL_s->content_type = macType;
				cur_entry->URL_s->content_encoding = macEncoding;
			}
#endif
		  }
	  }

	CD_RANGE_LENGTH = 0;
	CE_BYTES_RECEIVED = 0;

	/* check to see if we need to do a byterange here
     * If we are then set the ranges in the URL
     * so that the streams know the byte range.
	 * Once we parse one byterange copy
	 */
	if(CD_BYTERANGE_STRING && *CD_BYTERANGE_STRING)
	  {
		int32 low=0, high=0;
		char *cp, *dash;

		cp = PL_strchr(CD_BYTERANGE_STRING, ',');
		if(cp)
		    *cp = '\0';

		dash = PL_strchr(CD_BYTERANGE_STRING, '-');

		if(!dash)
		  {
			/* no dash is an invalid byte range
			 * return zero and we will come back
			 * in here and give the whole file or
			 * the next byterange
			 */
			return(0);
		  }
		else
		  {
			*dash = '\0';
			
			if(CD_BYTERANGE_STRING == dash)
			    low = -1;
			else
			    low = atol(CD_BYTERANGE_STRING);
			high = atol(dash+1);
		  }


		if(low < 0 && high < 1)
		  {
			/* if both low and high are non-positive
			 * it's an error
			 */
			return(0);
		  }
		else if(low < 0)
		  {
			/* This is a byte range that specifies
			 * the last bytes of a file.  For
			 * instance  http://host/dir/foo;byterange=-500
			 */
			low = cur_entry->URL_s->content_length-(atoi(CD_BYTERANGE_STRING+1));
			high = cur_entry->URL_s->content_length;
		  }
		else if(high < 1)
		  {
			/* This is a byte range that specifies
			 * the a byte range from a specified point 
			 * till the end of the file.  For
			 * instance  http://host/dir/foo;byterange=500-
			 */
			high = cur_entry->URL_s->content_length;
		  }

		if(low >= cur_entry->URL_s->content_length)
		  {
			/* error to have the low byte range be larger than
			 * the file
			 */
			return(0);
		  }
		else if(high >= cur_entry->URL_s->content_length)
		  {
			high = cur_entry->URL_s->content_length;
		  }

		cur_entry->URL_s->low_range = low;
		cur_entry->URL_s->high_range = high;

		CD_RANGE_LENGTH = (high-low)+1;

		if(cp)
		    memcpy(CD_BYTERANGE_STRING, cp+1, PL_strlen(cp+1)+1);
		else
			CD_BYTERANGE_STRING = 0;
	  }

	if (CLEAR_CACHE_BIT(CE_FORMAT_OUT) == FO_PRESENT)
	{
	  if (NET_URL_Type(cur_entry->URL_s->address) == NEWS_TYPE_URL)
	  {
#ifdef MOZILLA_CLIENT
#ifdef OLD_MOZ_MAIL_NEWS
		/* #### DISGUSTING KLUDGE to make cacheing work for news articles. */
		CE_STATUS = net_InitializeNewsFeData (cur_entry);
		if (CE_STATUS < 0)
		  {
			/* #### what error message? */
			return CE_STATUS;
		  }
#endif /* OLD_MOZ_MAIL_NEWS */        
#else
		PR_ASSERT(0);
#endif /* MOZILLA_CLIENT */
	  }
	  else if (!PL_strncmp(cur_entry->URL_s->address, "IMAP://", 7))
	  {
#ifdef MOZILLA_CLIENT
#ifdef OLD_MOZ_MAIL_NEWS
		/* #### DISGUSTING KLUDGE to make cacheing work for imap articles. */
		CE_STATUS = IMAP_InitializeImapFeData (cur_entry);
		if (CE_STATUS < 0)
		  {
			/* #### what error message? */
			return CE_STATUS;
		  }
#endif /* OLD_MOZ_MAIL_NEWS */        
#else
		PR_ASSERT(0);
#endif /* MOZILLA_CLIENT */
	  }
	}

    con_data->stream = NET_StreamBuilder(CE_FORMAT_OUT, cur_entry->URL_s, CE_WINDOW_ID);

    if(!con_data->stream)
	  {
		cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
        return(MK_UNABLE_TO_CONVERT);
	  }

#ifdef MOZILLA_CLIENT
	GH_UpdateGlobalHistory(cur_entry->URL_s);
#endif /* MOZILLA_CLIENT */

	/* seek back to the beginning so that
	 * we are not forced to write out this
	 * first block without calling is_write_ready
	 *
	 * this also seeks to the beginning of the range that we
	 * are going to send
 	 */
	NET_XP_FileSeek(con_data->file_ptr, cur_entry->URL_s->low_range, SEEK_SET);

	con_data->next_state = NET_READ_FILE_CHUNK;
    con_data->pause_for_read = TRUE;

    return(CE_STATUS);  /* ok */
}

PRIVATE int
net_open_directory (ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *) cur_entry->con_data;
#ifdef XP_WIN
    char *tmpDirName = NULL;
#endif

	TRACEMSG(("Trying to open directory: %s", con_data->filename));

#ifdef XP_WIN
    /* nspr doesn't like our preceeding slash. */
    tmpDirName = (char*) PR_Malloc(PL_strlen(con_data->filename));
    if (!tmpDirName)
        return MK_UNABLE_TO_OPEN_FILE;
    PL_strcpy(tmpDirName, con_data->filename + 1);

    if((con_data->dir_ptr = PR_OpenDir (tmpDirName)) != NULL)
#else
    if((con_data->dir_ptr = PR_OpenDir (con_data->filename)) != NULL)
#endif
	  {
        con_data->next_state = NET_READ_DIRECTORY_CHUNK;
	  }
    else
	  {
#ifdef XP_WIN
        PR_Free(tmpDirName);
#endif
		cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_OPEN_FILE,
													  con_data->filename);
        return(MK_UNABLE_TO_OPEN_FILE);
	  }
#ifdef XP_WIN
    PR_Free(tmpDirName);
#endif

    con_data->is_dir = TRUE;

#if defined(SMOOTH_PROGRESS)
    PM_Progress(cur_entry->window_id,
                cur_entry->URL_s,
                0,
                cur_entry->URL_s->content_length);

    PM_Status(cur_entry->window_id,
              cur_entry->URL_s,
              XP_GetString(XP_PROGRESS_READDIR));
#else
    FE_GraphProgressInit(CE_WINDOW_ID, cur_entry->URL_s, cur_entry->URL_s->content_length);
    CD_DESTROY_GRAPH_PROGRESS = TRUE;  /* we will need to destroy it */
    NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_READDIR));
#endif
    CD_ORIGINAL_CONTENT_LENGTH = cur_entry->URL_s->content_length;

    /* make sure the last character isn't a slash
     */
    if(con_data->filename[PL_strlen(con_data->filename)-1] == '/')
        con_data->filename[PL_strlen(con_data->filename)-1] = '\0';

    return(0);  /* ok */
}

/* read part of a file and push it out the stream
 */
PRIVATE int
net_read_file_chunk(ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *) cur_entry->con_data;
    int count;
	int32 read_size = IS_WRITE_READY;

    /* If the stream sink doesn't want any data yet, do nothing. */
    if (read_size == 0)
      {
        con_data->pause_for_read = TRUE;
        return NET_READ_FILE_CHUNK;
      }

	read_size = MIN(read_size, NET_Socket_Buffer_Size);

	if(CD_RANGE_LENGTH)
		read_size = MIN(read_size, CD_RANGE_LENGTH-CE_BYTES_RECEIVED);
	
    count = NET_XP_FileRead(NET_Socket_Buffer, read_size, con_data->file_ptr);

    if(count < 1 || (CD_RANGE_LENGTH && CE_BYTES_RECEIVED >= CD_RANGE_LENGTH))
      {
		if(cur_entry->URL_s->real_content_length > cur_entry->URL_s->content_length)
		  {
			/* this is a case of a partial cache file.
			 * we need to fall out of here and
			 * get the rest of the file from the
			 * network
			 */
			cur_entry->save_stream = con_data->stream;
			con_data->stream = NULL;
			NET_ClearFileReadSelect(CE_WINDOW_ID, NET_XP_Fileno(con_data->file_ptr));
#if defined(SMOOTH_PROGRESS)
            PM_Status(cur_entry->window_id,
                      cur_entry->URL_s,
                      XP_GetString(XP_PROGRESS_FILEDONE));
#else
            if (!cur_entry->URL_s->load_background)
                NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_FILEDONE));
#endif
        	NET_XP_FileClose(con_data->file_ptr);
			CE_SOCK = NULL;
        	con_data->file_ptr = 0;
        	con_data->next_state = NET_FILE_FREE;
        	return(MK_GET_REST_OF_PARTIAL_FILE_FROM_NETWORK);
		  }
		else if(CD_BYTERANGE_STRING && *CD_BYTERANGE_STRING)
		  {
			/* go get more byte ranges */
			COMPLETE_STREAM;
			con_data->next_state = NET_SETUP_FILE_STREAM;
#if defined(SMOOTH_PROGRESS)
            PM_Status(cur_entry->window_id,
                      cur_entry->URL_s,
                      XP_GetString(XP_READING_SEGMENT));
#else
            if (!cur_entry->URL_s->load_background)
                NET_Progress(CE_WINDOW_ID, XP_GetString( XP_READING_SEGMENT ) );
#endif
			return(0);
		  }

		NET_ClearFileReadSelect(CE_WINDOW_ID, NET_XP_Fileno(con_data->file_ptr));
#if defined(SMOOTH_PROGRESS)
        PM_Status(cur_entry->window_id,
                  cur_entry->URL_s,
                  XP_GetString(XP_PROGRESS_FILEDONE));
#else
        if (!cur_entry->URL_s->load_background)
            NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_FILEDONE));
#endif
        NET_XP_FileClose(con_data->file_ptr);
		CE_SOCK = NULL;
        con_data->file_ptr = 0;
        con_data->next_state = NET_FILE_DONE;
        return(MK_DATA_LOADED);
      }

    CE_BYTES_RECEIVED += count;

    CE_STATUS = PUTB(NET_Socket_Buffer, count);

#if defined(SMOOTH_PROGRESS)
    PM_Progress(cur_entry->window_id,
                cur_entry->URL_s,
                cur_entry->bytes_received,
                cur_entry->URL_s->content_length);
#else
    if (!cur_entry->URL_s->load_background)
        FE_GraphProgress(CE_WINDOW_ID, cur_entry->URL_s, CE_BYTES_RECEIVED, count,
                         cur_entry->URL_s->content_length);
#endif

    con_data->pause_for_read = TRUE;

    return(CE_STATUS);
}

PRIVATE int
net_read_directory_chunk (ActiveEntry * cur_entry)
{

    FILEConData * con_data = (FILEConData *) cur_entry->con_data;
    NET_FileEntryInfo * file_entry;
    PRDirEntry * dir_entry;
    PRFileInfo  stat_entry;
    char *full_path=0;
	int32 len;

    con_data->sort_base = NET_SortInit();

    while((dir_entry = PR_ReadDir(con_data->dir_ptr, PR_SKIP_BOTH)) != 0)
      {
        file_entry = NET_CreateFileEntryInfoStruct();
        if(!file_entry)
		  {
		    cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
            return(MK_OUT_OF_MEMORY);
		  }

		/* escape and copy
		 */
		file_entry->filename = NET_Escape(dir_entry->name, URL_PATH);

        /* make a full path */
		len = PL_strlen(con_data->filename) + PL_strlen(dir_entry->name) + 30;
		PR_FREEIF(full_path);
		full_path = (char *)PR_Malloc(len*sizeof(char));

		if(!full_path)
			return(MK_OUT_OF_MEMORY);

        PL_strcpy(full_path, con_data->filename + 1);
        PL_strcat(full_path, "/");
        PL_strcat(full_path, dir_entry->name);
        memset(&stat_entry, 0, sizeof(PRFileInfo));	/* paranoia */

        if(PR_GetFileInfo(full_path, &stat_entry) == PR_SUCCESS)
          {
            PRInt64 r, u;
            TRACEMSG(("Found stat info for file %s\n",full_path));

            if(PR_FILE_DIRECTORY == stat_entry.type)
			  {
#if 0 /* Hoping this is not an issue with PR routines :). */
#ifdef XP_MAC
				stat_entry.st_size = 0;		/* Mac stat gives spurious size data for folders */
#endif
#endif
                file_entry->special_type = NET_DIRECTORY;
				StrAllocCat(file_entry->filename, "/");
			  }
            else
			  {
                file_entry->cinfo = NET_cinfo_find_type(file_entry->filename);
			  }

            file_entry->size = (int32) stat_entry.size;

            /* Convert the PRTime (64-bit int) into a time_t (32 bit int) */
            LL_I2L(u, PR_USEC_PER_SEC);
            LL_DIV(r, stat_entry.modifyTime, u);
            LL_L2I(file_entry->date, r);

            if (file_entry->date < 0) {
                file_entry->date = 0;
            }

            TRACEMSG(("Got file: %s, %ld",file_entry->filename, file_entry->size));

            NET_SortAdd(con_data->sort_base, file_entry);
          }
      }

	PR_FREEIF(full_path);

#if defined(SMOOTH_PROGRESS)
    PM_Status(cur_entry->window_id,
              cur_entry->URL_s,
              XP_GetString(XP_PROGRESS_DIRDONE));
#else
    NET_Progress(CE_WINDOW_ID, XP_GetString(XP_PROGRESS_DIRDONE));
#endif

    PR_CloseDir(con_data->dir_ptr);
    con_data->dir_ptr = 0;

    con_data->next_state = NET_BEGIN_PRINT_DIRECTORY;

    return(0); 
}

/* emit application/http-index-format */
PRIVATE int
net_BeginPrintDirectory(ActiveEntry * ce)
{
    FILEConData * cd = (FILEConData *) ce->con_data;

	cd->next_state = NET_PRINT_DIRECTORY;

	return(ce->status);
}

/* if the url is a local file this function returns
 * the portion of the url that represents the
 * file path as a malloc'd string.  If the
 * url is not a local file url this function
 * returns NULL
 */
PRIVATE char *
net_return_local_file_part_from_url(char *address)
{
	char *host;
#ifdef XP_WIN
	char *new_address;
	char *rv;
#endif
	
	if (address == NULL)
		return NULL;

	/* imap urls are never local */
	if (!PL_strncasecmp(address,"IMAP://",7))
		return NULL;

	/* mailbox url's are always local, but don't always point to a file */
	if(!PL_strncasecmp(address, "mailbox:", 8))
	{
		char *filename = NET_ParseURL(address, GET_PATH_PART);
		if (!filename)
			filename = PL_strdup("");
		return filename;
	}

	if(PL_strncasecmp(address, "file:", 5))
		return(NULL);
	
	host = NET_ParseURL(address, GET_HOST_PART);

    if(!host || *host == '\0' || !PL_strcasecmp(host, "LOCALHOST"))
	  {
		PR_FREEIF(host);
		return(NET_UnEscape(NET_ParseURL(address, GET_PATH_PART)));
	  }

#ifdef XP_WIN
	/* get the address minus the search and hash data */
	new_address = NET_ParseURL(address, 
							GET_PROTOCOL_PART | GET_HOST_PART | GET_PATH_PART);
	NET_UnEscape(new_address);

	/* check for local drives as hostnames
	 */
	if(PL_strlen(host) == 2
		&& isalpha(host[0]) 
		&& (host[1] == '|' || host[1] == ':'))
	  {
		PR_Free(host);
		/* skip "file:/" */
		rv = PL_strdup(new_address+6);
		PR_Free(new_address);
		return(rv);
	  }

	if(1)
	  {
    	XP_StatStruct       stat_entry;
		/* try stating the url just in case since
		 * the local file system can
		 * have url's of the form \\prydain\dist
		 */
		if(-1 != NET_XP_Stat(address+5, &stat_entry, xpURL))
		  {
			PR_Free(host);
			/* skip "file:" */
			rv = PL_strdup(address+5);
			PR_Free(new_address);
			return(rv);
		  }
	  }
#endif /* XP_WIN */

	PR_Free(host);

	return(NULL);
}

/* load local files and
 * display directories
 *
 * if cache_file is not null then we this module is being used to
 * read a localy cached url.  Use the passed in cache_file variable
 * as the location of the file and use the URL->address as the
 * URL for StreamBuilding etc.
 */
PRIVATE int32
net_FileLoad (ActiveEntry * cur_entry)
{
	char * cp;
    FILEConData * con_data;

    NET_SetCallNetlibAllTheTime(CE_WINDOW_ID, "mkfile");

    /* make space for the connection data */
    cur_entry->con_data = PR_NEW(FILEConData);
    if(!cur_entry->con_data)
      {
        FE_Alert(CE_WINDOW_ID, XP_GetString(XP_ALERT_OUTMEMORY));
		cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
        CE_STATUS = MK_OUT_OF_MEMORY;
        return(CE_STATUS);
      }

    /* zero out the structure 
     */
    memset(cur_entry->con_data, 0, sizeof(FILEConData));

	/* set this to make the CD_ macros work */
	con_data = cur_entry->con_data;

    if(!cur_entry->URL_s->cache_file)
      {
		char *path;

		if(!(path = net_return_local_file_part_from_url(cur_entry->URL_s->address)))
      	  {
        	PR_Free(cur_entry->con_data);
        	cur_entry->con_data = 0;
        	return(MK_USE_FTP_INSTEAD); /* use ftp */
      	  }

        con_data->filename = path;
      }
    else
      {
        StrAllocCopy(con_data->filename, cur_entry->URL_s->cache_file);
		con_data->is_cache_file = TRUE;
      }

    TRACEMSG(("Load File: looking for file: %s\n",con_data->filename));

	/* don't cache local files or local directory listings 
	 * if we are only looking to cache this file fail now
     */
	if(cur_entry->format_out == FO_CACHE_ONLY)
		return -1;
	cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);

	/* all non-cache filenames must begin with slash
	 */
	if(!con_data->is_cache_file == TRUE && *con_data->filename != '/')
	  {
		cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(
											MK_UNABLE_TO_LOCATE_FILE,
 											*con_data->filename ? con_data->filename : "/");
		CE_STATUS = MK_UNABLE_TO_LOCATE_FILE;
        return(MK_UNABLE_TO_LOCATE_FILE);
	  }

#define BYTERANGE_TOKEN "bytes="

#ifdef URL_BYTERANGE_METHOD
	/* if the URL has ";bytes=" in it then it
	 * is a special partial byterange url.
	 * Parse out the byterange part and store it
     * away
	 */
#define URL_BYTERANGE_TOKEN ";"BYTERANGE_TOKEN
	if (con_data->is_cache_file && (cp = PL_strcasestr(cur_entry->URL_s->address, URL_BYTERANGE_TOKEN)) != NULL)
	  {
		StrAllocCopy(CD_BYTERANGE_STRING, cp+PL_strlen(URL_BYTERANGE_TOKEN));
		strtok(CD_BYTERANGE_STRING, ";");
	  }
	else if ((cp = PL_strcasestr(con_data->filename, URL_BYTERANGE_TOKEN)) != NULL)
	  {
		*cp = '\0';
		/* remove any other weird ; stuff */
		strtok(cp+1, ";");
		StrAllocCopy(CD_BYTERANGE_STRING, cp+PL_strlen(URL_BYTERANGE_TOKEN));
	  }
#endif /* URL_BYTERANGE_METHOD */

	/* this is the byterange header method. 
	 * both the URL byterange and the header
	 * byterange methods can coexist peacefully
	 */
	if(cur_entry->URL_s->range_header && !PL_strncmp(cur_entry->URL_s->range_header, BYTERANGE_TOKEN, PL_strlen(BYTERANGE_TOKEN)))
	  {
		StrAllocCopy(CD_BYTERANGE_STRING, cur_entry->URL_s->range_header+PL_strlen(BYTERANGE_TOKEN));
	  }

    /* lets do a local file read 
     */
    cur_entry->local_file = TRUE;  /* se we don't select on the socket id */

   	con_data->next_state = NET_CHECK_FILE_TYPE;

	return(net_ProcessFile(cur_entry));
}

PRIVATE int32
net_ProcessFile (ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *) cur_entry->con_data;

    TRACEMSG(("Entering ProcessFile with state #%d\n",con_data->next_state));

    con_data->pause_for_read = FALSE;

    while(!con_data->pause_for_read)
      {

        switch (con_data->next_state)
          {
            case NET_CHECK_FILE_TYPE:
                CE_STATUS = net_check_file_type(cur_entry);
                break;
    
			case NET_DELETE_FILE:
				CE_STATUS = net_delete_file(cur_entry);
				break;

			case NET_PUT_FILE:
				CE_STATUS = net_put_file(cur_entry);
				break;

			case NET_MOVE_FILE:
				CE_STATUS = net_move_file(cur_entry);
				break;

			case NET_MAKE_DIRECTORY:
				CE_STATUS = net_make_directory(cur_entry);
				break;

            case NET_FILE_SETUP_STREAM:
                CE_STATUS = net_file_setup_stream(cur_entry);
                break;
    
            case NET_OPEN_FILE:
                CE_STATUS = net_open_file(cur_entry);
                break;

			case NET_SETUP_FILE_STREAM:
				CE_STATUS = net_setup_file_stream(cur_entry);
				break;
    
            case NET_OPEN_DIRECTORY:
                CE_STATUS = net_open_directory(cur_entry);
                break;
    
            case NET_READ_FILE_CHUNK:
                CE_STATUS = net_read_file_chunk(cur_entry);
                break;
    
            case NET_READ_DIRECTORY_CHUNK:
                CE_STATUS = net_read_directory_chunk(cur_entry);
                break;

            case NET_BEGIN_PRINT_DIRECTORY:
				CE_STATUS = net_BeginPrintDirectory(cur_entry);
                break;
    
            case NET_PRINT_DIRECTORY:
				if((*con_data->stream->is_write_ready)(con_data->stream))
				{
					CE_STATUS = NET_PrintDirectory(&con_data->sort_base, con_data->stream, con_data->filename, cur_entry->URL_s);
					con_data->next_state = NET_FILE_DONE;
				}
				else
				{
					/* come back into this state */
                    if(!con_data->calling_netlib_all_the_time)
                    {
                        con_data->calling_netlib_all_the_time = TRUE;
					    NET_SetCallNetlibAllTheTime(CE_WINDOW_ID, "mkfile");
                    }
					cur_entry->memory_file = TRUE;
					cur_entry->socket = NULL;
					con_data->pause_for_read = TRUE;
				}
                break;
    
            case NET_FILE_DONE:
    			if(con_data->stream)
               		COMPLETE_STREAM;
#if defined(SMOOTH_PROGRESS)
                PM_StopBinding(cur_entry->window_id, cur_entry->URL_s, 0, NULL);
#endif
                con_data->next_state = NET_FILE_FREE;
#ifdef TRUST_LABELS
                ProcessCookiesAndTrustLabels( cur_entry );
#endif
                break;
    
            case NET_FILE_ERROR_DONE:
                if(con_data->stream)
                    ABORT_STREAM(CE_STATUS);
                if(con_data->dir_ptr)
                    PR_CloseDir(con_data->dir_ptr);
                if(con_data->file_ptr)
				  {
					CE_SOCK = NULL;
					NET_ClearFileReadSelect(CE_WINDOW_ID, NET_XP_Fileno(con_data->file_ptr));
                    NET_XP_FileClose(con_data->file_ptr);
				  }
#if defined(SMOOTH_PROGRESS)
                PM_StopBinding(cur_entry->window_id, cur_entry->URL_s, -1, NULL);
#endif
                con_data->next_state = NET_FILE_FREE;
                break;
    
            case NET_FILE_FREE:
				if(con_data->calling_netlib_all_the_time)
				{
				   NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mkfile");
                   con_data->calling_netlib_all_the_time = FALSE;
				}

#if !defined(SMOOTH_PROGRESS)
				if(CD_DESTROY_GRAPH_PROGRESS)
                    FE_GraphProgressDestroy(CE_WINDOW_ID,
                                            cur_entry->URL_s,
                                            CD_ORIGINAL_CONTENT_LENGTH,
											CE_BYTES_RECEIVED);
#endif
				PR_Free(con_data->filename);
				PR_FREEIF(con_data->stream);
				PR_Free(cur_entry->con_data);

    			NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mkfile");

                return(-1); /* done */
          }
    
        if(CE_STATUS < 0 && con_data->next_state != NET_FILE_FREE)
		  {
    		con_data->pause_for_read = FALSE;
            con_data->next_state = NET_FILE_ERROR_DONE;
		  }

      }

    return(0);  /* keep going */
}

PRIVATE int32
net_InterruptFile(ActiveEntry * cur_entry)
{
    FILEConData * con_data = (FILEConData *)cur_entry->con_data;

    con_data->next_state = NET_FILE_ERROR_DONE;
    CE_STATUS = MK_INTERRUPTED;

    return(net_ProcessFile(cur_entry));
}

PRIVATE void
net_CleanupFile(void)
{
    return;
}

typedef struct {
	HTTPIndexParserData *parse_data;
	MWContext 			*context;
	int		 			 format_out;
	URL_Struct			*URL_s;
} index_format_conv_data_object;

PRIVATE int net_IdxConvPut(NET_StreamClass *stream, char *s, int32 l)
{
	index_format_conv_data_object *obj=stream->data_object;	
	return(NET_HTTPIndexParserPut(obj->parse_data, s, l));
}

PRIVATE int net_IdxConvWriteReady(NET_StreamClass *stream)
{	
	return(MAX_WRITE_READY);
}

#define PD_PUTS(s)  \
do { \
if(status > -1) \
        status = (*stream->put_block)(stream, s, PL_strlen(s)); \
} while(0)

/* take the parsed data and generate HTML */
PRIVATE void net_IdxConvComplete(NET_StreamClass *inputStream)
{
	index_format_conv_data_object *obj=inputStream->data_object;
	int32 num_files = NET_HTTPIndexParserGetTotalFiles(obj->parse_data);
    NET_FileEntryInfo * file_entry;
	int32  max_name_length, i, status = 0, window_width, len;
	char   out_buf[3096], *name, *date, *ptr;
	NET_StreamClass *stream;
	NET_cinfo * cinfo;
	char *base_url=NULL, *path=NULL;

	/* direct the stream to the html parser */
	StrAllocCopy(obj->URL_s->content_type, TEXT_HTML);

	stream = NET_StreamBuilder(obj->format_out,
								obj->URL_s,
								obj->context);
	if(!stream)
		goto cleanup;

	/* Ask layout how wide the window is in the <PRE> font, to decide
	   how much space we should allocate for the file name column. */
# define LISTING_OVERHEAD 51
#ifdef MOZILLA_CLIENT
	window_width = LO_WindowWidthInFixedChars (obj->context);
#else
	window_width = 80;
#endif /* MOZILLA_CLIENT */

	max_name_length = (window_width - LISTING_OVERHEAD);
	if (max_name_length < 12) /* 8.3 */
		max_name_length = 12;
	else if (max_name_length > 50)
		max_name_length = 50;

	/* the the base url and path */
	if((base_url = NET_HTTPIndexParserGetBaseURL(obj->parse_data)) != NULL)
	{
		path = NET_ParseURL(base_url, GET_PATH_PART);

	    if (path && path != '\0')  /* Not Empty path: */
        {
        	int end;
        	NET_UnEscape(path);

        	end = PL_strlen(path)-1;
        	/* if the path ends with a slash kill it.
         	 * that includes the path "/"
         	 */
        	if(path[end] == '/')
          	{
             	path[end] = 0; /* kill the last slash */
          	}

			/* output "directory of ..." message */
			PR_snprintf(out_buf, sizeof(out_buf),
                        XP_GetString( XP_TITLE_DIRECTORY_OF_ETC ),
                        *path ? path : "/", *path ? path : "/");
			PD_PUTS(out_buf);
		}
	}

	if(NET_HTTPIndexParserGetHTMLMessage(obj->parse_data))
	  {
		PL_strcpy(out_buf, CRLF"<HR>"CRLF);
		PD_PUTS(out_buf);

		PD_PUTS(NET_HTTPIndexParserGetHTMLMessage(obj->parse_data));

		if(!NET_HTTPIndexParserGetTextMessage(obj->parse_data))
		{
			PL_strcpy(out_buf, CRLF"<HR>"CRLF);
			PD_PUTS(out_buf);
		}
	  }

    if(NET_HTTPIndexParserGetTextMessage(obj->parse_data))
	  {
		char *msg;

		PL_strcpy(out_buf, "<HR>");
		PD_PUTS(out_buf);

		msg = NET_HTTPIndexParserGetTextMessage(obj->parse_data);

        status = NET_ScanForURLs (NULL, msg,
                                  PL_strlen(msg),
                                  out_buf, sizeof(out_buf),
                                  TRUE);
		PD_PUTS(out_buf);

		PL_strcpy(out_buf, "<HR>");
		PD_PUTS(out_buf);
	  }

    /* allow the user to go up if path is not empty
    */
    if(path && *path != '\0')
    {
        char *cp = PL_strrchr(path, '/');
        PL_strcpy(out_buf, "<A HREF=\"");
        PD_PUTS(out_buf);
        if(cp)
        {
            *cp = '\0';
            PD_PUTS(path);
            *cp = '/';
            PL_strcpy(out_buf, "/");
			PD_PUTS(out_buf);
        }
        PL_strncpyz(out_buf, 
                    XP_GetString(XP_UPTO_HIGHER_LEVEL_DIRECTORY), 
                    sizeof(out_buf));
		PD_PUTS(out_buf);
	}

    for(i=0; i < num_files; i++)
      {

		file_entry = NET_HTTPIndexParserGetFileNum(obj->parse_data, i);

		if(!file_entry)
			continue;

        if(!file_entry->date)
          {
            PR_snprintf(out_buf, sizeof(out_buf), " <A HREF=\"%s\">",
                       file_entry->filename);
            PD_PUTS(out_buf); /* output the line */

            PR_snprintf(out_buf, sizeof(out_buf), "%s</A>\n",
                       NET_UnEscape(file_entry->filename));
            PD_PUTS(out_buf); /* output the line */
          }
		else
		  {
			int max_size_for_this_name;

            date = ctime(&file_entry->date);
            if(date)
                date[24] = '\0';  /* strip return */
            else
                date = "";
    
            PR_snprintf(out_buf, sizeof(out_buf), " <A HREF=\"%s\">", 
					   file_entry->filename);
       		PD_PUTS(out_buf); /* output the line */
    
			/* do content_type icon stuff here */
			cinfo = NET_cinfo_find_info_by_type(file_entry->content_type);

            if(file_entry->special_type == NET_DIRECTORY 
		    		|| file_entry->special_type == NET_SYM_LINK
		    		|| file_entry->special_type == NET_SYM_LINK_TO_DIR
		    		|| file_entry->special_type == NET_SYM_LINK_TO_FILE)
              {
				PR_snprintf(out_buf, 
							sizeof(out_buf), 
							"<IMG ALIGN=absbottom "
							"BORDER=0 SRC=\"resource:/res/network/gopher-menu.gif\">");
			  }
            else if(cinfo && cinfo->icon)
              {
                PR_snprintf(out_buf, sizeof(out_buf),
						   "<IMG ALIGN=absbottom BORDER=0 SRC=\"%.512s\">",
						   cinfo->icon);
              }
            else 
              {
                PR_snprintf(out_buf, 
							sizeof(out_buf), 
							"<IMG ALIGN=absbottom BORDER=0 "
						   	"SRC=\"resource:/res/network/gopher-unknown.gif\">");
		      }

       		PD_PUTS(out_buf); /* output the line */
    
#define SIZE_LISTING_OVERHEAD 9

			/* print the filename
			 */
			name = NET_UnEscape (file_entry->filename);
			len = PL_strlen (name);
			max_size_for_this_name = max_name_length;
			if(!file_entry->size)
				max_size_for_this_name += SIZE_LISTING_OVERHEAD;

			if(len > max_size_for_this_name)
			  {
				  PL_strcpy (out_buf, " ");
				  memcpy (out_buf + 1, name, max_name_length - 3);
				  PL_strcpy (out_buf + 1 + max_name_length - 3, "...</A>");
				}
			  else
				{
				  PL_strcpy (out_buf, " ");
				  PL_strcat (out_buf, name);
				  PL_strcat (out_buf, "</A>");
				}
       		PD_PUTS(out_buf); /* output the line */
			
			/* put a standard number of spaces between the name and date
			 */
			for(ptr = out_buf; len < max_size_for_this_name; len++)
			  {
				*ptr++ = ' ';
			  }
			*ptr = '\0';

			/* start from the end of out_buf
			 *
			 * note: add 4 to the date to get rid of the day of the week
			 */
            if(file_entry->size
#ifdef XP_MAC
				|| file_entry->special_type == NET_FILE_TYPE	/* show size for zero-length files */
#endif            
            )
              {
                 PR_snprintf(&out_buf[PL_strlen(out_buf)], 
						sizeof(out_buf) - PL_strlen(out_buf), 
						" %5ld %s %s ",
						file_entry->size > 1024 ?
							file_entry->size/1024 : file_entry->size,
						file_entry->size > 1024 ? "Kb" : " b",
                        date+4);
			  }
			else
			  {
				PR_snprintf(&out_buf[PL_strlen(out_buf)],
						   sizeof(out_buf) - PL_strlen(out_buf), 
						   " %s ", date+4);
			  }

			PD_PUTS(out_buf); /* output the line */
    
            if(file_entry->special_type == NET_SYM_LINK
            	|| file_entry->special_type == NET_SYM_LINK_TO_DIR
            	|| file_entry->special_type == NET_SYM_LINK_TO_FILE)
		      {
                PR_snprintf(out_buf, sizeof(out_buf), "Symbolic link\n");
		      }
            else if(file_entry->special_type == NET_DIRECTORY)
		      {
                PR_snprintf(out_buf, sizeof(out_buf), "Directory\n");
		      }
            else if(cinfo && cinfo->desc)
		      {
                PR_snprintf(out_buf, 
							sizeof(out_buf)-1, 
							"%s", 
							cinfo->desc);
                PL_strcat(out_buf, "\n");
		      }
		    else
		      {
                PL_strcpy(out_buf, "\n");
		      }
		    	
		    PD_PUTS(out_buf);
		  }
      }

    PL_strcpy(out_buf, "\n</PRE>");
    PD_PUTS(out_buf);

cleanup:
	NET_HTTPIndexParserFree(obj->parse_data);
	PR_FREEIF(path);
	(stream->complete)(stream);
}

PRIVATE void net_IdxConvAbort(NET_StreamClass *stream, int32 status)
{
	index_format_conv_data_object *obj=stream->data_object;	
	NET_HTTPIndexParserFree(obj->parse_data);
}

PUBLIC NET_StreamClass *
NET_HTTPIndexFormatToHTMLConverter(int         format_out,
                               void       *data_object,
                               URL_Struct *URL_s,
                               MWContext  *window_id)
{
	index_format_conv_data_object* obj;
    NET_StreamClass* stream;

    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

	stream = PR_NEW(NET_StreamClass);
    if(stream == NULL)
        return(NULL);

    obj = PR_NEW(index_format_conv_data_object);
    if (obj == NULL)
	  {
		PR_Free(stream);
        return(NULL);
	  }

    memset(obj, 0, sizeof(index_format_conv_data_object));

	obj->parse_data = NET_HTTPIndexParserInit();

	if(!obj->parse_data)
	  {
		PR_Free(stream);
		PR_Free(obj);
	  }

	obj->context = window_id;
	obj->URL_s = URL_s;
	obj->format_out = format_out;

    stream->name           = "HTTP index format converter";
    stream->complete       = (MKStreamCompleteFunc) net_IdxConvComplete;
    stream->abort          = (MKStreamAbortFunc) net_IdxConvAbort;
    stream->put_block      = (MKStreamWriteFunc) net_IdxConvPut;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_IdxConvWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

	return(stream);
}

#include "libmocha.h"

#define SANE_BUFLEN	1024

NET_StreamClass *
net_CloneWysiwygLocalFile(MWContext *window_id, URL_Struct *URL_s,
			  uint32 nbytes, const char * wysiwyg_url,
			  const char * base_href)
{
	char *filename;
	XP_File fromfp;
	NET_StreamClass *stream;
	int32 buflen, len;
	char *buf;

	filename = net_return_local_file_part_from_url(URL_s->address);
	if (!filename)
		return NULL;
	fromfp = NET_XP_FileOpen(filename, xpURL, XP_FILE_READ_BIN);
	PR_Free(filename);
	if (!fromfp)
		return NULL;
	stream = LM_WysiwygCacheConverter(window_id, URL_s, wysiwyg_url,
					  base_href);
	if (!stream)
	  {
		NET_XP_FileClose(fromfp);
		return 0;
	  }
	buflen = stream->is_write_ready(stream);
	if (buflen > SANE_BUFLEN)
		buflen = SANE_BUFLEN;
	buf = (char *)PR_Malloc(buflen * sizeof(char));
	if (!buf)
	  {
		NET_XP_FileClose(fromfp);
		return 0;
	  }
	while (nbytes != 0)
	  {
		len = buflen;
		if ((uint32)len > nbytes)
			len = (int32)nbytes;
		len = NET_XP_FileRead(buf, len, fromfp);
		if (len <= 0)
			break;
		if (stream->put_block(stream, buf, len) < 0)
			break;
		nbytes -= len;
	  }
	PR_Free(buf);
	NET_XP_FileClose(fromfp);
	if (nbytes != 0)
	  {
		/* NB: Our caller must clear top_state->mocha_write_stream. */
		stream->abort(stream, MK_UNABLE_TO_CONVERT);
		PR_Free(stream);
		return 0;
	  }
	return stream;
}

void
NET_InitFileProtocol(void)
{
    static NET_ProtoImpl file_proto_impl;

    file_proto_impl.init = net_FileLoad;
    file_proto_impl.process = net_ProcessFile;
    file_proto_impl.interrupt = net_InterruptFile;
    file_proto_impl.resume = NULL;
    file_proto_impl.cleanup = net_CleanupFile;

    NET_RegisterProtocolImplementation(&file_proto_impl, FILE_TYPE_URL);
    NET_RegisterProtocolImplementation(&file_proto_impl, FILE_CACHE_TYPE_URL);
}
