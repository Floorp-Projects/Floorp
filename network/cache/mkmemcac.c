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
/* Module to do memory caching; storage and retreival
 * very simple hash table based scheme.
 *
 * designed and originally implemented by Lou Montulli 
 * modifications/additions by Gagan Saksena
 */

/* Please leave outside of ifdef for windows precompiled headers */
#include "rosetta.h"
#include "xp.h"

#ifdef MOZILLA_CLIENT

#include "mkcache.h"
#include "mkselect.h"
#include "netutils.h"
#include "mktcp.h"
#include "glhist.h"
#include "xp_hash.h"
#include "xp_mcom.h"
#include "client.h"
#include "mkgeturl.h"
#include "mkstream.h"
#include "extcache.h"
#include "mkmemcac.h"
extern void IL_SetCacheSize(uint32 new_size);
#include "prclist.h"
#include "prmem.h"
#include "plstr.h"
#include "shist.h"
#include "secnav.h"
#include "timing.h"

#ifdef NU_CACHE
#include "CacheStubs.h"
#endif

/* exported error ints */
extern int MK_OUT_OF_MEMORY;

/* the size of the cache segment output size
 */
#define NET_MEM_CACHE_OUTPUT_SIZE 2048

/* MAX_MEMORY_ALLOC_SIZE is the size of the largest malloc
 * used in this module
 * 
 * the NET_Socket_Buffer can never be larger than the
 * MAX_MEMORY_ALLOC_SIZE or this will fail
 */
#ifdef XP_WIN16
#define MAX_MEMORY_ALLOC_SIZE 32767 /* MAXINT */
#elif defined(XP_MAC)
#define MAX_MEMORY_ALLOC_SIZE 64*1024
#else
#define MAX_MEMORY_ALLOC_SIZE (((unsigned) (~0) << 1) >> 1)  /* MAXINT */

/* @@@@@@@@@ there is currently a limit on PR_Malloc to less than 32 K */
#undef  MAX_MEMORY_ALLOC_SIZE
#define MAX_MEMORY_ALLOC_SIZE 32767 /* MAXINT */
#endif

/* this is the minimum size of each of the memory segments used to hold
 * the cache object in memory
 */
#ifdef XP_MAC
#define MEMORY_CACHE_SEGMENT_SIZE 64*1024
#else
#define MEMORY_CACHE_SEGMENT_SIZE 2048
#endif

/* !!! structure is typedef'd to net_MemoryCacheObject
 * in net.h
 */
struct _net_MemoryCacheObject {
	XP_List         *list;
	net_CacheObject  cache_obj;
	int              external_locks;       /* locks set by other modules calling in */
	int              mem_read_lock;        /* the number of current readers */
    XP_Bool             delete_me;            /* set this to delete the object 
									        * once all readers are done 
									        */
	XP_Bool             current_page_only;    /* this is set when the document
											* requested not to be cached.
											* When set the document should
											* only be served if it's a
											* resize or print
											*/
	XP_Bool             completed;            /* whether or not we have completed the
											* caching of this object.  Initialized to
											* false, then set to true once the
											* complete function is called.
											*/
	XP_Bool			aborted;			   /* whether or not we have aborted the
										    * caching of this object.
											*/
};

/* structure to hold memory
 * segments
 */
typedef struct _net_MemorySegment {
	char      *segment;
	uint32     seg_size;
	uint32     in_use;
} net_MemorySegment;

/* the hash table pointer that holds all the memory cache
 * objects for quick lookup
 */
PRIVATE XP_HashList * net_MemoryCacheHashList = 0;

/* semaphore counter set when calling any of the list add functions
 */
PRIVATE int net_cache_adding_object=0;

/* a list of all the documents currently in
 * memory
 *
 * The net_MemoryCacheList is used as a delete queue
 * and is ordered by last-accessed time.
 */
PRIVATE XP_List   *net_MemoryCacheList=0;
PRIVATE uint32     net_MemoryCacheSize=0;
PRIVATE uint32     net_MaxMemoryCacheSize=0;

/* this object is used by the MemCacheConverter.
 * MemCacheConverter is a standard netlib stream and uses
 * this structure to hold data between invokations of
 * the write function and complete or abort.
 */
typedef struct _CacheDataObject {
	PRCList               links;
    NET_StreamClass       *next_stream;
    net_MemoryCacheObject *memory_copy;
	URL_Struct            *URL_s;
	uint32            	   computed_content_length;
} CacheDataObject;

/* PRIVATE XXX Mac CodeWarrior bug */ PRCList mem_active_cache_data_objects
	= PR_INIT_STATIC_CLIST(&mem_active_cache_data_objects);

/* free a segmented memory copy of an object
 * and the included net_cacheObject struct
 */
PRIVATE void
net_FreeMemoryCopy(net_MemoryCacheObject * mem_copy)
{
    net_MemorySegment * mem_seg;

	PR_ASSERT(mem_copy);

	if(!mem_copy)
		return;

	 /* If the stream was aborted while being written, and nobody is
	  * reading it, then it's ok to delete it now.  That is,
	  * we don't need to check if we want to set delete_me
	  */
	if (!((mem_copy->mem_read_lock == 0) && mem_copy->aborted))
	{
		/* if this object is currently being read or written DO NOT delete it.
		 * It will be deleted by another call to this function
		 * after it is finished being read.
		 */
		 if (((mem_copy->mem_read_lock > 0) && mem_copy->completed) ||
			!mem_copy->completed)
		  {
			TRACEMSG(("Seting delete_me flag on memory cache object"));
			mem_copy->delete_me = TRUE;

			/* remove it from the hash list so it won't be found
			 * by NET_GetURLInMemCache
			 */
			XP_HashListRemoveObject(net_MemoryCacheHashList, mem_copy);
			return;
		  }
	}

	TRACEMSG(("Freeing memory cache copy"));

	/* free and delete the memory segment list
	 */
    while((mem_seg = (net_MemorySegment *)XP_ListRemoveTopObject(mem_copy->list)) != NULL)
      {
		/* reduce the global memory cache size
		 */
		net_MemoryCacheSize -= mem_seg->seg_size;
        PR_Free(mem_seg->segment);
        PR_Free(mem_seg);
      }

    XP_ListDestroy(mem_copy->list);

	/* Remove it from the hash and delete lists,
	 * If the object isn't in these lists anymore
	 * the call will be ignored.
	 */
	XP_HashListRemoveObject(net_MemoryCacheHashList, mem_copy);
	XP_ListRemoveObject(net_MemoryCacheList, mem_copy);

    PR_FREEIF(mem_copy->cache_obj.address);
    PR_FREEIF(mem_copy->cache_obj.post_data);
    PR_FREEIF(mem_copy->cache_obj.post_headers);
    PR_FREEIF(mem_copy->cache_obj.content_type);
    PR_FREEIF(mem_copy->cache_obj.charset);
    PR_FREEIF(mem_copy->cache_obj.content_encoding);

	PR_Free(mem_copy);

}

/* removes the last mem object.  Returns negative on error,
 * otherwise returns the current size of the cache
 */
PRIVATE int32
net_remove_last_memory_cache_object(void)
{
	net_MemoryCacheObject * mem_cache_obj;
	
	/* safty valve in case there are no list items 
	 * or if we are in the process of adding an object
	 */
    if(net_cache_adding_object)
        return -1;

    mem_cache_obj = (net_MemoryCacheObject *) 
								XP_ListRemoveEndObject(net_MemoryCacheList);

    if(!mem_cache_obj)
        return -1;

	/* we can't remove it if we have external locks or if it's not completed -
	 * try removing another one 
	 */
	if(mem_cache_obj->external_locks || !mem_cache_obj->completed)
	  {
		int status = net_remove_last_memory_cache_object(); /* recurse */
		/* add the object back in */
		XP_ListAddObject(net_MemoryCacheList, mem_cache_obj);
		return status;
	  }

    net_FreeMemoryCopy(mem_cache_obj);

	return((int32) net_MemoryCacheSize);
}

/* this function free's objects to bring the cache size
 * below the size passed in
 */
PRIVATE void
net_ReduceMemoryCacheTo(uint32 size)
{
    /* safty valve in case we are in the process of adding an object
     */
    if(net_cache_adding_object)
        return;

	while(net_MemoryCacheSize > size)
	  {
		if(0 > net_remove_last_memory_cache_object())
			break;
	  }
}

/* set the size of the Memory cache.
 * Set it to 0 if you want cacheing turned off
 * completely
 */
