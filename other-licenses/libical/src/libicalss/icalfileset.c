/* -*- Mode: C -*-
  ======================================================================
  FILE: icalfileset.c
  CREATOR: eric 23 December 1999
  
  $Id: icalfileset.c,v 1.12 2004/09/17 20:44:21 mostafah%oeone.com Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


 ======================================================================*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icalfileset.h"
#include "icalgauge.h"
#include <errno.h>
#ifndef XP_MAC
#include <sys/stat.h> /* for stat */
#ifndef WIN32
#include <unistd.h> /* for stat, getpid */
#else
#include <io.h>
#include <share.h>
#endif
#endif
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* for fcntl */
#ifdef XP_MAC
#include <extras.h>
#endif
#include "icalfilesetimpl.h"

#ifdef WIN32
#define snprintf	_snprintf
#define strcasecmp	stricmp

#define _S_ISTYPE(mode, mask)  (((mode) & _S_IFMT) == (mask))

#define S_ISDIR(mode)    _S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode)    _S_ISTYPE((mode), _S_IFREG)
#endif

extern int errno;

int icalfileset_lock(icalfileset *cluster);
int icalfileset_unlock(icalfileset *cluster);
icalerrorenum icalfileset_read_file(icalfileset* cluster, mode_t mode);
int icalfileset_filesize(icalfileset* cluster);

icalerrorenum icalfileset_create_cluster(const char *path);

