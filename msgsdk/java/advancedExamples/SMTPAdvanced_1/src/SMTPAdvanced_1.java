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
import java.net.*;
import java.util.*;

/**
 * This file tests some of the multi-threaded aspects of the SMTP Java module.
 * It starts a thread to send a message and uses another thread to cancel it.
 * Usage: java SMTPAdvanced_1 server domain sender recipient data
 * NOTE: Data must be 1 word only
 */

public class SMTPAdvanced_1
{
    // The client and the sink
    protected SMTPClient m_smtpClient;
    protected ISMTPSink m_smtpSink;

    // One thread for sending a message and another for cancelling the send
    protected SMTPSendThread m_sendThread;
    protected SMTPCancelThread m_cancelThread;

	public SMTPAdvanced_1( String in_server,
	                        String in_domain,
	                        String in_sender,
	                        String in_recipient,
	                        String in_data )
	{
        // Create the sink, client and the two threads
	    m_smtpSink = new SMTPSink();
	    m_smtpClient = new SMTPClient( m_smtpSink );
        m_sendThread = new SMTPSendThread( m_smtpClient,
                                            in_server,
                                            in_domain,
                                            in_sender,
                                            in_recipient,
                                            in_data );
        m_cancelThread = new SMTPCancelThread( m_smtpClient );
	}

	public static void main(String args[])
	{
        SMTPAdvanced_1 m_smtpAdvanced = new SMTPAdvanced_1( args[0],
                                                            args[1],
                                                            args[2],
                                                            args[3],
                                                            args[4] );

        // Start one thread to send the message and start the second thread to cancel it
        m_smtpAdvanced.m_sendThread.start();
        m_smtpAdvanced.m_cancelThread.start();
    }
}
