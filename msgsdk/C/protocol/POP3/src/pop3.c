/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/
  
/* 
 * Copyright (c) 1997 and 1998 Netscape Communications Corporation
 * (http://home.netscape.com/misc/trademarks.html) 
 */

/*
 * pop3.c
 * Implementation file for POP3 functionality.
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include "pop3.h"
#include "pop3priv.h"
#include "nsio.h"

#include <malloc.h>
#include <ctype.h>
#include <string.h>

/* Initializes and allocates the pop3Client_t structure and sets the sink. */
int pop3_initialize( pop3Client_t ** out_ppPOP3, pop3Sink_t * in_pPOP3Sink )
{
    pop3Client_i_t * l_pPOP3;

    /* Parameter validation. */
    if ( out_ppPOP3 == NULL || (*out_ppPOP3) != NULL || in_pPOP3Sink == NULL )
    {
		return NSMAIL_ERR_INVALIDPARAM;
    }

    /* Create the client */
    l_pPOP3 = (pop3Client_i_t*)malloc( sizeof(pop3Client_i_t) );
    
    if ( l_pPOP3 == NULL )
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}

    /* Set the data members of the client. */
    l_pPOP3->timeout = -1;
    l_pPOP3->chunkSize = 1024;
    l_pPOP3->pop3Sink = in_pPOP3Sink;
    l_pPOP3->mustProcess = FALSE;
    l_pPOP3->pCommandList = NULL;
    l_pPOP3->messageDataSize = 0;
    l_pPOP3->messageNumber = 0;
    l_pPOP3->multiLineState = FALSE;

	memset(&(l_pPOP3->io), 0, sizeof( IO_t ) );
    memset(l_pPOP3->commandBuffer, 0, MAX_COMMANDLINE_LENGTH * sizeof( char ) );
	memset(l_pPOP3->responseLine, 0, MAX_TEXTLINE_LENGTH * sizeof( char ) );

    l_pPOP3->messageData = (char*)malloc( l_pPOP3->chunkSize * sizeof(char) );

    if ( l_pPOP3->messageData == NULL )
    {
        return NSMAIL_ERR_OUTOFMEMORY;
    }

    *out_ppPOP3 = l_pPOP3;

    /*
	 * Initialize the IO structure.
	 */

    return IO_initialize( (&l_pPOP3->io), l_pPOP3->chunkSize, MAX_BUFFER_LENGTH );
}

/* Function used to free the pop3Client_t structure and it's data members. */
void pop3_free( pop3Client_t ** in_ppPOP3 )
{
	if ( in_ppPOP3 == NULL || *in_ppPOP3 == NULL )
	{
		return;
	}

    /* Free the members of the IO structure. */
    IO_free( &((*in_ppPOP3)->io) );

    /* Remove all the elements from the linked list. */
    RemoveAllElements( &((*in_ppPOP3)->pCommandList) );

    /* Free the message data buffer. */
    if ( (*in_ppPOP3)->messageData != NULL )
    {
        free( (*in_ppPOP3)->messageData );
    }

    /* Free the client. */
    free( (*in_ppPOP3) );

    *in_ppPOP3 = NULL;
}

int pop3_connect( pop3Client_t * in_pPOP3, const char * in_server, unsigned short  in_port )
{
	int l_nReturnCode;

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Reset data members. */
    in_pPOP3->messageDataSize = 0;
    in_pPOP3->multiLineState = FALSE;
    RemoveAllElements( &(in_pPOP3->pCommandList) );

	/* Connect to the server. */
    if ( in_port == 0 )
    {
        in_port = DEFAULT_POP3_PORT;
    }

	/* Connect to the server. */
    l_nReturnCode = IO_connect( &(in_pPOP3->io), in_server, in_port );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), (void*)&POP3Response_CONN );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_delete( pop3Client_t * in_pPOP3, int in_messageNumber )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), dele, strlen(dele) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_messageNumber );

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_pPOP3->commandBuffer, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
								(void*)&POP3Response_DELE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_disconnect( pop3Client_t * in_pPOP3 )
{
    /* Disconnect from the server. */
    return IO_disconnect( &(in_pPOP3->io) );
}

