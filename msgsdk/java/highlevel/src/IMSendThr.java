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


package netscape.messaging.convenience;
import java.io.*;
import java.lang.*;
import java.util.*;
 
import netscape.messaging.smtp.*;
import netscape.messaging.mime.*;



/**
 * IMSendThr
 * @author Prasad Yendluri
 * Internet Message Store
 */
class IMSendThr extends Thread
{
	PipedInputStream msg_inputStream;
	String host;
        String sender;
        String recipients;
	Vector rejects;

	/**
         * Default constructor for IMSendThr
         */
	public IMSendThr (PipedInputStream is,
			  String host,
                          String sender,
                          String recipients,
			  Vector  rejects)
        {
		this.msg_inputStream = is;
		this.host = host;
		this.sender = sender;
		this.recipients = recipients;
		this.rejects = rejects;
        }


	public void run ()
	{
		String [] l_rejects;

		try
		{
			l_rejects = IMTransport.sendMessage2 (host, 
							     sender, 
							     recipients, 
							     msg_inputStream);

			if (l_rejects != null)
			for (int i = 0, len = l_rejects.length; i < len; i++)
			{
				rejects.addElement (l_rejects [i]);
			}
		}
		catch (Exception e)
		{
			e.printStackTrace();
			System.out.println("ImSendThr Exception! = " + e.getMessage());
		}
	}
}
