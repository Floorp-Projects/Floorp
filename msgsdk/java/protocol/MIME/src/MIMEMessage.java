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
import java.io.*;
import java.lang.*;
import java.util.*;
import netscape.messaging.mime.MIMEParser;
import netscape.messaging.mime.MIMEDataSink;

/**
 * The MIMEMessage class represents the MIME message.
 * A MIME Message is made up of a set of headers and message content.
 * The message content itself comprises one of the MIMEBody types:
 * MIMEBasicPart, MIMEMultiPart, and MIMEMessagePart.
 * @author Prasad Yendluri
 */
public class MIMEMessage implements Cloneable
{
	/**
         * BodyPart is a MIMEBasicPart object.
         */
	public static final int BASICPART	= 0;
	/**
         * BodyPart is a MIMEMultiPart object.
         */
        public static final int MULTIPART	= 1;
	/**
         * BodyPart is a MIMEMessagePart object.
         */
        public static final int MESSAGEPART	= 2;

//===========================================================================
//
//              Internal members not visible at the API
//
//===========================================================================

        private static final int UNINITIALIZED = -1;

	private static final byte[] CRLF = "\r\n".getBytes();
        private static final byte[] LF = "\n".getBytes();

        private int             m_contentTransferEncoding;
        protected int             m_parsedPart;
        private boolean         m_fplaceTwo;

        // The key to the hashTable below is the name of the header field.
        // To make sure we handle case differences in the header-name, we
        // must convert the header name to lower-case always prior to the
        // hash look-up. The entries added to the hash table are objects of
        // class Header.
        private Hashtable       m_822HeadersTable;
	private Vector          m_repeatHdrs;

	// Body
        private int             m_bodyType;     // one of BASICPART, MULTIPART, MESSAGEPART
        private MIMEBodyPart    m_theBody;      // must be one of BASICPART, MULTIPART, MESSAGEPART

        // callback support
        MIMEParser m_parser;
        MIMEDataSink m_dataSink;
        private Object          m_UserObject;


//===========================================================================
//
//                              CONSTRUCTORS
//
//===========================================================================


	/**
         * Default constructor for the MIMEMessage class.
         */
	public MIMEMessage ()
        {
            m_822HeadersTable  = new Hashtable();

            m_parsedPart = 0;
            m_contentTransferEncoding =  UNINITIALIZED;
    		m_bodyType   = UNINITIALIZED;
	    	m_theBody    = null;
	    	m_parser     = null;
	    	m_dataSink   = null;

        }

	/**
         * Constructs a MIMEMessage object given a set of RFC822 headers.
	 * @param headers List of rfc822 headers to set in the message.
	 * @exception MIMEException If the headers are malformed.
         */
	public MIMEMessage (Header[] headers) throws MIMEException
        {
                m_822HeadersTable  = new Hashtable();

                m_parsedPart = 0;
                m_contentTransferEncoding =  UNINITIALIZED;
		m_bodyType   = UNINITIALIZED;
		m_theBody    = null;

		if (headers != null)
                for (int i = 0, len = headers.length; i < len; i++)
                {
                    try
                    {
			setHeader (headers[i].getName(), headers[i].getValue());
                    }
                    catch (Exception e)
                    {
                        throw new MIMEException (e.getMessage());
                    }
                } // for
        }