int pop3_list( pop3Client_t * in_pPOP3 )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), list, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
								(void*)&POP3Response_LIST );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_listA( pop3Client_t * in_pPOP3, int in_messageNumber )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), list, strlen(list) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_messageNumber );

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_pPOP3->commandBuffer, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
								(void*)&POP3Response_LISTA );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_noop( pop3Client_t * in_pPOP3 )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), noop, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
								(void*)&POP3Response_NOOP );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_pass( pop3Client_t * in_pPOP3, const char * in_password )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), pass, strlen(pass) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_password, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_PASS );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_quit( pop3Client_t * in_pPOP3 )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), quit, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_QUIT );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_reset( pop3Client_t * in_pPOP3 )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), rset, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_RSET );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_retrieve( pop3Client_t * in_pPOP3, int in_messageNumber )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), retr, strlen(retr) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_messageNumber );

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_pPOP3->commandBuffer, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_RETR );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->messageNumber = in_messageNumber;
    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_sendCommand( pop3Client_t * in_pPOP3, const char * in_command, boolean in_multiLine )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), in_command, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    if ( in_multiLine )
    {
	    l_nReturnCode = 
			AddElement( &(in_pPOP3->pCommandList), 
                        (void*)&POP3Response_SENDCOMMAND );
    }
    else
    {
	    l_nReturnCode = 
			AddElement( &(in_pPOP3->pCommandList), 
                        (void*)&POP3Response_SENDCOMMANDA );
    }

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_stat( pop3Client_t * in_pPOP3 )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), stat, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                    (void*)&POP3Response_STAT );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_top( pop3Client_t * in_pPOP3, int in_messageNumber, int in_lines )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), top, strlen(top) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_messageNumber );

    l_nReturnCode = IO_write( &(in_pPOP3->io), 
                                in_pPOP3->commandBuffer, 
                                strlen(in_pPOP3->commandBuffer) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_write( &(in_pPOP3->io), space, strlen(space) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_lines );

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_pPOP3->commandBuffer, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_TOP );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->messageNumber = in_messageNumber;
    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_uidList( pop3Client_t * in_pPOP3 )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), uidl, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_UIDL );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_uidListA( pop3Client_t * in_pPOP3, int in_messageNumber )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), uidl, strlen(uidl) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_messageNumber );

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_pPOP3->commandBuffer, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_UIDLA );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_user( pop3Client_t * in_pPOP3, const char * in_user )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL || in_user == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), user, strlen(user) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_user, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_USER );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_xAuthList( pop3Client_t * in_pPOP3 )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_send( &(in_pPOP3->io), xauthlist, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_XAUTHLIST );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_xAuthListA( pop3Client_t * in_pPOP3, int in_messageNumber )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), xauthlist, strlen(xauthlist) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_messageNumber );

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_pPOP3->commandBuffer, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_XAUTHLISTA );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}

int pop3_xSender( pop3Client_t * in_pPOP3, int in_messageNumber )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pPOP3->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Send the command. */
    l_nReturnCode = IO_write( &(in_pPOP3->io), xsender, strlen(xsender) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    sprintf( in_pPOP3->commandBuffer, "%d", in_messageNumber );

    l_nReturnCode = IO_send( &(in_pPOP3->io), in_pPOP3->commandBuffer, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pPOP3->pCommandList), 
                                (void*)&POP3Response_XSENDER );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pPOP3->mustProcess = TRUE;
    return NSMAIL_OK;
}


