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
 * smtp.c
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include "smtp.h"
#include "smtppriv.h"
#include "nsio.h"

#include <malloc.h>
#include <ctype.h>
#include <string.h>

int smtp_initialize( smtpClient_t ** out_ppSMTP, smtpSink_t * in_pSMTPSink )
{
    smtpClient_i_t * l_pSMTP;

    /* Parameter validation. */
    if ( out_ppSMTP == NULL || (*out_ppSMTP) != NULL || in_pSMTPSink == NULL )
    {
		return NSMAIL_ERR_INVALIDPARAM;
    }

    /* Create the client. */
    l_pSMTP = (smtpClient_i_t*)malloc( sizeof(smtpClient_i_t) );
    
    if ( l_pSMTP == NULL )
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}

    /* Set the data members of the smtpClient_t structure. */
    l_pSMTP->timeout = -1;
    l_pSMTP->chunkSize = 1024;
    l_pSMTP->smtpSink = in_pSMTPSink;
    l_pSMTP->mustProcess = FALSE;
    l_pSMTP->pipelining = FALSE;
    l_pSMTP->lastSentChar = '\n';
    l_pSMTP->pCommandList = NULL;
    l_pSMTP->fDATASent = FALSE;

	memset(&(l_pSMTP->io), 0, sizeof( IO_t ) );
    l_pSMTP->messageData = (char*)malloc( l_pSMTP->chunkSize * sizeof(char) );

    if ( l_pSMTP->messageData == NULL )
    {
        return NSMAIL_ERR_OUTOFMEMORY;
    }

    *out_ppSMTP = l_pSMTP;

    /* Initialize the IO structure. */
    return IO_initialize( (&l_pSMTP->io), MAX_BUFFER_LENGTH, l_pSMTP->chunkSize );
}

void smtp_free( smtpClient_t ** in_ppSMTP )
{
	if ( in_ppSMTP == NULL || *in_ppSMTP == NULL )
	{
		return;
	}

    /* Free the members of the IO structure. */
    IO_free( &((*in_ppSMTP)->io) );

    /* Remove all the elements from the linked list. */
    RemoveAllElements( &((*in_ppSMTP)->pCommandList) );

    if ( (*in_ppSMTP)->messageData != NULL )
    {
        free( (*in_ppSMTP)->messageData );
    }

    /* Free the client. */
    free( (*in_ppSMTP) );
    *in_ppSMTP = NULL;
}

int smtp_bdat( smtpClient_t * in_pSMTP, 
                const char * in_data, 
                unsigned int in_length, 
                boolean in_fLast )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_data == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_write( &(in_pSMTP->io), bdat, strlen(bdat) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Send the command. */
    sprintf( in_pSMTP->commandBuffer, "%d", in_length );

    if ( in_fLast )
    {
        l_nReturnCode = IO_write(&(in_pSMTP->io), bdatLast, strlen(bdatLast));

        if ( l_nReturnCode != NSMAIL_OK )
        {
		    return l_nReturnCode;
	    }
    }

    l_nReturnCode = IO_write(&(in_pSMTP->io), eoline, strlen(eoline));

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_write( &(in_pSMTP->io), in_data, in_length );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
						(void*)&SMTPResponse_BDAT );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_pSMTP->pipelining == FALSE )
    {
        in_pSMTP->mustProcess = TRUE;
    }

    return NSMAIL_OK;
}

