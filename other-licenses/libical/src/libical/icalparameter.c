/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedparameters.{c,h}
  CREATOR: eric 09 May 1999
  
  $Id: icalparameter.c,v 1.2 2001/12/21 18:56:22 mikep%oeone.com Exp $
  $Locker:  $
    

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalderivedparameters.{c,h}

  Contributions from:
     Graham Davison (g.m.davison@computer.org)

 ======================================================================*/
/*#line 29 "icalparameter.c.in"*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "icalparameter.h"
#include "icalproperty.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalparameterimpl.h"

#include <stdlib.h> /* for malloc() */
#include <errno.h>
#include <string.h> /* for memset() */

/* In icalderivedparameter */
icalparameter* icalparameter_new_from_value_string(icalparameter_kind kind,const  char* val);


struct icalparameter_impl* icalparameter_new_impl(icalparameter_kind kind)
{
    struct icalparameter_impl* v;

    if ( ( v = (struct icalparameter_impl*)
	   malloc(sizeof(struct icalparameter_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(v->id,"para");

    v->kind = kind;
    v->size = 0;
    v->string = 0;
    v->x_name = 0;
    v->parent = 0;
    v->data = 0;

    return v;
}

icalparameter*
icalparameter_new (icalparameter_kind kind)
{
    struct icalparameter_impl* v = icalparameter_new_impl(kind);

    return (icalparameter*) v;

}

void
icalparameter_free (icalparameter* parameter)
{
    struct icalparameter_impl * impl;

    impl = (struct icalparameter_impl*)parameter;

/*  HACK. This always triggers, even when parameter is non-zero
    icalerror_check_arg_rv((parameter==0),"parameter");*/


#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (impl->parent ==0),"Tried to free a parameter that is still attached to a component. ");
    
#else
    if(impl->parent !=0){
	return;
    }
#endif

    
    if (impl->string != 0){
	free ((void*)impl->string);
    }
    
    if (impl->x_name != 0){
	free ((void*)impl->x_name);
    }
    
    memset(impl,0,sizeof(impl));

    impl->parent = 0;
    impl->id[0] = 'X';
    free(impl);
}



icalparameter* 
icalparameter_new_clone(icalparameter* param)
{
    struct icalparameter_impl *old;
    struct icalparameter_impl *new;

    old = (struct icalparameter_impl *)param;
    new = icalparameter_new_impl(old->kind);

    icalerror_check_arg_rz((param!=0),"param");

    if (new == 0){
	return 0;
    }

    memcpy(new,old,sizeof(struct icalparameter_impl));

    if (old->string != 0){
	new->string = icalmemory_strdup(old->string);
	if (new->string == 0){
	    icalparameter_free(new);
	    return 0;
	}
    }

    if (old->x_name != 0){
	new->x_name = icalmemory_strdup(old->x_name);
	if (new->x_name == 0){
	    icalparameter_free(new);
	    return 0;
	}
    }

    return new;
}

icalparameter* icalparameter_new_from_string(const char *str)
{
    char* eq;
    char* cpy;
    icalparameter_kind kind;
    icalparameter *param;

    icalerror_check_arg_rz(str != 0,"str");

    cpy = icalmemory_strdup(str);

    if (cpy == 0){
        icalerror_set_errno(ICAL_NEWFAILED_ERROR);
        return 0;
    }

    eq = strchr(cpy,'=');

    if(eq == 0){
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return 0;
    }

    *eq = '\0';

    eq++;

    kind = icalparameter_string_to_kind(cpy);

    if(kind == ICAL_NO_PARAMETER){
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return 0;
    }

    param = icalparameter_new_from_value_string(kind,eq);

    if(kind == ICAL_X_PARAMETER){
        icalparameter_set_xname(param,cpy);
    }

    free(cpy);

    return param;
    
}

char*
icalparameter_as_ical_string (icalparameter* parameter)
{
    struct icalparameter_impl* impl;
    size_t buf_size = 1024;
    char* buf; 
    char* buf_ptr;
    char *out_buf;
    const char *kind_string;

    icalerror_check_arg_rz( (parameter!=0), "parameter");

    /* Create new buffer that we can append names, parameters and a
       value to, and reallocate as needed. Later, this buffer will be
       copied to a icalmemory_tmp_buffer, which is managed internally
       by libical, so it can be given to the caller without fear of
       the caller forgetting to free it */

    buf = icalmemory_new_buffer(buf_size);
    buf_ptr = buf;
    impl = (struct icalparameter_impl*)parameter;

    if(impl->kind == ICAL_X_PARAMETER) {

	icalmemory_append_string(&buf, &buf_ptr, &buf_size, 
				 icalparameter_get_xname(impl));

    } else {

	kind_string = icalparameter_kind_to_string(impl->kind);
	
	if (impl->kind == ICAL_NO_PARAMETER || 
	    impl->kind == ICAL_ANY_PARAMETER || 
	    kind_string == 0)
	{
	    icalerror_set_errno(ICAL_BADARG_ERROR);
	    return 0;
	}
	
	
	/* Put the parameter name into the string */
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, kind_string);

    }

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "=");

    if(impl->string !=0){
        int qm = 0;

	/* Encapsulate the property in quotes if necessary */
	if (strchr (impl->string, ';') != 0 || strchr (impl->string, ':') != 0) {
		icalmemory_append_char (&buf, &buf_ptr, &buf_size, '"');
		qm = 1;
	}
        icalmemory_append_string(&buf, &buf_ptr, &buf_size, impl->string); 
	if (qm == 1) {
		icalmemory_append_char (&buf, &buf_ptr, &buf_size, '"');
	}
    } else if (impl->data != 0){
        const char* str = icalparameter_enum_to_string(impl->data);
        icalmemory_append_string(&buf, &buf_ptr, &buf_size, str); 
    } else {
        icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
        return 0;
    }

    /* Now, copy the buffer to a tmp_buffer, which is safe to give to
       the caller without worring about de-allocating it. */
    
    out_buf = icalmemory_tmp_buffer(strlen(buf));
    strcpy(out_buf, buf);

    icalmemory_free_buffer(buf);

    return out_buf;

}


int
icalparameter_is_valid (icalparameter* parameter);


icalparameter_kind
icalparameter_isa (icalparameter* parameter)
{
    if(parameter == 0){
	return ICAL_NO_PARAMETER;
    }

    return ((struct icalparameter_impl *)parameter)->kind;
}


int
icalparameter_isa_parameter (void* parameter)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl *)parameter;

