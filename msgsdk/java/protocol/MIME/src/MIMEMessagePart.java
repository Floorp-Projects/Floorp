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



/**
 * The MIMEMessagePart class implements the MIME MessagePart Content Type.
 * @author Prasad Yendluri
 */
public class MIMEMessagePart extends MIMEBodyPart implements Cloneable
{

//===========================================================================
//
//              Internal members not visible at the API
//
//===========================================================================

	// Encodings
        private static final int BINARY          = MIMEBodyPart.BINARY;
        private static final int E7BIT           = MIMEBodyPart.E7BIT;
        private static final int E8BIT           = MIMEBodyPart.E8BIT;


    	private static final int UNINITIALIZED = -1;

	private static final String[] m_stringDisposition  = { "Attachment", "Inline"};
	private static final String[] m_stringEncoding  = { "base64", "quoted-printable",
                                                            "binary", "7bit", "8bit" };
	private static final String MESSAGE = "Message";

        private int             m_contentTransferEncoding;
        private int             m_parsedPart;

        // The key to the hashTable below is the name of the header field.
        // To make sure we handle case differences in the header-name, we
        // must convert the header name to lower-case always prior to the
        // hash look-up. The entries added to the hash table are objects of
        // class Header.
        private Hashtable       m_ExtBodyHeadersTable;

    	private MIMEMessage	m_theMessage;
        private InputStream     m_partial;


//===========================================================================
//
//                              CONSTRUCTORS
//
//===========================================================================

	/**
         * Constructs a MIMEMessagePart object.
         * Default constructor for the MIMEMessagePart.
         */
	public MIMEMessagePart ()
        {
            m_parsedPart = 0;
            m_UserObject = null;
            m_contentDisposition      =  UNINITIALIZED;
            m_contentTransferEncoding =  UNINITIALIZED;
            m_theMessage = null;
            m_partial = null;
            m_ExtBodyHeadersTable = null;
        }



	/**
	* Constructs a MIMEMessagePart object from the given MIMEMessage Object.
	* @param msg The message that should form the body of this part.
	* @exception MIMEException If msg is null.
	*/
	public MIMEMessagePart (MIMEMessage msg) throws MIMEException
        {
                m_parsedPart = 0;
                m_contentDisposition      =  UNINITIALIZED;
                m_contentTransferEncoding =  UNINITIALIZED;

		if (msg != null)
		{
		     try
		     {
		          m_theMessage = (MIMEMessage) msg.clone();
		     }
		     catch (CloneNotSupportedException e)
		     {
                          throw new MIMEException (e.getMessage());
		     }
                }
		else
                     throw new MIMEException ("MIMEMessagePart(): null message passed." );

        }

//===========================================================================
//
//                        Methods from Interface MIMEBodyPart
//
//===========================================================================

        /**
         * Returns Content-Type of this MIME Part. Always returns the string "Message".
         */
        public String getContentType ()
        {
                return (MESSAGE);
        }


	/**
         * Sets Content-Transfer-Encoding of this MIME Part.
	 * Only BINARY, E7BIT, and E8BIT are applicable to MIMEMessagePart.
	 * Additionally, for message/partial and message/external-body, the MIME
	 * standard requires E7BIT encoding. It is possible to
         * set encoding prior to setting the contentSubType.
         * If a different value is specified, no error occurs; instead,
         * this method will be overridden by the putByteStream() method.
         * @param encoding Value that represents the encoding.
         * @see MIMEBodyPart#BINARY
         * @see MIMEBodyPart#E7BIT
         * @see MIMEBodyPart#E8BIT
	 * @exception MIMEException If the value is not one of BINARY, E7BIT or E8BIT
         */
	public void setContentEncoding (int encoding) throws MIMEException
        {
	    	switch (encoding)
	    	{
		    case BINARY:
		    case E7BIT:
		    case E8BIT:
			m_contentTransferEncoding = encoding;
			break;
		    default:
			 throw new MIMEException ("Invalid/Inapplicable  Content-Transfer-Encoding: " + encoding);
              }
        }

	/**
         * Returns value of Content-Transfer-Encoding of this MIME Part. -1 if none present.
         * @see MIMEBodyPart#BINARY
         * @see MIMEBodyPart#E7BIT
         * @see MIMEBodyPart#E8BIT
         */
	public int getContentEncoding ()
        {
		if (m_contentTransferEncoding ==  UNINITIALIZED)
		    return (-1);

		return (m_contentTransferEncoding);
        }

//===========================================================================
//
//                        Methods Specific to this Class alone.
//
//===========================================================================


