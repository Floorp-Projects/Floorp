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
  * The MIMEMultiPart class implements the MIME multi-part Content Type.
  * <p>The multi-part content type represents messages with multiple
  * attachments of potentially different media. This content type
  * describes a message that is made up of one or more sub-body parts.
  * The Multipart type has several subtypes that describe how the
  * sub-parts relate to each other. These include mixed, alternative,
  * digest, and parallel.
  * @author Prasad Yendluri
  */
public class MIMEMultiPart extends MIMEBodyPart implements Cloneable
{

//===========================================================================
//
//              Internal members not visible at the API
//
//===========================================================================

        private static final int UNINITIALIZED = -1;
	private static final String[] m_stringDisposition  = { "Attachment", "Inline"};
	private static final String[] m_stringEncoding  = { "base64", "quoted-printable",
                                                            "binary", "7bit", "8bit" };
	private static final String MULTIPART = "MultiPart";

      //private int             m_contentType;
      //private int             m_contentTransferEncoding;
        protected int           m_parsedPart;
        private int             m_bpCount; // count of bodyparts
        private boolean         m_needPreamble;
        protected boolean       m_endPart;

	// Vector to hold the bodyparts.
        private Vector          m_bodyParts;
        private String          m_boundary;
        private String          m_preamble;
	private int             m_contentTransferEncoding;


//===========================================================================
//
//                              CONSTRUCTORS
//
//===========================================================================

	/**
	* Constructs a MIMEMultiPart object.
	* Default constructor for the MIMEMultiPart
	*/
	public MIMEMultiPart ()
    	{
        	m_parsedPart = 0;
        	m_contentDisposition      =  UNINITIALIZED;
	        m_contentTransferEncoding =  UNINITIALIZED;
		m_needPreamble = false;
		m_bodyParts = new Vector();
		m_boundary = null;
		m_UserObject = null;
    	}


	/**
	* Constructs a MIMEMultiPart object with the specified text and filename.
	* Both textIS and filename cannot be null or an exception is thrown.
	* (See MIMEMessage() if you need to do this.)
	* @param textIS InputStream to text that becomes (first) text part of the multi-part.
	* @param filename Name of the file that becomes second part of the multi-part.
	* @param encoding Encoding for the file attachment. To select a default encoding, use -1.
	* @see #MIMEMessage
	* @see MIMEBodyPart#BASE64
	* @see MIMEBodyPart#QP
	* @see MIMEBodyPart#BINARY
	* @see MIMEBodyPart#E7BIT
	* @see MIMEBodyPart#E8BIT
	* @exception MIMEException If filename is null or textIS null.
	* @exception FileNotFoundException If filename file does not exist
	* @exception SecurityException If filename file can not be accessed
	* @exception IOExcepton On IO Errors on filename or textIS.
	*/
	public MIMEMultiPart (InputStream textIS, String filename, int encoding) throws
			MIMEException, FileNotFoundException, SecurityException, IOException
    	{
		MIMEBasicPart 	l_txtPart;
		MIMEBasicPart 	l_filePart;
		FileInputStream l_fis;
		fileMIMEType	l_fmt;

        	m_parsedPart = 0;
        	m_contentDisposition      =  UNINITIALIZED;
	        m_contentTransferEncoding =  UNINITIALIZED;
		m_needPreamble = false;
		m_bodyParts = new Vector();
		m_boundary = null;

		if (textIS == null)
                 	throw new MIMEException ("Invalid null InputStream");

		if (filename == null)
                 	throw new MIMEException ("Invalid null filename:" + filename);

		if ((encoding != -1) && (encoding < MIMEBodyPart.BASE64 || encoding > MIMEBodyPart.E8BIT))

                 	throw new MIMEException ("Invalid MIME encoding:" + encoding);
		this.setContentSubType ("mixed");

		l_fis = new FileInputStream (filename);

		// The text-part!
		l_txtPart = new MIMEBasicPart (MIMEBasicPart.TEXT);

		l_txtPart.setContentSubType ("plain");
                l_txtPart.setContentTypeParams ("charset=us-ascii");
                l_txtPart.setContentEncoding(MIMEBodyPart.E7BIT);
                //l_txtPart.setContentDisposition(MIMEBodyPart.INLINE);

                l_txtPart.setBodyData (textIS);

		// The file-part
        	l_fmt  = MIMEHelper.getFileMIMEType (filename);

		if (l_fmt == null)
                 	throw new MIMEException ("Can't determine MIME info for file: " + filename);

		l_filePart = new MIMEBasicPart (l_fmt.content_type);

                l_filePart.setContentSubType (l_fmt.content_subtype);

                if (l_fmt.content_params != null)
                        l_filePart.setContentTypeParams (l_fmt.content_params);
                else
                        l_filePart.setContentTypeParams (new String ("name=" + l_fmt.file_shortname));

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

                // Add this part to multi-part
                this.addBodyPart (l_txtPart, false);
                this.addBodyPart (l_filePart, false);
	}



//===========================================================================
//
//                        Methods overridden from MIMEBodyPart
//
//===========================================================================

