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
/* Created: Lou Montulli */
/* Modifications/Additions: Gagan Saksena */
/* Please leave outside of ifdef for windows precompiled headers. */

#ifdef MOZILLA_CLIENT

#include "xp.h"
#include "mkcache.h"
#include "glhist.h"
#include "xp_hash.h"
#include "xp_mcom.h"
#include "client.h"
#include "mkgeturl.h"
#include "mkstream.h"
#include "secnav.h"
#include "extcache.h"
#include "mkextcac.h"

/* For 197 java hookup */
#include "jri.h"

#ifdef PROFILE
#pragma profile on
#endif

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_DATABASE_CANT_BE_VALIDATED_MISSING_NAME_ENTRY;
extern int XP_DB_SELECTED_DB_NAMED;
extern int XP_REQUEST_EXTERNAL_CACHE;

PRIVATE XP_List * ext_cache_database_list = 0;
PRIVATE XP_Bool AtLeastOneOpenCache = FALSE;


/* ************************************ Defines ************************************** */
#define SAR_CACHE_INFO_STRING "CACHE_ExternalSARCacheNameString"

#define EXTCACHE	  1
#define SARCACHE	  2
#define SUCCESS		  0
#define FAILURE		 -1
#define MIN_MAX_SIZE  1000

/* ************************************ Proto's ************************************** */

/* Modified existing functions to call these (read Cut n Paste with passed in args) */
PRIVATE void					cache_ReadCacheInfo(XP_File fp, XP_List **list_ptr, int type);
PRIVATE void					cache_SaveCacheInfo(XP_File fp, XP_List *list_ptr, int type);
PRIVATE void					cache_SaveSARCacheInfo(void);

/* Newly written functions */
PRIVATE int						cache_PutSARCacheInfoIntoDB(ExtCacheDBInfo *db_info);
PRIVATE ExtCacheDBInfo *		cache_GetSARCacheInfoFromDB(ExtCacheDBInfo *db_info);
MODULE_PRIVATE DBT *			cache_ExtCacheDBInfoStructToDBData(ExtCacheDBInfo * old_obj);
MODULE_PRIVATE ExtCacheDBInfo * cache_DBDataToExtCacheDBInfoStruct(DBT * db_obj);
MODULE_PRIVATE void				cache_freeExtCacheDBInfoObj (ExtCacheDBInfo * cache_obj);

/* ************************************ Variables ************************************** */
PRIVATE XP_List * SAR_cache_database_list = 0;
PRIVATE XP_Bool SARCacheIndexOpen = FALSE;


/* close an external cache 
 */
PRIVATE void
net_CloseExtCacheFat(ExtCacheDBInfo *db_info)
{
	if(db_info->database)
	  {
		(*db_info->database->close)(db_info->database);
		db_info->database = 0;
	  }

}


PRIVATE char *
net_GetExtCacheNameFromDB(ExtCacheDBInfo *db_info)
{
	DBT key;
	DBT data;

	if(!db_info->database)
		return NULL;

	key.data = EXT_CACHE_NAME_STRING;
	key.size = XP_STRLEN(EXT_CACHE_NAME_STRING);

	if(0 == (*db_info->database->get)(db_info->database, &key, &data, 0))
	  {
		/* make sure it's a valid cstring */
		char *name = (char *)data.data;
		if(name[data.size-1] == '\0')
			return(XP_STRDUP(name));
		else
			return(NULL);
	  }

	return(NULL);

}

/* read/open an External Cache File allocation table.
 *
 * Returns a True if successfully opened
 * Returns False if not.
 */
PRIVATE XP_Bool
net_OpenExtCacheFat(MWContext *ctxt, ExtCacheDBInfo *db_info)
{
	char *slash;
	char *db_name;
	XP_Bool close_db=FALSE;

    if(!db_info->database)
      {
		/* @@@ add .db to the end of the files
		 */
		char* filename = WH_FilePlatformName(db_info->filename);
        db_info->database = dbopen(filename,
                             		O_RDONLY,
                             		0600,
                             		DB_HASH,
                             		0);
#ifdef XP_WIN
/* This is probably the last checkin into Akbar */
/* What really needs to be fixed is that Warren's implementation */
/* of WH_FilePlatformName needs to return a malloc'd string */
/* Right now, on Mac & X, it does not. See xp_file.c */
		XP_FREEIF(filename);
#endif
		if(!db_info->database)
			return(FALSE);
		else
			AtLeastOneOpenCache = TRUE;

		StrAllocCopy(db_info->path, db_info->filename);

		/* figure out the path to the database */
#if defined(XP_WIN) || defined(XP_OS2)                 /* IBM-SAH */
  		slash = XP_STRRCHR(db_info->path, '\\');						
#elif defined(XP_MAC)
  		slash = XP_STRRCHR(db_info->path, '/');						
#else
  		slash = XP_STRRCHR(db_info->path, '/');						
#endif

		if(slash)
		  {
		    *(slash+1) = '\0';
		  }
		else
		  {
			*db_info->path = '\0';
		  }

		db_name = net_GetExtCacheNameFromDB(db_info);

		if(!db_name)
		  {
			close_db = !FE_Confirm(ctxt,
			XP_GetString( XP_DATABASE_CANT_BE_VALIDATED_MISSING_NAME_ENTRY ) );
		  }
		else if(XP_STRCMP(db_name, db_info->name))
		  {
			char buffer[2048];
			
			PR_snprintf(buffer, sizeof(buffer),
							  XP_GetString( XP_DB_SELECTED_DB_NAMED ),
							  db_name, db_info->name);
			
			close_db = !FE_Confirm(ctxt, buffer);
		  }

		if(close_db)
		  {
			(*db_info->database->close)(db_info->database);
			db_info->database = 0;
			return(FALSE);
		  }
	  }

 	return(TRUE);
}


