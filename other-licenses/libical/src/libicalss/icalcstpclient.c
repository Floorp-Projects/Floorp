/* -*- Mode: C -*-
    ======================================================================
    FILE: icalcstps.c
    CREATOR: ebusboom 23 Jun 2000
  
    $Id: icalcstpclient.c,v 1.7 2002/11/06 21:22:43 mostafah%oeone.com Exp $
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
#include "icalcstpclient.h"
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

#define EOL "\n"


/* Client state machine */

typedef enum icalcstpc_line_type {
    ICALCSTPC_RESPONSE_CODE_LINE,
    ICALCSTPC_TERMINATOR_LINE,
    ICALCSTPC_APPLICATION_DATA_LINE
} icalcstpc_line_type;

typedef enum icalcstpc_state {
    ICALCSTPC_SEND_STATE,
    ICALCSTPC_RESPONSE_CODE_STATE,
    ICALCSTPC_RESPONSE_DATA_STATE
} icalcstpc_state;



struct icalcstpc_impl {
    int timeout;
    icalparser *parser;
    icalcstp_command command;
    icalcstpc_state state;
    char* next_output;
    char* next_input;
};

icalcstpc* icalcstpc_new()
{
    struct icalcstpc_impl *impl;

    impl = malloc(sizeof(struct icalcstpc_impl));
    
    if(impl == 0){
        icalerror_set_errno(ICAL_NEWFAILED_ERROR);
        return 0;
    }

    memset(impl,0,sizeof(struct icalcstpc_impl));

    return impl;
}

void icalcstpc_free(icalcstpc* cstpc)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstpc;

    if(impl->next_output != 0){
        free(impl->next_output);
    }

    if(impl->next_input != 0){
        free(impl->next_input);
    }
    
    
    if(impl->parser != 0){
        icalparser_free(impl->parser);
    }
}

/* Get the next string to send to the server */
char* icalcstpc_next_output(icalcstpc* cstp, char * line)
{
    char* out;
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;

    if(impl->next_output == 0){
        return 0;
    }
    
    out = impl->next_output;

    impl->next_output = 0;

    icalmemory_add_tmp_buffer(out);

    return out;
}

/* process the next string sent by the server */ 
int icalcstpc_next_input(icalcstpc* cstp, char* line)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;
    icalcstpc_line_type line_type;

    if(icalcstp_line_is_endofdata(line) || line == 0){
        return 0;
    }

    switch (impl->command){
    case ICAL_ABORT_COMMAND:{
        break;
    }
    case ICAL_AUTHENTICATE_COMMAND:{
        break;
    }
    case ICAL_CAPABILITY_COMMAND:{
        break;
    }
    case ICAL_CONTINUE_COMMAND:{
        break;
    }
    case ICAL_CALIDEXPAND_COMMAND:{
        break;
    }
    case ICAL_IDENTIFY_COMMAND:{
        break;
    }
    case ICAL_DISCONNECT_COMMAND:{
        break;
    }
    case ICAL_SENDDATA_COMMAND:{
        break;
    }
    case ICAL_STARTTLS_COMMAND:{
        break;
    }
    case ICAL_UPNEXPAND_COMMAND:{
        break;
    }
    case ICAL_COMPLETE_COMMAND:{
        break;
    }
    case ICAL_UNKNOWN_COMMAND:{
        break;
    }
    default:
        break;
    }
	return 0;
}

/* After icalcstpc_next_input returns a 0, there are responses
   ready. use these to get them */
icalcstpc_response icalcstpc_first_response(icalcstpc* cstp)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;

}


icalcstpc_response icalcstpc_next_response(icalcstpc* cstp)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;
}


int icalcstpc_set_timeout(icalcstpc* cstp, int sec)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;
	return 1;
}

icalerrorenum icalcstpc_abort(icalcstpc* cstp)
{
    struct icalcstpc_impl* impl = (struct icalcstpc_impl*)cstp;
    
    icalerror_check_arg_re(cstp!=0,"cstp",ICAL_BADARG_ERROR);

    impl->next_output = "ABORT\n";

    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpclient_setup_output(icalcstpc* cstp, size_t sz)
{
   struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;

   if(impl->next_output != 0){
       icalerror_set_errno(ICAL_USAGE_ERROR);
       return ICAL_USAGE_ERROR;
   }

   impl->next_output = malloc(sz);

   if(impl->next_output == 0){
       icalerror_set_errno(ICAL_NEWFAILED_ERROR);
       return ICAL_NEWFAILED_ERROR;
   }
   
   return ICAL_NO_ERROR;

}

icalerrorenum icalcstpc_authenticate(icalcstpc* cstp, char* mechanism, 
                                        char* data, char* f(char*))
{
   struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;
   const char* command_str;
   icalerrorenum error;
   size_t sz;

   icalerror_check_arg_re(cstp!=0,"cstp",ICAL_BADARG_ERROR);
   icalerror_check_arg_re(mechanism!=0,"mechanism",ICAL_BADARG_ERROR);
   icalerror_check_arg_re(data!=0,"data",ICAL_BADARG_ERROR);
   icalerror_check_arg_re(f!=0,"f",ICAL_BADARG_ERROR);

   impl->command = ICAL_AUTHENTICATE_COMMAND;

   command_str = icalcstp_command_to_string(impl->command);

   sz = strlen(command_str) + strlen(mechanism) + strlen(data) + 4;

   if((error=icalcstpclient_setup_output(cstp,sz)) != ICAL_NO_ERROR){
       return error;
   }

   sprintf(impl->next_output,"%s %s %s%s",command_str,mechanism,data,EOL);

   return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_capability(icalcstpc* cstp)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;
    const char* command_str;
    icalerrorenum error;
    size_t sz;

    icalerror_check_arg_re(cstp!=0,"cstp",ICAL_BADARG_ERROR);
    
    impl->command = ICAL_CAPABILITY_COMMAND;
    
    command_str = icalcstp_command_to_string(impl->command);

    sz = strlen(command_str);
    
    if((error=icalcstpclient_setup_output(cstp,sz)) != ICAL_NO_ERROR){
        return error;
    }

    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_calidexpand(icalcstpc* cstp,char* calid)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;

    impl->command = ICAL_CALIDEXPAND_COMMAND;
    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_continue(icalcstpc* cstp, unsigned int time)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;

    impl->command = ICAL_CONTINUE_COMMAND;
    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_disconnect(icalcstpc* cstp)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;


    impl->command = ICAL_DISCONNECT_COMMAND;

    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_identify(icalcstpc* cstp, char* id)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;


    impl->command = ICAL_IDENTIFY_COMMAND;

    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_starttls(icalcstpc* cstp, char* command, 
                                    char* data, char * f(char*))
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;

    impl->command = ICAL_STARTTLS_COMMAND;
    
    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_upnexpand(icalcstpc* cstp,char* calid)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;


    impl->command = ICAL_UPNEXPAND_COMMAND;

    return ICAL_NO_ERROR;
}

icalerrorenum icalcstpc_sendata(icalcstpc* cstp, unsigned int time,
                                   icalcomponent *comp)
{
    struct icalcstpc_impl *impl = (struct icalcstpc_impl *)cstp;

    impl->command = ICAL_SENDDATA_COMMAND;
    
    return ICAL_NO_ERROR;
}




