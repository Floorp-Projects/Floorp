/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* implements disk caching:  See mkmemcac.c for memory cache
 *
 * Designed and originally implemented by Lou Montulli '94
 * Modifications/additions by Gagan Saksena '97
 */

#ifdef MOZILLA_CLIENT

/* dbm code */

#include "mkcache.h"
#include "rosetta.h"
#include "extcache.h"
#include "mkgeturl.h"
#include "netutils.h"
#include "netcache.h"
#include "glhist.h"
#include "xp_hash.h"
#include "xp_mcom.h"
#include "client.h"
#include "mkstream.h"
#include HG65288
#include "mcom_db.h"
#include "prclist.h"
#include "prmem.h"
#include "plstr.h"
#include "net_xp_file.h"

#include "mkextcac.h"
#include "fileurl.h"
#include "mkmemcac.h"
#include "merrors.h"

#ifdef NU_CACHE
#include "CacheStubs.h"
#endif

#ifdef PROFILE
#pragma profile on
#endif

#ifdef XP_OS2_NOT_YET
/*serious hack to put cache cleanup on another thread...*/
extern int XP_LazyFileRemove(const char * name, XP_FileType type);
#else
#define XP_LazyFileRemove  NET_XP_FileRemove
#endif

/* For Mac */
#include <errno.h>

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_CACHE_CLEANUP;
extern int MK_CACHE_CONTENT_LENGTH;
extern int MK_CACHE_REAL_CONTENT_LENGTH;
extern int MK_CACHE_CONTENT_TYPE;
extern int MK_CACHE_LOCAL_FILENAME;
extern int MK_CACHE_LAST_MODIFIED;
extern int MK_CACHE_EXPIRES;
extern int MK_CACHE_LAST_ACCESSED;
extern int MK_CACHE_CHARSET;
HG73891
extern int MK_CACHE_USES_RELATIVE_PATH;
extern int MK_CACHE_FROM_NETSITE_SERVER;

#define INTERNAL_NAME_PREFIX       "INT_"
#define DISK_CACHE_SIZE_KEY_NAME   "INT_DiskCacheSize"
#define DISK_CACHE_NUMBER_KEY_NAME "INT_DiskCacheNumber"
#define DISK_CACHE_NAME			   "Netscape Internal Disk Cache"

#define CACHE_SYNC_RATE 30

typedef enum {
	NEW_CACHE_STORE,
	REFRESH_CACHE_STORE
} store_type_enum;

PRIVATE DBT       *net_DiskCacheSizeKey=0;
PRIVATE DBT		  *net_DiskCacheNumberKey=0;
PRIVATE DBT		  *net_DiskCacheNameKey=0;
PRIVATE uint32     net_DiskCacheSize=0;
PRIVATE uint32     net_NumberInDiskCache=0;
PRIVATE uint32     net_MaxDiskCacheSize=0;
PRIVATE uint32     net_cache_file_block_size=0;

/* Saves the cache struct to the DB */
extern void CACHE_SaveCacheInfoToDB(ExtCacheDBInfo *db_info);

#ifdef NO_MEMORY_CACHE
PRIVATE uint32     net_MaxMemoryCacheSize=0;
#endif /* NO_MEMORY_CACHE */

/* trace variable for cache testing */
MODULE_PRIVATE PRBool NET_CacheTraceOn = PR_FALSE;
HG76298
PRIVATE DB * cache_database = 0; 
/* PRIVATE XXX Mac CodeWarrior bug */ PRCList active_cache_data_objects
	= PR_INIT_STATIC_CLIST(&active_cache_data_objects);

typedef struct _CacheDataObject {
    PRCList          links;
    XP_File          fp;
    NET_StreamClass *next_stream;
    URL_Struct      *URL_s;
#ifdef NU_CACHE
    	void* 	cache_object;
#else
    net_CacheObject *cache_object;
#endif
} CacheDataObject;

PRIVATE void net_RemoveAllDiskCacheObjects(void);

HG87325

PUBLIC PRBool
NET_IsCacheTraceOn(void)
{
#ifdef NU_CACHE
    return CacheTrace_IsEnabled();
#else
    return NET_CacheTraceOn;
#endif
}

/* return the size of the file on
 * disk based on the block size
 */
PRIVATE
uint32
net_calc_real_file_size(uint32 size)
{
	if(net_cache_file_block_size == 0)
		return(size);
	else  /* round sizes up to the next block size */
		return(PR_ROUNDUP(size, net_cache_file_block_size));
}

PRIVATE void
net_GetDiskCacheSize(void)
{
    DBT data;

    if(!cache_database)
        return;

    if(!net_DiskCacheSizeKey)
      {
        net_DiskCacheSizeKey = PR_NEW(DBT);
		if(!net_DiskCacheSizeKey)
			return;

        net_DiskCacheSizeKey->data = 0;
        BlockAllocCopy(net_DiskCacheSizeKey->data, 
		       		   DISK_CACHE_SIZE_KEY_NAME, 
					   PL_strlen(DISK_CACHE_SIZE_KEY_NAME));
        net_DiskCacheSizeKey->size = PL_strlen(DISK_CACHE_SIZE_KEY_NAME);
      }

    if(0 == (*cache_database->get)(cache_database, 
								   net_DiskCacheSizeKey, 
								   &data, 
								   0))
	  {
		if(data.size == sizeof(uint32))
		  {
			COPY_INT32(&net_DiskCacheSize, data.data);
		  }
		else
		  {
			net_DiskCacheSize = 0;
		  }
	  }
	else
	  {
		net_DiskCacheSize = 0;
	  }

    if(!net_DiskCacheNumberKey)
      {
        net_DiskCacheNumberKey = PR_NEW(DBT);
		if(!net_DiskCacheNumberKey)
			return;

        net_DiskCacheNumberKey->data = 0;
        BlockAllocCopy(net_DiskCacheNumberKey->data, 
					   DISK_CACHE_NUMBER_KEY_NAME,
					   PL_strlen(DISK_CACHE_NUMBER_KEY_NAME));
        net_DiskCacheNumberKey->size = PL_strlen(DISK_CACHE_NUMBER_KEY_NAME);
      }

    if(0 == (*cache_database->get)(cache_database,
                                   net_DiskCacheNumberKey,
                                   &data,
                                   0))
      {
        if(data.size == sizeof(uint32))
		  {
            COPY_INT32(&net_NumberInDiskCache, data.data);
		  }
		else
		  {
			net_NumberInDiskCache = 0;
		  }
      }
	else
	  {
		net_NumberInDiskCache = 0;
	  }
	HG63453
}


PRIVATE int
net_OpenCacheFatDB(void)
{
	char* filename;
	static XP_Bool have_tried_open=FALSE;

    if(!cache_database)
      {
	HASHINFO hash_info = {
		4 * 1024,
		0,
		0,
#ifdef WIN16
		60 * 1024,
#else
		96 * 1024,
#endif
		0,
		0};
	    filename = WH_FileName("", xpCacheFAT);
		if (!filename) return 0;

		/* @@@ add .db to the end of the files
		 */
		cache_database = dbopen(filename,
								O_RDWR | O_CREAT,
								0600,
								DB_HASH,
								&hash_info);
		PR_Free(filename);

        if(!have_tried_open && !cache_database)
          {
    		XP_StatStruct stat_entry;

			have_tried_open = TRUE; /* only do this once */

            TRACEMSG(("Could not open cache database -- errno: %d", errno));

			/* if the file is zero length remove it
			 */
         	if(NET_XP_Stat("", &stat_entry, xpCacheFAT) != -1)
			  {
           		if(stat_entry.st_size <= 0)
				  {
					NET_XP_FileRemove("", xpCacheFAT);
				  }
                else
                  {
                    XP_File fp;
#define BUFF_SIZE 1024
                    char buffer[BUFF_SIZE];

                    /* open the file and look for
                     * the old magic cookie.  If it's
                     * there delete the file
                     */
                    fp = NET_XP_FileOpen("", xpCacheFAT, XP_FILE_READ);

                    if(fp)
                      {
                        NET_XP_FileReadLine(buffer, BUFF_SIZE, fp);

                        NET_XP_FileClose(fp);

                        if(PL_strstr(buffer, 
								     "Cache-file-allocation-table-format"))
                          NET_XP_FileRemove("", xpCacheFAT);

                      }
				  }
			  }


			/* try it again */
			filename = WH_FileName("", xpCacheFAT);
			if (filename) {
				cache_database = dbopen(filename,
										O_RDWR | O_CREAT,
										0600,
										DB_HASH,
										0);
				PR_Free(filename);
			}
			else 
				cache_database = NULL;
          }

		HG98471
		if(cache_database)
		  {
    		if(-1 == (*cache_database->sync)(cache_database, 0))
      		  {
        		TRACEMSG(("Error syncing cache database"));

        		/* close the database */
        		(*cache_database->close)(cache_database);
        		cache_database = 0;
      		  }

			net_GetDiskCacheSize();
		  }

#ifndef XP_PC
        /* if we don't know the standard block size yet
         * get it now
         */
        if(!net_cache_file_block_size)
		  {
            XP_StatStruct stat_entry;

            if(NET_XP_Stat("", &stat_entry, xpCacheFAT) != -1)
            	net_cache_file_block_size = stat_entry.st_blksize;
		  }
#endif
      }

	/* return non-zero if the cache_database pointer is
	 * non-zero
	 */
	return((int) cache_database);

}

#if defined(DEBUG) && defined(UNIX)
int
cache_test_me()
{

	net_CacheObject test;
	net_CacheObject *rv;
	int32 total_size;
	DBT *db_obj;

	memset(&test, 0, sizeof(net_CacheObject));
	StrAllocCopy(test.address, "test1");
	db_obj = net_CacheStructToDBData(&test);
	rv = net_DBDataToCacheStruct(db_obj);
	printf("test1: %s\n", rv->address);

	memset(&test, 0, sizeof(net_CacheObject));
	StrAllocCopy(test.address, "test2");
	StrAllocCopy(test.charset, "test2");
	db_obj = net_CacheStructToDBData(&test);
	rv = net_DBDataToCacheStruct(db_obj);
	printf("test2: %s	%s\n", rv->address, rv->charset);

	memset(&test, 0, sizeof(net_CacheObject));
	StrAllocCopy(test.address, "test3");
	StrAllocCopy(test.charset, "test3");
	test.content_length = 3 ;
	test.method = 3 ;
	test.is_netsite = 3 ;
	db_obj = net_CacheStructToDBData(&test);
	rv = net_DBDataToCacheStruct(db_obj);
	printf("test3: %s	%s	%d	%d	%s\n", 
			rv->address, rv->charset,
			rv->content_length, rv->method, 
			(rv->is_netsite == 3 ? "TRUE" : "FALSE"));

}
#endif

PRIVATE void
net_StoreDiskCacheSize(void)
{
	DBT data;
	uint32 tmp_num;

	if(!cache_database)
		return;
	
	if(!net_DiskCacheSizeKey)
	  {
		net_DiskCacheSizeKey = PR_NEW(DBT);
		if(!net_DiskCacheSizeKey)
			return;

		net_DiskCacheSizeKey->data = 0;
		BlockAllocCopy(net_DiskCacheSizeKey->data, 
					   DISK_CACHE_SIZE_KEY_NAME,
					   PL_strlen((char *)DISK_CACHE_SIZE_KEY_NAME));
		net_DiskCacheSizeKey->size = PL_strlen(DISK_CACHE_SIZE_KEY_NAME);
	  }

	if(!net_DiskCacheNumberKey)
	  {
		net_DiskCacheNumberKey = PR_NEW(DBT);
		if(!net_DiskCacheNumberKey)
			return;

		net_DiskCacheNumberKey->data = 0;
		BlockAllocCopy(net_DiskCacheNumberKey->data, 
					   DISK_CACHE_NUMBER_KEY_NAME,
					   PL_strlen((char *)DISK_CACHE_NUMBER_KEY_NAME));
		net_DiskCacheNumberKey->size = PL_strlen(DISK_CACHE_NUMBER_KEY_NAME);
	  }

	if(!net_DiskCacheNameKey)
	  {
		/* set the name of the cache so that
		 * if it is read as an external cache
		 * we will know what created it
		 */
		net_DiskCacheNameKey = PR_NEW(DBT);
		if(!net_DiskCacheNameKey)
			return;

		net_DiskCacheNameKey->data = 0;
		BlockAllocCopy(net_DiskCacheNameKey->data, 
					   EXT_CACHE_NAME_STRING,
					   PL_strlen((char *)EXT_CACHE_NAME_STRING));
		net_DiskCacheNameKey->size = PL_strlen(EXT_CACHE_NAME_STRING);
	  }

	data.size = sizeof(uint32);
	COPY_INT32(&tmp_num, &net_DiskCacheSize);
	data.data = &tmp_num;
	
	(*cache_database->put)(cache_database, net_DiskCacheSizeKey, &data, 0);

	data.size = sizeof(uint32);
	COPY_INT32(&tmp_num, &net_NumberInDiskCache);
	data.data = &tmp_num;
	
	(*cache_database->put)(cache_database, net_DiskCacheNumberKey, &data, 0);

	data.size = PL_strlen(DISK_CACHE_NAME)+1;
	data.data = DISK_CACHE_NAME;
	HG52897
	(*cache_database->put)(cache_database, net_DiskCacheNameKey, &data, 0);

}

/* returns TRUE if the object gets stored
 * FALSE if not
 */
PRIVATE XP_Bool
net_CacheStore(net_CacheObject * obj,  
			   URL_Struct      * URL_s, 
			   XP_Bool		         accept_partial_files,
			   store_type_enum 	 store_type)