        /**
         * Sets the specified message as the body of this part.
         * @param msg Message to be set as body of this part.
         * @exception MIMEException If already set (or) can not be set.
         */
        protected void setMessage (MIMEMessage msg) throws MIMEException
        {
            setMessage (msg, true);
        }


	/**
	* Sets the specified message as the body of this part.
	* @param msg Message to be set as body of this part.
	* @param clone If false stores reference to passed object instead of cloning a copy.
	* @exception MIMEException If already set (or) can not be set.
	*/
        public void setMessage (MIMEMessage msg, boolean clone) throws MIMEException
        {
		if (m_theMessage != null)
                    throw new MIMEException ("setMessage(): already set!");

		if (msg != null)
		{
		     try
		     {
		          m_theMessage = (MIMEMessage) (clone ? msg.clone() : msg);
		     }
		     catch (CloneNotSupportedException e)
		     {
                          throw new MIMEException (e.getMessage());
		     }
                }
		else
                     throw new MIMEException ("setMessage(): null message passed." );
        }

	/**
	* Returns the MIMEMessage that is the Body of this part.
	* @exception MIMEException If no Message exists in body-data.
	*/
	protected MIMEMessage getMessage () throws MIMEException
        {
            return getMessage (true);
        }


        /**
         * Returns the MIMEMessage that is the Body of this part.
         * @param clone If false returns reference to the object instead of a cloned copy.
         * @exception MIMEException If no Message exists in body-data.
         */
        public MIMEMessage getMessage (boolean clone) throws MIMEException
        {
		if (m_theMessage == null)
                    throw new MIMEException ("getMessage(): null Message in body!");

		try
		{
                     MIMEMessage l_msg = (MIMEMessage) (clone ? m_theMessage.clone() : m_theMessage);
		     return (l_msg);
		}
		catch (CloneNotSupportedException e)
		{
                     throw new MIMEException (e.getMessage());
		}
        }

	/**
	* Deletes the MIMEMessage that is the Body of this part.
	* Noop if no body was ever set.
	*/
        public void deleteMessage ()
        {
		m_theMessage = null;
        }

	/**
	* Sets the headers associated with External Body, for external-body content-subtypes
	* only. If already set, these headers are
	* ignored for other subtypes (rfc822 and message-partial).
	* @param name Name of the header field. Should not include ':'
	* @param value Value of the header field to be added.
	* @exception MIMEException if either of name or value is null
	*/
        public void setExtBodyHeader (String name, String value) throws MIMEException
        {
	     if ((name == null) || (value == null))
                 throw new MIMEException ("Invalid  NULL Header name or value");

             if ( name.indexOf (':') > -1)
                 throw new MIMEException ("Invalid  ':' in Header name");

	     if (m_ExtBodyHeadersTable == null)
                 m_ExtBodyHeadersTable = new Hashtable();

             String l_hashKey = name.toLowerCase();  // Keys are always lower case
             Header l_hdr = new Header (name, value);
             m_ExtBodyHeadersTable.put (l_hashKey, l_hdr);
        }

	/**
	* Returns the value of the specified header associated with External Body.
	* Applicable to external-body subtypes only.
	* Returns NULL if the header is not present.
	* @param name Name of the header field.
	* @exception MIMEException If name passed is a NULL.
	*/
        public String getExtBodyHeader (String name) throws MIMEException
        {
	     if (name == null)
                 throw new MIMEException ("Invalid  NULL Header name");

	     if (m_ExtBodyHeadersTable == null)
                  return (null);

             String l_hashKey = name.toLowerCase();  // Keys are always lower case
             Header l_hdr = (Header) m_ExtBodyHeadersTable.get (l_hashKey);

             if (l_hdr != null)
                return (l_hdr.getValue()); // getValue() returns a new string.
             else
                  return (null);
        }


	/**
         * Returns all the headers in this Part as an array of Header objects.
	 * @exception MIMEException If no headers exist.
         */
	public Header[] getAllHeaders () throws MIMEException
        {
		int l_numElements = 0, i = 0;

		if (m_ExtBodyHeadersTable != null)
			l_numElements = m_ExtBodyHeadersTable.size();

		if (l_numElements <= 0)
		   return (null);

		Header[] hdrs = new Header [l_numElements];
		Enumeration he = m_ExtBodyHeadersTable.elements();

		while (he.hasMoreElements())
		{
			hdrs[i++] = (Header) he.nextElement();
		}

                return (hdrs);
        }


