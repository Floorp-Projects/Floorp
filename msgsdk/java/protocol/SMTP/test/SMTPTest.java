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

import netscape.messaging.smtp.*;
import java.io.*;
import java.util.*;

/**
*@author derekt@netscape.com
*@version 1.0
*/

// Usage: At the command line type "java SMTPTest serverName domainName sender recipient messageFile"

class SMTPTest
{
	public static void main( String args[] )
	{
        SMTPClient m_client;
        ISMTPSink m_smtpSink;

        try
        {
            // Create the response sink.
            m_smtpSink = new SMTPTestSink();

            // Create the SMTP client.
            m_client = new SMTPClient( m_smtpSink );

            // Set a timeout value of 10 seconds.
            m_client.setTimeout( 10000 );

            // Set a new chunk size.
            m_client.setChunkSize( 1048576 );
               
            // Connect to the specified server.
            m_client.connect( args[0] );
            m_client.processResponses();

            // Send the EHLO command.
            m_client.ehlo( args[1] );
            m_client.processResponses();

            // Specify the sender of the message.
            m_client.mailFrom( args[2], null );
            m_client.processResponses();

            // Specify the recipient of the message.
            m_client.rcptTo( args[3], null );
            m_client.processResponses();

            // Inform the server that data is about to be sent.
            m_client.data();
            m_client.processResponses();

            // Send the file based message.
            m_client.send( new FileInputStream( args[4] ) );
            m_client.processResponses();

            // Ask for help on helpSend the file based message.
            m_client.help( "help" );
            m_client.processResponses();

            // Perform a noopsk for help on helpSend the file based message.
            m_client.noop();
            m_client.processResponses();

            // Reset the state of the server.
            m_client.reset();
            m_client.processResponses();

            // Send a generic command to the server.  In this case it is RSET.
            m_client.sendCommand( "RSET" );
            m_client.processResponses();

            // End the SMTP session.
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
