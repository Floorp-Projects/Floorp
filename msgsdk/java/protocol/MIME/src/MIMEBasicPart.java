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
 * The MIMEBasicPart class is a common class used for all Basic
 * MIME BodyPart types:
 * Text, Image, Audio, Video, and Application. It does not represent structured
 * parts such as MIMEMessagePart and MIMEMultiPart.
 * @author Prasad Yendluri
 */
public class MIMEBasicPart extends MIMEBodyPart implements Cloneable
{

	/**
         * Content (primary) type Text.
         */
        public static final int TEXT            = 0;
	/**
         * Content (primary) type Audio.
         */
        public static final int AUDIO           = 1;
	/**
         * Content (primary) type Image.
         */
        public static final int IMAGE           = 2;
	/**
         * Content (primary) type Video.
         */
        public static final int VIDEO           = 3;
	/**
         * Content (primary) type Application.
         */
        public static final int APPLICATION     = 4;



//===========================================================================
//
//              Internal members not visible at the API
//
//===========================================================================

	// Encodings
        private static final int BASE64          = MIMEBodyPart.BASE64;
        private static final int QP              = MIMEBodyPart.QP;
        private static final int BINARY          = MIMEBodyPart.BINARY;
        private static final int E7BIT           = MIMEBodyPart.E7BIT;
        private static final int E8BIT           = MIMEBodyPart.E8BIT;
        private static final int EIGNORE         = 99;
        public static final int UNINITIALIZED = -1;
        private static final int MAXDATABUFSIZE = 1111111;

	private static final String[] m_stringContentTypes = { "Text", "Audio", "Image",
							       "Video", "Application" };
	private static final String[] m_stringEncoding  = { "base64", "quoted-printable",
							    "binary", "7bit", "8bit" };
	private static final String[] m_stringDisposition  = { "Attachment", "Inline"};

	private int 		m_contentType;
	private String 		m_contentMD5;
	private int 		m_contentTransferEncoding;
	private int 		m_parsedPart;

          // The key to the hashTable below is the name of the header field.
	  // To make sure we handle case differences in the header-name, we
	  // must convert the header name to lower-case always prior to the
          // hash look-up. The entries added to the hash table are objects of
          // class Header.
	private Hashtable 	m_mimeHeadersTable;

	  // Buffer to hold body-data (not encoded). We always store un-encoded.
          // Buffer stores data as parsed from input message or as given by the user
          // when constructing this bodyPart.
	private ByteBuffer m_databuf;
	private byte 	   m_readbuf[];
	private InputStream m_dataStream;

	private int m_nStartMessageDataIndex;
	private int m_nEndMessageDataIndex;
	private int m_nMessageDataLen;
    	private boolean m_bDecodedData;
    	protected boolean m_endData;
    	protected MIMEMultiPart m_parentContainer;


//===========================================================================
//
// 				CONSTRUCTORS
//
//===========================================================================

	/**
         * Constructs a MIMEBasicPart object with the given content-type.
	 * @param contentType Content type. Values:
         * TEXT, AUDIO, IMAGE, VIDEO, APPLICATION.
         * @see MIMEBasicPart#TEXT
         * @see MIMEBasicPart#AUDIO
         * @see MIMEBasicPart#IMAGE
         * @see MIMEBasicPart#VIDEO
         * @see MIMEBasicPart#APPLICATION
	 * @exception MIMEException If ContentType passed is invalid.
         */
	public MIMEBasicPart (int contentType) throws MIMEException
        {
	    	switch (contentType)
	    	{
		    case MIMEBasicPart.TEXT:
			m_contentType = TEXT;
			break;
		    case MIMEBasicPart.AUDIO:
			m_contentType = AUDIO;
			break;
		    case MIMEBasicPart.IMAGE:
			m_contentType = IMAGE;
			break;
		    case MIMEBasicPart.VIDEO:
			m_contentType = VIDEO;
			break;
		    case MIMEBasicPart.APPLICATION:
			m_contentType = APPLICATION;
			break;
		    default:
			 throw new MIMEException ("Invalid  content-type: " + contentType);
		}

		m_parsedPart = 0;
		m_contentDisposition      =  UNINITIALIZED;
		m_contentTransferEncoding =  UNINITIALIZED;
		m_mimeHeadersTable = new Hashtable();
		m_databuf =  null;
		m_readbuf = null;
		m_dataStream =  null;

    		m_nStartMessageDataIndex  = UNINITIALIZED;
	    	m_nEndMessageDataIndex    = UNINITIALIZED;
	    	m_nMessageDataLen         = 0;
	    	m_UserObject              = null;
	    	m_bDecodedData             = false;
	}


