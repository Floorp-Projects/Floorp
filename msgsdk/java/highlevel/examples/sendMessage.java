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

import java.io.*;
import java.net.*;
import java.util.*;

import netscape.messaging.mime.*;
import netscape.messaging.convenience.*;


/*  Simple Send Message Application using Netscape Messaging SDK Convenience API.
 * 
 *  This examples shows how to send a message already in MIME canaonical form
 *  using the sendMessage() Convenience API.
 *
 *  Please other examples in this directory and Netscape Messaging SDK MIME API,
 *  for details on how to build and encode a MIME message.
 *
 *  Author: Prasad Yendluri <prasad@netscape.com>, Mar-1998.
 */

public class sendMessage 
{
	///////////////////////////////////////////////////////////////////////////
	// Private data members.
	///////////////////////////////////////////////////////////////////////////

	private String 			filename = null;
	private String 			filename_lastnode = null;
	private String 			host, sender, To, subject; 
	public  String 			tmpdir = "/tmp/";
        private static String textMsg  =  "Hello this is a test Message";

  	/////////////////////////////////////////////////////////////////////
	// Constructor
  	/////////////////////////////////////////////////////////////////////
	public sendMessage ()
	{
		String sep = System.getProperty ("path.separator");
		if (sep.equals (";"))
			tmpdir = "C:\\temp"; // its windoz
		else
			tmpdir = "/tmp";
	}


  	/////////////////////////////////////////////////////////////////////
	// -----------------  MAIN -------------------------------
  	/////////////////////////////////////////////////////////////////////
	public static void main (String args[])
	{
		if (args.length != 4)
		{
                     System.out.println("usage: java sendMessage host sender To <file-name> ");
                     System.exit(0);
        	}

		sendMessage testClient = new sendMessage();

		testClient.host = args[0];
		testClient.sender = args[1];
		testClient.To = args[2];
		testClient.filename = args[3];


		// Now send it!
		try
		{
                        // The file must contain a message encoded in MIME canonical form.
			FileInputStream fis = new FileInputStream (testClient.filename);

			String[] rejrecips = IMTransport.sendMessage (testClient.host,
							    	    testClient.sender,
							            testClient.To,
							            fis);
		}	
		catch (Exception e)
		{
			
                     System.out.println("IMTransport.sendMessage() threw exception! " + e.getMessage()+ "\n");
		     e.printStackTrace();
                     System.exit(0);
		}
		
	}
}
