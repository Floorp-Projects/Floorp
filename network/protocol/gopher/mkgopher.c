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
/* A state machine to implement the Gopher protocol
 * Designed and Implemented by Lou Montulli circa '94
 */

#include "rosetta.h"
#include "xp.h"
#include "plstr.h"
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "prerror.h"
 
/* Gopher types:
 */
#define GTYPE_TEXT     	   '0'        /* text/plain */
#define GTYPE_MENU     	   '1'        /* menu - becomes text/html */
#define GTYPE_CSO      	   '2'        /* cso search - becomes text/html */
#define GTYPE_ERROR        '3'     /* some sort of error */
#define GTYPE_MACBINHEX    '4'     /* application/binhex */
#define GTYPE_PCBINARY     '5'     /* application/octet-stream */
#define GTYPE_UUENCODED    '6'     /* application/uuencoded */
#define GTYPE_INDEX        '7'     /* search - becomes text/html */
#define GTYPE_TELNET       '8'     /* maps to telnet URL */
#define GTYPE_BINARY       '9'     /* application/octet-stream */
#define GTYPE_GIF          'g'     /* image/gif */
#define GTYPE_HTML         'h'     /* HTML URL */
#define GTYPE_HTMLCAPS     'H'     /* HTML URL Capitalized */
#define GTYPE_INFO         'i'     /* unselectable info */
#define GTYPE_IMAGE        'I'     /* image/gif */
#define GTYPE_MIME         'm'     /* not used */
#define GTYPE_SOUND        's'     /* audio/(wildcard) */
#define GTYPE_TN3270       'T'     /* maps to tn3270 program (url) */
#define GTYPE_WWW          'w'     /* W3 address */
#define GTYPE_MPEG         ';'     /* video/mpeg */

#include "mkgeturl.h"       /* URL Struct */
#include "mkstream.h"
#include "mkformat.h"       /* for File Format stuff (cinfo) */
#include "gophurl.h"       /* function prototypes */
#include "mktcp.h"          /* connect, read, etc. */
#include "mkparse.h"        /* NET_ParseURL() */
#include "remoturl.h"       /* NET_RemoteHostLoad */

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_HTML_GOPHER_INDEX;
extern int XP_HTML_CSO_SEARCH;
extern int XP_PROGRESS_WAITREPLY_GOTHER;
extern int MK_OUT_OF_MEMORY;
extern int MK_SERVER_DISCONNECTED;
extern int MK_TCP_READ_ERROR;
extern int MK_TCP_WRITE_ERROR;
extern int XP_ERRNO_EWOULDBLOCK;
extern int XP_SERVER_RETURNED_NO_DATA;
extern int MK_MALFORMED_URL_ERROR;

#include "merrors.h"

#define CD_OUTPUT_BUFFER_SIZE 4*1024

typedef struct _GopherConData {
    int              next_state;        /* the next state or action to be taken */
    char            *data_buf;          /* temporary string storage */
    int32            data_buf_size;     /* current size of the line buffer */
    char             gopher_type;             /* the gopher type */
    char            *command;           /* the request command */
	char            *output_buf;        /* temporary output buffer */
    NET_StreamClass *stream;            /* The output stream */
    Bool          pause_for_read;    /* Pause now for next read? */
    Bool destroy_graph_progress;  /* do we need to destroy graph progress? */
    int32   original_content_length; /* the content length at the time of
                                      * calling graph progress
                                      */
	char             cso_last_char;
	TCP_ConData      *tcp_con_data;
} GopherConData;

#define PUTSTRING(s)     (*connection_data->stream->put_block) \
               					(connection_data->stream, s, PL_strlen(s))
#define PUTBLOCK(b, l)   (*connection_data->stream->put_block) \
               					(connection_data->stream, b, l)
#define COMPLETE_STREAM  (*connection_data->stream->complete) \
               					(connection_data->stream)
#define ABORT_STREAM(s)  (*connection_data->stream->abort) \
               					(connection_data->stream, s)

/* helpful shortcut names
 */
#define CD_NEXT_STATE           connection_data->next_state
#define CD_DATA_BUF             connection_data->data_buf
#define CD_DATA_BUF_SIZE        connection_data->data_buf_size
#define CD_STREAM       		connection_data->stream
#define CD_GOPHER_TYPE        	connection_data->gopher_type
#define CD_COMMAND      		connection_data->command
#define CD_OUTPUT_BUF      		connection_data->output_buf
#define CD_PAUSE_FOR_READ       connection_data->pause_for_read
#define CD_DESTROY_GRAPH_PROGRESS   connection_data->destroy_graph_progress
#define CD_ORIGINAL_CONTENT_LENGTH  connection_data->original_content_length
#define CD_CSO_LAST_CHAR			connection_data->cso_last_char
#define CD_TCP_CON_DATA			connection_data->tcp_con_data


