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
 * Demonstrates how to create messages with multipart content from scratch.
 * Please note that the classes MIMEMessage and MIMEMultiPart contain 
 * convenience methods to create a multi-part message with two parts, in a 
 * simple and single step. Please see other MIME examples provided for that
 * purpose. This example demonstrates how to build a multi-part message from 
 * ground up.
 */
public class TestMultiPartMessage
{
	private static String tmpdir;

	public TestMultiPartMessage ()
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

	// create a message part w/ external-body 
	public MIMEMessagePart createMsgPartExt ()
	{
	    MIMEMessagePart l_msgpart;

            try
            {
		// Create a message part 
		l_msgpart = new MIMEMessagePart ();

		// set the attributes of the messagePart
		l_msgpart.setContentSubType ("external-body"); 
		l_msgpart.setContentTypeParams ("access-type=local-file;name=\"e:\\share\\jpeg1.jpg\"");
		l_msgpart.setContentEncoding (MIMEBodyPart.E7BIT);

		// set ext-part headers
		l_msgpart.setExtBodyHeader ("Content-Type", "image/jpg");

		return l_msgpart;

            }
	    catch (Exception e)
            {
                System.out.println ("createMsgPartExt Caught exception: " + e);
		return null;
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
		l_mbp.setContentDispParams("Some_Description");
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


	// create a basic-part from the filename passed
	public MIMEBasicPart createFilePart (String filename, int encoding)
	{
	   MIMEBasicPart   l_filePart;
           FileInputStream l_fis;
           fileMIMEType    l_fmt;
		
           try
           {
		// get the mime-type info on the file passed
		l_fmt  = MIMEHelper.getFileMIMEType (filename);
		if (l_fmt == null)
			return null;

		// create a new basic-part for the file and set the attributes
		l_filePart = new MIMEBasicPart (l_fmt.content_type);
		l_filePart.setContentSubType (l_fmt.content_subtype);
		if (l_fmt.content_params != null)
                        l_filePart.setContentTypeParams (l_fmt.content_params);
                else
                        l_filePart.setContentTypeParams (new String ("name=" + l_fmt.file_shortname));

		if (encoding == -1)
                       l_filePart.setContentEncoding(l_fmt.mime_encoding);
                else
                       l_filePart.setContentEncoding (encoding); // need sanity check on encoding
	
		l_filePart.setContentDispParams (new String ("filename=" + l_fmt.file_shortname));
                l_filePart.setContentDescription (l_fmt.file_shortname);

		// open input-stream to the file
		l_fis = new FileInputStream (filename);

		// set the stream as body-data
		l_filePart.setBodyData (l_fis);

		// all done. Return the part
		return l_filePart;
           }
	   catch (Exception e)
           {
                System.out.println ("createTextPart Caught exception: " + e);
		return null;
           }
	}

        public static void main (String[] args)
        {
		MIMEMessage mmsg;
		MIMEMultiPart mmp;
		MIMEBasicPart bpText, bpFile;
		MIMEMessagePart mmsgpart;
		FileOutputStream fos;
		TestMultiPartMessage tm = new TestMultiPartMessage ();
		String filename = null;
		int encoding = -1;

		if (args.length < 1)
                {
                     System.out.println("usage: java TestMultiPartMessage <file-name> <B|Q>");
                     System.exit(0);
                }

		filename = args[0];

		if (args.length > 1)
                {
                        if (args[1].equalsIgnoreCase("B"))
                          encoding = MIMEBodyPart.BASE64;
 
                        if (args[1].equalsIgnoreCase("Q"))
                          encoding = MIMEBodyPart.QP;
                }

		try
		{
			// Create the MultiPart w/ mixed subtype
			mmp = new MIMEMultiPart ();
			mmp.setContentSubType ("mixed"); // You could use other subtypes (e.g."alternate") here
              		mmp.setContentTypeParams ("test-params");

			// create a text-part (basic-part) and add it to the multi-part
			bpText = tm.createTextPart ("Hello this a simple text part.");
			if (bpText != null)
				mmp.addBodyPart (bpText, false); // flase =>  don't clone it
	
			// create a basic-part from the file-name passed and add it to the multi-part
			bpFile = tm.createFilePart (filename, encoding);

			if (bpFile != null)
			{
				mmp.addBodyPart (bpFile, false); // flase =>  don't clone it
			}

			// Create a message part (external-body)
			mmsgpart = tm.createMsgPartExt ();

			if (bpFile != null)
				mmp.addBodyPart (mmsgpart, false); // flase =>  don't clone it

			// o.k we now have a multi-part w/ three parts
			// make a MIMEMessage out of it.
			mmsg = new MIMEMessage ();
			mmsg.setBody (mmp, false);

			// Set needed headers of the new Message
            		mmsg.setHeader ("Subject", "Test Message w/ MultiPart content");
 
                	// Set other headers of the Message
                	setOtherHeaders (mmsg);

			// Now the message is completely built!
			// Encode it in MIME canonical form
                	fos = new FileOutputStream (tmpdir + "MsgWithMultiPart.out");
                	mmsg.putByteStream (fos);

		}
		catch (Exception e)
		{
			System.out.println ("Caught exception: " + e);
		}

        } // main()

} // TestMultiPartMessage
