/* -*- Mode: C -*- */

/*======================================================================
  FILE: icalderivedproperty.c
  CREATOR: eric 15 Feb 2001
  
  $Id: icalderivedproperty.c,v 1.3 2002/11/06 21:22:38 mostafah%oeone.com Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalproperty.c

======================================================================*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icalproperty.h"
#include "icalcomponent.h"
#include "pvl.h"
#include "icalenums.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalparser.h"

#include <string.h> /* For icalmemory_strdup, rindex */
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h> /* for printf */
#include <stdarg.h> /* for va_list, va_start, etc. */
                                               
#define TMP_BUF_SIZE 1024

struct icalproperty_impl*
icalproperty_new_impl (icalproperty_kind kind);

void icalproperty_add_parameters(struct icalproperty_impl *prop,va_list args);

/* This map associates the property kinds with the string
   representation of the property name and the kind of VALUE that the
   property uses as a default */

struct  icalproperty_map {
	icalproperty_kind kind;
	const char *name;
	icalvalue_kind value;

};

extern struct icalproperty_map property_map[];

const char* icalproperty_kind_to_string(icalproperty_kind kind)
{
    int i;

    for (i=0; property_map[i].kind != ICAL_NO_PROPERTY; i++) {
	if (property_map[i].kind == kind) {
	    return property_map[i].name;
	}
    }

    return 0;

}


icalproperty_kind icalproperty_string_to_kind(const char* string)
{
    int i;

    if (string ==0 ) { 
	return ICAL_NO_PROPERTY;
    }


    for (i=0; property_map[i].kind  != ICAL_NO_PROPERTY; i++) {
	if (strcmp(property_map[i].name, string) == 0) {
	    return property_map[i].kind;
	}
    }

    if(strncmp(string,"X-",2)==0){
	return ICAL_X_PROPERTY;
    }


    return ICAL_NO_PROPERTY;
}


icalvalue_kind icalproperty_value_kind_to_kind(icalvalue_kind kind)
{
    int i;

    for (i=0; property_map[i].kind  != ICAL_NO_PROPERTY; i++) {
	if ( property_map[i].value == kind ) {
	    return property_map[i].kind;
	}
    }

    return ICAL_NO_VALUE;
}



icalvalue_kind icalproperty_kind_to_value_kind(icalproperty_kind kind)
{
    int i;

    for (i=0; property_map[i].kind  != ICAL_NO_PROPERTY; i++) {
	if ( property_map[i].kind == kind ) {
	    return property_map[i].value;
	}
    }

    return ICAL_NO_VALUE;
}


/* This map associates the property enumerations with the king of
   property that they are used in and the string representation of the
   enumeration */

struct icalproperty_enum_map {
    icalproperty_kind prop;
    int prop_enum;
    const char* str;
}; 

extern struct icalproperty_enum_map enum_map[];


const char* icalproperty_enum_to_string(int e)
{
    icalerror_check_arg_rz(e >= ICALPROPERTY_FIRST_ENUM,"e");
    icalerror_check_arg_rz(e <= ICALPROPERTY_LAST_ENUM,"e");

    return enum_map[e-ICALPROPERTY_FIRST_ENUM].str;
}

int icalproperty_string_to_enum(const char* str)
{
    int i;

    icalerror_check_arg_rz(str!=0,"str")

    while(*str == ' '){
	str++;
    }

    for (i=ICALPROPERTY_FIRST_ENUM; i != ICALPROPERTY_LAST_ENUM; i++) {
	if ( strcmp(enum_map[i-ICALPROPERTY_FIRST_ENUM].str, str) == 0) {
	    return enum_map[i-ICALPROPERTY_FIRST_ENUM].prop_enum;
	}
    }

    return 0;
}

int icalproperty_enum_belongs_to_property(icalproperty_kind kind, int e)
{
    int i;


    for (i=ICALPROPERTY_FIRST_ENUM; i != ICALPROPERTY_LAST_ENUM; i++) {
        if(enum_map[i-ICALPROPERTY_FIRST_ENUM].prop_enum == e && 
           enum_map[i-ICALPROPERTY_FIRST_ENUM].prop == kind ){
            return 1;
        }
    }

    return 0;
}


const char* icalproperty_method_to_string(icalproperty_method method)
{
    icalerror_check_arg_rz(method >= ICAL_METHOD_X,"method");
    icalerror_check_arg_rz(method <= ICAL_METHOD_NONE,"method");

    return enum_map[method-ICALPROPERTY_FIRST_ENUM].str;
}

icalproperty_method icalproperty_string_to_method(const char* str)
{
    int i;

    icalerror_check_arg_rx(str!=0,"str",ICAL_METHOD_NONE)

    while(*str == ' '){
	str++;
    }

    for (i=ICAL_METHOD_X-ICALPROPERTY_FIRST_ENUM; 
         i != ICAL_METHOD_NONE-ICALPROPERTY_FIRST_ENUM;
         i++) {
	if ( strcmp(enum_map[i].str, str) == 0) {
	    return (icalproperty_method)enum_map[i].prop_enum;
	}
    }

    return ICAL_METHOD_NONE;
}


