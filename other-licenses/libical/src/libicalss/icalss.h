/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalgauge.h
 CREATOR: eric 23 December 1999


 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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
/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalset.h
 CREATOR: eric 28 November 1999


 Icalset is the "base class" for representations of a collection of
 iCal components. Derived classes (actually delegatees) include:
 
    icalfileset   Store componetns in a single file
    icaldirset    Store components in multiple files in a directory
    icalheapset   Store components on the heap
    icalmysqlset  Store components in a mysql database. 

 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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

#ifndef ICALSET_H
#define ICALSET_H

#include <limits.h> /* For PATH_MAX */

#ifdef PATH_MAX
#define ICAL_PATH_MAX PATH_MAX
#else
#define ICAL_PATH_MAX 1024
#endif




typedef void icalset;

typedef enum icalset_kind {
    ICAL_FILE_SET,
    ICAL_DIR_SET,
    ICAL_HEAP_SET,
    ICAL_MYSQL_SET,
    ICAL_CAP_SET
} icalset_kind;


/* Create a specific derived type of set */
icalset* icalset_new_file(const char* path);
icalset* icalset_new_file_reader(const char* path);
icalset* icalset_new_file_writer(const char* path);

icalset* icalset_new_dir(const char* path);
icalset* icalset_new_file_reader(const char* path);
icalset* icalset_new_file_writer(const char* path);

icalset* icalset_new_heap(void);
icalset* icalset_new_mysql(const char* path);
/*icalset* icalset_new_cap(icalcstp* cstp);*/

void icalset_free(icalset* set);

const char* icalset_path(icalset* set);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately*/
void icalset_mark(icalset* set);
icalerrorenum icalset_commit(icalset* set); 

icalerrorenum icalset_add_component(icalset* set, icalcomponent* comp);
icalerrorenum icalset_remove_component(icalset* set, icalcomponent* comp);

int icalset_count_components(icalset* set,
			     icalcomponent_kind kind);

/* Restrict the component returned by icalset_first, _next to those
   that pass the gauge. _clear removes the gauge. */
icalerrorenum icalset_select(icalset* set, icalcomponent* gauge);
void icalset_clear_select(icalset* set);

/* Get a component by uid */
icalcomponent* icalset_fetch(icalset* set, const char* uid);
int icalset_has_uid(icalset* set, const char* uid);
icalcomponent* icalset_fetch_match(icalset* set, icalcomponent *c);

/* Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icalset_modify(icalset* set, icalcomponent *oldc,
			       icalcomponent *newc);

/* Iterate through the components. If a guage has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icalset_get_current_component(icalset* set);
icalcomponent* icalset_get_first_component(icalset* set);
icalcomponent* icalset_get_next_component(icalset* set);

#endif /* !ICALSET_H */



/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalfileset.h
 CREATOR: eric 23 December 1999


 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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

#ifndef ICALFILESET_H
#define ICALFILESET_H

#ifndef XP_MAC
#include <sys/types.h> /* For open() flags and mode */
#endif
#include <sys/stat.h> /* For open() flags and mode */
#include <fcntl.h> /* For open() flags and mode */

#ifdef WIN32
#define mode_t int
#endif

extern int icalfileset_safe_saves;

typedef void icalfileset;


/* icalfileset
   icalfilesetfile
   icalfilesetdir
*/


icalfileset* icalfileset_new(const char* path);
icalfileset* icalfileset_new_reader(const char* path);
icalfileset* icalfileset_new_writer(const char* path);


/* Like _new, but takes open() flags for opening the file */
icalfileset* icalfileset_new_open(const char* path, 
				  int flags, mode_t mode);

void icalfileset_free(icalfileset* cluster);

const char* icalfileset_path(icalfileset* cluster);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately. */
void icalfileset_mark(icalfileset* cluster);
icalerrorenum icalfileset_commit(icalfileset* cluster); 

icalerrorenum icalfileset_add_component(icalfileset* cluster,
					icalcomponent* child);

icalerrorenum icalfileset_remove_component(icalfileset* cluster,
					   icalcomponent* child);

int icalfileset_count_components(icalfileset* cluster,
				 icalcomponent_kind kind);

/* Restrict the component returned by icalfileset_first, _next to those
   that pass the gauge. _clear removes the gauge */