icalfileset* icalfileset_new_impl()
{
    struct icalfileset_impl* impl;
  
    if ( ( impl = (struct icalfileset_impl*)
	   malloc(sizeof(struct icalfileset_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	errno = ENOMEM;
	return 0;
    }
  
    memset(impl,0,sizeof(struct icalfileset_impl));
  
    strcpy(impl->id,ICALFILESET_ID);
  
    return impl;
}


icalfileset* icalfileset_new(const char* path)
{
    return icalfileset_new_open(path, O_RDWR|O_CREAT, 0664);
}

icalfileset* icalfileset_new_reader(const char* path)
{
    return icalfileset_new_open(path, O_RDONLY, 0644);
}

icalfileset* icalfileset_new_writer(const char* path)
{
    return icalfileset_new_open(path, O_WRONLY|O_CREAT|O_TRUNC, 0664);
}

icalfileset* icalfileset_new_open(const char* path, int flags, mode_t mode)
{
    struct icalfileset_impl *impl = icalfileset_new_impl(); 
    struct icaltimetype tt;
    off_t cluster_file_size;

    memset(&tt,0,sizeof(struct icaltimetype));

    icalerror_clear_errno();
    icalerror_check_arg_rz( (path!=0), "path");

    if (impl == 0){
	return 0;
    }

    impl->path = strdup(path); 

    cluster_file_size = icalfileset_filesize(impl);
        
    if(cluster_file_size < 0){
	icalfileset_free(impl);
	return 0;
    } 

#ifndef WIN32
    impl->fd = open(impl->path,flags, mode);
#else
	impl->fd = sopen(impl->path,flags | O_BINARY, _SH_DENYWR, _S_IREAD | _S_IWRITE);
#endif
    
    if (impl->fd < 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	icalfileset_free(impl);
	return 0;
    }

#ifndef WIN32
    icalfileset_lock(impl);
#endif
    
    if(cluster_file_size > 0 ){
	icalerrorenum error;
	if((error = icalfileset_read_file(impl,mode))!= ICAL_NO_ERROR){
	    icalfileset_free(impl);
	    return 0;
	}
    }
 
    if(impl->cluster == 0){
	impl->cluster = icalcomponent_new(ICAL_XROOT_COMPONENT);
    }      
    
    return impl;
}

char* icalfileset_read_from_file(char *s, size_t size, void *d)
{
    
    char* p = s;
    int fd = (int)d;

    /* Simulate fgets -- read single characters and stop at '\n' */

    for(p=s; p<s+size-1;p++){
	
	if(read(fd,p,1) != 1 || *p=='\n'){
	    p++;
	    break;
	} 
    }

    *p = '\0';
    
    if(*s == 0){
	return 0;
    } else {
	return s;
    }

}


icalerrorenum icalfileset_read_file(icalfileset* cluster,mode_t mode)
{

    icalparser *parser;
  
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;
    
    parser = icalparser_new();
    icalparser_set_gen_data(parser,(void*)impl->fd);
    impl->cluster = icalparser_parse(parser,icalfileset_read_from_file);
    icalparser_free(parser);

    if (impl->cluster == 0 || icalerrno != ICAL_NO_ERROR){
	icalerror_set_errno(ICAL_PARSE_ERROR);
	return ICAL_PARSE_ERROR;
    }
  
    if (icalcomponent_isa(impl->cluster) != ICAL_XROOT_COMPONENT){
	/* The parser got a single component, so it did not put it in
	   an XROOT. */
	icalcomponent *cl = impl->cluster;
	impl->cluster = icalcomponent_new(ICAL_XROOT_COMPONENT);
	icalcomponent_add_component(impl->cluster,cl);
    }

    return ICAL_NO_ERROR;

}

int icalfileset_filesize(icalfileset* cluster)
{
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;
    int cluster_file_size;
    struct stat sbuf;

    if (stat(impl->path,&sbuf) != 0){
	
	/* A file by the given name does not exist, or there was
	   another error */
	cluster_file_size = 0;
	if (errno == ENOENT) {
	    /* It was because the file does not exist */
	    return 0;
	} else {
	    /* It was because of another error */
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return -1;
	}
    } else {
	/* A file by the given name exists, but is it a regular file? */
	
	if (!S_ISREG(sbuf.st_mode)){ 
	    /* Nope, not a regular file */
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return -1;
	} else {
	    /* Lets assume that it is a file of the right type */
	    return sbuf.st_size;
	}	
    }

    /*return -1; not reached*/
}

void icalfileset_free(icalfileset* cluster)
{
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;

    icalerror_check_arg_rv((cluster!=0),"cluster");

    if (impl->cluster != 0){
	icalfileset_commit(cluster);
	icalcomponent_free(impl->cluster);
	impl->cluster=0;
    }

    if(impl->fd > 0){
	icalfileset_unlock(impl);
	close(impl->fd);
	impl->fd = -1;
    }

    if(impl->path != 0){
	free(impl->path);
	impl->path = 0;
    }

    free(impl);
}

const char* icalfileset_path(icalfileset* cluster)
{
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;
    icalerror_check_arg_rz((cluster!=0),"cluster");

    return impl->path;
}


int icalfileset_lock(icalfileset *cluster)
{
#if !defined(WIN32) && !defined(XP_MAC)
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;
    struct flock lock;
    int rtrn;

    icalerror_check_arg_rz((impl->fd>0),"impl->fd");
    errno = 0;
    lock.l_type = F_WRLCK;     /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = 0;  /* byte offset relative to l_whence */
    lock.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = 0;       /* #bytes (0 means to EOF) */

    rtrn = fcntl(impl->fd, F_SETLKW, &lock);

    return rtrn;
#else
	return 0;
#endif
}

int icalfileset_unlock(icalfileset *cluster)
{
#if !defined(WIN32) && !defined(XP_MAC)
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;
    struct flock lock;
    icalerror_check_arg_rz((impl->fd>0),"impl->fd");

    lock.l_type = F_WRLCK;     /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = 0;  /* byte offset relative to l_whence */
    lock.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = 0;       /* #bytes (0 means to EOF) */

    return (fcntl(impl->fd, F_UNLCK, &lock)); 
#else
	return 0;
#endif
}

#ifdef ICAL_SAFESAVES
int icalfileset_safe_saves=0;
#else
int icalfileset_safe_saves=0;
#endif

icalerrorenum icalfileset_commit(icalfileset* cluster)
{
    char tmp[ICAL_PATH_MAX]; 
    char *str;
    icalcomponent *c;
    off_t write_size=0;
    
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;
    
    icalerror_check_arg_re((impl!=0),"cluster",ICAL_BADARG_ERROR);  
    
    icalerror_check_arg_re((impl->fd>0),"impl->fd is invalid",
			   ICAL_INTERNAL_ERROR) ;

    if (impl->changed == 0 ){
	return ICAL_NO_ERROR;
    }
    
#ifndef XP_MAC
    if(icalfileset_safe_saves == 1){
#ifndef WIN32
	snprintf(tmp,ICAL_PATH_MAX,"cp '%s' '%s.bak'",impl->path,impl->path);
#else
	snprintf(tmp,ICAL_PATH_MAX,"copy %s %s.bak",impl->path,impl->path);
#endif
	
	if(system(tmp) < 0){
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return ICAL_FILE_ERROR;
	}
    }
#endif

    if(lseek(impl->fd,0,SEEK_SET) < 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return ICAL_FILE_ERROR;
    }
    
    for(c = icalcomponent_get_first_component(impl->cluster,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(impl->cluster,ICAL_ANY_COMPONENT)){
	int sz;

	str = icalcomponent_as_ical_string(c);
    
	sz=write(impl->fd,str,strlen(str));

	if ( sz != strlen(str)){
	    perror("write");
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return ICAL_FILE_ERROR;
	}

	write_size += sz;
    }
    
    impl->changed = 0;    

#if defined(WIN32)
    chsize( impl->fd, tell( impl->fd ) );
#elif defined(XP_OS2)
    /* On OS/2, the file can be larger than what you are told is written */
    /* Because of line endings (0x0A 0x0D). Unfortunately, we chouldn't */
    /* Just take the WIN32 path, because the chsize is crashing OS/2. */
    /* We're looking into the crash. */
    if(ftruncate(impl->fd,tell(impl->fd)) < 0){
	return ICAL_FILE_ERROR;
    }
#elif defined(XP_MAC)
    // XXX THIS IS BROKEN ON MAC, NEED REPLACEMENT FOR ftruncate()
#else
    if(ftruncate(impl->fd,write_size) < 0){
	return ICAL_FILE_ERROR;
    }
#endif
    return ICAL_NO_ERROR;
    
} 

void icalfileset_mark(icalfileset* cluster){

    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;

    icalerror_check_arg_rv((impl!=0),"cluster");

    impl->changed = 1;

}

icalcomponent* icalfileset_get_component(icalfileset* cluster){
    struct icalfileset_impl *impl = (struct icalfileset_impl*)cluster;

    icalerror_check_arg_re((impl!=0),"cluster",ICAL_BADARG_ERROR);

    return impl->cluster;
}


/* manipulate the components in the cluster */

icalerrorenum icalfileset_add_component(icalfileset *cluster,
					icalcomponent* child)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)cluster;

    icalerror_check_arg_re((cluster!=0),"cluster", ICAL_BADARG_ERROR);
    icalerror_check_arg_re((child!=0),"child",ICAL_BADARG_ERROR);

    icalcomponent_add_component(impl->cluster,child);

    icalfileset_mark(cluster);

    return ICAL_NO_ERROR;

}

icalerrorenum icalfileset_remove_component(icalfileset *cluster,
					   icalcomponent* child)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)cluster;

    icalerror_check_arg_re((cluster!=0),"cluster",ICAL_BADARG_ERROR);
    icalerror_check_arg_re((child!=0),"child",ICAL_BADARG_ERROR);

    icalcomponent_remove_component(impl->cluster,child);

    icalfileset_mark(cluster);

    return ICAL_NO_ERROR;
}