#define HTML_CACHE_MAX_BASE_SIZE 1024l*1024l
#define HTML_CACHE_VARIABLE_GROWTH .05   /* the percentage to add */
PUBLIC void
NET_SetMemoryCacheSize(int32 new_size)
{
	int32 image_cache_size;
	int32 html_cache_size;

	if(new_size <= 0)
	  {
    	IL_SetCacheSize(0);
    	net_ReduceMemoryCacheTo(0);
		net_MaxMemoryCacheSize = 0;
		return;
	  }

	/* the netlib mem cache is set to .3*new_size 
	 * or 200K + .05 * new_size, whichever is less.
	 * and imagelb uses the rest 
	 */
	if(new_size * .3 < HTML_CACHE_MAX_BASE_SIZE)
	  {
		/* use .3 of new_size */
		html_cache_size = (int32) (.3 * new_size);
	  }
	else
	  {
		/* use HTML_CACHE_MAX_BASE_SIZE + .05*new_size */
		html_cache_size = HTML_CACHE_MAX_BASE_SIZE + 
						  (int32)(HTML_CACHE_VARIABLE_GROWTH*
						   (new_size - HTML_CACHE_MAX_BASE_SIZE));
	  }
	
	image_cache_size = new_size - html_cache_size;

	net_MaxMemoryCacheSize = html_cache_size;
	
    net_ReduceMemoryCacheTo((uint32) html_cache_size);

	/* set the image cache to be the rest
	 */
    IL_SetCacheSize(image_cache_size);

	return;
}

/* Remove the last memory cache object if one exists
 * Returns the total size of the memory cache in bytes
 * after performing the operation
 */
PUBLIC int32
NET_RemoveLastMemoryCacheObject()
{
	net_remove_last_memory_cache_object();

	return(net_MemoryCacheSize);
}

/* returns the number of bytes currently in use by the Memory cache
 */
PUBLIC int32
NET_GetMemoryCacheSize()
{
    return(net_MemoryCacheSize);
}

PUBLIC int32
NET_GetMaxMemoryCacheSize()
{
    return net_MaxMemoryCacheSize;
}

/* compare entries for the hashing functions
 *
 * return 0 if match or positive or negative on
 * non match
 */
PRIVATE int net_CacheHashComp(net_MemoryCacheObject * obj1,
                                 net_MemoryCacheObject * obj2)
{
    int result;
	char *hash1;
	char *hash2;
	char *ques1=0;
	char *ques2=0;
	char *ques3=0;
	char *ques4=0;

    if(obj1->cache_obj.method != obj2->cache_obj.method)
        return(obj1->cache_obj.method - obj2->cache_obj.method);

	/* If this is a "news:" or "snews:" URL, then any search data in the URL
	 * (the stuff after '?') should be ignored for cache-comparison purposes,
	 * so that "news:MSGID" and "news:MSGID?headers=all" share the same cache
	 * file.
	 */
	if((NET_TO_UPPER(obj1->cache_obj.address[0]) == 'N'
		|| NET_TO_UPPER(obj1->cache_obj.address[0]) == 'S')
	    &&  NET_URL_Type(obj1->cache_obj.address) == NEWS_TYPE_URL)
	  {
		ques1 = PL_strchr(obj1->cache_obj.address, '?');
		if(ques1) 
			*ques1 = '\0';
	  }

    if((NET_TO_UPPER(obj2->cache_obj.address[0]) == 'N'
        || NET_TO_UPPER(obj2->cache_obj.address[0]) == 'S')
        &&  NET_URL_Type(obj2->cache_obj.address) == NEWS_TYPE_URL)
      {
        ques2 = PL_strchr(obj2->cache_obj.address, '?');
        if(ques2)
            *ques2 = '\0';
      }


	/* do the same for IMAP */
	if(!PL_strncasecmp(obj1->cache_obj.address,"mailbox://",10))
	  {
		ques3 = PL_strstr(obj1->cache_obj.address, "&part=");
		if(ques3) 
			*ques3 = '\0';
	  }

	if(!PL_strncasecmp(obj2->cache_obj.address,"mailbox://",10))
      {
        ques4 = PL_strstr(obj2->cache_obj.address, "&part=");
        if(ques4)
            *ques4 = '\0';
      }


	/* strip hash symbols because they
	 * really represent the same
	 * document
	 */
	hash1 = PL_strchr(obj1->cache_obj.address, '#');
	hash2 = PL_strchr(obj2->cache_obj.address, '#');

	if(hash1)
		*hash1 = '\0';
	if(hash2)
		*hash2 = '\0';

    result = PL_strcmp(obj1->cache_obj.address, obj2->cache_obj.address);

	/* set them back to previous values
	 */
	if(hash1)
		*hash1 = '#';
	if(hash2)
		*hash2 = '#';
	if(ques1)
		*ques1 = '?';
	if(ques2)
		*ques2 = '?';
	if(ques3)
		*ques3 = '&';
	if(ques4)
		*ques4 = '&';


    if(result != 0)
       return(result);

    if(!obj1->cache_obj.post_data && !obj2->cache_obj.post_data)
       return(0);  /* match; no post data on either */

    if(obj1->cache_obj.post_data && obj2->cache_obj.post_data)
      {
        result = PL_strcmp(obj1->cache_obj.post_data, 
						   obj2->cache_obj.post_data);
        return(result);
      }
    else
      {  /* one but not the other */
        if(obj1->cache_obj.post_data)
           return(1);
        else
           return(-1);
      }

    return(0);  /* match with post data */
}

/* hashing function for url's
 *
 * This is some magic Jamie gave me...
 */
PRIVATE uint32 net_CacheHashFunc(net_MemoryCacheObject * obj1)
{
    unsigned const char *x;
    uint32 h = 0;
    uint32 g;
	XP_Bool news_type_url = FALSE;
	XP_Bool imap_type_url = FALSE;
 
	if(!obj1)
		return 0;

    x = (unsigned const char *) obj1->cache_obj.address;

	/* figure out if it's a news type URL */
    if((NET_TO_UPPER(obj1->cache_obj.address[0]) == 'N'
        || NET_TO_UPPER(obj1->cache_obj.address[0]) == 'S')
        &&  NET_URL_Type(obj1->cache_obj.address) == NEWS_TYPE_URL)
		news_type_url = TRUE;
	/* figure out if it's an IMAP type URL */
	else if (NET_TO_UPPER(obj1->cache_obj.address[0]) == 'M' &&
		!PL_strncasecmp(obj1->cache_obj.address,"Mailbox://",10))
		imap_type_url = TRUE;

    /* modify the default String hash function
     * to work with URL's
     */
    assert(x);

    if (!x) return 0;

	/* ignore '#' data for all URL's.
 	 * ignore '?' data for news URL's
	 * ignore '&' data for all IMAP URL's
	 */
    while (*x != 0 && *x != '#' && (!news_type_url || *x != '?') &&
		(!imap_type_url || *x != '&'))
      {
        h = (h << 4) + *x++;
        if ((g = h & 0xf0000000) != 0)
          h = (h ^ (g >> 24)) ^ g;
      }
 
    return h;
}

/******************************************************************
 * Cache Converter Stream input routines
 */

/* is the stream ready for writing?
 */
PRIVATE unsigned int net_MemCacheWriteReady (NET_StreamClass *stream)
{
   CacheDataObject *obj=stream->data_object;   
   if(obj->next_stream)
       return((*obj->next_stream->is_write_ready)(obj->next_stream));
   else
	   return(MAX_WRITE_READY);
}

/*  stream write function
 *
 * this function accepts a stream and writes the data
 * into memory segments on a segment list.
 */
