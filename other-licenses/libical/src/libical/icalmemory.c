/* -*- Mode: C -*-
  ======================================================================
  FILE: icalmemory.c
  CREATOR: eric 30 June 1999
  
  $Id: icalmemory.c,v 1.3 2002/11/06 21:22:28 mostafah%oeone.com Exp $
  $Locker:  $
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is icalmemory.h

 ======================================================================*/

/* libical often passes strings back to the caller. To make these
 * interfaces simple, I did not want the caller to have to pass in a
 * memory buffer, but having libical pass out newly allocated memory
 * makes it difficult to de-allocate the memory.
 * 
 * The ring buffer in this scheme makes it possible for libical to pass
 * out references to memory which the caller does not own, and be able
 * to de-allocate the memory later. The ring allows libical to have
 * several buffers active simultaneously, which is handy when creating
 * string representations of components. */

#define ICALMEMORY_C

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#include "icalmemory.h"
#include "icalerror.h"

#include <stdio.h> /* for printf (debugging) */
#include <stdlib.h> /* for malloc, realloc */
#include <string.h> /* for memset(), strdup */
#ifdef XP_MAC
#include <extras.h> /* for strdup */
#endif

#define BUFFER_RING_SIZE 2500
#define MIN_BUFFER_SIZE 200

void icalmemory_free_tmp_buffer (void* buf);


/* HACK. Not threadsafe */
void* buffer_ring[BUFFER_RING_SIZE];
int buffer_pos = -1;
int initialized = 0;

/* Add an existing buffer to the buffer ring */
void  icalmemory_add_tmp_buffer(void* buf)
{
    /* I don't think I need this -- I think static arrays are
       initialized to 0 as a standard part of C, but I am not sure. */
    if (initialized == 0){
	int i;
	for(i=0; i<BUFFER_RING_SIZE; i++){
	    buffer_ring[i]  = 0;
	}
	initialized = 1;
    }

    /* Wrap around the ring */
    if(++buffer_pos == BUFFER_RING_SIZE){
	buffer_pos = 0;
    }

    /* Free buffers as their slots are overwritten */
    if ( buffer_ring[buffer_pos] != 0){
	free( buffer_ring[buffer_pos]);
	buffer_ring[buffer_pos] = 0;
    }

    /* Assign the buffer to a slot */
    buffer_ring[buffer_pos] = buf; 
}

/* Create a new temporary buffer on the ring. Libical owns these and
   wil deallocate them. */
void*
icalmemory_tmp_buffer (size_t size)
{
    char *buf;

    if (size < MIN_BUFFER_SIZE){
	size = MIN_BUFFER_SIZE;
    }
    
    buf = (void*)malloc(size);

    if( buf == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    memset(buf,0,size); 

    icalmemory_add_tmp_buffer(buf);

    return buf;
}

void icalmemory_free_ring()
{
	
   int i;
   for(i=0; i<BUFFER_RING_SIZE; i++){
    if ( buffer_ring[i] != 0){
       free( buffer_ring[i]);
    }
    buffer_ring[i]  = 0;
   }

   initialized = 1;

}



/* Like strdup, but the buffer is on the ring. */
char*
icalmemory_tmp_copy(const char* str)
{
    char* b = icalmemory_tmp_buffer(strlen(str)+1);

    strcpy(b,str);

    return b;
}
    

char* icalmemory_strdup(const char *s)
{
    return strdup(s);
}

void
icalmemory_free_tmp_buffer (void* buf)
{
   if(buf == 0)
   {
       return;
   }

   free(buf);
}


/* These buffer routines create memory the old fashioned way -- so the
   caller will have to delocate the new memory */

void* icalmemory_new_buffer(size_t size)
{
    void *b = malloc(size);

    if( b == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    memset(b,0,size); 

    return b;
}

void* icalmemory_resize_buffer(void* buf, size_t size)
{
    void *b = realloc(buf, size);

    if( b == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

   return b;
}

void icalmemory_free_buffer(void* buf)
{
    free(buf);
}

void 
icalmemory_append_string(char** buf, char** pos, size_t* buf_size, 
			      const char* string)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length, string_length;

#ifndef ICAL_NO_INTERNAL_DEBUG
    icalerror_check_arg_rv( (buf!=0),"buf");
    icalerror_check_arg_rv( (*buf!=0),"*buf");
    icalerror_check_arg_rv( (pos!=0),"pos");
    icalerror_check_arg_rv( (*pos!=0),"*pos");
    icalerror_check_arg_rv( (buf_size!=0),"buf_size");
    icalerror_check_arg_rv( (*buf_size!=0),"*buf_size");
    icalerror_check_arg_rv( (string!=0),"string");
#endif 

    string_length = strlen(string);
    data_length = (size_t)*pos - (size_t)*buf;    
    final_length = data_length + string_length; 

    if ( final_length >= (size_t) *buf_size) {

	
	*buf_size  = (*buf_size) * 2  + final_length;

	new_buf = realloc(*buf,*buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	*pos = new_pos;
	*buf = new_buf;
    }
    
    strcpy(*pos, string);

    *pos += string_length;
}


void 
icalmemory_append_char(char** buf, char** pos, size_t* buf_size, 
			      char ch)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length;

#ifndef ICAL_NO_INTERNAL_DEBUG
    icalerror_check_arg_rv( (buf!=0),"buf");
    icalerror_check_arg_rv( (*buf!=0),"*buf");
    icalerror_check_arg_rv( (pos!=0),"pos");
    icalerror_check_arg_rv( (*pos!=0),"*pos");
    icalerror_check_arg_rv( (buf_size!=0),"buf_size");
    icalerror_check_arg_rv( (*buf_size!=0),"*buf_size");
#endif

    data_length = (size_t)*pos - (size_t)*buf;

    final_length = data_length + 2; 

    if ( final_length > (size_t) *buf_size ) {

	
	*buf_size  = (*buf_size) * 2  + final_length +1;

	new_buf = realloc(*buf,*buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	*pos = new_pos;
	*buf = new_buf;
    }

    **pos = ch;
    *pos += 1;
    **pos = 0;
}