#define CE_WINDOW_ID            cur_entry->window_id
#define CE_URL_S        		cur_entry->URL_s
#define CE_STATUS       		cur_entry->status
#define CE_SOCK         		cur_entry->socket
#define CE_CON_SOCK     		cur_entry->con_sock
#define CE_BYTES_RECEIVED       cur_entry->bytes_received

/* states for the state machine
 */
#define GOPHER_BEGIN_CONNECT    1
#define GOPHER_FINISH_CONNECT   2
#define GOPHER_SEND_REQUEST     3
#define GOPHER_BEGIN_TRANSFER   4
#define GOPHER_TRANSFER_MENU    5
#define GOPHER_TRANSFER_CSO     6
#define GOPHER_TRANSFER_BINARY  7
#define GOPHER_DONE     		8
#define GOPHER_ERROR_DONE   	9
#define GOPHER_FREE             10


/* forward decl */
PRIVATE int32 net_ProcessGopher(ActiveEntry * cur_entry);

/* net_begin_gopher_menu
 *
 * adds little bits to the begining of the document so that is looks nice
 */

PRIVATE int 
net_begin_gopher_menu(GopherConData * connection_data)
{

    CD_NEXT_STATE = GOPHER_TRANSFER_MENU;

    PL_strcpy(CD_OUTPUT_BUF, "<H1>Gopher Menu</H1>\n<PRE>");
   	return(PUTSTRING(CD_OUTPUT_BUF));

}

/* parse the gopher menu type
 */