PRIVATE int net_MemCacheWrite (NET_StreamClass *stream, CONST char* buffer, int32 len)
{
	CacheDataObject *obj=stream->data_object;	
	TRACEMSG(("net_MemCacheWrite called with %ld buffer size", len));

	/* if delete_me is set, we don't want to keep writing to the memcache stream.
     */
	if(obj->memory_copy && !obj->memory_copy->delete_me)
	  {
		net_MemorySegment * mem_seg;
		char * cur_mem_seg_ptr;

		/* compute a content_length as we read data 
		 * to use as a comparison and for URL's that
		 * don't send a content type
		 */
		obj->computed_content_length += len;

		/* we are always adding to the end one in the list so 
		 * lets get it
		 */
		mem_seg = (net_MemorySegment *)
							XP_ListGetEndObject(obj->memory_copy->list);
		PR_ASSERT(mem_seg);
	
		if(mem_seg->in_use+len > mem_seg->seg_size)
		  {
			/* we need a new segment
			 */
			net_MemorySegment * new_mem_seg = PR_NEW(net_MemorySegment);
			uint32 size_left_in_old_buffer = mem_seg->seg_size - mem_seg->in_use;
			uint32 size_for_new_buffer = len - size_left_in_old_buffer;
			char * new_mem_seg_ptr;
		
			if(!new_mem_seg)
			  {
				net_FreeMemoryCopy(obj->memory_copy);
				obj->memory_copy = 0;
				goto EndOfMemWrite;
			  }

			/* @@@ the socket buffer can never be larger
			 * than the MAX_MEMORY_ALLOC_SIZE
			 */
			if(size_for_new_buffer > MEMORY_CACHE_SEGMENT_SIZE)
			  {
				new_mem_seg->segment = (char*)PR_Malloc(size_for_new_buffer);
				new_mem_seg->seg_size = size_for_new_buffer;
			  }
			else
			  {
				new_mem_seg->segment = (char*)PR_Malloc(
													MEMORY_CACHE_SEGMENT_SIZE);
				new_mem_seg->seg_size = MEMORY_CACHE_SEGMENT_SIZE;
			  }

			if(!new_mem_seg->segment)
			  {
				PR_Free(new_mem_seg);
				net_FreeMemoryCopy(obj->memory_copy);
				obj->memory_copy = 0;
				goto EndOfMemWrite;
			  }

			/* increase the global cache size counter
			 */
			net_MemoryCacheSize += new_mem_seg->seg_size;

			TRACEMSG(("Cache size now: %d", net_MemoryCacheSize));

			cur_mem_seg_ptr = mem_seg->segment;
			new_mem_seg_ptr = new_mem_seg->segment;

			/* fill in what we can to the old buffer
			 */
			if(size_left_in_old_buffer)
			  {
				memcpy(cur_mem_seg_ptr+mem_seg->in_use, 
						  buffer, 
						  (size_t) size_left_in_old_buffer);
				mem_seg->in_use = mem_seg->seg_size;  /* seg new size */
			  }

			/* fill in the new buffer
			 */
			memcpy(new_mem_seg_ptr, 
					  buffer+size_left_in_old_buffer, 
					  (size_t) size_for_new_buffer);
			new_mem_seg->in_use = size_for_new_buffer;

			net_cache_adding_object++; /* semaphore */
			XP_ListAddObjectToEnd(obj->memory_copy->list, new_mem_seg);
			net_cache_adding_object--; /* semaphore */

			TRACEMSG(("Adding %d to New memory segment %p", len, new_mem_seg));
	      }
		else
		  {
		  	assert (mem_seg->segment);
			cur_mem_seg_ptr = mem_seg->segment;

			/* fill in some more into the existing segment
			 */
			memcpy(cur_mem_seg_ptr + mem_seg->in_use, buffer, (size_t) len);
			mem_seg->in_use += len;

			TRACEMSG(("Adding %d to existing memory segment %p", len, mem_seg));
		  }

	  }

EndOfMemWrite:  /* target of a goto from an error above */

    if(obj->next_stream)
	  {
    	int status=0;
	
		PR_ASSERT (buffer);
		PR_ASSERT (len > -1);
        status = (*obj->next_stream->put_block)
									(obj->next_stream, buffer, len);

		/* abort */
	    if(status < 0)
			return(status);
	  }

    return(1);
}

/*  complete the stream
*/
PRIVATE void net_MemCacheComplete (NET_StreamClass *stream)
{
	CacheDataObject *obj=stream->data_object;	
	/* refresh these entries in case meta changes them */
	if(obj->URL_s && obj->memory_copy)
	  {
		obj->memory_copy->cache_obj.expires       = obj->URL_s->expires;
    	obj->memory_copy->cache_obj.last_modified = obj->URL_s->last_modified;
    	StrAllocCopy(obj->memory_copy->cache_obj.charset, obj->URL_s->charset);
	  }

	if (obj->memory_copy)
	{
		/* now it's completed */
		obj->memory_copy->completed = TRUE;
		
		/* if the object is zero size or 
	 	 * if the computed size is different that a given content type,
	 	 * zero the last modified date so that it wont get reused
	 	 * on a recheck or reload
	 	 */
		if(!obj->computed_content_length
		   ||  (obj->memory_copy->cache_obj.content_length
				&& obj->memory_copy->cache_obj.content_length 
				   != obj->computed_content_length
			   ) 
		  )
	  	  {
			obj->memory_copy->cache_obj.last_modified = 0;
	  	  }

		/* override the given content_length with the correct
		 * content length
		 */
		obj->memory_copy->cache_obj.content_length = 
												obj->computed_content_length;
	
 		/* set current time */
		obj->memory_copy->cache_obj.last_accessed = time(NULL); 
	}


/* loser: */
	/* complete the next stream
     */
    if(obj->next_stream)
	  {
	    (*obj->next_stream->complete)(obj->next_stream);
        PR_Free(obj->next_stream);
	  }

	/* finally, check to see if we want to delete this, after going
	   through all that work just to put it in here.  */
    if(obj->memory_copy &&
		obj->memory_copy->delete_me && 
		(obj->memory_copy->mem_read_lock == 0))
        {
          net_FreeMemoryCopy(obj->memory_copy);
        }


	/* don't need the data obj any more since this is the end 
	 * of the stream... */
	PR_REMOVE_LINK(&obj->links);
    PR_Free(obj);

	/* this is what keeps the cache from growing without end */
	net_ReduceMemoryCacheTo(net_MaxMemoryCacheSize);

	return;
}

/*  add the object into the cache, even if it hasn't been finished
    getting cached.  When it's finished, net_MemCacheComplete will
	set the obj->memory_copy->comlpleted flag.
*/
PRIVATE void net_MemCacheAddObjectToCache (CacheDataObject * obj)
{
    int status;

	/* if the hash table doesn't exist yet, initialize it now 
	 */
    if(!net_MemoryCacheHashList)
      {
 
        net_MemoryCacheHashList = XP_HashListNew(499, 
									(XP_HashingFunction) net_CacheHashFunc,
                                    (XP_HashCompFunction) net_CacheHashComp); 
        if(!net_MemoryCacheHashList)
          {
			net_FreeMemoryCopy(obj->memory_copy);
			goto loser;
            return;
          }

      }

	/* if the delete list doesn't exist yet, initialize it now 
	 */
	if(!net_MemoryCacheList)
	  {
		net_MemoryCacheList = XP_ListNew();
		if(!net_MemoryCacheList)
		  {
			net_FreeMemoryCopy(obj->memory_copy);
			goto loser;
			return;
		  }
	  }

	/* memory copy could have been free'd and clear if an
 	 * error occured in the write, so check to make sure
	 * it's still around.
	 */
    if(obj->memory_copy)
      {
		/* set the completed flag so that we know we're in
		 * the process of caching this object.
		 */
		obj->memory_copy->completed = FALSE;
		obj->memory_copy->aborted = FALSE;

		/* if the document requested not to be cached, 
		 * set the flag to only serve it for reloads and such
		 */
		if(obj->URL_s && obj->URL_s->dont_cache)
			obj->memory_copy->current_page_only = TRUE;
		
		/* add the struct to the delete list */
		net_cache_adding_object++; /* semaphore */
        XP_ListAddObject(net_MemoryCacheList, obj->memory_copy);
		net_cache_adding_object--; /* semaphore */

		/* add the struct to the hash list */
		net_cache_adding_object++; /* semaphore */
        status = XP_HashListAddObject(net_MemoryCacheHashList, 
									  obj->memory_copy);
		net_cache_adding_object--; /* semaphore */

		/* check for hash collision */
		if(status == XP_HASH_DUPLICATE_OBJECT)
		  {
			net_MemoryCacheObject * tmp_obj;

            tmp_obj = (net_MemoryCacheObject *)
                          		XP_HashListFindObject(net_MemoryCacheHashList, 
													  obj->memory_copy);

			if ((tmp_obj->mem_read_lock == 0) && tmp_obj->completed)
			{
				/* If there is nobody currently reading or writing the
				 * duplicate hash entry, then free the old copy.
				 * This will remove it from the hash list, also.
				 */
				net_FreeMemoryCopy(tmp_obj);
   
			    TRACEMSG(("Found duplicate object in cache.  Removing old object"));
	
				/* re add the object 
				 */
				net_cache_adding_object++; /* semaphore */
				XP_HashListAddObject(net_MemoryCacheHashList, 
									 obj->memory_copy);
				net_cache_adding_object--; /* semaphore */
			}
			else
			{
				/* We can't cache this entry, the duplicate is
				 * currently being read or written.  So delete this entry.
				 */
				net_FreeMemoryCopy(obj->memory_copy);
			}
		  }
      }

loser:

    return;
}

/* Abort the stream 
 * 
 */