PRIVATE void
net_SaveExtCacheInfo(void)
{
	XP_File fp;
	XP_List *list_ptr;

	fp = XP_FileOpen("", xpExtCacheIndex, XP_FILE_WRITE);
	list_ptr = ext_cache_database_list;

	cache_SaveCacheInfo(fp, list_ptr, EXTCACHE);
}

#define BUF_SIZE 2048
PRIVATE void
net_ReadExtCacheInfo(void)
{
	XP_File fp;

	fp = XP_FileOpen("", xpExtCacheIndex, XP_FILE_READ);

	cache_ReadCacheInfo(fp, &ext_cache_database_list, EXTCACHE);
}

PRIVATE void
net_OpenExtCacheFATCallback(MWContext *ctxt, char * filename, void *closure)
{
	ExtCacheDBInfo *db_info = (ExtCacheDBInfo*)closure;

	if(!db_info || !filename)
		return;

	StrAllocCopy(db_info->filename, filename);

	/* once we have the name try and open the
	 * cache fat
	 */
	if(net_OpenExtCacheFat(ctxt, db_info))
	  {
		AtLeastOneOpenCache = TRUE;
	  }
	else
	  {
		if(FE_Confirm(ctxt, "Unable to open External Cache.  Try again?"))
		  {
			/* try and open again */

			FE_PromptForFileName (ctxt,					/* context */
                              db_info->name,			/* prompt string */
                              db_info->filename,	/* default file/path */
                              TRUE,					/* file must exist */
                              FALSE,				/* directories allowed */
							  net_OpenExtCacheFATCallback, /* callback */
							  (void *)db_info);     /* closure */

		
			return; /* don't save ExtCache info */
		  }
	  }

	net_SaveExtCacheInfo();
}

PUBLIC void
NET_OpenExtCacheFAT(MWContext *ctxt, char * cache_name, char * instructions)
{
	ExtCacheDBInfo *db_info=0, *db_ptr;
	XP_Bool done = FALSE;
	XP_List *list_ptr;

	if(!ext_cache_database_list)
	  {
		net_ReadExtCacheInfo();
		if(!ext_cache_database_list)
			return;
	  }

	/* look up the name in a list and open the file
	 * if it's not open already
	 */
	list_ptr = ext_cache_database_list;
	while((db_ptr = (ExtCacheDBInfo *)XP_ListNextObject(list_ptr)) != NULL)
	  {
		if(db_ptr->name && !XP_STRCMP(db_ptr->name, cache_name))
		  {
			db_info = db_ptr;
			break;
		  }
	  }

	if(!db_info)
	  {
		db_info = XP_NEW(ExtCacheDBInfo);
		if(!db_info)
			return;
		XP_MEMSET(db_info, 0, sizeof(ExtCacheDBInfo));
		StrAllocCopy(db_info->name, cache_name);

		XP_ListAddObject(ext_cache_database_list, db_info);
	  }
	else if(db_info->queried_this_session)
	  {
		/* don't query or try and open again.
		 */
		return;
	  }

	if(db_info->filename)
	  {
		done = net_OpenExtCacheFat(ctxt, db_info);

		if(done)
		  {
			char buffer[1024];

			PR_snprintf(buffer, sizeof(buffer), 
						"Now using external cache: %.900s", 
					   	db_info->name);
			if(!FE_Confirm(ctxt, buffer))
				net_CloseExtCacheFat(db_info);
		  }
	  }

	/* If we don't have the correct filename, query the
	 * user for the name
	 */
	if(!done)
	  {

		if(instructions)
			done = !FE_Confirm(ctxt, instructions);
		else
			done = !FE_Confirm(ctxt, XP_GetString( XP_REQUEST_EXTERNAL_CACHE ) );

		if(!done)
			FE_PromptForFileName (ctxt,					/* context */
                              db_info->name,			/* prompt string */
                              db_info->filename,	/* default file/path */
                              TRUE,					/* file must exist */
                              FALSE,				/* directories allowed */
							  net_OpenExtCacheFATCallback, /* callback */
							  (void *)db_info);     /* closure */
	  }

	db_info->queried_this_session = TRUE;

	return;
}

/* lookup routine
 *
 * builds a key and looks for it in 
 * the database.  Returns an access
 * method and sets a filename in the
 * URL struct if found
 */
MODULE_PRIVATE int
NET_FindURLInExtCache(URL_Struct * URL_s, MWContext *ctxt)
{
	return CACHE_FindURLInCache(URL_s, ctxt);
}

/* ****************************************************************************************************************************
 * Exposed Navigator Cache Calls
 * ****************************************************************************************************************************/

/*
 * allows java to enumerate through all of the managed cache
 */
PUBLIC XP_List *
CACHE_GetManagedCacheList()
{
	XP_File			fp;

	/* If the Cache Index is not open, we can not see if the cache is open now can we */
	if (!SARCacheIndexOpen)
	{
		fp = XP_FileOpen("", xpSARCacheIndex, XP_FILE_READ);

		/* check to see if the open succeeds */
		if (fp) 
		{
			cache_ReadCacheInfo(fp, &SAR_cache_database_list, SARCACHE);
			SARCacheIndexOpen = TRUE;
		}
		else
			return NULL;
	}

	return SAR_cache_database_list;
}