#ifdef NU_CACHE
{
    /* Shouldn't be getting called */
    PR_ASSERT(0);
    if (!obj || !URL_s || !URL_s->address)
     return FALSE;

    if (URL_s->server_date 
     && URL_s->server_date < obj->last_modified)
    {
     /* the last modified date is after the current date
      * as given by the server.  We shoudn't cache this
      * object
      */
     obj->last_modified = 0;
     URL_s->last_modified = 0;

     /* URL_s is now updated from META tags etc. so re-read this information */
    }
    return TRUE;
}
#else
{
	DBT *data, *key;
	int status;
	static cache_sync_count = 0;
    XP_StatStruct stat_entry;

	/* larubbio */
	XP_Bool		SARCache = FALSE;
	XP_FileType fileType;
	DB			*local_cache_database = NULL;

	/* larubbio fix to allow 197 SAR cache adds without modifying 
	 * the navigator cache (too much)
	 */
	if ( URL_s != NULL && URL_s->SARCache != NULL )
	{
		SARCache = TRUE;
		fileType = xpSARCache;
		local_cache_database = URL_s->SARCache->database;
	}
	else
	{
		fileType = xpCache;
		local_cache_database = cache_database;
	}

	if(!local_cache_database)
	  {
		/* delete the file since we wont remember it
		 */
#ifdef XP_OS2
		XP_LazyFileRemove(obj->filename, fileType);
#else
		NET_XP_FileRemove(obj->filename, fileType);
#endif
		
		return FALSE;
	  }

	if(URL_s 
		&& URL_s->server_date 
		&& URL_s->server_date < obj->last_modified)
	  {
		/* the last modified date is after the current date
		 * as given by the server.  We shoudn't cache this
		 * object
		 */

		TRACEMSG(("Found file dated into the future.  "
				"Zeroing the last modified date"));
		obj->last_modified = 0;
		URL_s->last_modified = 0;
	  }

	if(store_type == NEW_CACHE_STORE)
	  {
	    if(URL_s && URL_s->dont_cache)  
  	      {
		    /* delete the file since we wont remember it
		     */
#ifdef XP_OS2
		    XP_LazyFileRemove(obj->filename, fileType);
#else
		    NET_XP_FileRemove(obj->filename, fileType);
#endif
		    return FALSE;
          }
        else if(NET_XP_Stat(obj->filename, &stat_entry,  fileType) != -1)
          {
		    if(stat_entry.st_size == 0)
		      {
			    /* don't cache any zero length files
		 	     */
#ifdef XP_OS2
				XP_LazyFileRemove(obj->filename, fileType);
#else
			    NET_XP_FileRemove(obj->filename, fileType);
#endif
			    return FALSE;
		      }

			if(obj->content_length <= 0)
			  {
		        /* fill this in for consistancy */
      		    obj->real_content_length = stat_entry.st_size;
      		    obj->content_length = stat_entry.st_size;
			  }
		    else if(accept_partial_files
			        && obj->content_length > (uint32) stat_entry.st_size)
		      {
      	        obj->real_content_length = obj->content_length;
			    obj->content_length = (uint32) stat_entry.st_size;
			    obj->incomplete_file = TRUE;
		      }
   	        else
	          {
	            /* stat the file to verify the content_length 
	             * if the content_length is positive and does not
	             * match the size of the cache file, handle it somehow
	             */
		        if(obj->content_length != (uint32) stat_entry.st_size)
 	              {
			        /* this is a case of a server or network
				     * error.  Assume that the file is corrupted,
				     * Display it as it was for history navigation
				     * but do a complete reload any time the server
				     * is contacted
				     * kill the last modified date to disable IMS 
				     */
      		        obj->last_modified = 0;
					if(URL_s)
						URL_s->last_modified = 0;
			        TRACEMSG(("\n\n!!!!!!!! Content length of cache file not equal"
					      "          To the content transfer length: %d - %d  !!!!!!!\n",
					      obj->content_length, stat_entry.st_size));
		  	      }
    
		        /* fill this in for consistancy */
      	    	
      	        obj->real_content_length 
							= obj->content_length 
										= stat_entry.st_size;
		      }

			/* must be set now */
			PR_ASSERT(obj->content_length);
			
	      }
	    else		
	      {
		    /* delete the file since we wont remember it
		     */
#ifdef XP_OS2
			XP_LazyFileRemove(obj->filename, fileType);
#else
		    NET_XP_FileRemove(obj->filename, fileType);
#endif
		    return FALSE;
	      }

	    /* refresh these entries in case meta changes them */
	    if(URL_s)
	      {
		    obj->expires = URL_s->expires;
		    obj->last_modified = URL_s->last_modified;
		    StrAllocCopy(obj->charset, URL_s->charset);
	      }
	  }
	else if(store_type == REFRESH_CACHE_STORE)
	  {
		/* OK */
	  }
	else
	  {
		PR_ASSERT(0);
	  }

	/* update the last accessed time
	 */
	obj->last_accessed = time(NULL);  /* current time */

	/* these must be true or else the partial cache logic is screwed */
	PR_ASSERT(obj->content_length == obj->real_content_length
	          || obj->incomplete_file);

	/* create key */
	key = net_GenCacheDBKey(obj->address, obj->post_data, obj->post_data_size);

	/* create new dbt data object */
	data = net_CacheStructToDBData(obj);
	HG73870
	if(!key || !data)
	  {
		TRACEMSG(("Failed to get key or data: malloc error probably"));
		/* delete the file since we wont remember it
		 */
#ifdef XP_OS2
		XP_LazyFileRemove(obj->filename, fileType);
#else
		NET_XP_FileRemove(obj->filename, fileType);
#endif
		return FALSE;
	  }

	if(store_type == NEW_CACHE_STORE)
		status = (*local_cache_database->put)(local_cache_database, 
										key, 
										data, 
										R_NOOVERWRITE);
	else /* overwrite by default */
		status = (*local_cache_database->put)(local_cache_database, key, data, 0);

    if(status > 0)
	  {
		DBT old_data;

		TRACEMSG(("Duplicate cache object found"));

		/* get the duplicate out of the database */
		status = (*local_cache_database->get)(local_cache_database, key, &old_data, 0);

		/* we should always get a success here 
		 * or at least a DB error.
		 */
		assert(status < 1);
		
		if(status == 0)
		  {
			/* found it */

		   	/* delete the previous object from the database
		     */
		   	/* remove the old file */
        	char * filename = net_GetFilenameInCacheDBT((DBT *)&old_data);
        	if(filename)
              {
#ifdef XP_OS2
				XP_LazyFileRemove(filename, fileType);
#else
            	NET_XP_FileRemove(filename, fileType);
#endif
            	PR_Free(filename);
           	  }

			/* larubbio file size checks for SARCache */
			if ( SARCache )
			{
				/* subtract the size of the file being removed */
        		URL_s->SARCache->DiskCacheSize -= net_calc_real_file_size(
												net_GetInt32InCacheDBT(&old_data,
													CONTENT_LENGTH_BYTE_POSITION));
				URL_s->SARCache->NumberInDiskCache--;
			}
			else
			{
				/* subtract the size of the file being removed */
        		net_DiskCacheSize -= net_calc_real_file_size(
												net_GetInt32InCacheDBT(&old_data,
													CONTENT_LENGTH_BYTE_POSITION));
				net_NumberInDiskCache--;
			}
    
		    /* put the new data in over the old data */
		    status = (*local_cache_database->put)(local_cache_database, key, data, 0);
   	      }
	  }


	if(status < 0)
	  {
        char * filename = net_GetFilenameInCacheDBT((DBT *)&data);

   		TRACEMSG(("cache update failed due to database error"));

		/* remove the old file */
        if(filename)
      	  {
#ifdef XP_OS2
			XP_LazyFileRemove(filename, fileType);
#else
           	NET_XP_FileRemove(filename, fileType);
#endif
           	PR_Free(filename);
      	  }

		if ( !SARCache ) {
			/* close the database */
			(*local_cache_database->close)(local_cache_database);

			cache_database = 0;
		}
		else
			CACHE_CloseCache(URL_s->SARCache);

		local_cache_database = 0;
	  }
 
    if (store_type == NEW_CACHE_STORE)
    {
	    if ( !SARCache )
	    {
		    /* add the size of the new file 
		     * size_on_disk can be in error, use content_length
		     * when missized
		     */
		    net_DiskCacheSize += net_calc_real_file_size(obj->content_length);
		    net_NumberInDiskCache++;

		    if (++cache_sync_count >= CACHE_SYNC_RATE) 
		    {
		    
			    /* sync the database everytime to guarentee
			     * consistancy
			     */
			    if(local_cache_database && -1 == (*local_cache_database->sync)(local_cache_database, 0))
			      {
				    TRACEMSG(("Error syncing cache database"));

				    /* close the database */
				    (*local_cache_database->close)(local_cache_database);
				    cache_database = 0;
				    local_cache_database = 0;
			      }

			    cache_sync_count = 0;

		    }
	    }
	    else
	    {
		    /* larubbio */
		    /* add the size of the new file 
		     * size_on_disk can be in error, use content_length
		     * when missized
		     */
		    URL_s->SARCache->DiskCacheSize += net_calc_real_file_size(obj->content_length);
		    URL_s->SARCache->NumberInDiskCache++;

	        if(URL_s->SARCache->DiskCacheSize > URL_s->SARCache->MaxSize && -1 != URL_s->SARCache->MaxSize )
		    {
			    TRACEMSG(("MaxSize exceeded!!!"));
			    /* delete the file since we wont remember it
			     */
			    NET_XP_FileRemove(obj->filename, fileType);

			    URL_s->SARCache->DiskCacheSize -= net_calc_real_file_size(obj->content_length);
			    URL_s->SARCache->NumberInDiskCache--;

			    return FALSE;
		    }

		    /* sync the database every time to guarentee
		     * consistancy
		     */
		    if(local_cache_database && -1 == (*local_cache_database->sync)(local_cache_database, 0))
		      {
			    TRACEMSG(("Error syncing cache database"));

			    /* close the database */
			    (*local_cache_database->close)(local_cache_database);
			    CACHE_CloseCache(URL_s->SARCache);
			    local_cache_database = 0;
		      }
		    else
		    {
			    /* This is inefficent to do each time, but it keeps us in synch, and this is for 
			       offline browsing, which is not a performance intensive task */
			    CACHE_SaveCacheInfoToDB(URL_s->SARCache);
		    }
	    }
    }

	net_FreeCacheDBTdata(key);
    net_FreeCacheDBTdata(data);

	return TRUE;

}
#endif /* NU_CACHE */

/* Public accesor function for Netcaster */
PUBLIC PRBool
NET_CacheStore(net_CacheObject * obj,  
			   URL_Struct      * URL_s, 
			   PRBool	         accept_partial_files)
{
	return net_CacheStore(obj, URL_s, accept_partial_files, NEW_CACHE_STORE);
}

/* Accessor functions for cache database used by cache browser */
PUBLIC int
NET_FirstCacheObject(DBT *key, DBT *data)
{
	return (*cache_database->seq)(cache_database, key, data, R_FIRST);
}

/* Accessor functions for cache database used by cache browser */
PUBLIC int
NET_NextCacheObject(DBT *key, DBT *data)
{
	return (*cache_database->seq)(cache_database, key, data, R_NEXT);
}

/* Accessor functions for cache database used by cache browser */
PUBLIC int32
NET_GetMaxDiskCacheSize()
{
	return net_MaxDiskCacheSize;
}

/****************************************************************
 * General cache routines needed by lots of stuff
 */

typedef struct _DatePlusDBT {
	DBT    * dbt;
	time_t   date;
} DatePlusDBT;

/* removes the specified number of objects from the
 * cache taking care to remove only the oldest objects.
 */
