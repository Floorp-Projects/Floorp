/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedproperties.{c,h}
  CREATOR: eric 09 May 1999
  
  $Id: icalderivedproperty.h,v 1.1 2001/11/15 19:26:55 mikep%oeone.com Exp $
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
 ======================================================================*/


#ifndef ICALDERIVEDPROPERTY_H
#define ICALDERIVEDPROPERTY_H

#include <time.h>
#include "icalparameter.h"
#include "icalderivedvalue.h"  
#include "icalrecur.h"

typedef void icalproperty;


/* Everything below this line is machine generated. Do not edit. */
typedef enum icalproperty_kind {
    ICAL_ANY_PROPERTY = 0,
    ICAL_ACTION_PROPERTY, 
    ICAL_ATTACH_PROPERTY, 
    ICAL_ATTENDEE_PROPERTY, 
    ICAL_CALSCALE_PROPERTY, 
    ICAL_CATEGORIES_PROPERTY, 
    ICAL_CLASS_PROPERTY, 
    ICAL_COMMENT_PROPERTY, 
    ICAL_COMPLETED_PROPERTY, 
    ICAL_CONTACT_PROPERTY, 
    ICAL_CREATED_PROPERTY, 
    ICAL_DESCRIPTION_PROPERTY, 
    ICAL_DTEND_PROPERTY, 
    ICAL_DTSTAMP_PROPERTY, 
    ICAL_DTSTART_PROPERTY, 
    ICAL_DUE_PROPERTY, 
    ICAL_DURATION_PROPERTY, 
    ICAL_EXDATE_PROPERTY, 
    ICAL_EXRULE_PROPERTY, 
    ICAL_FREEBUSY_PROPERTY, 
    ICAL_GEO_PROPERTY, 
    ICAL_LASTMODIFIED_PROPERTY, 
    ICAL_LOCATION_PROPERTY, 
    ICAL_MAXRESULTS_PROPERTY, 
    ICAL_MAXRESULTSSIZE_PROPERTY, 
    ICAL_METHOD_PROPERTY, 
    ICAL_ORGANIZER_PROPERTY, 
    ICAL_PERCENTCOMPLETE_PROPERTY, 
    ICAL_PRIORITY_PROPERTY, 
    ICAL_PRODID_PROPERTY, 
    ICAL_QUERY_PROPERTY, 
    ICAL_QUERYNAME_PROPERTY, 
    ICAL_RDATE_PROPERTY, 
    ICAL_RECURRENCEID_PROPERTY, 
    ICAL_RELATEDTO_PROPERTY, 
    ICAL_REPEAT_PROPERTY, 
    ICAL_REQUESTSTATUS_PROPERTY, 
    ICAL_RESOURCES_PROPERTY, 
    ICAL_RRULE_PROPERTY, 
    ICAL_SCOPE_PROPERTY, 
    ICAL_SEQUENCE_PROPERTY, 
    ICAL_STATUS_PROPERTY, 
    ICAL_SUMMARY_PROPERTY, 
    ICAL_TARGET_PROPERTY, 
    ICAL_TRANSP_PROPERTY, 
    ICAL_TRIGGER_PROPERTY, 
    ICAL_TZID_PROPERTY, 
    ICAL_TZNAME_PROPERTY, 
    ICAL_TZOFFSETFROM_PROPERTY, 
    ICAL_TZOFFSETTO_PROPERTY, 
    ICAL_TZURL_PROPERTY, 
    ICAL_UID_PROPERTY, 
    ICAL_URL_PROPERTY, 
    ICAL_VERSION_PROPERTY, 
    ICAL_X_PROPERTY, 
    ICAL_XLICCLUSTERCOUNT_PROPERTY, 
    ICAL_XLICERROR_PROPERTY, 
    ICAL_XLICMIMECHARSET_PROPERTY, 
    ICAL_XLICMIMECID_PROPERTY, 
    ICAL_XLICMIMECONTENTTYPE_PROPERTY, 
    ICAL_XLICMIMEENCODING_PROPERTY, 
    ICAL_XLICMIMEFILENAME_PROPERTY, 
    ICAL_XLICMIMEOPTINFO_PROPERTY, 
    ICAL_NO_PROPERTY
} icalproperty_kind;