	/**
	 * Constructs a (multi-part) MIMEMessage with given text and file.
	 * If filename parameter is null, builds a text message with textIS. Encoding will be E7BIT.
	 * If textIS parameter is null, builds a message with only the filename file as an attachment.
	 * If both textIS and filename parameters are null, an exception is thrown.
	 * @param textIS InputStream to text that becomes text part of the message. Can be null.
	 * @param filename Full name of file that becomes a part of the message. Can be null.
	 * @param encoding Encoding for the file attachment. To pick default value, pass -1.
         * @see MIMEBodyPart#BASE64
         * @see MIMEBodyPart#QP
         * @see MIMEBodyPart#BINARY
         * @see MIMEBodyPart#E7BIT
         * @see MIMEBodyPart#E8BIT
	 * @exception MIMEException If both textIS and filename are null or encoding is invalid
	 * @exception FileNotFoundException If filename file does not exist
         * @exception SecurityException If filename file can not be accessed
         * @exception IOExcepton On IO Errors on filename or textIS.
         */
	public MIMEMessage (InputStream textIS, String filename, int encoding) throws
				MIMEException, FileNotFoundException, SecurityException, IOException
        {
		MIMEBasicPart 	l_txtPart;
		MIMEBasicPart 	l_filePart;
		FileInputStream l_fis;
		fileMIMEType	l_fmt;

		// Message initialization
                m_822HeadersTable  = new Hashtable();
                m_parsedPart = 0;
                m_contentTransferEncoding =  UNINITIALIZED;
		m_bodyType   = UNINITIALIZED;
		m_theBody    = null;

		if (textIS == null && filename == null)
                 	throw new MIMEException ("Invalid null filename:" + filename);

		if ((encoding != -1) && (encoding < MIMEBodyPart.BASE64 || encoding > MIMEBodyPart.E8BIT))
                 	throw new MIMEException ("Invalid MIME encoding: " + encoding);


		if (textIS != null && filename != null)
		{
			MIMEMultiPart l_mmp = new MIMEMultiPart (textIS, filename, encoding);
			this.setBody (l_mmp, false);
		}
		else if (textIS != null) 	// text-part only!
		{
			l_txtPart = new MIMEBasicPart (MIMEBasicPart.TEXT);

			l_txtPart.setContentSubType ("plain");
                	l_txtPart.setContentTypeParams ("charset=us-ascii");
                	l_txtPart.setContentEncoding(MIMEBodyPart.E7BIT);
                	//l_txtPart.setContentDisposition(MIMEBodyPart.INLINE);

                	l_txtPart.setBodyData (textIS);

			this.setBody (l_txtPart, false);
		}
		else if (filename != null) // file-part only!
		{
			l_fis = new FileInputStream (filename);

			// The file-part

			l_fmt  = MIMEHelper.getFileMIMEType (filename);

			if (l_fmt == null)
                 		throw new MIMEException ("Can't determine MIME info for file: " + filename);

			l_filePart = new MIMEBasicPart (l_fmt.content_type);

                	l_filePart.setContentSubType (l_fmt.content_subtype);

                	if (l_fmt.content_params != null)
                        	l_filePart.setContentTypeParams (l_fmt.content_params);
                	else
                        	l_filePart.setContentTypeParams (new String ("name=" + filename));

			if (encoding == -1)
                		l_filePart.setContentEncoding(l_fmt.mime_encoding);
			else
                		l_filePart.setContentEncoding (encoding);

			if (l_fmt.content_type == MIMEBasicPart.TEXT);
                      		//l_filePart.setContentDisposition (MIMEBodyPart.INLINE);
			else
			{
                		//l_filePart.setContentDisposition (MIMEBodyPart.ATTACHMENT);
                		l_filePart.setContentDispParams (new String ("filename=" + l_fmt.file_shortname));
                		l_filePart.setContentDescription (l_fmt.file_shortname);
			}

            		// set body-data of this part
            		l_filePart.setBodyData (l_fis);
			this.setBody (l_filePart, false);

		} // filePart

	} // MIMEMessage()



//===========================================================================
//
//                        All the Methods Specific to this Class
//
//===========================================================================


	/**
	* Sets any RFC-822 headers including X-headers. Overwrites the existing
	* value if header exists already.
	* @param name Name of the header field.
	* @param value Value of the header field to be added.
	* @exception MIMEException If either name or value are NULL
	* @see MIMEHelper#encodeHeader
	*/
	public void setHeader (String name, String value) throws MIMEException
	{
             if ((name == null) || (value == null))
                 throw new MIMEException ("Invalid  NULL Header name or value");

             String l_hashKey = name.toLowerCase();  // Keys are always lower case

             Header l_hdr = new Header (name, value);
             m_822HeadersTable.put (l_hashKey, l_hdr);
	}



