/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalgauge.h
 CREATOR: eric 23 December 1999


 $Id: icalgauge.h,v 1.2 2001/12/21 18:56:37 mikep%oeone.com Exp $
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

#ifndef ICALGAUGE_H
#define ICALGAUGE_H

typedef void icalgauge;

icalgauge* icalgauge_new_from_sql(char* sql);

void icalgauge_free(icalgauge* gauge);

char* icalgauge_as_sql(icalcomponent* gauge);

void icalgauge_dump(icalcomponent* gauge);

/* Return true if comp matches the gauge. The component must be in
   cannonical form -- a VCALENDAR with one VEVENT, VTODO or VJOURNAL
   sub component */
int icalgauge_compare(icalgauge* g, icalcomponent* comp);

/* Clone the component, but only return the properties specified in
   the gauge */
icalcomponent* icalgauge_new_clone(icalgauge* g, icalcomponent* comp);

#endif /* ICALGAUGE_H*/