PRIVATE int 
net_parse_menu (ActiveEntry * cur_entry)
{
    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;
    char gopher_type;
    char *name=NULL;
	char *gopher_path=0; 
    char *port;         
    char *ptr;
    char *line;
    char *host=NULL;        

    CE_STATUS = NET_BufferedReadLine(CE_SOCK, &line, &CD_DATA_BUF,
                                                &CD_DATA_BUF_SIZE, &CD_PAUSE_FOR_READ);

    if(CE_STATUS == 0)
	  {
        CD_NEXT_STATE = GOPHER_DONE;
        CD_PAUSE_FOR_READ = FALSE;
		if(CE_BYTES_RECEIVED < 4)
		  {
			PL_strcpy(CD_OUTPUT_BUF, XP_GetString( XP_SERVER_RETURNED_NO_DATA ) );
        	PUTSTRING(CD_OUTPUT_BUF);
		  }

       	return(MK_DATA_LOADED);
	  }

    /* if TCP error of if there is not a full line yet return
     */
    if(!line)
      {
         return CE_STATUS;
      }
    else if(CE_STATUS < 0)
      {
        NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
      }

	if(CE_STATUS > 1)
	  {
		CE_BYTES_RECEIVED += CE_STATUS;
        FE_GraphProgress(CE_WINDOW_ID, CE_URL_S, CE_BYTES_RECEIVED, CE_STATUS, CE_URL_S->content_length);
	  }

    gopher_type = *line;

	if(gopher_type != '\0')
        ptr = line+1;
	else
		return(0);

    /* remove trailing spaces */
    XP_StripLine(line);

    TRACEMSG(("gopher_parse_menu: parsing line: %s\n",line));

    /* quit when just a dot is found on a line by itself
	 */
    if(!PL_strcmp(line,"."))
      {
        CD_NEXT_STATE = GOPHER_DONE;
        CD_PAUSE_FOR_READ = FALSE;

		if(CE_BYTES_RECEIVED < 4)
		  {
			PL_strcpy(CD_OUTPUT_BUF, XP_GetString( XP_SERVER_RETURNED_NO_DATA ) );
        	PUTSTRING(CD_OUTPUT_BUF);
		  }
			
        return(MK_DATA_LOADED);
      }

    if (gopher_type && *ptr) 
	  {
        name = ptr;
        gopher_path = PL_strchr(name, '\t');
        if (gopher_path) 
		  {
            *gopher_path++ = 0;
            host = PL_strchr(gopher_path, '\t');
            if (host) 
			  {
                *host++ = 0;    
                port = PL_strchr(host, '\t');
                if (port) 
				  {
                    char *tab;

                    port[0] = ':';    /* fake the port no */
                    tab = PL_strchr(port, '\t');
                    if (tab) 
						*tab++ = '\0';  

                    if (port[1]=='0' && port[2]=='\0')
                        port[0] = 0;    
                } 
             } 
        } 
    } 

	if(!gopher_path)
	  {
        return(PUTSTRING(ptr)); /* keep going bad data */
	  }
        
    /* Nameless files are a separator line */
    if (gopher_type == GTYPE_TEXT) 
      {
        int i=0;
        while(NET_IS_SPACE(name[i])) i++;
        if(!PL_strlen(name))
        gopher_type = GTYPE_INFO;
      }

    /* these first few are grouped together with an
     * if else because they each have special needs 
     * the rest of them fit nicely in the switch
     */
	if(gopher_type == GTYPE_ERROR)
	  {
		/* ignore it */
	  }
    else if(gopher_type == GTYPE_WWW)
      {
        /* points to URL */
		PL_strcpy(CD_OUTPUT_BUF, "<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-text\">");
        CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);

		if(CE_STATUS < 0)
			return(CE_STATUS);

    	PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "<A HREF=\"%s\">%s</A>\n", gopher_path, name);
    	return(PUTSTRING(CD_OUTPUT_BUF));

      }
    else if(gopher_type == GTYPE_INFO)
      {
            /* Information or separator line */
		PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "       %s\n", name);
        CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
       }
    else if(gopher_type == GTYPE_TELNET)
      {
        if (*gopher_path) 
		  {
			char * temp = NET_Escape(gopher_path, URL_XALPHAS);
		    PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "<A HREF=\"telnet://%s@%s/\"><IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-telnet\"> %s</A>\n", temp, host, name);
			PR_Free(temp);
		  }
        else 
		  {
		    PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "<A HREF=\"telnet://%s/\"><IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-telnet\"> %s</A>\n", host, name);
		  }
    	CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
      }
    else if(gopher_type == GTYPE_TN3270)
      {
        if (gopher_path && *gopher_path) 
		    PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "<A HREF=\"tn3270://%s@%s/\"><IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-telnet\"> %s</A>\n", gopher_path, host, name);
        else 
		    PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "<A HREF=\"tn3270://%s/\"><IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-telnet\"> %s</A>\n", host, name);
    	CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
      }
    else
      {
        if(host && gopher_path) 
	 	  {
			if(!PL_strcmp(host, "error.host:70"))
			  {
				
                PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "Error: %s", name);
                CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
				
			  }
			else
			  {
			    char * newpath = NET_Escape(gopher_path, URL_PATH);
    
                PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "<A HREF=\"gopher://%s/%c%s\">", host, gopher_type, newpath);
    
                switch(gopher_type) {
                    case GTYPE_TEXT:
        		    case GTYPE_HTML:
        		    case GTYPE_HTMLCAPS:
            		  PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
									    "<IMG ALIGN=absbottom BORDER=0 "
										"SRC=\"internal-gopher-text\">");
                            break;
                    case GTYPE_MENU:
            			    PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
											    "<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-menu\">");
                            break;
                    case GTYPE_CSO:
                    case GTYPE_INDEX:
            			    PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
											    "<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-index\">");
                            break;
                    case GTYPE_PCBINARY:
                    case GTYPE_UUENCODED:
                    case GTYPE_BINARY:
                    case GTYPE_MACBINHEX:
            			    PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
											    "<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-binary\">");
                            break;
                    case GTYPE_IMAGE:
                    case GTYPE_GIF:
            			    PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
											    "<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-image\">");
                            break;
                    case GTYPE_SOUND:
            			    PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
											    "<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-sound\">");
                            break;
        		    case GTYPE_MPEG:
            			    PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
											    "<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-movie\">");
                		    break;
    
                    case GTYPE_MIME:
                    default:
            			    PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
											"<IMG ALIGN=absbottom BORDER=0 SRC=\"internal-gopher-unknown\">");
                            break;
            	}

                PR_snprintf(&CD_OUTPUT_BUF[PL_strlen(CD_OUTPUT_BUF)], 
										CD_OUTPUT_BUFFER_SIZE 
												- PL_strlen(CD_OUTPUT_BUF),
										" %s</A>\n", name);
			    PR_Free(newpath);

                CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
			  }	
          }
      }

    return(CE_STATUS); /* keep going */

}

/* net_begin_gopher_cso
 *
 * adds formatting to the front of the document to make
 * it look pretty
 */
PRIVATE int
net_begin_gopher_cso(GopherConData * connection_data)
{
    PL_strcpy(CD_OUTPUT_BUF, "<H1>CSO Search Results</H1>\n<PRE>");

    CD_NEXT_STATE = GOPHER_TRANSFER_CSO;

    return(PUTSTRING(CD_OUTPUT_BUF));
}

/* 
 * parse cso output
 *
 * receives date from the cso server and turns it into HTML
 */