int pop3_processResponses( pop3Client_t * in_pPOP3 )
{
	pop3_ResponseType_t * l_pCurrentResponse;
    LinkedList_t * l_pCurrentLink;
    int l_nReturnCode;

	/* Check for errors. */
    if ( in_pPOP3 == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    /* Make sure that all buffered commands are sent. */
    l_nReturnCode = IO_flush( &(in_pPOP3->io) );
    
    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}
    
    /* Continue to process responses while the list is not empty. */
    /* or a timeout occurs. */
    while ( in_pPOP3->pCommandList != NULL )
    {
		l_pCurrentResponse = (pop3_ResponseType_t *)in_pPOP3->pCommandList->pData;

		switch ( (*l_pCurrentResponse) )
		{
			case CONN:
                l_nReturnCode = parseSingleLine( in_pPOP3, in_pPOP3->pop3Sink->connect );
                if ( l_nReturnCode != NSMAIL_OK )
                {
                    IO_disconnect( &(in_pPOP3->io) );
                }
				break;
			case DELE:
                l_nReturnCode = parseSingleLine( in_pPOP3, in_pPOP3->pop3Sink->dele );
				break;
			case LISTA:
	            l_nReturnCode = parseListA( in_pPOP3 );
				break;
			case LIST:
	            l_nReturnCode = parseList( in_pPOP3 );
				break;
			case NOOP:
	            l_nReturnCode = parseNoop( in_pPOP3 );
				break;
			case PASS:
                l_nReturnCode = parseSingleLine( in_pPOP3, in_pPOP3->pop3Sink->pass );
				break;
			case QUIT:
                l_nReturnCode = parseSingleLine( in_pPOP3, in_pPOP3->pop3Sink->quit );

                if ( l_nReturnCode != NSMAIL_OK )
                {
                    return l_nReturnCode;
                }
                l_nReturnCode = IO_disconnect( &(in_pPOP3->io) );
				break;
			case RSET:
                l_nReturnCode = parseSingleLine( in_pPOP3, in_pPOP3->pop3Sink->reset );
				break;
			case RETR:
	            l_nReturnCode = parseRetr( in_pPOP3 );
				break;
			case SENDCOMMANDA:
	            l_nReturnCode = parseSendCommandA( in_pPOP3 );
				break;
			case SENDCOMMAND:
	            l_nReturnCode = parseSendCommand( in_pPOP3 );
				break;
			case STAT:
	            l_nReturnCode = parseStat( in_pPOP3 );
				break;
			case TOP:
	            l_nReturnCode = parseTop( in_pPOP3 );
				break;
			case UIDLA:
	            l_nReturnCode = parseUidlA( in_pPOP3 );
				break;
			case UIDL:
	            l_nReturnCode = parseUidl( in_pPOP3 );
				break;
			case USER:
                l_nReturnCode = parseSingleLine( in_pPOP3, in_pPOP3->pop3Sink->user );
				break;
			case XAUTHLISTA:
	            l_nReturnCode = parseXAuthListA( in_pPOP3 );
				break;
			case XAUTHLIST:
	            l_nReturnCode = parseXAuthList( in_pPOP3 );
				break;
			case XSENDER:
	            l_nReturnCode = parseXSender( in_pPOP3 );
				break;
			default:
				l_nReturnCode = NSMAIL_ERR_UNEXPECTED;
				break;
		}

        /* An timeout or an error has occured. */
        if ( l_nReturnCode != NSMAIL_OK )
        {
            if ( l_nReturnCode != NSMAIL_ERR_TIMEOUT )
            {
                in_pPOP3->mustProcess = FALSE;
            }
            return l_nReturnCode;
        }

        /* Move to the next element in the pending response list. */
        l_pCurrentLink = in_pPOP3->pCommandList;
		in_pPOP3->pCommandList = in_pPOP3->pCommandList->next;

        /* Free the current link in the pending response list. */
        free ( l_pCurrentLink );
	}
	
    in_pPOP3->mustProcess = FALSE;
    return NSMAIL_OK;
}

int pop3_setChunkSize( pop3Client_t * in_pPOP3, int in_chunkSize )
{
    if ( in_pPOP3 == NULL || in_chunkSize <= 0 )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    in_pPOP3->chunkSize = in_chunkSize;

    free ( in_pPOP3->messageData );

    in_pPOP3->messageData = (char*)malloc( in_chunkSize * sizeof(char) );

    if ( in_pPOP3->messageData == NULL )
    {
        return NSMAIL_ERR_OUTOFMEMORY;
    }

    return NSMAIL_OK;
}

int pop3_setResponseSink( pop3Client_t * in_pPOP3, pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3 == NULL || in_pPOP3Sink == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    in_pPOP3->pop3Sink = in_pPOP3Sink;

    return NSMAIL_OK;
}