const char* icalenum_status_to_string(icalproperty_status status)
{
    icalerror_check_arg_rz(status >= ICAL_STATUS_X,"status");
    icalerror_check_arg_rz(status <= ICAL_STATUS_NONE,"status");

    return enum_map[status-ICALPROPERTY_FIRST_ENUM].str;
}

icalproperty_status icalenum_string_to_status(const char* str)
{
    int i;

    icalerror_check_arg_rx(str!=0,"str",ICAL_STATUS_NONE)

    while(*str == ' '){
	str++;
    }

    for (i=ICAL_STATUS_X-ICALPROPERTY_FIRST_ENUM; 
         i != ICAL_STATUS_NONE-ICALPROPERTY_FIRST_ENUM;
         i++) {
	if ( strcmp(enum_map[i].str, str) == 0) {
	    return (icalproperty_method)enum_map[i].prop_enum;
	}
    }

    return ICAL_STATUS_NONE;

}



/* Everything below this line is machine generated. Do not edit. */
static struct icalproperty_map property_map[] = {
{ICAL_ACTION_PROPERTY,"ACTION",ICAL_ACTION_VALUE},
{ICAL_ANY_PROPERTY,"ANY",ICAL_NO_VALUE},
{ICAL_ATTACH_PROPERTY,"ATTACH",ICAL_ATTACH_VALUE},
{ICAL_ATTENDEE_PROPERTY,"ATTENDEE",ICAL_CALADDRESS_VALUE},
{ICAL_CALSCALE_PROPERTY,"CALSCALE",ICAL_TEXT_VALUE},
{ICAL_CATEGORIES_PROPERTY,"CATEGORIES",ICAL_TEXT_VALUE},
{ICAL_CLASS_PROPERTY,"CLASS",ICAL_CLASS_VALUE},
{ICAL_COMMENT_PROPERTY,"COMMENT",ICAL_TEXT_VALUE},
{ICAL_COMPLETED_PROPERTY,"COMPLETED",ICAL_DATETIME_VALUE},
{ICAL_CONTACT_PROPERTY,"CONTACT",ICAL_TEXT_VALUE},
{ICAL_CREATED_PROPERTY,"CREATED",ICAL_DATETIME_VALUE},
{ICAL_DESCRIPTION_PROPERTY,"DESCRIPTION",ICAL_TEXT_VALUE},
{ICAL_DTEND_PROPERTY,"DTEND",ICAL_DATETIME_VALUE},
{ICAL_DTSTAMP_PROPERTY,"DTSTAMP",ICAL_DATETIME_VALUE},
{ICAL_DTSTART_PROPERTY,"DTSTART",ICAL_DATETIME_VALUE},
{ICAL_DUE_PROPERTY,"DUE",ICAL_DATETIME_VALUE},
{ICAL_DURATION_PROPERTY,"DURATION",ICAL_DURATION_VALUE},
{ICAL_EXDATE_PROPERTY,"EXDATE",ICAL_DATETIME_VALUE},
{ICAL_EXRULE_PROPERTY,"EXRULE",ICAL_RECUR_VALUE},
{ICAL_FREEBUSY_PROPERTY,"FREEBUSY",ICAL_PERIOD_VALUE},
{ICAL_GEO_PROPERTY,"GEO",ICAL_GEO_VALUE},
{ICAL_LASTMODIFIED_PROPERTY,"LAST-MODIFIED",ICAL_DATETIME_VALUE},
{ICAL_LOCATION_PROPERTY,"LOCATION",ICAL_TEXT_VALUE},
{ICAL_MAXRESULTS_PROPERTY,"MAXRESULTS",ICAL_INTEGER_VALUE},
{ICAL_MAXRESULTSSIZE_PROPERTY,"MAXRESULTSSIZE",ICAL_INTEGER_VALUE},
{ICAL_METHOD_PROPERTY,"METHOD",ICAL_METHOD_VALUE},
{ICAL_ORGANIZER_PROPERTY,"ORGANIZER",ICAL_CALADDRESS_VALUE},
{ICAL_PERCENTCOMPLETE_PROPERTY,"PERCENT-COMPLETE",ICAL_INTEGER_VALUE},
{ICAL_PRIORITY_PROPERTY,"PRIORITY",ICAL_INTEGER_VALUE},
{ICAL_PRODID_PROPERTY,"PRODID",ICAL_TEXT_VALUE},
{ICAL_QUERY_PROPERTY,"QUERY",ICAL_QUERY_VALUE},
{ICAL_QUERYNAME_PROPERTY,"QUERYNAME",ICAL_TEXT_VALUE},
{ICAL_RDATE_PROPERTY,"RDATE",ICAL_DATETIMEPERIOD_VALUE},
{ICAL_RECURRENCEID_PROPERTY,"RECURRENCE-ID",ICAL_DATETIME_VALUE},
{ICAL_RELATEDTO_PROPERTY,"RELATED-TO",ICAL_TEXT_VALUE},
{ICAL_REPEAT_PROPERTY,"REPEAT",ICAL_INTEGER_VALUE},
{ICAL_REQUESTSTATUS_PROPERTY,"REQUEST-STATUS",ICAL_REQUESTSTATUS_VALUE},
{ICAL_RESOURCES_PROPERTY,"RESOURCES",ICAL_TEXT_VALUE},
{ICAL_RRULE_PROPERTY,"RRULE",ICAL_RECUR_VALUE},
{ICAL_SCOPE_PROPERTY,"SCOPE",ICAL_TEXT_VALUE},
{ICAL_SEQUENCE_PROPERTY,"SEQUENCE",ICAL_INTEGER_VALUE},
{ICAL_STATUS_PROPERTY,"STATUS",ICAL_STATUS_VALUE},
{ICAL_SUMMARY_PROPERTY,"SUMMARY",ICAL_TEXT_VALUE},
{ICAL_TARGET_PROPERTY,"TARGET",ICAL_CALADDRESS_VALUE},
{ICAL_TRANSP_PROPERTY,"TRANSP",ICAL_TEXT_VALUE},
{ICAL_TRIGGER_PROPERTY,"TRIGGER",ICAL_TRIGGER_VALUE},
{ICAL_TZID_PROPERTY,"TZID",ICAL_TEXT_VALUE},
{ICAL_TZNAME_PROPERTY,"TZNAME",ICAL_TEXT_VALUE},
{ICAL_TZOFFSETFROM_PROPERTY,"TZOFFSETFROM",ICAL_UTCOFFSET_VALUE},
{ICAL_TZOFFSETTO_PROPERTY,"TZOFFSETTO",ICAL_UTCOFFSET_VALUE},
{ICAL_TZURL_PROPERTY,"TZURL",ICAL_URI_VALUE},
{ICAL_UID_PROPERTY,"UID",ICAL_TEXT_VALUE},
{ICAL_URL_PROPERTY,"URL",ICAL_URI_VALUE},
{ICAL_VERSION_PROPERTY,"VERSION",ICAL_TEXT_VALUE},
{ICAL_X_PROPERTY,"X",ICAL_X_VALUE},
{ICAL_XLICCLASS_PROPERTY,"X-LIC-CLASS",ICAL_XLICCLASS_VALUE},
{ICAL_XLICCLUSTERCOUNT_PROPERTY,"X-LIC-CLUSTERCOUNT",ICAL_STRING_VALUE},
{ICAL_XLICERROR_PROPERTY,"X-LIC-ERROR",ICAL_TEXT_VALUE},
{ICAL_XLICMIMECHARSET_PROPERTY,"X-LIC-MIMECHARSET",ICAL_STRING_VALUE},
{ICAL_XLICMIMECID_PROPERTY,"X-LIC-MIMECID",ICAL_STRING_VALUE},
{ICAL_XLICMIMECONTENTTYPE_PROPERTY,"X-LIC-MIMECONTENTTYPE",ICAL_STRING_VALUE},
{ICAL_XLICMIMEENCODING_PROPERTY,"X-LIC-MIMEENCODING",ICAL_STRING_VALUE},
{ICAL_XLICMIMEFILENAME_PROPERTY,"X-LIC-MIMEFILENAME",ICAL_STRING_VALUE},
{ICAL_XLICMIMEOPTINFO_PROPERTY,"X-LIC-MIMEOPTINFO",ICAL_STRING_VALUE},
{ICAL_NO_PROPERTY,"",ICAL_NO_VALUE}};