	/**
	* Appends the value to an existing RFC822 header. Creates new header if
	* one does not exist already.
	* @param name Name of the header field.
	* @param value Value of the header field to be added.
	* @exception MIMEException If either name or value are NULL
	* @see MIMEHelper#encodeHeader
	*/
	public void addHeader (String name, String value) throws MIMEException
        {
             if ((name == null) || (value == null))
                 throw new MIMEException ("Invalid  NULL Header name or value");

	     if (m_fplaceTwo == true)
	     {
		 addHeader2 (name, value);
		 return;
	     }

             String l_hashKey = name.toLowerCase();  // Keys are always lower case
             Header l_hdr = (Header) m_822HeadersTable.get (l_hashKey);
             if (l_hdr == null)
	     {
		// Add a new one.
                Header l_newhdr = new Header (name, value);
                m_822HeadersTable.put (l_hashKey, l_newhdr);
             }
             else
	     {
		 // Append value to existing one.
		 StringBuffer l_oldvalue = new StringBuffer (l_hdr.getValue());
		 l_oldvalue.append (value);

		 Header l_newhdr = new Header (name, l_oldvalue.toString());
                 m_822HeadersTable.put (l_hashKey, l_newhdr);
	     }
        }

	private void addHeader2 (String name, String value) throws MIMEException
	{
		Header l_oldHdr, l_newhdr;
		int idx = m_repeatHdrs.size() -1;

		l_oldHdr = (Header) m_repeatHdrs.elementAt (idx);

		if (!(l_oldHdr.m_headerName.equalsIgnoreCase (name)))
		{
			l_newhdr = new Header (name, value);
                 	m_repeatHdrs.addElement (l_newhdr);
                 	m_fplaceTwo = true;
		}
		else
		{
			m_repeatHdrs.removeElementAt (idx);

			StringBuffer l_oldvalue = new StringBuffer (l_oldHdr.getValue());
			l_oldvalue.append (value);

			l_newhdr = new Header (name, l_oldvalue.toString());
                 	m_repeatHdrs.addElement (l_newhdr);
                 	m_fplaceTwo = true;
		}
	}

	// Adds a potential repeat header to the message
	protected void addRHeader (String name, String value) throws MIMEException
        {
             if ((name == null) || (value == null))
                 throw new MIMEException ("Invalid  NULL Header name or value");

             String l_hashKey = name.toLowerCase();  // Keys are always lower case
             Header l_hdr = (Header) m_822HeadersTable.get (l_hashKey);

             if (l_hdr == null)
	     {
		// Add a new one.
                Header l_newhdr = new Header (name, value);
                m_822HeadersTable.put (l_hashKey, l_newhdr);
		m_fplaceTwo = false;
             }
             else
	     {
		 // Its a repeat header!
		 if (m_repeatHdrs == null)
		 	m_repeatHdrs = new Vector();

                 Header l_newhdr = new Header (name, value);
		 m_repeatHdrs.addElement (l_newhdr);
		 m_fplaceTwo = true;
	     }
	}

	/**
         * Returns the value of the requested header. NULL if the header is not present.
         * @param name Name of the header field.
	 * @exception MIMEException If name is NULL
	 * @see MIMEHelper#decodeHeader
         */
	public String getHeader (String name) throws MIMEException
        {
             if (name == null)
                 throw new MIMEException ("Invalid  NULL Header name");

             String l_hashKey = name.toLowerCase();  // Keys are always lower case
             Header l_hdr = (Header) m_822HeadersTable.get (l_hashKey);

             if (l_hdr != null)
                return (l_hdr.getValue());
             else
                  return (null);
        }

	/**
         * Deletes the requested header.  Ignores if the header specified does not exist.
         * @param name Name of the header field to delete.
	 * @exception MIMEException if name is null
         */
	public void deleteHeader (String name) throws MIMEException
        {
             if (name == null)
                 throw new MIMEException ("Invalid  NULL Header name");

             String l_hashKey = name.toLowerCase();  // Keys are always lower case
             m_822HeadersTable.remove (l_hashKey);
        }

	/**
         * Returns all the RFC-822 headers in the Message as an array of Header objects.
         * Content-Type is not returned by this Method, as separate methods to get
	 * Content primary type, sub-type and parameters exist.
	 * @exception MIMEException If no headers exist.
	 * @see MIMEHelper#decodeHeader
	 * @see getContentType
	 * @see getContentSubType
	 * @see getContentTypeParams
         */
	public Header[] getAllHeaders () throws MIMEException
        {
                int l_numElements = m_822HeadersTable.size();
                int i = 0, l_repeats = 0;
                Header[] hdrs = null;

                if (l_numElements <= 0)
                   return (null);

		if (m_repeatHdrs != null)
			l_repeats = m_repeatHdrs.size();

                hdrs = new Header[l_numElements + l_repeats];
                Enumeration he = m_822HeadersTable.elements();

                while (he.hasMoreElements())
                {
			hdrs[i++] =  (Header) he.nextElement();
                }

		for (int j = 0; j < l_repeats; j++)
			hdrs[i++] = (Header) m_repeatHdrs.elementAt (j);

                return (hdrs);
        }

