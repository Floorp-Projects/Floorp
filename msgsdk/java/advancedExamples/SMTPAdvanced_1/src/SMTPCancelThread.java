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
 * This file attempts to cancel a message by performing a disconnect()
 */

class SMTPCancelThread extends Thread
{
    private SMTPClient m_SMTPClient;

    // Constructor to initialize all the parameters
    SMTPCancelThread( SMTPClient in_SMTPClient )
    {
        m_SMTPClient = in_SMTPClient;
    }

    public void run()
    {
        try
        {
            // Make sure that the client is not null.
            if ( m_SMTPClient == null )
            {
                throw new IllegalArgumentException();
            }
            
            // Disconnect from the server.
            m_SMTPClient.disconnect();
        }
        catch( Exception e )
        {
            System.out.println( "SMTPCancelThread.java" );
            System.out.println( e );
        }
    }
}
