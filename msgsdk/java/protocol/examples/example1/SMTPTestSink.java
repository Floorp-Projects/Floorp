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

/*  Support class for the Demo Message Send Application using Netscape Messaging SDK.
 *  Implemnts SMTPTestSink.
 *  prasad@netscape.com
 */

public class SMTPTestSink extends SMTPSink
{
	public  boolean            	m_cmdStatus;
	public  boolean            	m_cmdStatusPending;
	public  boolean            	m_ehloStatus;
	public  boolean            	m_expnStatus;
	public  boolean            	m_debug;
	public  String 			m_thrCmd;
	public  int 			m_failCode;
	private static                  int FAILCODE = 400;

	public SMTPTestSink ()
	{
		m_cmdStatus = false;
		m_cmdStatusPending = true;
		m_ehloStatus = false;
	}

	public boolean tryProcessResponses(SMTPClient p_client) 
    	{
		m_cmdStatus = false;
		m_cmdStatusPending = false;
		m_failCode = 0;

          	while (true)
          	{
                	try
                	{
                        	p_client.processResponses();

				if (m_cmdStatusPending != true)
                        		return m_cmdStatus;
                	}
/*
                	catch (InterruptedIOException e)
                	{
			     if (m_debug)
                           	System.out.println ("*** processResponses()> InterruptedIOException"
                                         +  e.getMessage());	
                	}
                	catch (IOException e)
                	{
			     if (m_debug)
                           	System.out.println ("*** processResponses()> IOException"
                                         		+  e.getMessage());	
				 m_cmdStatus = false;
				 return m_cmdStatus;
			}
*/
                	catch (Exception e)
			{
			     if (m_debug)
                           	System.out.println ("*** processResponses()> Exception"
                                         		+  e.getMessage());	
				 m_cmdStatus = false;
				 return m_cmdStatus;
			}
         	}
    	} 

	/**
	* Notification for error()
	*/
	public void error (int respCode, StringBuffer respMsg) throws SMTPServerException
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
		m_failCode = respCode;

		if (m_cmdStatus != false)
			throw new SMTPServerException ("respCode = " + respCode +
							" server response = " + respCode);
	} // end error


	/**
	*  Notification for connect() response
	*/
	public void connect (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}


	/**
	* Notification for the response to the DATA command.
	*/
	public void data (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}

	/**
	*Notification for the response to the EHLO command.
	*/
	public void ehlo (int respCode, StringBuffer respMsg)
	{
		m_ehloStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = true;
	}

	/**
	*Notification for the completion of the EHLO command.
	*/
	public void ehloComplete() 
	{

		m_cmdStatusPending = false;
		m_cmdStatus = m_ehloStatus;
	}

	/**
	*Notification for the response to the EXPN command.
	*/
	public void expand (int respCode, StringBuffer respMsg)
	{
		m_expnStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = true;
	}

	/**
	*Notification for the completion of the EXPN command.
	*/
	public void expandComplete () 
	{
		m_cmdStatusPending = false;
		m_cmdStatus = m_expnStatus;
	}

	/**
	*Notification for the response to the MAIL FROM command.
	*/
	public void mailFrom (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}

	/**
	*Notification for the response to the NOOP command.
	*/
	public void noop (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}

	/**
	* Notification for the QUIT response
	*/
	public void quit (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}

	/**
	*Notification for the response to the RCPT TO command.
	*/
	public void rcptTo (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}

	/**
	*Notification for the response to the RSET command.
	*/
	public void reset (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}
	
	/**
	*Notification for the response to data sent to the server.
	*/
	public void send (int respCode, StringBuffer respMsg)
	{
		m_cmdStatus = respCode < FAILCODE ? true : false;
		m_cmdStatusPending = false;
	}
	
}