MODULE_PRIVATE void
CACHE_CloseAllOpenSARCache()
{
	ExtCacheDBInfo * db_info;
    XP_StatStruct    stat_entry;

	if (!SARCacheIndexOpen) return;

	if (!SAR_cache_database_list) return;

    while(NULL != (db_info = (ExtCacheDBInfo *)XP_ListNextObject(SAR_cache_database_list)))
    {
		CACHE_CloseCache(db_info);

		if ( db_info->logFile )
			XP_FileClose(db_info->logFile);

		/* stat the db file to see if it exists, if it doesn't remove it from */
		/* the archive.fat file to keep things consistent */
	    if(db_info->filename && (XP_Stat(db_info->filename, &stat_entry, xpSARCache) == -1) )
	    {
		    /* file does not exist!!
		     * remove the entry 
		     */
		     TRACEMSG(("Error! Cache db file missing: %s", db_info->filename));

			 if ( NULL != SAR_cache_database_list->prev )
			    SAR_cache_database_list = SAR_cache_database_list->prev;
			 
			 XP_ListRemoveObject(SAR_cache_database_list, db_info);
	     }
		cache_freeExtCacheDBInfoObj(db_info);
    }

    cache_SaveSARCacheInfo();

	SAR_cache_database_list = 0;
	SARCacheIndexOpen = FALSE;
}

MODULE_PRIVATE void
CACHE_OpenAllSARCache()
{
	XP_File			fp;
	XP_List			*tmpList;
	ExtCacheDBInfo	*db_info,
					*tmpDB_info;
	char			* filename,			/* The platform specific filename returned by WH_PlatformFileName */
					* tmpName;			/* The filename returned by WH_FileName */

	if (!SARCacheIndexOpen)
	{
		fp = XP_FileOpen("", xpSARCacheIndex, XP_FILE_READ);

		/* check to see if the open succeeds */
		if (fp)
			cache_ReadCacheInfo(fp, &SAR_cache_database_list, SARCACHE);
		else
			return;
	}

	SARCacheIndexOpen = TRUE;

	if (!SAR_cache_database_list) return;

	tmpList = SAR_cache_database_list;
    while(NULL != (db_info = (ExtCacheDBInfo *)XP_ListNextObject(tmpList)))
    {
		if ( !db_info->database )
		{
			tmpName = WH_FileName(db_info->filename, xpSARCache);
			filename = WH_FilePlatformName(tmpName);

			/* Open or create the db file they specified */
			db_info->database = dbopen(filename,
                           				O_RDWR | O_CREAT,
                           				0600,
                           				DB_HASH,
                           				0);

			/* check to see if the dbopen failed */
			if ( NULL == db_info->database )
			{
				CACHE_CloseCache(db_info);
				XP_FREEIF(db_info);
				db_info = NULL;
			}

			/* Do stuff to get the type, and name */
			tmpDB_info = cache_GetSARCacheInfoFromDB(db_info);

			if ( !tmpDB_info ) return;

			/* Since they can access this one, fill in the cache name value */
			db_info->DiskCacheSize	   = tmpDB_info->DiskCacheSize;
			db_info->NumberInDiskCache = tmpDB_info->NumberInDiskCache;
			db_info->MaxSize		   = tmpDB_info->MaxSize;
			db_info->name			   = XP_STRDUP(tmpDB_info->name);
			db_info->logFile		   = NULL;	

			cache_freeExtCacheDBInfoObj(tmpDB_info);
#ifdef XP_WIN
			XP_FREEIF(filename);
#endif
		}
	}	

}


/*
 * Closes the cache db
 */
PUBLIC int 
CACHE_CloseCache(ExtCacheDBInfo *db)
{
	/* If there is no Cache Struct return */
	if (!db) return 0;

	/* If there is no db return */
	if (!db->database) return 0;

    if(-1 == (*db->database->close)(db->database))
    {
        TRACEMSG(("Error closing cache database"));

		return 0;
    }

	db->database = 0;

	return SUCCESS;
}

/*
 * Closes the cache db
 */
PUBLIC void 
CACHE_UpdateCache(ExtCacheDBInfo *db)
{
	if ( db )
		cache_PutSARCacheInfoIntoDB(db);
}

	
/*
 * Returns a handle to the desired persitent cache,
 * It also adds this newly opened cache to the list
 * of opened caches
 */