int smtp_connect( smtpClient_t * in_pSMTP, 
                    const char * in_server, 
                    unsigned short in_port )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_server == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

	/* Reset data members. */
    in_pSMTP->pipelining = FALSE;
    in_pSMTP->lastSentChar = '\n';
    RemoveAllElements( &(in_pSMTP->pCommandList) );

	/* Connect to the server. */
    if ( in_port == 0 )
    {
        in_port = DEFAULT_SMTP_PORT;
    }

    l_nReturnCode = IO_connect( &(in_pSMTP->io), in_server, in_port );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_CONN );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_data( smtpClient_t * in_pSMTP )
{
	int l_nReturnCode;

	/* Check for errors. */

    if ( in_pSMTP == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    /* Send the command. */
    l_nReturnCode = IO_send( &(in_pSMTP->io), data, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_DATA );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;

    return NSMAIL_OK;
}

int smtp_disconnect( smtpClient_t * in_pSMTP )
{
    /* Disconnect from the server. */
    return IO_disconnect( &(in_pSMTP->io) );
}

int smtp_ehlo( smtpClient_t * in_pSMTP, const char * in_domain )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_domain == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    /* Send the command. */
    l_nReturnCode = IO_write( &(in_pSMTP->io), ehlo, strlen(ehlo) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_send( &(in_pSMTP->io), in_domain, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_EHLO );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_expand( smtpClient_t * in_pSMTP, const char * in_mailingList )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_mailingList == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    /* Send the command. */
    l_nReturnCode = IO_write( &(in_pSMTP->io), expn, strlen(expn) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_send( &(in_pSMTP->io), in_mailingList, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_EXPN );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_help( smtpClient_t * in_pSMTP, const char * in_helpTopic )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    /* Send the command. */
    l_nReturnCode = IO_write( &(in_pSMTP->io), help, strlen(help) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_helpTopic != NULL )
    {
        l_nReturnCode = IO_send( &(in_pSMTP->io), in_helpTopic, FALSE );
    }
    else
    {
        l_nReturnCode = IO_send( &(in_pSMTP->io), empty, FALSE );
    }

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_HELP );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_mailFrom( smtpClient_t * in_pSMTP, 
                    const char * in_reverseAddress, 
                    const char * in_esmtpParams )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_reverseAddress == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_write( &(in_pSMTP->io), mailFrom, strlen(mailFrom) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                in_reverseAddress, 
                                strlen(in_reverseAddress) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_write( &(in_pSMTP->io), rBracket, strlen(rBracket) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_esmtpParams != NULL )
    {
        l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                    in_esmtpParams, 
                                    strlen(in_esmtpParams) );

        if ( l_nReturnCode != NSMAIL_OK )
        {
		    return l_nReturnCode;
	    }
    }

    l_nReturnCode = IO_send( &(in_pSMTP->io), empty, in_pSMTP->pipelining );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_MAIL );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_pSMTP->pipelining == FALSE )
    {
        in_pSMTP->mustProcess = TRUE;
    }

    return NSMAIL_OK;
}

int smtp_noop( smtpClient_t * in_pSMTP )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_send( &(in_pSMTP->io), noop, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_NOOP );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_quit( smtpClient_t * in_pSMTP )
{
	int l_nReturnCode;

	/* Check for errors. */

    if ( in_pSMTP == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_send( &(in_pSMTP->io), quit, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_QUIT );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_rcptTo( smtpClient_t * in_pSMTP, 
                    const char * in_forwardAddress, 
                    const char * in_esmtpParams )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_forwardAddress == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_write( &(in_pSMTP->io), rcptTo, strlen(rcptTo) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                in_forwardAddress, 
                                strlen(in_forwardAddress) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_write( &(in_pSMTP->io), rBracket, strlen(rBracket) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_esmtpParams != NULL )
    {
        l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                    in_esmtpParams, 
                                    strlen(in_esmtpParams) );

        if ( l_nReturnCode != NSMAIL_OK )
        {
		    return l_nReturnCode;
	    }
    }

    l_nReturnCode = IO_send( &(in_pSMTP->io), empty, in_pSMTP->pipelining );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_RCPT );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_pSMTP->pipelining == FALSE )
    {
        in_pSMTP->mustProcess = TRUE;
    }

    return NSMAIL_OK;
}

int smtp_reset( smtpClient_t * in_pSMTP )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_send( &(in_pSMTP->io), rset, in_pSMTP->pipelining );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_RSET );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    return NSMAIL_OK;
}