PUBLIC int32
NET_RemoveDiskCacheObjects(uint32 remove_num)
{
#define CORRUPT_ENTRY_ARRAY_SIZE 50
	uint32 corrupt_entry_count = 0;
	DatePlusDBT *old_array;
	DBT *corrupt_entry_array[CORRUPT_ENTRY_ARRAY_SIZE];
	uint32 total_cl=0;
	uint32 total_number=0;
	time_t date, lock_date;
    time_t last_modified_date = (time_t) 0;
	uint32 i,j;
	char *filename;
	char *url_address;
	DBT data;
	DBT key;

	HG74380
	/*
	 * This optimization is not valid for the case of URLs that normally
	 * would not be cached because their size exceeds the cache size, but
	 * will be cached anyway because the must_cache flag of the URL is set.
	 * For these URLs, the condition below would be true (since we're
	 * trying to remove as many files as possible to make room for the
	 * too-big file), which would cause all files to be removed but also
	 * the cache database to be closed and deleted.  With the database
	 * gone, net_CacheStore will fail, so even though the large file was
	 * saved correctly it won't be used.  (See bug #11327.)
	 */
#if 0
	if(remove_num >= net_NumberInDiskCache)
	  {
		net_RemoveAllDiskCacheObjects();
		return(net_DiskCacheSize);
	  }
#endif

	if(!cache_database)
		return(net_DiskCacheSize);

	old_array = (DatePlusDBT *)PR_Malloc(sizeof(DatePlusDBT) * remove_num);
	
	if(!old_array)
		return(net_DiskCacheSize);

	memset(old_array, 0, sizeof(DatePlusDBT) * remove_num);

    /* We need to delete the oldest items in the database.
	 * Sequentailly go throught the DB and remove
	 * the oldest items
	 */
	
	if(0 != (*cache_database->seq)(cache_database, &key, &data, R_FIRST))
		return(net_DiskCacheSize);
	total_cl += net_calc_real_file_size(net_GetInt32InCacheDBT(&data, 
												CONTENT_LENGTH_BYTE_POSITION));
	total_number++;

	/* Only add the first item to the old_array if the number to remove
	   is greater than zero, otherwise we're writing where we shouldn't be */
	if(remove_num > 0)
	{
		old_array[0].dbt  = net_CacheDBTDup(&key);
		old_array[0].date = net_GetTimeInCacheDBT(&data, 
												  LAST_ACCESSED_BYTE_POSITION);
	}

	while(0 == (*cache_database->seq)(cache_database, &key, &data, R_NEXT))
	  {
		date = net_GetTimeInCacheDBT(&data, LAST_ACCESSED_BYTE_POSITION);

		if(date)
			last_modified_date = net_GetTimeInCacheDBT(&data,
												 LAST_MODIFIED_BYTE_POSITION);

		/* there are other items beside FAT entries in here
		 * but they don't have a date
		 */
		if(!date)
		  {
			/* this could be a corrupt entry */
			if(!net_IsValidCacheDBT(&data))
			  {
			 	if((key.size < 4 || PL_strncmp((char*)key.data, 
												INTERNAL_NAME_PREFIX, 
												4))
					&& corrupt_entry_count < CORRUPT_ENTRY_ARRAY_SIZE)
				  {
	
					/* store invalid cache entries in an array
					 * for later deletion
					 */
					corrupt_entry_array[corrupt_entry_count++] = 
															net_CacheDBTDup(&key);
					TRACEMSG(("Found %d corrupted cache entries", 	
							  corrupt_entry_count));
				  }
				  
			  }
		  }
		else
		  {
			url_address = net_GetAddressFromCacheKey(&key);

			if(!last_modified_date 
			   || (url_address && PL_strncasecmp(url_address, "http",4)))
			  {
				/* this is not a http URL or it has no last modified date.
				 * Delete it preferentially
				 * if it's older than the current session
				 */
				if(NET_StartupTime > date)
				  {
					/* set the date really low so it automatically 
					 * gets deleted
					 */
					date = 1;
				  }
			  }

		    total_cl += net_calc_real_file_size(net_GetInt32InCacheDBT(&data, 
											   CONTENT_LENGTH_BYTE_POSITION));
		    total_number++;

			lock_date = net_GetTimeInCacheDBT(&data, LOCK_DATE_BYTE_POSITION);	

			/* honor locks issued during this session */
            TRACEMSG(("cache: comparing lock date %ld to startup time %ld\n",
                      lock_date, NET_StartupTime));
			if(lock_date > NET_StartupTime)
				continue;
			else if(lock_date)
				net_SetTimeInCacheDBT(&data, 
						  				LOCK_DATE_BYTE_POSITION, 
						  				0); /* current time */

		    for(i=0; i < remove_num; i++)
		      {
			    if(date < old_array[i].date || !old_array[i].date)
			      {
				    /* free the oldest one so it 
				     * doesn't get lost as it is pushed off
				     * the stack
				     */
				    if(old_array[remove_num-1].dbt)
					    net_FreeCacheDBTdata(old_array[remove_num-1].dbt);
    
				    /* shift all the entries down */
				    for(j=remove_num-1; j > i; j--)
				      {
					    old_array[j].dbt = old_array[j-1].dbt;
					    old_array[j].date = old_array[j-1].date;
				      }
    
				    old_array[i].date = date;
				    old_array[i].dbt = net_CacheDBTDup(&key);
					
					break;
			      }
	  	      }
		  }
	  }

#ifdef XP_UNIX
	/* assert(total_cl == net_DiskCacheSize); */
#endif 
	net_DiskCacheSize = total_cl;

#ifdef XP_UNIX
	/* assert(total_number == net_NumberInDiskCache); */
#endif 
	net_NumberInDiskCache = total_number;

	/* now old1 old2 and old3 should contain the oldest
	 * keys in the database.  Delete them all.
	 */
	for(i=0; i < remove_num; i++)
	  {
		DBT *db_obj = old_array[i].dbt;

		if(db_obj)
  		  {
			if(0 == (*cache_database->get)(cache_database, db_obj, &data, 0))
		 	  {
                  lock_date = net_GetTimeInCacheDBT(&data, LOCK_DATE_BYTE_POSITION);	

                  /* honor locks issued during this session */
                  TRACEMSG(("cache: comparing (2) lock date %ld to startup time %ld\n",
                            lock_date, NET_StartupTime));
                  if(lock_date > NET_StartupTime)
                  {
                      TRACEMSG(("cache: saved your ass again, bud!"));
                      continue;
                  }

				filename = net_GetFilenameInCacheDBT(&data);
				if(filename)
	  		  	  {
					TRACEMSG(("Removing file: %s due to disk"
				  			  " cache size overflow",filename));
#ifdef XP_OS2
					XP_LazyFileRemove(filename, xpCache);
#else
					NET_XP_FileRemove(filename, xpCache);
#endif
					PR_Free(filename);	
	  		  	  }
				net_DiskCacheSize -= net_calc_real_file_size(
											net_GetInt32InCacheDBT(&data, 
					 						CONTENT_LENGTH_BYTE_POSITION)); 
				net_NumberInDiskCache--;
    			(*cache_database->del)(cache_database, db_obj, 0);
			  }

    		net_FreeCacheDBTdata(db_obj);
		  }
	  }

	/* remove any invalid cache intries
	 */
	for(i=0; i < corrupt_entry_count; i++)
	  {
		DBT *newkey = corrupt_entry_array[i];
    	if(newkey && -1 == (*cache_database->del)(cache_database, newkey, 0))
		  {
			TRACEMSG(("Ack, error trying to delete corrupt entry, aborting",i));
			break;
		  }

		TRACEMSG(("Deleting %d corrupted cache entries",i));
    	net_FreeCacheDBTdata(newkey);
	  } 

	/* don't need the stale array any more */
	PR_Free(old_array);

	/* sync the database
	 */
	net_StoreDiskCacheSize();

    if(-1 == (*cache_database->sync)(cache_database, 0))
      {
        TRACEMSG(("Error syncing cache database"));

		/* close the database */
		(*cache_database->close)(cache_database);
		cache_database = 0;
      }
	      
	return(net_DiskCacheSize);
}

PRIVATE void
net_ReduceDiskCacheTo(MWContext *window_id, uint32 size, int recurse_count)
{
	uint32 avg, reduce, remove_num;

	if(net_DiskCacheSize < size 
		|| !net_NumberInDiskCache
		  || recurse_count >= 5)
		return;

	TRACEMSG(("net_ReduceDiskCacheTo: Current size: %d  "
			  "Desired Size: %d  Number: %d", 
			  net_DiskCacheSize,
			  size,
			  net_NumberInDiskCache));

	/* compute the avarage size of the
	 * contents of the disk cache
	 */
	if(net_NumberInDiskCache < 1)
		avg = 0;
	else
		avg = net_DiskCacheSize/net_NumberInDiskCache;

	/* compute the amount that we need to shrink
	 * the disk cache
	 */
	reduce = net_DiskCacheSize - size;

	/* add 10% of the total disk cache size
	 * to the reduce number to reduce
	 * the number of times that we need to
	 * do this operation
	 */
	reduce += net_DiskCacheSize/10;

	/* compute the number of objects to remove
	 */
	if(avg < 1)
		remove_num = 1;
	else
		remove_num = reduce/avg;

	if(remove_num < 1)
		remove_num = 1;

	if(window_id)
	  {
		char buffer[256];
		sprintf(buffer, XP_GetString( XP_CACHE_CLEANUP ), remove_num);
		NET_Progress(window_id, buffer);
	  }

	NET_RemoveDiskCacheObjects(remove_num);

	/* make sure we removed enough
	 */
	if(net_DiskCacheSize > size)
		net_ReduceDiskCacheTo(window_id, size, ++recurse_count);

	return;
}

#ifdef NO_MEMORY_CACHE

/* set the size of the Memory cache.
 * Set it to 0 if you want cacheing turned off
 * completely
 */
PUBLIC void
NET_SetMemoryCacheSize(int32 new_size)
{
	
	net_MaxMemoryCacheSize=new_size;
    IL_ReduceImageCacheSizeTo(new_size);
	return;
}

PUBLIC int32
NET_GetMaxMemoryCacheSize()
{
	return net_MaxMemoryCacheSize;
}

/* remove the last memory cache object if one exists
 * returns the total size of the memory cache in bytes
 * after performing the operation
 */
PUBLIC int32
NET_RemoveLastMemoryCacheObject()
{
	/* and remove something from the image cache */
	return IL_ShrinkCache();

	/* XXX now the mac arithmetic is wrong */
	return(0);
}

/* returns the number of bytes currently in use by the Memory cache
 */
PUBLIC int32
NET_GetMemoryCacheSize()
{
    return(0);
}

/* stub functions for memory cache that are no
 * longer needed
 */
MODULE_PRIVATE int
NET_MemoryCacheLoad (ActiveEntry * cur_entry)
{
	return(-1);
}

MODULE_PRIVATE int
NET_ProcessMemoryCache (ActiveEntry * cur_entry)
{
	return(-1);
}

MODULE_PRIVATE int
NET_InterruptMemoryCache (ActiveEntry * cur_entry)
{
	return(-1);
}

#endif /* MEMORY_CACHE */

/* set the size of the Disk cache.
 * Set it to 0 if you want cacheing turned off
 * completely
 */
PUBLIC void
NET_SetDiskCacheSize(int32 new_size)
{
	if(new_size < 0)
		new_size = 0;
#ifdef NU_CACHE
    DiskModule_SetSize(new_size);
#else

    net_MaxDiskCacheSize = new_size;

	/* open it if it's not open already */
    if(net_MaxDiskCacheSize > 0)
	  {
    	net_OpenCacheFatDB();
    	net_ReduceDiskCacheTo(NULL, (uint32) new_size, 0);
	  }
	else
	  {
		net_RemoveAllDiskCacheObjects();
	  }
#endif /* NU_CACHE */
}

/* remove the last disk cache object if one exists
 * returns the total size of the disk cache in bytes
 * after performing the operation
 */
PUBLIC int32
NET_RemoveLastDiskCacheObject(void)
{
	return(net_DiskCacheSize);
}

PRIVATE void
net_RemoveAllDiskCacheObjects(void)
#ifdef NU_CACHE
{
    PR_ASSERT(0); /* Should not be called */
}
#else
{
	char * filename;
	DBT data;
	DBT key;

	if(!cache_database)
		return;

	if(0 != (*cache_database->seq)(cache_database, &key, &data, R_FIRST))
		return;

	do
	  {
    	filename = net_GetFilenameInCacheDBT(&data);     
    	if(filename)                                      
          {                                                
            TRACEMSG(("Removing file: %s due to disk"       
                      " cache size remove",filename));     
#ifdef XP_OS2
	    XP_LazyFileRemove(filename, xpCache);
#else
            NET_XP_FileRemove(filename, xpCache);                 
#endif
            PR_Free(filename);                                    
          }                                                     
		/* We don't acctually need to do the
		 * del since we will del the database
		 */
        /* (*cache_database->del)(cache_database, &key, 0); */
	  }
	while(0 == (*cache_database->seq)(cache_database, &key, &data, R_NEXT));

    net_DiskCacheSize = 0;
    net_NumberInDiskCache = 0;

    if(-1 == (*cache_database->close)(cache_database))
      {
        TRACEMSG(("Error closing cache database"));
      }

	cache_database = 0;

	/* remove the database */
	NET_XP_FileRemove("", xpCacheFAT);
}
#endif /* NU_CACHE */

/* returns the number of bytes currently in use by the Disk cache
 */
PUBLIC int32
NET_GetDiskCacheSize()
{
    return(net_DiskCacheSize);
}


/******************************************************************
 * Cache Stream input routines
 */

/* is the stream ready for writing?
 */
PRIVATE unsigned int net_CacheWriteReady (NET_StreamClass *stream)
{
   CacheDataObject *obj=stream->data_object;   
   if(obj->next_stream)
       return((*obj->next_stream->is_write_ready)(obj->next_stream));
   else
	   return(MAX_WRITE_READY);
}

/*  stream write function
 */
PRIVATE int net_CacheWrite (NET_StreamClass *stream, CONST char* buffer, int32 len)
#ifdef NU_CACHE
{
    CacheDataObject* obj = stream->data_object;
    if (obj && obj->cache_object)
    {
        if (!CacheObject_Write(obj->cache_object, buffer, len))
        {
            if (obj->URL_s)
            obj->URL_s->dont_cache = TRUE;

            /* CacheObject_MarkForDeletion(); TODO*/
        }

        /* Write for next stream */
        if (obj->next_stream)
        {
            int status = 0;
            PR_ASSERT(buffer && (len >= 0));
            status = (*obj->next_stream->put_block)
                (obj->next_stream, buffer, len);

            /* abort */
            if(status < 0)
                return(status);
        }
    }
    return(1);
}
#else
{
	CacheDataObject *obj=stream->data_object;	
	TRACEMSG(("net_CacheWrite called with %ld buffer size", len));

    if(obj->fp)
	  {
		/* increase the global cache size counter
		 */
        if(NET_XP_FileWrite((char *) buffer, len, obj->fp) < len)
		  {
			TRACEMSG(("Error writing to cache file"));
			NET_XP_FileClose(obj->fp);
#ifdef XP_OS2
			XP_LazyFileRemove(obj->cache_object->filename, xpCache);
#else
			NET_XP_FileRemove(obj->cache_object->filename, xpCache);
#endif
			if(obj->URL_s)
				obj->URL_s->dont_cache = TRUE;
			obj->fp = 0;
		  }
			
	  }

    if(obj->next_stream)
	  {
    	int status=0;
	
		assert (buffer);
		assert (len > -1);
        status = (*obj->next_stream->put_block)
								(obj->next_stream, buffer, len);

		/* abort */
	    if(status < 0)
			return(status);
	  }

    return(1);
}
#endif /* NU_CACHE */

