/* -*- Mode: C -*-
    ======================================================================
    FILE: icaldirset.c
    CREATOR: eric 28 November 1999
  
    $Id: icaldirset.c,v 1.6 2002/11/06 21:22:43 mostafah%oeone.com Exp $
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


/*
 
  icaldirset manages a database of ical components and offers
  interfaces for reading, writting and searching for components.

  icaldirset groups components in to clusters based on their DTSTAMP
  time -- all components that start in the same month are grouped
  together in a single file. All files in a sotre are kept in a single
  directory. 

  The primary interfaces are icaldirset_first and icaldirset_next. These
  routine iterate through all of the components in the store, subject
  to the current gauge. A gauge is an icalcomponent that is tested
  against other componets for a match. If a gauge has been set with
  icaldirset_select, icaldirset_first and icaldirset_next will only
  return componentes that match the gauge.

  The Store generated UIDs for all objects that are stored if they do
  not already have a UID. The UID is the name of the cluster (month &
  year as MMYYYY) plus a unique serial number. The serial number is
  stored as a property of the cluster.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "ical.h"
#include "icaldirset.h"
#include "pvl.h" 
#include "icalerror.h"
#include "icalparser.h"
#include "icaldirset.h"
#include "icalfileset.h"
#include "icalfilesetimpl.h"
#include "icalgauge.h"

#include <limits.h> /* For PATH_MAX */
#ifndef WIN32
#include <dirent.h> /* for opendir() */
#ifdef XP_MAC
#include <utsname.h> /* for uname */
#else
#include <unistd.h> /* for stat, getpid */
#include <sys/utsname.h> /* for uname */
#endif
#else
#include <io.h>
#include <process.h>
#endif
#include <errno.h>
#ifdef XP_MAC
#include <extras.h> /* for strdup */
#else
#include <sys/types.h> /* for opendir() */
#include <sys/stat.h> /* for stat */
#endif
#include <time.h> /* for clock() */
#include <stdlib.h> /* for rand(), srand() */
#include <string.h> /* for strdup */
#include "icaldirsetimpl.h"


#ifdef WIN32
#define snprintf	_snprintf
#define strcasecmp	stricmp

#define _S_ISTYPE(mode, mask)  (((mode) & _S_IFMT) == (mask))

#define S_ISDIR(mode)    _S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode)    _S_ISTYPE((mode), _S_IFREG)
#endif

struct icaldirset_impl* icaldirset_new_impl()
{
    struct icaldirset_impl* impl;

