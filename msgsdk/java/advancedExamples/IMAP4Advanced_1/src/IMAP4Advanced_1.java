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
  
import netscape.messaging.imap4.*;
import java.io.*;
import java.net.*;
import java.util.*;

/**
 * This file tests some of the multi-threaded aspects of the IMAP4 Java module.
 * It starts one thread which performs a fetch and another thread which performs
 * a search.
 * Usage: java IMAP4Advanced_1 server user password mailbox
 * Note: By default this program fetches message 1.
 */

public class IMAP4Advanced_1
{
    // The client and the sink
    protected IMAP4Client m_imap4Client;
    protected IIMAP4Sink m_imap4Sink;

    // One thread for fetching and another for searching
    public IMAP4FetchThread m_fetchThread;
    public IMAP4SearchThread m_searchThread;
    
	public IMAP4Advanced_1()
	{
        // Create the sink, client and the two threads
	    m_imap4Sink = new ServerSink();
	    m_imap4Client = new IMAP4Client( m_imap4Sink );
        m_fetchThread = new IMAP4FetchThread( m_imap4Client );
        m_searchThread = new IMAP4SearchThread( m_imap4Client );
	}

    public void initialize( String in_server, 
                            String in_user, 
                            String in_password, 
                            String in_mailBox )
    {
        try
        {
            // Set up the connection, login, and select a mailbox
            m_imap4Client.connect( in_server );
            m_imap4Client.processResponses();
            m_imap4Client.login( in_user, in_password );
            m_imap4Client.processResponses();
            m_imap4Client.select( in_mailBox );
            m_imap4Client.processResponses();
        }
        catch( Exception e )
        {
            System.out.println( "IMAPAdvanced.java" );
            System.out.println( e );
        }
    }

    public void terminate()
    {
        try
        {
            // Close the connection
            m_imap4Client.logout();
            m_imap4Client.processResponses();
        }
        catch( Exception e )
        {
            System.out.println( "IMAPAdvanced.java" );
            System.out.println( e );
        }
    }

	public static void main(String args[])
	{
        IMAP4Advanced_1 m_imapAdvanced = new IMAP4Advanced_1();

        // Initialize the connection
        m_imapAdvanced.initialize( args[0], args[1], args[2], args[3] );

        // Start the two threads
        m_imapAdvanced.m_fetchThread.start();
        m_imapAdvanced.m_searchThread.start();
       
        try
        {
            // Wait for both the threads to complete
            m_imapAdvanced.m_fetchThread.join();
            m_imapAdvanced.m_searchThread.join();
        }
        catch( Exception e )
        {
            System.out.println( "IMAPAdvanced.java" );
            System.out.println( e );
        }
        
        // Close the connection
        m_imapAdvanced.terminate();
    }

}