/*  complete the stream
*/
PRIVATE void net_CacheComplete (NET_StreamClass *stream)
#ifdef NU_CACHE
{
    CacheDataObject* obj = stream->data_object;
    PR_ASSERT(obj && obj->cache_object);

    /*Add it to the module and mark it as completed */
    CacheObject_Synch(obj->cache_object);
    CacheObject_SetIsCompleted(obj->cache_object, TRUE);

    /* Plugins can use this file */
    /* If object is in memory GetFilename will return null so this is ok */
    if (obj->URL_s && CacheObject_GetFilename(obj->cache_object))
    {
         StrAllocCopy(obj->URL_s->cache_file, CacheObject_GetFilename(obj->cache_object));
    }

    /* Complete the next stream */
    if (obj->next_stream)
    {
        (*obj->next_stream->complete)(obj->next_stream);
        PR_Free(obj->next_stream);
    }
    
    /* Do the things I don't as yet understand or have time to */
    PR_REMOVE_LINK(&obj->links);
    PR_Free(obj);
}
#else
{
	CacheDataObject *obj=stream->data_object;	
	/* close the cache file 
	 */
    if(obj->fp)
        NET_XP_FileClose(obj->fp);

	assert(obj->cache_object);

	/* add it to the database */
	if(net_CacheStore(obj->cache_object, 
					  obj->URL_s, 
					  FALSE, 
					  NEW_CACHE_STORE))
	  {
	    /* set the cache_file in the url struct so that Plugins can
 	     * use it...
	     */	
	    if(obj->URL_s)
		    StrAllocCopy(obj->URL_s->cache_file, obj->cache_object->filename);
	  }

    /* complete the next stream
     */
    if(obj->next_stream)
	  {
	    (*obj->next_stream->complete)(obj->next_stream);
        PR_Free(obj->next_stream);
	  }

	net_freeCacheObj(obj->cache_object);
	PR_REMOVE_LINK(&obj->links);
    PR_Free(obj);

    return;
}
#endif /* NU_CACHE */

/* Abort the stream 
 * 
 */
PRIVATE void net_CacheAbort (NET_StreamClass *stream, int status)
#ifdef NU_CACHE
{
    CacheDataObject* obj = stream->data_object;
    PR_ASSERT(obj);
    PR_ASSERT(obj->cache_object);
    
    /* abort the next stream */
    if(obj->next_stream)
    {
     (*obj->next_stream->abort)(obj->next_stream, status);
     PR_Free(obj->next_stream);
    }

    /* This will take care of open files and other cleanup activity
    plus will also mark the object as partial. TODO - Gagan */
    /* CacheObject_Abort(obj->cache_object); */
    /* Call CacheObject_Synch() if something was written */
    /* CacheObject_Destroy(obj->cache_object); */

    PR_REMOVE_LINK(&obj->links);
    PR_Free(obj);
}
#else
{

	CacheDataObject *obj=stream->data_object;	
	PR_ASSERT(obj);
	PR_ASSERT(obj->cache_object);

    /* abort the next stream
     */
    if(obj->next_stream)
	  {
	    (*obj->next_stream->abort)(obj->next_stream, status);
        PR_Free(obj->next_stream);
	  }

	/* fp might be null if we had a file write error */
    if(obj->fp)
      {
        NET_XP_FileClose(obj->fp);

		/* do this test in here since the file must be
		 * alright for us to use this
		 *
		 * test to see if we were user interrupted and if
		 * the server support byteranges.  If so, store
		 * the partial object since we will request it
		 * with a partial byterange later
		 *
		 * Also, we can't do partial caching unless we know
		 * the content_length
		 */
		if(obj->URL_s
			 && obj->URL_s->content_length
			  && (obj->URL_s->server_can_do_byteranges || obj->URL_s->server_can_do_restart)
				&& !obj->URL_s->dont_cache)  
		  {

			/* store the partial file */
            if(obj->URL_s->real_content_length > 0)
                obj->cache_object->content_length = obj->URL_s->real_content_length;
			net_CacheStore(obj->cache_object, 
						   obj->URL_s, 
						   TRUE, 
						   NEW_CACHE_STORE);

		  }
		else if(status == MK_INTERRUPTED 
				&& obj->URL_s->content_length
				&& !PL_strcasecmp(obj->URL_s->content_type, TEXT_HTML))
		  {
			/* temporarly cache interrupted HTML pages for history navigation */
			obj->URL_s->last_modified = 0;
			net_CacheStore(obj->cache_object, 
						   obj->URL_s, 
						   FALSE, 
						   NEW_CACHE_STORE);
			
		  }
		else
	 	  {
#ifdef XP_OS2
		    XP_LazyFileRemove(obj->cache_object->filename, xpCache);
#else
		    NET_XP_FileRemove(obj->cache_object->filename, xpCache);
#endif
	 	  }
      }

    net_freeCacheObj(obj->cache_object);
	PR_REMOVE_LINK(&obj->links);
    PR_Free(obj);

    return;
}
#endif


#ifdef XP_UNIX
char **fe_encoding_extensions; /* gag! */
#endif

/* setup the stream
 */
