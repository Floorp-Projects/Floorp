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

/*
 *   This example programs demonstrates the use of Static and
 *   Dynamic MIME Parser.
 */

import java.awt.*;
import java.sql.*;
import java.util.Vector;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;
import netscape.messaging.mime.*;


public class testAppStr 
{
    byte b[];
    FileOutputStream logfile;
    int m_nFileNo;
    
	public testAppStr()
	{
		b = new byte[100000];
    		m_nFileNo=0;
		
		try
		{
       	    		logfile = new FileOutputStream ("testapp.log");
    		}
    		catch (IOException e)
    		{
    	    		System.out.println (e.getMessage());
    		}
	}
	
        void showObject (Object o)
        {
            InputStream in = null;
    
            if  (o == null)
                return;
                
            if  (o instanceof MIMEMessage)
            {
        	    output ("--------------- Start Mime Message ---------------\n");
        		    
        	    MIMEMessage m = (MIMEMessage) o;
        
        	    try
        	    {
        	        Header[] h = m.getAllHeaders();
        	            
        		if (h != null)
        	        for  (int i=0; h != null && i < h.length; i++)
        	        {
        	                output ("message header = [" + h[i].getLine());
        	        }
        
                        output ("* message body type = " + m.getBodyType() + "\n");
                        output ("* message content type = " + m.getContentType() + "\n");
                        output ("* message content subtype = " + m.getContentSubType() + "\n");
                        output ("* message content type param = " + m.getContentTypeParams() + "\n");
        
        		showObject (m.getBody(false));
            		output ("---------------- End Mime Message -----------------\n");
                    }
                    catch (MIMEException e)
                    {
                        output (e.getMessage());
                    }
             }
             else if  (o instanceof MIMEBasicPart)
             {
        	    output ("------------------- Start Mime BasicPart -------------------\n");
        
                    try
                    {
            		MIMEBasicPart m = (MIMEBasicPart) o;
                        in = m.getBodyData();
        
                        if  (in != null)
                        {
                            int len = 0;
                            
                            if  (m.getMessageDataLen() < 100000)
                       		    len = in.read (b);
        
            	            Header[] h = m.getAllHeaders();
            	            
        		    if (h != null)
            	            for  (int i=0; h != null && i < h.length; i++)
            	            {
            	                output ("basic header = [" + h[i].getLine() );
            	            }
        
                            output ("* basicpart content type = " + m.getContentType() + "\n");
                            output ("* basicpart content subtype = " + m.getContentSubType() + "\n");
                            output ("* basicpart content type param = " + m.getContentTypeParams() + "\n");
                            output ("* basicpart content ID = " + m.getContentID() + "\n");
                            output ("* basicpart content Disposition = " + m.getContentDisposition() + "\n");
                            output ("* basicpart content Disposition params = " + m.getContentDispParams() + "\n");
                            output ("* basicpart content Description = " + m.getContentDescription() + "\n");
                            output ("* basicpart content MD5 = " + m.getContentMD5() + "\n");
                            output ("* basicpart getbodysize() = " + m.getSize() + "\n");
                            output ("* basicpart content encoding = " + m.getContentEncoding() + "\n");
        
                            output (">>>>>>>>>>>>>>>>>>>> start data >>>>>>>>>>>>>>>>>>>\n");
            
                            // base64
                   	    if  (m.getContentEncoding() == 0)
                   	            output ("[BASE64 BINARY DATA]");
                            // QP
                   	    else if  (m.getContentEncoding() == 1)
                   	            output ("[QP DATA]");
                   	    // text
                   	    else
                                output (b, len);
                            
                            output ("\n>>>>>>>>>>>>>>>>>>>> end data >>>>>>>>>>>>>>>>>>>>\n");
        
                            // base64
                   	    if  (m.getContentEncoding() == 1 && m.getMessageDataLen() < 100000)
                   	    {
                                FileOutputStream out = new FileOutputStream ("bodydata" + m_nFileNo++ + ".out");
                       	        out.write (b, 0, m.getMessageDataLen());
                       	        out.close();
                            }
                       	    
                            in.close();
                        }
                    }
                    catch (MIMEException e)
                    {
                        output (e.getMessage());
                    }
                    catch (IOException e)
                    {
                        output (e.getMessage());
                    }
        
        	    output ("------------------- End Mime BasicPart -------------------\n");
        	}
        	else if  (o instanceof MIMEMultiPart)
        	{
        	    output ("------------------- Start Mime MultiPart  -------------------\n");
        
        	    MIMEMultiPart m = (MIMEMultiPart) o;
        	    int count = m.getBodyPartCount();
        		    
                    // debug
                    output ("* multipart content type = " + m.getContentType() + "\n");
                    output ("* multipart content subtype = " + m.getContentSubType() + "\n");
                    output ("* multipart content type param = " + m.getContentTypeParams() + "\n");
                    output ("* multipart content ID = " + m.getContentID() + "\n");
                    output ("* multipart content Disposition = " + m.getContentDisposition() + "\n");
                    output ("* multipart content Disposition params = " + m.getContentDispParams() + "\n");
                    output ("* multipart content Description = " + m.getContentDescription() + "\n");
        
        	    if (count > 0)
        	    for  (int i = 0; i < count; i++)
        	    {
        		 try
        		 {
            		        Object part = m.getBodyPart(i, false);
            		        showObject (part);
                    	  }
                    	  catch (MIMEException e)
                    	  {
                            	output (e.getMessage());
                    	  }
        	    }
        
        	    output ("------------------- End Mime MultiPart ------------------\n");
        	}
        		
        	else if  (o instanceof MIMEMessagePart)
        	{
        	   output ("------------------- Start Mime MessagePart ----------------\n");
        
        	   MIMEMessagePart m = (MIMEMessagePart) o;
        
        	   try
        	   {
        		 MIMEMessage part = m.getMessage(false);
        
        	         Header[] h = m.getAllHeaders();
        	            
        		 if (h != null)
        	         for  (int i=0; h != null && i < h.length; i++)
        	         {
        	                output ("messagepart header = [" + h[i].getLine() );
        	         }
        
                        output ("* messagepart content type = " + m.getContentType() + "\n");
                        output ("* messagepart content subtype = " + m.getContentSubType() + "\n");
                        output ("* messagepart content type param = " + m.getContentTypeParams() + "\n");
                        output ("* messagepart content ID = " + m.getContentID() + "\n");
                        output ("* messagepart content Disposition = " + m.getContentDisposition() + "\n");
                        output ("* messagepart content Disposition params = " + m.getContentDispParams() + "\n");
                        output ("* messagepart content Description = " + m.getContentDescription() + "\n");
                        output ("* messagepart content encoding = " + m.getContentEncoding() + "\n");
        
        		showObject (part);
                    }
                    catch (MIMEException e)
                    {
                        output (e.getMessage());
                    }
        
        	    output ("------------------- End Mime MessagePart  -----------------\n");
        	}            
        }
    