PRIVATE void net_MemCacheAbort (NET_StreamClass *stream, int status)
{
	CacheDataObject *obj=stream->data_object;	
    /* abort the next stream
     */
    if(obj->next_stream)
	  {
	    (*obj->next_stream->abort)(obj->next_stream, status);
        PR_Free(obj->next_stream);
	  }

	/* set the aborted flag so that the object will really be deleted
	 * if there are no read locks. */
	obj->memory_copy->aborted = TRUE;
	net_FreeMemoryCopy(obj->memory_copy);

	PR_REMOVE_LINK(&obj->links);
    PR_Free(obj);

    return;
}

/* setup the stream
 */
MODULE_PRIVATE NET_StreamClass * 
NET_MemCacheConverter (FO_Present_Types format_out,
                    void       *converter_obj,
                    URL_Struct *URL_s,
                    MWContext  *window_id)
{
    CacheDataObject * data_object=0;
    NET_StreamClass * stream=0;
	net_MemorySegment * mem_seg=0;
	net_MemoryCacheObject *memory_copy=0;
	net_MemCacheConverterObject *mem_conv_obj = converter_obj;
    
    TRACEMSG(("Setting up cache stream. Have URL: %s\n", URL_s->address));

	TRACEMSG(("Setting up memory cache"));

	/* check to see if the object is larger than the
	 * size of the max memory cache 
	 *
	 * skip the size check if the must_cache flag is set.
	 */
	if(!URL_s->must_cache && (uint32)URL_s->content_length >= net_MaxMemoryCacheSize)
		goto malloc_failure; /* dont cache this URL */

	/* set up the memory caching struct.
 	 *
	 * malloc the segment first since it's the most likely malloc to fail
	 */
	mem_seg = PR_NEW(net_MemorySegment);

	if(!mem_seg)
		return(NULL);

	mem_seg->in_use = 0;

	if(URL_s->content_length > 0)
	  {
		/* use the content_length if we can since that would be
		 * the most efficient 
		 */
		if(URL_s->content_length > MAX_MEMORY_ALLOC_SIZE)
		  {
		    mem_seg->segment =  (char*)PR_Malloc(MAX_MEMORY_ALLOC_SIZE);
	        mem_seg->seg_size = MAX_MEMORY_ALLOC_SIZE;
		  }
		else
		  {
		    /* add 10 just in case it's needed due to a bad content type :) */
		    mem_seg->segment =  (char*)PR_Malloc(URL_s->content_length+10);
	        mem_seg->seg_size = URL_s->content_length+10;
		  }
	  }
	else
	  {
		/* this is a case of no content length, use standard size segments */
		mem_seg->segment  = (char*)PR_Malloc(MEMORY_CACHE_SEGMENT_SIZE);
	    mem_seg->seg_size = MEMORY_CACHE_SEGMENT_SIZE;
	  }

	if(!mem_seg->segment)
	  {
		PR_Free(mem_seg);
		goto malloc_failure;  /* skip memory cacheing */
	  }

	/* malloc the main cache holding structure */
	memory_copy = PR_NEW(net_MemoryCacheObject);
	if(!memory_copy)
      {
		PR_Free(mem_seg);
		PR_Free(mem_seg->segment);
		goto malloc_failure;  /* skip memory cacheing */
      }
	memset(memory_copy, 0, sizeof(net_MemoryCacheObject));

	memory_copy->list = XP_ListNew();
	if(!memory_copy->list)
	  {
		PR_Free(mem_seg);
		PR_Free(mem_seg->segment);
		PR_Free(memory_copy);
		goto malloc_failure;  /* skip memory cacheing */
	  }

	/* if we get this far add the object to the size count
	 * since if we fail anywhere below here it will get
	 * subtracted by net_FreeMemoryCopy(memory_copy);
	 */
    net_MemoryCacheSize += mem_seg->seg_size;

	/* add the segment malloced above to the segment list */
	net_cache_adding_object++; /* semaphore */
	XP_ListAddObject(memory_copy->list, mem_seg);
	net_cache_adding_object--; /* semaphore */

    StrAllocCopy(memory_copy->cache_obj.address, URL_s->address);
    memory_copy->cache_obj.method = URL_s->method;
    if(URL_s->post_data)
      {
		memory_copy->cache_obj.post_data_size = URL_s->post_data_size;
        StrAllocCopy(memory_copy->cache_obj.post_data, URL_s->post_data);
        StrAllocCopy(memory_copy->cache_obj.post_headers, URL_s->post_headers);
      }

    memory_copy->cache_obj.expires        = URL_s->expires;
    memory_copy->cache_obj.last_modified  = URL_s->last_modified;
   
    memory_copy->cache_obj.content_length      = URL_s->content_length;
    memory_copy->cache_obj.real_content_length = URL_s->content_length;
    memory_copy->cache_obj.is_netsite     = URL_s->is_netsite;

    StrAllocCopy(memory_copy->cache_obj.content_type, URL_s->content_type);
    StrAllocCopy(memory_copy->cache_obj.charset, URL_s->charset);
    StrAllocCopy(memory_copy->cache_obj.content_encoding, 
				 URL_s->content_encoding);

	/* check for malloc failure on critical fields above */
	if(!memory_copy->cache_obj.address || !memory_copy->cache_obj.content_type)
	  {
		net_FreeMemoryCopy(memory_copy);
		goto malloc_failure;  /* skip memory cacheing */
	  }
    
	HG83363

	/* build the stream object */
    stream = PR_NEW(NET_StreamClass);
    if(!stream)
	  {
		net_FreeMemoryCopy(memory_copy);
		goto malloc_failure;  /* skip memory cacheing */
	  }

	/* this structure gets passed back into the write, complete
	 * and abort stream functions
	 */
    data_object = PR_NEW(CacheDataObject);
    if (!data_object)
      {
        PR_Free(stream);
		net_FreeMemoryCopy(memory_copy);
		goto malloc_failure;  /* skip memory cacheing */
      }

    /* init the object */
    memset(data_object, 0, sizeof(CacheDataObject));

	/* assign the cache object to the stream data object
	 */
    data_object->memory_copy = memory_copy;

	if (mem_conv_obj->dont_hold_URL_s == FALSE)
		data_object->URL_s = URL_s;

    TRACEMSG(("Returning stream from NET_CacheConverter\n"));

    stream->name           = "Cache stream";
    stream->complete       = (MKStreamCompleteFunc) net_MemCacheComplete;
    stream->abort          = (MKStreamAbortFunc) net_MemCacheAbort;
    stream->put_block      = (MKStreamWriteFunc) net_MemCacheWrite;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_MemCacheWriteReady;
    stream->data_object    = data_object;  /* document info object */
    stream->window_id      = window_id;

	/* next stream is passed in by the caller but can be null */
    data_object->next_stream = mem_conv_obj->next_stream;

	PR_APPEND_LINK(&data_object->links, &mem_active_cache_data_objects);

	/* add the cache object to the hash list, but don't
	 * complete it yet
	 */
   	net_MemCacheAddObjectToCache(data_object);

	return stream;

malloc_failure: /* target of malloc failure */

	TRACEMSG(("NOT Caching this URL"));

	/* bypass the whole cache mechanism
	 */
	if(format_out != FO_CACHE_ONLY)
      {
        format_out = CLEAR_CACHE_BIT(format_out);
        return(mem_conv_obj->next_stream);
	  }

	return(NULL);
}

PRIVATE net_MemoryCacheObject *
net_FindObjectInMemoryCache(URL_Struct *URL_s)
{
	net_MemoryCacheObject tmp_cache_obj, *return_obj;

	/* fill in the temporary cache object so we can
	 * use it for searching
	 */
	memset(&tmp_cache_obj, 0, sizeof(tmp_cache_obj));
	tmp_cache_obj.cache_obj.method    = URL_s->method;
	tmp_cache_obj.cache_obj.address   = URL_s->address;
	tmp_cache_obj.cache_obj.post_data = URL_s->post_data;
	tmp_cache_obj.cache_obj.post_data_size = URL_s->post_data_size;

	return_obj = (net_MemoryCacheObject *)
		XP_HashListFindObject(net_MemoryCacheHashList, &tmp_cache_obj);
	if (return_obj)
		return (return_obj);
	else 
	{
		/*	try unescaping the URL address - MHTML parts are
			escaped, and weren't getting picked up in the
			cache */
		tmp_cache_obj.cache_obj.address = PL_strdup(URL_s->address);
		if (tmp_cache_obj.cache_obj.address)
		{
			/* unescape it */
			NET_UnEscape(tmp_cache_obj.cache_obj.address);
			if (tmp_cache_obj.cache_obj.address)
			{
				return_obj = (net_MemoryCacheObject *)
					XP_HashListFindObject(net_MemoryCacheHashList, &tmp_cache_obj);
				PR_Free(tmp_cache_obj.cache_obj.address);
			}
		}
		return (return_obj);
	}
}

/* set or unset a lock on a memory cache object
 */
