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

/**
 * This file fetches the body of message 1
 */

class IMAP4FetchThread extends Thread
{
    // Reference to the IMAP4Client
    private IMAP4Client m_IMAP4Client;

    // Constructor that takes a reference to a IMAP4Client
    IMAP4FetchThread( IMAP4Client in_IMAP4Client )
    {
        m_IMAP4Client = in_IMAP4Client;
    }

    public void run()
    {
        try
        {
            System.out.println( "IMAP4FetchThread executing" );

            // Make sure that the client is not null
            if ( m_IMAP4Client == null )
            {
                throw new IllegalArgumentException();
            }
            
            // Fetch the body of message 1
            m_IMAP4Client.fetch( "1", "(BODY[])" );
            m_IMAP4Client.processResponses();
        }
        catch( Exception e )
        {
            System.out.println( "IMAP4FetchThread.java" );
            System.out.println( e );
        }
    }
}