        /**
         * Returns Content-Type of this MIME Part. Always returns the String "MultiPart".
         */
        public String getContentType ()
        {
                return (MULTIPART);
        }


//===========================================================================
//
//                        Methods Specific to this Class alone.
//
//===========================================================================

	/**
         * Returns the count of BodyParts in this MultiPart or
         * Zero (0) if no parts exist.
         */
	public int getBodyPartCount ()
        {
	     if (m_bodyParts != null)
                  return (m_bodyParts.size());
	     else
                  return (0);
        }

	/**
         * Returns an Object of corresponding MIME BodyPart type at the
         * specified index of this MultiPart.
         * @param index Index for the BodyPart to return. Index ranges from 0 to (count_of_parts -1).
	 * @param clone Whether to return a reference to the internal BodyPart object or a cloned copy;
         * if true: cloned copy; if false: reference to the BodyPart object.
	 * @exception MIMEException If invalid index.
	 * @see MIMEBasicPart
	 * @see MIMEMessagePart
	 * @see MIMEMultiPart
	 * @see MIMEMultiPart#getBodyPartCount
         */
	public Object getBodyPart (int index, boolean clone)	throws MIMEException
        {
	     if (index >= m_bodyParts.size() || index < 0)
                 throw new MIMEException ("getBodyPart(). Index out of range: " + index);

	     Object obj = m_bodyParts.elementAt (index);

	     if (obj instanceof MIMEBasicPart)
	     {
		  MIMEBasicPart part = (MIMEBasicPart) obj;

		  try
		  {
		      if (clone)
	     	      	  return (part.clone());
		      else
	     	      	  return (part);
		  }
		  catch (CloneNotSupportedException e)
		  {
                      throw new MIMEException (e.getMessage());
		  }
	      }
	      else if (obj instanceof MIMEMessagePart)
	      {
		  MIMEMessagePart part = (MIMEMessagePart) obj;

		  try
		  {
		      	if (clone)
	     	      	  	return (part.clone());
		      	else
	     	      	  	return (part);
		  }
		  catch (CloneNotSupportedException e)
		  {
                      throw new MIMEException (e.getMessage());
		  }
	      }
	      else if (obj instanceof MIMEMultiPart)
	      {
		  MIMEMultiPart part = (MIMEMultiPart) obj;

		  try
		  {
		      	if (clone)
	     	      	  	return (part.clone());
		      	else
	     	      	  	return (part);
		  }
		  catch (CloneNotSupportedException e)
		  {
                      throw new MIMEException (e.getMessage());
		  }
	      }
	      else
                   throw new MIMEException ("Invalid bodyPart!");
        }

	/**
         * The following is an internal routine. Not to be exposed at the API.
	 * @exception MIMEException If the multipart is malformed.
	 * @exception IOException If IO error occurs.
         */
	protected void putByteStream (OutputStream os, boolean needPreamble)
							throws IOException, MIMEException
        {
	        if (needPreamble == true)
		    m_needPreamble = true;
		else
		    m_needPreamble = false; // In case we did not reset last pass due exception

		putByteStream (os);

		m_needPreamble = false; // reset after done.
	}

