/* -*- Mode: C -*-
    ======================================================================
    FILE: icalcstpserver.c
    CREATOR: ebusboom 13 Feb 01
  
    $Id: icalcstpserver.c,v 1.5 2002/11/06 21:22:43 mostafah%oeone.com Exp $
    $Locker:  $
    
    (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of either: 
    
    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html
    
    Or:
    
    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


    ======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ical.h"
#include "icalcstp.h"
#include "icalcstpserver.h"
#include "pvl.h" 

#ifndef XP_MAC
#include <sys/types.h> /* For send(), others */
#ifndef WIN32
#include <sys/socket.h>  /* For send(), others. */
#include <unistd.h> /* For alarm */
#endif
#endif
#include <errno.h>
#include <stdlib.h> /* for malloc */
#include <string.h>
#ifdef XP_MAC
#include <extras.h> /* for strdup */
#endif



struct icalcstps_impl {
	int timeout;
	icalparser *parser;
	enum cstps_state major_state;
	struct icalcstps_commandfp commandfp;
};




/* This state machine is a Mealy-type: actions occur on the
   transitions, not in the states.
   
   Here is the state machine diagram from the CAP draft:


        STARTTLS /
        CAPABILITY
       +-------+
       |       |                       +---------------+
       |   +-----------+ AUTHENTICATE  |               |
       +-->| Connected |-------------->| Authenticated |
           +-----------+               |               |
             |                         +---------------+
             |                              |
             |                              |
             |                              |
             |                              |       +-----+ STARTTLS /
             |                              V       |     | CAPABILITY /
             |                         +---------------+  | IDENTIFY
             |                         |               |<-+
             |                         |   Identified  |<----+
             |                +--------|               |     |
             |                |        +---------------+     | command
             |                |             |                | completes
             V                |DISCONNECT   |                |
           +--------------+   |             |SENDDATA        |
           | Disconnected |<--+             |                |
           +--------------+                 |                | ABORT
                     A                      |                |
                     |                      V                |
                     |     DISCONNECT     +---------------+  |
                     +--------------------|    Receive    |--+
                                          |               |<--+
                                          +---------------+   |
                                                         |    | CONTINUTE
                                                         +----+

   In this implmenetation, the transition from CONNECTED to IDENTIFIED
   is non-standard. The spec specifies that on the ATHENTICATE
   command, the machine transitions from CONNECTED to AUTHENTICATED,
   and then immediately goes to IDENTIFIED. This makes AUTHENTICATED a
   useless state, so I removed it */

struct state_table {
	enum cstps_state major_state;
	enum icalcstp_command command;
	void (*action)();
	enum cstps_state next_state;

} server_state_table[] = 
{
    { CONNECTED, ICAL_CAPABILITY_COMMAND , 0, CONNECTED},
    { CONNECTED, ICAL_AUTHENTICATE_COMMAND , 0,  IDENTIFIED}, /* Non-standard */
    { IDENTIFIED, ICAL_STARTTLS_COMMAND, 0, IDENTIFIED},
    { IDENTIFIED, ICAL_IDENTIFY_COMMAND, 0, IDENTIFIED},
    { IDENTIFIED, ICAL_CAPABILITY_COMMAND, 0, IDENTIFIED},
    { IDENTIFIED, ICAL_SENDDATA_COMMAND, 0, RECEIVE},
    { IDENTIFIED, ICAL_DISCONNECT_COMMAND, 0, DISCONNECTED},
    { DISCONNECTED, 0, 0, 0},
    { RECEIVE,  ICAL_DISCONNECT_COMMAND, 0, DISCONNECTED},
    { RECEIVE,  ICAL_CONTINUE_COMMAND, 0, RECEIVE},
    { RECEIVE,  ICAL_ABORT_COMMAND , 0, IDENTIFIED},
    { RECEIVE,  ICAL_COMPLETE_COMMAND , 0, IDENTIFIED}
};


/**********************************************************************/