int pop3_setTimeout( pop3Client_t * in_pPOP3, double in_timeout )
{
    if ( in_pPOP3 == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    return IO_setTimeout( &(in_pPOP3->io), in_timeout );
}

int pop3_set_option( pop3Client_t * in_pPOP3, int in_option, void * in_pOptionData )
{
    if ( in_pPOP3 == NULL || in_pOptionData == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    switch ( in_option )
    {
        case NSMAIL_OPT_IO_FN_PTRS:
            IO_setFunctions( &(in_pPOP3->io), (nsmail_io_fns_t *)in_pOptionData );
            return NSMAIL_OK;
            break;
        default:
            return NSMAIL_ERR_UNEXPECTED;
            break;
    }
}

int pop3_get_option( pop3Client_t * in_pPOP3, int in_option, void * in_pOptionData )
{
    if ( in_pPOP3 == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    switch ( in_option )
    {
        case NSMAIL_OPT_IO_FN_PTRS:
            IO_getFunctions( &(in_pPOP3->io), (nsmail_io_fns_t *)in_pOptionData );
            return NSMAIL_OK;
            break;
        default:
            return NSMAIL_ERR_UNEXPECTED;
            break;
    }
}

static int setStatusInfo( const char * in_responseLine, boolean * out_status )
{
    if ( in_responseLine == NULL || out_status == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( strlen(in_responseLine) < 3 )
    {
        return NSMAIL_ERR_PARSE;
    }

    if ( strncmp( ok, in_responseLine, 3 ) == 0 )
    {
        *out_status = TRUE;
    }
    else if ( strncmp( err, in_responseLine, 4 ) == 0 )
    {
        *out_status = FALSE;
    }
    else
    {
        return NSMAIL_ERR_PARSE;
    }

    return NSMAIL_OK;
}

static int parseSingleLine( pop3Client_t * in_pPOP3, sinkMethod_t in_callback )
{
    /* Server status */
    boolean l_status;

    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        if ( in_callback != NULL )
        {
            in_callback( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[4]) );
        }
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static int parseListA( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_ptr;
    int l_messageNum;
    int l_octetCount;
    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        l_messageNum = atoi( &(in_pPOP3->responseLine[4]) );

        if ( l_messageNum == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_ptr = strchr( &(in_pPOP3->responseLine[4]), ' ' );

        if ( l_ptr == NULL )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_octetCount = atoi( &(l_ptr[1]) );

        if ( l_octetCount == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        pop3Sink_listStart( in_pPOP3->pop3Sink );
        pop3Sink_list( in_pPOP3->pop3Sink, l_messageNum, l_octetCount );
        pop3Sink_listComplete( in_pPOP3->pop3Sink );
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static int parseList( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_ptr;
    int l_messageNum;
    int l_octetCount;
    int l_nReturnCode;

    if ( in_pPOP3->multiLineState == FALSE )
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the status and the message. */
        l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Invoke the callback. */
        if ( l_status )
        {
            pop3Sink_listStart( in_pPOP3->pop3Sink );
        }
        else
        {
            pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
            return NSMAIL_OK;
        }

        in_pPOP3->multiLineState = TRUE;
    }

    /* Read the next line of data from the socket. */
    l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    while ( '.' != (in_pPOP3->responseLine)[0] || strlen(in_pPOP3->responseLine) != 1 )
    {
        l_messageNum = atoi( in_pPOP3->responseLine );

        if ( l_messageNum == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_ptr = strchr( in_pPOP3->responseLine, ' ' );

        if ( l_ptr == NULL )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_octetCount = atoi( &(l_ptr[1]) );

        if ( l_octetCount == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        /* Invoke the callback. */
        pop3Sink_list( in_pPOP3->pop3Sink, l_messageNum, l_octetCount );

        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }
    }

    in_pPOP3->multiLineState = FALSE;
    /* Invoke the callback. */
    pop3Sink_listComplete( in_pPOP3->pop3Sink );

    return NSMAIL_OK;
}

static int parseNoop( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;

    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        pop3Sink_noop( in_pPOP3->pop3Sink );
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static int parseRetr( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_responseLine = NULL;
    unsigned int l_byteCount;
    unsigned int l_octetCount;
    unsigned int l_messageLength;
    int l_nReturnCode;

    if ( in_pPOP3->multiLineState == FALSE )
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the status and the message. */
        l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Invoke the callback. */
        if ( l_status )
        {
            l_octetCount = atoi( &(in_pPOP3->responseLine[4]) );

            if ( l_octetCount == 0 )
            {
		/* The server returned just +OK instead of +OK <octet-count> octets.
		   So we will also return 0 for octetcount :-( PY 7/29/98
                return NSMAIL_ERR_PARSE;
		*/
            }

            pop3Sink_retrieveStart( in_pPOP3->pop3Sink, in_pPOP3->messageNumber, l_octetCount );
        }
        else
        {
            pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
            return NSMAIL_OK;
        }

        in_pPOP3->multiLineState = TRUE;
    }

    /* Read the next line of data from the socket. */
    l_nReturnCode = IO_readDLine( &(in_pPOP3->io), &l_responseLine );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    l_messageLength = strlen( l_responseLine );
/*
    in_pPOP3->responseLine[l_messageLength++] = '\r';
    in_pPOP3->responseLine[l_messageLength++] = '\n';
    in_pPOP3->responseLine[l_messageLength] = '\0';
*/

    while ( ('.' == l_responseLine[0] && l_messageLength == 3) == FALSE )
    {
        l_byteCount = 0;

        if ( l_responseLine[0] == '.' )
        {
            l_byteCount++;
        }

        while ( l_byteCount < strlen(l_responseLine) )
        {
            if ( in_pPOP3->messageDataSize == in_pPOP3->chunkSize - 1 )
            {
                (in_pPOP3->messageData)[in_pPOP3->messageDataSize] = '\0';
                /* Invoke the callback. */
                pop3Sink_retrieve( in_pPOP3->pop3Sink, in_pPOP3->messageData );
                in_pPOP3->messageDataSize = 0;
            }

             (in_pPOP3->messageData)[in_pPOP3->messageDataSize++] = 
                                        l_responseLine[l_byteCount++];
        }

        /* Free the line here */
        l_nReturnCode = IO_freeDLine( &l_responseLine );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        l_responseLine = NULL;

        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readDLine( &(in_pPOP3->io), &l_responseLine );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        l_messageLength = strlen( l_responseLine );
/*
        in_pPOP3->responseLine[l_messageLength++] = '\r';
        in_pPOP3->responseLine[l_messageLength++] = '\n';
        in_pPOP3->responseLine[l_messageLength] = '\0';
*/
    }

    /* Free the line here */
    l_nReturnCode = IO_freeDLine( &l_responseLine );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    l_responseLine = NULL;

    if ( in_pPOP3->messageDataSize > 0 )
    {
        (in_pPOP3->messageData)[in_pPOP3->messageDataSize] = '\0';
        /* Invoke the callback. */
        pop3Sink_retrieve( in_pPOP3->pop3Sink, in_pPOP3->messageData );
        in_pPOP3->messageDataSize = 0;
    }

    in_pPOP3->multiLineState = FALSE;
    /* Invoke the callback. */
    pop3Sink_retrieveComplete( in_pPOP3->pop3Sink );

    return NSMAIL_OK;
}

static int parseSendCommandA( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;

    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        pop3Sink_sendCommandStart( in_pPOP3->pop3Sink );
        pop3Sink_sendCommand( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[4]) );
        pop3Sink_sendCommandComplete( in_pPOP3->pop3Sink );
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static int parseSendCommand( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    int l_nReturnCode;

    if ( in_pPOP3->multiLineState == FALSE )
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the status and the message. */
        l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Invoke the callback. */
        if ( l_status )
        {
            pop3Sink_sendCommandStart( in_pPOP3->pop3Sink );
        }
        else
        {
            pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
            return NSMAIL_OK;
        }

        in_pPOP3->multiLineState = TRUE;
    }

    /* Read the next line of data from the socket. */
    l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    while ( '.' != (in_pPOP3->responseLine)[0] || strlen(in_pPOP3->responseLine) != 1 )
    {
        /* Invoke the callback. */
        pop3Sink_sendCommand( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[4]) );

        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }
    }

    in_pPOP3->multiLineState = FALSE;
    /* Invoke the callback. */
    pop3Sink_sendCommandComplete( in_pPOP3->pop3Sink );

    return NSMAIL_OK;
}

static int parseStat( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_ptr;
    int l_messageNum;
    int l_octetCount;
    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        l_messageNum = atoi( &(in_pPOP3->responseLine[4]) );

        if ( ( l_messageNum == 0 ) && ( in_pPOP3->responseLine[4] != '0' ) )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_ptr = strchr( &(in_pPOP3->responseLine[4]), ' ' );

        if ( l_ptr == NULL )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_octetCount = atoi( &(l_ptr[1]) );

        if ( ( l_octetCount == 0 ) && ( l_ptr[1] != '0' ) )
        {
            return NSMAIL_ERR_PARSE;
        }

        pop3Sink_stat( in_pPOP3->pop3Sink, l_messageNum, l_octetCount );
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static int parseTop( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    int l_nReturnCode;

    if ( in_pPOP3->multiLineState == FALSE )
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the status and the message. */
        l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Invoke the callback. */
        if ( l_status )
        {
            pop3Sink_topStart( in_pPOP3->pop3Sink, in_pPOP3->messageNumber );
        }
        else
        {
            pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
            return NSMAIL_OK;
        }

        in_pPOP3->multiLineState = TRUE;
    }

    /* Read the next line of data from the socket. */
    l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    while ( '.' != (in_pPOP3->responseLine)[0] || strlen(in_pPOP3->responseLine) != 1 )
    {
        /* Invoke the callback. */
        pop3Sink_top( in_pPOP3->pop3Sink, in_pPOP3->responseLine );

        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }
    }

    in_pPOP3->multiLineState = FALSE;
    /* Invoke the callback. */
    pop3Sink_topComplete( in_pPOP3->pop3Sink );

    return NSMAIL_OK;
}

static int parseUidlA( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_ptr;
    int l_messageNum;
    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        l_messageNum = atoi( &(in_pPOP3->responseLine[4]) );

        if ( l_messageNum == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_ptr = strchr( &(in_pPOP3->responseLine[4]), ' ' );

        if ( l_ptr == NULL )
        {
            return NSMAIL_ERR_PARSE;
        }

        pop3Sink_uidListStart( in_pPOP3->pop3Sink );
        pop3Sink_uidList( in_pPOP3->pop3Sink, l_messageNum, &(l_ptr[1]) );
        pop3Sink_uidListComplete( in_pPOP3->pop3Sink );
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static int parseUidl( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_ptr;
    int l_messageNum;
    int l_nReturnCode;

    if ( in_pPOP3->multiLineState == FALSE )
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the status and the message. */
        l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Invoke the callback. */
        if ( l_status )
        {
            pop3Sink_uidListStart( in_pPOP3->pop3Sink );
        }
        else
        {
            pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
            return NSMAIL_OK;
        }

        in_pPOP3->multiLineState = TRUE;
    }

    /* Read the next line of data from the socket. */
    l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    while ( '.' != (in_pPOP3->responseLine)[0] || strlen(in_pPOP3->responseLine) != 1 )
    {
        l_messageNum = atoi( in_pPOP3->responseLine );

        if ( l_messageNum == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_ptr = strchr( in_pPOP3->responseLine, ' ' );

        if ( l_ptr == NULL )
        {
            return NSMAIL_ERR_PARSE;
        }

        /* Invoke the callback. */
        pop3Sink_uidList( in_pPOP3->pop3Sink, l_messageNum, &(l_ptr[1]) );

        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }
    }

    in_pPOP3->multiLineState = FALSE;
    /* Invoke the callback. */
    pop3Sink_uidListComplete( in_pPOP3->pop3Sink );

    return NSMAIL_OK;
}

static int parseXAuthListA( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_ptr;
    char * l_authName = NULL;
    int l_messageNum;
    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        l_messageNum = atoi( &(in_pPOP3->responseLine[4]) );

        if ( l_messageNum == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_ptr = strchr( &(in_pPOP3->responseLine[4]), ' ' );

        if ( l_ptr != NULL )
        {
            l_authName = &(l_ptr[1]);
        }

        pop3Sink_xAuthListStart( in_pPOP3->pop3Sink );
        pop3Sink_xAuthList( in_pPOP3->pop3Sink, l_messageNum, l_authName );
        pop3Sink_xAuthListComplete( in_pPOP3->pop3Sink );
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static int parseXAuthList( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;
    char * l_ptr;
    char * l_authName;
    int l_messageNum;
    int l_nReturnCode;

    if ( in_pPOP3->multiLineState == FALSE )
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the status and the message. */
        l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Invoke the callback. */
        if ( l_status )
        {
            pop3Sink_xAuthListStart( in_pPOP3->pop3Sink );
        }
        else
        {
            pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
            return NSMAIL_OK;
        }

        in_pPOP3->multiLineState = TRUE;
    }

    /* Read the next line of data from the socket. */
    l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    while ( '.' != (in_pPOP3->responseLine)[0] || strlen(in_pPOP3->responseLine) != 1 )
    {
        l_messageNum = atoi( in_pPOP3->responseLine );

        if ( l_messageNum == 0 )
        {
            return NSMAIL_ERR_PARSE;
        }

        l_ptr = strchr( in_pPOP3->responseLine, ' ' );

        if ( l_ptr != NULL )
        {
            l_authName = &(l_ptr[1]);
        }

        /* Invoke the callback. */
        pop3Sink_xAuthList( in_pPOP3->pop3Sink, l_messageNum, l_authName );

        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }
    }

    in_pPOP3->multiLineState = FALSE;
    /* Invoke the callback. */
    pop3Sink_xAuthListComplete( in_pPOP3->pop3Sink );

    return NSMAIL_OK;
}

static int parseXSender( pop3Client_t * in_pPOP3 )
{
    /* Server status */
    boolean l_status;

    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pPOP3->io), in_pPOP3->responseLine, MAX_TEXTLINE_LENGTH );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the status and the message. */
    l_nReturnCode = setStatusInfo(in_pPOP3->responseLine, &l_status);

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Invoke the callback. */
    if ( l_status )
    {
        pop3Sink_xSender( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[4]) );
    }
    else
    {
        pop3Sink_error( in_pPOP3->pop3Sink, &(in_pPOP3->responseLine[5]) );
    }

    return NSMAIL_OK;
}

static void pop3Sink_error( pop3Sink_t * in_pPOP3Sink, const char * in_response )
{
    if ( in_pPOP3Sink->error != NULL )
    {
        in_pPOP3Sink->error( in_pPOP3Sink, in_response );
    }
}

static void pop3Sink_listStart( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->listStart != NULL )
    {
        in_pPOP3Sink->listStart( in_pPOP3Sink );
    }
}