	/**
         * Outputs a byte stream for this Message in MIME format.
	 * Applies transfer encoding all bodyParts as applicable.
         * @param os OutputStream to be written to.
	 * @exception IOException If an IO error occurs.
	 * @exception MIMEException If detects an error during encoding.
         */
	public void putByteStream (OutputStream os) throws IOException, MIMEException
        {
	     int l_bpCount = m_bodyParts.size();

	     if ((m_bodyParts == null) || (l_bpCount < 1))
                 throw new MIMEException ("MIMEMultiPart.putByteStream(). No bodyParts!");

	     // Headers first
             StringBuffer l_hdrbuf = new StringBuffer (HDRBUFSZ);
             byte[]       l_bytebuf;
	     String l_boundary;
	     boolean l_fQBounds=false;

	     if (m_boundary != null && m_boundary.length() > 0)
	     {
	            l_boundary = m_boundary;
		    l_fQBounds = true;
	     }
	     else
	            l_boundary = generateBoundary();

	     // content-type
	     l_hdrbuf.append ("Content-Type: multipart/");

	     if (m_contentSubType != null)
                  l_hdrbuf.append (m_contentSubType);
             else
                  throw new MIMEException ("putByteStream: No content-subtype." );

             if (m_contentTypeParams != null && !m_contentTypeParams.startsWith ("boundary"))
             {
                  l_hdrbuf.append ("; ");
                  l_hdrbuf.append (m_contentTypeParams);
                  l_hdrbuf.append ("; boundary=");
		  if (l_fQBounds)
                      l_hdrbuf.append ('"');
                  l_hdrbuf.append (l_boundary);
		  if (l_fQBounds)
                      l_hdrbuf.append ('"');
                  l_hdrbuf.append ("\r\n");
             }
             else
             {
                  l_hdrbuf.append ("; boundary=");
		  if (l_fQBounds)
                      l_hdrbuf.append ('"');
                  l_hdrbuf.append (l_boundary);
		  if (l_fQBounds)
                      l_hdrbuf.append ('"');
                  l_hdrbuf.append ("\r\n");
             }


             // write the header to os
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

	     // This is the top-level multi-part. Add content-encoding
	     if (m_needPreamble == true && m_contentTransferEncoding != UNINITIALIZED)
	     {
		     boolean fwriteit = false;
		     switch (m_contentTransferEncoding)
                     {
                    	    case BINARY:
                            case E7BIT:
                            case E8BIT:
				l_hdrbuf.setLength (0);
                 		l_hdrbuf.append ("Content-Transfer-Encoding: " +
                               		m_stringEncoding [m_contentTransferEncoding]);
                 		l_hdrbuf.append ("\r\n");
				fwriteit = true;
                                break;
              	     }


                     if (fwriteit) try
                     {
		      	    l_bytebuf = MIMEHelper.unicodeToASCII (l_hdrbuf.toString());
                      	    os.write (l_bytebuf);
                     }
                     catch (Exception e)
                     {
                            throw new MIMEException ("Content-Encodding: " + e.getMessage());
                     }
	     }


	     // blank-line after headers
             //l_bytebuf = new String("\r\n").getBytes("iso-8859-1");
             os.write (CRLF);


	     if (m_preamble != null)
	     {
	          l_bytebuf = MIMEHelper.unicodeToASCII (m_preamble);
                  os.write (l_bytebuf);
	     }
             // Write the line "This is a multi-part message in MIME format".
	     // This needs to be done only if it is the body of the message. Not when it is
	     // embedded part of another part (like another multi-part or a Messagepart).
	     // This is accomplished by passing a flag to putByteStream() by Message class.
	     else if (m_needPreamble == true && m_parsedPart==0)
	     {
		  String l_preamble = new String ("\r\nThis is a multi-part message in MIME format\r\n");
                 // l_bytebuf = l_preamble.getBytes("iso-8859-1");
	          l_bytebuf = MIMEHelper.unicodeToASCII (l_preamble);
                  os.write (l_bytebuf);
	          m_needPreamble = false;
	     }

	     String l_trailBoundary = "\r\n" + "--" + l_boundary + "--" + "\r\n";
	     //l_boundary = "\r\n\r\n" + "--" + l_boundary + "\r\n";
	     l_boundary = "\r\n" + "--" + l_boundary + "\r\n";
	     //l_boundary = "--" + l_boundary + "\r\n";

	     for (int i = 0; i < l_bpCount; i++)
	     {
		  Object obj = m_bodyParts.elementAt (i);

		  if (obj instanceof MIMEBasicPart)
		  {
             		//l_bytebuf = l_boundary.getBytes("iso-8859-1");
	          	l_bytebuf = MIMEHelper.unicodeToASCII (l_boundary);
             		os.write (l_bytebuf);

			MIMEBasicPart part = (MIMEBasicPart) obj;
			part.putByteStream (os);
		  }
		  else if (obj instanceof MIMEMessagePart)
		  {
             		//l_bytebuf = l_boundary.getBytes("iso-8859-1");
	          	l_bytebuf = MIMEHelper.unicodeToASCII (l_boundary);
             		os.write (l_bytebuf);

			MIMEMessagePart part = (MIMEMessagePart) obj;
			part.putByteStream (os);
		  }
		  else if (obj instanceof MIMEMultiPart)
		  {
             		//l_bytebuf = l_boundary.getBytes("iso-8859-1");
	          	l_bytebuf = MIMEHelper.unicodeToASCII (l_boundary);
             		os.write (l_bytebuf);

			MIMEMultiPart part = (MIMEMultiPart) obj;
			part.putByteStream (os);
		  }
		  else
                       throw new MIMEException ("putByteStream(). Invalid bodyPart!");

	     } // end for

	    l_bytebuf = MIMEHelper.unicodeToASCII (l_trailBoundary);
            os.write (l_bytebuf);
        }

