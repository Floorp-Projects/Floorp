/* -*- Mode: C -*-
    ======================================================================
    FILE: icalcstps.c
    CREATOR: ebusboom 23 Jun 2000
  
    $Id: icalcstp.c,v 1.5 2002/11/06 21:22:41 mostafah%oeone.com Exp $
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


struct command_map {
	enum icalcstp_command command;
	char *str;
} command_map[] = 
{
    {ICAL_ABORT_COMMAND,"ABORT"},
    {ICAL_AUTHENTICATE_COMMAND,"AUTHENTICATE"},
    {ICAL_CAPABILITY_COMMAND,"CAPABILITY"},
    {ICAL_CONTINUE_COMMAND,"CONTINUE"},
    {ICAL_CALIDEXPAND_COMMAND,"CALIDEXPAND"},
    {ICAL_IDENTIFY_COMMAND,"IDENTIFY"},
    {ICAL_DISCONNECT_COMMAND,"DISCONNECT"},
    {ICAL_SENDDATA_COMMAND,"SENDDATA"},
    {ICAL_STARTTLS_COMMAND,"STARTTLS"},
    {ICAL_UPNEXPAND_COMMAND,"UPNEXPAND"},
    {ICAL_UNKNOWN_COMMAND,"UNKNOWN"}
};


icalcstp_command icalcstp_line_command(char* line)
{
    int i;

    for(i = 0; command_map[i].command != ICAL_UNKNOWN_COMMAND; i++){
        size_t l = strlen(command_map[i].str);

        if(strncmp(line, command_map[i].str, l) == 0){
            return command_map[i].command;
        }
    
    }

    return ICAL_UNKNOWN_COMMAND;
}

icalrequeststatus icalcstp_line_response_code(char* line)
{
    struct icalreqstattype rs; 

    rs = icalreqstattype_from_string(line);

    return rs.code;
}

int icalcstp_line_is_endofdata(char* line)
{
    if(line[0] == '.' && line[1] == '\n'){
        return 1;
    }

    return 0;

}

int icalcstp_line_is_mime(char* line)
{
}


const char* icalcstp_command_to_string(icalcstp_command command){

    int i;

    for(i = 0; command_map[i].command != ICAL_UNKNOWN_COMMAND; i++){
        size_t l = strlen(command_map[i].str);

        if(command_map[i].command == command){
            return command_map[i].str;
        }
    
    }

    return command_map[i].str;

}