MODULE_PRIVATE NET_StreamClass * 
NET_CacheConverter (FO_Present_Types format_out,
                    void       *converter_obj,
                    URL_Struct *URL_s,
                    MWContext  *window_id)
{
    CacheDataObject * data_object=0;
#ifdef NU_CACHE
    void*  cache_object=0;
#else
    net_CacheObject * cache_object=0;
#endif /* NU_CACHE */
    NET_StreamClass * stream=0;
    char *filename=0, *new_filename=0;
	char *org_content_type = 0;
	XP_Bool do_disk_cache=FALSE;
	XP_Bool want_to_cache=FALSE;
    XP_File fp=0;
	NET_StreamClass * next_stream=0;

	/* XXX brendan will #define this hack after 3.0 ships! */
	XP_Bool dont_hold_URL_s = (converter_obj != NULL);
    
    TRACEMSG(("Setting up cache stream. Have URL: %s\n", URL_s->address));

	FE_EnableClicking(window_id);

#ifdef MOZILLA_CLIENT
    /* add this new url to the Global history since we must be getting it now
     */
	if (NET_URL_Type(URL_s->address) != WYSIWYG_TYPE_URL && URL_s->SARCache == NULL)
		GH_UpdateGlobalHistory(URL_s);
#endif /* MOZILLA_CLIENT */ 

	StrAllocCopy(org_content_type, URL_s->content_type);
	if(!org_content_type)
		return(NULL);

	if(format_out != FO_CACHE_ONLY)
	  {
		format_out = CLEAR_CACHE_BIT(format_out);
    	next_stream = NET_StreamBuilder(format_out, URL_s, window_id);
		if(!next_stream)
	   	  {
			PR_FREEIF(org_content_type);
			return(NULL);
	 	  }
	  }

	/* set the content type back the way it should be
	 * this is because some streams muck with it and
	 * dont set it back and we need it intact
	 */
	PR_Free(URL_s->content_type);
	URL_s->content_type = org_content_type;
	org_content_type = 0;

    /* do we want to cache this object?
     */
    if(URL_s->server_can_do_restart) URL_s->must_cache = TRUE;


    TRACEMSG(("cache: URL_s->must_cache is %s", URL_s->must_cache ? "TRUE" : "FALSE"));

    if(URL_s->must_cache	/* overrides all the rest of these tests */
	   ||  (CLEAR_CACHE_BIT(format_out) == FO_INTERNAL_IMAGE 
			|| format_out == FO_CACHE_ONLY
			|| !PL_strncasecmp(URL_s->content_type, "IMAGE", 5)
			|| !PL_strncasecmp(URL_s->content_type, "TEXT", 4)
			|| !PL_strncasecmp(URL_s->content_type, "MESSAGE", 7)
			|| !PL_strcasecmp(URL_s->content_type, APPLICATION_HTTP_INDEX)
			|| !PL_strcasecmp(URL_s->content_type, APPLICATION_JAVAARCHIVE)
			|| ( (URL_s->content_length>0) && ((uint32) URL_s->content_length < (net_MaxDiskCacheSize/4)) )
		   )
	   )
	  {
		/* set a flag to say that we want to cache */
		want_to_cache = TRUE;
	  }
    else
    {
     /* bypass the whole cache mechanism
      */
     if(format_out != FO_CACHE_ONLY)
     {
         format_out = CLEAR_CACHE_BIT(format_out);
         return(next_stream);
     }
     return NULL; 
    }
	
	/* set a flag if we should disk cache it based
 	 * on whether the cache setting is non-zero and
	 * the cache database is open
	 */

	HG52980
    TRACEMSG(("cache: want_to_cache is %s", want_to_cache ? "TRUE" : "FALSE"));

	if(want_to_cache 
	   && (HG73896 
			PL_strncasecmp(URL_s->address, "https:", 6))
	   && !URL_s->dont_cache
#ifdef NU_CACHE
    && (DiskModule_GetSize() > 0 || URL_s->must_cache)
#else
	   && (net_MaxDiskCacheSize > 0 || URL_s->must_cache)
	   && net_OpenCacheFatDB() /* make sure database is open */
#endif /* NU_CACHE */
  	   && PL_strncasecmp(URL_s->address, "news:", 5)   /* not news: */
	   HG73684
	   && PL_strncasecmp(URL_s->address, "mailbox://", 10))  /* not IMAP */
		do_disk_cache = TRUE;

	/* Special case for StreamAsFile plugins:  */
	if (URL_s->must_cache   /* this only set by plugin code ? */
		&& ((PL_strncasecmp(URL_s->address, "news:", 5)==0)      /* is news */
		HG83903
		|| !PL_strncasecmp(URL_s->address, "mailbox://", 10))) /* is imap */
		do_disk_cache = TRUE;

#ifdef NU_CACHE    
    cache_object = CacheObject_Create(URL_s->address);
    if (!cache_object)
     return 0;
    do_disk_cache = TRUE; /* Testing remove later */
    CacheObject_SetModule(cache_object, (PRInt16) (do_disk_cache ? DISK_MODULE_ID : MEM_MODULE_ID));
    CacheObject_SetExpires(cache_object, URL_s->expires);
    CacheObject_SetEtag(cache_object, URL_s->etag);
    CacheObject_SetLastModified(cache_object, URL_s->last_modified);
    CacheObject_SetContentLength(cache_object, URL_s->content_length);
    CacheObject_SetContentType(cache_object, URL_s->content_type);
    CacheObject_SetCharset(cache_object, URL_s->charset);
    CacheObject_SetContentEncoding(cache_object, URL_s->content_encoding);
    CacheObject_SetPageServicesURL(cache_object, URL_s->page_services_url);
    /* This filename discovery thing takes place every time! 
     * So optimize this in nu code, TODO
     */
    /* BEGIN BAD CODE */
       filename = WH_TempName(xpCache, "cache");
     
     if (filename)  
       {
         /* Put an appropriate suffix on the tmp cache file */
         if (URL_s->address) /* Why are we doing this? */
        {
          
          char *tail = NULL;
          char *suffix;
          char *allocSuffix; /* in case we have to allocate a suffix */
          char *end;
          char *junk;
          char old_char = 0;

          suffix = allocSuffix = NULL;

          if (URL_s->content_name){
              suffix = (URL_s->content_name ? PL_strrchr (URL_s->content_name, '.') : 0);
          }
          else if ((URL_s->address) && 
               (!PL_strncasecmp(URL_s->address, "mailbox:", 8)
               || !PL_strncasecmp(URL_s->address, "news:", 5)
               HG65294)
                && (URL_s->content_type) && (*(URL_s->content_type)))
          {
              /*
               Gag. If we're a mailbox URL and can't figure out the content name,
               but have a content type, ask the type registry for a suitable suffix. 
               (This allows us to set up Java and audio cache files so that those
               subsystems will actually work with the cache files. Ick.)
              */
              char *regSuffix = NET_cinfo_find_ext(URL_s->content_type);
              if (regSuffix)
              {
               suffix = PR_smprintf(".%s", regSuffix);
               allocSuffix = suffix; /* we allocated it here, we delete it below */
              }
          }
          
          if (!suffix)
          {
              tail = PL_strrchr (URL_s->address, '/');
              suffix = (tail ? PL_strrchr (tail, '.') : 0);
          }
          end = suffix + (suffix ? PL_strlen (suffix) : 0);
          junk=0;         
         
#ifdef XP_UNIX
          /* Gag.  foo.html.gz --> cacheXXXXX.html, not cacheXXXXX.gz. */
          if (suffix && fe_encoding_extensions)
            {
              int i = 0;
              while (fe_encoding_extensions [i])
             {
               if (!PL_strcmp (suffix, fe_encoding_extensions [i]))
                 {
                end = suffix;
                suffix--;
                while (suffix > tail && *suffix != '.')
                  suffix--;
                if (*suffix != '.')
                  suffix = 0;
                break;
                 }
               i++;
             }
            }
#endif

          /* Avoid "cache2F1F6AD102169F0.sources&maxhits=10" */
          if (suffix)
            {
              junk = suffix + 1;
              while (isalnum (*junk))
             junk++;
              old_char = *junk;
              *junk = 0;
            }


#ifdef XP_PC
          /* Remove any suffix that the temp filename currently has
           */
          strtok(filename, ".");
#endif

          if (suffix && (end - suffix) < 20)
            {
              /* make sure it is terminated... */
              if(!junk)
              {
               junk = end;
               old_char = *junk;
               *junk = 0;
              }
              StrAllocCopy(new_filename, filename);
#ifdef XP_PC
                            /*        the comment */
                            /*        say 16 bit  */
                            /*        32 bit Win  */
                            /*        goes thru   */
                            /*        here, so do */
                            /*        the same for*/
                            /*        OS2      */
              /* make all suffixes be UPPERCASE for win16
            * since the operating system will make 
            * them so and we need to know the exact
            * name for hashing and comparison purposes
            */
              if(1)
             {
               char *new_suffix = PL_strdup(suffix);
               if(new_suffix)
                 {
                char *cp = new_suffix;
                int i;
                /* limit it to 4 chars.
                 * a dot and three letters
                 */
                for(i=0; *cp && i < 4; i++)
                  {
                    *cp = NET_TO_UPPER(*cp);
                    cp++;
                  }
                *cp = '\0'; /* make sure it's terminated */
                StrAllocCat(new_filename, new_suffix);
                PR_Free(new_suffix);
                 }
             }
#else
              StrAllocCat(new_filename, suffix);
#endif
            }
          else 
            {
              StrAllocCopy(new_filename, filename);
            }


          if(junk)
              *junk = old_char;

          if (allocSuffix)
              PR_Free(allocSuffix);
        }
         PR_Free(filename);
       }
    /* END BAD CODE */
    CacheObject_SetFilename(cache_object, new_filename);
    /* TODO check for return from SetFilename */

    /*  I am still not convinced that PostData should be 
     persistent but here it is for keeping it sake. Once 
     FE/History takes this, this should be ripped off.*/
    if (URL_s->post_data)
    {
        CacheObject_SetPostData(cache_object, URL_s->post_data, URL_s->post_data_size);
    }

    data_object = PR_NEW(CacheDataObject);
    if (!data_object)
    {
     CacheObject_Destroy(cache_object);
     return NULL;
    }

    /* init the object */
    memset(data_object, 0, sizeof(CacheDataObject));

    /* assign the cache object to the stream data object
     */
    data_object->cache_object = cache_object;

    /* save the URL struct since we will
     * need to use some data from it later
     * XXX if converter_obj is non-null it is a flag telling us not
     * XXX to hold a pointer to the URL struct.
     */
    if (dont_hold_URL_s == FALSE)
     data_object->URL_s = URL_s;
    data_object->fp = 0 /* fp */; /* TODO -Gagan */

    stream = PR_NEW(NET_StreamClass);
    if (!stream)
    {
        CacheObject_Destroy(cache_object);
        PR_Free(data_object);
        return NULL;
    }

    stream->name         = "Nu Cache Stream";
    stream->complete     = (MKStreamCompleteFunc) net_CacheComplete;
    stream->abort        = (MKStreamAbortFunc) net_CacheAbort;
    stream->put_block    = (MKStreamWriteFunc) net_CacheWrite;
    stream->is_write_ready  = (MKStreamWriteReadyFunc) net_CacheWriteReady;
    stream->data_object     = data_object;  /* document info object */
    stream->window_id    = window_id;
    
    if (format_out != FO_CACHE_ONLY)
     data_object->next_stream = next_stream;

    PR_APPEND_LINK(&data_object->links, &active_cache_data_objects); /* verify */
    
    return stream;
    
#else /* NU_CACHE */
	/* malloc and init all the necessary structs
	 * do_disk_cache will be set false on error
	 */
	if(do_disk_cache)
      {

		TRACEMSG(("Object has not been cached before"));
			
        cache_object = PR_NEW(net_CacheObject);
        if (!cache_object)
          {
            return(NULL);
          }
    
        /* assign and init the cache object */
        memset(cache_object, 0, sizeof(net_CacheObject));
    
        /* copy the contents of the URL structure
         */
        StrAllocCopy(cache_object->address, URL_s->address);
        cache_object->method = URL_s->method;
        if(URL_s->post_data)
          {
			cache_object->post_data_size = URL_s->post_data_size;
            StrAllocCopy(cache_object->post_data, URL_s->post_data);
            StrAllocCopy(cache_object->post_headers, URL_s->post_headers);
          }
    
        /* add expiration stuff from URL structure
         */
	    cache_object->expires        = URL_s->expires;
	    cache_object->last_modified  = URL_s->last_modified;
    
        cache_object->content_length      = URL_s->content_length;
        cache_object->real_content_length = URL_s->content_length;
        cache_object->is_netsite     = URL_s->is_netsite;

        StrAllocCopy(cache_object->content_type, URL_s->content_type);
        StrAllocCopy(cache_object->charset, URL_s->charset);
        StrAllocCopy(cache_object->content_encoding, URL_s->content_encoding);

		HG32138
        StrAllocCopy(cache_object->page_services_url, URL_s->page_services_url);

		/* we always use relative paths in the main disk cache
		 */
		cache_object->is_relative_path        = TRUE;

        /* open a file for the cache
		   larubbio added SARCache check it's late in 4.0 cycle, and I don't want to have
           any impact on the other platforms

         */
		  filename = WH_TempName(
#ifdef XP_UNIX 
			 (NULL != URL_s->SARCache) ? xpSARCache :
#endif
			 xpCache, "cache");
		
		if (filename)  
		  {
			/* Put an appropriate suffix on the tmp cache file */
			if (cache_object->address)
			  {
				
				char *tail = NULL;
				char *suffix;
				char *allocSuffix; /* in case we have to allocate a suffix */
				char *end;
				char *junk;
				char old_char = 0;

				suffix = allocSuffix = NULL;

				if (URL_s->content_name){
					suffix = (URL_s->content_name ? PL_strrchr (URL_s->content_name, '.') : 0);
				}
				else if ((URL_s->address) && 
						(!PL_strncasecmp(URL_s->address, "mailbox:", 8)
						|| !PL_strncasecmp(URL_s->address, "news:", 5)
						HG65294)
						 && (URL_s->content_type) && (*(URL_s->content_type)))
				{
					/*
						Gag. If we're a mailbox URL and can't figure out the content name,
						but have a content type, ask the type registry for a suitable suffix. 
						(This allows us to set up Java and audio cache files so that those
						subsystems will actually work with the cache files. Ick.)
					*/
					char *regSuffix = NET_cinfo_find_ext(URL_s->content_type);
					if (regSuffix)
					{
						suffix = PR_smprintf(".%s", regSuffix);
						allocSuffix = suffix; /* we allocated it here, we delete it below */
					}
				}
				
				if (!suffix)
				{
				    tail = PL_strrchr (cache_object->address, '/');
					suffix = (tail ? PL_strrchr (tail, '.') : 0);
				}
				end = suffix + (suffix ? PL_strlen (suffix) : 0);
				junk=0;				
			
#ifdef XP_UNIX
				/* Gag.  foo.html.gz --> cacheXXXXX.html, not cacheXXXXX.gz. */
				if (suffix && fe_encoding_extensions)
				  {
					int i = 0;
					while (fe_encoding_extensions [i])
					  {
						if (!PL_strcmp (suffix, fe_encoding_extensions [i]))
						  {
							end = suffix;
							suffix--;
							while (suffix > tail && *suffix != '.')
							  suffix--;
							if (*suffix != '.')
							  suffix = 0;
							break;
						  }
						i++;
					  }
				  }
#endif

				/* Avoid "cache2F1F6AD102169F0.sources&maxhits=10" */
				if (suffix)
				  {
					junk = suffix + 1;
					while (isalnum (*junk))
					  junk++;
					old_char = *junk;
					*junk = 0;
				  }


#ifdef XP_PC
				/* Remove any suffix that the temp filename currently has
				 */
				strtok(filename, ".");
#endif

				if (suffix && (end - suffix) < 20)
				  {
					/* make sure it is terminated... */
					if(!junk)
				  	{
						junk = end;
						old_char = *junk;
						*junk = 0;
				  	}
					StrAllocCopy(new_filename, filename);
#ifdef XP_PC
                                                 /*              the comment */
                                                 /*              say 16 bit  */
                                                 /*              32 bit Win  */
                                                 /*              goes thru   */
                                                 /*              here, so do */
                                                 /*              the same for*/
                                                 /*              OS2         */
					/* make all suffixes be UPPERCASE for win16
					 * since the operating system will make 
					 * them so and we need to know the exact
					 * name for hashing and comparison purposes
					 */
					if(1)
					  {
						char *new_suffix = PL_strdup(suffix);
						if(new_suffix)
						  {
							char *cp = new_suffix;
							int i;
							/* limit it to 4 chars.
							 * a dot and three letters
							 */
							for(i=0; *cp && i < 4; i++)
							  {
								*cp = NET_TO_UPPER(*cp);
								cp++;
							  }
							*cp = '\0'; /* make sure it's terminated */
							StrAllocCat(new_filename, new_suffix);
							PR_Free(new_suffix);
						  }
					  }
#else
					StrAllocCat(new_filename, suffix);
#endif
				  }
				else 
				  {
					StrAllocCopy(new_filename, filename);
				  }


				if(junk)
					*junk = old_char;

				if (allocSuffix)
					PR_Free(allocSuffix);
			  }
			PR_Free(filename);
		  }

		if(new_filename)
		{
			/* larubbio added 197 SAR cache checks */
			if ( URL_s->SARCache == NULL )
       			fp = NET_XP_FileOpen(new_filename, xpCache, XP_FILE_WRITE_BIN);
			else
       			fp = NET_XP_FileOpen(new_filename, xpSARCache, XP_FILE_WRITE_BIN);
		}
   
		if(!fp)
		  {
    	    net_freeCacheObj(cache_object);
			cache_object = 0;
			do_disk_cache = FALSE;
			PR_FREEIF(new_filename);
		  }
		else
		  {
            cache_object->filename = new_filename;

		    /* cap the total size of the cache
		     */

			/* larubbio disk cache size checks for SARCache 
			 * Also, don't reduce the cache size yet for "must_cache"
			 * objects, since in the download-restart case the object
			 * will be deleted most of the time before it ever gets
			 * counted in the cache.  Objects that do get put into
			 * the cache will self correct at the next call to
			 * ReduceDiskCacheTo()
			 */
			if(net_DiskCacheSize+URL_s->content_length > net_MaxDiskCacheSize 
				&& URL_s->SARCache == NULL
				&& !URL_s->must_cache)
			  {
				int32 new_size = net_MaxDiskCacheSize - URL_s->content_length;
				if(new_size < 0)
					new_size = 0;
				net_ReduceDiskCacheTo(window_id, (uint32)new_size, 0);
			  }
		  }
	  }

	/* if we get this far and do_disk_cache is true
	 * then we want cache and all the setup succeeded.
	 */
	if(do_disk_cache)
	  {
		TRACEMSG(("Caching this URL!!!!"));

        data_object = PR_NEW(CacheDataObject);
        if (!data_object)
          {
			NET_XP_FileClose(fp);
            return(NULL);
          }

        stream = PR_NEW(NET_StreamClass);
        if(!stream)
		  {
			NET_XP_FileClose(fp);
			PR_Free(data_object);
            return(NULL);
		  }

        /* init the object */
        memset(data_object, 0, sizeof(CacheDataObject));
    
	    /* assign the cache object to the stream data object
	     */
        data_object->cache_object = cache_object;

		/* save the URL struct since we will
	 	 * need to use some data from it later
		 * XXX if converter_obj is non-null it is a flag telling us not
		 * XXX to hold a pointer to the URL struct.
		 */
		if (dont_hold_URL_s == FALSE)
			data_object->URL_s = URL_s;
        data_object->fp = fp;

    	TRACEMSG(("Returning stream from NET_CacheConverter\n"));

    	stream->name           = "Cache stream";
    	stream->complete       = (MKStreamCompleteFunc) net_CacheComplete;
    	stream->abort          = (MKStreamAbortFunc) net_CacheAbort;
    	stream->put_block      = (MKStreamWriteFunc) net_CacheWrite;
    	stream->is_write_ready = (MKStreamWriteReadyFunc) net_CacheWriteReady;
    	stream->data_object    = data_object;  /* document info object */
    	stream->window_id      = window_id;

    	if(format_out != FO_CACHE_ONLY)
            data_object->next_stream = next_stream;

		PR_APPEND_LINK(&data_object->links, &active_cache_data_objects);
    	return stream;
	  }
	else
	  {

		TRACEMSG(("NOT Disk Caching this URL"));

		if(want_to_cache)
		  {
			net_MemCacheConverterObject mem_conv_obj;

			mem_conv_obj.next_stream = next_stream;
			mem_conv_obj.dont_hold_URL_s = dont_hold_URL_s;
			return(NET_MemCacheConverter(format_out, 
										 &mem_conv_obj,
										 URL_s, 
										 window_id));
		  }

		/* bypass the whole cache mechanism
	 	 */
	    if(format_out != FO_CACHE_ONLY)
          {
          	format_out = CLEAR_CACHE_BIT(format_out);

            return(next_stream);
		  }

	  }
#endif /* NU_CACHE */
	return(NULL);
}

/***************************************************************************
 *   Cache object manipulation stuff
 */

/* locks or unlocks a cache file.  The 
 * cache lock is only good for a single
 * session
 */
PUBLIC XP_Bool
NET_ChangeCacheFileLock(URL_Struct *URL_s, XP_Bool set)
#ifdef NU_CACHE
{
    PR_ASSERT(0); /* Shouldn't be getting called */
    return TRUE;
}
#else
{
	int   status;
	DBT   data;
    DBT * key;
	DBT * new_data=0;
	net_CacheObject *found_cache_obj;

	/* lock or unlock the memory cache too */
	NET_ChangeMemCacheLock(URL_s, set);

	if(!cache_database)
		return(FALSE);

    key = net_GenCacheDBKey(URL_s->address,
                            URL_s->post_data,
                            URL_s->post_data_size);

    status = (*cache_database->get)(cache_database, key, &data, 0);

    if(status != 0)
      {
        TRACEMSG(("Key not found in database"));
        net_FreeCacheDBTdata(key);
        return(FALSE);
      }

	found_cache_obj = net_Fast_DBDataToCacheStruct(&data);

	if(!found_cache_obj)
	  {
		TRACEMSG(("mkcache: Corrupted db entry"));
		return(FALSE);
	  }

	if(set)
		found_cache_obj->lock_date = time(NULL);
	else
		found_cache_obj->lock_date = 0;

    /* create new dbt data object */
    new_data = net_CacheStructToDBData(found_cache_obj);

	if(new_data)
		status = (*cache_database->put)(cache_database, key, new_data, 0);

	return(status == 0);
}
#endif

