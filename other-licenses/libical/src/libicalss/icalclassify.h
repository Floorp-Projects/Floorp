/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalclassify.h
 CREATOR: eric 21 Aug 2000


 $Id: icalclassify.h,v 1.1 2001/11/15 19:27:19 mikep%oeone.com Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


 =========================================================================*/

#ifndef ICALCLASSIFY_H
#define ICALCLASSIFY_H

#include "ical.h"
#include "icalset.h"


typedef enum icalclass {
    ICAL_NO_CLASS,
    ICAL_PUBLISH_NEW_CLASS,
    ICAL_PUBLISH_UPDATE_CLASS,
    ICAL_PUBLISH_FREEBUSY_CLASS,
    ICAL_REQUEST_NEW_CLASS,
    ICAL_REQUEST_UPDATE_CLASS,
    ICAL_REQUEST_RESCHEDULE_CLASS,
    ICAL_REQUEST_DELEGATE_CLASS,
    ICAL_REQUEST_NEW_ORGANIZER_CLASS,
    ICAL_REQUEST_FORWARD_CLASS,
    ICAL_REQUEST_STATUS_CLASS,
    ICAL_REQUEST_FREEBUSY_CLASS,
    ICAL_REPLY_ACCEPT_CLASS,
    ICAL_REPLY_DECLINE_CLASS,
    ICAL_REPLY_CRASHER_ACCEPT_CLASS,
    ICAL_REPLY_CRASHER_DECLINE_CLASS,
    ICAL_ADD_INSTANCE_CLASS,
    ICAL_CANCEL_EVENT_CLASS,
    ICAL_CANCEL_INSTANCE_CLASS,
    ICAL_CANCEL_ALL_CLASS,
    ICAL_REFRESH_CLASS,
    ICAL_COUNTER_CLASS,
    ICAL_DECLINECOUNTER_CLASS,
    ICAL_MALFORMED_CLASS, 
    ICAL_OBSOLETE_CLASS, /* 21 */
    ICAL_MISSEQUENCED_CLASS, /* 22 */
    ICAL_UNKNOWN_CLASS /* 23 */
} ical_class;

ical_class icalclassify(icalcomponent* c,icalcomponent* match, 
			      const char* user);

icalcomponent* icalclassify_find_overlaps(icalset* set, icalcomponent* comp);

#endif /* ICALCLASSIFY_H*/


				    