	/**
         * Deletes bodyPart at the requested index from this Multipart.
	 * Adjusts indices of any parts after the deleted part upwards
	 * as needed.
         * @param index of the bodyPart to remove.
	 * @exception MIMEException if invalid index.
         */
	public void deleteBodyPart (int index) throws MIMEException
        {
	     if (index > m_bodyParts.size())
                 throw new MIMEException ("deleteBodyPart(). Index out of range: " + index);

	     m_bodyParts.removeElementAt (index);
        }


	/**
	* Adds the specified filename file as a (MIMEBasicPart) BodyPart to this MultiPart.
	* @param filename Name of file to add as bodyPart.
	* @param encoding Preferred MIME encoding for this part. To select a default, use -1.
	* @return The index at which this bodyPart is added.
	* @see MIMEBodyPart#BASE64
	* @see MIMEBodyPart#QP
	* @see MIMEBodyPart#BINARY
	* @see MIMEBodyPart#E7BIT
	* @see MIMEBodyPart#E8BIT
	* @exception MIMEException If filename file is inaccessible or I/O errors.
	* @exception FileNotFoundException If filename file does not exist
	* @exception SecurityException If filename file cannot be accessed
	* @exception IOExcepton On IO Errors on filename
	*/
	public int addBodyPart (String filename, int encoding) throws MIMEException, FileNotFoundException, SecurityException, IOException
	{
		MIMEBasicPart 	l_filePart;
		FileInputStream l_fis;
		fileMIMEType	l_fmt;

		if (filename == null)
                 	throw new MIMEException ("Invalid null filename:" + filename);

		if ((encoding != -1) && (encoding < MIMEBodyPart.BASE64 || encoding > MIMEBodyPart.E8BIT))

                 	throw new MIMEException ("Invalid MIME encoding:" + encoding);

		l_fis = new FileInputStream (filename);

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
                	l_filePart.setContentEncoding (l_fmt.mime_encoding);
		else
                	l_filePart.setContentEncoding (encoding);

		if (l_fmt.content_type == MIMEBasicPart.TEXT)
                      l_filePart.setContentDisposition (MIMEBodyPart.INLINE);
		else
		{
                	l_filePart.setContentDisposition (MIMEBodyPart.ATTACHMENT);
                	l_filePart.setContentDispParams (new String ("filename=" + filename));
                	l_filePart.setContentDescription (filename);
		}

                // set body-data of this part
                l_filePart.setBodyData (l_fis);

                // Add this part to multi-part
                return addBodyPart (l_filePart, false);
	}