int smtp_send( smtpClient_t * in_pSMTP, const char * in_data )
{
    unsigned int l_offset = 0;
    unsigned int l_byteCount;
    unsigned int count;
    int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_data == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
	{
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
	}

    if ( in_pSMTP->fDATASent == FALSE )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_byteCount = strlen(in_data);

    for( count = 0; count < l_byteCount; count++ )
    {
        if ( (in_pSMTP->lastSentChar == '\n') && 
                (in_data[count] == '.') )
        {
            l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                        &(in_data[l_offset]),
                                        count-l_offset );
            if ( l_nReturnCode != NSMAIL_OK )
            {
		        return l_nReturnCode;
	        }

            l_nReturnCode = IO_writeByte( &(in_pSMTP->io), '.' );

            if ( l_nReturnCode != NSMAIL_OK )
            {
		        return l_nReturnCode;
	        }

            l_offset = count;
        }

        in_pSMTP->lastSentChar = in_data[count];
    }

    if ( l_offset < l_byteCount )
    {
        l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                    &(in_data[l_offset]),
                                    l_byteCount-l_offset );
        if ( l_nReturnCode != NSMAIL_OK )
        {
		    return l_nReturnCode;
	    }
    }

    l_nReturnCode = IO_send(&(in_pSMTP->io), eomessage, in_pSMTP->pipelining);

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->lastSentChar = '\n';

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_SEND );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_pSMTP->pipelining == FALSE )
    {
        in_pSMTP->mustProcess = TRUE;
    }

    in_pSMTP->fDATASent = FALSE;
    return NSMAIL_OK;
}

int smtp_sendStream( smtpClient_t * in_pSMTP, nsmail_inputstream_t * in_inputStream )
{
    unsigned int l_offset;
    int l_nReturnCode;
    unsigned int count;
    unsigned int l_byteCount;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_inputStream == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
	{
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
	}

    if ( in_pSMTP->fDATASent == FALSE )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    while ( (l_byteCount = in_inputStream->read( in_inputStream->rock, 
                                    in_pSMTP->messageData, 
                                    in_pSMTP->chunkSize ) ) != -1 )
    {
        l_offset = 0;

        for( count = 0; count < l_byteCount; count++ )
        {
            if ( (in_pSMTP->lastSentChar == '\n') && 
                    (in_pSMTP->messageData[count] == '.') )
            {
                l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                            &(in_pSMTP->messageData[l_offset]),
                                            count-l_offset );
                if ( l_nReturnCode != NSMAIL_OK )
                {
		            return l_nReturnCode;
	            }

                l_nReturnCode = IO_writeByte( &(in_pSMTP->io), '.' );

                if ( l_nReturnCode != NSMAIL_OK )
                {
		            return l_nReturnCode;
	            }

                l_offset = count;
            }

            in_pSMTP->lastSentChar = in_pSMTP->messageData[count];
        }

        if ( l_offset < l_byteCount )
        {
            l_nReturnCode = IO_write( &(in_pSMTP->io), 
                                        &(in_pSMTP->messageData[l_offset]),
                                        l_byteCount-l_offset );
            if ( l_nReturnCode != NSMAIL_OK )
            {
		        return l_nReturnCode;
	        }
        }
    }

    l_nReturnCode = IO_send(&(in_pSMTP->io), eomessage, in_pSMTP->pipelining);

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->lastSentChar = '\n';

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_SEND );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    if ( in_pSMTP->pipelining == FALSE )
    {
        in_pSMTP->mustProcess = TRUE;
    }

    in_pSMTP->fDATASent = FALSE;
    return NSMAIL_OK;
}

int smtp_sendCommand( smtpClient_t * in_pSMTP, const char * in_command )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_command == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_send( &(in_pSMTP->io), in_command, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_SENDCOMMAND );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_verify( smtpClient_t * in_pSMTP, const char * in_user )
{
	int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL || in_user == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( in_pSMTP->mustProcess )
    {
	    return NSMAIL_ERR_MUSTPROCESSRESPONSES;
    }

    if ( in_pSMTP->fDATASent )
    {
	    return NSMAIL_ERR_SENDDATA;
    }

    l_nReturnCode = IO_write( &(in_pSMTP->io), vrfy, strlen(vrfy) );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    l_nReturnCode = IO_send( &(in_pSMTP->io), in_user, FALSE );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

	/* Store the command type on the pending response list. */
    l_nReturnCode = AddElement( &(in_pSMTP->pCommandList), 
								(void*)&SMTPResponse_VRFY );

    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    in_pSMTP->mustProcess = TRUE;
    return NSMAIL_OK;
}

