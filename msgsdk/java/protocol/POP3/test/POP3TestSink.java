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
  
import netscape.messaging.pop3.*;
import java.io.*;

/**
*Notification sink for SMTP commands.
*@author derekt@netscape.com
*@version 1.0
*/

public class POP3TestSink implements IPOP3Sink
{
    boolean m_fStatus;
    int m_octetCount;
    int m_messageCount;
    FileOutputStream m_outStream;

    POP3TestSink()
    {
        m_fStatus = true;
        m_octetCount = 0;
        m_messageCount = 0;
    }

    /**
    *Notification for the response to the connection to the server.
    */
    public void connect( StringBuffer in_responseMessage )
    {
	    System.out.println( in_responseMessage.toString() );
    }

    /**
    *Notification for the response to the DELE command.
    */
    public void delete( StringBuffer in_responseMessage )
    {
	    System.out.println( in_responseMessage.toString() );
    }

    /**
    *Notification for an error response.
    */
    public void error( StringBuffer in_responseMessage ) throws POP3ServerException
    {
	    throw new POP3ServerException();
    }


    /**
    *Notification for the start of the LIST command.
    */
    public void listStart()
    {
	    System.out.println( "LIST Start" );
    }

    /**
    *Notification for the response to the LIST command.
    */
    public void list( int in_messageNumber, int in_octetCount )
    {
	    System.out.println( (new Integer(in_messageNumber)) + " " +
	                        (new Integer(in_octetCount)) );
    }

    /**
    *Notification for the completion of the LIST command.
    */
    public void listComplete()
    {
	    System.out.println( "LIST Complete" );
    }

    /**
    *Notification for the response to the NOOP command.
    */
    public void noop()
    {
	    System.out.println( "NOOP" );
    }

    /**
    *Notification for the response to the PASS command.
    */
    public void pass( StringBuffer in_responseMessage )
    {
	    System.out.println( in_responseMessage.toString() );
    }

    /**
    *Notification for the response to the QUIT command.
    */
    public void quit( StringBuffer in_responseMessage )
    {
	    System.out.println( in_responseMessage.toString() );
    }

    /**
    *Notification for the response to the RSET command.
    */
    public void reset( StringBuffer in_responseMessage )
    {
	    System.out.println( in_responseMessage.toString() );
    }

    /**
    *Notification for the start of a message from the RETR command.
    */
    public void retrieveStart( int in_messageNumber, int in_octetCount )
    {
        try
        {
            m_outStream = new FileOutputStream( "retrieve.txt" );
        }
        catch ( IOException e )
        {
        }
	    System.out.println( new Integer(in_messageNumber) + " " + new Integer(in_octetCount) );
    }

    /**
    *Notification for raw message from the RETR command.
    */
    public void retrieve( byte[] in_messageData )
    {
        try
        {
            m_outStream.write( in_messageData );
        }
        catch ( IOException e )
        {
        }
	    System.out.println( new String( in_messageData ) );
    }

    /**
    *Notification for the completion of the RETR command.
    */
    public void retrieveComplete()
    {
        try
        {
            m_outStream.close();
        }
        catch ( IOException e )
        {
        }
	    System.out.println( "RETR Complete" );
    }

    /**
    *Notification for the start of the extended command.
    */
    public void sendCommandStart()
    {
	    System.out.println( "SENDCOMMAND Start" );
    }

    /**
    *Notification for the response to sendCommand() method.
    */
	public void sendCommand( StringBuffer in_line )
	{
	    System.out.println( in_line.toString() );
	}

    /**
    *Notification for the completion of the extended command.
    */
    public void sendCommandComplete()
    {
	    System.out.println( "SENDCOMMAND Complete" );
    }

    /**
    *Notification for the response to the STAT command.
    */
    public void stat( int in_messageCount, int in_octetCount )
    {
        m_octetCount = in_octetCount;
        m_messageCount = in_messageCount;

	    System.out.println( new Integer(m_messageCount) + " " + new Integer(m_octetCount) );
    }

    /**
    *Notification for the start of a message from the TOP command.
    */
    public void topStart( int in_messageNumber )
    {
	    System.out.println( "TOP Start " + new Integer(in_messageNumber) );
    }

    /**
    *Notification for a line of the message from the TOP command.
    */
    public void top( StringBuffer in_line )
    {
	    System.out.println( in_line.toString() );
    }

    /**
    *Notification for the completion of the TOP command.
    */
    public void topComplete()
    {
	    System.out.println( "TOP Complete" );
    }

    /**
    *Notification for the start of the UIDL command.
    */
    public void uidListStart()
    {
	    System.out.println( "UIDL Start" );
    }

    /**
    *Notification for the response to the UIDL command.
    */
    public void uidList( int in_messageNumber, StringBuffer in_uid )
    {
	    System.out.println( (new Integer(in_messageNumber)) + " " +
	                        in_uid.toString() );
    }

    /**
    *Notification for the completion of the UIDL command.
    */
    public void uidListComplete()
    {
	    System.out.println( "UIDL Complete" );
    }

    /**
    *Notification for the response to the USER command.
    */
    public void user( StringBuffer in_responseMessage )
    {
	    System.out.println( in_responseMessage.toString() );
    }

    /**
    *Notification for the start of the XAUTHLIST command.
    */
    public void xAuthListStart()
    {
	    System.out.println( "XAUTHLIST Start" );
    }

    /**
    *Notification for the response to the XAUTHLIST command.
    */
    public void xAuthList( int in_messageNumber,
                            int in_octetCount,
                            StringBuffer in_emailAddress )
    {
	    System.out.println( (new Integer(in_messageNumber)) + " " +
	                        (new Integer(in_octetCount)) + " " +
	                        in_emailAddress.toString() );
    }

    /**
    *Notification for the completion of the XAUTHLIST command.
    */
    public void xAuthListComplete()
    {
	    System.out.println( "XAUTHLIST Complete" );
    }

    /**
    *Notification for the response to the XSENDER command.
    */
    public void xSender( StringBuffer in_emailAddress )
    {
	    System.out.println( in_emailAddress.toString() );
    }
}