int icalfileset_count_components(icalfileset *cluster,
				 icalcomponent_kind kind)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)cluster;

    if(cluster == 0){
	icalerror_set_errno(ICAL_BADARG_ERROR);
	return -1;
    }

    return icalcomponent_count_components(impl->cluster,kind);
}

icalerrorenum icalfileset_select(icalfileset* set, icalgauge* gauge)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)set;

    icalerror_check_arg_re(gauge!=0,"guage",ICAL_BADARG_ERROR);

    impl->gauge = gauge;

    return ICAL_NO_ERROR;
}

void icalfileset_clear(icalfileset* gauge)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)gauge;
    
    impl->gauge = 0;

}

icalcomponent* icalfileset_fetch(icalfileset* store,const char* uid)
{
    icalcompiter i;    
    struct icalfileset_impl* impl = (struct icalfileset_impl*)store;
    
    for(i = icalcomponent_begin_component(impl->cluster,ICAL_ANY_COMPONENT);
	icalcompiter_deref(&i)!= 0; icalcompiter_next(&i)){
	
		icalcomponent *this = icalcompiter_deref(&i);
		icalcomponent *inner;
		icalcomponent *p;
		const char *this_uid;

		for(inner = icalcomponent_get_first_component(this,ICAL_ANY_COMPONENT);
			inner != 0;
			inner = icalcomponent_get_next_component(this,ICAL_ANY_COMPONENT)){

			p = icalcomponent_get_first_property(inner,ICAL_UID_PROPERTY);
			if ( p )
			{
				this_uid = icalproperty_get_uid(p);

				if(this_uid==0){
				icalerror_warn("icalfileset_fetch found a component with no UID");
				continue;
				}

				if (strcmp(uid,this_uid)==0){
					return this;
				}
			}
		}
	}

    return 0;
}

