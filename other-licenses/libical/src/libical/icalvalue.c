/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalvalue.c
  CREATOR: eric 02 May 1999
  
  $Id: icalvalue.c,v 1.6 2003/02/18 17:18:44 mostafah%oeone.com Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvalue.c

  Contributions from:
     Graham Davison (g.m.davison@computer.org)


======================================================================*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icalerror.h"
#include "icalmemory.h"
#include "icalparser.h"
#include "icalenums.h"
#include "icalvalueimpl.h"

#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for sprintf */
#include <string.h> /* For memset, others */
#include <stddef.h> /* For offsetof() macro */
#include <errno.h>
#include <time.h> /* for mktime */
#include <stdlib.h> /* for atoi and atof */
#include <limits.h> /* for SHRT_MAX */         

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

#if _MAC_OS_
#include "icalmemory_strdup.h"
#endif

#define TMP_BUF_SIZE 1024

void print_datetime_to_string(char* str,  struct icaltimetype *data);
void print_date_to_string(char* str,  struct icaltimetype *data);
void print_time_to_string(char* str,  struct icaltimetype *data);
void print_recur_to_string(char* str,  struct icaltimetype *data);


struct icalvalue_impl*  icalvalue_new_impl(icalvalue_kind kind){

    struct icalvalue_impl* v;