int smtp_processResponses( smtpClient_t * in_pSMTP )
{
    smtp_ResponseType_t * l_pCurrentResponse;
    LinkedList_t * l_pCurrentLink;
    int l_nReturnCode;

	/* Check for errors. */
    if ( in_pSMTP == NULL )
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

    /* Make sure that all buffered commands are sent. */
    l_nReturnCode = IO_flush( &(in_pSMTP->io) );
    
    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    /* Continue to process responses while the list is not empty. */
    /* or a timeout occurs. */
    while ( in_pSMTP->pCommandList != NULL )
    {
        l_pCurrentResponse = (smtp_ResponseType_t *)in_pSMTP->pCommandList->pData;

        switch ( *l_pCurrentResponse )
        {
            case BDAT:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->bdat );
                break;
            case CONN:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->connect );
                if ( l_nReturnCode != NSMAIL_OK )
                {
                    IO_disconnect( &(in_pSMTP->io) );
                }
                break;
            case DATA:
                l_nReturnCode = parseData( in_pSMTP );
                break;
            case EHLO:
                l_nReturnCode = parseEhlo( in_pSMTP );
                break;
            case EXPN:
                l_nReturnCode = parseMultiLine( in_pSMTP, 
                                                in_pSMTP->smtpSink->expand,
                                                in_pSMTP->smtpSink->expandComplete );
                break;
            case HELP:
                l_nReturnCode = parseMultiLine( in_pSMTP, 
                                                    in_pSMTP->smtpSink->help,
                                                    in_pSMTP->smtpSink->helpComplete );
                break;
            case MAIL:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->mailFrom );
                break;
            case NOOP:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->noop );
                break;
            case QUIT:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->quit );

                if ( l_nReturnCode != NSMAIL_OK )
                {
                    return l_nReturnCode;
                }
                l_nReturnCode = IO_disconnect( &(in_pSMTP->io) );
                break;
            case RCPT:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->rcptTo );
                break;
            case RSET:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->reset );
                break;
            case SEND:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->send );
                break;
            case SENDCOMMAND:
                l_nReturnCode = parseMultiLine( in_pSMTP, 
                                                in_pSMTP->smtpSink->sendCommand,
                                                in_pSMTP->smtpSink->sendCommandComplete );
                break;
            case VRFY:
                l_nReturnCode = parseSingleLine( in_pSMTP, in_pSMTP->smtpSink->verify );
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
                in_pSMTP->mustProcess = FALSE;
            }
            return l_nReturnCode;
        }

        /* Move to the next element in the pending response list. */
        l_pCurrentLink = in_pSMTP->pCommandList;
		in_pSMTP->pCommandList = in_pSMTP->pCommandList->next;

        /* Free the current link in the pending response list. */
        free ( l_pCurrentLink );
	}
	
    in_pSMTP->mustProcess = FALSE;
    return NSMAIL_OK;
}

int smtp_setChunkSize( smtpClient_t * in_pSMTP, int in_chunkSize )
{
    if ( in_pSMTP == NULL || in_chunkSize <= 0 )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    free( in_pSMTP->messageData );

    in_pSMTP->messageData = (char*)malloc( in_chunkSize * sizeof(char) );

    if ( in_pSMTP->messageData == NULL )
    {
        return NSMAIL_ERR_OUTOFMEMORY;
    }

    in_pSMTP->chunkSize = in_chunkSize;

    return NSMAIL_OK;
}

int smtp_setPipelining( smtpClient_t * in_pSMTP, boolean in_enablePipelining )
{
    if ( in_pSMTP->pipeliningSupported == FALSE && in_enablePipelining )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    in_pSMTP->pipelining = in_enablePipelining;

    return NSMAIL_OK;
}