PRIVATE int 
net_parse_cso (ActiveEntry * cur_entry)
{
    char *line;
    char *colon1, *colon2; 
    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;
    
    CE_STATUS = NET_BufferedReadLine(CE_SOCK, &line, &CD_DATA_BUF,
                                                &CD_DATA_BUF_SIZE, &CD_PAUSE_FOR_READ);

    if(CE_STATUS == 0)
    CE_STATUS = MK_SERVER_DISCONNECTED;
    CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_SERVER_DISCONNECTED);
    
    /* if TCP error of if there is not a full line yet return
     */
    if(CE_STATUS < 0)
      {
        NET_ExplainErrorDetails(MK_TCP_READ_ERROR, PR_GetOSError());

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
      }
 	else if(!line)
      {
         return CE_STATUS;
      }

    if(CE_STATUS > 1)
      { 
        CE_BYTES_RECEIVED += CE_STATUS;
        FE_GraphProgress(CE_WINDOW_ID, CE_URL_S, CE_BYTES_RECEIVED, CE_STATUS, CE_URL_S->content_length);
      }

    /* a line beginning with a 2 means the end of data
     */
    if (*line == '2')
      {
    	CD_NEXT_STATE = GOPHER_DONE;
        CD_PAUSE_FOR_READ = FALSE;
        return(MK_DATA_LOADED);
      }
            
    /* if it starts with a 5 something went wrong, print
     * out the error message 
     */
    if (*line == '5') 
      {
    	PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, "<H2>%s</H2>", line+4);
    	PUTSTRING(CD_OUTPUT_BUF);
    	CD_NEXT_STATE = GOPHER_DONE;
        CD_PAUSE_FOR_READ = FALSE;
        return(MK_DATA_LOADED);
      }
            
    if(*line == '-') 
      {
		colon1 = PL_strchr(line,':');
		if(colon1)
            colon2 = PL_strchr(colon1+1, ':'); 
		else
			colon2 = NULL;
    
        if(colon2 != NULL)
          {
            if (*(colon2-1) != CD_CSO_LAST_CHAR)   
              { /* print seperator */
    			PL_strcpy(CD_OUTPUT_BUF, "</PRE><H2>");
    			CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
              }
                    
			if(CE_STATUS > -1)
            	CE_STATUS = PUTSTRING(colon2+1);
			PL_strcpy(CD_OUTPUT_BUF, "\n");
			if(CE_STATUS > -1)
				CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
                
            if (*(colon2-1) != CD_CSO_LAST_CHAR)   
              {
                /* end seperator */
    			PL_strcpy(CD_OUTPUT_BUF, "</H2><PRE>");
				if(CE_STATUS > -1)
    				CE_STATUS = PUTSTRING(CD_OUTPUT_BUF);
              }
                                    
            /* remember the last char so that we can
			 * tell when the sequence number changes
             */
            CD_CSO_LAST_CHAR =  *(colon2-1);
                
          } /* end if colon2 */
      } /* end if *line == '-' */
    
    return(1); /* keep going */

} /* end of procedure */

/*  display the page that allows searching of a gopher index 
 */
PRIVATE int 
net_display_index_splash_screen (ActiveEntry * cur_entry)
{
    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;
	char *address_copy = 0;

	StrAllocCopy(address_copy, CE_URL_S->address);
	NET_UnEscape(address_copy);

    PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, XP_GetString(XP_HTML_GOPHER_INDEX), address_copy, address_copy);
    PUTSTRING(CD_OUTPUT_BUF);

    COMPLETE_STREAM;

	PR_Free(address_copy);

    return(0);
}


/* parse CSO server output
*/
PRIVATE int 
net_display_cso_splash_screen (ActiveEntry * cur_entry)
{
    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;
    char *address_copy = 0;

    StrAllocCopy(address_copy, CE_URL_S->address);
    NET_UnEscape(address_copy);

    PR_snprintf(CD_OUTPUT_BUF, CD_OUTPUT_BUFFER_SIZE, XP_GetString(XP_HTML_CSO_SEARCH), address_copy, address_copy);

   	PUTSTRING(CD_OUTPUT_BUF);

    COMPLETE_STREAM;

	PR_Free(address_copy);

    return(0);
}

/* send the request to get the data
 */
PRIVATE int
net_send_gopher_request (ActiveEntry * cur_entry)
{

    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;

    TRACEMSG(("MKGopher: Connected, writing command `%s' to socket %d\n", 
                            CD_COMMAND, CE_SOCK));

    CE_STATUS = (int) NET_BlockingWrite(CE_SOCK, CD_COMMAND, PL_strlen(CD_COMMAND));

    NET_Progress (CE_WINDOW_ID, XP_GetString(XP_PROGRESS_WAITREPLY_GOTHER));

	/* start the graph progress indicator
     */
    FE_GraphProgressInit(CE_WINDOW_ID, CE_URL_S, CE_URL_S->content_length);
    CD_DESTROY_GRAPH_PROGRESS = TRUE;  /* we will need to destroy it */
    CD_ORIGINAL_CONTENT_LENGTH = CE_URL_S->content_length;

    CD_NEXT_STATE = GOPHER_BEGIN_TRANSFER;
    CD_PAUSE_FOR_READ = TRUE;

	if(CE_STATUS < 0)
		CE_STATUS = MK_TCP_WRITE_ERROR;
    
    return(CE_STATUS); /* everything OK */

} /* end GopherLoad */