PUBLIC ExtCacheDBInfo * 
CACHE_GetCache(ExtCacheDBInfo *db)
{
	XP_File			fp;
	char			* filename,			/* The platform specific filename returned by WH_PlatformFileName */
					* tmpName;			/* The filename returned by WH_FileName */

	ExtCacheDBInfo	*db_info,
					*tmpDB_info;
	XP_Bool			inIndex = FALSE,
					add2Index = FALSE;
	XP_List			*tmpList;
	uint32			maxSize;

	/* If there is no Cache Struct return */
	if (!db) return NULL;

/* **************************************************************************************************
	   This is a hack.  It is a temp fix for bug 69098.  The plan is to go with this for 4.0 and
       do the right thing in 4.1
************************************************************************************************** */
	if ( -1 == db->type ) {
		tmpName = WH_FileName(db->filename, xpSARCache);
#ifndef XP_MAC
		filename = WH_FilePlatformName(tmpName);
#else
		filename = tmpName;
#endif

		/* Open or create the db file they specified */
		db->database = dbopen(filename,
                           			O_RDWR | O_CREAT,
                           			0600,
                           			DB_HASH,
                           			0);
		return db;
	}
/* **************************************************************************************************
	   The above was a hack.
************************************************************************************************** */


	/* Set up the max size for the cache */
	if (db->MaxSize < 0) {
		maxSize = NET_GetDiskCacheSize();

		if (maxSize < MIN_MAX_SIZE)
			maxSize = MIN_MAX_SIZE;
	}
	else {
		maxSize = db->MaxSize;
	}

	/* Kludge to prevent NULL writes to the Cache Index */
	if ( db->path == NULL || *(db->path) == '\0' || *(db->path) == ' ')
	{
		db->path = XP_STRDUP("\\");
	}

	/* If the Cache Index is not open, we can not see if the cache is open now can we */
	if (!SARCacheIndexOpen)
	{
		fp = XP_FileOpen("", xpSARCacheIndex, XP_FILE_READ);

		/* check to see if the open succeeds */
		if (fp)
			cache_ReadCacheInfo(fp, &SAR_cache_database_list, SARCACHE);
		else
		{
			/* I guess the index isn't there, let's make a new one */
			SAR_cache_database_list = XP_ListNew();
			if(!SAR_cache_database_list)
				return NULL;

			/* set the db_info values here since they will not be retrieved later */
			db_info						  = (ExtCacheDBInfo *)XP_ALLOC(sizeof(ExtCacheDBInfo));
			db_info->name				  = XP_STRDUP(db->name);
			db_info->filename			  = XP_STRDUP(db->filename);
			db_info->path				  = XP_STRDUP(db->path);
			db_info->queried_this_session = FALSE;
			db_info->type				  = db->type;
			db_info->database			  = NULL;
			db_info->DiskCacheSize		  = 0;
			db_info->NumberInDiskCache	  = 0;
			db_info->MaxSize		      = maxSize;
			db_info->logFile			  = NULL;

			add2Index = TRUE;
		}
		SARCacheIndexOpen = TRUE;
	}

	if (!SAR_cache_database_list) return NULL;

	/* If I have to add it, I already know it's not there */
	if ( !add2Index )
	{
		tmpList = SAR_cache_database_list;
		while( NULL != (db_info = (ExtCacheDBInfo *)XP_ListNextObject(tmpList)))
		{
			if(!XP_STRCMP(db->filename, db_info->filename) ) /* && !XP_STRCMP(db->path, db_info->path) ) */
			{
				inIndex = TRUE;
				break;
			}
		}
	}

	if (!inIndex && !add2Index)
	{
		/* We know nothing about this cache so add it */
		db_info						  = (ExtCacheDBInfo *)XP_ALLOC(sizeof(ExtCacheDBInfo));
		db_info->name				  = XP_STRDUP(db->name);
		db_info->filename			  = XP_STRDUP(db->filename);
		db_info->path				  = XP_STRDUP(db->path);
		db_info->queried_this_session = FALSE;
		db_info->type				  = db->type;
		db_info->database			  = NULL;
		db_info->DiskCacheSize		  = 0;
		db_info->NumberInDiskCache	  = 0;
		db_info->MaxSize			  = maxSize;
		db_info->logFile			  = NULL;

		add2Index = TRUE;
	}
	else
	{
		/* if it is already open I am going to return the handle to it,
		   if it is not open I will return NULL since we have a name conflict */
		if ((db_info!= NULL) && (db_info->database))
		{
			/* Do the meager name check to see if they can access this cache */
			if ( XP_STRCMP(db->name, db_info->name) )
			{
				db_info = NULL;
			}

			return db_info;
		}
	}
		
	tmpName = WH_FileName(db->filename, xpSARCache);
#ifndef XP_MAC
	filename = WH_FilePlatformName(tmpName);
#else
	filename = tmpName;
#endif

    if (!db_info)
        return NULL ;

	/* Open or create the db file they specified */
	db_info->database = dbopen(filename,
                           		O_RDWR | O_CREAT,
                           		0600,
                           		DB_HASH,
                           		0);

	/* check to see if the dbopen failed */
	if ( NULL == db_info->database )
	{
		CACHE_CloseCache(db_info);
		XP_FREEIF(db_info);	
		
		db_info = NULL;

		return NULL;
	}

	/* Do stuff to get the type, and name */
	tmpDB_info = cache_GetSARCacheInfoFromDB(db_info);

	/* If tmpDB_info doesn't exist then this is a new cache */
	if ( !tmpDB_info )
	{
		/* Since they can access this one, fill in the cache name value before it is saved */
		db_info->name = XP_STRDUP(db->name);
		cache_PutSARCacheInfoIntoDB(db_info);
	}
	else 
	{
		/* Do the meager name check to see if they can access this cache */
		if ( XP_STRCMP(db->name, tmpDB_info->name) )
		{
			CACHE_CloseCache(db_info);
			XP_FREEIF(db_info);
			
			db_info = NULL;
		}
		else
		{
			/* Since they can access this one, fill in the cache name value */
			db_info->name			   = XP_STRDUP(db->name);
			db_info->DiskCacheSize	   = tmpDB_info->DiskCacheSize;
			db_info->NumberInDiskCache = tmpDB_info->NumberInDiskCache;
			db_info->MaxSize		   = maxSize;
			db_info->logFile		   = NULL;	
		}

		cache_freeExtCacheDBInfoObj(tmpDB_info);
	}	

	if ( add2Index ) 
	{
		/* Since this is not in the index, add it */
		XP_ListAddObject(SAR_cache_database_list, db_info);
		cache_SaveSARCacheInfo();
	}

	/* This might cause problems for FileName routins 
	XP_FREEIF(tmpName);
	*/
#ifdef XP_WIN
	XP_FREEIF(filename);
#endif

    if (db_info != NULL)
	    db_info->type  = db->type;

	/* free the struct they passed in, and have it point to the new 
	 * struct we will be using.
	 */
	XP_FREEIF(db);
	db = db_info;

	return db_info;
}