int smtp_setResponseSink( smtpClient_t * in_pSMTP, smtpSink_t * in_pSMTPSink )
{
    if ( in_pSMTP == NULL || in_pSMTPSink == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    in_pSMTP->smtpSink = in_pSMTPSink;

    return NSMAIL_OK;
}

int smtp_setTimeout( smtpClient_t * in_pSMTP, double in_timeout )
{
    if ( in_pSMTP == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    return IO_setTimeout( &(in_pSMTP->io), in_timeout );
}

int smtp_set_option( smtpClient_t * in_pSMTP, int in_option, void * in_pOptionData )
{
    if ( in_pSMTP == NULL || in_pOptionData == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    switch ( in_option )
    {
        case NSMAIL_OPT_IO_FN_PTRS:
            IO_setFunctions( &(in_pSMTP->io), (nsmail_io_fns_t *)in_pOptionData );
            return NSMAIL_OK;
            break;
        default:
            return NSMAIL_ERR_UNEXPECTED;
            break;
    }
}

int smtp_get_option( smtpClient_t * in_pSMTP, int in_option, void * in_pOptionData )
{
    if ( in_pSMTP == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    switch ( in_option )
    {
        case NSMAIL_OPT_IO_FN_PTRS:
            IO_getFunctions( &(in_pSMTP->io), (nsmail_io_fns_t *)in_pOptionData );
            return NSMAIL_OK;
            break;
        default:
            return NSMAIL_ERR_UNEXPECTED;
            break;
    }
}

static int setStatusInfo( const char * in_responseLine, int * out_responseCode )
{
    if ( in_responseLine == NULL || out_responseCode == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( strlen(in_responseLine) < 4 )
    {
        return NSMAIL_ERR_PARSE;
    }

    *out_responseCode = atoi( in_responseLine );

    if ( *out_responseCode == 0 )
    {
        return NSMAIL_ERR_PARSE;
    }

    return NSMAIL_OK;
}

static int parseSingleLine( smtpClient_t * in_pSMTP, sinkMethod_t in_callback )
{
    /* The 3-digit response code. */
    int l_nResponseCode;

    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pSMTP->io), in_pSMTP->responseLine, 512 );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the response code and the message. */
    l_nReturnCode = setStatusInfo( in_pSMTP->responseLine, 
                                    &l_nResponseCode );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    if ( l_nResponseCode >= 100 && l_nResponseCode < 400 )
    {
        invokeCallback( in_pSMTP->smtpSink, 
                        in_callback, 
                        l_nResponseCode, 
                        &(in_pSMTP->responseLine[4]) );
    }
    else if ( l_nResponseCode >= 400 && l_nResponseCode < 600 )
    {
        invokeCallback( in_pSMTP->smtpSink, 
                        in_pSMTP->smtpSink->error, 
                        l_nResponseCode, 
                        &(in_pSMTP->responseLine[4]) );
    }
    else
    {
        return NSMAIL_ERR_UNEXPECTED;
    }

    return NSMAIL_OK;
}

static int parseMultiLine( smtpClient_t * in_pSMTP, 
                    sinkMethod_t in_callback, 
                    sinkMethodComplete_t in_callbackComplete )
{
    /* The 3-digit response code. */
    int l_nResponseCode;
    int l_nReturnCode;

    do
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pSMTP->io), in_pSMTP->responseLine, 512 );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the response code and the message. */
        l_nReturnCode = setStatusInfo( in_pSMTP->responseLine, 
                                    &l_nResponseCode );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        if ( l_nResponseCode >= 100 && l_nResponseCode < 400 )
        {
            invokeCallback( in_pSMTP->smtpSink, 
                            in_callback, 
                            l_nResponseCode, 
                            &(in_pSMTP->responseLine[4]) );
        }
        else if ( l_nResponseCode >= 400 && l_nResponseCode < 600 )
        {
            invokeCallback( in_pSMTP->smtpSink, 
                            in_pSMTP->smtpSink->error, 
                            l_nResponseCode, 
                            &(in_pSMTP->responseLine[4]) );
        }
        else
        {
            return NSMAIL_ERR_UNEXPECTED;
        }

    } while( in_pSMTP->responseLine[3] == '-' );

    /* Invoke the callback. */
    if ( in_callbackComplete != NULL )
    {
        in_callbackComplete(in_pSMTP->smtpSink);
    }

    return NSMAIL_OK;
}

