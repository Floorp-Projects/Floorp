/*======================================================================
  FILE: icalcalendar.c
  CREATOR: eric 23 December 1999
  
  $Id: icalcalendar.c,v 1.1 2001/12/21 19:04:05 mikep%oeone.com Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 ======================================================================*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "icalcalendar.h"
#include "icalset.h"
#include "icalfileset.h"
#include "icaldirset.h"
#include <limits.h> 
#include <sys/stat.h> /* For mkdir, stat */
#include <sys/types.h> /* For mkdir */
#include <fcntl.h> /* For mkdir */
#include <unistd.h>  /* For mkdir, stat */    
#include <stdlib.h> /* for malloc */
#include <string.h> /* for strcat */
#include <errno.h>

#define BOOKED_DIR "booked"
#define INCOMING_FILE "incoming.ics"
#define PROP_FILE "properties.ics"
#define FBLIST_FILE "freebusy.ics"

struct icalcalendar_impl 
{
	char* dir;
	icalcomponent* freebusy;
	icalcomponent* properties;
	icalset* booked;
	icalset* incoming;
};

struct icalcalendar_impl* icalcalendar_new_impl()
{
    struct icalcalendar_impl* impl;

    if ( ( impl = (struct icalcalendar_impl*)
	   malloc(sizeof(struct icalcalendar_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    return impl;
}


icalerrorenum icalcalendar_create(struct icalcalendar_impl* impl)
{
    char path[PATH_MAX];
    struct stat sbuf;
    int r;
    
    icalerror_check_arg_re((impl != 0),"impl",ICAL_BADARG_ERROR);

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,BOOKED_DIR);

    r = stat(path,&sbuf);

    if( r != 0 && errno == ENOENT){

	if(mkdir(path,0777)!=0){
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return ICAL_FILE_ERROR;
	}
    }

    return ICAL_NO_ERROR;
}

icalcalendar* icalcalendar_new(char* dir)
{
    struct icalcalendar_impl* impl;

    icalerror_check_arg_rz((dir != 0),"dir");
    
    impl = icalcalendar_new_impl();

    if (impl == 0){
	return 0;
    }

    impl->dir = (char*)strdup(dir);
    impl->freebusy = 0;
    impl->properties = 0;
    impl->booked = 0;
    impl->incoming = 0;

    if (icalcalendar_create(impl) != ICAL_NO_ERROR){
	free(impl);
	return 0;
    }

    return impl;
}

void icalcalendar_free(icalcalendar* calendar)
{

    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
        
    if (impl->dir !=0){
	free(impl->dir);
    }

    if (impl->freebusy !=0){
	icalfileset_free(impl->freebusy);
    }

    if (impl->properties !=0){
	icalfileset_free(impl->properties);
    }

    if (impl->booked !=0){
	icaldirset_free(impl->booked);
    }

    if (impl->incoming !=0){
	icaldirset_free(impl->incoming);
    }

    impl->dir = 0;
    impl->freebusy = 0;
    impl->properties = 0;
    impl->booked = 0;
    impl->incoming = 0;


    free(impl);
}


int icalcalendar_lock(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

int icalcalendar_unlock(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

int icalcalendar_islocked(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

int icalcalendar_ownlock(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

icalset* icalcalendar_get_booked(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    char dir[PATH_MAX];

    icalerror_check_arg_rz((impl != 0),"impl");
    
    dir[0] = '\0';
    strcpy(dir,impl->dir);
    strcat(dir,"/");
    strcat(dir,BOOKED_DIR);

    if (impl->booked == 0){
	icalerror_clear_errno();
	impl->booked = icaldirset_new(dir);
	assert(icalerrno == ICAL_NO_ERROR);
    }

    return impl->booked;

}

icalset* icalcalendar_get_incoming(icalcalendar* calendar)
{
    char path[PATH_MAX];
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,INCOMING_FILE);

    if (impl->properties == 0){
	impl->properties = icalfileset_new(path);
    }

    return impl->properties;
}

icalset* icalcalendar_get_properties(icalcalendar* calendar)
{
    char path[PATH_MAX];
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,PROP_FILE);

    if (impl->properties == 0){
	impl->properties = icalfileset_new(path);
    }

    return impl->properties;
}

icalset* icalcalendar_get_freebusy(icalcalendar* calendar)
{
    char path[PATH_MAX];
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,FBLIST_FILE);


    if (impl->freebusy == 0){
	impl->freebusy = icalfileset_new(path);
    }

    return impl->freebusy;
}