/* pull down data in binary mode
 */
PRIVATE int
net_pull_gopher_data(ActiveEntry * cur_entry)
{
    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;
    unsigned int write_ready, read_size;

    /* check to see if the stream is ready for writing
     */
    write_ready = (*CD_STREAM->is_write_ready)(CD_STREAM);

    if(write_ready < 1)
      {
        CD_PAUSE_FOR_READ = TRUE;
        return(0);  /* wait until we are ready to write */
      }
    else if(write_ready < (unsigned int) NET_Socket_Buffer_Size)
      {
        read_size = write_ready;
      }
    else
      {
        read_size = NET_Socket_Buffer_Size;
      }

    CE_STATUS = PR_Read(CE_SOCK, NET_Socket_Buffer, read_size);
 
    if(CE_STATUS == 0)
      {  /* all done */
    	CD_NEXT_STATE = GOPHER_DONE;
    	CE_STATUS = MK_DATA_LOADED;
      }
    else if(CE_STATUS > 0)
      {
	    CE_BYTES_RECEIVED += CE_STATUS;
        FE_GraphProgress(CE_WINDOW_ID, 
						 CE_URL_S, 
						 CE_BYTES_RECEIVED, 
						 CE_STATUS, 
						 CE_URL_S->content_length);
    	CE_STATUS = PUTBLOCK(NET_Socket_Buffer, CE_STATUS);
    	CD_PAUSE_FOR_READ = TRUE;
      }
	else
	  {
		/* status less than zero
		 */
		int rv = PR_GetError();
		if(rv == PR_WOULD_BLOCK_ERROR)
		  {
			CD_PAUSE_FOR_READ = TRUE;
			return 0;
		  }
		CE_STATUS = (rv < 0) ? rv : (-rv);
	  }
   
    return(CE_STATUS);

}

/* a list of dis-allowed ports for gopher
 * connections
 */
PRIVATE int bad_ports_table[] = { 
1, 7, 9, 11 , 13, 15, 17, 19, 20,
21, 23, 25, 37, 42, 43, 53, 77, 79, 87, 95, 101, 102,
103, 104, 109, 110, 111, 113, 115, 117, 119,
135, 143, 512, 513, 514, 515, 526, 530, 531, 532, 
540, 556, 601, 6000, 0 };

/* begin the load by initializing data structures and calling Process Gopher
 */