	/**
	* Default constructor.
	* Constructs a MIMEBasicPart object with MIMEBasicPart#TEXT content-type.
	*/
	public MIMEBasicPart() throws MIMEException
	{
   		 this (MIMEBasicPart.TEXT);
	}


//===========================================================================
//
//                        Methods overriden from MIMEBodyPart
//
//===========================================================================

	/**
	* Returns Content-Type of this MIME Part.
	* @see MIMEBodyPart#getContentType
	*/
	public String getContentType ()
        {
		String ct = new String (m_stringContentTypes [m_contentType]);
                return (ct);
        }


//===========================================================================
//
//                        Methods Specific to this Class alone.
//
//===========================================================================

	/**
	* Returns Content-MD5 of this MIME Part or NULL if Content-MD5 is not present.
        * @see #setContentMD5
	*/
	public String getContentMD5 ()
        {
		if (m_contentMD5 == null)
			return (null);

                return (m_contentMD5);
        }

	/**
         * Sets Content-MD5 of this MIME Part.
         * @param cid String to use as Content-ID.
	 * @see #getContentMD5
         */
	public void setContentMD5 (String md5)
	{
	    m_contentMD5 = md5;
	}

	/**
         * Returns an InputStream to this Part's Data after decoding any
	 * transfer encoding.
	 * @exception IOException If an IO error occurs.
	 * @exception MIMEException If the BodyData is not present.
	 * or an error is detected during decoding.
         * @see #setBodyData
         */
	public InputStream getBodyData () throws IOException, MIMEException
        {
	     if (m_databuf == null && m_dataStream == null)
	     {
		  throw new MIMEException ("getBodyData(): No BodyData present." );
	     }

	     if (m_databuf != null)
	     {
	     	ByteArrayInputStream ins = new ByteArrayInputStream (m_databuf.getBytes());
             	return (ins);
	     }

	     if (m_dataStream.markSupported())
			m_dataStream.reset();

             return (m_dataStream);
        }

	/**
     * Returns an ByteBuffer object, return null if no data
     * carsonl, used by parser
     */
	protected ByteBuffer getDataBuf() { return m_databuf; }



	/**
         * Sets BodyData of this MIME Part.
         * @param is Stream to read input data from. This data must be un-encoded raw data.
	 * @exception MIMEException If already set (or) if is parameter is null.
	 * @exception IOException If an IO error occurs.
	 * @see #getDataBuf
         */
	public void setBodyData (InputStream is) throws MIMEException, IOException
        {
		if (m_databuf != null && m_dataStream != null)
		{
		      throw new MIMEException ("setBodyData(): BodyData already set." );
		}

		if (is == null)
		{
		      throw new MIMEException ("setBodyData(): null inputStream");
		}

        	setMessageDataLen( is.available() );

		if (m_nMessageDataLen <= 0)
		{
			m_nMessageDataLen = 0;
		        throw new MIMEException ("setBodyData(): no data in inputStream");
		}

		if (m_nMessageDataLen > MAXDATABUFSIZE)
		{
			m_dataStream = is;
			if (is.markSupported())
				is.mark (2*m_nMessageDataLen);
		}
		else
		{
			ByteBuffer l_inbuf = new ByteBuffer (DATABUFSZ);
			int l_read = is.read (l_inbuf.buffer, 0, DATABUFSZ);

			if (l_read > 0)
			{
		     		m_databuf  = new ByteBuffer(DATABUFSZ); // initial size
			}
			else
			{
		     		throw new MIMEException ("setBodyData(): InputStream Empty.");
			}

			while (l_read > 0)
			{
		    		l_inbuf.hi = l_read;
		   		m_databuf.append (l_inbuf);
		  		l_read = is.read (l_inbuf.buffer, 0, DATABUFSZ);
			}
		}
        }