	/**
	* Adds a MIMEBodyPart to this MultiPart. This part must already be constructed
	* and fully set-up. As MIMEBodyPart is an abstract class, the object passed in the
	* part parameter must be one of three concrete classes:  MIMEBasicPart, MIMEMultiPart, or
	* MIMEMessagePart. If not, a MIMEException is thrown.
	* @param part bodyPart to add.
	* @param clone Whether to store a reference to the passed object instead of cloning a copy;
        * if true: clones a copy; if false: stores reference to the passed part object.
	* @return The index at which to add this bodyPart.
	* @exception MIMEException If part is malformed or is not an object of one of three
	* classes: MIMEBasicPart, MIMEMultiPart, or MIMEMessagePart.
	* @see MIMEBasicPart
	* @see MIMEMultiPart
	* @see MIMEMessagePart
	*/
	public int addBodyPart (MIMEBodyPart part, boolean clone) throws MIMEException
        {
		if (part == null)
			throw new MIMEException ("addBodyPart(): null part passed!");

		if (part instanceof MIMEBasicPart)
                        return addBodyPart ((MIMEBasicPart) part, clone);
                else if (part instanceof MIMEMessagePart)
                        return addBodyPart ((MIMEMessagePart) part, clone);
                else if (part instanceof MIMEMultiPart)
                        return addBodyPart ((MIMEMultiPart) part, clone);
                else
                        throw new MIMEException ("addBodyPart(): Invalid part ");
        }

	/**
	* Adds a MIMEBasicPart BodyPart to this MultiPart. This part should have been
	* constructed and fully set-up
	* @param part bodyPart to add.
	* @return The index at which this bodyPart is added.
	* @exception MIMEException If it is a malformed bodyPart (or) can not be added.
	* @see MIMEBasicPart
	*/
	protected int addBodyPart (MIMEBasicPart part ) throws MIMEException
	{
	    return addBodyPart( part, true );
	}


	/**
	* Adds a MIMEBasicPart BodyPart to this MultiPart. This part should have been
	* constructed and fully set-up.
	* @param part bodyPart to add.
	* @return The index at which this bodyPart is added.
	* @param clone if false stores reference to passed object instead of cloning a copy.
	* @exception MIMEException If it is a malformed bodyPart (or) can not be added.
	* @see MIMEBasicPart
	*/
	protected int addBodyPart (MIMEBasicPart part, boolean clone) throws MIMEException
        {
		MIMEBasicPart l_part;

		if (part == null)
			throw new MIMEException ("addBodyPart(): null part passed!");

		if (clone)
		{
    	     		try
    	     		{
    	         		l_part = (MIMEBasicPart) part.clone();
                 		m_bodyParts.addElement (l_part);
    	     		}
    	     		catch (CloneNotSupportedException e)
    	     		{
                 		throw new MIMEException (e.getMessage());
    	     		}
		}
		else
	        	m_bodyParts.addElement (part);

		return (m_bodyParts.size() -1);
        }


	/**
         * Adds a MIMEBasicPart BodyPart to this MultiPart. This part should have been
         * constructed and fully set-up
         * @param part bodyPart to add.
	 * @return The index at which this bodyPart is added.
	 * @exception MIMEException If it is a malformed bodyPart (or) can not be added.
         * @see MIMEBasicPart
         */
  	protected int addBodyPart (MIMEMultiPart part) throws MIMEException
  	{
  	    return addBodyPart( part, true );
  	}


	/**
	* Adds a MIMEMultiPart BodyPart to this MultiPart. This part should have been
	* constructed and fully set-up.
	* @param part bodyPart to add.
	* @param clone if false stores reference to passed object instead of cloning a copy.
	* @return The index at which this bodyPart is added.
	* @exception MIMEException If it is a malformed bodyPart (or) can not be added.
	* clone made optional
	*/
	protected int addBodyPart (MIMEMultiPart part, boolean clone) throws MIMEException
        {
		MIMEMultiPart l_part;

		if (part == null)
			throw new MIMEException ("addBodyPart(): null part passed!");

		if (clone)
		{
			try
			{
				l_part = (MIMEMultiPart) part.clone();
				m_bodyParts.addElement (l_part );
			}
			catch (CloneNotSupportedException e)
			{
				throw new MIMEException (e.getMessage());
			}
		}
		else
			m_bodyParts.addElement (part);

		return (m_bodyParts.size() -1);
        }