MODULE_PRIVATE void
NET_ChangeMemCacheLock(URL_Struct *URL_s, PRBool set)
{
	net_MemoryCacheObject * found_cache_obj;

	/* look up cache struct */
	found_cache_obj = net_FindObjectInMemoryCache(URL_s);

	if(found_cache_obj && found_cache_obj->completed)
	  {
		PR_ASSERT(found_cache_obj->external_locks >= 0);

		if(set)
		  {
			/* increment lock counter */
			found_cache_obj->external_locks++;
		  }
		else
		  {
			/* decrement lock counter */
			found_cache_obj->external_locks--;

/*			PR_ASSERT(found_cache_obj->external_locks >= 0); */

			if(found_cache_obj->external_locks < 0)
				found_cache_obj->external_locks = 0;
		  }
	  }
}

/* remove a URL from the memory cache
 */
MODULE_PRIVATE void
NET_RemoveURLFromMemCache(URL_Struct *URL_s)
{
	net_MemoryCacheObject * found_cache_obj;

	found_cache_obj = net_FindObjectInMemoryCache(URL_s);

	if(found_cache_obj &&
		(found_cache_obj->mem_read_lock == 0) &&
		found_cache_obj->completed)
		net_FreeMemoryCopy(found_cache_obj);
}

/* Returns TRUE if the URL is currently in the
 * memory cache and false otherwise.
 */
PUBLIC XP_Bool
NET_IsURLInMemCache(URL_Struct *URL_s)
{
	if(net_FindObjectInMemoryCache(URL_s))
		return(TRUE);
	else
		return(FALSE);
}

/* this function looks up a URL in the hash table and
 * returns MEMORY_CACHE_TYPE_URL if it's found and
 * should be loaded via the memory cache.  
 *
 * It also sets several entries in the passed in URL
 * struct including "memory_copy" which is a pointer
 * to the found net_MemoryCacheObject struct.
 */
MODULE_PRIVATE int
NET_FindURLInMemCache(URL_Struct * URL_s, MWContext *ctxt)
#ifdef NU_CACHE
{
	PR_ASSERT(0); /* Should not be getting called.  Also, compiler will warn of missing prototype */
	return 0;
}
#else
{
	net_MemoryCacheObject *found_cache_obj;

	TRACEMSG(("Checking for URL in cache"));

    if(!net_MemoryCacheHashList)
		return(0);

	found_cache_obj = net_FindObjectInMemoryCache(URL_s);

    if(found_cache_obj)
      {
        TRACEMSG(("mkcache: found URL in memory cache!"));

		if(found_cache_obj->current_page_only)
		  {
			History_entry * he = SHIST_GetCurrent(&ctxt->hist);
			int hist_num;

			/* if the current_page_only flag is set 
			 * then the page should only be
			 * served if it's the current document loaded
			 * (i.e. reloads)
			 *
			 * @@@ if !he then let it through, this allows the
			 *     doc info window to work
			 */
			if(he)
			  {
				hist_num = SHIST_GetIndex(&ctxt->hist, he);

				/* we can tell if the document being loaded is the
			 	 * same as the current document by looking up it's
			 	 * index in the history.  If they don't match
			 	 * then it isn't the current document
			 	 */
				if(URL_s->history_num != hist_num)
					return 0;
			  }
		  }

		/* set a pointer to the structure in the URL struct
		 */
		URL_s->memory_copy = found_cache_obj;

        /* copy the contents of the URL struct so that the content type
         * and other stuff gets recorded
         */
        StrAllocCopy(URL_s->content_type,     
					 found_cache_obj->cache_obj.content_type);
        StrAllocCopy(URL_s->charset,          
					 found_cache_obj->cache_obj.charset);
        StrAllocCopy(URL_s->content_encoding, 
					 found_cache_obj->cache_obj.content_encoding);
        URL_s->content_length      = found_cache_obj->cache_obj.content_length;
        URL_s->real_content_length = found_cache_obj->cache_obj.content_length;
 		URL_s->expires             = found_cache_obj->cache_obj.expires;
 		URL_s->last_modified       = found_cache_obj->cache_obj.last_modified;
 		URL_s->is_netsite          = found_cache_obj->cache_obj.is_netsite;

		HG26557
		net_cache_adding_object++; /* semaphore */
		/* reorder objects so that the list is in last accessed order */
		XP_ListRemoveObject(net_MemoryCacheList, found_cache_obj);
		XP_ListAddObject(net_MemoryCacheList, found_cache_obj);
		net_cache_adding_object--; /* semaphore */

        TIMING_MESSAGE(("cache,%s,in memory", URL_s->address));
		TRACEMSG(("Cached copy is valid. returning method"));

		return(MEMORY_CACHE_TYPE_URL);
      }

	TRACEMSG(("URL not found in cache"));

    return(0);
}
#endif

/*****************************************************************
 *  Memory cache output module routine
 *
 * This set of routines pushes the document in memory up a stream
 * created by NET_StreamBuilder
 */

/* used to hold data between invokations of ProcessNet
 */
typedef struct _MemCacheConData {
	XP_List         *cur_list_ptr;
	uint32           bytes_written_in_segment;
	NET_StreamClass *stream;
} MemCacheConData;

#define CD_CUR_LIST_PTR  connection_data->cur_list_ptr
#define CD_BYTES_WRITTEN_IN_SEGMENT connection_data->bytes_written_in_segment
#define CD_STREAM        connection_data->stream

#define CE_URL_S          cur_entry->URL_s
#define CE_WINDOW_ID      cur_entry->window_id
#define CE_FORMAT_OUT     cur_entry->format_out
#define CE_STATUS         cur_entry->status
#define CE_BYTES_RECEIVED cur_entry->bytes_received

/* begin the load, This is called from NET_GetURL
 */
#ifdef OLD_MOZ_MAIL_NEWS
extern int
net_InitializeNewsFeData (ActiveEntry * cur_entry);
extern int
IMAP_InitializeImapFeData (ActiveEntry * cur_entry);
extern void
IMAP_URLFinished(URL_Struct *URL_s);
#endif /* OLD_MOZ_MAIL_NEWS */