	/**
         * Returns the type of the body of the Message.
	 * @exception MIMEException If no Body exists for the message.
	 * @see MIMEMessage#BASICPART
	 * @see MIMEMessage#MULTIPART
	 * @see MIMEMessage#MESSAGEPART
         */
	public int getBodyType ()	throws MIMEException
        {
		if (m_bodyType != UNINITIALIZED)
		    return (m_bodyType);
		else
		    throw new MIMEException ("getBodyType(): No body present!");
        }

	/**
         * Returns the content-type of the Message.
         * The content-type returned is the MIME content-type.
	 * @exception MIMEException If no Body exists.
         */
        public String getContentType () throws MIMEException
        {
		if (m_theBody == null)
		    throw new MIMEException ("getContentType(): No body present!");

 		return (m_theBody.getContentType());


/*		if (m_theBody instanceof MIMEBasicPart)
 *		{
 *		     MIMEBasicPart l_body = (MIMEBasicPart) m_theBody;
 *		     return (l_body.getContentType());
 *		}
 *		else if (m_theBody instanceof MIMEMessagePart)
 *		{
 *		     MIMEMessagePart l_body = (MIMEMessagePart) m_theBody;
 *		     return (l_body.getContentType());
 *		}
 *		else if (m_theBody instanceof MIMEMultiPart)
 *		{
 *		     MIMEMultiPart l_body = (MIMEMultiPart) m_theBody;
 *		     return (l_body.getContentType());
 *		}
 *		else
 *                    throw new MIMEException ("getContentType(): Invalid body!");
 */
        }

	/**
         * Returns the content subtype of the Message. NULL if none exists.
	 * @exception MIMEException If no Body exists.
         */
        public String getContentSubType () throws MIMEException
        {
		if (m_theBody == null)
		    throw new MIMEException ("getContentSubType(): No body present!");

		return (m_theBody.getContentSubType());

/*		MIMEBodyPart l_body = (MIMEBodyPart) m_theBody;
 *		return (l_body.getContentSubType());
 *
 *		if (m_theBody instanceof MIMEBasicPart)
 *		{
 *		     MIMEBasicPart l_body = (MIMEBasicPart) m_theBody;
 *		     return (l_body.getContentSubType());
 *		}
 *		else if (m_theBody instanceof MIMEMessagePart)
 *		{
 *		     MIMEMessagePart l_body = (MIMEMessagePart) m_theBody;
 *		     return (l_body.getContentSubType());
 *		}
 *		else if (m_theBody instanceof MIMEMultiPart)
 *		{
 *		     MIMEMultiPart l_body = (MIMEMultiPart) m_theBody;
 *		     return (l_body.getContentSubType());
 *		}
 *		else
 *                    throw new MIMEException ("getContentSubType(): Invalid body!");
 */
        }

	/**
         * Returns the content-type params of the Message. NULL if none exist.
	 * @exception MIMEException If no Body exists.
         */
        public String getContentTypeParams () throws MIMEException
        {
		if (m_theBody == null)
		    throw new MIMEException ("getContentTypeParams(): No body present!");

		return (m_theBody.getContentTypeParams());

/*		MIMEBodyPart l_body = (MIMEBodyPart) m_theBody;
 *		return (l_body.getContentTypeParams());
 *
 *		if (m_theBody instanceof MIMEBasicPart)
 *		{
 *		     MIMEBasicPart l_body = (MIMEBasicPart) m_theBody;
 *		     return (l_body.getContentTypeParams());
 *		}
 *		else if (m_theBody instanceof MIMEMessagePart)
 *		{
 *		     MIMEMessagePart l_body = (MIMEMessagePart) m_theBody;
 *		     return (l_body.getContentTypeParams());
 *		}
 *		else if (m_theBody instanceof MIMEMultiPart)
 *		{
 *		     MIMEMultiPart l_body = (MIMEMultiPart) m_theBody;
 *		     return (l_body.getContentTypeParams());
 *		}
 *		else
 *                    throw new MIMEException ("getContentTypeParams(): Invalid body!");
 */
        }