static int parseData( smtpClient_t * in_pSMTP )
{
    /* The 3-digit response code. */
    int l_nResponseCode;

    /* Read the next line of data from the socket. */
    int l_nReturnCode = IO_readLine( &(in_pSMTP->io), in_pSMTP->responseLine, 512 );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    /* Parse out the response code and the message. */
    l_nReturnCode = setStatusInfo( in_pSMTP->responseLine, 
                                    &l_nResponseCode );

    if ( l_nReturnCode != NSMAIL_OK )
    {
	    return l_nReturnCode;
    }

    if ( l_nResponseCode >= 100 && l_nResponseCode < 400 )
    {
        invokeCallback( in_pSMTP->smtpSink, 
                        in_pSMTP->smtpSink->data, 
                        l_nResponseCode, 
                        &(in_pSMTP->responseLine[4]) );

        in_pSMTP->fDATASent = TRUE;
    }
    else if ( l_nResponseCode >= 400 && l_nResponseCode < 600 )
    {
        invokeCallback( in_pSMTP->smtpSink, 
                        in_pSMTP->smtpSink->error, 
                        l_nResponseCode, 
                        &(in_pSMTP->responseLine[4]) );
    }
    else
    {
        return NSMAIL_ERR_UNEXPECTED;
    }

    return NSMAIL_OK;
}

static int parseEhlo( smtpClient_t * in_pSMTP )
{
    /* The 3-digit response code. */
    int l_nResponseCode;
    int l_nReturnCode;

    do
    {
        /* Read the next line of data from the socket. */
        l_nReturnCode = IO_readLine( &(in_pSMTP->io), in_pSMTP->responseLine, 512 );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Parse out the response code and the message. */
        l_nReturnCode = setStatusInfo( in_pSMTP->responseLine, 
                                    &l_nResponseCode );

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        /* Determine if PIPELINING is supported by the server. */
        if ( in_pSMTP->pipelining == FALSE )
        {
            if (strncmp(pipelining, &(in_pSMTP->responseLine[4]), 10) == 0)
            {
                in_pSMTP->pipeliningSupported = TRUE;
            }
        }

        if ( l_nReturnCode != NSMAIL_OK )
        {
	        return l_nReturnCode;
        }

        if ( l_nResponseCode >= 100 && l_nResponseCode < 400 )
        {
            invokeCallback( in_pSMTP->smtpSink, 
                            in_pSMTP->smtpSink->ehlo, 
                            l_nResponseCode, 
                            &(in_pSMTP->responseLine[4]) );
        }
        else if ( l_nResponseCode >= 400 && l_nResponseCode < 600 )
        {
            invokeCallback( in_pSMTP->smtpSink, 
                            in_pSMTP->smtpSink->error, 
                            l_nResponseCode, 
                            &(in_pSMTP->responseLine[4]) );
        }
        else
        {
            return NSMAIL_ERR_UNEXPECTED;
        }

    } while( in_pSMTP->responseLine[3] == '-' );

    /* Invoke the callback. */
    if ( in_pSMTP->smtpSink->ehloComplete != NULL )
    {
        in_pSMTP->smtpSink->ehloComplete(in_pSMTP->smtpSink);
    }

    return NSMAIL_OK;
}

static void invokeCallback( smtpSink_t * in_pSMTPSink,
                        sinkMethod_t in_callback,
                        int in_responseCode, 
                        const char * in_responseMessage )
{
    if ( in_callback != NULL )
    {
        in_callback( in_pSMTPSink, in_responseCode, in_responseMessage );
    }
}

/* Initializes and allocates the smtpSink_t structure. */
int smtpSink_initialize( smtpSink_t ** out_ppSMTPSink )
{
    /* Parameter validation. */
    if ( out_ppSMTPSink == NULL || (*out_ppSMTPSink) != NULL )
    {
		return NSMAIL_ERR_INVALIDPARAM;
    }

    /* Create the sink */
    (*out_ppSMTPSink) = (smtpSink_t*)malloc( sizeof(smtpSink_t) );
    
    if ( (*out_ppSMTPSink) == NULL )
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}

    memset( (*out_ppSMTPSink), 0, sizeof(smtpSink_t) );

    return NSMAIL_OK;
}

/* Function used to free the smtpSink_t structure. */
void smtpSink_free( smtpSink_t ** in_ppSMTPSink )
{
	if ( in_ppSMTPSink == NULL || *in_ppSMTPSink == NULL )
	{
		return;
	}

    /* Free the sink. */
    free( *in_ppSMTPSink );
    *in_ppSMTPSink = NULL;
}

#ifdef SMTP_HIGHLEVEL_API

/*
 * A higher level API
 * @author derekt@netscape.com
 * @version 1.0
 */