    if ( ( v = (struct icalvalue_impl*)
	   malloc(sizeof(struct icalvalue_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(v->id,"val");
    
    v->kind = kind;
    v->size = 0;
    v->parent = 0;
    v->x_value = 0;
    memset(&(v->data),0,sizeof(v->data));
    
    return v;

}



icalvalue*
icalvalue_new (icalvalue_kind kind)
{
    return (icalvalue*)icalvalue_new_impl(kind);
}

icalvalue* icalvalue_new_clone(icalvalue* value){

    struct icalvalue_impl* new;
    struct icalvalue_impl* old = (struct icalvalue_impl*)value;

    new = icalvalue_new_impl(old->kind);

    if (new == 0){
	return 0;
    }

    
    strcpy(new->id, old->id);
    new->kind = old->kind;
    new->size = old->size;

    switch (new->kind){

	/* The contents of the attach value may or may not be owned by the 
	 * library. */
	case ICAL_ATTACH_VALUE: 
	case ICAL_BINARY_VALUE: 
	{
	    /* HACK ugh. I don't feel like impleenting this */
	}

	case ICAL_STRING_VALUE:
	case ICAL_TEXT_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_URI_VALUE:
	{
	    if (old->data.v_string != 0) { 
		new->data.v_string=icalmemory_strdup(old->data.v_string);

		if ( new->data.v_string == 0 ) {
		    return 0;
		}		    

	    }
	    break;
	}
	case ICAL_RECUR_VALUE:
	{
	    if(old->data.v_recur != 0){
		new->data.v_recur = malloc(sizeof(struct icalrecurrencetype));

		if(new->data.v_recur == 0){
		    return 0;
		}

		memcpy(	new->data.v_recur, old->data.v_recur,
			sizeof(struct icalrecurrencetype));	
	    }
	    break;
	}

	default:
	{
	    /* all of the other types are stored as values, not
               pointers, so we can just copy the whole structure. */

	    new->data = old->data;
	}
    }

    return new;
}

char* icalmemory_strdup_and_dequote(const char* str)
{
    const char* p;
    char* out = (char*)malloc(sizeof(char) * strlen(str) +1);
    char* pout;

    if (out == 0){
	return 0;
    }

    pout = out;

    for (p = str; *p!=0; p++){
	
	if( *p == '\\')
	{
	    p++;
	    switch(*p){
		case 0:
		{
		    *pout = '\0';
		    break;

		}
		case 'n':
		case 'N':
		{
		    *pout = '\n';
		    break;
		}
		case 't':
		case 'T':
		{
		    *pout = '\t';
		    break;
		}
		case 'r':
		case 'R':
		{
		    *pout = '\r';
		    break;
		}
		case 'b':
		case 'B':
		{
		    *pout = '\b';
		    break;
		}
		case 'f':
		case 'F':
		{
		    *pout = '\f';
		    break;
		}
		case ';':
		case ',':
		case '"':
		case '\\':
		{
		    *pout = *p;
		    break;
		}
		default:
		{
		    *pout = ' ';
		}		
	    }
	} else {
	    *pout = *p;
	}

	pout++;
	
    }

    *pout = '\0';

    return out;
}

icalvalue* icalvalue_new_enum(icalvalue_kind kind, int x_type, const char* str)
{
    int e = icalproperty_string_to_enum(str);
    struct icalvalue_impl *value; 

    if(e != 0 && icalproperty_enum_belongs_to_property(
                   icalproperty_value_kind_to_kind(kind),e)) {
        
        value = icalvalue_new_impl(kind);
        value->data.v_enum = e;
    } else {
        /* Make it an X value */
        value = icalvalue_new_impl(kind);
        value->data.v_enum = x_type;
        icalvalue_set_x(value,str);
    }

    return value;
}


icalvalue* icalvalue_new_from_string_with_error(icalvalue_kind kind,const char* str,icalproperty** error)
{

    struct icalvalue_impl *value = 0;
    
    icalerror_check_arg_rz(str!=0,"str");

    if (error != 0){
	*error = 0;
    }

    switch (kind){
	
    case ICAL_ATTACH_VALUE:
    case ICAL_BINARY_VALUE:	
    case ICAL_BOOLEAN_VALUE:
        {
            /* HACK */
            value = 0;
            
	    if (error != 0){
		char temp[TMP_BUF_SIZE];
		sprintf(temp,"%s Values are not implemented",
                        icalparameter_kind_to_string(kind)); 
		*error = icalproperty_vanew_xlicerror( 
                                   temp, 
                                   icalparameter_new_xlicerrortype( 
                                        ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
                                   0); 
	    }
	    break;
	}
        

    case ICAL_TRANSP_VALUE:
        value = icalvalue_new_enum(kind, ICAL_TRANSP_X,str);
        break;
    case ICAL_METHOD_VALUE:
        value = icalvalue_new_enum(kind, ICAL_METHOD_X,str);
        break;
    case ICAL_STATUS_VALUE:
        value = icalvalue_new_enum(kind, ICAL_STATUS_X,str);
        break;
    case ICAL_ACTION_VALUE:
        value = icalvalue_new_enum(kind, ICAL_ACTION_X,str);
        break;
    case ICAL_CLASS_VALUE:
        value = icalvalue_new_enum(kind, ICAL_CLASS_X,str);
        break;


    case ICAL_INTEGER_VALUE:
	{
	    value = icalvalue_new_integer(atoi(str));
	    break;
	}

    case ICAL_FLOAT_VALUE:
        {
	    value = icalvalue_new_float(atof(str));
	    break;
	}
        
    case ICAL_UTCOFFSET_VALUE:
	{
	    value = icalparser_parse_value(kind,str,(icalcomponent*)0);
	    break;
	}
        
    case ICAL_TEXT_VALUE:
	{
	    char* dequoted_str = icalmemory_strdup_and_dequote(str);
	    value = icalvalue_new_text(dequoted_str);
	    free(dequoted_str);
	    break;
	}
        
        
    case ICAL_STRING_VALUE:
	{
	    value = icalvalue_new_string(str);
	    break;
	}
        
    case ICAL_CALADDRESS_VALUE:
	{
	    value = icalvalue_new_caladdress(str);
	    break;
	}
        
    case ICAL_URI_VALUE:
	{
	    value = icalvalue_new_uri(str);
	    break;
	}
        
        
    case ICAL_GEO_VALUE:
	{
	    value = 0;
	    /* HACK */
            
	    if (error != 0){
		char temp[TMP_BUF_SIZE];
		sprintf(temp,"GEO Values are not implemented"); 
		*error = icalproperty_vanew_xlicerror( 
                                                      temp, 
                                                      icalparameter_new_xlicerrortype( 
                                                                                      ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
                                                      0); 
	    }
            
	    /*icalerror_warn("Parsing GEO properties is unimplmeneted");*/
            
	    break;
	}
        
    case ICAL_RECUR_VALUE:
	{
	    struct icalrecurrencetype rt;
	    rt = icalrecurrencetype_from_string(str);
            if(rt.freq != ICAL_NO_RECURRENCE){
                value = icalvalue_new_recur(rt);
            }
	    break;
	}
        
    case ICAL_DATE_VALUE:
    case ICAL_DATETIME_VALUE:
	{
	    struct icaltimetype tt;
      
	    tt = icaltime_from_string(str);
            if(!icaltime_is_null_time(tt)){
                value = icalvalue_new_impl(kind);
                value->data.v_time = tt;

                icalvalue_reset_kind(value);
            }
	    break;
	}
        
    case ICAL_DATETIMEPERIOD_VALUE:
	{
	    struct icaltimetype tt;
            struct icalperiodtype p;
            tt = icaltime_from_string(str);
            p = icalperiodtype_from_string(str);
            
            if(!icaltime_is_null_time(tt)){
                value = icalvalue_new_datetime(tt);
            } else if (!icalperiodtype_is_null_period(p)){
                value = icalvalue_new_period(p);
            }            
            
            break;
	}
        
    case ICAL_DURATION_VALUE:
	{
            struct icaldurationtype dur = icaldurationtype_from_string(str);
            
            if(icaldurationtype_is_null_duration(dur)){
                value = 0;
            } else {
                value = icalvalue_new_duration(dur);
            }
            
	    break;
	}
        
    case ICAL_PERIOD_VALUE:
	{
            struct icalperiodtype p;
            p = icalperiodtype_from_string(str);  
            
            if(!icalperiodtype_is_null_period(p)){
                value = icalvalue_new_period(p);
            }
            break; 
	}
	
    case ICAL_TRIGGER_VALUE:
	{
	    struct icaltriggertype tr = icaltriggertype_from_string(str);
            if (!icaltriggertype_is_null_trigger(tr)){
                value = icalvalue_new_trigger(tr);
            }
	    break;
	}
        
    case ICAL_REQUESTSTATUS_VALUE:
        {
            struct icalreqstattype rst = icalreqstattype_from_string(str);
            if(rst.code != ICAL_UNKNOWN_STATUS){
                value = icalvalue_new_requeststatus(rst);
            }
            break;

        }

    case ICAL_X_VALUE:
        {
            char* dequoted_str = icalmemory_strdup_and_dequote(str);
            value = icalvalue_new_x(dequoted_str);
            free(dequoted_str);
        }
        break;

    default:
        {
            
            if (error != 0 ){
		char temp[TMP_BUF_SIZE];
                
                snprintf(temp,TMP_BUF_SIZE,"Unknown type for \'%s\'",str);
			    
		*error = icalproperty_vanew_xlicerror( 
		    temp, 
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    icalerror_warn("icalvalue_new_from_string got an unknown value type");
            value=0;
	}
    }


    if (error != 0 && *error == 0 && value == 0){
	char temp[TMP_BUF_SIZE];
	
        snprintf(temp,TMP_BUF_SIZE,"Failed to parse value: \'%s\'",str);
	
	*error = icalproperty_vanew_xlicerror( 
	    temp, 
	    icalparameter_new_xlicerrortype( 
		ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
	    0); 
    }


    return value;

}

icalvalue* icalvalue_new_from_string(icalvalue_kind kind,const char* str)
{
    return icalvalue_new_from_string_with_error(kind,str,(icalproperty*)0);
}



void
icalvalue_free (icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    icalerror_check_arg_rv((value != 0),"value");

#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (v->parent ==0),"This value is still attached to a property");
    
#else
    if(v->parent !=0){
	return;
    }
#endif

    if(v->x_value != 0){
        free(v->x_value);
    }

    switch (v->kind){
	case ICAL_BINARY_VALUE: 
	case ICAL_ATTACH_VALUE: {
	    /* HACK ugh. This will be tough to implement */
	}
	case ICAL_TEXT_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_URI_VALUE:
	{
	    if (v->data.v_string != 0) { 
		free((void*)v->data.v_string);
		v->data.v_string = 0;
	    }
	    break;
	}
	case ICAL_RECUR_VALUE:
	{
	    if(v->data.v_recur != 0){
		free((void*)v->data.v_recur);
		v->data.v_recur = 0;
	    }
	    break;
	}

	default:
	{
	    /* Nothing to do */
	}
    }

    v->kind = ICAL_NO_VALUE;
    v->size = 0;
    v->parent = 0;
    memset(&(v->data),0,sizeof(v->data));
    v->id[0] = 'X';
    free(v);
}

int
icalvalue_is_valid (icalvalue* value)
{
    /*struct icalvalue_impl* v = (struct icalvalue_impl*)value;*/
    
    if(value == 0){
	return 0;
    }
    
    return 1;
}

char* icalvalue_binary_as_ical_string(icalvalue* value) {

    const char* data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_binary(value);

    str = (char*)icalmemory_tmp_buffer(60);
    sprintf(str,"icalvalue_binary_as_ical_string is not implemented yet");

    return str;
}


#define MAX_INT_DIGITS 12 /* Enough for 2^32 + sign*/ 
char* icalvalue_int_as_ical_string(icalvalue* value) {
    
    int data;
    char* str = (char*)icalmemory_tmp_buffer(MAX_INT_DIGITS); 

    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_integer(value);
	
    snprintf(str,MAX_INT_DIGITS,"%d",data);

    return str;
}

char* icalvalue_utcoffset_as_ical_string(icalvalue* value)
{    
    int data,h,m,s;
    char sign;
    char* str = (char*)icalmemory_tmp_buffer(9);

    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_utcoffset(value);

    if (abs(data) == data){
	sign = '+';
    } else {
	sign = '-';
    }

    h = data/3600;
    m = (data - (h*3600))/ 60;
    s = (data - (h*3600) - (m*60));

    sprintf(str,"%c%02d%02d%02d",sign,abs(h),abs(m),abs(s));

    return str;
}

char* icalvalue_string_as_ical_string(icalvalue* value) {

    const char* data;
    char* str = 0;
    icalerror_check_arg_rz( (value!=0),"value");
    data = ((struct icalvalue_impl*)value)->data.v_string;

    str = (char*)icalmemory_tmp_buffer(strlen(data)+1);   

    strcpy(str,data);

    return str;
}


char* icalvalue_recur_as_ical_string(icalvalue* value) 
{
    struct icalvalue_impl *impl = (struct icalvalue_impl*)value;
    struct icalrecurrencetype *recur = impl->data.v_recur;

    return icalrecurrencetype_as_string(recur);
}

char* icalvalue_text_as_ical_string(icalvalue* value) {

    char *str;
    char *str_p;
    char *rtrn;
    const char *p;
    size_t buf_sz;
    int line_length;

    line_length = 0;

    buf_sz = strlen(((struct icalvalue_impl*)value)->data.v_string)+1;

    str_p = str = (char*)icalmemory_new_buffer(buf_sz);

    if (str_p == 0){
      return 0;
    }

    for(p=((struct icalvalue_impl*)value)->data.v_string; *p!=0; p++){

	switch(*p){
	    case '\n': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\n");
		line_length+=3;
		break;
	    }

	    case '\t': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\t");
		line_length+=3;
		break;
	    }
	    case '\r': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\r");
		line_length+=3;
		break;
	    }
	    case '\b': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\b");
		line_length+=3;
		break;
	    }
	    case '\f': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\f");
		line_length+=3;
		break;
	    }

	    case ';':
	    case ',':
	    case '"':
	    case '\\':{
		icalmemory_append_char(&str,&str_p,&buf_sz,'\\');
		icalmemory_append_char(&str,&str_p,&buf_sz,*p);
		line_length+=3;
		break;
	    }

	    default: {
		icalmemory_append_char(&str,&str_p,&buf_sz,*p);
		line_length++;
	    }
	}

	if (line_length > 65 && *p == ' '){
	    icalmemory_append_string(&str,&str_p,&buf_sz,"\n ");
	    line_length=0;
	}


	if (line_length > 75){
	    icalmemory_append_string(&str,&str_p,&buf_sz,"\n ");
	    line_length=0;
	}

    }

    /* Assume the last character is not a '\0' and add one. We could
       check *str_p != 0, but that would be an uninitialized memory
       read. */


    icalmemory_append_char(&str,&str_p,&buf_sz,'\0');

    rtrn = icalmemory_tmp_copy(str);

    icalmemory_free_buffer(str);

    return rtrn;
}


char* icalvalue_attach_as_ical_string(icalvalue* value) {

    struct icalattachtype a;
    char * str;

    icalerror_check_arg_rz( (value!=0),"value");

    a = icalvalue_get_attach(value);

    if (a.binary != 0) {
	return  icalvalue_binary_as_ical_string(value);
    } else if (a.base64 != 0) {
	str = (char*)icalmemory_tmp_buffer(strlen(a.base64)+1);
	strcpy(str,a.base64);
	return str;
    } else if (a.url != 0){
	return icalvalue_string_as_ical_string(value);
    } else {
	icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	return 0;
    }
}


char* icalvalue_duration_as_ical_string(icalvalue* value) {

    struct icaldurationtype data;

    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_duration(value);

    return icaldurationtype_as_ical_string(data);
}

void print_time_to_string(char* str,  struct icaltimetype *data)
{
    char temp[20];

    if (data->is_utc == 1){
	sprintf(temp,"%02d%02d%02dZ",data->hour,data->minute,data->second);
    } else {
	sprintf(temp,"%02d%02d%02d",data->hour,data->minute,data->second);
    }   

    strcat(str,temp);
}

 
void print_date_to_string(char* str,  struct icaltimetype *data)
{
    char temp[20];

    sprintf(temp,"%04d%02d%02d",data->year,data->month,data->day);

    strcat(str,temp);
}

char* icalvalue_date_as_ical_string(icalvalue* value) {

    struct icaltimetype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_date(value);

    str = (char*)icalmemory_tmp_buffer(9);
 
    str[0] = 0;
    print_date_to_string(str,&data);
   
    return str;
}

void print_datetime_to_string(char* str,  struct icaltimetype *data)
{
    print_date_to_string(str,data);
    strcat(str,"T");
    print_time_to_string(str,data);

}

const char* icalvalue_datetime_as_ical_string(icalvalue* value) {
    
    struct icaltimetype data;
    char* str;
    icalvalue_kind kind = icalvalue_isa(value);    

    icalerror_check_arg_rz( (value!=0),"value");


    if( !(kind == ICAL_DATE_VALUE || kind == ICAL_DATETIME_VALUE ))
	{
	    icalerror_set_errno(ICAL_BADARG_ERROR);
	    return 0;
	}

    data = icalvalue_get_datetime(value);

    str = (char*)icalmemory_tmp_buffer(20);
 
    str[0] = 0;

    print_datetime_to_string(str,&data);
   
    return str;

}

char* icalvalue_float_as_ical_string(icalvalue* value) {

    float data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_float(value);

    str = (char*)icalmemory_tmp_buffer(15);

    sprintf(str,"%f",data);

    return str;
}

char* icalvalue_geo_as_ical_string(icalvalue* value) {

    struct icalgeotype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_geo(value);

    str = (char*)icalmemory_tmp_buffer(25);

    sprintf(str,"%f;%f",data.lat,data.lon);

    return str;
}

const char* icalvalue_datetimeperiod_as_ical_string(icalvalue* value) {
    struct icaldatetimeperiodtype dtp = icalvalue_get_datetimeperiod(value);

    icalerror_check_arg_rz( (value!=0),"value");

    if(!icaltime_is_null_time(dtp.time)){
	return icaltime_as_ical_string(dtp.time);
    } else {
	return icalperiodtype_as_ical_string(dtp.period);
    }
}

const char* icalvalue_period_as_ical_string(icalvalue* value) {
    struct icalperiodtype data;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_period(value);

    return icalperiodtype_as_ical_string(data);

}

char* icalvalue_trigger_as_ical_string(icalvalue* value) {

    struct icaltriggertype data;

    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_trigger(value);

    if(!icaltime_is_null_time(data.time)){
	return icaltime_as_ical_string(data.time);
    } else {
	return icaldurationtype_as_ical_string(data.duration);
    }   

}

const char*
icalvalue_as_ical_string (icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    v=v;

    if(value == 0){
	return 0;
    }

    switch (v->kind){

    case ICAL_ATTACH_VALUE:
        return icalvalue_attach_as_ical_string(value);
        
    case ICAL_BINARY_VALUE:
        return icalvalue_binary_as_ical_string(value);
        
    case ICAL_BOOLEAN_VALUE:
    case ICAL_INTEGER_VALUE:
        return icalvalue_int_as_ical_string(value);                  
        
    case ICAL_UTCOFFSET_VALUE:
        return icalvalue_utcoffset_as_ical_string(value);                  
        
    case ICAL_TEXT_VALUE:
        return icalvalue_text_as_ical_string(value);
        
    case ICAL_STRING_VALUE:
    case ICAL_URI_VALUE:
    case ICAL_CALADDRESS_VALUE:
        return icalvalue_string_as_ical_string(value);
        
    case ICAL_DATE_VALUE:
        return icalvalue_date_as_ical_string(value);
    case ICAL_DATETIME_VALUE:
        return icalvalue_datetime_as_ical_string(value);
    case ICAL_DURATION_VALUE:
        return icalvalue_duration_as_ical_string(value);
        
    case ICAL_PERIOD_VALUE:
        return icalvalue_period_as_ical_string(value);
    case ICAL_DATETIMEPERIOD_VALUE:
        return icalvalue_datetimeperiod_as_ical_string(value);
        
    case ICAL_FLOAT_VALUE:
        return icalvalue_float_as_ical_string(value);
        
    case ICAL_GEO_VALUE:
        return icalvalue_geo_as_ical_string(value);
        
    case ICAL_RECUR_VALUE:
        return icalvalue_recur_as_ical_string(value);
        
    case ICAL_TRIGGER_VALUE:
        return icalvalue_trigger_as_ical_string(value);

    case ICAL_REQUESTSTATUS_VALUE:
        return icalreqstattype_as_string(v->data.v_requeststatus);
        
    case ICAL_ACTION_VALUE:
    case ICAL_METHOD_VALUE:
    case ICAL_STATUS_VALUE:
    case ICAL_TRANSP_VALUE:
    case ICAL_CLASS_VALUE:
        if(v->x_value !=0){
            return icalmemory_tmp_copy(v->x_value);
        }

        return icalproperty_enum_to_string(v->data.v_enum);
        
    case ICAL_X_VALUE: 
        return icalmemory_tmp_copy(v->x_value);

    case ICAL_NO_VALUE:
    default:
	{
	    return 0;
	}
    }
}


icalvalue_kind
icalvalue_isa (icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    if(value == 0){
	return ICAL_NO_VALUE;
    }

    return v->kind;
}


int
icalvalue_isa_value (void* value)
{
    struct icalvalue_impl *impl = (struct icalvalue_impl *)value;

    icalerror_check_arg_rz( (value!=0), "value");

    if (strcmp(impl->id,"val") == 0) {
	return 1;
    } else {
	return 0;
    }
}


int icalvalue_is_time(icalvalue* a) {
    icalvalue_kind kind = icalvalue_isa(a);

    if(kind == ICAL_DATETIME_VALUE ||
       kind == ICAL_DATE_VALUE ){
	return 1;
    }

    return 0;

}

icalparameter_xliccomparetype
icalvalue_compare(icalvalue* a, icalvalue *b)
{
    struct icalvalue_impl *impla = (struct icalvalue_impl *)a;
    struct icalvalue_impl *implb = (struct icalvalue_impl *)b;

    icalerror_check_arg_rz( (a!=0), "a");
    icalerror_check_arg_rz( (b!=0), "b");

    /* Not the same type; they can only be unequal */
    if( ! (icalvalue_is_time(a) && icalvalue_is_time(b)) &&
	icalvalue_isa(a) != icalvalue_isa(b)){
	return ICAL_XLICCOMPARETYPE_NOTEQUAL;
    }

    switch (icalvalue_isa(a)){

	case ICAL_ATTACH_VALUE:
	case ICAL_BINARY_VALUE:

	case ICAL_BOOLEAN_VALUE:
	{
	    if (icalvalue_get_boolean(a) == icalvalue_get_boolean(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }
	}

	case ICAL_FLOAT_VALUE:
	{
	    if (impla->data.v_float > implb->data.v_float){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (impla->data.v_float < implb->data.v_float){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}

	case ICAL_INTEGER_VALUE:
	case ICAL_UTCOFFSET_VALUE:
	{
	    if (impla->data.v_int > implb->data.v_int){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (impla->data.v_int < implb->data.v_int){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}

	case ICAL_DURATION_VALUE: 
	{
	    int a = icaldurationtype_as_int(impla->data.v_duration);
	    int b = icaldurationtype_as_int(implb->data.v_duration);

	    if (a > b){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (a < b){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}	    


	case ICAL_TEXT_VALUE:
	case ICAL_URI_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_TRIGGER_VALUE:
	case ICAL_DATE_VALUE:
	case ICAL_DATETIME_VALUE:
	case ICAL_DATETIMEPERIOD_VALUE:
	{
	    int r;

	    r =  strcmp(icalvalue_as_ical_string(a),
			  icalvalue_as_ical_string(b));

	    if (r > 0) { 	
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (r < 0){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }

		
	}

	case ICAL_METHOD_VALUE:
	{
	    if (icalvalue_get_method(a) == icalvalue_get_method(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }

	}

	case ICAL_STATUS_VALUE:
	{
	    if (icalvalue_get_status(a) == icalvalue_get_status(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }

	}

	case ICAL_PERIOD_VALUE:
	case ICAL_GEO_VALUE:
	case ICAL_RECUR_VALUE:
	case ICAL_NO_VALUE:
	default:
	{
	    icalerror_warn("Comparison not implemented for value type");
	    return ICAL_XLICCOMPARETYPE_REGEX+1; /* HACK */
	}
    }   

}

/* Examine the value and possiby chage the kind to agree with the value */
void icalvalue_reset_kind(icalvalue* value)
{
    struct icalvalue_impl* impl = (struct icalvalue_impl*)value;

    if( (impl->kind==ICAL_DATETIME_VALUE || impl->kind==ICAL_DATE_VALUE )&&
        !icaltime_is_null_time(impl->data.v_time) ) {
        
        if( impl->data.v_time.is_date == 1){
            impl->kind = ICAL_DATE_VALUE;
        } else {
            impl->kind = ICAL_DATETIME_VALUE;
        }
    }
       
}

void icalvalue_set_parent(icalvalue* value,
			     icalproperty* property)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    v->parent = property;

}

icalproperty* icalvalue_get_parent(icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;


    return v->parent;
}



/* The remaining interfaces are 'new', 'set' and 'get' for each of the value
   types */


/* Everything below this line is machine generated. Do not edit. */
