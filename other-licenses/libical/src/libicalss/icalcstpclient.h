/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalcstpclient.h
  CREATOR: eric 4 Feb 01
  
  $Id: icalcstpclient.h,v 1.2 2001/12/21 18:56:35 mikep%oeone.com Exp $


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

#include "ical.h"
#include "icalcstp.h"

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