static void pop3Sink_list( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, int in_octetCount )
{
    if ( in_pPOP3Sink->list != NULL )
    {
        in_pPOP3Sink->list( in_pPOP3Sink, in_messageNumber, in_octetCount );
    }
}

static void pop3Sink_listComplete( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->listComplete != NULL )
    {
        in_pPOP3Sink->listComplete( in_pPOP3Sink );
    }
}

static void pop3Sink_noop( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->noop != NULL )
    {
        in_pPOP3Sink->noop( in_pPOP3Sink );
    }
}

static void pop3Sink_retrieveStart( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, int in_octetCount )
{
    if ( in_pPOP3Sink->retrieveStart != NULL )
    {
        in_pPOP3Sink->retrieveStart( in_pPOP3Sink, in_messageNumber, in_octetCount );
    }
}

static void pop3Sink_retrieve( pop3Sink_t * in_pPOP3Sink, const char * in_response )
{
    if ( in_pPOP3Sink->retrieve != NULL )
    {
        in_pPOP3Sink->retrieve( in_pPOP3Sink, in_response );
    }
}

static void pop3Sink_retrieveComplete( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->retrieveComplete != NULL )
    {
        in_pPOP3Sink->retrieveComplete( in_pPOP3Sink );
    }
}

static void pop3Sink_sendCommandStart( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->sendCommandStart != NULL )
    {
        in_pPOP3Sink->sendCommandStart( in_pPOP3Sink );
    }
}