    if (parameter == 0){
	return 0;
    }

    if (strcmp(impl->id,"para") == 0) {
	return 1;
    } else {
	return 0;
    }
}


void
icalparameter_set_xname (icalparameter* param, const char* v)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;
    icalerror_check_arg_rv( (param!=0),"param");
    icalerror_check_arg_rv( (v!=0),"v");

    if (impl->x_name != 0){
	free((void*)impl->x_name);
    }

    impl->x_name = icalmemory_strdup(v);

    if (impl->x_name == 0){
	errno = ENOMEM;
    }

}

const char*
icalparameter_get_xname (icalparameter* param)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;
    icalerror_check_arg_rz( (param!=0),"param");

    return impl->x_name;
}

void
icalparameter_set_xvalue (icalparameter* param, const char* v)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rv( (param!=0),"param");
    icalerror_check_arg_rv( (v!=0),"v");

    if (impl->string != 0){
	free((void*)impl->string);
    }

    impl->string = icalmemory_strdup(v);

    if (impl->string == 0){
	errno = ENOMEM;
    }

}

const char*
icalparameter_get_xvalue (icalparameter* param)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rz( (param!=0),"param");

    return impl->string;

}

void icalparameter_set_parent(icalparameter* param,
			     icalproperty* property)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rv( (param!=0),"param");

    impl->parent = property;
}

icalproperty* icalparameter_get_parent(icalparameter* param)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rz( (param!=0),"param");

    return impl->parent;
}


/* Everything below this line is machine generated. Do not edit. */
/* ALTREP */
