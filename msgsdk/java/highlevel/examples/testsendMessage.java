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


/*  Demo Message Send Application using Netscape Messaging SDK Convenience API.
 *
 *  This example has two parts:
 *  
 *  Part 1> Shows how to build a MIME message given a file to send as a 
 *          MIME attachment. For this the  Messaging SDK MIME API is used.
 *
 *  Part 2> Shows how to send the MIME message built above using the Messaging SDK
 *          Convenience API sendMessage().
 *
 *  Author: Prasad Yendluri <prasad@netscape.com>, Mar-1998.
 */

public class testsendMessage 
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
	public testsendMessage ()
	{
		String sep = System.getProperty ("path.separator");
		if (sep.equals (";"))
			tmpdir = "C:\\temp"; // its windoz
		else
			tmpdir = "/tmp";
	}


	/////////////////////////////////////////////////////////////////////
	// Send the Message out. On success return false and true on failure.
	// On detection of any errors Pop up a Dialog Box asking user to check 
	// for the needed fields.
	/////////////////////////////////////////////////////////////////////
	public InputStream endcodeit (String sender, String To, 
                               String subject, String filename, int encoding)
        {
		try
		{
			return getMIMEMessage (sender, To, subject, filename, encoding);
		}
		catch ( Exception e )
                {
			System.out.println("endcodeit() Exception!"+ e.getMessage());
		     	e.printStackTrace();
			return null;
                }

	}

	/////////////////////////////////////////////////////////////////////////////////
	// Build a MIME Message and return an inputStream
	/////////////////////////////////////////////////////////////////////////////////

	public InputStream getMIMEMessage (String sender, String To,
					String subject, String fullfilename, int encoding)
	{
		return MultiPartMIMEMessage (sender, To, subject, fullfilename, encoding);
	}

	//////////////////////////////////////////////////////////////////////
	// Build and return an InputStream for MIME-Multi-Part-Message
	//////////////////////////////////////////////////////////////////////
	public InputStream MultiPartMIMEMessage  (String sender, String To, 
						  String subject, String fullfilename, int encoding)
	{
	    FileInputStream fis, fdis;
	    FileOutputStream fos;
	    ByteArrayInputStream bins;

	    MIMEMessage   mmsg;

	    try
	    {
		// >>>> Build and return a Multi-Part MIME Message <<<<

		// Get an inputStream to user entered text 
                bins = new ByteArrayInputStream (textMsg.getBytes());

		// Create a new Multi-Part MIMEMessage with the above text and the file passed
                mmsg = new MIMEMessage(bins, fullfilename, encoding);

		// set the user entered headers on the message
		mmsg.setHeader ("From", sender);
                mmsg.setHeader ("Reply-To", sender);
                mmsg.setHeader ("To", To);
 
                if (subject != null && subject.length() != 0 )
                        mmsg.setHeader ("Subject", subject);
 
		// Add any other desired headers.
                mmsg.setHeader ("X-MsgSdk-Header", "This is a Text Message");

		String mimefile = new String (tmpdir + "/SDKMIMEMsg.out");
                fos = new FileOutputStream (mimefile);

		// Encode the message in MIME Canaonical form for transmission
		mmsg.putByteStream (fos);

		// Return an inputStream to the encoded MIME message
		fis = new FileInputStream (mimefile);
		return fis;
	    }
	    catch (Exception e)
            {
		System.out.println("MultiPartMIMEMessage() Exception!"+ e.getMessage());
		e.printStackTrace();
		return null;
            }
	}


  	/////////////////////////////////////////////////////////////////////
	// -----------------  MAIN -------------------------------
  	/////////////////////////////////////////////////////////////////////
	public static void main (String args[])
	{
		if (args.length != 5 && args.length != 6) 
		{
                     System.out.println("usage: java testsendMessage host sender To subject <file-name> <B|Q>");
                     System.exit(0);
        	}

		testsendMessage testClient = new testsendMessage();

		testClient.host = args[0];
		testClient.sender = args[1];
		testClient.To = args[2];
		testClient.subject = args[3];
		testClient.filename = args[4];

		int encoding = -1;

		if (args.length > 5)
		{
			if (args[5].equalsIgnoreCase("B"))
			  encoding = MIMEBodyPart.BASE64;

			if (args[5].equalsIgnoreCase("Q"))
			  encoding = MIMEBodyPart.QP;
		}
			

		// Build and encode a MIME message with the given file-name encoding type and
                // other parameters (sender, subject, recipients).
		InputStream mimeMsgStream = testClient.endcodeit (testClient.sender, testClient.To, 
								  testClient.subject, testClient.filename, 
								  encoding);

		// Now send the MIME encoded message from above using the convenience method sendMessage.
		try
		{
			String[] rejrecips = IMTransport.sendMessage (testClient.host,
							    	    testClient.sender,
							            testClient.To,
							            mimeMsgStream);
		}	
		catch (Exception e)
		{
			
                     System.out.println("IMTransport.sendMessage() threw exception! " + e.getMessage()+ "\n");
		     e.printStackTrace();
                     System.exit(0);
		}
		
	}
}
