/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltypes.c
  CREATOR: eric 16 May 1999
  
  $Id: icaltypes.c,v 1.4 2002/03/14 15:17:52 mikep%oeone.com Exp $
  $Locker:  $
    

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icaltypes.c

 ======================================================================*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icaltypes.h"
#include "icalerror.h"
#include "icalmemory.h"
#include <stdlib.h> /* for malloc and abs() */
#include <errno.h> /* for errno */
#include <string.h> /* for icalmemory_strdup */
#include <assert.h>

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

#define TEMP_MAX 1024

void*
icalattachtype_get_data (struct icalattachtype* type);

struct icalattachtype*
icalattachtype_new()
{
    struct icalattachtype* v;

    if ( ( v = (struct icalattachtype*)
	   malloc(sizeof(struct icalattachtype))) == 0) {
	errno = ENOMEM;
	return 0;
    }

    v->refcount = 1;

    v->binary = 0;
    v->owns_binary = 0;

    v->base64 = 0;
    v->owns_base64 = 0;

    v->url = 0; 

    return v;
}


void
icalattachtype_free(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    
    v->refcount--;

    if (v->refcount <= 0){
	
	if (v->base64 != 0 && v->owns_base64 != 0){
	    free(v->base64);
	}

	if (v->binary != 0 && v->owns_binary != 0){
	    free(v->binary);
	}
	
	if (v->url != 0){
	    free(v->url);
	}

	free(v);
    }
}

void  icalattachtype_add_reference(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    v->refcount++;
}

void icalattachtype_set_url(struct icalattachtype* v, char* url)
{
    icalerror_check_arg( (v!=0),"v");

    if (v->url != 0){
	free (v->url);
    }

    v->url = icalmemory_strdup(url);

    /* HACK This routine should do something if icalmemory_strdup returns NULL */

}

char* icalattachtype_get_url(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    return v->url;
}

void icalattachtype_set_base64(struct icalattachtype* v, char* base64,
				int owns)
{
    icalerror_check_arg( (v!=0),"v");

    v->base64 = base64;
    v->owns_base64 = !(owns != 0 );
    
}

char* icalattachtype_get_base64(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    return v->base64;
}

void icalattachtype_set_binary(struct icalattachtype* v, char* binary,
				int owns)
{
    icalerror_check_arg( (v!=0),"v");

    v->binary = binary;
    v->owns_binary = !(owns != 0 );

}

void* icalattachtype_get_binary(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    return v->binary;
}

int icaltriggertype_is_null_trigger(struct icaltriggertype tr)
{
    if(icaltime_is_null_time(tr.time) && 
       icaldurationtype_is_null_duration(tr.duration)){
        return 1;
    }

    return 0;
}
    
struct icaltriggertype icaltriggertype_from_string(const char* str)
{

    
    struct icaltriggertype tr, null_tr;
    icalerrorstate es;
    icalerrorenum e;

    tr.time= icaltime_null_time();
    tr.duration = icaldurationtype_from_int(0);

    null_tr = tr;

    if(str == 0) goto error;

    /* Surpress errors so a failure in icaltime_from_string() does not cause an abort */
    es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);
    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);
    e = icalerrno;
    icalerror_set_errno(ICAL_NO_ERROR);

    tr.time = icaltime_from_string(str);

    if (icaltime_is_null_time(tr.time)){

	tr.duration = icaldurationtype_from_string(str);

	if(icaldurationtype_as_int(tr.duration) == 0) goto error;
    } 

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);
    icalerror_set_errno(e);
    return tr;

 error:
    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return null_tr;

}


struct icalreqstattype icalreqstattype_from_string(const char* str)
{
  const char *p1,*p2;
  struct icalreqstattype stat;
  int major, minor;

  icalerror_check_arg((str != 0),"str");

  stat.code = ICAL_UNKNOWN_STATUS;
  stat.debug = 0; 
   stat.desc = 0;

  /* Get the status numbers */

  sscanf(str, "%d.%d",&major, &minor);

  if (major <= 0 || minor < 0){
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return stat;
  }

  stat.code = icalenum_num_to_reqstat(major, minor);

  if (stat.code == ICAL_UNKNOWN_STATUS){
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return stat;
  }
  

  p1 = strchr(str,';');

  if (p1 == 0){
/*    icalerror_set_errno(ICAL_BADARG_ERROR);*/
    return stat;
  }

  /* Just ignore the second clause; it will be taken from inside the library 
   */



  p2 = strchr(p1+1,';');
  if (p2 != 0 && *p2 != 0){
    stat.debug = p2+1;
  } 

  return stat;
  
}

const char* icalreqstattype_as_string(struct icalreqstattype stat)
{
  char *temp;

  temp = (char*)icalmemory_tmp_buffer(TEMP_MAX);

  icalerror_check_arg_rz((stat.code != ICAL_UNKNOWN_STATUS),"Status");
  
  if (stat.desc == 0){
    stat.desc = icalenum_reqstat_desc(stat.code);
  }
  
  if(stat.debug != 0){
    snprintf(temp,TEMP_MAX,"%d.%d;%s;%s", icalenum_reqstat_major(stat.code),
             icalenum_reqstat_minor(stat.code),
             stat.desc, stat.debug);
    
  } else {
    snprintf(temp,TEMP_MAX,"%d.%d;%s", icalenum_reqstat_major(stat.code),
             icalenum_reqstat_minor(stat.code),
             stat.desc);
  }

  return temp;
}