    if ( ( impl = (struct icaldirset_impl*)
	   malloc(sizeof(struct icaldirset_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    strcpy(impl->id,ICALDIRSET_ID);

    return impl;
}

const char* icaldirset_path(icaldirset* cluster)
{
    struct icaldirset_impl *impl = icaldirset_new_impl();

    return impl->dir;

}

void icaldirset_mark(icaldirset* store)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;

    icalfileset_mark(impl->cluster);
}


icalerrorenum icaldirset_commit(icaldirset* store)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;

    return icalfileset_commit(impl->cluster);

}

void icaldirset_lock(const char* dir)
{
}


void icaldirset_unlock(const char* dir)
{
}

/* Load the contents of the store directory into the store's internal directory list*/
icalerrorenum icaldirset_read_directory(struct icaldirset_impl* impl)
{
    char *str;
#ifndef WIN32
    struct dirent *de;
    DIR* dp;
 
    dp = opendir(impl->dir);
   
    if ( dp == 0) {
	icalerror_set_errno(ICAL_FILE_ERROR);
	return ICAL_FILE_ERROR;
    }

    /* clear contents of directory list */
    while((str = pvl_pop(impl->directory))){
	free(str);
    }
    
    /* load all of the cluster names in the directory list */
    for(de = readdir(dp);
	de != 0;
	de = readdir(dp)){

	/* Remove known directory names  '.' and '..'*/
	if (strcmp(de->d_name,".") == 0 ||
	    strcmp(de->d_name,"..") == 0 ){
	    continue;
	}

	pvl_push(impl->directory, (void*)strdup(de->d_name));	
    }

    closedir(dp);
#else
	struct _finddata_t c_file;
	long hFile;
	
	/* Find first .c file in current directory */
	if( (hFile = _findfirst( "*", &c_file )) == -1L )
	{
		icalerror_set_errno(ICAL_FILE_ERROR);
		return ICAL_FILE_ERROR;
	}
	else
	{
		while((str = pvl_pop(impl->directory))){
			free(str);
		}
		
		/* load all of the cluster names in the directory list */
		do
		{
			/* Remove known directory names  '.' and '..'*/
			if (strcmp(c_file.name,".") == 0 ||
				strcmp(c_file.name,"..") == 0 ){
				continue;
			}
			
			pvl_push(impl->directory, (void*)strdup(c_file.name));
		}
		while ( _findnext( hFile, &c_file ) == 0 );
			
		_findclose( hFile );
	}

#endif

    return ICAL_NO_ERROR;
}

icaldirset* icaldirset_new(const char* dir)
{
    struct icaldirset_impl *impl = icaldirset_new_impl();
    struct stat sbuf;

    if (impl == 0){
	return 0;
    }

    icalerror_check_arg_rz( (dir!=0), "dir");

    if (stat(dir,&sbuf) != 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return 0;
    }
    
    /* dir is not the name of a direectory*/
    if (!S_ISDIR(sbuf.st_mode)){ 
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }	    

    icaldirset_lock(dir);

    impl = icaldirset_new_impl();

    if (impl ==0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    impl->directory = pvl_newlist();
    impl->directory_iterator = 0;
    impl->dir = (char*)strdup(dir);
    impl->gauge = 0;
    impl->first_component = 0;
    impl->cluster = 0;

    icaldirset_read_directory(impl);

    return (icaldirset*) impl;
}

icaldirset* icaldirset_new_writer(const char* dir)
{
    struct icaldirset_impl *impl = icaldirset_new_impl();
    struct stat sbuf;

    if (impl == 0){
	return 0;
    }

    icalerror_check_arg_rz( (dir!=0), "dir");

    if (stat(dir,&sbuf) != 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return 0;
    }
    
    /* dir is not the name of a direectory*/
    if (!S_ISDIR(sbuf.st_mode)){ 
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }	    

    icaldirset_lock(dir);

    impl = icaldirset_new_impl();

    if (impl ==0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    impl->directory = pvl_newlist();
    impl->directory_iterator = 0;
    impl->dir = (char*)strdup(dir);
    impl->gauge = 0;
    impl->first_component = 0;
    impl->cluster = 0;

    return (icaldirset*) impl;
}

icaldirset* icaldirset_new_reader(const char* dir)
{
    struct icaldirset_impl *impl = icaldirset_new_impl();
    struct stat sbuf;

    if (impl == 0){
	return 0;
    }

    icalerror_check_arg_rz( (dir!=0), "dir");

    if (stat(dir,&sbuf) != 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return 0;
    }
    
    /* dir is not the name of a direectory*/
    if (!S_ISDIR(sbuf.st_mode)){ 
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }	    

    icaldirset_lock(dir);

    impl = icaldirset_new_impl();

    if (impl ==0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    impl->directory = pvl_newlist();
    impl->directory_iterator = 0;
    impl->dir = (char*)strdup(dir);
    impl->gauge = 0;
    impl->first_component = 0;
    impl->cluster = 0;

    icaldirset_read_directory(impl);

    return (icaldirset*) impl;
}

void icaldirset_free(icaldirset* s)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)s;
    char* str;

    icaldirset_unlock(impl->dir);

    if(impl->dir !=0){
	free(impl->dir);
    }

    if(impl->gauge !=0){
	icalcomponent_free(impl->gauge);
    }

    if(impl->cluster !=0){
	icalfileset_free(impl->cluster);
    }

    while(impl->directory !=0 &&  (str=pvl_pop(impl->directory)) != 0){
	free(str);
    }

    if(impl->directory != 0){
	pvl_free(impl->directory);
    }

    impl->directory = 0;
    impl->directory_iterator = 0;
    impl->dir = 0;
    impl->gauge = 0;
    impl->first_component = 0;

    free(impl);

}

/* icaldirset_next_uid_number updates a serial number in the Store
   directory in a file called SEQUENCE */

int icaldirset_next_uid_number(icaldirset* store)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;
    char sequence = 0;
    char temp[128];
    char filename[ICAL_PATH_MAX];
    char *r;
    FILE *f;
    struct stat sbuf;

    icalerror_check_arg_rz( (store!=0), "store");

    sprintf(filename,"%s/%s",impl->dir,"SEQUENCE");

    /* Create the file if it does not exist.*/
    if (stat(filename,&sbuf) == -1 || !S_ISREG(sbuf.st_mode)){

	f = fopen(filename,"w");
	if (f != 0){
	    fprintf(f,"0");
	    fclose(f);
	} else {
	    icalerror_warn("Can't create SEQUENCE file in icaldirset_next_uid_number");
	    return 0;
	}
	
    }
    
    if ( (f = fopen(filename,"r+")) != 0){

	rewind(f);
	r = fgets(temp,128,f);

	if (r == 0){
	    sequence = 1;
	} else {
	    sequence = atoi(temp)+1;
	}

	rewind(f);

	fprintf(f,"%d",sequence);

	fclose(f);

	return sequence;
	
    } else {
	icalerror_warn("Can't create SEQUENCE file in icaldirset_next_uid_number");
	return 0;
    }

}

icalerrorenum icaldirset_next_cluster(icaldirset* store)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;
    char path[ICAL_PATH_MAX];

    if (impl->directory_iterator == 0){
	icalerror_set_errno(ICAL_INTERNAL_ERROR);
	return ICAL_INTERNAL_ERROR;
    }	    
    impl->directory_iterator = pvl_next(impl->directory_iterator);

    if (impl->directory_iterator == 0){
	/* There are no more clusters */
	if(impl->cluster != 0){
	    icalfileset_free(impl->cluster);
	    impl->cluster = 0;
	}
	return ICAL_NO_ERROR;
    }
	    
    sprintf(path,"%s/%s",impl->dir,(char*)pvl_data(impl->directory_iterator));

    icalfileset_free(impl->cluster);

    impl->cluster = icalfileset_new(path);

    return icalerrno;
}

void icaldirset_add_uid(icaldirset* store, icaldirset* comp)
{
    char uidstring[ICAL_PATH_MAX];
    icalproperty *uid;
#ifndef WIN32
    struct utsname unamebuf;
#endif

    icalerror_check_arg_rv( (store!=0), "store");
    icalerror_check_arg_rv( (comp!=0), "comp");

    uid = icalcomponent_get_first_property(comp,ICAL_UID_PROPERTY);
    
    if (uid == 0) {
	
#ifndef WIN32
	uname(&unamebuf);
	
	sprintf(uidstring,"%d-%s",(int)getpid(),unamebuf.nodename);
#else
	sprintf(uidstring,"%d-%s",(int)getpid(),"WINDOWS");  /* FIX: There must be an easy get the system name */
#endif
	
	uid = icalproperty_new_uid(uidstring);
	icalcomponent_add_property(comp,uid);
    } else {
	
	strcpy(uidstring,icalproperty_get_uid(uid));
    }
}


/* This assumes that the top level component is a VCALENDAR, and there
   is an inner component of type VEVENT, VTODO or VJOURNAL. The inner
   component must have a DTAMP property */

icalerrorenum icaldirset_add_component(icaldirset* store, icaldirset* comp)
{
    struct icaldirset_impl *impl;
    char clustername[ICAL_PATH_MAX];
    icalproperty *dt;
    icalvalue *v;
    struct icaltimetype tm;
    icalerrorenum error = ICAL_NO_ERROR;
    icalcomponent *inner;

    impl = (struct icaldirset_impl*)store;
    icalerror_check_arg_rz( (store!=0), "store");
    icalerror_check_arg_rz( (comp!=0), "comp");

    errno = 0;
    
    icaldirset_add_uid(store,comp);

    /* Determine which cluster this object belongs in. This is a HACK */

    for(inner = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	inner != 0;
	inner = icalcomponent_get_next_component(comp,ICAL_ANY_COMPONENT)){
  
	dt = icalcomponent_get_first_property(inner,ICAL_DTSTAMP_PROPERTY);
 	
	if (dt != 0){
	    break; 
	}	
    }

    if (dt == 0){

	for(inner = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	    inner != 0;
	    inner = icalcomponent_get_next_component(comp,ICAL_ANY_COMPONENT)){
	    
	    dt = icalcomponent_get_first_property(inner,ICAL_DTSTART_PROPERTY);
	    
	    if (dt != 0){
		break; 
	    }	
	}

    }

    if (dt == 0){


	icalerror_warn("The component does not have a DTSTAMP or DTSTART property, so it cannot be added to the store");
	icalerror_set_errno(ICAL_BADARG_ERROR);
	return ICAL_BADARG_ERROR;
    }

    v = icalproperty_get_value(dt);

    tm = icalvalue_get_datetime(v);

    snprintf(clustername,ICAL_PATH_MAX,"%s/%04d%02d",impl->dir,tm.year,tm.month);

    /* Load the cluster and insert the object */

    if(impl->cluster != 0 && 
       strcmp(clustername,icalfileset_path(impl->cluster)) != 0 ){
	icalfileset_free(impl->cluster);
	impl->cluster = 0;
    }

    if (impl->cluster == 0){
	impl->cluster = icalfileset_new(clustername);

	if (impl->cluster == 0){
	    error = icalerrno;
	}
    }
    
    if (error != ICAL_NO_ERROR){
	icalerror_set_errno(error);
	return error;
    }

    /* Add the component to the cluster */

    icalfileset_add_component(impl->cluster,comp);
    
    icalfileset_mark(impl->cluster);

    return ICAL_NO_ERROR;    
}

/* Remove a component in the current cluster. HACK. This routine is a
   "friend" of icalfileset, and breaks its encapsulation. It was
   either do it this way, or add several layers of interfaces that had
   no other use.  */
icalerrorenum icaldirset_remove_component(icaldirset* store, icaldirset* comp)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;