MODULE_PRIVATE void NET_RefreshCacheFileExpiration(URL_Struct * URL_s)
#ifdef NU_CACHE
{
    void* cache_object;

    /* only update if the server status is 304 
     * The other cases involve a new cache object
     * and all the info will automatically get updated
     */
    if (URL_s->server_status != 304)
    {
     return;
    }

    cache_object = CacheManager_GetObject(URL_s->address);
    if (cache_object)
    {
     CacheObject_SetLastModified(cache_object, URL_s->last_modified);
     CacheObject_SetExpires(cache_object, URL_s->expires);
    }
}
#else
{
    net_CacheObject *found_cache_obj;
    int  status;
    DBT *key;
    DBT  data;

    if(!cache_database)
      {
        TRACEMSG(("Cache database not open"));
        return;
      }

	/* only update if the server status is 304 
	 * The other cases involve a new cache object
	 * and all the info will automatically get updated
	 */
	if(URL_s->server_status != 304)
	  	return;

	/* gen a key */
    key = net_GenCacheDBKey(URL_s->address,
                            URL_s->post_data,
                            URL_s->post_data_size);

	/* look up the key */
    status = (*cache_database->get)(cache_database, key, &data, 0);

	/* don't need the key anymore */
    net_FreeCacheDBTdata(key);

    if(status != 0)
        return;

	/* turn the cache DB object into a cache object */
    found_cache_obj = net_Fast_DBDataToCacheStruct(&data);

    if(!found_cache_obj)
        return;

    /* stick the address and post data in there
     * since this stuff is ususally in the Key
     */
    StrAllocCopy(found_cache_obj->address, URL_s->address);
    if(URL_s->post_data_size)
      {
        found_cache_obj->post_data_size = URL_s->post_data_size;
        BlockAllocCopy(found_cache_obj->post_data,
                       URL_s->post_data,
                       URL_s->post_data_size);
      }

	/* this will update the last modified time */
	net_CacheStore(found_cache_obj, 
					NULL, 
					FALSE, 
					REFRESH_CACHE_STORE);

	return;

}
#endif /* NU_CACHE */

/* returns TRUE if the url is in the disk cache
 */
PUBLIC XP_Bool
NET_IsURLInDiskCache(URL_Struct *URL_s)
{
	DBT *key;
	DBT data;
	int status;
#ifdef NU_CACHE
    PR_ASSERT(0); /* Don't call this! Use DiskModule_Contains(URL_s->address);*/ 
    return FALSE; 
#endif

	if(!cache_database)
	  {
		TRACEMSG(("Cache database not open"));
	 	return(0); 
	  }

	key = net_GenCacheDBKey(URL_s->address, 
							URL_s->post_data, 
							URL_s->post_data_size);

	status = (*cache_database->get)(cache_database, key, &data, 0);

    net_FreeCacheDBTdata(key);
	
	if(status == 0)
		return(TRUE);
	else
		return(FALSE);
}

/* remove a URL from the cache
 */
MODULE_PRIVATE void
NET_RemoveURLFromCache(URL_Struct *URL_s)
#ifdef NU_CACHE
{
    if (!URL_s || !URL_s->address)
     return;

    CacheManager_Remove(URL_s->address);
}
#else
{

	int status;
	DBT data;
	DBT *key;

	/* larubbio */
	XP_Bool		SARCache = FALSE;
	XP_FileType fileType;
	DB			*local_cache_database = NULL;

	/* larubbio fix to allow 197 SAR cache adds without modifying 
	 * the navigator cache (too much)
	 */
	if ( URL_s != NULL && URL_s->SARCache != NULL )
	{
		SARCache = TRUE;
		fileType = xpSARCache;
		local_cache_database = URL_s->SARCache->database;
	}
	else
	{
		fileType = xpCache;
		local_cache_database = cache_database;
	}


	NET_RemoveURLFromMemCache(URL_s);

	if(!local_cache_database)
		return;

	key = net_GenCacheDBKey(URL_s->address, 
								URL_s->post_data, 
								URL_s->post_data_size);

	if(!key)
		return;

	status = (*local_cache_database->get)(local_cache_database, key, &data, 0);

	if(status == 0)
 	  {
        char * filename = net_GetFilenameInCacheDBT(&data);

		if(filename)
		  {
#ifdef XP_OS2
			XP_LazyFileRemove(filename, fileType);
#else
			NET_XP_FileRemove(filename, fileType);
#endif
			PR_Free(filename);
		  }

		(*local_cache_database->del)(local_cache_database, key, 0);

		if ( SARCache )
		{
			/* subtract the size of the file being removed */
        	URL_s->SARCache->DiskCacheSize -= net_calc_real_file_size(
											    net_GetInt32InCacheDBT(&data,
												CONTENT_LENGTH_BYTE_POSITION));
			URL_s->SARCache->NumberInDiskCache--;
		}
		else
		{
        	net_DiskCacheSize -= net_calc_real_file_size(
											    net_GetInt32InCacheDBT(&data,
												CONTENT_LENGTH_BYTE_POSITION));
			net_NumberInDiskCache--;
		}
	  }

	net_FreeCacheDBTdata(key);
}
#endif /* NU_CACHE */

/* return TRUE if the URL is in the cache and
 * is a partial cache file
 */
MODULE_PRIVATE PRBool
NET_IsPartialCacheFile(URL_Struct *URL_s)
#ifdef NU_CACHE 
{
    if (!URL_s || !URL_s->address)
     return PR_FALSE;
    /* Todo change this to just getObject, if null then... - Gagan */    
    if (CacheManager_Contains(URL_s->address))
    {
        return CacheObject_IsPartial(CacheManager_GetObject(URL_s->address));
    }
    return PR_FALSE;
}
#else
{
	net_CacheObject *found_cache_obj;
	int  status;
	DBT *key;
	DBT  data;

    if(!cache_database)
      {
        TRACEMSG(("Cache database not open"));
        return(0);
      }

	key = net_GenCacheDBKey(URL_s->address, 
							URL_s->post_data, 
							URL_s->post_data_size);

	status = (*cache_database->get)(cache_database, key, &data, 0);

	net_FreeCacheDBTdata(key);

	if(status != 0)
		return(0);
		
	found_cache_obj = net_Fast_DBDataToCacheStruct(&data);

	if(found_cache_obj)
		return(found_cache_obj->incomplete_file);
	else
		return(0);
}
#endif /* NU_CACHE */

