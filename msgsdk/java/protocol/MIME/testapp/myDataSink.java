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
import java.lang.*;
import java.util.*;
import netscape.messaging.mime.*;

public class myDataSink extends MIMEDataSink
{
    FileOutputStream out;
    FileOutputStream out2;
    int m_nChunkNo;
    String contentType;
    int basicPartCount, msgPartCount, msgCount, multiPartCount;
    
    public myDataSink()
    {
        super();

        try
        {
    	    out = new FileOutputStream ("output.log");
    	    out2 = new FileOutputStream ("list2.com");
    	}
    	catch (IOException e)
    	{
	    // Ignore if we can't create a log file
    	}
    	
    	m_nChunkNo = 0;
	msgCount=0;
	basicPartCount=0;
        msgPartCount=0; 
	multiPartCount=0;
   }
	
    public void log (String s)
    {
   	System.out.println (s);
        String ss = s + "\r\n";
	    
	if (out != null)
	try
	{
    	    out.write (ss.getBytes());
   	}
    	catch (IOException e)
    	{
	    // Ignore if we can't write to log file
    	}
    }


    public void header (Object callbackObject, byte[] name, byte[] value)
    {
	String s;
	
	if (callbackObject != null)
	{
	    s = (String) callbackObject;

            log (s + "> header() --> name = [" + new String(name) + "]  value = [" + 
		       new String (value) + "]");
	}
	else
	{
              log ("header() --> name = [" + new String(name) + "]  value = [" + 
		    new String (value) + "]");
	}
    }
     
    public void addHeader (Object callbackObject, byte[] name, byte[] value)
    {
	String s;
	
	if (callbackObject != null)
	{
	    s = (String) callbackObject;

            log (s + "> addHeader() --> name = [" + new String(name) + "]  value = [" + 
		       new String (value) + "]");
	}
	else
	{
              log ("addHeader() --> name = [" + new String(name) + "]  value = [" + 
		    new String (value) + "]");
	}
    }
     

    public void endMessageHeader (Object callbackObject)
    {
	String s;
	
	if (callbackObject != null)
	{
	    s = (String) callbackObject;

            log (s + " endMessageHeader()");
	}
	else
	{
                 log ("???? endMessageHeader()");
	}
    }

    public void contentType (Object callbackObject, byte[] bContentType)
    {
        contentType = new String (bContentType);
	String s = (String) callbackObject;
        log (s + ">  contentType() --> [" + contentType + "]");
    }
     
    public void contentSubType (Object callbackObject, byte[] contentSubType)
    {
	String s = (String) callbackObject;
        log (s + "> contentSubType() --> [" + new String (contentSubType) + "]");
    }
     
    public void contentTypeParams (Object callbackObject, byte[] contentTypeParams)
    {
	String s = (String) callbackObject;
        log (s+ "> contentTypeParams() --> [" + new String (contentTypeParams) + "]");
    }
     
    public void contentID (Object callbackObject, byte[] contentID)
    {
	String s = (String) callbackObject;
        log (s+ "> contentID() --> [" + new String (contentID) + "]");
    }
     
    public void contentMD5 (Object callbackObject, byte[] contentMD5)
    {
	String s = (String) callbackObject;
        log (s+ "> contentMD5() --> + [" + new String (contentMD5) + "]");
    }
     
    public void contentDisposition (Object callbackObject, int nContentDisposition)
    {
	String s = (String) callbackObject;
        log (s+ "> contentDisposition() --> [" + nContentDisposition + "]");
    }
     
    public void contentDispParams (Object callbackObject, byte[] contentDispParams)
    {
	String s = (String) callbackObject;
        log (s+ "> contentDispParams() --> [" + new String (contentDispParams) + "]");
    }
     
    public void contentDescription (Object callbackObject, byte[] contentDescription)
    {
	String s = (String) callbackObject;
        log (s+ "> contentDescription() --> [" + new String (contentDescription) + "]");
    }
     
    public void contentEncoding (Object callbackObject, int nContentEncoding)
    {
	String s = (String) callbackObject;
        log (s+ "> contentEncoding() --> [" + nContentEncoding + "]");
    }
     
    public Object startMessage ()
    {
	String s = new String ("message" + ++msgCount);

        log ("startMessage() " + msgCount);
        
        return s;
        //return null;
    }
     
    public void endMessage (Object callbackObject)
    {
	String s = (String) callbackObject;
        log (s+ "> endMessage()");
        
        try
        {
	    if (out != null)
            	out.close();
	    if (out2 != null)
            	out2.close();
    	}
    	catch (IOException e)
    	{
    	}
    }

    public Object startBasicPart ()
    {
	String s = new String ("BasicPart" + ++basicPartCount);
        log ("startBasicPart() " + basicPartCount);
        return s;
        //return null;
    }
     
    public void bodyData (Object callbackObject, InputStream input, int len)
    {
	String s = (String) callbackObject;

        log (s+ ">  bodyData() --> len = " + len + "   ChunkNo = " + m_nChunkNo++);

        if  (len == 0)
            return;

        //if (contentType.equalsIgnoreCase ("Application"))
        {
            try
            {
                for  (int b = input.read(), i = 0; i < len && b != -1; b = input.read(), i++)
                    out2.write (b);
            }
            catch (IOException e)
            {
            }
        }
    }
     
    public void endBasicPart (Object callbackObject)
    {
	String s = (String) callbackObject;

        log (s+ "> endBasicPart()");
    }

    public Object startMultiPart ()
    {
	String s = new String ("MultiPart" + ++multiPartCount);
        log ("startMultiPart() " + multiPartCount);
        
        return s;
        //return null;
    }
     
    public void boundary (Object callbackObject, byte[] boundary)
    {
	String s = (String) callbackObject;
        log (s+ "> boundary() --> [" + new String (boundary) + "]");
    }
     
    public void endMultiPart (Object callbackObject)
    {
	String s = (String) callbackObject;

        log (s+ "> endMultiPart()");
    }
     
    public Object startMessagePart ()
    {
	String s = new String ("MessagePart" + ++msgPartCount);

        log ("startMessagePart() " + msgPartCount);
        
        return s;
        //return null;
    }
     
    public void endMessagePart (Object callbackObject)
    {
	String s = (String) callbackObject;
        log (s+ "> endMessagePart()");
    }
     
}