    struct icalfileset_impl *filesetimpl = 
	(struct icalfileset_impl*)impl->cluster;

    icalcomponent *filecomp = filesetimpl->cluster;

    icalcompiter i;
    int found = 0;

    icalerror_check_arg_re((store!=0),"store",ICAL_BADARG_ERROR);
    icalerror_check_arg_re((comp!=0),"comp",ICAL_BADARG_ERROR);
    icalerror_check_arg_re((impl->cluster!=0),"Cluster pointer",ICAL_USAGE_ERROR);

    for(i = icalcomponent_begin_component(filecomp,ICAL_ANY_COMPONENT);
	icalcompiter_deref(&i)!= 0; icalcompiter_next(&i)){
	
	icalcomponent *this = icalcompiter_deref(&i);

	if (this == comp){
	    found = 1;
	    break;
	}
    }

    if (found != 1){
	icalerror_warn("icaldirset_remove_component: component is not part of current cluster");
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return ICAL_USAGE_ERROR;
    }

    icalfileset_remove_component(impl->cluster,comp);

    icalfileset_mark(impl->cluster);

    /* If the removal emptied the fileset, get the next fileset */
    if( icalfileset_count_components(impl->cluster,ICAL_ANY_COMPONENT)==0){
	
	icalerrorenum error = icaldirset_next_cluster(store);

	if(impl->cluster != 0 && error == ICAL_NO_ERROR){
	    icalfileset_get_first_component(impl->cluster);
	} else {
	    /* HACK. Not strictly correct for impl->cluster==0 */
	    return error;
	}
    } else {
	/* Do nothing */
    }