	/**
        * Sets BodyData of this MIME Part.
        * @param s Source string for input data. Must be un-encoded raw data.
	* @exception MIMEException If already set or if is parameter is null.
        */
    	public void setBodyData (String s) throws MIMEException
    	{
		if (m_databuf != null && m_dataStream != null)
		{
		      throw new MIMEException ("setBodyData(): BodyData already set." );
		}

		if ( s != null )
		{
			m_databuf = new ByteBuffer( s.length() );
			m_databuf.append( s.getBytes() );
			setMessageDataLen( s.length() );
		}
    	}

    	// non clone version
    	// carsonl. Not Exposed at API
    	protected void setBodyData( ByteBuffer s ) throws MIMEException
    	{
		if (m_databuf != null && m_dataStream != null)
		{
		      throw new MIMEException ("setBodyData(): BodyData already set." );
		}

    	    	if ( s != null )
        	    m_databuf = s;
    	}

        /**
        * Sets BodyData of this MIME Part.
	* Saves a reference to the passed byte buffer.
	* Does not copy the data.
        * @param s  un-encoded raw data.
	* @exception MIMEException If already set or if s parameter is null.
        */
    	public void setBodyData( byte s[] ) throws MIMEException
    	{
		if (m_databuf != null && m_dataStream != null)
		{
		      throw new MIMEException ("setBodyData(): BodyData already set.");
		}

    	    if ( s != null )
    	    {
        	    m_databuf = new ByteBuffer( s );
                setMessageDataLen( s.length );
        	}
    	}


	/**
         * Deletes BodyData of this MIME Part.
	 * If no BodyData is present, this method has no effect.
         */
	public void deleteBodyData ()
        {
		m_databuf = null;
		m_dataStream = null;
        	m_nMessageDataLen = 0;
        }

