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

package netscape.messaging.mime;
import java.lang.*;
import java.util.*;


/**
 * The Header class is used to represent the Header of the message.
 * @author Prasad Yendluri 
 */
public class Header implements Cloneable
{
	protected String 	m_headerName;
	private String 		m_headerValue;

	/**
 	 * Header constructor. 
	 * Creates a Header object given header name and value strings.
	 * @param name Name of the header.
	 * @param value Value of the header.
	 * @exception MIMEException If either name or value is null.
 	 */
	public Header (String name, String value) throws MIMEException
        {
		if (name == null || value == null)
		    throw new MIMEException ("Inavlid null <name> (or) <value>.");

		m_headerName  = name;
		m_headerValue = value;
        }


	/**
 	 * Header constructor. 
	 * Creates a Header object given a string in <name>:<value> format.
	 * Any '\r' and '\n' chars at the end of the line are stripped. 
	 * All the chars in inStr must map to ASCII.
	 * @param inStr. A String with header in the form <name>:<value>
	 * @exception MIMEException If inStr is null or not in <name>:<value> form.
 	 */
	public Header (String inStr) throws MIMEException
        {
		int sepIndex = 0, strLen;

		if (inStr == null)
		    throw new MIMEException ("String in <name>:<value> form expected.");

		inStr.trim();

		strLen = inStr.length();

		while (strLen > 0)
		{ 
			if ((inStr.lastIndexOf ('\n', strLen-1) > 0)  ||
			    (inStr.lastIndexOf ('\r', strLen-1) > 0))
			    strLen--;
			else
			     break;

                }

		if (strLen <= 3)
		    throw new MIMEException ("String in <name>:<value> form expected.");

		sepIndex = inStr.indexOf (':');

		if ((sepIndex == 0) || (sepIndex == strLen-1))
		    throw new MIMEException ("String in <name>:<value> form expected.");

		m_headerName   = inStr.substring (0, sepIndex);
		m_headerValue  = inStr.substring (sepIndex+1, strLen);
        }

	/**
 	 * Header constructor. 
	 * Creates a Header object given a byte array with header in the form <name>:<value>.
	 * Any '\r' and '\n' chars at the end of the line are stripped. 
	 * All the chars in line[] must be ASCII.
	 * @param line. A byte array with header in the form <name>:<value>
	 * @exception MIMEException If line is null or not in <name>:<value> form.
 	 */
	public Header (byte[] line) throws MIMEException
        {
		int linelen, sepIndex = 0;

		if (line == null)
		    throw new MIMEException ("byte[] in <name>:<value> form expected.");

		linelen = line.length;

		// Trim trailing spaces and CR LFs. 
		while ((linelen > 0) && ((line [linelen-1] == '\n') || 
			(line [linelen-1] == '\r') || (line [linelen-1] == ' ')))
			linelen--;

		// NOTE: No need to check for leading spaces as we trim Name and Value anyway.

		if (linelen <= 3)
		    throw new MIMEException ("byte[] in <name>:<value> form expected.");

		for (int i = 0; i < linelen; i++)
		{
			if (line [i] == ':')
			{
			     sepIndex = i;
			     break;
			}
		}

		if ((sepIndex == 0) || (sepIndex == linelen-1))
		    throw new MIMEException ("byte[] in <name>:<value> form expected.");

		m_headerName  = new String (line, 0, sepIndex);
		m_headerValue = new String (line, sepIndex+1, (linelen - (sepIndex +1)));
        }


	/**
         * Returns the name of this header.
         */
	public String getName ()
        {
                return (m_headerName.trim());
        }

	/**
         * Returns the value of this header.
         */
	public String getValue ()
        {
                return (m_headerValue.trim());
        }

	/**
    	* Sets the name of this header.
	* @param name Name of the header to set.
	*/
	protected void setName ( String name ) throws MIMEException
    	{
		if (name == null)
		    throw new MIMEException ("Inavlid null <name> (or) <value>.");
		    
		m_headerName  = name;
    	}

	/**
     	* Sets the value of this header.
	* @param value Value of the header to set.
     	*/
	protected void setValue ( String value ) throws MIMEException
    	{
		if (value == null)
		    throw new MIMEException ("Inavlid null <name> (or) <value>.");

		m_headerValue = value;
    	}

	/**
         * Returns the compelete header-line in the form <Name>: <Value> with CRLF termination.
         */
	public String getLine ()
        {
		String line;
		line = new String (m_headerName.trim() + ": " + m_headerValue.trim() + "\r\n");
                return (line);
        }

	/**
         * Clones an instance of the Header object.
         */
        public Object clone () 
        {
	     Header l_theClone;

	     try
             {
	         l_theClone = (Header) super.clone();
	     }
	     catch (CloneNotSupportedException e)
             {
	         return (null);
	     }

	     if (m_headerName != null)
		 l_theClone.m_headerName = m_headerName;
	     if (m_headerValue != null)
		 l_theClone.m_headerValue = m_headerValue;

	     return (l_theClone);
	}
}
