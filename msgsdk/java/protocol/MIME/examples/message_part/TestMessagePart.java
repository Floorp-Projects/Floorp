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
 * Demonstrates how to create messages with MessagePart content
 */
public class TestMessagePart
{
	private static final int MultiPart   = 10;
        private static final int MessagePart = 11;
	private static String tmpdir;

	public TestMessagePart ()
        {
		String sep = System.getProperty ("path.separator");

           	if (sep.equals (";"))
                	tmpdir = "C:\\temp\\";
           	else
                	tmpdir = "/tmp/";
        }

	public static void setOtherHeaders (MIMEMessage mmsg)
        {
	    try 
	    {
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


	// create a text-part
        public MIMEBasicPart createTextPart (String inText)
        {
            MIMEBasicPart l_mbp;
 
            try
            {
                l_mbp = new MIMEBasicPart (MIMEBasicPart.TEXT);
                l_mbp.setContentSubType ("plain");
                l_mbp.setContentTypeParams ("charset=us-ascii");
                l_mbp.setContentID("Text_BasicPart_ContentID");
                //l_mbp.setContentEncoding(MIMEBodyPart.E7BIT);
                l_mbp.setContentDisposition(MIMEBodyPart.INLINE);
                //l_mbp.setContentDisposition(MIMEBodyPart.ATTACHMENT); // if you like
                l_mbp.setContentDispParams("TextPart Disp-Params");
                l_mbp.setContentDispParams("Some_Params");
                l_mbp.setHeader ("X-TextPartHeader-1", "myValue-1");
                l_mbp.setHeader ("X-TextPartHeader-2", "myValue-2");
                l_mbp.setBodyData (inText);
 
                return l_mbp;
            }
            catch (Exception e)
            {
                System.out.println ("createTextPart Caught exception: " + e);
                return null;
            }
        }

	// create a message 
	public MIMEMessage createMessage () throws MIMEException
	{
	    	MIMEMessage	 	l_msg;

		// create a blank message
		l_msg = new MIMEMessage();

		// create a text part
		MIMEBasicPart l_txtPart = createTextPart ("Hello this is a simple text part");
		
		// set the text part as body of the message
		l_msg.setBody (l_txtPart, false);    // false implies don't clone the part

		// set some headers on the message
		l_msg.setHeader ("Subject", "Inner Text Message");
		l_msg.setHeader ("From", "Prasad Yendluri <prasad@mcom.com>");
		l_msg.setHeader ("To", "Prasad Yendluri <prasad@mcom.com>");
		l_msg.setHeader ("Reply-To", "Prasad Yendluri <prasad@mcom.com>");
		l_msg.setHeader ("X-Hdr-Inner", "Some Value");

		// we are done. Return the created message
		return l_msg;
	}

	// create a message part 
	public MIMEMessagePart createMessagePart ()
	{
	    MIMEMessagePart 	l_msgpart;

            try
            {
		// first create the message that goes in the message part
		MIMEMessage l_msg = createMessage ();
		
		// Create a message part w/ above message
		l_msgpart = new MIMEMessagePart (l_msg);

		// set any attributes on the messagePart 
                l_msgpart.setContentDescription ("This is a forwarded message");
                l_msgpart.setContentDisposition (MIMEBodyPart.INLINE);
                l_msgpart.setContentEncoding (MIMEBodyPart.E7BIT); 	// default E7BIT
                //l_msgpart.setContentSubType ("rfc822"); 		// default rfc822

		return l_msgpart;

            }
	    catch (Exception e)
            {
                System.out.println ("createMsgPartExt Caught exception: " + e);
		return null;
            }
	}

        public static void main (String[] args)
        {
           MIMEMessage mmsg;
           MIMEMessagePart mmsgpart;
	   FileOutputStream fos;
	   TestMessagePart tm = new TestMessagePart ();

           try
           {
		// Create a message part 
		mmsgpart = tm.createMessagePart ();

		// Create a new Message with this messagePart.
		MIMEMessage msg = new MIMEMessage ();

		// set the above messagePart as body of the message
		msg.setBody (mmsgpart, false);

		// Set headers of the new Message
		msg.setHeader ("Subject", "Test Message w/ MessagePart Body");
		setOtherHeaders (msg);

		// MIME encode the created message
		fos = new FileOutputStream (tmpdir + "MsgWithMessagePart.out");
		msg.putByteStream (fos);
           }
	   catch (Exception e)
           {
                System.out.println ("Caught exception: " + e);
           }

        } // main()

} // TestMessagePart