	/**
         * Outputs a byte-stream for this part with its MIME part headers and encoded
         * body data.
         * @param fullfilename Filename including full path of where to write the byte-stream.
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
         * Outputs a byte-stream for this part with its MIME part headers and encoded
         * body data.
         * @param os OutputStream to write to.
	 * @exception IOException If an IO error occurs.
	 * @exception MIMEException If any required fields in the bodyPart are not set-up.
         */
	public void putByteStream (OutputStream os) throws IOException, MIMEException
        {
	     if (m_databuf == null && m_dataStream == null)
	     {
		throw new MIMEException ("MIMEBasicPart.putByteStream(). No bodyData!");
	     }

	     // Write out the headers first
	     StringBuffer l_hdrbuf = new StringBuffer (HDRBUFSZ);
	     byte[]       l_bytebuf;

	     //  Headers in member variables

	     // content-type
             switch (m_contentType)
             {
                  case TEXT:
		      l_hdrbuf.append ("Content-Type: text/");

		      if (m_contentSubType != null)
		           l_hdrbuf.append (m_contentSubType);
		      else
		           l_hdrbuf.append ("plain");

		      if (m_contentTypeParams != null && m_contentTypeParams.length() > 0 )
		      {
		           l_hdrbuf.append ("; ");
		           l_hdrbuf.append (m_contentTypeParams);
		           l_hdrbuf.append ("\r\n");
		      }
		      else
		      {
		      //    l_hdrbuf.append ("; ");
		      //    l_hdrbuf.append ("charset=us-ascii\r\n");
		            l_hdrbuf.append ("\r\n");
		      }
                      break;
                  case AUDIO:
		      l_hdrbuf.append ("Content-Type: audio/");

		      if (m_contentSubType != null)
		           l_hdrbuf.append (m_contentSubType);
		      else
		           throw new MIMEException ("putByteStream: No content-subtype." );
		           //l_hdrbuf.append ("basic");

		      if (m_contentTypeParams != null && m_contentTypeParams.length() > 0 )
		      {
		           l_hdrbuf.append ("; ");
		           l_hdrbuf.append (m_contentTypeParams);
		           l_hdrbuf.append ("\r\n");
		      }
		      else
		           l_hdrbuf.append ("\r\n");
                      break;
                  case IMAGE:
		      l_hdrbuf.append ("Content-Type: image/");

		      if (m_contentSubType != null)
		           l_hdrbuf.append (m_contentSubType);
		      else
		           throw new MIMEException ("putByteStream: No content-subtype.");
		           //l_hdrbuf.append ("jpeg");

		      if (m_contentTypeParams != null && m_contentTypeParams.length() > 0 )
		      {
		           l_hdrbuf.append ("; ");
		           l_hdrbuf.append (m_contentTypeParams);
		           l_hdrbuf.append ("\r\n");
		      }
		      else
		           l_hdrbuf.append ("\r\n");
                      break;
                  case VIDEO:
		      l_hdrbuf.append ("Content-Type: video/");
		      if (m_contentSubType != null)
		           l_hdrbuf.append (m_contentSubType);
		      else
		           throw new MIMEException ("putByteStream: No content-subtype.");
		           //l_hdrbuf.append ("mpeg");

		      if (m_contentTypeParams != null && m_contentTypeParams.length() > 0 )
		      {
		           l_hdrbuf.append ("; ");
		           l_hdrbuf.append (m_contentTypeParams);
		           l_hdrbuf.append ("\r\n");
		      }
		      else
		           l_hdrbuf.append ("\r\n");
                      break;
                  case APPLICATION:
		      l_hdrbuf.append ("Content-Type: application/");
		      if (m_contentSubType != null)
		           l_hdrbuf.append (m_contentSubType);
		      else
		           l_hdrbuf.append ("octet-stream");

		      if (m_contentTypeParams != null && m_contentTypeParams.length() > 0 )
		      {
		           l_hdrbuf.append ("; ");
		           l_hdrbuf.append (m_contentTypeParams);
		           l_hdrbuf.append ("\r\n");
		      }
		      else
		           l_hdrbuf.append ("\r\n");
                      break;
                  default:
                       throw new MIMEException ("Invalid  content-type: " + m_contentType);
             }

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
	              //l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
	              l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
	              os.write (l_bytebuf);
	          }
	          catch (Exception e)
	          {
		      throw new MIMEException ("Content-ID: " + e.getMessage());
	          }
	     }

	     // contentMD5
	     if (m_contentMD5 != null)
	     {
		  l_hdrbuf.setLength (0);
		  l_hdrbuf.append ("Content-MD5: " + m_contentMD5);
		  l_hdrbuf.append ("\r\n");

	     	  try
	     	  {
	            //l_bytebuf = l_hdrbuf.toString().getBytes("iso-8859-1");
	              l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
	              os.write (l_bytebuf);
	          }
	          catch (Exception e)
	          {
		      throw new MIMEException ("Content-MD5: " + e.getMessage());
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
	//	if (m_contentTransferEncoding == UNINITIALIZED)
	//	{
	//	  if ((m_contentType == TEXT) && (m_contentTypeParams != null) && m_contentTypeParams.length() > 0)
	//	  {
	//	       String l_params = m_contentTypeParams.toLowerCase();
	//	       int idx = l_params.indexOf ("charset=us-ascii");
	//	       if (idx >= 0)
	//	           m_contentTransferEncoding = E7BIT;
	//	       else
	//	           //m_contentTransferEncoding = BASE64;
	//	           //m_contentTransferEncoding = E8BIT;
	//	           m_contentTransferEncoding = EIGNORE;
        //
	//	  }
	//	  else
	//	       m_contentTransferEncoding = E7BIT;
	 //    }

	     if (m_contentTransferEncoding != EIGNORE && m_contentTransferEncoding != UNINITIALIZED)
	     {
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

	     // Do all the other headers
	     Header[] hdrs = getAllHeaders ();
	     if (hdrs != null)
                for (int i = 0, len = hdrs.length; i < len; i++)
		{
	            try
	            {
	                l_bytebuf = MIMEHelper.unicodeToASCII (hdrs[i].getLine());
	                os.write (l_bytebuf);
	            }
	            catch (Exception e)
	            {
    		        throw new MIMEException (hdrs[i].getLine() + "> "  + e.getMessage());
	            }
		}

             // blank-line after headers
             //l_bytebuf = new String("\r\n").getBytes("iso-8859-1");
             //os.write (LF);
             os.write (CRLF);

	     int encoding = m_contentTransferEncoding;

	     if (encoding == UNINITIALIZED || encoding == EIGNORE)
		   encoding = E7BIT;

             // Write out the bodydata encoding if needed.
	     switch (encoding)  // we know it is not UNINITIALIZED
	     {
		    case BASE64:
    			if (m_dataStream == null)
    			{
    	                	ByteArrayInputStream isb =
    					new ByteArrayInputStream (m_databuf.getBytes());
    				MIMEHelper.encodeBase64 (isb, os);
    			}
    			else
    			{
    	     			if (m_dataStream.markSupported())
    					m_dataStream.reset();
    				MIMEHelper.encodeBase64 (m_dataStream, os);
    			}
    			break;

		    case QP:
    			if (m_dataStream == null)
    			{
    	                	ByteArrayInputStream isq =
    					new ByteArrayInputStream (m_databuf.getBytes());
    				MIMEHelper.encodeQP (isq, os);
    			}
    			else
    			{
    	     			if (m_dataStream.markSupported())
    					m_dataStream.reset();
    				MIMEHelper.encodeQP (m_dataStream, os);
    			}
    			break;

		    case BINARY:
		    case E7BIT:
		    case E8BIT:
		    case EIGNORE:
    			if (m_dataStream == null)
    			{
    				os.write (m_databuf.getBytes());
    			}
    			else
    			{
    	     			if (m_dataStream.markSupported())
    					m_dataStream.reset();

    				if (m_readbuf == null)
    					m_readbuf = new byte [DATABUFSZ];

    				int l_read = m_dataStream.read (m_readbuf, 0, DATABUFSZ);

    				while (l_read > 0)
    				{
    				     os.write (m_readbuf, 0, l_read);
    		   		     l_read = m_dataStream.read (m_readbuf, 0, DATABUFSZ);
    				}
    			}
			break;

		    default:
			 throw new MIMEException ("Invalid  Content Transfer Encoding : "
						  + m_contentTransferEncoding);
	     } // switch

             //os.write (CRLF);

        } // putByteStream()

	/**
         * Returns the size of this Part's BodyData.
	 * @return Size of BodyData or -1 if this part does not have bodyData.
         */
	public int getSize ()
        {
		if (m_databuf != null)
		{
		     return (m_databuf.size());
		}
		else if (m_dataStream != null)
		{
		     return (m_nMessageDataLen);
		}

		return (-1);
        }

	/**
         * Sets Content-Transfer-Encoding of this MIME Part.
         * @param encoding Value that represents the encoding.
         * @see MIMEBodyPart#BASE64
         * @see MIMEBodyPart#QP
         * @see MIMEBodyPart#BINARY
         * @see MIMEBodyPart#E7BIT
         * @see MIMEBodyPart#E8BIT
	 * @exception MIMEException If encoding is invalid.
         */
	public void setContentEncoding (int encoding) throws MIMEException
        {
	    	switch (encoding)
	    	{
		    case BASE64:
		    case QP:
		    case BINARY:
		    case E7BIT:
		    case E8BIT:
			m_contentTransferEncoding = encoding;
			break;
		    default:
			 throw new MIMEException ("Invalid  Content Transfer Encoding : " + encoding);
              }
        }

	/**
         * Returns value of Content-Transfer-Encoding of this MIME Part. -1 if none present.
         * @see MIMEBodyPart#BASE64
         * @see MIMEBodyPart#QP
         * @see MIMEBodyPart#BINARY
         * @see MIMEBodyPart#E7BIT
         * @see MIMEBodyPart#E8BIT
         */
	public int getContentEncoding ()
        {
		if (m_contentTransferEncoding ==  UNINITIALIZED || m_contentTransferEncoding == EIGNORE)
		    return (-1);

		return (m_contentTransferEncoding);
        }

	/**
         * Sets a header of this body-part given name and value.
         * To be used to set any header that is not explicitly supported with a
         * set method, such as X-headers. Multiple invocations on the same header
	 * replace the old value.
         * @param name Name of the header field. Should not include ':'
         * @param value Value of the header field to be added.
	 * @exception MIMEException If either of name or value is NULL.
         */
	public void setHeader (String name, String value) throws MIMEException
        {
	     if ((name == null) || (value == null))
		 throw new MIMEException ("Invalid  NULL Header name or value");

	     if ( name.indexOf (':') > -1)
		 throw new MIMEException ("Invalid  ':' in Header name");

	     // Check if the header is one of those they need to specify with other methods
	     String rslt = isOKtoset (name);
	     if (rslt != null)
		 throw new MIMEException ("Invalid setHeader(). Use method: " + rslt);

	     String l_hashKey = name.toLowerCase();
	     Header l_hdr = new Header (name, value);
	     m_mimeHeadersTable.put (l_hashKey, l_hdr);
        }

        /**
        * Appends the value to an existing header. Creates new header if one does
	* not exist already.
        * @param name Name of the header field.
        * @param value Value of the header field to be added.
        * @exception MIMEException If either name or value are NULL
        */
	public void addHeader (String name, String value) throws MIMEException
	{
	     if ((name == null) || (value == null))
		 throw new MIMEException ("Invalid  NULL Header name or value");

	     String l_hashKey = name.toLowerCase();

	     Header l_hdr = (Header) m_mimeHeadersTable.get (l_hashKey);

	     if (l_hdr == null)
		setHeader (name, value);
	     else
	     {
		  StringBuffer l_oldvalue = new StringBuffer (l_hdr.getValue());
		  l_oldvalue.append (value);
		  Header l_newhdr = new Header (name, l_oldvalue.toString());
		  m_mimeHeadersTable.put (l_hashKey, l_newhdr);
	     }
	}

	/**
         * Returns the value of the requested header.
	 * Returns NULL if the header is not present.
         * @param name Name of the header field.
	 * @exception MIMEException If name passed is a NULL.
         */
	public String getHeader (String name) throws MIMEException
        {
	     if (name == null)
		 throw new MIMEException ("Invalid  NULL Header name");

	     String l_hashKey = name.toLowerCase();
	     Header l_hdr = (Header) m_mimeHeadersTable.get (l_hashKey);

	     if (l_hdr != null)
		return (l_hdr.getValue());
	     else
		  return (null);
        }

	/**
         * Deletes the requested header from this Part.
         * @param name Name of the header field.
	 * @exception MIMEException if name is NULL.
         */
	public void deleteHeader (String name) throws MIMEException
        {
	     if (name == null)
		 throw new MIMEException ("Invalid  NULL Header name");

	     String l_hashKey = name.toLowerCase();
	     m_mimeHeadersTable.remove (l_hashKey);
        }

	/**
         * Returns all the headers in this Part as an array of Header objects.
	 * @exception MIMEException If no headers exist.
         */
	public Header[] getAllHeaders () throws MIMEException
        {
		int l_numElements = m_mimeHeadersTable.size();
		int i = 0;

		if (l_numElements <= 0)
		   return (null);

		Header[] hdrs = new Header [l_numElements];
		Enumeration he = m_mimeHeadersTable.elements();

		while (he.hasMoreElements())
		{
			hdrs[i++] = (Header) he.nextElement();
		}

                return (hdrs);
        }

	/**
         * Checks if it is OK to set the header with the setHeader().
	 * @return null if it is OK to set. Name of the method to use if it is NOT ok.
         */
	private String isOKtoset (String name)
	{
	     if (name.equalsIgnoreCase ("Content-Type"))
		 return ("setContentType()");

	     if (name.equalsIgnoreCase ("Content-ID"))
		 return ("setContentID()");

	     if (name.equalsIgnoreCase ("Content-MD5"))
		 return ("setContentMD5()");

	     if (name.equalsIgnoreCase ("Content-Disposition"))
		 return ("setContentDisposition()");

	     if (name.equalsIgnoreCase ("Content-Description"))
		 return ("setContentDescription()");

	     if (name.equalsIgnoreCase ("Content-Transfer-Encoding"))
		 return ("setContentEncoding()");

	     return (null);

	} // end isOKtoset ()

	private ByteBuffer getStreamData (InputStream is)
	{
		ByteBuffer l_retbuf, l_readbuf = new ByteBuffer (DATABUFSZ);

		try
		{
	     		if (m_dataStream.markSupported())
				m_dataStream.reset();
			int l_read = is.read (l_readbuf.buffer, 0, DATABUFSZ);

			if (l_read > 0)
			{
		     		l_retbuf  = new ByteBuffer(DATABUFSZ); // initial size
			}
			else
			      return null;

			while (l_read > 0)
			{
		    	    l_readbuf.hi = l_read;
		            l_retbuf.append (l_readbuf);
		            l_read = is.read (l_readbuf.buffer, 0, DATABUFSZ);
			}

	     		if (m_dataStream.markSupported())
				m_dataStream.reset();
			return l_retbuf;
		}
		catch (IOException e)
		{
			return null;
		}
	}

	/**
	* Clones an instance of this MIMEBasicPart object.
	* @exception CloneNotSupportedException If thrown by constituent components.
	*/
    	public Object clone () throws CloneNotSupportedException
    	{
		// Do the bit-wise copy first.
		MIMEBasicPart l_theClone = (MIMEBasicPart) super.clone();

		// Take care of all other "reference"s.
        	if (m_mimeHeadersTable != null)
		      l_theClone.m_mimeHeadersTable = (Hashtable) m_mimeHeadersTable.clone();

		if (m_databuf != null)
			l_theClone.m_databuf = (ByteBuffer) m_databuf.clone();
		else if (m_dataStream != null)
		{
			l_theClone.m_databuf = getStreamData (m_dataStream);
			l_theClone.m_dataStream = null;
		}
		if (m_contentSubType != null)
			l_theClone.m_contentSubType = m_contentSubType;
		if (m_contentTypeParams != null)
			l_theClone.m_contentTypeParams = m_contentTypeParams;

		if (m_contentID != null)
			l_theClone.m_contentID = m_contentID;

		if (m_contentMD5 != null)
			l_theClone.m_contentMD5 = m_contentMD5;

		if (m_contentDispParams != null)
			l_theClone.m_contentDispParams = m_contentDispParams;

		if (m_contentDescription != null)
			l_theClone.m_contentDescription = m_contentDescription;

		l_theClone.m_nStartMessageDataIndex = m_nStartMessageDataIndex;
		l_theClone.m_nEndMessageDataIndex = m_nEndMessageDataIndex;
		l_theClone.m_nMessageDataLen = m_nMessageDataLen;
		l_theClone.m_UserObject = m_UserObject;
		l_theClone.m_bDecodedData = m_bDecodedData;

		return (l_theClone);

	} // end clone()


    protected int getStartMessageDataIndex() { return m_nStartMessageDataIndex; }
    protected void setStartMessageDataIndex( int i ) { m_nStartMessageDataIndex = i; }

    protected int getEndMessageDataIndex() { return m_nEndMessageDataIndex; }
    protected void setEndMessageDataIndex( int i ) { m_nEndMessageDataIndex = i; }

      public int getMessageDataLen() { return getSize(); }

    protected void setMessageDataLen( int len ) { setSize( len ); }

    protected void setContentType( int contentType ) { m_contentType = contentType; }

    protected boolean getDecodedData() { return m_bDecodedData; }
    protected void setDecodedData( boolean decodedData ) { m_bDecodedData = decodedData; }

    protected void setSize( int len )
    {
    	m_nMessageDataLen = len;

    	if (m_databuf != null)
    	     m_databuf.setSize( len );
    	//else if (m_dataStream != null)
    	//     m_nMessageDataLen = len;
    }

} // end class MIMEBasicPart