PUBLIC int
NET_FindURLInCache(URL_Struct * URL_s, MWContext *ctxt)
#ifdef NU_CACHE
{

    if (!URL_s || !URL_s->address)
     return 0;
    
    /* zero the last modified date so that we don't
     * screw up the If-modified-since header by
     * having it in even when the document isn't
     * cached.
     */
    URL_s->last_modified = 0;

    if (!CacheManager_Contains(URL_s->address))
        return 0;
    else
    {
     /* mkabout.c relies on updating the URL_struct with the found info */
     void* pObject = CacheManager_GetObject(URL_s->address);
     /* Copy all the stuff from CacheObject to URL_struct */
     if (pObject)
     {
         URL_s->expires = CacheObject_GetExpires(pObject);
         URL_s->last_modified = CacheObject_GetLastModified(pObject);
         StrAllocCopy(URL_s->charset, CacheObject_GetCharset(pObject));
         StrAllocCopy(URL_s->content_encoding, CacheObject_GetContentEncoding(pObject));
         if (!URL_s->preset_content_type)
         {
             StrAllocCopy(URL_s->content_type, CacheObject_GetContentType(pObject));
         }
         /* StrAllocCopy(URL_s->page_services_url, CacheObject_GetPageServicesURL() */
        
         /* Only set these for HTTP URLs otherwise the content-length will mess up files that
          * dont return it and are different. 
          */
         if(!PL_strncasecmp(URL_s->address, "http", 4) || !PL_strncasecmp(URL_s->address, "ftp", 3))
         {
             URL_s->content_length = CacheObject_GetContentLength(pObject);
             URL_s->real_content_length = CacheObject_GetSize(pObject); 
             /* If no Content-Length: header was supplied */
             if ((0 == URL_s->content_length) && (URL_s->real_content_length>0))
                URL_s->content_length = URL_s->real_content_length;
         }

         URL_s->cache_object = pObject;

         return NU_CACHE_TYPE_URL;
     }
     else 
         return 0;
    }
}
#else
{
	net_CacheObject *found_cache_obj;
    XP_StatStruct    stat_entry;
	int   status;
	char *byterange;
	char  byterange_char = 0;
	DBT  *key;
	DBT   data;
	TRACEMSG(("Checking for URL in cache"));

	/* zero the last modified date so that we don't
	 * screw up the If-modified-since header by
     * having it in even when the document isn't
	 * cached.
	 */
	URL_s->last_modified = 0;

	if(!cache_database)
	  {
		TRACEMSG(("Cache database not open"));

		/* can't find it here.  Let's look
		 * in the memory and external caches
		 */
		status = NET_FindURLInMemCache(URL_s, ctxt);
	
		if(status)
			return(status);
		else
			return(NET_FindURLInExtCache(URL_s, ctxt));
	  }

    byterange = PL_strcasestr(URL_s->address, ";bytes=");
    
    /* this might be a cached imap url.  Ignore the mime
       part when going back for attachments, otherwise we'll
       reload the attachment from the server */
    if (!byterange)
    	byterange = PL_strcasestr(URL_s->address, "&part=");

    if(byterange)
      {
        byterange_char = *byterange;
        *byterange = '\0';
      }

	key = net_GenCacheDBKey(URL_s->address, 
							URL_s->post_data, 
							URL_s->post_data_size);
	

    if(byterange)
        *byterange = byterange_char;

	status = (*cache_database->get)(cache_database, key, &data, 0);


	/* try it again with the byterange stuff
	 */
	if(status != 0 && byterange)
	  {
		net_FreeCacheDBTdata(key);
		key = net_GenCacheDBKey(URL_s->address, 
								URL_s->post_data, 
								URL_s->post_data_size);
		status = (*cache_database->get)(cache_database, key, &data, 0);
	  }

	/* If the file doesn't have an extension try adding
	 * a slash to the end and look for it again.
	 * this will catch the most common redirect cases
 	 */
	if(status != 0)
	  {
		char *slash;

		slash = PL_strrchr(URL_s->address, '/');
		if(slash && !PL_strchr(slash, '.') && *(slash+1) != '\0')
		  {
			/* no extension.  Add the slash and
			 * look for it in the database
			 */
			StrAllocCat(URL_s->address, "/");
			net_FreeCacheDBTdata(key);
			key = net_GenCacheDBKey(URL_s->address, 
									URL_s->post_data, 
									URL_s->post_data_size);
			status = (*cache_database->get)(cache_database, key, &data, 0);

			if(status != 0)
			  {
				/* adding the slash didn't work, get rid of it 
			 	 */
				URL_s->address[PL_strlen(URL_s->address)-1] = '\0';
			  }
		  }
	  }

	/* If the java Cache api is requesting this file, and it was found in the
	 * navigator cache, remove it from that cache so it can be recached in the proper archive cache
	 */
	if( (0 == status) && (NULL != URL_s->SARCache) )
	  {
		found_cache_obj = net_Fast_DBDataToCacheStruct(&data);

		if(!found_cache_obj)
		{
			TRACEMSG(("mkcache: Corrupted db entry"));
			return(0);
		}

		/* remove the object from the cache
		 */
		NET_XP_FileRemove(found_cache_obj->filename, xpCache);

		if(cache_database)
			status = (*cache_database->del)(cache_database, key, 0);

		/* remove the size of that file from the running total */
        net_DiskCacheSize -= net_calc_real_file_size(
											found_cache_obj->content_length);

		status = -1;
	  }


	if(status != 0)
	  {
		TRACEMSG(("Key not found in database"));
		net_FreeCacheDBTdata(key);

		/* can't find it here.  Let's look
		 * in the memory and external caches
		 */
		status = NET_FindURLInMemCache(URL_s, ctxt);
	
		if(status)
			return(status);
		else
			return(NET_FindURLInExtCache(URL_s, ctxt));
	  }

	found_cache_obj = net_Fast_DBDataToCacheStruct(&data);

	if(!found_cache_obj)
	  {
		TRACEMSG(("mkcache: Corrupted db entry"));
		return(0);
	  }

    TRACEMSG(("mkcache: found URL in cache!"));

	/* pretend we don't have the object cached if we are looking
	 * for a byterange URL and the object is only partially
	 * cached
	 */
	if(byterange && found_cache_obj->incomplete_file)
	  {
		return(0);
	  }

	/* make sure the file still exists
	 */
	if(NET_XP_Stat(found_cache_obj->filename, &stat_entry, xpCache) == -1)
	  {
		/* file does not exist!!
		 * remove the entry 
		 */
		TRACEMSG(("Error! Cache file missing"));

		if(cache_database)
			status = (*cache_database->del)(cache_database, key, 0);

		/* remove the size of that file from the running total */
        net_DiskCacheSize -= net_calc_real_file_size(
											found_cache_obj->content_length);

#ifdef DEBUG
		if(status != 0)
			TRACEMSG(("Error deleting bad file from cache"));
#endif /* DEBUG */

	    net_FreeCacheDBTdata(key);

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
	 *
	 * NOTE: use the use_local_copy flag as a signal not to
 	 * do the "once per session" check since it is possible
	 * that the cache is read only, also use history_num
	 * since we shouldn't reload on history navigation or resize
     */
    if(!URL_s->use_local_copy)
	  {
	    if(found_cache_obj->last_accessed < NET_StartupTime
		    && NET_CacheUseMethod != CU_NEVER_CHECK 
#ifdef MOZ_OFFLINE          
			&& !NET_IsOffline() 
#endif /* MOZ_OFFLINE */
         )							/* *X* check for is offline */
	      {
		    if(!PL_strncasecmp(URL_s->address, "http", 4))
		      {
        	    URL_s->expires = 1;

#if 0
	/* this is ifdeffed because of the following bug:
	 * If you update now, there is still the possibility
	 * of putting this URL on the wait queue.  If that
	 * happens, when the URL gets pulled off the wait queue
	 * it will be called with NET_GetURL again and come
 	 * here again.  Since we already updated it wont
	 * get set expired again.  So instead I will update
	 * the URL when I get a 304 in NET_RefreshCacheFileExpiration
	 */
				/* stick the address and post data in there
				 * since this stuff is in the Key
				 */
				StrAllocCopy(found_cache_obj->address, URL_s->address);
				if(URL_s->post_data_size)
				  {
					found_cache_obj->post_data_size = URL_s->post_data_size;
					BlockAllocCopy(found_cache_obj->post_data,
							   		URL_s->post_data, 
							   		URL_s->post_data_size);
				  }
				
				/* this will update the last accessed time
				 */
				net_CacheStore(found_cache_obj, 
							   NULL, 
							   FALSE, 
							   REFRESH_CACHE_STORE);
#endif
		      }
		    else
		      {
			    /* remove the object from the cache
			     */
#ifdef XP_OS2
			    XP_LazyFileRemove(found_cache_obj->filename, xpCache);
#else
			    NET_XP_FileRemove(found_cache_obj->filename, xpCache);
#endif
			    /* remove the size of that file from the running total */
        	    net_DiskCacheSize -= net_calc_real_file_size(
											found_cache_obj->content_length);
    
			    status = (*cache_database->del)(cache_database, key, 0);
#ifdef DEBUG
			    if(status != 0)
				    TRACEMSG(("Error deleting expired file from cache"));
#endif /* DEBUG */

	    	    net_FreeCacheDBTdata(key);

			    return(0);
		      }
	      }
	    else
	      {
		    /* otherwise use the normal expires date */
 		    URL_s->expires = found_cache_obj->expires;
	      }
	  }
	else
	  {
 		URL_s->expires = 0; /* prevent looping */
	  }

	/* we don't need the key anymore */
	net_FreeCacheDBTdata(key);

	/* copy in the cache file name
	 */
	StrAllocCopy(URL_s->cache_file, found_cache_obj->filename);

    /* copy the contents of the URL struct so that the content type
     * and other stuff gets recorded
     */
	if(!URL_s->preset_content_type)
    	StrAllocCopy(URL_s->content_type,     found_cache_obj->content_type);
    StrAllocCopy(URL_s->charset,          found_cache_obj->charset);
    StrAllocCopy(URL_s->content_encoding, found_cache_obj->content_encoding);

    StrAllocCopy(URL_s->page_services_url, found_cache_obj->page_services_url);

	/* only set these for HTTP url's
	 * otherwise the content-length will mess up files that don't
	 * return a content-length and are different
	 */
	if(!PL_strncasecmp(URL_s->address, "http", 4) || !PL_strncasecmp(URL_s->address, "ftp", 3))
	  {
    	URL_s->content_length = found_cache_obj->content_length;
   		URL_s->real_content_length = found_cache_obj->real_content_length;

	    if(URL_s->content_length != URL_s->real_content_length
	       && !found_cache_obj->incomplete_file)
	      {
            /* TODO later- switch back to a PR_ASSERT(0) */
            if (1)
            {
			    char *buf = 0;
			    StrAllocCopy(buf,"Possible Gromit vs. Mozilla cache corruption!\nDid you forget to clean your cache?\nIf you see this error AFTER cleaning your cache then call me- Gagan(x2187)");
			    FE_Alert(ctxt, buf);
			    PR_Free(buf);
            }

			URL_s->real_content_length = 0;
			URL_s->content_length = 0;

		    /* content type indicates a partial cache file but
		     * the incomplete file flag is not set.  Something
		     * went wrong.
		     */
		    return(0);  /* return not found */
	  }
	  }

 	URL_s->last_modified  = found_cache_obj->last_modified;
 	URL_s->is_netsite     = found_cache_obj->is_netsite;

	HG83267

	TRACEMSG(("Cached copy is valid. returning method"));

	TRACEMSG(("Using Disk Copy"));

	return(FILE_CACHE_TYPE_URL);
}
#endif


/* read the Cache File allocation table.
 */
PUBLIC void
NET_ReadCacheFAT(char * cachefatfile, XP_Bool stat_files)
{
#ifdef NU_CACHE
    PR_ASSERT(0); /* Shouldn't be getting called */
#endif
    if(net_MaxDiskCacheSize > 0)
    	net_OpenCacheFatDB();

	return;
}

PUBLIC void
NET_CacheInit(void)
{
#ifdef NU_CACHE
    Cache_Init();
#else
	 NET_ReadCacheFAT("", TRUE);
#endif /* NU_CACHE */
}

/* unload the disk cache FAT list to disk
 *
 * set final_call to true if this is the last call to
 * this function before cleaning up the cache and shutting down.
 *
 * Front ends should never set final_call to TRUE, this will
 * be done by the cache cleanup code when the netlib is being
 * shutdown.
 */
PUBLIC void
NET_WriteCacheFAT(char *filename, XP_Bool final_call)
{
#ifdef NU_CACHE
    PR_ASSERT(0);
#endif
	net_StoreDiskCacheSize();

	if(!cache_database)
        return;

    if(-1 == (*cache_database->sync)(cache_database, 0))
      {
        TRACEMSG(("Error syncing cache database"));
		/* close the database */
		(*cache_database->close)(cache_database);
		cache_database = 0;
      }

	return;
}

MODULE_PRIVATE void
NET_CleanupCache (void)
{
#ifdef NU_CACHE
    Cache_Shutdown();
#else
	net_StoreDiskCacheSize();

	if(!cache_database)
        return;

    if(-1 == (*cache_database->close)(cache_database))
      {
        TRACEMSG(("Error closing cache database"));
      }

	cache_database = 0;

	CACHE_CloseAllOpenSARCache();
#endif
}

/* returns the number of files currently in the
 * disk cache
 */
PUBLIC uint32
NET_NumberOfFilesInDiskCache()
{
#ifdef NU_CACHE
     return DiskModule_Entries();
#else
	return(net_NumberInDiskCache);
#endif
}

static int
net_cache_strcmp (const void *a, const void *b)
{
  return PL_strcmp ((const char *) a, (const char *) b);
}


/* this function recurses through all directories in
 * the cache directory and stuffs all the files into
 * the hash table.  Any directory names below the
 * root cache directory should have their relative path 
 * names prepended to them
 *
 * returns 0 on success, -1 on failure
 */
PRIVATE int
net_cache_recursive_file_finder(XP_HashList *hash_table, 
								const char *prefix,
								char *cur_dir, 
								char *base_dir,
								char *buffer)
{
	PRDir *dir_ptr;
   	PRDirEntry *dir_entry;
	int base_len;
	int prefix_len, d_len, status;
	char *dir_prefix=0;
	const char *d_name;
	XP_Bool add_dir_prefix=TRUE;

	/* compute the difference between base_dir and
	 * cur_dir.  The difference should be prepended
	 * to the front of filenames
	 */
	base_len = PL_strlen(base_dir);
	StrAllocCopy(dir_prefix, cur_dir+base_len);

	if(!dir_prefix)
		return(-1);


	if(*dir_prefix == '\0')
		add_dir_prefix = FALSE;
	else if(dir_prefix[PL_strlen(dir_prefix)-1] == '/')
		dir_prefix[PL_strlen(dir_prefix)-1] = '\0';

    if(!(dir_ptr = PR_OpenDir (cur_dir)))
	{
		PR_Free(dir_prefix);
		return(-1);
	}

	/* add all the files on disk to a hash table
	 */
	prefix_len = PL_strlen(prefix);
    while((dir_entry = PR_ReadDir(dir_ptr, PR_SKIP_BOTH)) != 0)
      {
		/* only do comparison and delete if the prefix
		 * is the same
		 */
		d_name = dir_entry->name;
		d_len = PL_strlen(d_name);
		status = 0;
		if(PL_strncmp(d_name, prefix, prefix_len))
	 	  {
#if 0
    		XP_StatStruct stat_entry;
#endif

			/* This directory didn't begin with the cache prefix, so it
			   *should* be a directory.  But if it's not, then it's some
			   random junk that somehow got into the cache directory.

			   Rather than stat'ing it first and then recursing, we will
			   simply assume it's a directory, recurse, and call XP_OpenDir
			   on it.  If it's not a directory this will fail, and we will
			   ignore it.
			 */

#ifdef XP_UNIX
			/* Unix-specific speed hack: only recursively descend directories
			   if they have names which consist of exactly two hex digits.
			   Because of the behavior of xp_file.c on Unix, anything else is
			   either not a directory, or not a directory which we use.
			 */
			if (! (d_len == 2 &&
				   ((d_name[0] >= '0' && d_name[0] <= '9') ||
					(d_name[0] >= 'A' && d_name[0] <= 'F')) &&
				   ((d_name[1] >= '0' && d_name[1] <= '9') ||
					(d_name[1] >= 'A' && d_name[1] <= 'F'))))
			  continue;
#endif /* !XP_UNIX */

			if(add_dir_prefix)
		  	  {
				sprintf(buffer, "%.250s/%.250s", dir_prefix, d_name);
		  	  }

    		if(status != -1)
		  	  {
				char *new_dir=0;
				StrAllocCopy(new_dir, cur_dir);
				if(cur_dir[PL_strlen(cur_dir)-1] != '/')
					StrAllocCat(new_dir, "/");
				StrAllocCat(new_dir, d_name);
				net_cache_recursive_file_finder(hash_table, 
												prefix,
												new_dir, 
												base_dir,
												buffer);
				PR_Free(new_dir);
		  	  }
			continue;
		  }

		if(add_dir_prefix)
		  {
			sprintf(buffer, "%.250s/%.250s", dir_prefix, d_name);
			XP_HashListAddObject(hash_table, PL_strdup(buffer));
		  }
		else
		  {
			XP_HashListAddObject(hash_table, PL_strdup(d_name));
		  }
		
	  }

	PR_Free(dir_prefix);

	PR_CloseDir(dir_ptr);
	return 0;
}
/* cleans up the cache directory by listing every
 * file and deleting the ones it doesn't know about
 * that begin with the cacheing prefix
 */
PUBLIC int
NET_CleanupCacheDirectory(char * dir_name, const char * prefix)
{
#define REMOVE_LIST_SIZE 1000
	DBT *remove_list[REMOVE_LIST_SIZE];
	char *filename, *d_name;
	XP_HashList *hash_table;
	DBT key, data;
	int status;
	char buffer[512];
	uint32 num_files;
	int i;
	int number_in_remove_list=0;
#ifdef NU_CACHE 
    /* PR_ASSERT(0);  Shouldn't be getting called */
    return 0;
#endif 
#ifdef XP_MAC

	/* make sure the size of the cache is as we want it
	 */
	if(net_MaxDiskCacheSize == 0)
		net_RemoveAllDiskCacheObjects();
	else
    	net_ReduceDiskCacheTo(NULL, net_MaxDiskCacheSize, 0);

	if(!dir_name)
		dir_name = "";

	/* optimization for cleanup.
	 *
	 * If the number of files on disk is within a couple
	 * of the number of files that we know about then
	 * skip the cache directory cleanup
	 */
	num_files = XP_FileNumberOfFilesInDirectory(dir_name, xpCache);
    if(num_files-3 <= net_NumberInDiskCache)
	    return(0);
#endif 

	if(!cache_database)
		return(-1);

	TRACEMSG(("cleaning up cache directory of unknown cache files"));

	hash_table = XP_HashListNew(1597, XP_StringHash, net_cache_strcmp);

	if(!hash_table)
		return(-1);
	
	/* find all the files in the cache directory 
	 */
	if(net_cache_recursive_file_finder(hash_table, 
									   prefix, 
									   dir_name, 
									   dir_name, 
									   buffer))
		return(-1);

	/* init the count of all the files in the database */
	num_files = 0;

	/* now run through the disk cache FAT and remove
	 * any matches from the hash table
	 */
    status = (*cache_database->seq)(cache_database, &key, &data, R_FIRST);

    while(status == 0)
      {
        filename = net_GetFilenameInCacheDBT(&data);
        if(filename)
          {
			time_t last_modified_date = net_GetTimeInCacheDBT(&data, 
												LAST_MODIFIED_BYTE_POSITION);

			if(!last_modified_date)
			  {
				/* if there is no last_modified date then
				 * it doesn't do us much good to keep the
				 * data since it can't be compared and used
				 * in a future session.  Put the file in a remove list
				 *
				 * Don't do this if the check mode is NEVER
				 *
				 */
		    	if(NET_CacheUseMethod != CU_NEVER_CHECK
				   && number_in_remove_list < REMOVE_LIST_SIZE)
					remove_list[number_in_remove_list++] = 
														net_CacheDBTDup(&key);
			  }
			else
			  {
				num_files++; /* keep a count of all the files in the database */

				d_name = (char*) XP_HashListRemoveObject(hash_table, filename);
				if(d_name)
			  	  {
					PR_Free(d_name);
			  	  }
			  }
            PR_Free(filename);
          }
	    else
		  {
			/* could not grab the filename from
			 * the DBT.  See if this is a valid
			 * entry
			 */
			if(!net_IsValidCacheDBT(&data)
				&& (key.size < 4 || PL_strncmp((char*)key.data,
                                                INTERNAL_NAME_PREFIX,
                                                4))
                && number_in_remove_list < REMOVE_LIST_SIZE)
			  {
				remove_list[number_in_remove_list++] = net_CacheDBTDup(&key);
			  }
		  }	

    	status = (*cache_database->seq)(cache_database, &key, &data, R_NEXT);
      }

	/* any files left in the hash table are not in the
	 * cache FAT, so delete them
	 *
	 * @@@@ Manipulate the hash table directly
	 */
	{
		int list_count;
		XP_List *list_ptr;
		
		for(list_count=0; list_count < hash_table->size; list_count++)
      	  {
        	list_ptr = hash_table->list[list_count];
        	if(list_ptr)
          	  {
            	while((filename = 
						    (char *) XP_ListRemoveTopObject(list_ptr)) != 0)
              	  {
#ifdef XP_OS2
					XP_LazyFileRemove(filename, xpCache);
#else
					NET_XP_FileRemove(filename, xpCache);
#endif
					TRACEMSG(("Unknown cache file %s found! -- deleteing...", 
														filename));
					PR_Free(filename);
              	  }
            	XP_ListDestroy(list_ptr);  /* free the list */
          	  }
      	  }

		XP_HashListDestroy(hash_table);
	}

	/* remove the entries that are no longer needed or
	 * are corrupt
	 *
	 * Note that we don't need to remove the files since
	 * they will already have been removed from the code
	 * above
	 */
    TRACEMSG(("Deleting %d unneeded cache entries",number_in_remove_list));
    for(i=0; i < number_in_remove_list; i++)
      {
        DBT *newkey = remove_list[i];
        if(-1 == (*cache_database->del)(cache_database, newkey, 0))
          {
            TRACEMSG(("Ack, error trying to delete corrupt entry, aborting",i));            break;
          }

        net_FreeCacheDBTdata(newkey);
      }

	/* if the number of files that we counted in the database
	 * does not agree with the number we think it should be
	 * reset the number we think it should be with the actual
	 * number
	 */
    if(net_NumberInDiskCache != num_files)
      {
		TRACEMSG(("Reseting number of files in cache to: %d", num_files));
        net_NumberInDiskCache = num_files;
	    net_StoreDiskCacheSize();  /* this will store the number of files */
	  }
	return(0);
}

/* removes all cache files from the cache directory
 * and deletes the cache FAT
 */
PUBLIC int
NET_DestroyCacheDirectory(char * dir_name, char * prefix)
{
#ifdef NU_CACHE
    PR_ASSERT(0); /* Shouldn't be getting called */
#endif
	net_RemoveAllDiskCacheObjects();
	NET_CleanupCacheDirectory(dir_name, prefix);

    (*cache_database->close)(cache_database);
    cache_database = 0;

	net_DiskCacheSize=0;
	net_NumberInDiskCache=0;
	
    return NET_XP_FileRemove("", xpCacheFAT);
}

/* create an HTML stream and push a bunch of HTML about
 * the cache
 */
MODULE_PRIVATE void 
NET_DisplayCacheInfoAsHTML(ActiveEntry * cur_entry)
#ifdef NU_CACHE
{
    NET_StreamClass *stream;
    char buffer[1024];
    
    StrAllocCopy(cur_entry->URL_s->content_type, TEXT_HTML);

    cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
    stream = NET_StreamBuilder(cur_entry->format_out, 
                   cur_entry->URL_s, 
                   cur_entry->window_id);

    if(!stream)
    {
        cur_entry->status = MK_UNABLE_TO_CONVERT;
        return;
    }
    
    CacheManager_InfoAsHTML(buffer);

    cur_entry->status = (*stream->put_block)(stream, buffer,PL_strlen(buffer));

    if(cur_entry->status < 0)
        (*stream->abort)(stream, cur_entry->status);
    else
        (*stream->complete)(stream);
}
#else
{
	char *buffer = (char*)PR_Malloc(2048);
	char *address;
	char *escaped;
   	NET_StreamClass * stream;
	net_CacheObject * cache_obj;
	DBT key, data;
	XP_Bool long_form = FALSE;
	int i;

	if(!buffer)
	  {
		cur_entry->status = MK_UNABLE_TO_CONVERT;
		return;
	  }

	if(PL_strcasestr(cur_entry->URL_s->address, "?long"))
		long_form = TRUE;
	else if(PL_strcasestr(cur_entry->URL_s->address, "?traceon"))
		NET_CacheTraceOn = TRUE;
	else if(PL_strcasestr(cur_entry->URL_s->address, "?traceoff"))
		NET_CacheTraceOn = FALSE;

	StrAllocCopy(cur_entry->URL_s->content_type, TEXT_HTML);

	cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
	stream = NET_StreamBuilder(cur_entry->format_out, 
							   cur_entry->URL_s, 
							   cur_entry->window_id);

	if(!stream)
	  {
		cur_entry->status = MK_UNABLE_TO_CONVERT;
		PR_Free(buffer);
		return;
	  }


	/* define a macro to push a string up the stream
	 * and handle errors
	 */
#define PUT_PART(part)													\
cur_entry->status = (*stream->put_block)(stream,			\
										part ? part : "Unknown",		\
										part ? PL_strlen(part) : 7);	\
if(cur_entry->status < 0)												\
  goto END;

	sprintf(buffer, 
"<TITLE>Information about the Netscape disk cache</TITLE>\n"
"<h2>Disk Cache statistics</h2>\n"
"<TABLE>\n"
"<TR>\n"
"<TD ALIGN=RIGHT><b>Maximum size:</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"<TR>\n"
"<TD ALIGN=RIGHT><b>Current size:</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"<TR>\n"
"<TD ALIGN=RIGHT><b>Number of files in cache:</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"<TR>\n"
"<TD ALIGN=RIGHT><b>Average cache file size:</TD>\n"
"<TD>%ld</TD>\n"
"</TR>\n"
"</TABLE>\n"
"<HR>",
net_MaxDiskCacheSize,
net_DiskCacheSize,
net_NumberInDiskCache,
net_NumberInDiskCache ? net_DiskCacheSize/net_NumberInDiskCache : 0);

	PUT_PART(buffer);

	if(!cache_database)
	  {
		PL_strcpy(buffer, "The cache database is currently closed");
		PUT_PART(buffer);
		goto END;
	  }

    if(0 != (*cache_database->seq)(cache_database, &key, &data, R_FIRST))
        return;

	/* define some macros to help us output HTML tables
	 */
#if 0

#define TABLE_TOP(arg1)				\
	sprintf(buffer, 				\
"<TR><TD ALIGN=RIGHT><b>%s</TD>\n"	\
"<TD>", arg1);						\
PUT_PART(buffer);

#define TABLE_BOTTOM				\
	sprintf(buffer, 				\
"</TD></TR>");						\
PUT_PART(buffer);

#else

#define TABLE_TOP(arg1)					\
	PL_strcpy(buffer, "<tt>");			\
	for(i=PL_strlen(arg1); i < 16; i++)	\
		PL_strcat(buffer, "&nbsp;");	\
	PL_strcat(buffer, arg1);			\
	PL_strcat(buffer, " </tt>");		\
	PUT_PART(buffer);

#define TABLE_BOTTOM					\
	PL_strcpy(buffer, "<BR>\n");		\
	PUT_PART(buffer);

#endif

    do
      {

		cache_obj = net_Fast_DBDataToCacheStruct(&data);

        if(!cache_obj)
		  {
			if(!net_IsValidCacheDBT(&data)
			 	&& (key.size < 4 || PL_strncmp((char*)key.data, 
												INTERNAL_NAME_PREFIX, 
												4)))
			  {
				
				PL_strcpy(buffer, "<H3>Corrupted or misversioned cache entry</H3>"
								  "This entry will be deleted automatically "
								  "in the future."
								  "<HR ALIGN=LEFT WIDTH=50%>");
				PUT_PART(buffer);
			  }
		  }
		else
          {
#if 0
			/* begin a table for this entry */
			PL_strcpy(buffer, "<TABLE>");
			PUT_PART(buffer);
#endif

			/* put the URL out there */
			/* the URL is 8 bytes into the key struct
         	 */
			address = (char *)key.data+8;

			TABLE_TOP("URL:");
			PL_strcpy(buffer, "<A TARGET=Internal_URL_Info HREF=\"about:");
			PUT_PART(buffer);
			escaped = NET_EscapeDoubleQuote(address);
			PUT_PART(escaped);
			PR_Free(escaped);
			PL_strcpy(buffer, "\">");
			PUT_PART(buffer);
			escaped = NET_EscapeHTML(address);
			PUT_PART(escaped);
			PR_Free(escaped);
			PL_strcpy(buffer, "</A>");
			PUT_PART(buffer);
			TABLE_BOTTOM;

			TABLE_TOP( XP_GetString(MK_CACHE_CONTENT_LENGTH) );
			sprintf(buffer, "%lu", cache_obj->content_length);
			PUT_PART(buffer);
			TABLE_BOTTOM;

			if(cache_obj->content_length != cache_obj->real_content_length)
			  {
				TABLE_TOP( XP_GetString(MK_CACHE_REAL_CONTENT_LENGTH) );
				sprintf(buffer, "%lu", cache_obj->real_content_length);
				PUT_PART(buffer);
				TABLE_BOTTOM;
			  }

			TABLE_TOP( XP_GetString(MK_CACHE_CONTENT_TYPE) );
			PUT_PART(cache_obj->content_type);
			TABLE_BOTTOM;

			TABLE_TOP( XP_GetString(MK_CACHE_LOCAL_FILENAME) );
			PUT_PART(cache_obj->filename);
			TABLE_BOTTOM;

			TABLE_TOP( XP_GetString(MK_CACHE_LAST_MODIFIED) );
			if(cache_obj->last_modified)
			  {
				PUT_PART(ctime(&cache_obj->last_modified));
			  }
			else
			  {
				PL_strcpy(buffer, "No date sent");
				PUT_PART(buffer);
			  }
			TABLE_BOTTOM;

			TABLE_TOP( XP_GetString(MK_CACHE_EXPIRES) );
			if(cache_obj->expires)
			  {
				PUT_PART(ctime(&cache_obj->expires));
			  }
			else
			  {
				PL_strcpy(buffer, "No expiration date sent");
				PUT_PART(buffer);
			  }
			TABLE_BOTTOM;

			if(long_form)
			  {
				TABLE_TOP( XP_GetString(MK_CACHE_LAST_ACCESSED) );
				PUT_PART(ctime(&cache_obj->last_accessed));
				TABLE_BOTTOM;

				TABLE_TOP( XP_GetString(MK_CACHE_CHARSET) );
				if(cache_obj->charset)
				  {
					PUT_PART(cache_obj->charset);
				  }
				else
				  {
					PL_strcpy(buffer, "iso-8859-1 (default)");
					PUT_PART(buffer);
				  }
				TABLE_BOTTOM;

				HG63287

				TABLE_TOP( XP_GetString(MK_CACHE_USES_RELATIVE_PATH) );
				sprintf(buffer, "%s", cache_obj->is_relative_path ? 
													"TRUE" : "FALSE");
				PUT_PART(buffer);
				TABLE_BOTTOM;

				TABLE_TOP( XP_GetString(MK_CACHE_FROM_NETSITE_SERVER) );
				sprintf(buffer, "%s", cache_obj->is_netsite ? 
													"TRUE" : "FALSE");
				PUT_PART(buffer);
				TABLE_BOTTOM;

				TABLE_TOP( "Lock date:" );
				if(cache_obj->lock_date)
			  	{
					PUT_PART(ctime(&cache_obj->lock_date));
			  	}
				else
			  	{
					PL_strcpy(buffer, "Not locked");
					PUT_PART(buffer);
			  	}
				TABLE_BOTTOM;
			  }
				

#if 0
			/* end the table for this entry */
			PL_strcpy(buffer, "</TABLE><HR ALIGN=LEFT WIDTH=50%>");
			PUT_PART(buffer);
#else
			PL_strcpy(buffer, "\n<P>\n");
			PUT_PART(buffer);
#endif
          }
	
	
      }
    while(0 == (*cache_database->seq)(cache_database, &key, &data, R_NEXT));

END:
	PR_Free(buffer);
	if(cur_entry->status < 0)
		(*stream->abort)(stream, cur_entry->status);
	else
		(*stream->complete)(stream);

	return;
}
#endif /* NU_CACHE */

#define SANE_BUFLEN	1024

#include "libmocha.h"

NET_StreamClass *
NET_CloneWysiwygCacheFile(MWContext *window_id, URL_Struct *URL_s,
			  uint32 nbytes, const char * wysiwyg_url, 
			  const char * base_href)
#ifdef NU_CACHE
{
    /*PR_ASSERT(0);*/ /* I hate GOTOs == Lou's programming */
    return NULL;
}
#else
{
	char *filename;
	PRCList *link;
	CacheDataObject *data_object;
	NET_StreamClass *stream;
	XP_File fromfp;
	int32 buflen, len;
	char *buf;
    int url_type = NET_URL_Type(URL_s->address);
	XP_FileType mode = xpCache;
    
    if (url_type == MAILBOX_TYPE_URL ||
        url_type == IMAP_TYPE_URL ||
        url_type == POP3_TYPE_URL)
        return NULL;

    filename = URL_s->cache_file;   
	if (!filename) 
	  {
		/* not hitting the cache -- check whether we're filling it */
		for (link = active_cache_data_objects.next;
			 link != &active_cache_data_objects;
			 link = link->next)
		  {
			data_object = (CacheDataObject *)link;
			if (data_object->URL_s == URL_s)
			  {
				XP_FileFlush(data_object->fp);
				filename = data_object->cache_object->filename;
				goto found;
			  }
		  }

		/* try memory cache and local filesystem */
		stream = net_CloneWysiwygMemCacheEntry(window_id, URL_s, nbytes,
						       wysiwyg_url, base_href);
		if (stream)
			return stream;
		return net_CloneWysiwygLocalFile(window_id, URL_s, nbytes, 
						 wysiwyg_url, base_href);
	  }
	else
	  {
		/* 
		 * If this is a local file URL_s->cache_file will have been
		 *   set by the code in mkfile.c so that the stream is treated
		 *   like a disk-cache stream.  But, in this case, we care that
		 *   they are different and need to preserve the fact that its
		 *   a local file URL
		 */
		if(NET_IsLocalFileURL(URL_s->address))
			mode = xpURL;
	  }

found:
	fromfp = NET_XP_FileOpen(filename, mode, XP_FILE_READ_BIN);
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
#endif

#ifdef NU_CACHE
PUBLIC PRBool
NET_IsURLInCache(const URL_Struct *URL_s)
{
    return CacheManager_Contains(URL_s->address);
}
#endif

#endif /* MOZILLA_CLIENT */