PRIVATE int32
net_GopherLoad (ActiveEntry * cur_entry)
{
    char gopher_type;       /* type */
    char * path;            /* the URL path */
    char * gopher_path;        /* the gopher path */
	char * gopher_host;
    char * query;           /* holds the '?' query string */
    char *ptr;
    GopherConData * connection_data;    /* state data for this connection */

	/* check for illegal gopher port numbers and
	 * return invalid URL for those
	 */
	gopher_host = NET_ParseURL(CE_URL_S->address, GET_HOST_PART);
	if(gopher_host)
	  {
		int port_number;
		int i;
		char *colon = PL_strchr(gopher_host, ':');

		if(colon)
		  {
			colon++;  /* now it points to a port number */
			
			port_number = XP_ATOI(colon);

			/* disallow well known ports */
			for(i=0; bad_ports_table[i]; i++)
				if(port_number == bad_ports_table[i])
			  	  {
        			char *error_msg = NET_ExplainErrorDetails(
														MK_MALFORMED_URL_ERROR, 
														CE_URL_S->address);
					if(error_msg)
						FE_Alert(CE_WINDOW_ID, error_msg);
					PR_Free(gopher_host);
					return(MK_MALFORMED_URL_ERROR);
			  	  }
		  }
		PR_Free(gopher_host);
	  }
		

    /* malloc space for connection data 
     */
    connection_data = PR_NEW(GopherConData);
    if(!connection_data)
	  {
        CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
        return(MK_OUT_OF_MEMORY);
	  }
    memset(connection_data, 0, sizeof(GopherConData));  /* zero it */

    CE_SOCK = NULL;

    cur_entry->con_data = connection_data;

	CD_OUTPUT_BUF = (char*) PR_Malloc(CD_OUTPUT_BUFFER_SIZE);
	if(!CD_OUTPUT_BUF)
	  {
        CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
		return(MK_OUT_OF_MEMORY);
	  }
   
    /* Get entity type, and gopher_path string.
     */        
    path = NET_ParseURL(CE_URL_S->address, GET_PATH_PART);

    TRACEMSG(("GopherLoad: Got path: %s\n",path));

    if(*path && *path != '/') 
      {
        gopher_type = *path;   
        gopher_path = path+1; 
      }
    else if ((*path=='/') && (*(path+1)))   /* past first slash slash */
      {
    	gopher_type = *(path+1);  /* get gopher_type */
        gopher_path = path+2;  /* go past slash and gopher_type */
      }
    else /* no path or just slash */
      {
		/* special case!!!
		 * if the URL looks like gopher://host/?search term
		 * treat it as a text file and append the search term
		 */
		if(PL_strchr(CE_URL_S->address, '?'))
		  {
			gopher_type = '0';
		  }
		else
		  {
			gopher_type = '1';      /* menus are the default */
		  }
		if(*path == '\0')
            gopher_path = path;    
		else
            gopher_path = path+1; 
      }

    CD_GOPHER_TYPE = gopher_type; /* set for later use */

    TRACEMSG(("URL: %s\ngopher_type: %c\nGopher_Path: %s\n", 
                    CE_URL_S->address, gopher_type, gopher_path));

    /* We now have the gopher type so we can safely set up
     * the Stream stack since we know what type will be
     * returned ahead of time
     */
    switch(gopher_type) {
        case GTYPE_HTML:    /* all HTML types */
        case GTYPE_HTMLCAPS:
        case GTYPE_MENU:
        case GTYPE_CSO:
        case GTYPE_INDEX:
            StrAllocCopy(CE_URL_S->content_type, TEXT_HTML);
            break;
        case GTYPE_TEXT:
            StrAllocCopy(CE_URL_S->content_type, TEXT_PLAIN);
            break;
        case GTYPE_MACBINHEX:
            StrAllocCopy(CE_URL_S->content_type, APPLICATION_BINHEX);
            break;
        case GTYPE_PCBINARY:
        case GTYPE_BINARY:
        case GTYPE_MIME:
            StrAllocCopy(CE_URL_S->content_type, APPLICATION_OCTET_STREAM);
            break;
        case GTYPE_UUENCODED:
            StrAllocCopy(CE_URL_S->content_type, APPLICATION_UUENCODE);
            break;
        case GTYPE_TELNET:
        case GTYPE_TN3270:
#ifdef MOZILLA_CLIENT
            /* do the telnet and return */
            return(NET_RemoteHostLoad(cur_entry));
#else
			PR_ASSERT(0);
			break;
#endif /* MOZILLA_CLIENT */
        case GTYPE_GIF:
        case GTYPE_IMAGE:
            StrAllocCopy(CE_URL_S->content_type, IMAGE_GIF);
            break;
        case GTYPE_SOUND:
            StrAllocCopy(CE_URL_S->content_type, AUDIO_BASIC);
            break;
        case GTYPE_MPEG:
            StrAllocCopy(CE_URL_S->content_type, VIDEO_MPEG);
            break;
        default:
            StrAllocCopy(CE_URL_S->content_type, TEXT_PLAIN);
            break;
    }

    CD_STREAM = NET_StreamBuilder(cur_entry->format_out, CE_URL_S, CE_WINDOW_ID);

    if (!CD_STREAM) 
      {
        CE_URL_S->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
        return(MK_UNABLE_TO_CONVERT);
      }

    TRACEMSG(("MKGopher: Stream now set up \n"));

    /* now do all the special handling of each type that
     * doesn't involve just a direct get 
     */
    switch(gopher_type) {
        case GTYPE_INDEX:
            /* Search is allowed */
            query = PL_strchr(CE_URL_S->address, '?');  /* Look for search string */

            if (!query || !query[1]) {      /* No search defined */
                /* Display "cover page" */
                net_display_index_splash_screen(cur_entry);
                CD_NEXT_STATE = GOPHER_DONE;
                return -1;      /* Local function only */
            }

            *query++ = 0;     /* Go past '?' */
          
            StrAllocCopy(CD_COMMAND, NET_UnEscape(gopher_path));

            StrAllocCat(CD_COMMAND, "\t");
      
            /* Remove plus signs */
            for (ptr=query; *ptr; ptr++) 
                if (*ptr == '+') 
				    *ptr = ' ';

            StrAllocCat(CD_COMMAND, NET_UnEscape(query));

			*(query-1) = '?';  /* set it back to the way it was */
            break;

        case GTYPE_CSO:
            /* Search is allowed */
            query = PL_strchr(CE_URL_S->address, '?');      /* Look for search string */

            if (!query || !query[1])           /* No search required */
			  {
                /* Display "cover page" */
                net_display_cso_splash_screen(cur_entry);     
                CE_STATUS = MK_DATA_LOADED;
                return -1;      /* Local function only */
              }
            *query++ = 0;       /* Go past '?' */

            StrAllocCopy(CD_COMMAND, "query ");

            /* Remove plus signs */
            for (ptr=query; *ptr; ptr++) 
                if (*ptr == '+') 
				    *ptr = ' ';

            StrAllocCat(CD_COMMAND, (char *)NET_UnEscape(query));

			*(query-1) = '?';  /* set it back to the way it was */
           
			break;

	    case GTYPE_TEXT:
		   /* Look for search string */
		    query = PL_strchr(CE_URL_S->address, '?');     

			/* special case!!!
			 * if a query exist treat it special, send 
			 * the query instead
			 */
			if(query)
			  {
				*query++ = 0;       /* Go past '?' */

				/* Remove plus signs */
				for (ptr=query; *ptr; ptr++) 
				  if (*ptr == '+') 
				    *ptr = ' ';

				StrAllocCopy(CD_COMMAND, (char *)NET_UnEscape(query));
				
				*(query-1) = '?';  /* set it back to the way it was */
			  }	
			else if(*path != '\0')
			  {
				StrAllocCopy(CD_COMMAND, (char*)NET_UnEscape(gopher_path));
			  }
			else
			  {
				StrAllocCopy(CD_COMMAND, "/");
			  }
			break;

        default:   /* all other types besides index and CSO */
            if(*path == '\0')
                StrAllocCopy(CD_COMMAND, "/");
            else
                StrAllocCopy(CD_COMMAND, (char*)NET_UnEscape(gopher_path));
    }

    PR_FREEIF(path);  /* NET_Parse malloc'd the path string */
    path = NULL;

    /* protect against other protocol attacks by limiting
     * the command to a single line.  Terminate the command
     * at any \n or \r
     */
    if(PL_strchr(CD_COMMAND, '\n') || PL_strchr(CD_COMMAND, '\r'))
      {
   		char *error_msg = NET_ExplainErrorDetails(MK_MALFORMED_URL_ERROR, 
												  CE_URL_S->address);
		if(error_msg)
			FE_Alert(CE_WINDOW_ID, error_msg);
        return(MK_MALFORMED_URL_ERROR);
      }
    
    StrAllocCat(CD_COMMAND, CRLF); /* finish off the command */

    CD_NEXT_STATE = GOPHER_BEGIN_CONNECT;

    return(net_ProcessGopher(cur_entry));
}