static void pop3Sink_sendCommand( pop3Sink_t * in_pPOP3Sink, const char * in_response )
{
    if ( in_pPOP3Sink->sendCommand != NULL )
    {
        in_pPOP3Sink->sendCommand( in_pPOP3Sink, in_response );
    }
}

static void pop3Sink_sendCommandComplete( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->sendCommandComplete != NULL )
    {
        in_pPOP3Sink->sendCommandComplete( in_pPOP3Sink );
    }
}

static void pop3Sink_stat( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, int in_octetCount )
{
    if ( in_pPOP3Sink->stat != NULL )
    {
        in_pPOP3Sink->stat( in_pPOP3Sink, in_messageNumber, in_octetCount );
    }
}

static void pop3Sink_topStart( pop3Sink_t * in_pPOP3Sink, int in_messageNumber )
{
    if ( in_pPOP3Sink->topStart != NULL )
    {
        in_pPOP3Sink->topStart( in_pPOP3Sink, in_messageNumber );
    }
}

static void pop3Sink_top( pop3Sink_t * in_pPOP3Sink, const char * in_response )
{
    if ( in_pPOP3Sink->top != NULL )
    {
        in_pPOP3Sink->top( in_pPOP3Sink, in_response );
    }
}

static void pop3Sink_topComplete( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->topComplete != NULL )
    {
        in_pPOP3Sink->topComplete( in_pPOP3Sink );
    }
}