PUBLIC XP_Bool
CACHE_Put(char *filename, URL_Struct *url_s)
{
    net_CacheObject *cacheObject = XP_NEW(net_CacheObject);
	XP_MEMSET(cacheObject, 0, sizeof(net_CacheObject));

	cacheObject->last_modified = url_s->last_modified;
	cacheObject->content_length = url_s->content_length;
	StrAllocCopy(cacheObject->filename, filename);
	cacheObject->is_relative_path = TRUE;
	StrAllocCopy(cacheObject->address, url_s->address);
	StrAllocCopy(cacheObject->content_type, url_s->content_type);

    return NET_CacheStore(cacheObject, url_s, FALSE);
}

PUBLIC char *
CACHE_GetCachePath(char * name)
{
	char * tmpName, * filename;

	tmpName = WH_FileName(name, xpSARCache);
	filename = WH_FilePlatformName(tmpName);

	return filename;
}


PUBLIC ExtCacheDBInfo * 
CACHE_GetCacheStruct(char * path, char * filename, char * name)
{
	ExtCacheDBInfo * db_info;
	XP_List			*tmpList;

	tmpList = SAR_cache_database_list;
	while( NULL != (db_info = (ExtCacheDBInfo *)XP_ListNextObject(tmpList)))
	{
		if(!XP_STRCMP(path, db_info->path) && !XP_STRCMP(filename, db_info->filename) && !XP_STRCMP(name, db_info->name) )
		{
			return db_info;
		}
	}
	
	return NULL;
}

/*
 * New Cache version to allow for different files, and cache lists
 */
PRIVATE void
cache_SaveCacheInfo(XP_File fp, XP_List *list_ptr, int type)
{ 
	ExtCacheDBInfo *db_info;
	int32 len = 0;

	if(!list_ptr)
		return;

	if(!fp)
		return;

	len = XP_FileWrite("# Netscape External Cache Index" LINEBREAK
				 "# This is a generated file!  Do not edit." LINEBREAK
				 LINEBREAK,
				 -1, fp);
	if (len < 0)
	{
		XP_FileClose(fp);
		return;
	}

    /* file format is:
     *   Filename  <TAB> database_name
     */
     while((db_info = (ExtCacheDBInfo *)XP_ListNextObject(list_ptr)) != NULL)
     {
	
		if( !db_info->filename && ( (type == EXTCACHE && !db_info->name) || (type == SARCACHE && !db_info->path) ) )
			continue;

		len = XP_FileWrite(db_info->filename, -1, fp);
		if (len < 0)
		{
			XP_FileClose(fp);
			return;
		}
		XP_FileWrite("\t", -1, fp);

		if ( type == EXTCACHE )
			XP_FileWrite(db_info->name, -1, fp);
		else
		{
			if ( db_info->path == NULL || *(db_info->path) == '\0' || *(db_info->path) == ' ')
			{
				XP_FileWrite("\\", -1, fp);
			}
			else
			{
				XP_FileWrite(db_info->path, -1, fp);
			}
		}

		len = XP_FileWrite(LINEBREAK, -1, fp);
		if (len < 0)
		{
			XP_FileClose(fp);
			return;
		}
      }

	XP_FileClose(fp);
}

PRIVATE void 
cache_SaveSARCacheInfo(void)
{	
	XP_File fp;
	XP_List *list_ptr;

	fp = XP_FileOpen("", xpSARCacheIndex, XP_FILE_WRITE);
	list_ptr = SAR_cache_database_list;

	cache_SaveCacheInfo(fp, list_ptr, SARCACHE);
}

/*
 * New Cache version to allow for different files, and cache lists
 */
PRIVATE void
cache_ReadCacheInfo(XP_File fp, XP_List **list_ptr, int type)
{
	ExtCacheDBInfo *new_db_info;
	char buf[BUF_SIZE];
	char *name;

	if(!*list_ptr)
	  {
		*list_ptr = XP_ListNew();
		if(!*list_ptr)
			return;
	  }
	
	if(!fp)
		return;

	/* file format is:
	 *   Filename  <TAB> database_name
	 */
	while(XP_FileReadLine(buf, BUF_SIZE-1, fp))
	  {
		if (*buf == 0 || *buf == '#' || *buf == CR || *buf == LF)
		  continue;

		/* remove /r and /n from the end of the line */
		XP_StripLine(buf);

		name = XP_STRCHR(buf, '\t');

		if(!name)
			continue;

		*name++ = '\0';

		new_db_info = XP_NEW(ExtCacheDBInfo);
        if(!new_db_info)
            return;

        XP_MEMSET(new_db_info, 0, sizeof(ExtCacheDBInfo));

        StrAllocCopy(new_db_info->filename, buf);

		if ( type == EXTCACHE )
			StrAllocCopy(new_db_info->name, name);
		else
			StrAllocCopy(new_db_info->path, name);

        XP_ListAddObject(*list_ptr, new_db_info);
	  }
	
	XP_FileClose(fp);
}

/*
 * Delete the specified cache and all the files in it.
 */
PUBLIC int CACHE_EmptyCache(ExtCacheDBInfo *db)
{
	DBT data;
	DBT key;
	int ret;
	char *filename;

#ifdef DEBUG_francis
    printf("CACHE_EmptyCache()\n");
#endif

	ret = (*db->database->seq)(db->database, &key, &data, R_FIRST);

	if( 0 != ret )
	  {
#ifdef DEBUG_francis
    printf("CACHE_EmptyCache(): 0 != ret\n");
#endif

		return 0;
  }

	do
	  {
    	filename = net_GetFilenameInCacheDBT(&data);     
    	if(filename)                                      
          {                                                
#ifdef DEBUG_francis
    printf("CACHE_EmptyCache(): removing %s\n",filename);
#endif

            TRACEMSG(("Removing file: %s due to disk"       
                      " cache remove",filename));     
            XP_FileRemove(filename, xpSARCache);			/* *X* What will happen if I pass this */
            XP_FREE(filename);                                 /*     a relative path */
          }  
	  }
	while(0 == (*db->database->seq)(db->database, &key, &data, R_NEXT));

	db->DiskCacheSize	  = 0;
	db->NumberInDiskCache = 0;

#ifdef DEBUG_francis
    printf("CACHE_EmptyCache(): success\n");
#endif


	return SUCCESS;
}