PRIVATE int32
net_MemoryCacheLoad (ActiveEntry * cur_entry)
#ifdef NU_CACHE
{
	PR_ASSERT(0); /* Should not be getting called */
	return 0;
}
#else
{
    /* get memory for Connection Data */
    MemCacheConData * connection_data = PR_NEW(MemCacheConData);
	net_MemorySegment * mem_seg;
	uint32  chunk_size;
	char   *mem_seg_ptr;
	char   *first_buffer;

	TRACEMSG(("Entering NET_MemoryCacheLoad!\n"));

	if(!connection_data)
	  {
		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		CE_STATUS = MK_OUT_OF_MEMORY;
		return (CE_STATUS);
	  }

	if (!CE_URL_S->memory_copy)
	{
		/* the memory_copy has been freed before fully
		 * making it into the cache.  In other words, it's
		 * been aborted.  Abort this load then, also.
		 * We should probably never hit this case, however.
		 * If we got here, it means we've just found this entry
		 * in the hash table, which means we have not deleted it
		 * and we have not set the delete_me flag.
		 */
		PR_ASSERT(FALSE);
		CE_STATUS = MK_OBJECT_NOT_IN_CACHE;
		return (CE_STATUS);
	}

    cur_entry->protocol = MEMORY_CACHE_TYPE_URL;
	cur_entry->memory_file = TRUE;

	/* point to the first list struct that contains data
	 */
	CD_CUR_LIST_PTR = CE_URL_S->memory_copy->list->next;
	CD_BYTES_WRITTEN_IN_SEGMENT = 0;

	/* put a read lock on the data
	 */
	CE_URL_S->memory_copy->mem_read_lock++;

	cur_entry->con_data = connection_data;

	cur_entry->local_file = TRUE;

	cur_entry->socket = NULL;

	NET_SetCallNetlibAllTheTime(CE_WINDOW_ID, "mkmemcac");

    cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
	
	FE_EnableClicking(CE_WINDOW_ID);

#ifdef OLD_MOZ_MAIL_NEWS    
    if (cur_entry->format_out == FO_PRESENT)
    {
      if (NET_URL_Type(cur_entry->URL_s->address) == NEWS_TYPE_URL)
      {
        /* #### DISGUSTING KLUDGE to make cacheing work for news articles. */
        cur_entry->status = net_InitializeNewsFeData (cur_entry);
        if (cur_entry->status < 0)
          {
            /* #### what error message? */
            return cur_entry->status;
          }
      }
      else if (!PL_strncmp(CE_URL_S->address, "Mailbox://", 10))
      {
        /* #### DISGUSTING KLUDGE to make cacheing work for imap articles. */
        cur_entry->status = IMAP_InitializeImapFeData (cur_entry);
        if (cur_entry->status < 0)
          {
            /* #### what error message? */
            return cur_entry->status;
          }
      }
    }
#endif /* OLD_MOZ_MAIL_NEWS */      

	/* open the outgoing stream
	 */
    CD_STREAM = NET_StreamBuilder(CE_FORMAT_OUT, CE_URL_S, CE_WINDOW_ID);
	if(!CD_STREAM)
	  {
        NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mkmemcac");

		PR_Free(connection_data);

		CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
        CE_STATUS = MK_UNABLE_TO_CONVERT;
        return (CE_STATUS);
	  }

	if (!CE_URL_S->load_background)
        FE_GraphProgressInit(CE_WINDOW_ID, CE_URL_S, CE_URL_S->content_length);

	/* process one chunk of the
	 * cache file so that
	 * layout can continue
	 * when images are in the cache
	 */
#define FIRST_BUFF_SIZE 1024
	if (CE_URL_S->memory_copy->completed)
	{
		mem_seg = (net_MemorySegment *) CD_CUR_LIST_PTR->object;

		mem_seg_ptr = mem_seg->segment;

		chunk_size = MIN(FIRST_BUFF_SIZE, 	
						 mem_seg->in_use-CD_BYTES_WRITTEN_IN_SEGMENT);

		/* malloc this first buffer because we can't use
 		 * the NET_SocketBuffer in calls from NET_GetURL 
		 * because of reentrancy
		 */
		first_buffer = (char *) PR_Malloc(chunk_size);

		if(!first_buffer)
		  {
			PR_Free(connection_data);
			CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
			return(MK_OUT_OF_MEMORY);
		  }

		/* copy the segment because the parser will muck with it
		 */
		memcpy(first_buffer,
				  mem_seg_ptr+CD_BYTES_WRITTEN_IN_SEGMENT,
				  (size_t) chunk_size);

		CD_BYTES_WRITTEN_IN_SEGMENT += chunk_size;

		CE_STATUS = (*CD_STREAM->put_block)(CD_STREAM,
											first_buffer,
											chunk_size);
		if(CE_STATUS < 0)
		  {
			NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mkmemcac");

			if (!CE_URL_S->load_background)
				FE_GraphProgressDestroy(CE_WINDOW_ID,
										CE_URL_S,
										CE_URL_S->content_length,
										CE_BYTES_RECEIVED);

			PR_Free(connection_data);

			return (CE_STATUS);
		  }

		CE_BYTES_RECEIVED += chunk_size;

		/* check to see if we need to advance the pointer yet.
		 * should hardly ever happen here since the first buffer
		 * is so small
		 */
		if(CD_BYTES_WRITTEN_IN_SEGMENT >= mem_seg->in_use)
		  {
			CD_CUR_LIST_PTR = CD_CUR_LIST_PTR->next;
			CD_BYTES_WRITTEN_IN_SEGMENT = 0;
		  }

		PR_Free(first_buffer);
	}
	else
	{
		CE_STATUS = 0;
	}

    return(CE_STATUS);
	
}
#endif /* NU_CACHE */

/* called repeatedly from NET_ProcessNet to push all the
 * data up the stream
 */
PRIVATE int32
net_ProcessMemoryCache (ActiveEntry * cur_entry)
#ifdef NU_CACHE
{
	PR_ASSERT(0); /* Should not be getting called */
	return 0;
}
#else
{
	MemCacheConData * connection_data = (MemCacheConData *)cur_entry->con_data;
	net_MemorySegment * mem_seg;
    uint32  chunk_size;
	uint32  buffer_size;
	char  *mem_seg_ptr;


	if (!CE_URL_S->memory_copy)
	{
		/* the memory_copy has been freed before fully
		 * making it into the cache.  In other words, it's
		 * been aborted.  Abort this load then, also,
		 * since it's a concurrent load.
		 * We know that CE_URL_S->memory_copy will be NULL
		 * if it's been removed, because we have a read lock
		 * on the entry, and that will cause the delete_me
		 * flag to be set, rather than just deleting the
		 * object initially.  We catch the delete_me flag
		 * here, and set CE_URL_S->memory_copy to NULL.
		 */
		CE_STATUS = MK_OBJECT_NOT_IN_CACHE;
		return (CE_STATUS);
	}


	/* wait until the object has been fully inserted into
	 * the cache.
	 */
	if (!CE_URL_S->memory_copy->completed &&
		!(CE_URL_S->memory_copy->delete_me &&
		  CE_URL_S->memory_copy->aborted))
	{
		/* normal case - we are still waiting for it to complete */
		return 0;
	}

	if(!CD_CUR_LIST_PTR ||
		(CE_URL_S->memory_copy->aborted &&
		  CE_URL_S->memory_copy->delete_me))
	  {
		/* when CD_CUR_LIST_PTR turns NULL we are at the end
 		 * of the document.  Finish up the stream and exit.
		 *
		 * If the entry is aborted and the delete_me flag is set,
		 * the stream was aborted in the middle of being written.
		 * If we are here, then it's simultaneously being read also.
		 * So, finish up.
		 */

		/* complete the stream
		 */
		(*CD_STREAM->complete)(CD_STREAM);

		/* free the stream
		 */
		PR_Free(CD_STREAM);

		/* remove the read lock
         */
        CE_URL_S->memory_copy->mem_read_lock--;

		/* check to see if we should delete this memory cached object
		 *
		 * delete_me is set by FreeMemoryCacheObject when it tried
		 * to free it but couldn't because of read locks on the
		 * object
         */
        if(CE_URL_S->memory_copy->delete_me && 
                    CE_URL_S->memory_copy->mem_read_lock == 0)
          {
            net_FreeMemoryCopy(CE_URL_S->memory_copy);
            CE_URL_S->memory_copy = 0;
          }

		/* PR_Free the structs used by this protocol module
	     */
		PR_Free(connection_data);

		/* set the status to success */
		if (CE_URL_S->memory_copy && CE_URL_S->memory_copy->completed)
			CE_STATUS = MK_DATA_LOADED;
		else
			CE_STATUS = MK_OBJECT_NOT_IN_CACHE;

		/* clear the CallNetlibAllTheTime if there are no
		 * other readers
		 */
	    NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mkmemcac");

        if (!CE_URL_S->load_background)
            FE_GraphProgressDestroy(CE_WINDOW_ID,
                                    CE_URL_S,
                                    CE_URL_S->content_length,
                                    CE_BYTES_RECEIVED);

#ifdef OLD_MOZ_MAIL_NEWS
	if (!PL_strncmp(CE_URL_S->address, "Mailbox://", 10))
        /* #### DISGUSTING KLUDGE to make cacheing work for imap articles. */
        IMAP_URLFinished(CE_URL_S);
#endif
		/* Tell ProcessNet that we are all done */
		return(-1);
	  }

	/* CD_CUR_LIST_PTR is pointing to the most current
	 * memory segment list object
 	 */
    mem_seg = (net_MemorySegment *) CD_CUR_LIST_PTR->object;

	TRACEMSG(("ProcessMemoryCache: printing segment %p",mem_seg));

	mem_seg_ptr = mem_seg->segment;

	/* write out at least part of the buffer
	 */
	buffer_size = (*CD_STREAM->is_write_ready)(CD_STREAM);
    buffer_size = MIN(buffer_size, (unsigned int) NET_Socket_Buffer_Size);

	/* make it ??? at the most
	 * when coming out of the cache
	 */
	buffer_size = MIN(buffer_size, NET_MEM_CACHE_OUTPUT_SIZE);

    chunk_size = MIN(buffer_size, mem_seg->in_use-CD_BYTES_WRITTEN_IN_SEGMENT);

    /* copy the segment because the parser will muck with it
     */
    memcpy(NET_Socket_Buffer, 
			  mem_seg_ptr+CD_BYTES_WRITTEN_IN_SEGMENT, 
			  (size_t) chunk_size);

	/* remember how much of this segment we have written */
    CD_BYTES_WRITTEN_IN_SEGMENT += chunk_size;

    CE_STATUS = (*CD_STREAM->put_block)(CD_STREAM,
                                        NET_Socket_Buffer,
                                        chunk_size);
	CE_BYTES_RECEIVED += chunk_size;

	/* check to see if we need to advance the pointer yet
	 */
	if(CD_BYTES_WRITTEN_IN_SEGMENT >= mem_seg->in_use)
	  {
	    CD_CUR_LIST_PTR = CD_CUR_LIST_PTR->next;
        CD_BYTES_WRITTEN_IN_SEGMENT = 0;
	  }

	if (!CE_URL_S->load_background)
        FE_GraphProgress(CE_WINDOW_ID, CE_URL_S, CE_BYTES_RECEIVED,
                         chunk_size, CE_URL_S->content_length);

	if(CE_STATUS < 0)
	  {
        NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mkmemcac");

	    (*CD_STREAM->abort)(CD_STREAM, CE_STATUS);
	  }

	return(CE_STATUS);

}
#endif /* NU_CACHE */