static void pop3Sink_uidListStart( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->uidListStart != NULL )
    {
        in_pPOP3Sink->uidListStart( in_pPOP3Sink );
    }
}

static void pop3Sink_uidList( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, const char * in_response )
{
    if ( in_pPOP3Sink->uidList != NULL )
    {
        in_pPOP3Sink->uidList( in_pPOP3Sink, in_messageNumber, in_response );
    }
}

static void pop3Sink_uidListComplete( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->uidListComplete != NULL )
    {
        in_pPOP3Sink->uidListComplete( in_pPOP3Sink );
    }
}

static void pop3Sink_xAuthListStart( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->xAuthListStart != NULL )
    {
        in_pPOP3Sink->xAuthListStart( in_pPOP3Sink );
    }
}

static void pop3Sink_xAuthList( pop3Sink_t * in_pPOP3Sink, int in_messageNumber, const char * in_response )
{
    if ( in_pPOP3Sink->xAuthList != NULL )
    {
        in_pPOP3Sink->xAuthList( in_pPOP3Sink, in_messageNumber, in_response );
    }
}

static void pop3Sink_xAuthListComplete( pop3Sink_t * in_pPOP3Sink )
{
    if ( in_pPOP3Sink->xAuthListComplete != NULL )
    {
        in_pPOP3Sink->xAuthListComplete( in_pPOP3Sink );
    }
}

