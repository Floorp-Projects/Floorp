/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalattendee.h
 CREATOR: eric 8 Mar 01


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icaltypes.h

======================================================================*/

#ifndef ICALATTENDEE_H
#define ICALATTENDEE_H

#include <time.h>
#include "icalenums.h"
#include "icaltime.h"
#include "icalduration.h"
#include "icalperiod.h"
#include "icalderivedparameter.h"
#include "icalderivedvalue.h"

struct icalorganizertype {
    const char* value;
    const char* common_name;
    const char* dir;
    const char* sentby;
    const char* language;

};

/* Create a copy of the given organizer. Libical will not own the
   memory for the strings in the copy; the call must free them */
struct icalorganizertype icalorganizertype_new_clone(struct icalorganizertype a);


struct icalattendeetype {
    const char* cuid; /* Cal user id, contents of the property value */
    /*icalparameter_cutype cutype;*/
    const char* member;
    /*icalparameter_role role;*/
    int rsvp;
    const char* delto;
    const char* delfrom;
    const char* sentby;
    const char* cn;
    const char* dir;
    const char* language;
};

/* Create a copy of the given attendee. Libical will not own the
   memory for the strings in the copy; the call must free them */
struct icalattendeetype icalattendeetype_new_clone(struct icalattendeetype a);


#endif /* !ICALATTENDEE_H */