icalerrorenum icalfileset_select(icalfileset* store, icalgauge* gauge);
void icalfileset_clear(icalfileset* store);

/* Get and search for a component by uid */
icalcomponent* icalfileset_fetch(icalfileset* cluster, const char* uid);
int icalfileset_has_uid(icalfileset* cluster, const char* uid);
icalcomponent* icalfileset_fetch_match(icalfileset* set, icalcomponent *c);


/* Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icalfileset_modify(icalfileset* store, icalcomponent *oldcomp,
			       icalcomponent *newcomp);

/* Iterate through components. If a guage has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icalfileset_get_current_component (icalfileset* cluster);
icalcomponent* icalfileset_get_first_component(icalfileset* cluster);
icalcomponent* icalfileset_get_next_component(icalfileset* cluster);
/* Return a reference to the internal component. You probably should
   not be using this. */

icalcomponent* icalfileset_get_component(icalfileset* cluster);


#endif /* !ICALFILESET_H */



/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaldirset.h
 CREATOR: eric 28 November 1999


 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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

#ifndef ICALDIRSET_H
#define ICALDIRSET_H


/* icaldirset Routines for storing, fetching, and searching for ical
 * objects in a database */

typedef void icaldirset;


icaldirset* icaldirset_new(const char* path);

icaldirset* icaldirset_new_reader(const char* path);
icaldirset* icaldirset_new_writer(const char* path);


void icaldirset_free(icaldirset* store);

const char* icaldirset_path(icaldirset* store);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately*/
void icaldirset_mark(icaldirset* store);
icalerrorenum icaldirset_commit(icaldirset* store); 

icalerrorenum icaldirset_add_component(icaldirset* store, icalcomponent* comp);
icalerrorenum icaldirset_remove_component(icaldirset* store, icalcomponent* comp);

int icaldirset_count_components(icaldirset* store,
			       icalcomponent_kind kind);

/* Restrict the component returned by icaldirset_first, _next to those
   that pass the gauge. _clear removes the gauge. */
icalerrorenum icaldirset_select(icaldirset* store, icalcomponent* gauge);
void icaldirset_clear(icaldirset* store);

/* Get a component by uid */
icalcomponent* icaldirset_fetch(icaldirset* store, const char* uid);
int icaldirset_has_uid(icaldirset* store, const char* uid);
icalcomponent* icaldirset_fetch_match(icaldirset* set, icalcomponent *c);

/* Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icaldirset_modify(icaldirset* store, icalcomponent *oldc,
			       icalcomponent *newc);

/* Iterate through the components. If a guage has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icaldirset_get_current_component(icaldirset* store);
icalcomponent* icaldirset_get_first_component(icaldirset* store);
icalcomponent* icaldirset_get_next_component(icaldirset* store);

#endif /* !ICALDIRSET_H */



/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcalendar.h
 CREATOR: eric 23 December 1999


 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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

#ifndef ICALCALENDAR_H
#define ICALCALENDAR_H


/* icalcalendar
 * Routines for storing calendar data in a file system. The calendar 
 * has two icaldirsets, one for incoming components and one for booked
 * components. It also has interfaces to access the free/busy list
 * and a list of calendar properties */

typedef  void icalcalendar;

icalcalendar* icalcalendar_new(char* dir);

void icalcalendar_free(icalcalendar* calendar);

int icalcalendar_lock(icalcalendar* calendar);

int icalcalendar_unlock(icalcalendar* calendar);

int icalcalendar_islocked(icalcalendar* calendar);

int icalcalendar_ownlock(icalcalendar* calendar);

icalset* icalcalendar_get_booked(icalcalendar* calendar);

icalset* icalcalendar_get_incoming(icalcalendar* calendar);

icalset* icalcalendar_get_properties(icalcalendar* calendar);

icalset* icalcalendar_get_freebusy(icalcalendar* calendar);


#endif /* !ICALCALENDAR_H */



/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalclassify.h
 CREATOR: eric 21 Aug 2000


 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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


icalproperty_xlicclass icalclassify(icalcomponent* c,icalcomponent* match, 
			      const char* user);

icalcomponent* icalclassify_find_overlaps(icalset* set, icalcomponent* comp);

char* icalclassify_class_to_string(icalproperty_xlicclass c);


#endif /* ICALCLASSIFY_H*/


				    


/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalspanlist.h
 CREATOR: eric 21 Aug 2000


 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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
				    