/* called by functions in mkgeturl to interrupt the loading of
 * an object.  (Usually a user interrupt) 
 */
PRIVATE int32
net_InterruptMemoryCache (ActiveEntry * cur_entry)
#ifdef NU_CACHE
{
	PR_ASSERT(0); /* Should not be getting called */
	return 0;
}
#else
{
	MemCacheConData * connection_data = (MemCacheConData *)cur_entry->con_data;

	/* abort and free the outgoing stream
	 */
	(*CD_STREAM->abort)(CD_STREAM, MK_INTERRUPTED);
	PR_Free(CD_STREAM);

	if (!CE_URL_S->memory_copy)
	{
		/* the memory_copy has been freed before fully
		 * making it into the cache.  In other words, it's
		 * been aborted.  Abort this load then, also,
		 * since it's a concurrent load.
		 */
		CE_STATUS = MK_OBJECT_NOT_IN_CACHE;
		return (CE_STATUS);
	}
    
	/* remove the read lock
     */
    CE_URL_S->memory_copy->mem_read_lock--;

    /* check to see if we should delete this memory cached object
     */
    if(CE_URL_S->memory_copy->delete_me && 
			CE_URL_S->memory_copy->mem_read_lock == 0)
      {
        net_FreeMemoryCopy(CE_URL_S->memory_copy);
        CE_URL_S->memory_copy = 0;
      }

	/* PR_Free the structs
	 */
	PR_Free(connection_data);
	CE_STATUS = MK_INTERRUPTED;

    NET_ClearCallNetlibAllTheTime(CE_WINDOW_ID, "mkmemcac");

	return(CE_STATUS);
}
#endif

/* create an HTML stream and push a bunch of HTML about
 * the memory cache
 */
MODULE_PRIVATE void 
NET_DisplayMemCacheInfoAsHTML(ActiveEntry * cur_entry)
{
	char *buffer = (char*)PR_Malloc(2048);
	char *address;
	char *escaped;
   	NET_StreamClass * stream;
	net_CacheObject * cache_obj;
    net_MemoryCacheObject * mem_cache_obj;
	XP_Bool long_form = FALSE;
	int32 number_in_memory_cache;
	XP_List *list_ptr;
	int i;

	if(!buffer)
	  {
		cur_entry->status = MK_UNABLE_TO_CONVERT;
		return;
	  }

	if(PL_strcasestr(cur_entry->URL_s->address, "?long"))
		long_form = TRUE;
	else if(PL_strcasestr(cur_entry->URL_s->address, "?traceon"))
#ifdef NU_CACHE
		CacheTrace_Enable(TRUE);
#else
		NET_CacheTraceOn = TRUE;
#endif
	else if(PL_strcasestr(cur_entry->URL_s->address, "?traceoff"))
#ifdef NU_CACHE
		CacheTrace_Enable(FALSE);
#else
		NET_CacheTraceOn = FALSE;
#endif

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

	if(!net_MemoryCacheList)
	  {
		PL_strcpy(buffer, "There are no objects in the memory cache");
		PUT_PART(buffer);
		goto END;
	  }

	number_in_memory_cache = XP_ListCount(net_MemoryCacheList);

	/* add the header info */
	sprintf(buffer, 
"<TITLE>Information about the Netscape memory cache</TITLE>\n"
"<h2>Memory Cache statistics</h2>\n"
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
net_MaxMemoryCacheSize,
net_MemoryCacheSize,
number_in_memory_cache,
number_in_memory_cache ? net_MemoryCacheSize/number_in_memory_cache : 0);

	PUT_PART(buffer);

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

	list_ptr = net_MemoryCacheList;

    while((mem_cache_obj = (net_MemoryCacheObject *) XP_ListNextObject(list_ptr)) != NULL)
      {

		cache_obj = &mem_cache_obj->cache_obj;
		address = PL_strdup(mem_cache_obj->cache_obj.address);

		/* put the URL out there */
		TABLE_TOP("URL:");
		PL_strcpy(buffer, "<A TARGET=Internal_URL_Info HREF=\"about:");
		PUT_PART(buffer);
		escaped = NET_EscapeDoubleQuote(address);
		PUT_PART(escaped);
		PR_Free(escaped);
		XP_STRCPY(buffer, "\">");
		PUT_PART(buffer);
		escaped = NET_EscapeHTML(address);
		PUT_PART(escaped);
		PR_Free(address);
		PR_Free(escaped);
		PL_strcpy(buffer, "</A>");
		PUT_PART(buffer);
		TABLE_BOTTOM;

		TABLE_TOP("Content Length:");
		sprintf(buffer, "%lu", cache_obj->content_length);
		PUT_PART(buffer);
		TABLE_BOTTOM;

		TABLE_TOP("Content type:");
		PUT_PART(cache_obj->content_type);
		TABLE_BOTTOM;

		TABLE_TOP("Last Modified:");
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

		TABLE_TOP("Expires:");
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

		TABLE_TOP("Last accessed:");
		PUT_PART(ctime(&cache_obj->last_accessed));
		TABLE_BOTTOM;

		TABLE_TOP("Character set:");
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

		HG27328

		PL_strcpy(buffer, "\n<P>\n");
		PUT_PART(buffer);
	
	
      }

END:
	PR_Free(buffer);
	if(cur_entry->status < 0)
		(*stream->abort)(stream, cur_entry->status);
	else
		(*stream->complete)(stream);

	return;
}

/*Accessor for use by Cache Browser */
PUBLIC net_CacheObject* 
NET_FirstMemCacheObject(XP_List* list_ptr) 
{
	if (net_MemoryCacheList)
	{
		list_ptr = net_MemoryCacheList;
		return NET_NextMemCacheObject(list_ptr);
	}
	else 
		return 0;
}

/*Accessor for use by Cache Browser */
PUBLIC net_CacheObject*
NET_NextMemCacheObject(XP_List* list_ptr)
{
	if (list_ptr)
	{
		net_MemoryCacheObject * tmp = (net_MemoryCacheObject *) XP_ListNextObject(list_ptr);
		if (tmp)
		{
			return &tmp->cache_obj;
		}
	}
	return 0;
}

#include "libmocha.h"

NET_StreamClass *
net_CloneWysiwygMemCacheEntry(MWContext *window_id, URL_Struct *URL_s,
			      uint32 nbytes, const char * wysiwyg_url,
			      const char * base_href)