static struct icalproperty_enum_map enum_map[] = {
    {ICAL_ACTION_PROPERTY,ICAL_ACTION_X,"" }, /*10000*/
    {ICAL_ACTION_PROPERTY,ICAL_ACTION_AUDIO,"AUDIO" }, /*10001*/
    {ICAL_ACTION_PROPERTY,ICAL_ACTION_DISPLAY,"DISPLAY" }, /*10002*/
    {ICAL_ACTION_PROPERTY,ICAL_ACTION_EMAIL,"EMAIL" }, /*10003*/
    {ICAL_ACTION_PROPERTY,ICAL_ACTION_PROCEDURE,"PROCEDURE" }, /*10004*/
    {ICAL_ACTION_PROPERTY,ICAL_ACTION_NONE,"" }, /*10005*/
    {ICAL_CLASS_PROPERTY,ICAL_CLASS_X,"" }, /*10006*/
    {ICAL_CLASS_PROPERTY,ICAL_CLASS_PUBLIC,"PUBLIC" }, /*10007*/
    {ICAL_CLASS_PROPERTY,ICAL_CLASS_PRIVATE,"PRIVATE" }, /*10008*/
    {ICAL_CLASS_PROPERTY,ICAL_CLASS_CONFIDENTIAL,"CONFIDENTIAL" }, /*10009*/
    {ICAL_CLASS_PROPERTY,ICAL_CLASS_NONE,"" }, /*10010*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_X,"" }, /*10011*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_PUBLISH,"PUBLISH" }, /*10012*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_REQUEST,"REQUEST" }, /*10013*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_REPLY,"REPLY" }, /*10014*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_ADD,"ADD" }, /*10015*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_CANCEL,"CANCEL" }, /*10016*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_REFRESH,"REFRESH" }, /*10017*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_COUNTER,"COUNTER" }, /*10018*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_DECLINECOUNTER,"DECLINECOUNTER" }, /*10019*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_CREATE,"CREATE" }, /*10020*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_READ,"READ" }, /*10021*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_RESPONSE,"RESPONSE" }, /*10022*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_MOVE,"MOVE" }, /*10023*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_MODIFY,"MODIFY" }, /*10024*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_GENERATEUID,"GENERATEUID" }, /*10025*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_DELETE,"DELETE" }, /*10026*/
    {ICAL_METHOD_PROPERTY,ICAL_METHOD_NONE,"" }, /*10027*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_X,"" }, /*10028*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_TENTATIVE,"TENTATIVE" }, /*10029*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_CONFIRMED,"CONFIRMED" }, /*10030*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_COMPLETED,"COMPLETED" }, /*10031*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_NEEDSACTION,"NEEDS-ACTION" }, /*10032*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_CANCELLED,"CANCELLED" }, /*10033*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_INPROCESS,"IN-PROCESS" }, /*10034*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_DRAFT,"DRAFT" }, /*10035*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_FINAL,"FINAL" }, /*10036*/
    {ICAL_STATUS_PROPERTY,ICAL_STATUS_NONE,"" }, /*10037*/
    {ICAL_TRANSP_PROPERTY,ICAL_TRANSP_X,"" }, /*10038*/
    {ICAL_TRANSP_PROPERTY,ICAL_TRANSP_OPAQUE,"OPAQUE" }, /*10039*/
    {ICAL_TRANSP_PROPERTY,ICAL_TRANSP_TRANSPARENT,"TRANSPARENT" }, /*10040*/
    {ICAL_TRANSP_PROPERTY,ICAL_TRANSP_NONE,"" }, /*10041*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_X,"" }, /*10042*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_PUBLISHNEW,"PUBLISH-NEW" }, /*10043*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_PUBLISHUPDATE,"PUBLISH-UPDATE" }, /*10044*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_PUBLISHFREEBUSY,"PUBLISH-FREEBUSY" }, /*10045*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTNEW,"REQUEST-NEW" }, /*10046*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTUPDATE,"REQUEST-UPDATE" }, /*10047*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTRESCHEDULE,"REQUEST-RESCHEDULE" }, /*10048*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTDELEGATE,"REQUEST-DELEGATE" }, /*10049*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTNEWORGANIZER,"REQUEST-NEW-ORGANIZER" }, /*10050*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTFORWARD,"REQUEST-FORWARD" }, /*10051*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTSTATUS,"REQUEST-STATUS" }, /*10052*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REQUESTFREEBUSY,"REQUEST-FREEBUSY" }, /*10053*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REPLYACCEPT,"REPLY-ACCEPT" }, /*10054*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REPLYDECLINE,"REPLY-DECLINE" }, /*10055*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REPLYDELEGATE,"REPLY-DELEGATE" }, /*10056*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REPLYCRASHERACCEPT,"REPLY-CRASHER-ACCEPT" }, /*10057*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REPLYCRASHERDECLINE,"REPLY-CRASHER-DECLINE" }, /*10058*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_ADDINSTANCE,"ADD-INSTANCE" }, /*10059*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_CANCELEVENT,"CANCEL-EVENT" }, /*10060*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_CANCELINSTANCE,"CANCEL-INSTANCE" }, /*10061*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_CANCELALL,"CANCEL-ALL" }, /*10062*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_REFRESH,"REFRESH" }, /*10063*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_COUNTER,"COUNTER" }, /*10064*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_DECLINECOUNTER,"DECLINECOUNTER" }, /*10065*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_MALFORMED,"MALFORMED" }, /*10066*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_OBSOLETE,"OBSOLETE" }, /*10067*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_MISSEQUENCED,"MISSEQUENCED" }, /*10068*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_UNKNOWN,"UNKNOWN" }, /*10069*/
    {ICAL_XLICCLASS_PROPERTY,ICAL_XLICCLASS_NONE,"" }, /*10070*/
    {ICAL_NO_PROPERTY,0,""}
};

