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
import java.util.Vector;

/**
*Notification sink for SMTP commands.
*@author derekt@netscape.com
*@version 1.0
*/

public class SMTPTestSink implements ISMTPSink
{
    Vector m_extendedList;
    
    SMTPTestSink()
    {
    }
    
    /**
    *Notification for the response to the BDAT command
    */
    public void bdat( int in_responseCode, StringBuffer in_responseMessage ) 
    {
    };

    /**
    *Notification for the response to the connection to the server.
    */
	public void connect( int in_responseCode, StringBuffer in_responseMessage )
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
    }

    /**
    *Notification for the response to the DATA command.
    */
	public void data( int in_responseCode, StringBuffer in_responseMessage )
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
	}

    /**
    *Notification for the response to the EHLO command.
    */
	public void ehlo( int in_responseCode, StringBuffer in_serverInfo )
	{
        if ( m_extendedList == null )
        {
            m_extendedList = new Vector();
        }

        m_extendedList.addElement( in_serverInfo.toString() );

	    System.out.println( (new Integer(in_responseCode)) + " " + (in_serverInfo.toString()) );
    }

    /**
    *Notification for the completion of the EHLO command.
    */
    public void ehloComplete() 
    {
	    System.out.println( "EHLO Complete" );
    }

    /**
    *Notification for the response to a server error.
    */
	public void error( int in_responseCode, StringBuffer in_errorMessage ) throws SMTPServerException
	{
	    throw new SMTPServerException( in_errorMessage.toString() );
	}

    /**
    *Notification for the response to the EXPN command.
    */
	public void expand( int in_responseCode, StringBuffer in_user ) 
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_user.toString()) );
	}

    /**
    *Notification for the completion of the EXPN command.
    */
    public void expandComplete() 
    {
	    System.out.println( "EXPAND Complete" );
    }

    /**
    *Notification for the response to the HELP command.
    */
	public void help( int in_responseCode, StringBuffer in_help ) 
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_help.toString()) );
	}

    /**
    *Notification for the completion of the HELP command.
    */
    public void helpComplete() 
    {
	    System.out.println( "HELP Complete" );
    }

    /**
    *Notification for the response to the MAIL FROM command.
    */
	public void mailFrom( int in_responseCode, StringBuffer in_responseMessage )
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
    }

    /**
    *Notification for the response to the NOOP command.
    */
	public void noop( int in_responseCode, StringBuffer in_responseMessage ) 
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
	}

    /**
    *Notification sink for the response to the QUIT command.
    */
	public void quit( int in_responseCode, StringBuffer in_responseMessage )
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
	}

    /**
    *Notification for the response to the RCPT TO command.
    */
	public void rcptTo( int in_responseCode, StringBuffer in_responseMessage )
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
	}

    /**
    *Notification for the response to the RSET command.
    */
	public void reset( int in_responseCode, StringBuffer in_responseMessage ) 
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
	}

    /**
    *Notification for the response to data sent to the server.
    */
	public void send( int in_responseCode, StringBuffer in_responseMessage ) 
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
    }

    /**
    *Notification for the response to sendCommand() method.
    */
	public void sendCommand( int in_responseCode, StringBuffer in_responseLine ) 
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseLine.toString()) );
    };

    /**
    *Notification for the completion of the extended command.
    */
    public void sendCommandComplete() 
    {
	    System.out.println( "SENDCOMMAND Complete" );
    };

    /**
    *Notification for the response to the VRFY command.
    */
	public void verify( int in_responseCode, StringBuffer in_responseMessage ) 
	{
	    System.out.println( (new Integer(in_responseCode)) + " " + (in_responseMessage.toString()) );
	};
}
