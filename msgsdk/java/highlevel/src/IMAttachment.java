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

package netscape.messaging.convenience;

import java.io.*;
import java.net.*;
import java.util.*;

/**
 * The IMAttachment class represents a MIME attachment for an
 * email message.
 * @author Prasad Yendluri
 */
public class IMAttachment
{
	protected int 		content_type;
	protected String 	content_subtype;
	protected String 	content_params;
        protected int  		content_disposition;
	protected int		mime_encoding;
        protected InputStream  	attach_stream;
	protected String	attach_file;

       /**
	 * Constructor for an IMAttachment object.
         *
	 * @param dataStream InputStream to attachment data. Cannot be null.
	 * @param contentPrimaryType Primary content-type of the message.
         * @see IMTransport#TEXT
         * @see IMTransport#AUDIO
         * @see IMTransport#IMAGE
         * @see IMTransport#VIDEO
         * @see IMTransport#APPLICATION
         * @param contentSubType Strings representing the content sub-types.
         * @param contentParameters Strings representing the content-type parameters.
         * @param contentDisposition Can only be IMTransport.INLINE or IMTransport.ATTACHMENT
         * @see IMTransport#INLINE
         * @see IMTransport#ATTACHMENT
         * @param encodings Can only be IMTransport.BASE64 or IMTransport.QP or IMTransport.NONE
         * @see IMTransport#BASE64
         * @see IMTransport#QP
         * @see IMTransport#NONE
	 */
	public IMAttachment (InputStream dataStream,
			     int contentPrimaryType,
			     String contentSubType,
			     String contentParameters,
			     int contentDisposition,
			     int encoding) throws IMException
        {
		if (dataStream == null)
			throw new IMException ("Invalid Parameter: dataStream = null");

		if (contentPrimaryType < IMTransport.TEXT ||
		    contentPrimaryType > IMTransport.APPLICATION)
			throw new IMException ("Invalid Parameter: contentPrimaryType = "
						+ contentPrimaryType);
		if (contentSubType == null)
			throw new IMException ("Invalid parameter: contentSubType = null");

		if (contentDisposition > 0 && contentDisposition != IMTransport.INLINE &&
		     contentDisposition != IMTransport.ATTACHMENT)
			throw new IMException ("Invalid parameter: contentDisposition = "
						+ contentDisposition);

		if (encoding > -1 && encoding != IMTransport.BASE64 && encoding != IMTransport.QP &&
		    encoding != IMTransport.NONE)
			throw new IMException ("Invalid parameter: encoding = " + encoding);

		attach_stream = dataStream;
		attach_file = null;
		content_type 	= contentPrimaryType;
		content_subtype = contentSubType;
		content_params 	= contentParameters;
		content_disposition = contentDisposition;
		mime_encoding = encoding;
        }


       /**
	 * Constructor for an IMAttachment object that includes a filename.
	 * @param fullfileName Full name (including path) of a file that contains attachment data.
	 * @param contentPrimaryType Primary content-type of the message.
         * @see IMTransport#TEXT
         * @see IMTransport#AUDIO
         * @see IMTransport#IMAGE
         * @see IMTransport#VIDEO
         * @see IMTransport#APPLICATION
         * @param contentSubType Strings representing the content sub-types.
         * @param contentParameters Strings representing the content-type parameters.
         * @param contentDisposition Can only be IMTransport.INLINE or IMTransport.ATTACHMENT
         * @see IMTransport#INLINE
         * @see IMTransport#ATTACHMENT
         * @param encodings Can only be IMTransport.BASE64 or IMTransport.QP or IMTransport.NONE
         * @see IMTransport#BASE64
         * @see IMTransport#QP
         * @see IMTransport#NONE
	 */
	public IMAttachment (String fullfileName,
			     int contentPrimaryType,
			     String contentSubType,
			     String contentParameters,
			     int contentDisposition,
			     int encoding) throws IMException
        {

		if (fullfileName == null)
			throw new IMException ("Invalid Parameter: fullfileName = null");

		if ((contentPrimaryType > -1) && (contentPrimaryType < IMTransport.TEXT ||
		    contentPrimaryType > IMTransport.APPLICATION))
			throw new IMException ("Invalid Parameter: contentPrimaryType = "
						+ contentPrimaryType);

		if (contentSubType == null && contentPrimaryType > -1)
			throw new IMException ("Invalid parameter: contentSubType = null");

		if (contentDisposition > 0 && contentDisposition != IMTransport.INLINE &&
		     contentDisposition != IMTransport.ATTACHMENT)
			throw new IMException ("Invalid parameter: contentDisposition = "
						+ contentDisposition);
		if (encoding > -1 && encoding != IMTransport.BASE64 && encoding != IMTransport.QP &&
		    encoding != IMTransport.NONE)
			throw new IMException ("Invalid parameter: encoding = " + encoding);

		attach_stream = null;
		attach_file = fullfileName;
		content_type 	= contentPrimaryType;
		content_subtype = contentSubType;
		content_params 	= contentParameters;
		content_disposition = contentDisposition;
		mime_encoding = encoding;
        }
}