icalproperty* icalproperty_vanew_action(enum icalproperty_action v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ACTION_PROPERTY);   
   icalproperty_set_action((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* ACTION */
icalproperty* icalproperty_new_action(enum icalproperty_action v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ACTION_PROPERTY);   
   icalproperty_set_action((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_action(icalproperty* prop, enum icalproperty_action v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_action(v));
}
enum icalproperty_action icalproperty_get_action(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_action(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_attach(struct icalattachtype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTACH_PROPERTY);   
   icalproperty_set_attach((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* ATTACH */
icalproperty* icalproperty_new_attach(struct icalattachtype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTACH_PROPERTY);   
   icalproperty_set_attach((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_attach(icalproperty* prop, struct icalattachtype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_attach(v));
}
struct icalattachtype icalproperty_get_attach(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_attach(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_attendee(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTENDEE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_attendee((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* ATTENDEE */
icalproperty* icalproperty_new_attendee(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTENDEE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_attendee((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_attendee(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_caladdress(v));
}
const char* icalproperty_get_attendee(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_caladdress(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_calscale(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CALSCALE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_calscale((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* CALSCALE */
icalproperty* icalproperty_new_calscale(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CALSCALE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_calscale((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_calscale(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_calscale(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_categories(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CATEGORIES_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_categories((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* CATEGORIES */
icalproperty* icalproperty_new_categories(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CATEGORIES_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_categories((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_categories(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_categories(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_class(enum icalproperty_class v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CLASS_PROPERTY);   
   icalproperty_set_class((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* CLASS */
icalproperty* icalproperty_new_class(enum icalproperty_class v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CLASS_PROPERTY);   
   icalproperty_set_class((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_class(icalproperty* prop, enum icalproperty_class v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_class(v));
}
enum icalproperty_class icalproperty_get_class(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_class(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_comment(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMMENT_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_comment((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* COMMENT */
icalproperty* icalproperty_new_comment(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMMENT_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_comment((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_comment(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_comment(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_completed(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMPLETED_PROPERTY);   
   icalproperty_set_completed((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* COMPLETED */
icalproperty* icalproperty_new_completed(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMPLETED_PROPERTY);   
   icalproperty_set_completed((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_completed(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_completed(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_contact(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CONTACT_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_contact((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* CONTACT */
icalproperty* icalproperty_new_contact(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CONTACT_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_contact((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_contact(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_contact(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_created(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CREATED_PROPERTY);   
   icalproperty_set_created((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* CREATED */
icalproperty* icalproperty_new_created(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CREATED_PROPERTY);   
   icalproperty_set_created((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_created(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_created(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_description(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DESCRIPTION_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_description((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* DESCRIPTION */
icalproperty* icalproperty_new_description(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DESCRIPTION_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_description((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_description(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_description(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_dtend(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTEND_PROPERTY);   
   icalproperty_set_dtend((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* DTEND */
icalproperty* icalproperty_new_dtend(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTEND_PROPERTY);   
   icalproperty_set_dtend((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_dtend(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_dtend(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_dtstamp(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTAMP_PROPERTY);   
   icalproperty_set_dtstamp((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* DTSTAMP */
icalproperty* icalproperty_new_dtstamp(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTAMP_PROPERTY);   
   icalproperty_set_dtstamp((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_dtstamp(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_dtstamp(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_dtstart(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTART_PROPERTY);   
   icalproperty_set_dtstart((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* DTSTART */
icalproperty* icalproperty_new_dtstart(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTART_PROPERTY);   
   icalproperty_set_dtstart((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_dtstart(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_dtstart(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_due(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DUE_PROPERTY);   
   icalproperty_set_due((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* DUE */
icalproperty* icalproperty_new_due(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DUE_PROPERTY);   
   icalproperty_set_due((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_due(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_due(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_duration(struct icaldurationtype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DURATION_PROPERTY);   
   icalproperty_set_duration((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* DURATION */
icalproperty* icalproperty_new_duration(struct icaldurationtype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DURATION_PROPERTY);   
   icalproperty_set_duration((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_duration(icalproperty* prop, struct icaldurationtype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_duration(v));
}
struct icaldurationtype icalproperty_get_duration(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_duration(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_exdate(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXDATE_PROPERTY);   
   icalproperty_set_exdate((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* EXDATE */
icalproperty* icalproperty_new_exdate(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXDATE_PROPERTY);   
   icalproperty_set_exdate((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_exdate(icalproperty* prop, struct icaltimetype v){
    icalvalue *value;
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    if (v.is_date)
        value = icalvalue_new_date(v);
    else
        value = icalvalue_new_datetime(v);
    icalproperty_set_value(prop,value);
}
struct icaltimetype icalproperty_get_exdate(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_exrule(struct icalrecurrencetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXRULE_PROPERTY);   
   icalproperty_set_exrule((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* EXRULE */
icalproperty* icalproperty_new_exrule(struct icalrecurrencetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXRULE_PROPERTY);   
   icalproperty_set_exrule((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_exrule(icalproperty* prop, struct icalrecurrencetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_recur(v));
}
struct icalrecurrencetype icalproperty_get_exrule(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_recur(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_freebusy(struct icalperiodtype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_FREEBUSY_PROPERTY);   
   icalproperty_set_freebusy((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* FREEBUSY */
icalproperty* icalproperty_new_freebusy(struct icalperiodtype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_FREEBUSY_PROPERTY);   
   icalproperty_set_freebusy((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_freebusy(icalproperty* prop, struct icalperiodtype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_period(v));
}
struct icalperiodtype icalproperty_get_freebusy(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_period(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_geo(struct icalgeotype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_GEO_PROPERTY);   
   icalproperty_set_geo((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* GEO */
icalproperty* icalproperty_new_geo(struct icalgeotype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_GEO_PROPERTY);   
   icalproperty_set_geo((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_geo(icalproperty* prop, struct icalgeotype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_geo(v));
}
struct icalgeotype icalproperty_get_geo(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_geo(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_lastmodified(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LASTMODIFIED_PROPERTY);   
   icalproperty_set_lastmodified((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* LAST-MODIFIED */
icalproperty* icalproperty_new_lastmodified(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LASTMODIFIED_PROPERTY);   
   icalproperty_set_lastmodified((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_lastmodified(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_lastmodified(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_location(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LOCATION_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_location((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* LOCATION */
icalproperty* icalproperty_new_location(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LOCATION_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_location((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_location(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_location(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_maxresults(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_MAXRESULTS_PROPERTY);   
   icalproperty_set_maxresults((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* MAXRESULTS */
icalproperty* icalproperty_new_maxresults(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_MAXRESULTS_PROPERTY);   
   icalproperty_set_maxresults((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_maxresults(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_integer(v));
}
int icalproperty_get_maxresults(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_integer(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_maxresultssize(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_MAXRESULTSSIZE_PROPERTY);   
   icalproperty_set_maxresultssize((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* MAXRESULTSSIZE */
icalproperty* icalproperty_new_maxresultssize(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_MAXRESULTSSIZE_PROPERTY);   
   icalproperty_set_maxresultssize((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_maxresultssize(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_integer(v));
}
int icalproperty_get_maxresultssize(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_integer(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_method(enum icalproperty_method v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_METHOD_PROPERTY);   
   icalproperty_set_method((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* METHOD */
icalproperty* icalproperty_new_method(enum icalproperty_method v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_METHOD_PROPERTY);   
   icalproperty_set_method((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_method(icalproperty* prop, enum icalproperty_method v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_method(v));
}
enum icalproperty_method icalproperty_get_method(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_method(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_organizer(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ORGANIZER_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_organizer((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* ORGANIZER */
icalproperty* icalproperty_new_organizer(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ORGANIZER_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_organizer((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_organizer(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_caladdress(v));
}
const char* icalproperty_get_organizer(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_caladdress(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_percentcomplete(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PERCENTCOMPLETE_PROPERTY);   
   icalproperty_set_percentcomplete((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* PERCENT-COMPLETE */
icalproperty* icalproperty_new_percentcomplete(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PERCENTCOMPLETE_PROPERTY);   
   icalproperty_set_percentcomplete((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_percentcomplete(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_integer(v));
}
int icalproperty_get_percentcomplete(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_integer(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_priority(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRIORITY_PROPERTY);   
   icalproperty_set_priority((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* PRIORITY */
icalproperty* icalproperty_new_priority(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRIORITY_PROPERTY);   
   icalproperty_set_priority((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_priority(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_integer(v));
}
int icalproperty_get_priority(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_integer(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_prodid(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRODID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_prodid((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* PRODID */
icalproperty* icalproperty_new_prodid(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRODID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_prodid((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_prodid(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_prodid(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_query(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_QUERY_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_query((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* QUERY */
icalproperty* icalproperty_new_query(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_QUERY_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_query((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_query(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_query(v));
}
const char* icalproperty_get_query(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_query(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_queryname(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_QUERYNAME_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_queryname((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* QUERYNAME */
icalproperty* icalproperty_new_queryname(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_QUERYNAME_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_queryname((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_queryname(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_queryname(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_rdate(struct icaldatetimeperiodtype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RDATE_PROPERTY);   
   icalproperty_set_rdate((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* RDATE */
icalproperty* icalproperty_new_rdate(struct icaldatetimeperiodtype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RDATE_PROPERTY);   
   icalproperty_set_rdate((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_rdate(icalproperty* prop, struct icaldatetimeperiodtype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetimeperiod(v));
}
struct icaldatetimeperiodtype icalproperty_get_rdate(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetimeperiod(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_recurrenceid(struct icaltimetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RECURRENCEID_PROPERTY);   
   icalproperty_set_recurrenceid((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* RECURRENCE-ID */
icalproperty* icalproperty_new_recurrenceid(struct icaltimetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RECURRENCEID_PROPERTY);   
   icalproperty_set_recurrenceid((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_recurrenceid(icalproperty* prop, struct icaltimetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_datetime(v));
}
struct icaltimetype icalproperty_get_recurrenceid(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_datetime(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_relatedto(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RELATEDTO_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_relatedto((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* RELATED-TO */
icalproperty* icalproperty_new_relatedto(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RELATEDTO_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_relatedto((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_relatedto(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_relatedto(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_repeat(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REPEAT_PROPERTY);   
   icalproperty_set_repeat((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* REPEAT */
icalproperty* icalproperty_new_repeat(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REPEAT_PROPERTY);   
   icalproperty_set_repeat((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_repeat(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_integer(v));
}
int icalproperty_get_repeat(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_integer(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_requeststatus(struct icalreqstattype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REQUESTSTATUS_PROPERTY);   
   icalproperty_set_requeststatus((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* REQUEST-STATUS */
icalproperty* icalproperty_new_requeststatus(struct icalreqstattype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REQUESTSTATUS_PROPERTY);   
   icalproperty_set_requeststatus((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_requeststatus(icalproperty* prop, struct icalreqstattype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_requeststatus(v));
}
struct icalreqstattype icalproperty_get_requeststatus(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_requeststatus(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_resources(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RESOURCES_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_resources((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* RESOURCES */
icalproperty* icalproperty_new_resources(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RESOURCES_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_resources((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_resources(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_resources(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_rrule(struct icalrecurrencetype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RRULE_PROPERTY);   
   icalproperty_set_rrule((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* RRULE */
icalproperty* icalproperty_new_rrule(struct icalrecurrencetype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RRULE_PROPERTY);   
   icalproperty_set_rrule((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_rrule(icalproperty* prop, struct icalrecurrencetype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_recur(v));
}
struct icalrecurrencetype icalproperty_get_rrule(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_recur(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_scope(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SCOPE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_scope((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* SCOPE */
icalproperty* icalproperty_new_scope(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SCOPE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_scope((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_scope(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_scope(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_sequence(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SEQUENCE_PROPERTY);   
   icalproperty_set_sequence((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* SEQUENCE */
icalproperty* icalproperty_new_sequence(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SEQUENCE_PROPERTY);   
   icalproperty_set_sequence((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_sequence(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_integer(v));
}
int icalproperty_get_sequence(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_integer(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_status(enum icalproperty_status v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_STATUS_PROPERTY);   
   icalproperty_set_status((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* STATUS */
icalproperty* icalproperty_new_status(enum icalproperty_status v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_STATUS_PROPERTY);   
   icalproperty_set_status((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_status(icalproperty* prop, enum icalproperty_status v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_status(v));
}
enum icalproperty_status icalproperty_get_status(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_status(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_summary(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SUMMARY_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_summary((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* SUMMARY */
icalproperty* icalproperty_new_summary(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SUMMARY_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_summary((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_summary(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_summary(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_target(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TARGET_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_target((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TARGET */
icalproperty* icalproperty_new_target(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TARGET_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_target((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_target(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_caladdress(v));
}
const char* icalproperty_get_target(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_caladdress(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_transp(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRANSP_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_transp((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TRANSP */
icalproperty* icalproperty_new_transp(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRANSP_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_transp((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_transp(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_transp(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_trigger(struct icaltriggertype v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRIGGER_PROPERTY);   
   icalproperty_set_trigger((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TRIGGER */
icalproperty* icalproperty_new_trigger(struct icaltriggertype v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRIGGER_PROPERTY);   
   icalproperty_set_trigger((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_trigger(icalproperty* prop, struct icaltriggertype v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_trigger(v));
}
struct icaltriggertype icalproperty_get_trigger(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_trigger(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_tzid(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_tzid((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TZID */
icalproperty* icalproperty_new_tzid(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_tzid((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_tzid(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_tzid(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_tzname(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZNAME_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_tzname((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TZNAME */
icalproperty* icalproperty_new_tzname(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZNAME_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_tzname((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_tzname(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_tzname(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_tzoffsetfrom(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETFROM_PROPERTY);   
   icalproperty_set_tzoffsetfrom((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TZOFFSETFROM */
icalproperty* icalproperty_new_tzoffsetfrom(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETFROM_PROPERTY);   
   icalproperty_set_tzoffsetfrom((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_tzoffsetfrom(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_utcoffset(v));
}
int icalproperty_get_tzoffsetfrom(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_utcoffset(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_tzoffsetto(int v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETTO_PROPERTY);   
   icalproperty_set_tzoffsetto((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TZOFFSETTO */
icalproperty* icalproperty_new_tzoffsetto(int v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETTO_PROPERTY);   
   icalproperty_set_tzoffsetto((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_tzoffsetto(icalproperty* prop, int v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_utcoffset(v));
}
int icalproperty_get_tzoffsetto(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_utcoffset(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_tzurl(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZURL_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_tzurl((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* TZURL */
icalproperty* icalproperty_new_tzurl(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZURL_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_tzurl((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_tzurl(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_uri(v));
}
const char* icalproperty_get_tzurl(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_uri(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_uid(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_UID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_uid((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* UID */
icalproperty* icalproperty_new_uid(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_UID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_uid((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_uid(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_uid(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_url(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_URL_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_url((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* URL */
icalproperty* icalproperty_new_url(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_URL_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_url((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_url(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_uri(v));
}
const char* icalproperty_get_url(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_uri(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_version(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_VERSION_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_version((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* VERSION */
icalproperty* icalproperty_new_version(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_VERSION_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_version((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_version(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_version(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_x(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_X_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_x((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X */
icalproperty* icalproperty_new_x(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_X_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_x((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_x(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_x(v));
}
const char* icalproperty_get_x(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_x(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicclass(enum icalproperty_xlicclass v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICCLASS_PROPERTY);   
   icalproperty_set_xlicclass((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-CLASS */
icalproperty* icalproperty_new_xlicclass(enum icalproperty_xlicclass v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICCLASS_PROPERTY);   
   icalproperty_set_xlicclass((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicclass(icalproperty* prop, enum icalproperty_xlicclass v){
    
    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_xlicclass(v));
}
enum icalproperty_xlicclass icalproperty_get_xlicclass(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_xlicclass(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicclustercount(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICCLUSTERCOUNT_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicclustercount((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-CLUSTERCOUNT */
icalproperty* icalproperty_new_xlicclustercount(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICCLUSTERCOUNT_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicclustercount((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicclustercount(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_string(v));
}
const char* icalproperty_get_xlicclustercount(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_string(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicerror(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICERROR_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicerror((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-ERROR */
icalproperty* icalproperty_new_xlicerror(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICERROR_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicerror((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicerror(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_text(v));
}
const char* icalproperty_get_xlicerror(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_text(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicmimecharset(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMECHARSET_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimecharset((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-MIMECHARSET */
icalproperty* icalproperty_new_xlicmimecharset(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMECHARSET_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimecharset((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicmimecharset(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_string(v));
}
const char* icalproperty_get_xlicmimecharset(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_string(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicmimecid(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMECID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimecid((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-MIMECID */
icalproperty* icalproperty_new_xlicmimecid(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMECID_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimecid((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicmimecid(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_string(v));
}
const char* icalproperty_get_xlicmimecid(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_string(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicmimecontenttype(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMECONTENTTYPE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimecontenttype((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-MIMECONTENTTYPE */
icalproperty* icalproperty_new_xlicmimecontenttype(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMECONTENTTYPE_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimecontenttype((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicmimecontenttype(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_string(v));
}
const char* icalproperty_get_xlicmimecontenttype(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_string(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicmimeencoding(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMEENCODING_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimeencoding((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-MIMEENCODING */
icalproperty* icalproperty_new_xlicmimeencoding(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMEENCODING_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimeencoding((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicmimeencoding(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_string(v));
}
const char* icalproperty_get_xlicmimeencoding(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_string(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicmimefilename(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMEFILENAME_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimefilename((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-MIMEFILENAME */
icalproperty* icalproperty_new_xlicmimefilename(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMEFILENAME_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimefilename((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicmimefilename(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_string(v));
}
const char* icalproperty_get_xlicmimefilename(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_string(icalproperty_get_value(prop));
}
icalproperty* icalproperty_vanew_xlicmimeoptinfo(const char* v, ...){
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMEOPTINFO_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimeoptinfo((icalproperty*)impl,v);
   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);
   return (icalproperty*)impl;
}

/* X-LIC-MIMEOPTINFO */
icalproperty* icalproperty_new_xlicmimeoptinfo(const char* v) {
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICMIMEOPTINFO_PROPERTY);   icalerror_check_arg_rz( (v!=0),"v");

   icalproperty_set_xlicmimeoptinfo((icalproperty*)impl,v);
   return (icalproperty*)impl;
}

void icalproperty_set_xlicmimeoptinfo(icalproperty* prop, const char* v){
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");
    icalproperty_set_value(prop,icalvalue_new_string(v));
}
const char* icalproperty_get_xlicmimeoptinfo(icalproperty* prop){
    icalerror_check_arg( (prop!=0),"prop");
    return icalvalue_get_string(icalproperty_get_value(prop));
}
