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


/**
 * The MIMEBodyPart class represents a body-part of a MIME message.
 * MIMEBodyPart is the base class for all MIME BodyPart classes:
 * MIMEBasicPart, MIMEMessagePart, and MIMEMultiPart.
 * @author Prasad Yendluri
 */
public abstract class MIMEBodyPart implements Cloneable
{
	/**
         * Content Disposition is Attachment.
         */
	public static final int ATTACHMENT      = 0;
	/**
         * Content Disposition is inline.
         */
        public static final int INLINE          = 1;


	/**
         * Base64 Transfer Encoding.
         */
         public static final int BASE64          = 0;
        /**
         * Quoted Printable Transfer Encoding.
         */
        public static final int QP              = 1;
        /**
         * Binary Data with No Transfer Encoding.
         */
        public static final int BINARY          = 2;
        /**
         * 7bit Data with No Transfer Encoding.
         */
        public static final int E7BIT           = 3;
        /**
         * 8bit Data with No Transfer Encoding.
         */
        public static final int E8BIT           = 4;


//===========================================================================
//
//              Internal members not visible at the API
//
//===========================================================================

	public static final int UNINITIALIZED = -1;
	protected static final int DATABUFSZ  = 8192;
	protected static final int HDRBUFSZ   = 1024;

	protected String          m_contentSubType;
        protected String          m_contentTypeParams;
        protected String          m_contentID;
        protected int             m_contentDisposition;
        protected String          m_contentDispParams;
        protected String          m_contentDescription;

	protected static final byte[] CRLF = "\r\n".getBytes();
        protected static final byte[] LF = "\n".getBytes();
    	protected Object m_UserObject;


//===========================================================================
//
//              get() and set() Methods
//
//===========================================================================

	/**
         * Returns Content-Type of this MIME Part.
	 * Note: The primary content-type is determined by the
	 * actual MIMEPart class and thus cannot be set.
         * @return String containing the Content-Type or
	 * NULL if Content-Type cannot be determined.
         */
	public abstract String getContentType ();

	/**
         * Returns Content SubType of this MIME Part.
	 * @return String containing the Content-SubType or
	 * NULL if Content-SubType cannot be determined.
         */
	public String getContentSubType ()
	{
		if (m_contentSubType == null)
                   return (null);

                return (m_contentSubType);
	}

	/**
         * Sets the Content SubType of this MIME Part.
	 * Note: The primary content-type is determined by the
	 * actual MIMEPart class and thus cannot be set.
         * @param subType String to use as sub-type for this part.
	 * @exception MIMEException  If the subtype parameter is null.
         */
	public void setContentSubType (String subType) throws MIMEException
        {
		// It is client's responsibility to check the consistency with content-type etc.
                // We can not really enforce this as, new sub-types can be added any time.

		if (subType != null)
			m_contentSubType = subType;
		else
			throw new MIMEException ("Invalid null Content-SubType");
        }

	/**
         * Returns Content-Type Parameters of this MIME Part.
	 * @return String containing the Content-Type Parameters or
         * NULL if no parameters exist.
         */
	public String getContentTypeParams ()
	{
		if (m_contentTypeParams == null)
                   return (null);

                return (m_contentTypeParams);
	}

	/**
         * Sets Content-Type Parameters of this MIME Part.
         * @param params String to use as Content-Type Parameters.
         */
	public void setContentTypeParams (String params)
        {
                // It is client's responsibility to check the consistency with content-type etc.
                // We can not really enforce this as, new kind of params can be added any time.

		if (params != null)
			m_contentTypeParams = params;
		else
			m_contentTypeParams = null;
        }

	/**
         * Returns Content-ID of this MIME Part.
	 * @return String containing the Content-ID Parameters or
         * NULL if none is present.
         */
	public String getContentID ()
	{
                if (m_contentID == null)
                   return (null);

                return (m_contentID);
        }

	/**
         * Sets Content-ID of this MIME Part.
         * @param cid String to use as Content-ID.
         */
	public void setContentID (String cid)
	{
		if (cid != null)
                    m_contentID = cid;
		else
		     m_contentID = null;
	}

	/**
         * Returns Content-Disposition of this MIME Part.
	 * @return Content-Disposition of this MIME Part or
	 * -1 if none present.
         * @see MIMEBodyPart#ATTACHMENT
         * @see MIMEBodyPart#INLINE
         */
	public int getContentDisposition ()
	{
		 if (m_contentDisposition ==  UNINITIALIZED)
                  	return (-1);
             	else
                  	return (m_contentDisposition);
	}

	/**
         * Sets Content-Disposition of this MIME Part.
         * @param disposition Value of the Content-Disposition. Must be ATTACHMENT or INLINE.
         * @see MIMEBodyPart#ATTACHMENT
         * @see MIMEBodyPart#INLINE
	 * @exception MIMEException If invalid disposition value is passed.
         */
	public void setContentDisposition (int disposition) throws MIMEException
	{
		if ((disposition != MIMEBodyPart.ATTACHMENT) && (disposition != MIMEBodyPart.INLINE))
             	{
                 	throw new MIMEException ("Invalid ContentDisposition:  " + disposition);
             	}

             	m_contentDisposition = disposition;
	}

	/**
         * Returns Content-Disposition Parameters of this MIME Part.
	 * @return String containing the Content Disposition Parameters or
         * NULL if none exist.
         */
	public String getContentDispParams ()
	{
		if (m_contentDispParams == null)
                   return (null);

                return (m_contentDispParams);
	}

	/**
         * Sets Content-Disposition Parameters of this MIME Part.
         * @param params String to be used as Content-Disposition Parameters.
         */
	public void setContentDispParams (String params)
        {
		if (params != null)
			m_contentDispParams = params;
		else
			m_contentDispParams = null;
        }

	/**
         * Returns Content-Description of this MIME Part. NULL if none present.
         */
	public String getContentDescription ()
	{
		if (m_contentDescription == null)
                   return (null);

                return (m_contentDescription);
	}

	/**
         * Sets Content-Description of this MIME Part.
         * @param description String to be used as Content-Description.
         */
	public void setContentDescription (String description)
	{
		if (description != null)
			m_contentDescription = description;
		else
			m_contentDescription = null;
	}

	public Object clone () throws CloneNotSupportedException
        {
                return (super.clone());
        }

    protected Object getUserObject() { return m_UserObject; }
    protected void setUserObject( Object userObject ) { m_UserObject = userObject; }

}
