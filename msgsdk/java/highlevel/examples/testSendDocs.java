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
 *  This example demonstrates the use of the sendDocuments() Convenience API.
 *  This API privides simple interface for sending files / documents as attachments
 *  in a MIME message over SMTP transport.
 *
 *  Author: Prasad Yendluri <prasad@netscape.com>, Mar-1998.
 */

public class testSendDocs 
{
	///////////////////////////////////////////////////////////////////////////
	// Private data members.
	///////////////////////////////////////////////////////////////////////////

	private String 			filename1 = null, filename2 = null;
	private String 			filename_lastnode = null;
	private String 			host, sender, To, subject; 
	private  String 		tmpdir = "/tmp/";
	private boolean			fUseTempFiles;
        private static String textMsg  =  "Hello this is a test Message";

  	/////////////////////////////////////////////////////////////////////
	// Constructor
  	/////////////////////////////////////////////////////////////////////
	public testSendDocs ()
	{
/*
		String sep = System.getProperty ("path.separator");
		if (sep.equals (";"))
			tmpdir = "C:\\temp"; // its windoz
		else
			tmpdir = "/tmp";
*/
	}


  	/////////////////////////////////////////////////////////////////////
	// -----------------  MAIN -------------------------------
  	/////////////////////////////////////////////////////////////////////
	public static void main (String args[])
	{
		if (args.length < 5 || args.length > 7)
		{
                     System.out.println("usage: java testSendDocs host sender To subject <file-name> [<file-name>] [T|F]");
                     System.exit(0);
        	}

		testSendDocs testClient = new testSendDocs();

		testClient.host = args[0];
		testClient.sender = args[1];
		testClient.To = args[2];
		testClient.subject = args[3];
		testClient.filename1 = args[4];
		testClient.fUseTempFiles = false;

		if (args.length > 5)
		{
			if (args[5].equalsIgnoreCase("T"))
				testClient.fUseTempFiles = true;
			else if (args[5].equalsIgnoreCase("F"))
				testClient.fUseTempFiles = false;
			else
				testClient.filename2 = args[5];
		}

		if (args.length > 6)
		{
			if (args[6].equalsIgnoreCase("T"))
				testClient.fUseTempFiles = true;
			else if (args[6].equalsIgnoreCase("F"))
				testClient.fUseTempFiles = false;
		}

		int encoding = -1;
		
		// Now send the specified files to the specified recipients.
                // The sendDocuments() method builds and encodes a MIME message 
                // with the specified attachments and other parameters and sends
                // the message over SMTP transport.
		try
		{
			IMAttachment [] attchments = new IMAttachment [2];

			attchments [0] = new IMAttachment (testClient.filename1, -1, null, null, -1, -1);

			if (testClient.filename2 != null)
			{
				attchments [1] = new IMAttachment (testClient.filename2, -1, null, null, -1, -1);
			}
			
                     	System.out.println("fUseTempFiles = " + testClient.fUseTempFiles);

			String[] rejrecips = IMTransport.sendDocuments (testClient.host,
							    	    testClient.sender,
							            testClient.To,
							            testClient.subject,
								    null,
								    null,
							            attchments,
								    testClient.fUseTempFiles);
		}	
		catch (Exception e)
		{
			
                     System.out.println("IMTransport.sendDocuments() threw exception! " + e.getMessage()+ "\n");
		     e.printStackTrace();
                     System.exit(0);
		}
		
	}
}
