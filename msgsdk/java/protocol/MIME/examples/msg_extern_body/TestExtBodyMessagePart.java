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
  
import java.io.*;
import java.lang.*;
import java.util.*;
import netscape.messaging.mime.*;


/**
 * Example program for MIME Encoder API.
 * Demonstrates how to create messages with message/external-body content.
 */
public class TestExtBodyMessagePart
{
	private static final int MultiPart   = 10;
        private static final int MessagePart = 11;
	private static String tmpdir;

	public TestExtBodyMessagePart ()
        {
		String sep = System.getProperty ("path.separator");

           	if (sep.equals (";"))
                	tmpdir = "C:\\temp\\";
           	else
                	tmpdir = "/tmp/";
        }

	public static void setOtherHeaders (MIMEMessage mmsg)
        {
	  try {
	     mmsg.setHeader ("From", "Prasad Yendluri <prasad@netscape.com>");
	     mmsg.setHeader ("To", "Prasad Yendluri <prasad@netscape.com>");
	     mmsg.setHeader ("CC", "Prasad Yendluri <prasad@netscape.com>");
	     mmsg.setHeader ("Reply-To", "Prasad Yendluri <prasad@netscape.com>");
	     mmsg.setHeader ("X-MyTest-Header", "Test-Header-1");
	  }
	  catch (Exception e)
	  {
                System.out.println ("setOtherHeader> Caught exception: " + e);
	  }
        }


        public static void main (String[] args)
        {
           MIMEMessage mmsg;
           MIMEMessagePart mmsgpart;
	   FileOutputStream fos;
	   TestExtBodyMessagePart tm = new TestExtBodyMessagePart ();

           try
           {
		System.out.println ("Creating Message w/ External Body");

		// Create a message part 
		System.out.println ("Creating MessagePart");
		mmsgpart = new MIMEMessagePart ();

		// set the attributes of the messagePart
		mmsgpart.setContentSubType ("external-body"); 
		mmsgpart.setContentTypeParams ("access-type=local-file;name=\"e:\\share\\jpeg1.jpg\"");
		mmsgpart.setContentEncoding (MIMEBodyPart.E7BIT);

		// set ext-part headers
		mmsgpart.setExtBodyHeader ("Content-Type", "image/jpg");

		// Create a new Message with this messagePart.
		System.out.println ("Creating a new Message ");
		MIMEMessage msg = new MIMEMessage ();

		System.out.println ("Setting the above MessagePart as body of this Message.");
		msg.setBody (mmsgpart, false);

		System.out.println ("Setting headers in the new Message");
		msg.setHeader ("Subject", "Test Message w/ external MessagePart Body");

		System.out.println ("Setting other headers in the new Message");
		setOtherHeaders (msg);

		fos = new FileOutputStream (tmpdir + "MsgWithExtBodyMessagePart.out");
		System.out.println ("Writing message to output Stream");
		msg.putByteStream (fos);
           }
	   catch (Exception e)
           {
                System.out.println ("Caught exception: " + e);
           }

        } // main()

} // TestExtBodyMessagePart
