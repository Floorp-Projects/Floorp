/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalspanlist.h
 CREATOR: eric 21 Aug 2000


 $Id: icalspanlist.h,v 1.2 2001/12/21 18:56:38 mikep%oeone.com Exp $
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
#ifndef ICALSPANLIST_H
#define ICALSPANLIST_H

#include "ical.h"
#include "icalset.h"

typedef void icalspanlist;

/* Make a free list from a set of component. Start and end should be in UTC */
icalspanlist* icalspanlist_new(icalset *set, 
				struct icaltimetype start,
				struct icaltimetype end);

void icalspanlist_free(icalspanlist* spl);

icalcomponent* icalspanlist_make_free_list(icalspanlist* sl);
icalcomponent* icalspanlist_make_busy_list(icalspanlist* sl);

/* Get first free or busy time after time t. all times are in UTC */
struct icalperiodtype icalspanlist_next_free_time(icalspanlist* sl,
						struct icaltimetype t);
struct icalperiodtype icalspanlist_next_busy_time(icalspanlist* sl,
						struct icaltimetype t);

void icalspanlist_dump(icalspanlist* s);

#endif
				    


