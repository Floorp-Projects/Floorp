/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* forks a telnet process
 *
 * Designed and implemented by Lou Montulli
 */

/* Please leave outside of ifdef for windows precompiled headers */

#include "xp.h"
#include "plstr.h"
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#ifdef MOZILLA_CLIENT

#include "mkparse.h"
#include "remoturl.h"
#include "glhist.h"
#include "mkgeturl.h"   /* for URL types */
#include "merrors.h"

/*
 * begin a session with a remote host.
 *
 * URL types permitted: Telnet, TN3270, and Rlogin
 */
MODULE_PRIVATE int32
NET_RemoteHostLoad (ActiveEntry  * cur_entry)
{
    char * host_string;
    int url_type;
    char * cp;
    char * username;
    char * hostname;
    char * port_string;

    TRACEMSG(("In NET_RemoteHostLoad"));

    GH_UpdateGlobalHistory(cur_entry->URL_s);

    if(cur_entry->format_out == FO_CACHE_AND_PRESENT || cur_entry->format_out == FO_PRESENT)
      {
        url_type = NET_URL_Type(cur_entry->URL_s->address);
        host_string = NET_ParseURL(cur_entry->URL_s->address, GET_USERNAME_PART | GET_PASSWORD_PART | GET_HOST_PART);

    	hostname = PL_strchr(host_string, '@');
    	port_string = PL_strchr(host_string, ':');

    	if (hostname)
      	  {
        	*hostname++ = '\0';  
    	    username = NET_UnEscape(host_string);
          }
        else
          {
            hostname = host_string;
            username = NULL;  /* no username given */
          }

        if (port_string)
		  {
            *port_string++ = '\0';  

			/* Sanity check the port part
             * prevent telnet://hostname:30;rm -rf *  URL's (VERY BAD)
             * only allow digits
             */
            for(cp=port_string; *cp != '\0'; cp++)
                if(!isdigit(*cp))
                  {
                    *cp = '\0';
                    break;
                  }
		  }

	   
		if(username)
		  {
			/* Sanity check the username part
             * prevent telnet://hostname:30;rm -rf *  URL's (VERY BAD)
             * only allow alphanums
             */
            for(cp=username; *cp != '\0'; cp++)
                if(!isalnum(*cp))
                  {
                    *cp = '\0';
                    break;
                  }
		  }

		/* now sanity check the hostname part
         * prevent telnet://hostname;rm -rf *  URL's (VERY BAD)
         * only allow alphanumeric characters and a few symbols
         */
        for(cp=hostname; *cp != '\0'; cp++)
        if(!isalnum(*cp) && *cp != '_' && *cp != '-' &&
              *cp != '+' && *cp != ':' && *cp != '.' && *cp != '@')
          {
            *cp = '\0';
            break;
          }

		TRACEMSG(("username: %s, hostname: %s, port: %s", 
				  username ? username : "(null)",
				  hostname ? hostname : "(null)",
				  port_string ? port_string : "(null)"));

		if(url_type == TELNET_TYPE_URL)
		  {
		    FE_ConnectToRemoteHost(cur_entry->window_id, FE_TELNET_URL_TYPE, hostname, port_string, username);
		  }
		else if(url_type == TN3270_TYPE_URL)
		  {
		    FE_ConnectToRemoteHost(cur_entry->window_id, FE_TN3270_URL_TYPE, hostname, port_string, username);
		  }
		else if(url_type == RLOGIN_TYPE_URL)
		  {
		    FE_ConnectToRemoteHost(cur_entry->window_id, FE_RLOGIN_URL_TYPE, hostname, port_string, username);
		  }
		/* fall through if it wasn't any of the above url types */

        PR_Free(host_string);
      }

	cur_entry->status = MK_NO_DATA;
    return -1;
}

PRIVATE int32
net_ProcessRemote(ActiveEntry *ce)
{
	PR_ASSERT(0);
	return -1;
}

PRIVATE int32
net_InterruptRemote(ActiveEntry *ce)
{
	PR_ASSERT(0);
	return -1;
}

PRIVATE void
net_CleanupRemote(void)
{
}

MODULE_PRIVATE void
NET_InitRemoteProtocol(void)
{
    static NET_ProtoImpl remote_proto_impl;

    remote_proto_impl.init = NET_RemoteHostLoad;
    remote_proto_impl.process = net_ProcessRemote;
    remote_proto_impl.interrupt = net_InterruptRemote;
    remote_proto_impl.resume = NULL;
    remote_proto_impl.cleanup = net_CleanupRemote;

    NET_RegisterProtocolImplementation(&remote_proto_impl, RLOGIN_TYPE_URL);
    NET_RegisterProtocolImplementation(&remote_proto_impl, TELNET_TYPE_URL);
    NET_RegisterProtocolImplementation(&remote_proto_impl, TN3270_TYPE_URL);
}

#endif /* MOZILLA_CLIENT */