/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalmessage.h
 CREATOR: eric 07 Nov 2000


 $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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


#ifndef ICALMESSAGE_H
#define ICALMESSAGE_H


icalcomponent* icalmessage_new_accept_reply(icalcomponent* c, 
					    const char* user,
					    const char* msg);

icalcomponent* icalmessage_new_decline_reply(icalcomponent* c,
					    const char* user,
					    const char* msg);

/* New is modified version of old */
icalcomponent* icalmessage_new_counterpropose_reply(icalcomponent* oldc,
						    icalcomponent* newc,
						    const char* user,
						    const char* msg);


icalcomponent* icalmessage_new_delegate_reply(icalcomponent* c,
					      const char* user,
					      const char* delegatee,
					      const char* msg);


icalcomponent* icalmessage_new_cancel_event(icalcomponent* c,
					    const char* user,
					    const char* msg);
icalcomponent* icalmessage_new_cancel_instance(icalcomponent* c,
					    const char* user,
					    const char* msg);
icalcomponent* icalmessage_new_cancel_all(icalcomponent* c,
					    const char* user,
					    const char* msg);


icalcomponent* icalmessage_new_error_reply(icalcomponent* c,
					   const char* user,
					   const char* msg,
					   const char* debug,
					   icalrequeststatus rs);


#endif /* ICALMESSAGE_H*/
/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalcstp.h
  CREATOR: eric 20 April 1999
  
  $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalcstp.h

======================================================================*/


#ifndef ICALCSTP_H
#define ICALCSTP_H



/* Connection state, from the state machine in RFC2445 */
enum cstps_state {
    NO_STATE,
    CONNECTED,
    AUTHENTICATED,
    IDENTIFIED,
    DISCONNECTED,
    RECEIVE
};

/* CSTP Commands that a client can issue to a server */
typedef enum icalcstp_command {
    ICAL_ABORT_COMMAND,
    ICAL_AUTHENTICATE_COMMAND,
    ICAL_CAPABILITY_COMMAND,
    ICAL_CONTINUE_COMMAND,
    ICAL_CALIDEXPAND_COMMAND,
    ICAL_IDENTIFY_COMMAND,
    ICAL_DISCONNECT_COMMAND,
    ICAL_SENDDATA_COMMAND,
    ICAL_STARTTLS_COMMAND,
    ICAL_UPNEXPAND_COMMAND,
    ICAL_COMPLETE_COMMAND,
    ICAL_UNKNOWN_COMMAND
} icalcstp_command;



/* A statement is a combination of command or response code and a
   component that the server and client exchage with each other. */
struct icalcstp_statement {
    icalcstp_command command;
    char* str_data; /* If non-NUll use as arguments to command */
    int int_data; /* If non-NULL use as arguments to command */

    icalrequeststatus code;

    icalcomponent* data;
};

const char* icalcstp_command_to_string(icalcstp_command command);
icalcstp_command icalcstp_string_to_command(const char* str);

#endif /* !ICALCSTP_H */



/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalcstpclient.h
  CREATOR: eric 4 Feb 01
  
  $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalcstp.h

======================================================================*/


#ifndef ICALCSTPC_H
#define ICALCSTPC_H


/********************** Client (Sender) Interfaces **************************/

/* How to use: 

   1) Construct a new icalcstpc
   2) Issue a command by calling one of the command routines. 
   3) Repeat until both call icalcstpc_next_output and
   icalcstpc_next_input return 0:
     3a) Call icalcstpc_next_output. Send string to server. 
     3b) Get string from server, & give to icalcstp_next_input()
   4) Iterate with icalcstpc_first_response & icalcstp_next_response to 
   get the servers responses
   5) Repeat at #2
*/


typedef void icalcstpc;

/* Response code sent by the server. */
typedef struct icalcstpc_response {	
    icalrequeststatus code;
    char *arg; /* These strings are owned by libical */
    char *debug_text;
    char *more_text;
    void* result;
} icalcstpc_response;


icalcstpc* icalcstpc_new();

void icalcstpc_free(icalcstpc* cstpc);

int icalcstpc_set_timeout(icalcstpc* cstp, int sec);


/* Get the next string to send to the server */
char* icalcstpc_next_output(icalcstpc* cstp, char* line);