	void parseFileMessage (String filename, boolean bDynamic)
	{
	    if (!bDynamic) // perform Static Parsing
	    {
		    MIMEParser p = new MIMEParser();
		    
		    output ("============== Begin Static Parsing " + filename + " ============\n");
	
		    if  (filename != null && p != null)
		    {
			try
	            	{
	                	FileInputStream input = new FileInputStream (filename);
	                	showObject (p.parseEntireMessage (input));
	                	input.close();
	            	}
	            	catch (IOException e)
	            	{
	                	System.out.println (e.getMessage());
	            	}
	            	catch (MIMEException e)
	            	{
	                	System.out.println (e.getMessage());
	            	}
	            }
	
		    output ("\n=========== Parsing Complete " + filename + " =============\n\n");

	    } 
	    else  // perform Dynamic Parsing
	    { 
		try
	        {
	    	    myDataSink dataSink = new myDataSink();
	            //MIMEDynamicParser p =  new MIMEDynamicParser (dataSink, true, true);
	            MIMEDynamicParser p =  new MIMEDynamicParser (dataSink);
	            FileInputStream input = new FileInputStream (filename);
	            byte[] buffer = new byte[256];
	            int bytesRead;
	
		    output ("============== Begin Dynamic Parsing " + filename + " ============\n");

	            p.beginParse();

	            p.parse (input);
	
	            //bytesRead = input.read (buffer); 
	            //while (bytesRead > 0)
	            //{
	            //   p.parse (buffer);
	            //	bytesRead = input.read (buffer); 
	            //}
	            
	            p.endParse();
	            input.close();
	            
	            //showObject (dataSink.m_mimeMessage);

		    output ("\n=========== Parsing Complete " + filename + " =============\n\n");
	        }
	        catch (MIMEException e)
	        {
	            System.out.println (e.getMessage());
	        }
	        catch (IOException e)
	        {
	            System.out.println (e.getMessage());
	        }

	    } // bDynamic 

        } // parseFileMessage


	void output (String s)
	{
	    try
	    {
    	    	logfile.write (s.getBytes());
    	    	System.out.println (s);
    	    }
            catch (IOException e)
            {
                System.out.println (e.getMessage());
            }
	}


	void output (byte b[], int len)
	{
	    try
	    {
    	    	logfile.write (b, 0, len);
	    	System.out.println (new String (b, 0, len));
       	    }
            catch (IOException e)
            {
            	System.out.println (e.getMessage());
            }
	}

	/************ MIAN ******************/
	static public void main(String args[]) 
	{
		boolean bDynamic=true;  // Dynamic Parsing by Default
		String filename;

		if (args.length < 1)
                {
                     System.out.println("usage: java testAppStr <file-name> [D]");
                     System.out.println("example: java testAppStr mime1.txt");
                     System.exit(0);
                }

		filename = args[0];
		
		if (args.length > 1 && !args[1].equalsIgnoreCase("D"))
		{
			bDynamic = false; // Switch to Static Parsing
		}

	    	log.init();
	    	log.timestampOff();
	    	testAppStr ta = new testAppStr();

		// parse the message in the file
		ta.parseFileMessage (filename, bDynamic);
	}
}
