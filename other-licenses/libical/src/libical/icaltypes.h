/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaltypes.h
 CREATOR: eric 20 March 1999


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

#ifndef ICALTYPES_H
#define ICALTYPES_H

#include <time.h>
#include "icalenums.h"
#include "icaltime.h"
#include "icalduration.h"
#include "icalperiod.h"


/* This type type should probably be an opaque type... */
struct icalattachtype
{
	void* binary;
	int owns_binary; 

	char* base64;
	int owns_base64;

	char* url;

	int refcount; 

};

/* converts base64 to binary, fetches url and stores as binary, or
   just returns data */

struct icalattachtype* icalattachtype_new(void);
void  icalattachtype_add_reference(struct icalattachtype* v);
void icalattachtype_free(struct icalattachtype* v);

void icalattachtype_set_url(struct icalattachtype* v, char* url);
char* icalattachtype_get_url(struct icalattachtype* v);

void icalattachtype_set_base64(struct icalattachtype* v, char* base64,
				int owns);
char* icalattachtype_get_base64(struct icalattachtype* v);

void icalattachtype_set_binary(struct icalattachtype* v, char* binary,
				int owns);
void* icalattachtype_get_binary(struct icalattachtype* v);

struct icalgeotype 
{
	float lat;
	float lon;
};


struct icaldatetimeperiodtype 
{
	struct icaltimetype time;
	struct icalperiodtype period;
};


struct icaltriggertype 
{
	struct icaltimetype time; 
	struct icaldurationtype duration;
};

struct icaltriggertype icaltriggertype_from_string(const char* str);

int icaltriggertype_is_null_trigger(struct icaltriggertype tr);

/* struct icalreqstattype. This struct contains two string pointers,
but don't try to free either of them. The "desc" string is a pointer
to a static table inside the library.  Don't try to free it. The
"debug" string is a pointer into the string that the called passed
into to icalreqstattype_from_string. Don't try to free it either, and
don't use it after the original string has been freed.

BTW, you would get that original string from
*icalproperty_get_requeststatus() or icalvalue_get_text(), when
operating on a the value of a request_status property. */

struct icalreqstattype {

	icalrequeststatus code;
	const char* desc;
	const char* debug;
};

struct icalreqstattype icalreqstattype_from_string(const char* str);
const char* icalreqstattype_as_string(struct icalreqstattype);



struct icaltimezonephase {
    const char* tzname;
    int is_stdandard; /* 1 = standard tme, 0 = daylight savings time */
    struct icaltimetype dtstart;
    int offsetto;
    int tzoffsetfrom;
    const char* comment;
    struct icaldatetimeperiodtype rdate;
    const char* rrule;    
};


struct icaltimezonetype {
    const char* tzid;
    struct icaltimetype last_mod;
    const char* tzurl;
    
    /* Array of phases. The end of the array is a phase with tzname == 0 */
    struct icaltimezonephase *phases;
};

void icaltimezonetype_free(struct icaltimezonetype tzt);


#endif /* !ICALTYPES_H */