	/**
	* Sets the message partial. Stores reference to the supplied inputStream.
	* Does not copy data. The data in partial is copied as is by putByteStream().
        * To be used with message/partial subtypes only. It is an error to setPartial()
	* for other subtypes (rfc822 and external-body).
        * @param instream inputStream that supplies the data for this partial.
        * @exception MIMEException if content-subtype is not partial or if already set.
        * @see deletePartial
        */
        protected void setPartial (InputStream instream) throws MIMEException
        {
		if (m_partial != null)
                 	throw new MIMEException ("Partial already set.");

		if ((m_contentSubType != null) &&
		    (!m_contentSubType.equalsIgnoreCase("partial")))
                 	throw new MIMEException ("Content-Subtype partial expected.");

		m_partial = instream;

	}

	/**
	* Deletes the message partial data of this part. Ignores if partial was not set.
        * @see setPartial
        */
        protected void deletePartial ()
        {
		m_partial = null;
	}

        /**
         * Outputs a byte-stream for this part with its MIME part headers and encoded
         * body data.
         * @param os OutputStream to write to.
         * @exception IOException If an IO error occurs.
         * @exception MIMEException If encoding error is detected.
         */
	public void putByteStream (OutputStream os) throws IOException, MIMEException
        {
		boolean l_external_body  = false;
		boolean l_message_partial = false;
	        String l_contentSubType = null;
		if (m_contentSubType != null)
 			l_contentSubType = m_contentSubType.trim();

		if ((l_contentSubType != null) &&
			 (l_contentSubType.equalsIgnoreCase("external-body")))
		{
		        l_external_body = true;

			if (m_ExtBodyHeadersTable == null);
			    //throw new MIMEException ("External body header Content-Id required for external-body");
		}

		if ((l_contentSubType != null) &&
			 (l_contentSubType.equalsIgnoreCase("partial")))
		{
		        l_message_partial = true;

			if (m_partial == null)
			    throw new MIMEException ("Message partial not set");

			if (m_contentTransferEncoding != E7BIT && m_contentTransferEncoding != UNINITIALIZED)
                     		throw new MIMEException ("Bad content-encoding for message/partial");

		}

		if ((m_theMessage == null) && (!l_external_body) && (!l_message_partial))
		{
                     throw new MIMEException ("putByteStream(): null Message body!");
		}

		// Write out the headers first
             	StringBuffer l_hdrbuf = new StringBuffer (HDRBUFSZ);
             	byte[]       l_bytebuf;

		// content-type
		l_hdrbuf.append ("Content-Type: message/");

		if (m_contentSubType != null)
		     l_hdrbuf.append (m_contentSubType);
		else
		{
		     if (m_theMessage != null)
			  l_hdrbuf.append ("rfc822"); // default
		     else
		          throw new MIMEException ("putByteStream(): No content-subtype." );
		}

		if (m_contentTypeParams != null && m_contentTypeParams.length() > 0)
		{
                     l_hdrbuf.append ("; ");
                     l_hdrbuf.append (m_contentTypeParams);
		}

        	l_hdrbuf.append ("\r\n");

		// write the header out to os
		try
		{
		     //l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
		     l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
		     os.write (l_bytebuf);
		}
		catch (Exception e)
		{
		     throw new MIMEException ("Content-Type: " + e.getMessage());
		}

		// contentID
		if (m_contentID != null)
		{
		     l_hdrbuf.setLength (0);
		     l_hdrbuf.append ("Content-ID: " + m_contentID);
		     l_hdrbuf.append ("\r\n");

		     try
		     {
		         // l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
		          l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
		          os.write (l_bytebuf);
		     }
		     catch (Exception e)
		     {
		          throw new MIMEException ("Content-ID: " + e.getMessage());
		     }
		}

		// contentDisposition
		if (m_contentDisposition != UNINITIALIZED)
		{
		     l_hdrbuf.setLength (0);
		     l_hdrbuf.append ("Content-Disposition: " +
                                      m_stringDisposition [m_contentDisposition]);

		     if (m_contentDispParams != null)
		     {
		          l_hdrbuf.append ("; ");
		          l_hdrbuf.append (m_contentDispParams);
		     }

		     l_hdrbuf.append ("\r\n");

		     try
		     {
		          //l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
		          l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
		          os.write (l_bytebuf);
		     }
		     catch (Exception e)
		     {
		          throw new MIMEException ("Content-Disposition: " + e.getMessage());
		     }
		}

		// contentDescription
		if (m_contentDescription != null)
		{
		     l_hdrbuf.setLength (0);
		     l_hdrbuf.append ("Content-Description: " + m_contentDescription);
		     l_hdrbuf.append ("\r\n");

		     try
		     {
		          //l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
		          l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
		          os.write (l_bytebuf);
		     }
		     catch (Exception e)
		     {
		          throw new MIMEException ("Content-Description: " + e.getMessage());
		     }
		}

		// Content-transfer-encoding
		//if (m_contentTransferEncoding == UNINITIALIZED)
		//{
                //   m_contentTransferEncoding = E7BIT; // default
                //}

		if (m_contentSubType != null)
		{
 		    if 	((l_message_partial || l_external_body) && m_contentTransferEncoding != UNINITIALIZED)
                          m_contentTransferEncoding = E7BIT; // As mandated by MIME standard
                }

		//if (!l_external_body) // for external-body we write it later
		{
		    if (m_contentTransferEncoding != UNINITIALIZED)
		    {
			if (l_external_body)
			       m_contentTransferEncoding = E7BIT;

			l_hdrbuf.setLength (0);
			l_hdrbuf.append ("Content-Transfer-Encoding: " +
                                 m_stringEncoding [m_contentTransferEncoding]);
			l_hdrbuf.append ("\r\n");

			try
			{
                     		// l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
                     		l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
                     		os.write (l_bytebuf);
			}
			catch (Exception e)
			{
				throw new MIMEException ("Content-Transfer-Encoding: " + e.getMessage());
			}
		    }
		}


		// blank-line after headers
		//l_bytebuf = new String("\r\n").getBytes("iso-8859-1");
		os.write (CRLF);

		// write out the message part if it is not external-body
		if ((m_theMessage != null) && (!l_external_body) && (!l_message_partial))
		     m_theMessage.putByteStream(os);
		else if (l_external_body)
		{
		    //os.write (CRLF); // Prior to the headers for the external-body

		    if (m_ExtBodyHeadersTable != null)
		    {
			Enumeration he = m_ExtBodyHeadersTable.elements();

			while (he.hasMoreElements())
			{
			     Header l_tmphdr = (Header) he.nextElement();

			     try
                    	     {
                                   l_bytebuf = MIMEHelper.unicodeToASCII (l_tmphdr.getLine());
                                   os.write (l_bytebuf);
                              }
                              catch (Exception e)
                              {
                                  throw new MIMEException (l_tmphdr.getLine() + e.getMessage());
                              }

			} // while

		    //if (m_contentTransferEncoding != UNINITIALIZED)
		    //{
		    //	// write out content-encoding
		    //	l_hdrbuf.setLength (0);
		    //	l_hdrbuf.append ("Content-Transfer-Encoding: " +
                    //              m_stringEncoding [m_contentTransferEncoding]);
		   //	l_hdrbuf.append ("\r\n");
   		   //
		   //	try
		   //	{
                   //  		// l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
                   //  		l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
                   // 		os.write (l_bytebuf);
		   //	}
		   //	catch (Exception e)
		   //	{
		   //		throw new MIMEException ("Content-Transfer-Encoding: " + e.getMessage());
		   //	}
                   //   }

		        os.write (CRLF); // After the headers for the external-body

		    } // if m_ExtBodyHeadersTable

		} // if l_external_body
		else if (l_message_partial)
		{
		    os.write (CRLF);
		    byte l_buf [] = new byte [DATABUFSZ];
		    int l_len;

		    while ( (l_len = m_partial.read (l_buf, 0, DATABUFSZ)) > 0)
		    {
			os.write (l_buf, 0, l_len);
		    }

		} // if l_external_body

		os.write (CRLF); // terminate the part

        } // end putByteStream()

        /**
         * Clones an instance of this MIMEMessagePart object.
         * @exception CloneNotSupportedException If thrown by a constituent components.
         */
        public Object clone () throws CloneNotSupportedException
        {
    		MIMEMessagePart l_theClone = (MIMEMessagePart) super.clone();

    		if (m_theMessage != null)
    		{
    			l_theClone.m_theMessage = (MIMEMessage) m_theMessage.clone();
    		}

    		if (m_contentSubType != null)
                         l_theClone.m_contentSubType = m_contentSubType;

    		if (m_contentTypeParams != null)
                         l_theClone.m_contentTypeParams = m_contentTypeParams;

    		if (m_contentID != null)
                         l_theClone.m_contentID = m_contentID;

    		if (m_contentDispParams != null)
                         l_theClone.m_contentDispParams = m_contentDispParams;

    		if (m_contentDescription != null)
                         l_theClone.m_contentDescription = m_contentDescription;

            	l_theClone.m_UserObject = m_UserObject;

		return (l_theClone);
	}
}
