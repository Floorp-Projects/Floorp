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
  
import netscape.messaging.smtp.*;
import java.io.*;

/**
 * This file attempts to send a message
 */

class SMTPSendThread extends Thread
{
    private SMTPClient m_SMTPClient;
    private String m_server;
    private String m_domain;
    private String m_sender;
    private String m_recipient;
    private String m_data;

    // Constructor to initialize all the parameters
    SMTPSendThread( SMTPClient in_SMTPClient, 
                    String in_server, 
                    String in_domain, 
                    String in_sender, 
                    String in_recipient,
                    String in_data )
    {
        m_server = in_server;
        m_domain = in_domain;
        m_sender = in_sender;
        m_recipient = in_recipient;
        m_data = in_data;
        m_SMTPClient = in_SMTPClient;
    }

    public void run()
    {
        // Make sure that the client and the parameters are not null.
        if ( m_SMTPClient == null
                || m_server == null
                || m_domain == null
                || m_sender == null
                || m_recipient == null
                || m_data == null )
        {
            throw new IllegalArgumentException();
        }

        try
        {
            // Perform the sequence of steps required to send a message.
            m_SMTPClient.connect( m_server );
            m_SMTPClient.processResponses();
            m_SMTPClient.ehlo( m_domain );
            m_SMTPClient.processResponses();
            m_SMTPClient.mailFrom( m_sender, null );
            m_SMTPClient.processResponses();
            m_SMTPClient.rcptTo( m_recipient, null );
            m_SMTPClient.processResponses();
            m_SMTPClient.data();
            m_SMTPClient.processResponses();
            m_SMTPClient.send( new ByteArrayInputStream(m_data.getBytes()) );
            m_SMTPClient.processResponses();
        }
        catch( Exception e )
        {
            System.out.println( "SMTPSendThread.java" );
            System.out.println( e );
        }
    }
}