icalcstps* icalcstps_new(struct icalcstps_commandfp cfp)
{
    struct icalcstps_impl* impl;

    if ( ( impl = (struct icalcstps_impl*)
	   malloc(sizeof(struct icalcstps_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    impl->commandfp = cfp;
    impl->timeout = 10;

    return (icalcstps*)impl;

}

void icalcstps_free(icalcstps* cstp);

int icalcstps_set_timeout(icalcstps* cstp, int sec) 
{
    struct icalcstps_impl *impl = (struct icalcstps_impl *) cstp;

    icalerror_check_arg_rz( (cstp!=0), "cstp");

    impl->timeout = sec;

    return sec;
}

typedef struct icalcstps_response {	
	icalrequeststatus code;
	char caluid[1024];
	void* result;
} icalcstps_response;


icalerrorenum prep_abort(struct icalcstps_impl* impl, char* data)
{
    return ICAL_NO_ERROR;
}
icalerrorenum prep_authenticate(struct icalcstps_impl* impl, char* data)
{    return ICAL_NO_ERROR;
}
icalerrorenum prep_capability(struct icalcstps_impl* impl, char* data)
{    return ICAL_NO_ERROR;
}
icalerrorenum prep_calidexpand(struct icalcstps_impl* impl, char* data)
{
    return ICAL_NO_ERROR;
}
icalerrorenum prep_continue(struct icalcstps_impl* impl, char* data)
{
    return ICAL_NO_ERROR;
}
icalerrorenum prep_disconnect(struct icalcstps_impl* impl, char* data)
{
    return ICAL_NO_ERROR;
}
icalerrorenum prep_identify(struct icalcstps_impl* impl, char* data)
{
    return ICAL_NO_ERROR;
}
icalerrorenum prep_starttls(struct icalcstps_impl* impl, char* data)
{
    return ICAL_NO_ERROR;
}
icalerrorenum prep_upnexpand(struct icalcstps_impl* impl, char* data)
{
    return ICAL_NO_ERROR;
}
icalerrorenum prep_sendata(struct icalcstps_impl* impl, char* data)
{    return ICAL_NO_ERROR;
}

char* icalcstps_process_incoming(icalcstps* cstp, char* input)
{
    struct icalcstps_impl *impl = (struct icalcstps_impl *) cstp;
    char *i;
    char *cmd_or_resp;
    char *data;
    char *input_cpy;
    icalerrorenum error;

    icalerror_check_arg_rz(cstp !=0,"cstp");
    icalerror_check_arg_rz(input !=0,"input");

    if ((input_cpy = (char*)strdup(input)) == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    i = (char*)strstr(" ",input_cpy);

    cmd_or_resp = input_cpy;

    if (i != 0){
	*i = '\0';
	data = ++i;
    } else {
	data = 0;
    }

    printf("cmd: %s\n",cmd_or_resp);
    printf("data: %s\n",data);
	
    /* extract the command, look up in the state table, and dispatch
       to the proper handler */    

    if(strcmp(cmd_or_resp,"ABORT") == 0){
	error = prep_abort(impl,data);	
    } else if(strcmp(cmd_or_resp,"AUTHENTICATE") == 0){
	error = prep_authenticate(impl,data);
    } else if(strcmp(cmd_or_resp,"CAPABILITY") == 0){
	error = prep_capability(impl,data);
    } else if(strcmp(cmd_or_resp,"CALIDEXPAND") == 0){
	error = prep_calidexpand(impl,data);
    } else if(strcmp(cmd_or_resp,"CONTINUE") == 0){
	error = prep_continue(impl,data);
    } else if(strcmp(cmd_or_resp,"DISCONNECT") == 0){
	error = prep_disconnect(impl,data);
    } else if(strcmp(cmd_or_resp,"IDENTIFY") == 0){
	error = prep_identify(impl,data);
    } else if(strcmp(cmd_or_resp,"STARTTLS") == 0){
	error = prep_starttls(impl,data);
    } else if(strcmp(cmd_or_resp,"UPNEXPAND") == 0){
	error = prep_upnexpand(impl,data);
    } else if(strcmp(cmd_or_resp,"SENDDATA") == 0){
	error = prep_sendata(impl,data);
    }
    
    return 0;
}

    /* Read data until we get a end of data marker */



struct icalcstps_server_stubs {
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

