
/* -*- Mode: C -*-*/
/*======================================================================
 FILE: icalenums.h

 

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalenums.h

  Contributions from:
     Graham Davison (g.m.davison@computer.org)

======================================================================*/

#ifndef ICALENUMS_H
#define ICALENUMS_H



/***********************************************************************
 * Component enumerations
**********************************************************************/

typedef enum icalcomponent_kind {
    ICAL_NO_COMPONENT,
    ICAL_ANY_COMPONENT,	/* Used to select all components*/
    ICAL_XROOT_COMPONENT,
    ICAL_XATTACH_COMPONENT, /* MIME attached data, returned by parser. */
    ICAL_VEVENT_COMPONENT,
    ICAL_VTODO_COMPONENT,
    ICAL_VJOURNAL_COMPONENT,
    ICAL_VCALENDAR_COMPONENT,
    ICAL_VFREEBUSY_COMPONENT,
    ICAL_VALARM_COMPONENT,
    ICAL_XAUDIOALARM_COMPONENT,  
    ICAL_XDISPLAYALARM_COMPONENT,
    ICAL_XEMAILALARM_COMPONENT,
    ICAL_XPROCEDUREALARM_COMPONENT,
    ICAL_VTIMEZONE_COMPONENT,
    ICAL_XSTANDARD_COMPONENT,
    ICAL_XDAYLIGHT_COMPONENT,
    ICAL_X_COMPONENT,
    ICAL_VSCHEDULE_COMPONENT,
    ICAL_VQUERY_COMPONENT,
    ICAL_VCAR_COMPONENT,
    ICAL_VCOMMAND_COMPONENT,
    ICAL_XLICINVALID_COMPONENT,
    ICAL_XLICMIMEPART_COMPONENT /* a non-stardard component that mirrors
				structure of MIME data */

} icalcomponent_kind;



/***********************************************************************
 * Request Status codes
 **********************************************************************/

typedef enum icalrequeststatus {
    ICAL_UNKNOWN_STATUS,
    ICAL_2_0_SUCCESS_STATUS,
    ICAL_2_1_FALLBACK_STATUS,
    ICAL_2_2_IGPROP_STATUS,
    ICAL_2_3_IGPARAM_STATUS,
    ICAL_2_4_IGXPROP_STATUS,
    ICAL_2_5_IGXPARAM_STATUS,
    ICAL_2_6_IGCOMP_STATUS,
    ICAL_2_7_FORWARD_STATUS,
    ICAL_2_8_ONEEVENT_STATUS,
    ICAL_2_9_TRUNC_STATUS,
    ICAL_2_10_ONETODO_STATUS,
    ICAL_2_11_TRUNCRRULE_STATUS,
    ICAL_3_0_INVPROPNAME_STATUS,
    ICAL_3_1_INVPROPVAL_STATUS,
    ICAL_3_2_INVPARAM_STATUS,
    ICAL_3_3_INVPARAMVAL_STATUS,
    ICAL_3_4_INVCOMP_STATUS,
    ICAL_3_5_INVTIME_STATUS,
    ICAL_3_6_INVRULE_STATUS,
    ICAL_3_7_INVCU_STATUS,
    ICAL_3_8_NOAUTH_STATUS,
    ICAL_3_9_BADVERSION_STATUS,
    ICAL_3_10_TOOBIG_STATUS,
    ICAL_3_11_MISSREQCOMP_STATUS,
    ICAL_3_12_UNKCOMP_STATUS,
    ICAL_3_13_BADCOMP_STATUS,
    ICAL_3_14_NOCAP_STATUS,
    ICAL_4_0_BUSY_STATUS,
    ICAL_5_0_MAYBE_STATUS,
    ICAL_5_1_UNAVAIL_STATUS,
    ICAL_5_2_NOSERVICE_STATUS,
    ICAL_5_3_NOSCHED_STATUS
} icalrequeststatus;


const char* icalenum_reqstat_desc(icalrequeststatus stat);
short icalenum_reqstat_major(icalrequeststatus stat);
short icalenum_reqstat_minor(icalrequeststatus stat);
icalrequeststatus icalenum_num_to_reqstat(short major, short minor);

/***********************************************************************
 * Conversion functions
**********************************************************************/


/* Thse routines used to be in icalenums.c, but were moved into the
   icalproperty, icalparameter, icalvalue, or icalcomponent modules. */

/* const char* icalproperty_kind_to_string(icalproperty_kind kind);*/
#define icalenum_property_kind_to_string(x) icalproperty_kind_to_string(x)

/*icalproperty_kind icalproperty_string_to_kind(const char* string)*/
#define icalenum_string_to_property_kind(x) icalproperty_string_to_kind(x)

/*icalvalue_kind icalproperty_kind_to_value_kind(icalproperty_kind kind);*/
#define icalenum_property_kind_to_value_kind(x) icalproperty_kind_to_value_kind(x)

/*const char* icalenum_method_to_string(icalproperty_method);*/
#define icalenum_method_to_string(x) icalproperty_method_to_string(x)

/*icalproperty_method icalenum_string_to_method(const char* string);*/
#define icalenum_string_to_method(x) icalproperty_string_to_method(x)

/*const char* icalenum_status_to_string(icalproperty_status);*/
#define icalenum_status_to_string(x) icalproperty_status_to_string(x)

/*icalproperty_status icalenum_string_to_status(const char* string);*/
#define icalenum_string_to_status(x) icalproperty_string_to_status(x)

/*icalvalue_kind icalenum_string_to_value_kind(const char* str);*/
#define icalenum_string_to_value_kind(x) icalvalue_string_to_kind(x)

/*const char* icalenum_value_kind_to_string(icalvalue_kind kind);*/
#define icalenum_value_kind_to_string(x) icalvalue_kind_to_string(x)

/*const char* icalenum_component_kind_to_string(icalcomponent_kind kind);*/
#define icalenum_component_kind_to_string(x) icalcomponent_kind_to_string(x)

/*icalcomponent_kind icalenum_string_to_component_kind(const char* string);*/
#define icalenum_string_to_component_kind(x) icalcomponent_string_to_kind(x)


#endif /* !ICALENUMS_H */