#ifdef NU_CACHE
{
	PR_ASSERT(0);
	return NULL;
}
#else
{
	net_MemoryCacheObject *memory_copy;
	PRCList *link;
	CacheDataObject *data_object;
	NET_StreamClass *stream;
	XP_List *list;
	net_MemorySegment *seg;
	uint32 len;

	if (!(memory_copy = URL_s->memory_copy))
	  {
		/* not hitting the cache -- check whether we're filling it */
		for (link = mem_active_cache_data_objects.next;
			 link != &mem_active_cache_data_objects;
			 link = link->next)
		  {
			data_object = (CacheDataObject *)link;
			if (data_object->URL_s == URL_s &&
				(memory_copy = data_object->memory_copy) != NULL)
			  {
				goto found;
			  }
		  }
		return NULL;
	  }

found:
	stream = LM_WysiwygCacheConverter(window_id, URL_s, 
					  wysiwyg_url, base_href);
	if (!stream)
		return 0;
	list = memory_copy->list;
	while (nbytes != 0 &&
		   (seg = (net_MemorySegment *) XP_ListNextObject(list)) != NULL)
	  {
		len = seg->seg_size;
		if (len > nbytes)
			len = nbytes;
		if (stream->put_block(stream, seg->segment,
							  (int32)len) < 0)
			break;
		nbytes -= len;
	  }
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

PRIVATE void
net_CleanupMemoryCacheProtocol(void)
{
}

void
NET_InitMemCacProtocol(void) /* no prototype when NU_CACHE */
{
    static NET_ProtoImpl mem_cac_proto_impl;
#ifdef NU_CACHE
    PR_ASSERT(0);
    return;
#endif
    mem_cac_proto_impl.init = net_MemoryCacheLoad;
    mem_cac_proto_impl.process = net_ProcessMemoryCache;
    mem_cac_proto_impl.interrupt = net_InterruptMemoryCache;
    mem_cac_proto_impl.resume = NULL;
    mem_cac_proto_impl.cleanup = net_CleanupMemoryCacheProtocol;

    NET_RegisterProtocolImplementation(&mem_cac_proto_impl, MEMORY_CACHE_TYPE_URL);
}

#ifdef NU_CACHE
typedef struct _NuCacheConData {
    void*           cache_object;
    NET_StreamClass *stream;
} NuCacheConData;

#define FIRST_BUFF_SIZE 1024

PRIVATE int32
net_NuCacheLoad (ActiveEntry * cur_entry)
{
    if (cur_entry && cur_entry->URL_s)
    {
        NuCacheConData* con_data = PR_NEW(NuCacheConData);
        void* pObject = cur_entry->URL_s->cache_object;

        if (!con_data || !pObject)
        {
            cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
            cur_entry->status = MK_OUT_OF_MEMORY;
            return cur_entry->status;
        }

        cur_entry->protocol = NU_CACHE_TYPE_URL;
        cur_entry->memory_file = (CacheObject_GetModule(pObject) == 0); /* TODO replace 0 with the enum */
    
        /* CacheObject->SetReadLock(pObject); */
        cur_entry->local_file = TRUE; /* Check about this one- TODO */
        cur_entry->con_data = con_data;
        cur_entry->socket = NULL;

        NET_SetCallNetlibAllTheTime(cur_entry->window_id, "nucache");
        cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
        FE_EnableClicking(cur_entry->window_id);
        
        if (0==cur_entry->format_out) 
        {
            /* This means that this is a CACHE_ONLY request and shouldn't 
             * be getting read from memory anyway. So mark it as not present in memory
             * and return as such. */
		    cur_entry->status = MK_OBJECT_NOT_IN_CACHE;
            return cur_entry->status;
        }

        /* Build the stream to read data from */
        con_data->stream = NET_StreamBuilder(cur_entry->format_out, cur_entry->URL_s, cur_entry->window_id);
        if(!con_data->stream)
        {
            NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");
            PR_DELETE(con_data);
            cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
            cur_entry->status = MK_UNABLE_TO_CONVERT;
            return cur_entry->status;
        }

        if (!cur_entry->URL_s->load_background)
            FE_GraphProgressInit(cur_entry->window_id, cur_entry->URL_s, cur_entry->URL_s->content_length);
    
        /* Process the first chunk so that images can start loading */
        if (!CacheObject_IsPartial(pObject)) /*todo- change this to is Completed */
        {
            char* firstBuffer = (char*) PR_Malloc(FIRST_BUFF_SIZE);
            PRInt32 amountRead = 0;
            if (!firstBuffer)
            {
                PR_DELETE(con_data);
                cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
                return(MK_OUT_OF_MEMORY);
            }

            amountRead = CacheObject_Read(pObject, firstBuffer, FIRST_BUFF_SIZE);
            if (amountRead > 0)
            {
                cur_entry->status = (*con_data->stream->put_block)(con_data->stream, firstBuffer, amountRead);
                if(cur_entry->status < 0)
                {
                    NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");

                    if (!cur_entry->URL_s->load_background)
                    {
                        FE_GraphProgressDestroy(cur_entry->window_id,
                            cur_entry->URL_s,
                            cur_entry->URL_s->content_length,
                            cur_entry->bytes_received);
                    }
                    PR_DELETE(con_data);
                    return (cur_entry->status);
                }

                cur_entry->bytes_received += amountRead;
                PR_DELETE(firstBuffer);
            }
        }
        else
        {
            cur_entry->status = 0;
        }
    }
    return cur_entry->status;
}

/* called repeatedly from NET_ProcessNet to push all the
 * data up the stream
 */
PRIVATE int32
net_ProcessNuCache (ActiveEntry * cur_entry)
{
    if (cur_entry)
    {
        PRUint32  amountRead;
        PRUint32  buffer_size;

        NuCacheConData* con_data = (NuCacheConData*) cur_entry->con_data;
        void* pObject = cur_entry->URL_s->cache_object;

        NET_SetCallNetlibAllTheTime(cur_entry->window_id, "nucache");

        if (!pObject)
        {
            cur_entry->status = MK_OBJECT_NOT_IN_CACHE;
            return cur_entry->status;
        }

        /* Wait for the object to complete */
        if (!CacheObject_GetIsCompleted(pObject))
            return 0;
#if 0
         /* If the object has been aborted or expired */
        if ((CACHEOBJECT_ABORTED == CacheObject_GetState(pObject)) ||
            (CACHEOBJECT_EXPIRED == CacheObject_GetState(pObject)))
        {
            (*con_data->stream->complete)(con_data->stream);
            PR_Free(con_data->stream);
            /* CacheObject->UnsetReadLock(pObject); */
            PR_Free(con_data);

            cur_entry->status = MK_DATA_LOADED;
            NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");
            if (!cur_entry->URL_s->load_background)
                FE_GraphProgressDestroy(
                    cur_entry->window_id,
                    cur_entry->URL_s,
                    cur_entry->URL_s->content_length,
                    cur_entry->bytes_received);
            return -1;
        }
#endif
        /* Get the buffer size that the stream is ready to receive */
        buffer_size = (*con_data->stream->is_write_ready)(con_data->stream);
        
        /* if the stream doesn't want any data, continue */
        if (0 == buffer_size)
        {
            cur_entry->status = 1;
            return cur_entry->status;
        }
        
        /* Use the minimum from these factors. 
         * Not sure why we are doing this part. Will look into this later- Gagan
         */
        buffer_size = MIN(buffer_size, (unsigned int) NET_Socket_Buffer_Size);
        buffer_size = MIN(buffer_size, NET_MEM_CACHE_OUTPUT_SIZE); 

        amountRead = CacheObject_Read(pObject, NET_Socket_Buffer, buffer_size);
        if (amountRead > 0)
        {
            cur_entry->status = (*con_data->stream->put_block)(con_data->stream, NET_Socket_Buffer, amountRead);
            
            /* Update the progress control */
            if (!cur_entry->URL_s->load_background)
                FE_GraphProgress(
                    cur_entry->window_id, 
                    cur_entry->URL_s, 
                    cur_entry->bytes_received,
                    amountRead, 
                    cur_entry->URL_s->content_length);

            if(cur_entry->status < 0)
            {
                NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");

                if (!cur_entry->URL_s->load_background)
                {
                    FE_GraphProgressDestroy(cur_entry->window_id,
                        cur_entry->URL_s,
                        cur_entry->URL_s->content_length,
                        cur_entry->bytes_received);
                }
                PR_Free(con_data);
                return (cur_entry->status);
            }

            cur_entry->bytes_received += amountRead;
        }
        else  /* we didn't read anything, must be the end. */
        {
            (*con_data->stream->complete)(con_data->stream);
            PR_Free(con_data->stream);
            /* Reset read locks, reset streams, etc. */
            CacheObject_Reset(pObject);

            PR_Free(con_data);

            cur_entry->status = MK_DATA_LOADED;
            NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");
            if (!cur_entry->URL_s->load_background)
                FE_GraphProgressDestroy(
                    cur_entry->window_id,
                    cur_entry->URL_s,
                    cur_entry->URL_s->content_length,
                    cur_entry->bytes_received);
            return -1;
        }
   
        return cur_entry->status; 
    }
    return -1;
}

/* called by functions in mkgeturl to interrupt the loading of
 * an object.  (Usually a user interrupt) 
 */
PRIVATE int32
net_InterruptNuCache (ActiveEntry * cur_entry)
{
    if (cur_entry)
    {
        NuCacheConData* con_data = (NuCacheConData*) cur_entry->con_data;
        void* pObject = cur_entry->URL_s->cache_object;

        (*con_data->stream->abort)(con_data->stream, MK_INTERRUPTED);
        PR_Free(con_data->stream);
        /* Reset read locks, reset streams, etc. */
        CacheObject_Reset(pObject);

        PR_Free(con_data);

        NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");

        cur_entry->status = MK_INTERRUPTED;

        return cur_entry->status;
    }
    return -1;
}


PRIVATE void
net_CleanupNuCacheProtocol(void)
{
}

void
NET_InitNuCacheProtocol(void)
{
    static NET_ProtoImpl nu_cache_proto_impl;

    nu_cache_proto_impl.init = net_NuCacheLoad;
    nu_cache_proto_impl.process = net_ProcessNuCache;
    nu_cache_proto_impl.interrupt = net_InterruptNuCache;
    nu_cache_proto_impl.resume = NULL;
    nu_cache_proto_impl.cleanup = net_CleanupNuCacheProtocol;

    NET_RegisterProtocolImplementation(&nu_cache_proto_impl, NU_CACHE_TYPE_URL);
}
#endif /* NU_CACHE */
#endif /* MOZILLA_CLIENT */