static void pop3Sink_xSender( pop3Sink_t * in_pPOP3Sink, const char * in_response )
{
    if ( in_pPOP3Sink->xSender != NULL )
    {
        in_pPOP3Sink->xSender( in_pPOP3Sink, in_response );
    }
}

/* Initializes and allocates the pop3Sink_t structure. */
int pop3Sink_initialize( pop3Sink_t ** out_ppPOP3Sink )
{
    /* Parameter validation. */
    if ( out_ppPOP3Sink == NULL || (*out_ppPOP3Sink) != NULL )
    {
		return NSMAIL_ERR_INVALIDPARAM;
    }

    /* Create the sink */
    (*out_ppPOP3Sink) = (pop3Sink_t*)malloc( sizeof(pop3Sink_t) );
    
    if ( (*out_ppPOP3Sink) == NULL )
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}

    memset( (*out_ppPOP3Sink), 0, sizeof(pop3Sink_t) );

    return NSMAIL_OK;
}

/* Function used to free the pop3Sink_t structure. */
void pop3Sink_free( pop3Sink_t ** in_ppPOP3Sink )
{
	if ( in_ppPOP3Sink == NULL || *in_ppPOP3Sink == NULL )
	{
		return;
	}

    /* Free the client. */
    free( *in_ppPOP3Sink );
    *in_ppPOP3Sink = NULL;
}
