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


/*  Demo MIME encode Application using Netscape Messaging SDK MIME API.
 *  creates a multi-part message given a text buffer and a file.
 *  Note: In this example a default String (see textMsg) is used for 
 *        the text buffer for simplicity. 
 */

public class MIMEEncodeTest 
{
	///////////////////////////////////////////////////////////////////////////
	// Private data members.
	///////////////////////////////////////////////////////////////////////////

	private String 			filename = null;
	private String 			sender, To, subject; 
	public  String 			tmpdir = "/tmp/";
        private static String textMsg  =  "Hello this is a test Message";

  	/////////////////////////////////////////////////////////////////////
	// Constructor
  	/////////////////////////////////////////////////////////////////////
	public MIMEEncodeTest ()
	{
		String sep = System.getProperty ("path.separator");
		if (sep.equals (";"))
			tmpdir = "C:\\temp"; // its windoz
		else
			tmpdir = "/tmp";
	}


	/////////////////////////////////////////////////////////////////////
	// Build a Multi-Part MIME Message with the input parameters.
	// MIME encode the message and return an inputStream to encoded data.
	/////////////////////////////////////////////////////////////////////
	public InputStream endcodeit (String sender, String To, 
                               String subject, String filename, int encoding) throws MIMEException
        {
		try
		{
			return MultiPartMIMEMessage (sender, To, subject, filename, encoding);
		}
		catch ( Exception e )
                {
			throw new MIMEException (e.getMessage());
                }

	}

	//////////////////////////////////////////////////////////////////////
	// Build and return an InputStream for MIME-Multi-Part-Message
	//////////////////////////////////////////////////////////////////////
	public InputStream MultiPartMIMEMessage  (String sender, String To, 
						  String subject, String fullfilename, 
						  int encoding)
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

		// Create a new Multi-part MIMEMessage with the above text and the file passed
                mmsg = new MIMEMessage(bins, fullfilename, encoding);

		// set the user entered headers on the message
		mmsg.setHeader ("From", sender);  // example sender = prasad@netscape.com
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
		System.out.println("Exception!"+ e.getMessage());
		e.printStackTrace();
		return null;
            }
	}


  	/////////////////////////////////////////////////////////////////////
	// -----------------  MAIN -------------------------------
  	/////////////////////////////////////////////////////////////////////
	public static void main (String args[])
	{
		if (args.length != 4 && args.length != 5) 
		{
                     System.out.println("usage: java MIMEEncodeTest sender To subject <file-name> <B|Q>");
                     System.exit(0);
        	}

		MIMEEncodeTest testClient = new MIMEEncodeTest();

		testClient.sender = args[0];
		testClient.To = args[1];
		testClient.subject = args[2];
		testClient.filename = args[3];

		int encoding = -1;

		if (args.length > 5)
		{
			if (args[5].equalsIgnoreCase("B"))
			  encoding = MIMEBodyPart.BASE64;

			if (args[5].equalsIgnoreCase("Q"))
			  encoding = MIMEBodyPart.QP;
		}
			

		try
		{
			testClient.endcodeit (testClient.sender, 
				      		testClient.To, 
				      		testClient.subject, 
				      		testClient.filename, 
				      		encoding);
		}
		catch (Exception e)
		{
			System.out.println("Exception!"+ e.getMessage());
			e.printStackTrace();
		}
		
	}
}