    return ICAL_NO_ERROR;
}



int icaldirset_count_components(icaldirset* store,
			       icalcomponent_kind kind)
{
    /* HACK, not implemented */
    
    assert(0);

    return 0;
}


icalcomponent* icaldirset_fetch_match(icaldirset* set, icalcomponent *c)
{
    fprintf(stderr," icaldirset_fetch_match is not implemented\n");
    assert(0);
}


icalcomponent* icaldirset_fetch(icaldirset* store, const char* uid)
{
    icalcomponent *gauge;
    icalcomponent *old_gauge;
    icalcomponent *c;
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;

    icalerror_check_arg_rz( (store!=0), "store");
    icalerror_check_arg_rz( (uid!=0), "uid");

    gauge = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_uid(
		    uid,
		    icalparameter_new_xliccomparetype(
			ICAL_XLICCOMPARETYPE_EQUAL),
		    0),
		0),
	    0);

    old_gauge = impl->gauge;
    impl->gauge = gauge;

    c= icaldirset_get_first_component(store);

    impl->gauge = old_gauge;

    icalcomponent_free(gauge);

    return c;
}


int icaldirset_has_uid(icaldirset* store, const char* uid)
{
    icalcomponent *c;

    icalerror_check_arg_rz( (store!=0), "store");
    icalerror_check_arg_rz( (uid!=0), "uid");
    
    /* HACK. This is a temporary implementation. _has_uid should use a
       database, and _fetch should use _has_uid, not the other way
       around */
    c = icaldirset_fetch(store,uid);

    return c!=0;

}