	/**
	* Returns an Object of corresponding MIMEBodyPart type that is
	* the body of this Message.
	* @param clone Whether to return a reference to the internal object or a cloned copy;
        * if true: cloned copy; if false: reference to the object.
	* @exception MIMEException If no Body exists.
	* @see MIMEBasicPart
	* @see MIMEMultiPart
	* @see MIMEMessagePart
	*/
	public Object getBody (boolean clone) throws MIMEException
    	{
		if (m_theBody == null)
		    throw new MIMEException ("getBody(): No body present!");

		if (m_theBody instanceof MIMEBasicPart)
		{
		     try
		     {
		         MIMEBasicPart l_body = (MIMEBasicPart) m_theBody;
		         return ( clone ? l_body.clone() : l_body );
		     }
		     catch (CloneNotSupportedException e)
		     {
			 throw new MIMEException (e.getMessage());
		     }
		}
		else if (m_theBody instanceof MIMEMessagePart)
		{
		     try
		     {
		         MIMEMessagePart l_body = (MIMEMessagePart) m_theBody;
		         return ( clone ? l_body.clone() : l_body );
		     }
		     catch (CloneNotSupportedException e)
		     {
			 throw new MIMEException (e.getMessage());
		     }
		}
		else if (m_theBody instanceof MIMEMultiPart)
		{
		     try
		     {
		         MIMEMultiPart l_body = (MIMEMultiPart) m_theBody;
		         return ( clone ? l_body.clone() : l_body );
		     }
		     catch (CloneNotSupportedException e)
		     {
			 throw new MIMEException (e.getMessage());
		     }
		}
		else
                     throw new MIMEException ("getBody(): Invalid body!");
        }



	/**
	* Returns an Object of corresponding MIME BodyPart type that is
	* the body of this Message.
	* @exception MIMEException If no Body exists.
	*/
	protected Object getBody () throws MIMEException
    	{
       		return getBody( true );
    	}


	/**
         * Deletes the body of the Message. Has no effect if no body is present.
         */
	public void deleteBody ()
        {
	      if (m_theBody != null)
	      {
		   m_theBody = null;
		   m_bodyType = UNINITIALIZED;
	      }
        }


	/**
	* Sets the MIMEBasicPart as Body of this Message. This part must already be
	* constructed and fully set-up. Sets the content-type of the message based on the
	* type of the part added by this method.
	* @param part BodyPart to add.
	* @exception MIMEException If body is already set (or) part is null.
	* @see MIMEBasicPart
	*/
	protected void setBody (MIMEBasicPart part ) throws MIMEException
	{
	    setBody( part, true );
	}



	/**
	* Sets the MIMEBodyPart as Body of this Message. This part must already be
	* constructed and fully set-up. Sets the content-type of the message based on the
	* type of the part being added. This MIMEBodyPart must be an object of one of the
	* three concrete classes: MIMEBasicPart, MIMEMultiPart and MIMEMessagePart.
	* @param part BodyPart to add.
	* @param clone Whether to store a reference to the object or a cloned copy;
        * if true: clones a copy; if false: stores a reference to the object.
	* @exception MIMEException If body is already set, or if part is null, or
	* if part is not an object of one of the three concrete classes:
	* MIMEBasicPart, MIMEMultiPart, or MIMEMessagePart.
	* @see MIMEBasicPart
	* @see MIMEMultiPart
	* @see MIMEMessagePart
	*/
	public void setBody (MIMEBodyPart part, boolean clone) throws MIMEException
	{
		if (part == null)
                 	throw new MIMEException ("setBody(): null part passed!");

		if (part instanceof MIMEBasicPart)
			setBody ((MIMEBasicPart) part, clone);
		else if (part instanceof MIMEMessagePart)
			setBody ((MIMEMessagePart) part, clone);
		else if (part instanceof MIMEMultiPart)
			setBody ((MIMEMultiPart) part, clone);
		else
             		throw new MIMEException ("setBody(): Invalid part ");
	}