/*
 * Delete the specified cache and all the files in it.
 */
PUBLIC int CACHE_RemoveCache(ExtCacheDBInfo *db)
{
	int ret;
	ExtCacheDBInfo *db_info;
	XP_List			*tmpList;

	/* See if the database is open */
	if (!db->database) return 0;

	CACHE_EmptyCache(db);
    CACHE_CloseCache(db);

	/* Find the database object in my internal list */
	tmpList = SAR_cache_database_list;
	while(NULL != (db_info = (ExtCacheDBInfo *)XP_ListNextObject(tmpList)))
	{
		if(!XP_STRCMP(db->path, db_info->path) && !XP_STRCMP(db->filename, db_info->filename)) break;
	}

	/* Remove the cache from SAR_cache_database_list */
	if ( XP_ListRemoveObject(SAR_cache_database_list, db_info) )
		ret = XP_FileRemove(db->filename, xpSARCache);

	/* This was released in the call to CACHE_CloseCache above */
	db_info->database = 0;

	cache_freeExtCacheDBInfoObj(db_info);

	cache_SaveSARCacheInfo();

    return ret;
}

MODULE_PRIVATE int
CACHE_FindURLInCache(URL_Struct *URL_s, MWContext *ctxt)
{
	DBT *key;
	DBT data;
	net_CacheObject *found_cache_obj;
	ExtCacheDBInfo *db_ptr;
	int status;
    XP_StatStruct    stat_entry;
	char *filename=0;
	XP_List *list_ptr;

	/* larubbio */
	XP_FileType fileType;

	TRACEMSG(("Checking for URL in external cache"));

	/* zero the last modified date so that we don't
	 * screw up the If-modified-since header by
     * having it in even when the document isn't
	 * cached.
	 */
	URL_s->last_modified = 0;

	if(!AtLeastOneOpenCache && !SARCacheIndexOpen)
	  {
		TRACEMSG(("No External Cache databases open"));
	 	return(0); 
	  }

	key = net_GenCacheDBKey(URL_s->address, 
							URL_s->post_data, 
							URL_s->post_data_size);

	status = 1;

	/* Search the external caches first */
	if (AtLeastOneOpenCache)
	{
		list_ptr = ext_cache_database_list;
		while((db_ptr = (ExtCacheDBInfo *)XP_ListNextObject(list_ptr)) != NULL)
		  {
			if(db_ptr->database)
			  {
				TRACEMSG(("Searching databse: %s", db_ptr->name));
				status = (*db_ptr->database->get)(db_ptr->database, key, &data, 0);
				if(status == 0)
				{
					fileType = xpExtCache;
					break;
				}
			  }
		  }
	}

	/* Search the SAR caches next */
	if (SARCacheIndexOpen && status != 0)
	{
		list_ptr = SAR_cache_database_list;
		while((db_ptr = (ExtCacheDBInfo *)XP_ListNextObject(list_ptr)) != NULL)
		{
			if(db_ptr->database)
			{
				TRACEMSG(("Searching databse: %s", db_ptr->name));
				status = (*db_ptr->database->get)(db_ptr->database, key, &data, 0);
				if(status == 0)
				{
                   time_t cur_time = time(NULL);

                   /*
                    * larubbio: If were here, then the file must have
                    * been found in an external (CD or archive) cache.
                    * In order to prevent unnecessary network refresh,
                    * override the network check if the file has
                    * not already expired out of the cache and the call
                    * isnt being made from Java (so its not the Netcaster
                    * crawler).
                    */
                    if( (NULL == URL_s->SARCache) &&
                        ( (0 == URL_s->expires) || (cur_time < URL_s->expires) ))
                           URL_s->use_local_copy = 1;

					fileType = xpSARCache;
					URL_s->SARCache = db_ptr;
					break;
				}
			}
		}
	}

	if(status != 0)
	{
		TRACEMSG(("Key not found in any database"));
		net_FreeCacheDBTdata(key);
	 	return(0); 
	}

	found_cache_obj = net_Fast_DBDataToCacheStruct(&data);

    TRACEMSG(("mkextcache: found URL in cache!"));

	if(!found_cache_obj)
	    return(0);

	/* copy in the cache file name
	 */
	if(db_ptr->path 
#if REAL_TIME  /* use this for real */
		&& found_cache_obj->is_relative_path
#endif
         )
	  {
		if ( XP_STRCMP(db_ptr->path, "\\" ) )
			StrAllocCopy(filename, db_ptr->path);

		StrAllocCat(filename, found_cache_obj->filename);
	  }
	else
	  {
		StrAllocCopy(filename, found_cache_obj->filename);
	  }

	/* make sure the file still exists
	 * Looks like the new cache is ok since xpExtCache tells XP_Stat to use it's default
	 * settings, which is what we want so I won't touch it.
	 */
	if(XP_Stat(filename, &stat_entry, fileType) == -1)
	  {
		/* file does not exist!!
		 * remove the entry 
		 */
		TRACEMSG(("Error! Cache file missing: %s", filename));

	    net_FreeCacheDBTdata(key);

		XP_FREE(filename);

		return(0);
			
	  }

    /*
     * if the last accessed date is before the startup date set the
     * expiration date really low so that the URL is forced to be rechecked
     * again.  We don't just not return the URL as being in the
     * cache because we want to use "If-modified-since"
	 *
     * This works correctly because mkhttp will zero the
	 * expires field.
	 *
	 * if it's not an http url then just delete the entry
	 * since we can't do an If-modified-since
     */

	/* since we can't set a last accessed time
	 * we can't do the once per session thing.
	 * Always assume EVERY TIME
	 *
	 *  if(found_cache_obj->last_accessed < NET_StartupTime
	 *  	  && NET_CacheUseMethod != CU_NEVER_CHECK)
	 */
    if(URL_s->use_local_copy || URL_s->history_num)
      {
        /* we already did an IMS get or it's coming out
		 * of the history.
         * Set the expires to 0 so that we can now use the
         * object
         */
         URL_s->expires = 0;
      }

    else if(NET_CacheUseMethod != CU_NEVER_CHECK)
	  {
		if(!strncasecomp(URL_s->address, "http", 4))
		  {
#ifdef MOZ_OFFLINE
			if ( NET_IsOffline() )
            {
                time_t cur_time = time(NULL);

                if ( (0 == URL_s->expires) || (cur_time < URL_s->expires) )
                    URL_s->use_local_copy = 1;
            }
#endif /* MOZ_OFFLINE */
		  }
		else
		  {
			/* object has expired and cant use IMS. Don't return it */

	    	net_FreeCacheDBTdata(key);
			XP_FREE(filename);

			return(0);
		  }
	  }
	else
	  {
		/* otherwise use the normal expires date for CU_NEVER_CHECK */
 		URL_s->expires = found_cache_obj->expires;
	  }

	/* won't need the key anymore */
	net_FreeCacheDBTdata(key);

	URL_s->cache_file = filename;

    /* copy the contents of the URL struct so that the content type
     * and other stuff gets recorded
     */
    StrAllocCopy(URL_s->content_type,     found_cache_obj->content_type);
    StrAllocCopy(URL_s->charset,          found_cache_obj->charset);
    StrAllocCopy(URL_s->content_encoding, found_cache_obj->content_encoding);
    URL_s->content_length = found_cache_obj->content_length;
    URL_s->real_content_length = found_cache_obj->real_content_length;
 	URL_s->last_modified  = found_cache_obj->last_modified;
 	URL_s->is_netsite     = found_cache_obj->is_netsite;

	/* copy security info */
	URL_s->security_on             = found_cache_obj->security_on;
    URL_s->sec_info = SECNAV_CopySSLSocketStatus(found_cache_obj->sec_info);

	TRACEMSG(("Cached copy is valid. returning method"));

	TRACEMSG(("Using Disk Copy"));

	URL_s->ext_cache_file = TRUE;

	return(FILE_CACHE_TYPE_URL);
}


 /* free the cache object
 */