icalerrorenum icaldirset_select(icaldirset* store, icalcomponent* gauge)
 {
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;

    icalerror_check_arg_re( (store!=0), "store",ICAL_BADARG_ERROR);
    icalerror_check_arg_re( (gauge!=0), "gauge",ICAL_BADARG_ERROR);

    if (!icalcomponent_is_valid(gauge)){
	return ICAL_BADARG_ERROR;
    }

    impl->gauge = gauge;

    return ICAL_NO_ERROR;
}


icalerrorenum icaldirset_modify(icaldirset* store, icalcomponent *old,
			       icalcomponent *new)
{
    assert(0);
    return ICAL_NO_ERROR; /* HACK, not implemented */

}


void icaldirset_clear(icaldirset* store)
{

    assert(0);
    return;
    /* HACK, not implemented */
}

icalcomponent* icaldirset_get_current_component(icaldirset* store)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;

    if(impl->cluster == 0){
	icaldirset_get_first_component(store);
    }

    return icalfileset_get_current_component(impl->cluster);

}


icalcomponent* icaldirset_get_first_component(icaldirset* store)
{
    struct icaldirset_impl *impl = (struct icaldirset_impl*)store;
    icalerrorenum error;
    char path[ICAL_PATH_MAX];

    error = icaldirset_read_directory(impl);

    if (error != ICAL_NO_ERROR){
	icalerror_set_errno(error);
	return 0;
    }

    impl->directory_iterator = pvl_head(impl->directory);
    
    if (impl->directory_iterator == 0){
	icalerror_set_errno(error);
	return 0;
    }
    
    snprintf(path,ICAL_PATH_MAX,"%s/%s",impl->dir,(char*)pvl_data(impl->directory_iterator));

    /* If the next cluster we need is different than the current cluster, 
       delete the current one and get a new one */

    if(impl->cluster != 0 && strcmp(path,icalfileset_path(impl->cluster)) != 0 ){
	icalfileset_free(impl->cluster);
	impl->cluster = 0;
    }
    
    if (impl->cluster == 0){
	impl->cluster = icalfileset_new(path);

	if (impl->cluster == 0){
	    error = icalerrno;
	}
    } 

    if (error != ICAL_NO_ERROR){
	icalerror_set_errno(error);
	return 0;
    }

    impl->first_component = 1;

    return icaldirset_get_next_component(store);
}

icalcomponent* icaldirset_get_next_component(icaldirset* store)
{
    struct icaldirset_impl *impl;
    icalcomponent *c;
    icalerrorenum error;

    icalerror_check_arg_rz( (store!=0), "store");

    impl = (struct icaldirset_impl*)store;

    if(impl->cluster == 0){

	icalerror_warn("icaldirset_get_next_component called with a NULL cluster (Caller must call icaldirset_get_first_component first");
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;

    }

    /* Set the component iterator for the following for loop */
    if (impl->first_component == 1){
	icalfileset_get_first_component(impl->cluster);
	impl->first_component = 0;
    } else {
	icalfileset_get_next_component(impl->cluster);
    }


    while(1){
	/* Iterate through all of the objects in the cluster*/
	for( c = icalfileset_get_current_component(impl->cluster);
	     c != 0;
	     c = icalfileset_get_next_component(impl->cluster)){
	    
	    /* If there is a gauge defined and the component does not
	       pass the gauge, skip the rest of the loop */

#if 0 /* HACK */
	    if (impl->gauge != 0 && icalgauge_test(c,impl->gauge) == 0){
		continue;
	    }
#else
	    assert(0); /* icalgauge_test needs to be fixed */
#endif
	    /* Either there is no gauge, or the component passed the
	       gauge, so return it*/

	    return c;
	}

	/* Fell through the loop, so the component we want is not
	   in this cluster. Load a new cluster and try again.*/

	error = icaldirset_next_cluster(store);

	if(impl->cluster == 0 || error != ICAL_NO_ERROR){
	    /* No more clusters */
	    return 0;
	} else {
	    c = icalfileset_get_first_component(impl->cluster);

	    return c;
	}
	
    }

    return 0; /* Should never get here */
}

   




	