/* process the next string from the server */ 
int icalcstpc_next_input(icalcstpc* cstp, char * line);

/* After icalcstpc_next_input returns a 0, there are responses
   ready. use these to get them */
icalcstpc_response icalcstpc_first_response(icalcstpc* cstp);
icalcstpc_response icalcstpc_next_response(icalcstpc* cstp);

/* Issue a command */
icalerrorenum icalcstpc_abort(icalcstpc* cstp);
icalerrorenum icalcstpc_authenticate(icalcstpc* cstp, char* mechanism, 
                                        char* init_data, char* f(char*) );
icalerrorenum icalcstpc_capability(icalcstpc* cstp);
icalerrorenum icalcstpc_calidexpand(icalcstpc* cstp,char* calid);
icalerrorenum icalcstpc_continue(icalcstpc* cstp, unsigned int time);
icalerrorenum icalcstpc_disconnect(icalcstpc* cstp);
icalerrorenum icalcstpc_identify(icalcstpc* cstp, char* id);
icalerrorenum icalcstpc_starttls(icalcstpc* cstp, char* command, 
                                    char* init_data, char* f(char*));
icalerrorenum icalcstpc_senddata(icalcstpc* cstp, unsigned int time,
				icalcomponent *comp);
icalerrorenum icalcstpc_upnexpand(icalcstpc* cstp,char* calid);
icalerrorenum icalcstpc_sendata(icalcstpc* cstp, unsigned int time,
                                   icalcomponent *comp);


#endif /* !ICALCSTPC_H */



/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalcstpserver.h
  CREATOR: eric 13 Feb 01
  
  $Id: icalss.h,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalcstp.h

======================================================================*/


#ifndef ICALCSTPS_H
#define ICALCSTPS_H



/********************** Server (Reciever) Interfaces *************************/

/* On the server side, the caller will recieve data from the incoming
   socket and pass it to icalcstps_next_input. The caller then takes
   the return from icalcstps_next_outpu and sends it out through the
   socket. This gives the caller a point of control. If the cstp code
   connected to the socket itself, it would be hard for the caller to
   do anything else after the cstp code was started.

   All of the server and client command routines will generate
   response codes. On the server side, these responses will be turned
   into text and sent to the client. On the client side, the reponse
   is the one sent from the server.

   Since each command can return multiple responses, the responses are
   stored in the icalcstps object and are accesses by
   icalcstps_first_response() and icalcstps_next_response()

   How to use: 

   1) Construct a new icalcstps, bound to your code via stubs
   2) Repeat forever:
     2a) Get string from client & give to icalcstps_next_input()
     2b) Repeat until icalcstp_next_output returns 0:
       2b1) Call icalcstps_next_output. 
       2b2) Send string to client.
*/



typedef void icalcstps;

/* Pointers to the rountines that
   icalcstps_process_incoming will call when it recognizes a CSTP
   command in the data. BTW, the CONTINUE command is named 'cont'
   because 'continue' is a C keyword */

struct icalcstps_commandfp {
  icalerrorenum (*abort)(icalcstps* cstp);
  icalerrorenum (*authenticate)(icalcstps* cstp, char* mechanism,
                                    char* data);
  icalerrorenum (*calidexpand)(icalcstps* cstp, char* calid);
  icalerrorenum (*capability)(icalcstps* cstp);
  icalerrorenum (*cont)(icalcstps* cstp, unsigned int time);
  icalerrorenum (*identify)(icalcstps* cstp, char* id);
  icalerrorenum (*disconnect)(icalcstps* cstp);
  icalerrorenum (*sendata)(icalcstps* cstp, unsigned int time,
                               icalcomponent *comp);
  icalerrorenum (*starttls)(icalcstps* cstp, char* command,
                                char* data);
  icalerrorenum (*upnexpand)(icalcstps* cstp, char* upn);
  icalerrorenum (*unknown)(icalcstps* cstp, char* command, char* data);
};                                                        



icalcstps* icalcstps_new(struct icalcstps_commandfp stubs);

void icalcstps_free(icalcstps* cstp);

int icalcstps_set_timeout(icalcstps* cstp, int sec);

/* Get the next string to send to the client */
char* icalcstps_next_output(icalcstps* cstp);

/* process the next string from the client */ 
int icalcstps_next_input(icalcstps* cstp);

#endif /* ICALCSTPS */