MODULE_PRIVATE void cache_freeExtCacheDBInfoObj (ExtCacheDBInfo * cache_obj)
{

    XP_FREEIF(cache_obj->database);
    XP_FREEIF(cache_obj->filename);
    XP_FREEIF(cache_obj->path);
    XP_FREEIF(cache_obj->name);

    XP_FREE(cache_obj);
}


PRIVATE ExtCacheDBInfo *
cache_GetSARCacheInfoFromDB(ExtCacheDBInfo *db_info)
{
	DBT key;
	DBT data;

	if(!db_info->database)
		return NULL;

	key.data = SAR_CACHE_INFO_STRING;
	key.size = XP_STRLEN(SAR_CACHE_INFO_STRING);

	if(0 == (*db_info->database->get)(db_info->database, &key, &data, 0))
	{
		/* convert it to a struct */
		return cache_DBDataToExtCacheDBInfoStruct(&data);
	}

	return(NULL);
}

/* Flushes the DB's contents to disk */
PUBLIC void
CACHE_FlushCache(ExtCacheDBInfo *db_info)
{
	(*db_info->database->sync)(db_info->database, 0);
}

PUBLIC void
CACHE_SaveCacheInfoToDB(ExtCacheDBInfo *db_info)
{
	cache_PutSARCacheInfoIntoDB(db_info);
}

PRIVATE int
cache_PutSARCacheInfoIntoDB(ExtCacheDBInfo *db_info)
{
	DBT key;
	DBT * data;

	if(!db_info->database)
		return 0;

	key.data = SAR_CACHE_INFO_STRING;
	key.size = XP_STRLEN(SAR_CACHE_INFO_STRING);

	data = cache_ExtCacheDBInfoStructToDBData(db_info);

	if(0 == (*db_info->database->put)(db_info->database, &key, data, 0))
    {
		/* Flush the db to disk */
		(*db_info->database->sync)(db_info->database, 0);
		return(SUCCESS);
    }

	return 0;
}

/* takes a cache object and returns a malloc'd 
 * (void *) suitible for passing in as a database
 * data storage object
 */
