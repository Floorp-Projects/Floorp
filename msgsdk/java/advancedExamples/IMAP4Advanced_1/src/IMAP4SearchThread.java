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
 * This file searches the body for "World"
 */

class IMAP4SearchThread extends Thread
{
    // Reference to the IMAP4Client
    private IMAP4Client m_IMAP4Client;

    // Constructor that takes a reference to a IMAP4Client
    IMAP4SearchThread( IMAP4Client in_IMAP4Client )
    {
        m_IMAP4Client = in_IMAP4Client;
    }

    public void run()
    {
        try
        {
            System.out.println( "IMAP4SearchThread executing" );

            // Make sure that the client is not null
            if ( m_IMAP4Client == null )
            {
                throw new IllegalArgumentException();
            }
            
            // Perform the search
            m_IMAP4Client.search( "BODY \"World\"" );
            m_IMAP4Client.processResponses();
        }
        catch( Exception e )
        {
            System.out.println( "IMAP4SearchThread.java" );
            System.out.println( e );
        }
    }
}
