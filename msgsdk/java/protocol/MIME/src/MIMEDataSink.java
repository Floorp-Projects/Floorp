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
 * The MIMEDataSink class represents the DataSink that implements
 * callbacks. Clients can subclass from this abstract class.
 * @author Carson Lee
 */
abstract public class MIMEDataSink
{
     /**
      * Default constructor
      */
      public MIMEDataSink ()
      {
      }


     /**
      * Callback that supplies header information.
      * @param callbackObject Client-supplied opaque object.
      * @param name Name of the header.
      * @param value Value of the header.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void header (Object callbackObject, byte[] name, byte[] value ){}

     /**
      * Callback that supplies additional value for a header.
      * @param callbackObject Client-supplied opaque object.
      * @param name Name of the header.
      * @param value Value of the header.
      * @see #header
      */
     public void addHeader (Object callbackObject, byte[] name, byte[] value ){}


     /**
      * Callback to indicate end of headers on the top level message.
      * @param callbackObject Client-supplied opaque object.
      * @see #startMessage
      * @see #header
      */
     public void endMessageHeader (Object callbackObject){}


     /**
      * Callback that supplies contentType information.
      * @param callbackObject Client-supplied opaque object.
      * @param nContentType Content type.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     //public void contentType( Object callbackObject, int nBodyPartType){}
     public void contentType( Object callbackObject, byte[] contentType){}

     /**
      * Callback that supplies contentSubType information.
      * @param callbackObject Client-supplied opaque object.
      * @param contentSubType Content subtype.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentSubType( Object callbackObject, byte[] contentSubType ){}

     /**
      * Callback that supplies contentTypeParams.
      * @param callbackObject Client-supplied opaque object.
      * @param contentTypeParams Content type parameters.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentTypeParams( Object callbackObject, byte[] contentTypeParams ){}

     /**
      * Callback that supplies ContentID.
      * @param callbackObject Client-supplied opaque object.
      * @param contentID Content identifier.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentID( Object callbackObject, byte[] ContentID ){}

     /**
      * Callback that supplies contentMD5.
      * @param callbackObject Client-supplied opaque object.
      * @param contentMD5 Content MD5 information.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentMD5( Object callbackObject, byte[] contentMD5 ){}

     /**
      * Callback that supplies ContentDisposition.
      * @param callbackObject Client-supplied opaque object.
      * @param nContentDisposition Content Disposition type.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentDisposition( Object callbackObject, int nContentDisposition ){}

     /**
      * Callback that supplies contentDispParams.
      * @param callbackObject Client-supplied opaque object.
      * @param contentDispParams Content Disposition parameters.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentDispParams( Object callbackObject, byte[] contentDispParams ) {}

     /**
      * Callback that supplies contentDescription.
      * @param callbackObject Client-supplied opaque object.
      * @param contentDescription Content Description.
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentDescription( Object callbackObject, byte[] contentDescription ){}

     /**
      * Callback that supplies ContentEncoding.
      * @param callbackObject Client-supplied opaque object.
      * @param nContentEncoding Content Encoding type. For values, see "MIME Encoding Types."
      * @see #startMessage
      * @see #startBasicPart
      * @see #startMultiPart
      * @see #startMessagePart
      */
     public void contentEncoding( Object callbackObject, int nContentEncoding ){}

     /**
      * Callback that indicates start of a new MIMEMessage.
      * No reference to MIMEMessage object is kept internally.
      * @return Object Client-supplied opaque object to be passed to subsequent callbacks.
      * @see #endMessage
      */
     public Object startMessage(){ return null; }

     /**
      * Callback that indicates end of MIMEMessage.
      * @param callbackObject Client-supplied opaque object.
      * @see #startMessage
      */
     public void endMessage(Object callbackObject) {}

     /**
      * Callback that indicates start of a new MIMEBasicPart.
      * No reference to MIMEBasicPart object is kept internally.
      * @return Object Client-supplied opaque object to be passed to subsequent callbacks.
      * @see #endBasicPart
      */
     public Object startBasicPart(){ return null; }

     /**
      * No reference to MIMEBasicPart object is kept internally.
      * @param callbackObject Client-supplied opaque object.
      * @param input Input stream for body data.
      * @param len Length of buffer
      */
     public void bodyData( Object callbackObject, InputStream input, int len ) {}

     /**
      * Callback that indicates end of the MIMEBasicPart.
      * @param callbackObject Client-supplied opaque object.
      * @see #startBasicPart
      */
     public void endBasicPart(Object callbackObject) { }

     /**
      * Callback that indicates start of a new MIMEMultiPart.
      * No reference to MIMEMultiPart object is kept internally.
      * @return Object Client-supplied opaque object to be passed to subsequent callbacks.
      * @see #endMultiPart
      */
     public Object startMultiPart(){ return null; }

     /**
      * Callback that suppiles the boundary string.
      * @param callbackObject Client-supplied opaque object.
      * @param boundary Encapsulation boundary that separates sub-body parts in a MultiPart.
      * @see #startMessagePart
      */
     public void boundary (Object callbackObject, byte[] boundary ){}

     /**
      * Callback that indicates end of the MultiPart.
      * @param callbackObject Client-supplied opaque object.
      * @see #startMultiPart
      */
     public void endMultiPart( Object callbackObject ){}

     /**
      * Callback that indicates start of a new MIMEMessagePart.
      * No reference to MIMEMessagePart object is kept internally.
      * @return Object Client-supplied opaque object to be passed to subsequent callbacks.
      * @see #endMessagePart
      */
     public Object startMessagePart(){ return null; }

     /**
      * Callback that indicates end of the MessagePart.
      * @param callbackObject Client-supplied opaque object.
      * @see #startMessagePart
      */
     public void endMessagePart( Object callbackObject ) {}
}