MODULE_PRIVATE DBT *
cache_ExtCacheDBInfoStructToDBData(ExtCacheDBInfo * old_obj)
{
    int32 len;
	char *cur_ptr;
	void *new_obj;
	int32 total_size;
	DBT *rv;

	rv = XP_NEW(DBT);

	if(!rv)
		return(NULL);

	total_size = sizeof(ExtCacheDBInfo);

#define ADD_STRING_SIZE(string) \
total_size += old_obj->string ? XP_STRLEN(old_obj->string)+1 : 0

	total_size += sizeof(NULL);
	ADD_STRING_SIZE(filename);
	ADD_STRING_SIZE(path);
	ADD_STRING_SIZE(name);
	total_size += sizeof(XP_Bool);
	total_size += sizeof(uint32);
	total_size += sizeof(uint32);
	total_size += sizeof(uint32);
	total_size += sizeof(uint32);

#undef ADD_STRING_SIZE
	
	new_obj = XP_ALLOC(total_size * sizeof(char));

	if(!new_obj)
	  {
		XP_FREE(rv);
		return NULL;
	  }

	XP_MEMSET(new_obj, 0, total_size * sizeof(char));
	/* VERY VERY IMPORTANT.  Whenever the
	 * format of the record structure changes
 	 * you must verify that the byte positions	
	 * in extcache.h are updated
	 */

#define STUFF_STRING(string) 									\
{	 															\
  len = (old_obj->string ? XP_STRLEN(old_obj->string)+1 : 0);	\
  COPY_INT32((void *)cur_ptr, &len);							\
  cur_ptr = cur_ptr + sizeof(int32);							\
  if(len)														\
      XP_MEMCPY((void *)cur_ptr, old_obj->string, len);			\
  cur_ptr += len;												\
}

#define STUFF_NUMBER(number) 									\
{ 																\
  COPY_INT32((void *)cur_ptr, &old_obj->number);				\
  cur_ptr = cur_ptr + sizeof(int32);							\
}

#define STUFF_BOOL(bool_val)	 									\
{ 																	\
  if(old_obj->bool_val)												\
    ((char *)(cur_ptr))[0] = 1;										\
  else																\
    ((char *)(cur_ptr))[0] = 0;										\
  cur_ptr = cur_ptr + sizeof(char);									\
}

	cur_ptr = (char *)new_obj;

	/* put the total size of the struct into
	 * the first field so that we have
	 * a cross check against corruption
	 */
  	COPY_INT32((void *)cur_ptr, &total_size);
  	cur_ptr = cur_ptr + sizeof(int32);

	STUFF_STRING(filename);
	STUFF_STRING(path);
	STUFF_STRING(name);
	STUFF_BOOL(queried_this_session);
	STUFF_NUMBER(type);
	STUFF_NUMBER(DiskCacheSize);
	STUFF_NUMBER(NumberInDiskCache);
	STUFF_NUMBER(MaxSize);

#undef STUFF_STRING
#undef STUFF_NUMBER
#undef STUFF_BOOL

	rv->data = new_obj;
	rv->size = total_size;

	return(rv);
}

/* takes a database storage object and returns a malloc'd
 * cache data object.  The cache object needs all of
 * it's parts free'd.
 *
 * returns NULL on parse error 
 */
MODULE_PRIVATE ExtCacheDBInfo *
cache_DBDataToExtCacheDBInfoStruct(DBT * db_obj)
{
	ExtCacheDBInfo * rv = XP_NEW(ExtCacheDBInfo);
	char * cur_ptr;
	char * max_ptr;
	uint32 len;

	if(!rv)
		return NULL;

	XP_MEMSET(rv, 0, sizeof(ExtCacheDBInfo));

/* if any strings are larger than this then
 * there was a serious database error
 */
#define MAX_HUGE_STRING_SIZE 10000

#define RETRIEVE_STRING(string)					\
{												\
    if(cur_ptr > max_ptr)                       \
	  {					 						\
		cache_freeExtCacheDBInfoObj(rv);		\
        return(NULL);                           \
	  }											\
	COPY_INT32(&len, cur_ptr);					\
	cur_ptr += sizeof(int32);					\
	if(len)										\
	  {											\
		if(len > MAX_HUGE_STRING_SIZE)			\
	      {				 						\
		    cache_freeExtCacheDBInfoObj(rv);	\
			return(NULL);						\
	      }										\
	    rv->string = (char*)XP_ALLOC(len);		\
		if(!rv->string)							\
	      {				 						\
		    cache_freeExtCacheDBInfoObj(rv);	\
			return(NULL);						\
	      }										\
	    XP_MEMCPY(rv->string, cur_ptr, len);	\
	    cur_ptr += len;							\
	  }											\
}											

#define RETRIEVE_NUMBER(number)					\
{												\
    if(cur_ptr > max_ptr)                       \
        return(rv);                             \
	COPY_INT32(&rv->number, cur_ptr);			\
	cur_ptr += sizeof(int32);					\
}

#define RETRIEVE_BOOL(bool)				\
{										\
  if(cur_ptr > max_ptr)                 \
    return(rv);                         \
  if(((char *)(cur_ptr))[0])			\
	rv->bool = TRUE;					\
  else 									\
	rv->bool = FALSE;					\
  cur_ptr += sizeof(char);				\
}

	cur_ptr = (char *)db_obj->data;

	max_ptr = cur_ptr+db_obj->size;

	/* get the total size of the struct out of
	 * the first field to check it
	 */
	COPY_INT32(&len, cur_ptr);
	cur_ptr += sizeof(int32);

	if(len != db_obj->size)
	  {
		TRACEMSG(("Size going in is not the same as size coming out"));
		XP_FREE(rv);
		return(NULL);
	  }

	RETRIEVE_STRING(filename);
	RETRIEVE_STRING(path);
	RETRIEVE_STRING(name);
	RETRIEVE_BOOL(queried_this_session);
	RETRIEVE_NUMBER(type);
	RETRIEVE_NUMBER(DiskCacheSize);
	RETRIEVE_NUMBER(NumberInDiskCache);
	RETRIEVE_NUMBER(MaxSize);

	return(rv);
}

#endif /* MOZILLA_CLIENT */