int icalfileset_has_uid(icalfileset* store,const char* uid)
{
    assert(0); /* HACK, not implemented */
    return 0;
}

/******* support routines for icalfileset_fetch_match *********/

struct icalfileset_id{
    char* uid;
    char* recurrence_id;
    int sequence;
};

void icalfileset_id_free(struct icalfileset_id *id)
{
    if(id->recurrence_id != 0){
	free(id->recurrence_id);
    }

    if(id->uid != 0){
	free(id->uid);
    }

}

struct icalfileset_id icalfileset_get_id(icalcomponent* comp)
{

    icalcomponent *inner;
    struct icalfileset_id id;
    icalproperty *p;

    inner = icalcomponent_get_first_real_component(comp);
    
    p = icalcomponent_get_first_property(inner, ICAL_UID_PROPERTY);

    assert(p!= 0);

    id.uid = strdup(icalproperty_get_uid(p));

    p = icalcomponent_get_first_property(inner, ICAL_SEQUENCE_PROPERTY);

    if(p == 0) {
	id.sequence = 0;
    } else { 
	id.sequence = icalproperty_get_sequence(p);
    }

    p = icalcomponent_get_first_property(inner, ICAL_RECURRENCEID_PROPERTY);

    if (p == 0){
	id.recurrence_id = 0;
    } else {
	icalvalue *v;
	v = icalproperty_get_value(p);
	id.recurrence_id = strdup(icalvalue_as_ical_string(v));

	assert(id.recurrence_id != 0);
    }

    return id;
}

/* Find the component that is related to the given
   component. Currently, it just matches based on UID and
   RECURRENCE-ID */
icalcomponent* icalfileset_fetch_match(icalfileset* set, icalcomponent *comp)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)set;
    icalcompiter i;    

    struct icalfileset_id comp_id, match_id;
    
    comp_id = icalfileset_get_id(comp);

    for(i = icalcomponent_begin_component(impl->cluster,ICAL_ANY_COMPONENT);
	icalcompiter_deref(&i)!= 0; icalcompiter_next(&i)){
	
	icalcomponent *match = icalcompiter_deref(&i);

	match_id = icalfileset_get_id(match);

	if(strcmp(comp_id.uid, match_id.uid) == 0 &&
	   ( comp_id.recurrence_id ==0 || 
	     strcmp(comp_id.recurrence_id, match_id.recurrence_id) ==0 )){

	    /* HACK. What to do with SEQUENCE? */

	    icalfileset_id_free(&match_id);
	    icalfileset_id_free(&comp_id);
	    return match;
	    
	}
	
	icalfileset_id_free(&match_id);
    }

    icalfileset_id_free(&comp_id);
    return 0;

}


icalerrorenum icalfileset_modify(icalfileset* store, icalcomponent *old,
				 icalcomponent *new)
{
    assert(0); /* HACK, not implemented */
    return ICAL_NO_ERROR;
}


/* Iterate through components */
icalcomponent* icalfileset_get_current_component (icalfileset* cluster)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)cluster;

    icalerror_check_arg_rz((cluster!=0),"cluster");

    return icalcomponent_get_current_component(impl->cluster);
}


icalcomponent* icalfileset_get_first_component(icalfileset* cluster)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)cluster;
    icalcomponent *c=0;

    icalerror_check_arg_rz((cluster!=0),"cluster");

    do {
	if (c == 0){
	    c = icalcomponent_get_first_component(impl->cluster,
						  ICAL_ANY_COMPONENT);
	} else {
	    c = icalcomponent_get_next_component(impl->cluster,
						 ICAL_ANY_COMPONENT);
	}

	if(c != 0 && (impl->gauge == 0 ||
		      icalgauge_compare(impl->gauge,c) == 1)){
	    return c;
	}

    } while(c != 0);


    return 0;
}

icalcomponent* icalfileset_get_next_component(icalfileset* cluster)
{
    struct icalfileset_impl* impl = (struct icalfileset_impl*)cluster;
    icalcomponent *c;

    icalerror_check_arg_rz((cluster!=0),"cluster");
    
    do {
	c = icalcomponent_get_next_component(impl->cluster,
					     ICAL_ANY_COMPONENT);

	if(c != 0 && (impl->gauge == 0 ||
		      icalgauge_compare(impl->gauge,c) == 1)){
	    return c;
	}
	
    } while(c != 0);
    
    
    return 0;
}