/* ACTION */
icalproperty* icalproperty_new_action(enum icalproperty_action v);
icalproperty* icalproperty_vanew_action(enum icalproperty_action v, ...);
void icalproperty_set_action(icalproperty* prop, enum icalproperty_action v);
enum icalproperty_action icalproperty_get_action(icalproperty* prop);
/* ATTACH */
icalproperty* icalproperty_new_attach(struct icalattachtype v);
icalproperty* icalproperty_vanew_attach(struct icalattachtype v, ...);
void icalproperty_set_attach(icalproperty* prop, struct icalattachtype v);
struct icalattachtype icalproperty_get_attach(icalproperty* prop);
/* ATTENDEE */
icalproperty* icalproperty_new_attendee(const char* v);
icalproperty* icalproperty_vanew_attendee(const char* v, ...);
void icalproperty_set_attendee(icalproperty* prop, const char* v);
const char* icalproperty_get_attendee(icalproperty* prop);
/* CALSCALE */
icalproperty* icalproperty_new_calscale(const char* v);
icalproperty* icalproperty_vanew_calscale(const char* v, ...);
void icalproperty_set_calscale(icalproperty* prop, const char* v);
const char* icalproperty_get_calscale(icalproperty* prop);
/* CATEGORIES */
icalproperty* icalproperty_new_categories(const char* v);
icalproperty* icalproperty_vanew_categories(const char* v, ...);
void icalproperty_set_categories(icalproperty* prop, const char* v);
const char* icalproperty_get_categories(icalproperty* prop);
/* CLASS */
icalproperty* icalproperty_new_class(const char* v);
icalproperty* icalproperty_vanew_class(const char* v, ...);
void icalproperty_set_class(icalproperty* prop, const char* v);
const char* icalproperty_get_class(icalproperty* prop);
/* COMMENT */
icalproperty* icalproperty_new_comment(const char* v);
icalproperty* icalproperty_vanew_comment(const char* v, ...);
void icalproperty_set_comment(icalproperty* prop, const char* v);
const char* icalproperty_get_comment(icalproperty* prop);
/* COMPLETED */
icalproperty* icalproperty_new_completed(struct icaltimetype v);
icalproperty* icalproperty_vanew_completed(struct icaltimetype v, ...);
void icalproperty_set_completed(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_completed(icalproperty* prop);
/* CONTACT */
icalproperty* icalproperty_new_contact(const char* v);
icalproperty* icalproperty_vanew_contact(const char* v, ...);
void icalproperty_set_contact(icalproperty* prop, const char* v);
const char* icalproperty_get_contact(icalproperty* prop);
/* CREATED */
icalproperty* icalproperty_new_created(struct icaltimetype v);
icalproperty* icalproperty_vanew_created(struct icaltimetype v, ...);
void icalproperty_set_created(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_created(icalproperty* prop);
/* DESCRIPTION */
icalproperty* icalproperty_new_description(const char* v);
icalproperty* icalproperty_vanew_description(const char* v, ...);
void icalproperty_set_description(icalproperty* prop, const char* v);
const char* icalproperty_get_description(icalproperty* prop);
/* DTEND */
icalproperty* icalproperty_new_dtend(struct icaltimetype v);
icalproperty* icalproperty_vanew_dtend(struct icaltimetype v, ...);
void icalproperty_set_dtend(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtend(icalproperty* prop);
/* DTSTAMP */
icalproperty* icalproperty_new_dtstamp(struct icaltimetype v);
icalproperty* icalproperty_vanew_dtstamp(struct icaltimetype v, ...);
void icalproperty_set_dtstamp(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstamp(icalproperty* prop);
/* DTSTART */
icalproperty* icalproperty_new_dtstart(struct icaltimetype v);
icalproperty* icalproperty_vanew_dtstart(struct icaltimetype v, ...);
void icalproperty_set_dtstart(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstart(icalproperty* prop);
/* DUE */
icalproperty* icalproperty_new_due(struct icaltimetype v);
icalproperty* icalproperty_vanew_due(struct icaltimetype v, ...);
void icalproperty_set_due(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_due(icalproperty* prop);
/* DURATION */
icalproperty* icalproperty_new_duration(struct icaldurationtype v);
icalproperty* icalproperty_vanew_duration(struct icaldurationtype v, ...);
void icalproperty_set_duration(icalproperty* prop, struct icaldurationtype v);
struct icaldurationtype icalproperty_get_duration(icalproperty* prop);
/* EXDATE */
icalproperty* icalproperty_new_exdate(struct icaltimetype v);
icalproperty* icalproperty_vanew_exdate(struct icaltimetype v, ...);
void icalproperty_set_exdate(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_exdate(icalproperty* prop);
/* EXRULE */
icalproperty* icalproperty_new_exrule(struct icalrecurrencetype v);
icalproperty* icalproperty_vanew_exrule(struct icalrecurrencetype v, ...);
void icalproperty_set_exrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_exrule(icalproperty* prop);
/* FREEBUSY */
icalproperty* icalproperty_new_freebusy(struct icalperiodtype v);
icalproperty* icalproperty_vanew_freebusy(struct icalperiodtype v, ...);
void icalproperty_set_freebusy(icalproperty* prop, struct icalperiodtype v);
struct icalperiodtype icalproperty_get_freebusy(icalproperty* prop);
/* GEO */
icalproperty* icalproperty_new_geo(struct icalgeotype v);
icalproperty* icalproperty_vanew_geo(struct icalgeotype v, ...);
void icalproperty_set_geo(icalproperty* prop, struct icalgeotype v);
struct icalgeotype icalproperty_get_geo(icalproperty* prop);
/* LAST-MODIFIED */
icalproperty* icalproperty_new_lastmodified(struct icaltimetype v);
icalproperty* icalproperty_vanew_lastmodified(struct icaltimetype v, ...);
void icalproperty_set_lastmodified(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_lastmodified(icalproperty* prop);
/* LOCATION */
icalproperty* icalproperty_new_location(const char* v);
icalproperty* icalproperty_vanew_location(const char* v, ...);
void icalproperty_set_location(icalproperty* prop, const char* v);
const char* icalproperty_get_location(icalproperty* prop);
/* MAXRESULTS */
icalproperty* icalproperty_new_maxresults(int v);
icalproperty* icalproperty_vanew_maxresults(int v, ...);
void icalproperty_set_maxresults(icalproperty* prop, int v);
int icalproperty_get_maxresults(icalproperty* prop);
/* MAXRESULTSSIZE */
icalproperty* icalproperty_new_maxresultssize(int v);
icalproperty* icalproperty_vanew_maxresultssize(int v, ...);
void icalproperty_set_maxresultssize(icalproperty* prop, int v);
int icalproperty_get_maxresultssize(icalproperty* prop);
/* METHOD */
icalproperty* icalproperty_new_method(enum icalproperty_method v);
icalproperty* icalproperty_vanew_method(enum icalproperty_method v, ...);
void icalproperty_set_method(icalproperty* prop, enum icalproperty_method v);
enum icalproperty_method icalproperty_get_method(icalproperty* prop);
/* ORGANIZER */
icalproperty* icalproperty_new_organizer(const char* v);
icalproperty* icalproperty_vanew_organizer(const char* v, ...);
void icalproperty_set_organizer(icalproperty* prop, const char* v);
const char* icalproperty_get_organizer(icalproperty* prop);
/* PERCENT-COMPLETE */
icalproperty* icalproperty_new_percentcomplete(int v);
icalproperty* icalproperty_vanew_percentcomplete(int v, ...);
void icalproperty_set_percentcomplete(icalproperty* prop, int v);
int icalproperty_get_percentcomplete(icalproperty* prop);
/* PRIORITY */
icalproperty* icalproperty_new_priority(int v);
icalproperty* icalproperty_vanew_priority(int v, ...);
void icalproperty_set_priority(icalproperty* prop, int v);
int icalproperty_get_priority(icalproperty* prop);
/* PRODID */
icalproperty* icalproperty_new_prodid(const char* v);
icalproperty* icalproperty_vanew_prodid(const char* v, ...);
void icalproperty_set_prodid(icalproperty* prop, const char* v);
const char* icalproperty_get_prodid(icalproperty* prop);
/* QUERY */
icalproperty* icalproperty_new_query(const char* v);
icalproperty* icalproperty_vanew_query(const char* v, ...);
void icalproperty_set_query(icalproperty* prop, const char* v);
const char* icalproperty_get_query(icalproperty* prop);
/* QUERYNAME */
icalproperty* icalproperty_new_queryname(const char* v);
icalproperty* icalproperty_vanew_queryname(const char* v, ...);
void icalproperty_set_queryname(icalproperty* prop, const char* v);
const char* icalproperty_get_queryname(icalproperty* prop);
/* RDATE */
icalproperty* icalproperty_new_rdate(struct icaldatetimeperiodtype v);
icalproperty* icalproperty_vanew_rdate(struct icaldatetimeperiodtype v, ...);
void icalproperty_set_rdate(icalproperty* prop, struct icaldatetimeperiodtype v);
struct icaldatetimeperiodtype icalproperty_get_rdate(icalproperty* prop);
/* RECURRENCE-ID */
icalproperty* icalproperty_new_recurrenceid(struct icaltimetype v);
icalproperty* icalproperty_vanew_recurrenceid(struct icaltimetype v, ...);
void icalproperty_set_recurrenceid(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_recurrenceid(icalproperty* prop);
/* RELATED-TO */
icalproperty* icalproperty_new_relatedto(const char* v);
icalproperty* icalproperty_vanew_relatedto(const char* v, ...);
void icalproperty_set_relatedto(icalproperty* prop, const char* v);
const char* icalproperty_get_relatedto(icalproperty* prop);
/* REPEAT */
icalproperty* icalproperty_new_repeat(int v);
icalproperty* icalproperty_vanew_repeat(int v, ...);
void icalproperty_set_repeat(icalproperty* prop, int v);
int icalproperty_get_repeat(icalproperty* prop);
/* REQUEST-STATUS */
icalproperty* icalproperty_new_requeststatus(const char* v);
icalproperty* icalproperty_vanew_requeststatus(const char* v, ...);
void icalproperty_set_requeststatus(icalproperty* prop, const char* v);
const char* icalproperty_get_requeststatus(icalproperty* prop);
/* RESOURCES */
icalproperty* icalproperty_new_resources(const char* v);
icalproperty* icalproperty_vanew_resources(const char* v, ...);
void icalproperty_set_resources(icalproperty* prop, const char* v);
const char* icalproperty_get_resources(icalproperty* prop);
/* RRULE */
icalproperty* icalproperty_new_rrule(struct icalrecurrencetype v);
icalproperty* icalproperty_vanew_rrule(struct icalrecurrencetype v, ...);
void icalproperty_set_rrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_rrule(icalproperty* prop);
/* SCOPE */
icalproperty* icalproperty_new_scope(const char* v);
icalproperty* icalproperty_vanew_scope(const char* v, ...);
void icalproperty_set_scope(icalproperty* prop, const char* v);
const char* icalproperty_get_scope(icalproperty* prop);
/* SEQUENCE */
icalproperty* icalproperty_new_sequence(int v);
icalproperty* icalproperty_vanew_sequence(int v, ...);
void icalproperty_set_sequence(icalproperty* prop, int v);
int icalproperty_get_sequence(icalproperty* prop);
/* STATUS */
icalproperty* icalproperty_new_status(enum icalproperty_status v);
icalproperty* icalproperty_vanew_status(enum icalproperty_status v, ...);
void icalproperty_set_status(icalproperty* prop, enum icalproperty_status v);
enum icalproperty_status icalproperty_get_status(icalproperty* prop);
/* SUMMARY */
icalproperty* icalproperty_new_summary(const char* v);
icalproperty* icalproperty_vanew_summary(const char* v, ...);
void icalproperty_set_summary(icalproperty* prop, const char* v);
const char* icalproperty_get_summary(icalproperty* prop);
/* TARGET */
icalproperty* icalproperty_new_target(const char* v);
icalproperty* icalproperty_vanew_target(const char* v, ...);
void icalproperty_set_target(icalproperty* prop, const char* v);
const char* icalproperty_get_target(icalproperty* prop);
/* TRANSP */
icalproperty* icalproperty_new_transp(const char* v);
icalproperty* icalproperty_vanew_transp(const char* v, ...);
void icalproperty_set_transp(icalproperty* prop, const char* v);
const char* icalproperty_get_transp(icalproperty* prop);
/* TRIGGER */
icalproperty* icalproperty_new_trigger(struct icaltriggertype v);
icalproperty* icalproperty_vanew_trigger(struct icaltriggertype v, ...);
void icalproperty_set_trigger(icalproperty* prop, struct icaltriggertype v);
struct icaltriggertype icalproperty_get_trigger(icalproperty* prop);
/* TZID */
icalproperty* icalproperty_new_tzid(const char* v);
icalproperty* icalproperty_vanew_tzid(const char* v, ...);
void icalproperty_set_tzid(icalproperty* prop, const char* v);
const char* icalproperty_get_tzid(icalproperty* prop);
/* TZNAME */
icalproperty* icalproperty_new_tzname(const char* v);
icalproperty* icalproperty_vanew_tzname(const char* v, ...);
void icalproperty_set_tzname(icalproperty* prop, const char* v);
const char* icalproperty_get_tzname(icalproperty* prop);
/* TZOFFSETFROM */
icalproperty* icalproperty_new_tzoffsetfrom(int v);
icalproperty* icalproperty_vanew_tzoffsetfrom(int v, ...);
void icalproperty_set_tzoffsetfrom(icalproperty* prop, int v);
int icalproperty_get_tzoffsetfrom(icalproperty* prop);
/* TZOFFSETTO */
icalproperty* icalproperty_new_tzoffsetto(int v);
icalproperty* icalproperty_vanew_tzoffsetto(int v, ...);
void icalproperty_set_tzoffsetto(icalproperty* prop, int v);
int icalproperty_get_tzoffsetto(icalproperty* prop);
/* TZURL */
icalproperty* icalproperty_new_tzurl(const char* v);
icalproperty* icalproperty_vanew_tzurl(const char* v, ...);
void icalproperty_set_tzurl(icalproperty* prop, const char* v);
const char* icalproperty_get_tzurl(icalproperty* prop);
/* UID */
icalproperty* icalproperty_new_uid(const char* v);
icalproperty* icalproperty_vanew_uid(const char* v, ...);
void icalproperty_set_uid(icalproperty* prop, const char* v);
const char* icalproperty_get_uid(icalproperty* prop);
/* URL */
icalproperty* icalproperty_new_url(const char* v);
icalproperty* icalproperty_vanew_url(const char* v, ...);
void icalproperty_set_url(icalproperty* prop, const char* v);
const char* icalproperty_get_url(icalproperty* prop);
/* VERSION */
icalproperty* icalproperty_new_version(const char* v);
icalproperty* icalproperty_vanew_version(const char* v, ...);
void icalproperty_set_version(icalproperty* prop, const char* v);
const char* icalproperty_get_version(icalproperty* prop);
/* X */
icalproperty* icalproperty_new_x(const char* v);
icalproperty* icalproperty_vanew_x(const char* v, ...);
void icalproperty_set_x(icalproperty* prop, const char* v);
const char* icalproperty_get_x(icalproperty* prop);
/* X-LIC-CLUSTERCOUNT */
icalproperty* icalproperty_new_xlicclustercount(const char* v);
icalproperty* icalproperty_vanew_xlicclustercount(const char* v, ...);
void icalproperty_set_xlicclustercount(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicclustercount(icalproperty* prop);
/* X-LIC-ERROR */
icalproperty* icalproperty_new_xlicerror(const char* v);
icalproperty* icalproperty_vanew_xlicerror(const char* v, ...);
void icalproperty_set_xlicerror(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicerror(icalproperty* prop);
/* X-LIC-MIMECHARSET */
icalproperty* icalproperty_new_xlicmimecharset(const char* v);
icalproperty* icalproperty_vanew_xlicmimecharset(const char* v, ...);
void icalproperty_set_xlicmimecharset(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecharset(icalproperty* prop);
/* X-LIC-MIMECID */
icalproperty* icalproperty_new_xlicmimecid(const char* v);
icalproperty* icalproperty_vanew_xlicmimecid(const char* v, ...);
void icalproperty_set_xlicmimecid(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecid(icalproperty* prop);
/* X-LIC-MIMECONTENTTYPE */
icalproperty* icalproperty_new_xlicmimecontenttype(const char* v);
icalproperty* icalproperty_vanew_xlicmimecontenttype(const char* v, ...);
void icalproperty_set_xlicmimecontenttype(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecontenttype(icalproperty* prop);
/* X-LIC-MIMEENCODING */
icalproperty* icalproperty_new_xlicmimeencoding(const char* v);
icalproperty* icalproperty_vanew_xlicmimeencoding(const char* v, ...);
void icalproperty_set_xlicmimeencoding(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimeencoding(icalproperty* prop);
/* X-LIC-MIMEFILENAME */
icalproperty* icalproperty_new_xlicmimefilename(const char* v);
icalproperty* icalproperty_vanew_xlicmimefilename(const char* v, ...);
void icalproperty_set_xlicmimefilename(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimefilename(icalproperty* prop);
/* X-LIC-MIMEOPTINFO */
icalproperty* icalproperty_new_xlicmimeoptinfo(const char* v);
icalproperty* icalproperty_vanew_xlicmimeoptinfo(const char* v, ...);
void icalproperty_set_xlicmimeoptinfo(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimeoptinfo(icalproperty* prop);

#endif /*ICALPROPERTY_H*/