	/**
	* Sets the MIMEBasicPart as Body of this Message. This part must already be
	* constructed and fully set-up. Sets the content-type of the message based on the
	* type of the part being added.
	* @param part BodyPart to add.
	* @param clone If false stores reference to passed object instead of cloning a copy.
	* @exception MIMEException If body is already set (or) part is null.
	* @see MIMEBasicPart
	*/
	protected void setBody (MIMEBasicPart part, boolean clone) throws MIMEException
        {
         	MIMEBasicPart l_part = null;

	     	if (m_theBody != null)
                 	throw new MIMEException ("setBody(): Body already set!");

	     	if (part == null)
                 	throw new MIMEException ("setBody(): null part passed!");

         	try
         	{
             		l_part = (MIMEBasicPart) ( clone ? part.clone() : part );
         	}
         	catch (CloneNotSupportedException e)
         	{
             		throw new MIMEException (e.getMessage());
         	}

	        m_theBody = l_part;
	        m_bodyType = BASICPART;
        }

	/**
	* Sets the MIMEMultiPart as Body of this Message. This part must already be
	* constructed and fully set-up. Sets the content-type of the message based on the
	* type of the part being added
	* @param part BodyPart to add.
	* @exception MIMEException If body is already set (or) part is null.
	* @see MIMEBasicPart
	*/
	protected void setBody (MIMEMultiPart part) throws MIMEException
	{
	    setBody( part, true );
	}


	/**
	* Sets the MIMEMultiPart as Body of this Message. This part should have been
	* constructed and fully set-up. Sets the content-type of the message accordingly.
	* @param part BodyPart to add.
	* @param clone If false stores reference to passed object instead of cloning a copy.
	* @exception MIMEException If body is already set  (or) part is null.
	* @see MIMEMultiPart
	*/
	protected void setBody (MIMEMultiPart part, boolean clone) throws MIMEException
        {
             MIMEMultiPart l_part = null;

	     if (m_theBody != null)
                 throw new MIMEException ("setBody(): Body already set!");

	     if (part == null)
                 throw new MIMEException ("setBody(): null part passed!");

	     try
             {
                 l_part = (MIMEMultiPart) ( clone ? part.clone() : part );
             }
             catch (CloneNotSupportedException e)
             {
                 throw new MIMEException (e.getMessage());
             }

	     m_theBody = l_part;
	     m_bodyType = MULTIPART;
        }


	/**
	* Sets the MIMEMessagePart as Body of this Message. This part must already be
	* constructed and fully set-up. Sets the content-type of the message based on the
	* type of the part being added.
	* @param part BodyPart to add.
	* @exception MIMEException If body is already set (or) part is null.
	* @see MIMEBasicPart
	*/
	protected void setBody (MIMEMessagePart part) throws MIMEException
	{
	    setBody( part, true );
	}


	/**
	* Sets the MIMEMessagePart as Body of this Message. This part must already be
	* constructed and fully set-up. Sets the content-type of the message accordingly.
	* @param part BodyPart to add.
	* @param clone If false stores reference to passed object instead of cloning a copy.
	* @exception MIMEException If body is already set (or) part is null.
	* @see MIMEMessagePart
	*/
	protected void setBody (MIMEMessagePart part, boolean clone) throws MIMEException
        {
             MIMEMessagePart l_part = null;

	     if (m_theBody != null)
                 throw new MIMEException ("setBody(): Body already set!");

	     if (part == null)
                 throw new MIMEException ("setBody(): null part passed!");

         try
         {
             l_part = (MIMEMessagePart) ( clone ? part.clone() : part );
         }
         catch (CloneNotSupportedException e)
         {
             throw new MIMEException (e.getMessage());
         }

	     m_theBody = l_part;
	     m_bodyType = MESSAGEPART;
        }


        /**
         * Outputs a byte-stream for this Message in MIME format with transfer encoding
         * applied to all bodyParts as applicable.
         * @param fullfilename Filename, including full path to write the byte-stream to.
         * @exception IOException If an IO error occurs.
         * @exception MIMEException If any required fields in the bodyPart are not set-up.
         */
        public void putByteStream (String fullfilename) throws IOException, MIMEException
        {
                FileOutputStream fos;
                fos = new FileOutputStream (fullfilename);
                putByteStream (fos);
        }