/* NET_ProcessGopher
 *    completes the data transfer; is called from NET_ProcessNet()
 *
 * returns negative when complete
 */
PRIVATE int32
net_ProcessGopher(ActiveEntry * cur_entry)
{
    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;

    TRACEMSG(("Entered ProcessGopher: gopher_type=%c\n",CD_GOPHER_TYPE));

    CD_PAUSE_FOR_READ = FALSE;

    while(!CD_PAUSE_FOR_READ)
      {
        TRACEMSG(("ProcessGopher: in switch with state: #%d\n",CD_NEXT_STATE));

        switch(CD_NEXT_STATE) {

    	case GOPHER_BEGIN_CONNECT:
            /*  Set up a socket to the server for the data:
            */
            TRACEMSG(("MKGopher: Setting up net connection\n"));

            CE_STATUS = NET_BeginConnect(CE_URL_S->address, 
										 CE_URL_S->IPAddressString,
										 "Gopher", 
										 70, 
										 &CE_SOCK, 
										 HG10300 
										 &CD_TCP_CON_DATA, 
										 CE_WINDOW_ID,
										 &CE_URL_S->error_msg,
										 cur_entry->socks_host,
										 cur_entry->socks_port,
                                         cur_entry->URL_s->localIP);

			if(CE_SOCK != NULL)
				NET_TotalNumberOfOpenConnections++;

        	if(CE_STATUS == MK_CONNECTED)
          	  {
        		CD_NEXT_STATE = GOPHER_SEND_REQUEST;
		    	NET_SetReadSelect(CE_WINDOW_ID, CE_SOCK);
              }
            else if(CE_STATUS > -1)
              {
        	    CD_NEXT_STATE = GOPHER_FINISH_CONNECT;
        	    CD_PAUSE_FOR_READ = TRUE;
        	    CE_CON_SOCK = CE_SOCK; /* set so we select on it */
                NET_SetConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
              }
            break;

        case GOPHER_FINISH_CONNECT:
            CE_STATUS = NET_FinishConnect(CE_URL_S->address,
										  "Gopher",
										  70, 
										  &CE_SOCK, 
										  &CD_TCP_CON_DATA, 
										  CE_WINDOW_ID,
										  &CE_URL_S->error_msg,
                                          cur_entry->URL_s->localIP);
            if(CE_STATUS == MK_CONNECTED)
              {
        	    CD_NEXT_STATE = GOPHER_SEND_REQUEST;
				NET_ClearConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
        	    CE_CON_SOCK = NULL;  /* reset so we don't select on it */
				NET_SetReadSelect(CE_WINDOW_ID, CE_SOCK);
              }
            else
              {

                /* unregister the old CE_SOCK from the select list
                 * and register the new value in the case that it changes
                 */
                if(CE_CON_SOCK != CE_SOCK)
                  {
                    NET_ClearConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                    CE_CON_SOCK = CE_SOCK;
                    NET_SetConnectSelect(CE_WINDOW_ID, CE_CON_SOCK);
                  }

        	    CD_PAUSE_FOR_READ = TRUE;
              }
            break;

            case GOPHER_SEND_REQUEST:
            CE_STATUS = net_send_gopher_request(cur_entry);
            break;
       
        case GOPHER_BEGIN_TRANSFER:
            if(CD_GOPHER_TYPE == GTYPE_MENU || CD_GOPHER_TYPE == GTYPE_INDEX)
                CE_STATUS = net_begin_gopher_menu(connection_data);
            else if(CD_GOPHER_TYPE == GTYPE_CSO)
                CE_STATUS = net_begin_gopher_cso(connection_data);
            else
                CD_NEXT_STATE = GOPHER_TRANSFER_BINARY;
            break;

        case GOPHER_TRANSFER_MENU:
            CE_STATUS = net_parse_menu(cur_entry);
            break;

        case GOPHER_TRANSFER_CSO:
            CE_STATUS = net_parse_cso(cur_entry);
            break;
    
        case GOPHER_TRANSFER_BINARY:
            CE_STATUS = net_pull_gopher_data(cur_entry);
            break;
    
        case GOPHER_DONE:
			NET_ClearReadSelect(CE_WINDOW_ID, CE_SOCK);
            PR_Close(CE_SOCK);
			NET_TotalNumberOfOpenConnections--;
            COMPLETE_STREAM;
            CD_NEXT_STATE = GOPHER_FREE;
            break;

        case GOPHER_ERROR_DONE:
            if(CE_SOCK != NULL)
			  {
				NET_ClearReadSelect(CE_WINDOW_ID, CE_SOCK);
				NET_ClearConnectSelect(CE_WINDOW_ID, CE_SOCK);
                NET_ClearDNSSelect(CE_WINDOW_ID, CE_SOCK);
                PR_Close(CE_SOCK);
				NET_TotalNumberOfOpenConnections--;
			  }
            if(CD_STREAM)
                ABORT_STREAM(CE_STATUS);
            CD_NEXT_STATE = GOPHER_FREE;
            break;
    
        case GOPHER_FREE:
            if(CD_DESTROY_GRAPH_PROGRESS)
                FE_GraphProgressDestroy(CE_WINDOW_ID,
                                        CE_URL_S,
                                        CD_ORIGINAL_CONTENT_LENGTH,
										CE_BYTES_RECEIVED);
            PR_FREEIF(CD_COMMAND);
            PR_FREEIF(CD_STREAM);  	/* don't forget the stream */
			PR_FREEIF(CD_OUTPUT_BUF);
            PR_FREEIF(CD_DATA_BUF);
            if(CD_TCP_CON_DATA)
                NET_FreeTCPConData(CD_TCP_CON_DATA);

            PR_Free(connection_data);
            return(-1);  /* all done */

    } /* end switch(NEXT_STATE) */


    if(CE_STATUS < 0 && CD_NEXT_STATE != GOPHER_DONE && 
                CD_NEXT_STATE != GOPHER_ERROR_DONE && 
                CD_NEXT_STATE != GOPHER_FREE)
      {
        CD_NEXT_STATE = GOPHER_ERROR_DONE;
        CD_PAUSE_FOR_READ = FALSE; 
      }
    
    } /* end while */

    return(CE_STATUS);
}

/* abort the connection in progress
 */
PRIVATE int32
net_InterruptGopher(ActiveEntry * cur_entry)
{
    GopherConData * connection_data = (GopherConData *)cur_entry->con_data;

    CD_NEXT_STATE = GOPHER_ERROR_DONE;
    CE_STATUS = MK_INTERRUPTED;
 
    return(net_ProcessGopher(cur_entry));
}

/* Free any memory 
 */
PRIVATE void
net_CleanupGopher(void)
{
    /* nothing to free */
    return;
}

MODULE_PRIVATE void
NET_InitGopherProtocol(void)
{
    static NET_ProtoImpl gopher_proto_impl;

    gopher_proto_impl.init = net_GopherLoad;
    gopher_proto_impl.process = net_ProcessGopher;
    gopher_proto_impl.interrupt = net_InterruptGopher;
    gopher_proto_impl.resume = NULL;
    gopher_proto_impl.cleanup = net_CleanupGopher;

    NET_RegisterProtocolImplementation(&gopher_proto_impl, GOPHER_TYPE_URL);
}