void sinkError( smtpSinkPtr_t in_pSMTPSink, int in_responseCode, const char * in_errorMessage )
{
    returnInfo_t * l_pReturnInfo = (returnInfo_t*)in_pSMTPSink->pOpaqueData;

    l_pReturnInfo->error = TRUE;
    strncpy( (char*)&(l_pReturnInfo->errorBuffer), in_errorMessage, 512 );
}

int smtp_sendMessage( const char * in_server, 
                        const char * in_domain, 
                        const char * in_sender, 
                        const char * in_recipients, 
                        const char * in_message,
                        char * out_serverError )
{
    int l_nReturn;
    char l_delim = ',';
    smtpClient_t * l_pClient = NULL;
    smtpSink_t * l_pSink = NULL;
    char * l_pRecipient1;
    char * l_pRecipient2;
    char l_recipient[256];
    returnInfo_t l_returnInfo;

    /* Check for invalid parameters. */
    if ( in_server == NULL ||
            in_domain == NULL ||
            in_sender == NULL ||
            in_recipients == NULL ||
            in_message == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( out_serverError != NULL )
    {
        out_serverError[0] = '\0';
    }

    /* Initialize the response sink. */
    l_nReturn = smtpSink_initialize( &l_pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_pSink->error = sinkError;

    l_returnInfo.error = FALSE;
    memset( &(l_returnInfo.errorBuffer), 0, 512 * sizeof(char) );
    l_pSink->pOpaqueData = &l_returnInfo;

    /* Initialize the client. */
    l_nReturn = smtp_initialize( &l_pClient, l_pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        free( l_pSink->pOpaqueData );
        smtpSink_free( &l_pSink );
        return l_nReturn;
    }

    /* Connect to the server. */
    l_nReturn = smtp_connect( l_pClient, in_server, 25 );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Send the EHLO command. */
    l_nReturn = smtp_ehlo( l_pClient, in_domain );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Send the MAIL FROM command. */
    l_nReturn = smtp_mailFrom( l_pClient, in_sender, NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    l_pRecipient1 = (char*)in_recipients;

    while( (l_pRecipient2 = strchr( l_pRecipient1, l_delim )) != NULL )
    {
        l_pRecipient2[0] = '\0';

        if ( strlen(l_pRecipient2) > 256 )
        {
            cleanup( l_pClient, l_pSink );
            return NSMAIL_ERR_PARSE;
        }

        strncpy( l_recipient, l_pRecipient1, 256 );
        l_pRecipient2[0] = ',';

        /* Send the RCPT TO command. */
        l_nReturn = smtp_rcptTo( l_pClient, l_recipient, NULL );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }

        l_nReturn = smtp_processResponses( l_pClient );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }
        else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
        {
            cleanupServerError( l_pClient, l_pSink, out_serverError );
            return NSMAIL_ERR_PARSE;
        }

        l_pRecipient1 = &(l_pRecipient2[1]);
    }

    /* Move ahead past the white space. */
    while ( *l_pRecipient1 == ' ' || *l_pRecipient1 == '\t' )
    {
        l_pRecipient1 = &(l_pRecipient1[1]);
    }

    /* See if it is the end. */
    if ( strlen( l_pRecipient1 ) != 0 )
    {
        /* Send the RCPT TO command. */
        l_nReturn = smtp_rcptTo( l_pClient, l_pRecipient1, NULL );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }

        l_nReturn = smtp_processResponses( l_pClient );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }
        else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
        {
            cleanupServerError( l_pClient, l_pSink, out_serverError );
            return NSMAIL_ERR_PARSE;
        }
    }

    /* Send the DATA command. */
    l_nReturn = smtp_data( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Send the message. */
    l_nReturn = smtp_send( l_pClient, in_message );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Disconnect from the server. */
    l_nReturn = smtp_quit( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    cleanup( l_pClient, l_pSink );
    return NSMAIL_OK;
}

int smtp_sendMessageStream( const char * in_server, 
                            const char * in_domain, 
                            const char * in_sender, 
                            const char * in_recipients, 
                            nsmail_inputstream_t * in_inputStream,
                            char * out_serverError )
{
    int l_nReturn;
    char l_delim = ',';
    smtpClient_t * l_pClient = NULL;
    smtpSink_t * l_pSink = NULL;
    char * l_pRecipient1;
    char * l_pRecipient2;
    char l_recipient[256];
    returnInfo_t l_returnInfo;

    /* Check for invalid parameters. */
    if ( in_server == NULL ||
            in_domain == NULL ||
            in_sender == NULL ||
            in_recipients == NULL ||
            in_inputStream == NULL )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    if ( out_serverError != NULL )
    {
        out_serverError[0] = '\0';
    }

    /* Initialize the response sink. */
    l_nReturn = smtpSink_initialize( &l_pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_pSink->error = sinkError;

    l_returnInfo.error = FALSE;
    memset( &(l_returnInfo.errorBuffer), 0, 512 * sizeof(char) );
    l_pSink->pOpaqueData = &l_returnInfo;

    /* Initialize the client. */
    l_nReturn = smtp_initialize( &l_pClient, l_pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        free( l_pSink->pOpaqueData );
        smtpSink_free( &l_pSink );
        return l_nReturn;
    }

    /* Connect to the server. */
    l_nReturn = smtp_connect( l_pClient, in_server, 25 );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Send the EHLO command. */
    l_nReturn = smtp_ehlo( l_pClient, in_domain );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Send the MAIL FROM command. */
    l_nReturn = smtp_mailFrom( l_pClient, in_sender, NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    l_pRecipient1 = (char*)in_recipients;

    while( (l_pRecipient2 = strchr( l_pRecipient1, l_delim )) != NULL )
    {
        l_pRecipient2[0] = '\0';

        if ( strlen(l_pRecipient2) > 256 )
        {
            cleanup( l_pClient, l_pSink );
            return NSMAIL_ERR_PARSE;
        }

        strncpy( l_recipient, l_pRecipient1, 256 );
        l_pRecipient2[0] = ',';

        /* Send the RCPT TO command. */
        l_nReturn = smtp_rcptTo( l_pClient, l_recipient, NULL );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }

        l_nReturn = smtp_processResponses( l_pClient );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }
        else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
        {
            cleanupServerError( l_pClient, l_pSink, out_serverError );
            return NSMAIL_ERR_PARSE;
        }

        l_pRecipient1 = &(l_pRecipient2[1]);
    }

    /* Move ahead past the white space. */
    while ( *l_pRecipient1 == ' ' || *l_pRecipient1 == '\t' )
    {
        l_pRecipient1 = &(l_pRecipient1[1]);
    }

    /* See if it is the end. */
    if ( strlen( l_pRecipient1 ) != 0 )
    {
        /* Send the RCPT TO command. */
        l_nReturn = smtp_rcptTo( l_pClient, l_pRecipient1, NULL );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }

        l_nReturn = smtp_processResponses( l_pClient );

        if ( l_nReturn != NSMAIL_OK )
        {
            cleanup( l_pClient, l_pSink );
            return l_nReturn;
        }
        else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
        {
            cleanupServerError( l_pClient, l_pSink, out_serverError );
            return NSMAIL_ERR_PARSE;
        }
    }

    /* Send the DATA command. */
    l_nReturn = smtp_data( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Send the message. */
    l_nReturn = smtp_sendStream( l_pClient, in_inputStream );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    /* Disconnect from the server. */
    l_nReturn = smtp_quit( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( l_pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        cleanup( l_pClient, l_pSink );
        return l_nReturn;
    }
    else if ( ((returnInfo_t*)l_pSink->pOpaqueData)->error == TRUE )
    {
        cleanupServerError( l_pClient, l_pSink, out_serverError );
        return NSMAIL_ERR_PARSE;
    }

    cleanup( l_pClient, l_pSink );
    return NSMAIL_OK;
}

void cleanup( smtpClient_t * in_pClient, smtpSink_t * in_pSink )
{
    smtp_free( &in_pClient );
    smtpSink_free( &in_pSink );
}

void cleanupServerError( smtpClient_t * in_pClient, smtpSink_t * in_pSink, char * out_serverError )
{
    if ( out_serverError != NULL )
    {
        strncpy( out_serverError, (char*)&(((returnInfo_t*)in_pSink->pOpaqueData)->errorBuffer), 512 );
    }

    smtp_free( &in_pClient );
    smtpSink_free( &in_pSink );
}

#endif /*SMTP_HIGHLEVEL_API*/