	/**
	* Outputs a byte stream for this Message in MIME format with transfer encoding
	* applied to all bodyParts as applicable.
	* @param os OutputStream to write to.
	* @exception IOException If an IO error occurs.
	* @exception MIMEException If detects an error during encoding.
	*/
	public void putByteStream (OutputStream os) throws IOException, MIMEException
        {
	     // Don't write content-type here. The Body will do that. That is, if it were
	     // a multi-part, it would do that. If it were a basic-part, it would do that etc.

	     if (m_theBody == null)
             {
                throw new MIMEException ("MIMEMessage.putByteStream(). No body!");
             }

	     // write out all the headers.
	     //StringBuffer l_hdrbuf;
	     boolean l_fMIMEVersionWritten = false;
             byte[]       l_bytebuf;

	     Header[] hdrs = getAllHeaders ();

	     if (hdrs != null)
	     for (int i = 0, len = hdrs.length; i < len; i++)
             {
                    try
                    {
                        //l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
			if (l_fMIMEVersionWritten == false &&
				(hdrs[i].m_headerName).equalsIgnoreCase("MIME-Version"))
				l_fMIMEVersionWritten = true;
                        l_bytebuf = MIMEHelper.unicodeToASCII (hdrs[i].getLine());
                        os.write (l_bytebuf);
                    }
                    catch (Exception e)
                    {
                        throw new MIMEException (hdrs[i].getLine() + "> "  + e.getMessage());
                    }
             } // for

	     // Now put the MIME Version.
	     if (!l_fMIMEVersionWritten && m_parsedPart==0)
	     {
	          l_bytebuf = MIMEHelper.unicodeToASCII ("MIME-Version: 1.0\r\n");
                  os.write (l_bytebuf);
	     }

	     // call thebody.putByteStream()
	     if (m_theBody instanceof MIMEBasicPart)
	     {
		  MIMEBasicPart l_body = (MIMEBasicPart) m_theBody;
		  l_body.putByteStream(os);
	     }
	     else if (m_theBody instanceof MIMEMessagePart)
	     {
		  MIMEMessagePart l_body = (MIMEMessagePart) m_theBody;
		  l_body.putByteStream(os);
	     }
	     else if (m_theBody instanceof MIMEMultiPart)
	     {
		  MIMEMultiPart l_body = (MIMEMultiPart) m_theBody;
		  l_body.putByteStream(os, true); // needPreamble!
	     }
	     else
        	  throw new MIMEException ("putByteStream(): Invalid body!");

             //os.write (CRLF); // terminate the message

        } // end putByteStream()


	/**
	* Clones an instance of this MIMEMessage object.
	* @exception CloneNotSupportedException If thrown by constituent components.
	*/
	public Object clone () throws CloneNotSupportedException
        {
                MIMEMessage l_theClone = (MIMEMessage) super.clone();

		if (m_822HeadersTable != null)
                    l_theClone.m_822HeadersTable = (Hashtable) m_822HeadersTable.clone();

		if (m_repeatHdrs != null)
                    l_theClone.m_repeatHdrs = (Vector) m_repeatHdrs.clone();

		if (m_theBody != null)
		{
		     if (m_theBody instanceof MIMEBasicPart)
		     {
		          MIMEBasicPart l_body = (MIMEBasicPart) m_theBody;
			  l_theClone.m_theBody = (MIMEBasicPart) l_body.clone();
		     }
		     else if (m_theBody instanceof MIMEMessagePart)
		     {
		          MIMEMessagePart l_body = (MIMEMessagePart) m_theBody;
			  l_theClone.m_theBody = (MIMEMessagePart) l_body.clone();
		     }
		     else if (m_theBody instanceof MIMEMultiPart)
		     {
		          MIMEMultiPart l_body = (MIMEMultiPart) m_theBody;
			  l_theClone.m_theBody = (MIMEMultiPart) l_body.clone();
		     }
		     else
                  	  return (null);

             	     l_theClone.m_UserObject = m_UserObject;

	        } // end if

             return (l_theClone);
        }

    protected Object getUserObject() { return m_UserObject; }
    protected void setUserObject( Object userObject ) { m_UserObject = userObject; }
}
