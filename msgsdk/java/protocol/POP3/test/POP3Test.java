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

import netscape.messaging.pop3.*;
import java.io.*;

/**
*@author derekt@netscape.com
*@version 1.0
*/

// Usage: At the command line type "java POP3Test serverName user password messageNumber"

class POP3Test
{
	public static void main( String args[] )
	{
        POP3Client m_client;
        POP3TestSink m_pop3Sink;

        try
        {
            // Create the response sink.
            m_pop3Sink = new POP3TestSink();

            // Create the client.
            m_client = new POP3Client( m_pop3Sink );

            // Set the timeout to 10 seconds.
            m_client.setTimeout( 10000 );

            // Connect to the specified server.
            m_client.connect( args[0] );
            m_client.processResponses();

            // Send the USER command.
            m_client.user( args[1] );
            m_client.processResponses();

            // Send the PASS command.
            m_client.pass( args[2] );
            m_client.processResponses();
            
            // Send the LIST command.
            m_client.list();
            m_client.processResponses();

            // Send the LIST command with an argument.
            m_client.list( 1 );
            m_client.processResponses();

            // Send the RETR command.
            m_client.retrieve( (new Integer(args[3])).intValue() );
            m_client.processResponses();

            // Send the NOOP command.
            m_client.noop();
            m_client.processResponses();

            // Send the DELE command.
            m_client.delete( 1 );
            m_client.processResponses();

            // Send the RSET command.
            m_client.reset();
            m_client.processResponses();

            // Send a generic command.
            m_client.sendCommand( "RSET", false );
            m_client.processResponses();

            // Send the STAT command.
            m_client.stat();
            m_client.processResponses();

            // Send the TOP command.
            m_client.top( 1, 10 );
            m_client.processResponses();

            // Send the UIDL command.
            m_client.uidList();
            m_client.processResponses();

            // Send the UIDL command with an argument.
            m_client.uidList( 1 );
            m_client.processResponses();

            // Send the XAUTHLIST command.
            m_client.xAuthList();
            m_client.processResponses();

            // Send the XAUTHLIST command with an argument.
            m_client.xAuthList( 1 );
            m_client.processResponses();

            // Send the XSENDER command.
            m_client.xSender( 1 );
            m_client.processResponses();

            // Send the QUIT command.
            m_client.quit();
            m_client.processResponses();
        }
        catch( IOException e )
        {
            // Print out the exception.
            System.out.println( "Exception caught in main:" + e );
        }
    }
}