	/**
	* Adds a MIMEBasicPart BodyPart to this MultiPart. This part should have been
	* constructed and fully set-up.
	* @param part bodyPart to add.
	* @return The index at which this bodyPart is added.
	* @exception MIMEException If it is a malformed bodyPart (or) can not be added.
	* @see MIMEBasicPart
	*/
	protected int addBodyPart (MIMEMessagePart part) throws MIMEException
	{
	    return addBodyPart( part, true );
	}

	/**
	* Adds a MIMEMessagePart BodyPart to this MultiPart. This part should have been
	* constructed and fully set-up.
	* @param part bodyPart to add.
	* @param clone if false stores reference to passed object instead of cloning a copy.
	* @return The index at which this bodyPart is added.
	* @exception MIMEException If it is a malformed bodyPart (or) can not be added.
	*/
	protected int addBodyPart (MIMEMessagePart part, boolean clone) throws MIMEException
        {
		MIMEMessagePart l_part;

		if (part == null)
			throw new MIMEException ("addBodyPart(): null part passed!");

		if (clone)
		{
			try
			{
				l_part = (MIMEMessagePart) part.clone();
				m_bodyParts.addElement (l_part);
			}
			catch (CloneNotSupportedException e)
			{
				throw new MIMEException (e.getMessage());
			}
		}
		else
			m_bodyParts.addElement (part);

		return (m_bodyParts.size() -1);
        }

	protected void setContentEncoding (int encoding) //throws MIMEException
	{
                switch (encoding)
                {
                    case BINARY:
                    case E7BIT:
                    case E8BIT:
                        m_contentTransferEncoding = encoding;
                        break;
                    default:
                         //throw new MIMEException ("Invalid  Content Transfer Encoding : " + encoding);
              }
        }

        /**
         * Returns the preamble for this multi-part if present.
	 * Returns null if preamble does not exist for this part.
	 * @return Preamble for the multi-part if present, null otherwise.
         */
        public String getPreamble ()
        {
		return m_preamble;
	}

        /**
         * Sets the preamble for this multi-part.
	 * @param preamble Preamble string.
         */
        public void setPreamble (String preamble)
        {
		m_preamble = preamble;
	}

        protected void addPreamble (byte[] preamble, int len )
        {
		if (m_preamble == null)
		   m_preamble = new String (preamble, 0, len);
		else
		{
		   m_preamble = new String (m_preamble + new String (preamble, 0, len));
		}
        }

        protected void addPreamble (byte[] preamble)
        {
		if (m_preamble == null)
		   m_preamble = new String (preamble);
		else
		{
		   m_preamble = new String (m_preamble + new String (preamble));
		}
	}

        /**
         * Generates and returns a boundary string that can be used in multi-parts etc.
         * @return The boundary string.
         */
        private String generateBoundary ()
        {
		Random l_rand = new Random (System.currentTimeMillis());
		long l_numboundary = l_rand.nextLong();
		String l_boundary = new String ("-----" + Long.toHexString(l_numboundary));
		return (l_boundary);
        }


	/**
	 * Clones an instance of this MIMEMultiPart object.
	 * @exception CloneNotSupportedException If thrown by constituent components.
         */
	public Object clone () throws CloneNotSupportedException
        {
  	     MIMEMultiPart l_theClone = (MIMEMultiPart) super.clone();

  	     // Take care of all other "reference"s.
  	     if (m_bodyParts != null)
  		    l_theClone.m_bodyParts = (Vector) m_bodyParts.clone();

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

	     m_needPreamble = false;
	     l_theClone.m_UserObject = m_UserObject;

 	     return (l_theClone);

        } // end clone ()


    // Should not be exposed at the API level. Internal only routines.
    protected void setBoundary( String boundary )
    {
        if ( boundary != null )
            m_boundary = boundary;
    }

    protected String getBoundary() { return m_boundary; }

} // end class MIMEMultiPart
