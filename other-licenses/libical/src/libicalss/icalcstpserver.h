/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalcstpserver.h
  CREATOR: eric 13 Feb 01
  
  $Id: icalcstpserver.h,v 1.2 2001/12/21 18:56:36 mikep%oeone.com Exp $


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

#include "ical.h"


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
