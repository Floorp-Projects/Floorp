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

/**
 * The MIMEParser class represents a MIME parser.
 * This generic parser takes an email message encoded in
 * MIME and decodes it into a set of usable structures.
 * @author Carson Lee
 * @version 1.0
 * Dec 17,1997
 */

package netscape.messaging.mime;

import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Vector;
import java.io.InputStream;
import java.io.ByteArrayInputStream;
import java.util.Enumeration;
import netscape.messaging.mime.MIMEMessage;
import netscape.messaging.mime.MIMEBasicPart;
import netscape.messaging.mime.MIMEMultiPart;
import netscape.messaging.mime.MIMEMessagePart;
import netscape.messaging.mime.MIMEHelper;
import netscape.messaging.mime.MIMEDataSink;
import netscape.messaging.mime.log;




public final class MIMEParser
{
    // used internally by parser to track information
	final class mimeInfo
	{
	    int m_type;             // content type
	    byte[] m_name;          // header name
            byte[] m_value;         // header value
            Vector m_param;         // header param

	    mimeInfo( int type1, byte[] name1, byte[] value1 )
	    {
	        m_type = type1;
	        m_name = name1;
	        m_value = value1;
	        m_param = new Vector();
	    }

	    mimeInfo()
	    {
	        m_type = 0;
	        m_name = null;
	        m_value = null;
	        m_param = new Vector();
	    }

	    mimeInfo( int type1, byte[] name1 )
	    {
	        m_type = type1;
	        m_name = name1;
	        m_param = new Vector();
	    }
	}

    final static int MIME_INFO_CONTENT_TYPE     		    =	1;
    final static int MIME_INFO_CONTENT_SUBTYPE     		    =	2;
    final static int MIME_INFO_CONTENT_TRANSFER_ENCODING    =	3;
    final static int MIME_INFO_CONTENT_DISPOSITION  		=	4;
    final static int MIME_INFO_CONTENT_ID            		=	5;
    final static int MIME_INFO_CONTENT_DESCRIPTION	    	=	6;
    final static int MIME_INFO_CONTENT_BASE     	    	=	7;
    final static int MIME_INFO_MIME_VERSION         		=	8;

    final static private int MIME_INFO          =   31;
    final static private int MIME_HEADER        =   32;
    final static private int MIME_XHEADER       =   33;
    final static private int MIME_MESSAGE_DATA  =   34;
    final static private int MIME_PARAM         =   35;
    final static private int MIME_BOUNDARY      =   36;
    final static private int MIME_CRLF          =   37;

    final static private int MIME_UNINITIALIZED = -88;
    final static private int MIME_CONTENT_PARSE_ALL = -99;

    final static private int NSMAIL_ERR_INVALIDPARAM = -1;
    final static private int NSMAIL_ERR_OUTOFMEMORY = -2;
    final static private int NSMAIL_OK = 0;

    final static private int NOT_A_BOUNDARY = 21;
    final static private int START_BOUNDARY = 22;
    final static private int END_BOUNDARY   = 23;
    final static int BUFFER_SIZE = 2048;

    final static int SUBTYPE_RFC822         = 1;
    final static int SUBTYPE_EXTERNAL_BODY  = 2;
    final static byte[]  mb_MultiPart   = "Multipart".getBytes();
    //final static byte[]  mb_MessagePart = "Messagepart".getBytes();
    final static byte[]  mb_MessagePart = "Message".getBytes();
    final static byte[]  mb_TextPart    = "Text".getBytes();
    final static byte[]  mb_AudioPart   = "Audio".getBytes();
    final static byte[]  mb_VideoPart   = "Video".getBytes();
    final static byte[]  mb_ImagePart   = "Image".getBytes();
    final static byte[]  mb_ApplPart    = "Application".getBytes();
    private static final byte[] CRLF = "\r\n".getBytes();

    private Vector m_messageData;   // text buffer
    private Vector m_mimeInfo;      // message info
    private Vector m_msgParts;      // message info

    private MIMEMessage m_mimeMessage;      // entire decoded mime message
    private int m_nMessageType;             // message type, ie: message, basicpart, etc.

    private Object m_currentMessage;        // current message being processed
    private MIMEBasicPart m_currentBasicPart;
    private MIMEMultiPart m_currentMultiPart;
    private MIMEMessagePart m_currentMessagePart;

    private int m_nCurrentMessageType;      // current message type

    private boolean m_bStartData;               // TRUE if start of data
    private int m_emptyLineNo;                  // TRUE if current line is empty
    private MIMEDataSink m_dataSink;            // callback datasink
    private byte[] m_leftoverBuffer;            // leftover buffer
    private byte[] m_inputBuffer;               // inputbuffer
    private int m_leftoverBytes;                // left over bytes from previous parse
    private int m_out_byte;                     // state info
    private int m_out_bits;                     // state info
    private boolean m_bParseEntireFile;         // false if parsing in chunks
    private byte[] m_QPLeftoverBuffer;            // leftover buffer
    private int m_QPLeftoverBytes;                // left over bytes from previous parse
    private Vector m_currentParent;             // current message parent
    private MIMEMessage m_currentMimeMessage;             // current mime message
    private boolean m_bDecodeData = true;
    private boolean m_bLocalStorage = true;
    private MIMEMessage m_nextMimeMessage;
    private int m_lastBoundry;
    private Vector m_mimeInfoQueue;
    private Vector m_headerQueue;
    private boolean m_qp = false;
    private boolean m_readCR = false;
    private Object m_headerParent;
    private Object m_nextHeaderParent;
    private String m_previousHeaderName;
    private int m_messagePartSubType;
    private boolean m_fSeenBoundary = false;
    private boolean m_fEndMessageHeader = false;


    // exposed routines ------------------------------------------------------------



    /**
    * Default Constructor. Use with static parsing
    * when the entire message to be parsed is available.
    * @see #parseEntireMessage
    *
    */
    public MIMEParser()
    {
        this( (MIMEMessage) null);
    }




    /**
      * Parses an entire message from an input stream.
      * @param input User's input stream; source of message to parse.
      * @return New parsed MIMEMessage object.
      * @exception MIMEException If any parameter is NULL.
      */
    public MIMEMessage parseEntireMessage( InputStream input ) throws MIMEException
    {
        init( null );
        return (MIMEMessage) parseMimeMessage( input, MIME_CONTENT_PARSE_ALL );
    }




    // internal routines ------------------------------------------------------------




    /**
    * @param  dataSink User's datasink for callbacks. Can not be null.
    * @return New MIMEParser object
    * @exception none
    */
    protected MIMEParser (MIMEDataSink dataSink, boolean decodeData, boolean localStorage) throws MIMEException
    {
        this( (MIMEMessage) null );

        if ( dataSink == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

        m_dataSink = dataSink;
        m_bParseEntireFile = false;
        m_bDecodeData = decodeData;
        m_bLocalStorage = localStorage;
    }




    /**
    * Begin parse / reset parser.
    * Only used for callbacks
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  none
    * @return none
    * @exception none
    */
    protected void beginParse() throws MIMEException
    {
        m_leftoverBytes = 0;
        m_QPLeftoverBytes = 0;
        m_out_byte = 0;
        m_out_bits = 0;

        if ( m_dataSink == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

        reInit (null); 

        //if ( m_bLocalStorage )
        //    m_mimeMessage.setUserObject( m_dataSink.startMessage( m_mimeMessage ) );
        //else
            m_mimeMessage.setUserObject( m_dataSink.startMessage() );
    }




    /**
    * Parse incoming data.  Only used for callbacks
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  input User's inputstream.  Data will be read off this inputstream.
    * @return none
    * @exception MIMEException
    */
    protected void parse( InputStream input ) throws MIMEException
    {
        parseMimeMessage( input, MIME_CONTENT_PARSE_ALL );
    }



    /**
    * End parse.  Tell parse there is no more data.
    * Used only for callbacks.
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  none
    * @return none
    * @exception MIMEException
    */
    protected void endParse() throws MIMEException
    {
        decodeDataBuffer();

        if ( m_dataSink == null )
            return;

        MIMEBodyPart m = (MIMEBodyPart) m_currentMessage;

         // take care of leftover bytes
        if ( m_nCurrentMessageType == MIMEMessage.BASICPART )
        {
		MIMEBasicPart mimeBasicPart = (MIMEBasicPart) m_currentMessage;

		/* base64 */
		if ( mimeBasicPart.getContentEncoding() == MIMEBodyPart.BASE64 && m_out_bits > 0 )
            	{
        		byte[] decodedBuffer = MIMEHelper.decodeBase64LeftOverBytes( m_out_bits, m_out_byte );
                	saveBodyData( new ByteBuffer( decodedBuffer ), decodedBuffer.length, mimeBasicPart );
            	}

// will never happen
/*
		else if ( mimeBasicPart.getContentEncoding() == MIMEBodyPart.QP && m_QPLeftoverBytes > 0 )
            	{
    			int[] param = new int[2];
    			param[0] = m_QPLeftoverBytes;

	            byte[] decodedBuffer = MIMEHelper.decodeQPVector( m_messageData,
                                					mimeBasicPart.getStartMessageDataIndex(),
                                					mimeBasicPart.getEndMessageDataIndex(),
                                					param,
                                					m_QPLeftoverBuffer );
                if ( decodedBuffer != null )
                {
                    m_QPLeftoverBytes = param[0];
        			param = null;
                    saveBodyData( new ByteBuffer( decodedBuffer ), param[1], mimeBasicPart );
                }
            }
*/
            m_dataSink.endBasicPart( m.getUserObject() );
        }
        else if ( m_nCurrentMessageType == MIMEMessage.MULTIPART )
	{
            m_dataSink.endMultiPart( m.getUserObject() );
	}
        else if ( m_nCurrentMessageType == MIMEMessage.MESSAGEPART )
	{
            m_dataSink.endMessagePart( m.getUserObject() );
	    m_msgParts.removeElement (m); 
	}

       
       // systematically invoke end* callback on all pending parts.
       int nCurrentParentType = getCurrentParentType();			

        while (nCurrentParentType != MIME_UNINITIALIZED)		
        {								
            switch (nCurrentParentType)					
            {								
                case MIMEMessage.MULTIPART:				
                        //MIMEMultiPart mp = (MIMEMultiPart) m_currentParent.lastElement();;	
                        //m_dataSink.endMultiPart (mp.getUserObject());	
			// ingore. taken care of at END_BOUNDARY!	
                        break;						
                case MIMEMessage.MESSAGEPART:				
                        MIMEMessagePart msgP = (MIMEMessagePart) m_currentParent.lastElement();	
			String cst = msgP.getContentSubType();		
			if (cst.equalsIgnoreCase ("rfc822"))		
			{						
                            // if it is rfc822. do endMessage() also  	
			    MIMEMessage mmm = msgP.getMessage();	
                            m_dataSink.endMessage (mmm.getUserObject());
			}						
                        m_dataSink.endMessagePart (msgP.getUserObject()); 
	    		m_msgParts.removeElement (msgP); 		
                        break;						
            }								

            m_currentParent.removeElementAt (m_currentParent.size() - 1); 
            nCurrentParentType = getCurrentParentType();		
       	    //System.out.println ("placeyy> currentParentType = " + nCurrentParentType);
        }								

	// now check for any leftover ones still!			
	int partCount = m_msgParts.size();				

	while (partCount > 0)						
	{
		MIMEMessagePart msgP = (MIMEMessagePart) m_msgParts.elementAt(partCount-1); 
		String cst = msgP.getContentSubType();			

		if (cst.equalsIgnoreCase ("rfc822"))			
		{							
                      // if it is rfc822. do endMessage() also 		
		      MIMEMessage mmm = msgP.getMessage();		
                      m_dataSink.endMessage (mmm.getUserObject());	
		}							

                m_dataSink.endMessagePart (msgP.getUserObject()); 	
	    	m_msgParts.removeElement (msgP); 			

		partCount--;						
	}


        m_dataSink.endMessage( m_mimeMessage.getUserObject() );
        //checkForEmptyMessages( m_mimeMessage );
    }




    /**
    * General constructor
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  m User's MimeMessage object.  Will populate data into this object.
    * @return none
    * @exception none
    */
    protected MIMEParser( MIMEMessage m )
    {
        init( m );
    }



    /**
    * General constructor
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  m User's MimeMessage object.  Will populate data into this object.
    * @return none
    * @exception none
    */
    protected void init( MIMEMessage m )
    {
        m_messageData = new Vector();
        m_mimeInfo = new Vector();
        m_msgParts = new Vector();
        m_mimeMessage = m == null ? new MIMEMessage() : m;
        m_bStartData = false;
	m_mimeMessage.m_parsedPart = 1; 
        m_nMessageType = MIME_UNINITIALIZED;
        m_nCurrentMessageType = MIME_UNINITIALIZED;
        m_currentMessage = null;
        m_dataSink = null;
        m_leftoverBuffer = new byte[2048];
        m_inputBuffer = new byte[2048];
        m_out_byte = 0;
        m_out_bits = 0;
        m_currentBasicPart = null;
        m_currentMultiPart = null;
        m_currentMessagePart = null;
        m_bParseEntireFile = true;
        m_currentParent = new Vector();
        m_currentMimeMessage = m_mimeMessage;
        m_currentParent.addElement( m_mimeMessage );
        m_nextMimeMessage = null;
        m_lastBoundry = START_BOUNDARY;
        m_mimeInfoQueue = new Vector();
        m_headerQueue = new Vector();
        m_QPLeftoverBuffer = new byte[2];
        m_qp = false;
        m_readCR = false;
        m_headerParent = m_mimeMessage;
        m_nextHeaderParent = null;
        m_previousHeaderName = null;
        m_messagePartSubType = SUBTYPE_RFC822;
        m_fEndMessageHeader = false;
    }

    
    protected void reInit( MIMEMessage m )
    {
        m_messageData = new Vector();
        m_mimeInfo = new Vector();
        m_msgParts = new Vector();
        m_mimeMessage = m == null ? new MIMEMessage() : m;
        m_bStartData = false;
	m_mimeMessage.m_parsedPart = 1; 
        m_nMessageType = MIME_UNINITIALIZED;
        m_nCurrentMessageType = MIME_UNINITIALIZED;
        m_currentMessage = null;
        //m_dataSink = null;
        m_leftoverBuffer = new byte[2048];
        m_inputBuffer = new byte[2048];
        m_out_byte = 0;
        m_out_bits = 0;
        m_currentBasicPart = null;
        m_currentMultiPart = null;
        m_currentMessagePart = null;
        m_bParseEntireFile = true;
        m_currentParent = new Vector();
        m_currentMimeMessage = m_mimeMessage;
        m_currentParent.addElement( m_mimeMessage );
        m_nextMimeMessage = null;
        m_lastBoundry = START_BOUNDARY;
        m_mimeInfoQueue = new Vector();
        m_headerQueue = new Vector();
        m_QPLeftoverBuffer = new byte[2];
        m_qp = false;
        m_readCR = false;
        m_headerParent = m_mimeMessage;
        m_nextHeaderParent = null;
        m_previousHeaderName = null;
        m_messagePartSubType = SUBTYPE_RFC822;
        m_fEndMessageHeader = false;
    }



    /**
    * Parse user inputstream and return a populated data structure
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param input          User's inputstream
    * @param nMessageType <PRE>
    *               MIME_CONTENT_PARSE_ALL      if you want it to parse the entire message
    *               MIMEMessage.BASICPART       for basicPart
    *               MIMEMessage.MULTIPART       for multiPart
    *               MIMEMessage.MESSAGEPART     for messagePart</PRE>
    *
    * @return <PRE>MIMEMessage object if nMessageType = MIME_CONTENT_PARSE_ALL
    *MIMEBasicPart object if nMessageType = MIMEMessage.BASICPART;
    *MIMEMultiPart object if nMessageType = MIMEMessage.MULTIPART;
    *MIMEMessagePart object if nMessageType = MIMEMessage.MESSAGEPART; </PRE>
    * @exception MIMEException
    */
    protected Object parseMimeMessage( InputStream input, int nMessageType ) throws MIMEException
    {
        byte ch = 0;
    	String s;
    	int type, lineLen, messageLen;
    	Object returnObject = null;
    	int i = 0, j = 0, ii;
    	boolean lastLine = false;
    	byte[] swap;
    	boolean bMoreData = true;

    	if ( input == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

    	type = MIME_HEADER;

        try
        {
            messageLen = input.available();

        	for ( i = 0, j = m_leftoverBytes; bMoreData; )
        	{
                lineLen = input.read( m_inputBuffer );

                if ( lineLen < m_inputBuffer.length )
                    bMoreData = false;

                if ( lineLen < 0 )
                    break;

                i = i + lineLen;

                for ( ii = 0; ii < lineLen; ii++ )
            	{
            	    ch = m_inputBuffer[ii];

            	    if ( ch == -1 )
            	        break;

                    // eat it
                    if ( !m_readCR && ch == '\r' )
                        continue;

                    else if ( ch == '\n' )
                    {
                        // look for continuation
                        if ( MIMEHelper.bStringEquals( m_leftoverBuffer, "content-" ) )
                        {
                            int jj = j > 0 ? j - 1 : 0;
                            char ch2;

                            for ( ch2 = (char) m_leftoverBuffer[jj]; jj > 0 && Character.isWhitespace( ch2 ); ch2 = (char) m_leftoverBuffer[--jj] )
                                ;

                            // implement look ahead logic
                            if ( ( m_inputBuffer[ii+1] == ' ' || m_inputBuffer[ii+1] == '\t' ) &&
                                !MIMEHelper.bStringEquals( m_leftoverBuffer, "content-transfer-encoding" ) )
                            {
//                                System.out.println("continue = [" + new String( m_leftoverBuffer, 0, j) + "]" );
                                m_leftoverBuffer[j++] = ch;
                                continue;
                            }
                        }

                        if ( m_readCR )
                            m_leftoverBuffer[j++] = ch;

        				type = parseLine( m_leftoverBuffer, j, type, lastLine );
                        j = 0;

                        continue;

                    }

                    m_leftoverBuffer[j++] = ch;
                }
            }
        }
        catch ( IOException e )
        {
            // rethrow to a common exception handler
            throw new MIMEException( e.getMessage() );
        }

        // handle lastline
        if ( m_dataSink == null && j != 0 )
        {
        	type = parseLine( m_leftoverBuffer, j, type, lastLine );
        }

        m_leftoverBytes = j;

        if ( m_mimeMessage != null && m_dataSink == null )
            checkForEmptyMessages( m_mimeMessage );

        if ( m_mimeMessage != null && m_dataSink == null )
        {
            decodeDataBuffer();

            if ( nMessageType == MIME_CONTENT_PARSE_ALL )
                returnObject = m_mimeMessage;

            else if ( nMessageType == MIMEMessage.BASICPART ||
                    nMessageType == MIMEMessage.MULTIPART ||
                    nMessageType == MIMEMessage.MESSAGEPART )
                returnObject = m_mimeMessage.getBody();
        }

        // send data out right now using callbacks
        else if ( m_dataSink != null && m_bStartData )
        {
            decodeDataBuffer();
        }

        return returnObject;
    }




    /**
    * Save decoded data
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  byteBuffer data buffer
    * @param len number of bytes in the data buffer to save
    * @param mimeBasicPart save data to this mimeBasicPart
    * @return none
    * @exception MIMEException
    */
    private void saveBodyData( ByteBuffer byteBuffer, int len, MIMEBasicPart mimeBasicPart ) throws MIMEException
    {
        if ( byteBuffer == null || mimeBasicPart == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

        if ( m_bLocalStorage )
        {
            if ( m_bParseEntireFile )
            {
    			mimeBasicPart.setBodyData( byteBuffer );
       			mimeBasicPart.setMessageDataLen( len );
    	    }
    	    else
    	    {
    	        ByteBuffer b = mimeBasicPart.getDataBuf();

    	        if ( b == null )
    	        {
    	            mimeBasicPart.setBodyData( byteBuffer );
           			mimeBasicPart.setMessageDataLen( len );
    	        }
    	        else
    	        {
    	            b.append( byteBuffer );
           			mimeBasicPart.setMessageDataLen( mimeBasicPart.getMessageDataLen() + len );
    	        }
    	    }
    	}

        if (m_dataSink != null)
	{
            if (len>0)
            	m_dataSink.bodyData (mimeBasicPart.getUserObject(), byteBuffer.getInputStream(), len);

            if (mimeBasicPart.m_endData == true)
            	m_dataSink.endBasicPart (mimeBasicPart.getUserObject());

           if (mimeBasicPart.m_parentContainer != null)
           {
                  MIMEMultiPart mp = mimeBasicPart.m_parentContainer;
		  if (mp.m_endPart == true)
                      m_dataSink.endMultiPart (mp.getUserObject());
		  else; // ignore taken care of at end_boundary
           }

	}
        else
    		mimeBasicPart.setDecodedData( true );
    }



    /**
    * Decode message Data
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  none
    * @return none
    * @exception MIMEException
    */
    private void decodeDataBuffer() throws MIMEException
    {
    	int i;
    	int type2;
    	int len = 0;
    	int nIndex;
        boolean finished = false;
    	mimeInfo mi;
    	MIMEMessagePart mimeMessagePart;
    	MIMEBasicPart mimeBasicPart;
    	MIMEMultiPart mimeMultiPart;
        ByteBuffer newBuffer = null;
        byte decodedBuffer[] = null;
        byte line[];
        int param[];
        boolean bDecoded = false;
        int nStart;
        int nEnd;

        // no decoding
        if (m_nCurrentMessageType != MIMEMessage.BASICPART || m_currentMessage == null)
		return;

   	mimeBasicPart = (MIMEBasicPart) m_currentMessage;
        nStart = mimeBasicPart.getStartMessageDataIndex();
        nEnd = mimeBasicPart.getEndMessageDataIndex();

	// already being here
	if (mimeBasicPart.getDecodedData())
		return;

	/* base64 */
	if ( mimeBasicPart.getContentEncoding() == MIMEBodyPart.BASE64 && m_bDecodeData )
	{
		param = new int[3];
		param[0] = m_out_byte;
		param[1] = m_out_bits;

		decodedBuffer = MIMEHelper.decodeBase64Vector( m_messageData,
                                        			nStart,
                                        			nEnd,
                                        			mimeBasicPart.getMessageDataLen(),
                                        			param );

		if ( decodedBuffer != null )
		{
    			m_out_byte = param[0];
    			m_out_bits = param[1];
    			len = param[2];
    			param = null;
                	saveBodyData( new ByteBuffer( decodedBuffer, len ), len, mimeBasicPart );
            	}

            	bDecoded = true;
        }
	/* quoted printable */
	else if ( mimeBasicPart.getContentEncoding() == MIMEBodyPart.QP && m_bDecodeData )
	{
		param = new int[2];
		param[0] = m_QPLeftoverBytes;

		decodedBuffer = MIMEHelper.decodeQPVector( m_messageData,
                                    			   nStart,
                                    			   nEnd,
                                    			   param,
                                    			   m_QPLeftoverBuffer );
            	if ( decodedBuffer != null )
            	{
                    m_QPLeftoverBytes = param[0];
                    len = param[1];
    		    param = null;
                    saveBodyData( new ByteBuffer( decodedBuffer ), len, mimeBasicPart );
                }

                bDecoded = true;
	}
	/* base64 without decodeing */
	else if ( mimeBasicPart.getContentEncoding() == MIMEBodyPart.BASE64 )
	{
            if ( nEnd >= nStart && nStart != MIMEBasicPart.UNINITIALIZED )
            {
                // get message size
        	for (i = nStart, len = 0; i <= nEnd; i++)
        	{
        		line = (byte[]) m_messageData.elementAt( i );
        		//len = len + line.length;
        		len = len + line.length + 2; // for CRLF 
        	}

        	newBuffer = new ByteBuffer( len );

        	if ( newBuffer != null )
            	{
            		for (	i = nStart; i <= nEnd; i++ )
            		{
            		    	line = (byte[]) m_messageData.elementAt( i );
            			newBuffer.append( line );
            			newBuffer.append( CRLF );
            		}
                }

                saveBodyData( newBuffer, len, mimeBasicPart );
                bDecoded = true;
            }
	}
	/* plain buffer creation */
	else
	{
	    boolean  fQP = false;

	    if (mimeBasicPart.getContentEncoding() == MIMEBodyPart.QP)
		fQP = true;

            if ( nEnd >= nStart && nStart != MIMEBasicPart.UNINITIALIZED )
            {
                // get message size
        	for (	i = nStart, len = 0; i <= nEnd; i++ )
        	{
        		    line = (byte[]) m_messageData.elementAt( i );
        			//len = len + line.length + 1;
        			len = len + line.length; 
			    	if (!fQP)		 
        			    len = len + 2;	 
        	}

        	newBuffer = new ByteBuffer( len );

        	byte b = '\n';
        	byte r = '\r';

        	if ( newBuffer != null )
            	{
            		for (	i = nStart; i <= nEnd; i++ )
            		{
            		    line = (byte[]) m_messageData.elementAt( i );
            	  	    newBuffer.append( line );
			    if (!fQP)
			    {
            			newBuffer.append(r);
            			newBuffer.append(b);
			    }
            		}
                }

                saveBodyData( newBuffer, len, mimeBasicPart );
                bDecoded = true;
            }
	}

        if ( bDecoded )
        {
   		if ( m_dataSink == null )
       			mimeBasicPart.setDecodedData( true );
       		else
       		{
       			// reset
                	mimeBasicPart.setStartMessageDataIndex( MIMEBasicPart.UNINITIALIZED );
                	mimeBasicPart.setEndMessageDataIndex( MIMEBasicPart.UNINITIALIZED );
            	}
        }
    }





    /**
    * Parse one line of data
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  s input line
    * @param len length of line
    * @param type line type for the previous line<PRE>
    *   MIME_HEADER     header line
    *   MIME_INFO       mime info line</PRE>
    *
    * @return line type for current line
    * @exception MIMEException
    */
    private int parseLine( byte s[], int len, int type, boolean lastLine ) throws MIMEException
    {
    	int i;
    	int type2;
    	int nIndex;
        boolean finished = false;
    	mimeInfo mi;

    	MIMEMessagePart mimeMessagePart;
    	MIMEBasicPart mimeBasicPart;
    	MIMEMultiPart mimeMultiPart;
    	MIMEMessage mimeMessage;
        MIMEBodyPart m = (MIMEBodyPart) m_currentMessage;

    	if ( s == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );



// debug
/*
//if ( MIMEHelper.bStringEquals( s, 7, "Subject" ) )
//if ( MIMEHelper.bStringEquals (s, 13, "--mail.sleepy"))
String ss = new String(s,0,len);
System.out.println (ss);
boolean fRightHdr = false;
//if (MIMEHelper.bStringEquals (s, "X-Original-In-reply-to"))
if (MIMEHelper.bStringEquals (s, "Content-Type: message/rfc822"))
{
   ss = new String(s,0,len);
   fRightHdr = true;
}
else if (MIMEHelper.bStringEquals (s, "Content-type: message/external-body"))
{
   ss = new String(s,0,len);
   fRightHdr = false;
}
*/

    	/* ignore additional header info */
        if ( ( type == MIME_INFO || type == MIME_HEADER || type == MIME_XHEADER )&& ( len > 0 &&( s[0] == '\t' || s[0] == ' ' ) ) )
        {
	   if (!(m_nCurrentMessageType == MIMEMessage.BASICPART && m_bStartData == true)) 
           {                                                                            
            	Header h[];

		    // continuation headers
		    if ( m_headerParent instanceof MIMEBasicPart )
		    {
		        mimeBasicPart = (MIMEBasicPart) m_currentMessage;
                mimeBasicPart.addHeader( m_previousHeaderName, new String ("\r\n" +  new String( s, 0, len ) ));
            	    }
		    else if ( m_headerParent instanceof MIMEMessagePart )
		    {  // Ignore!!
		       // mimeMessagePart = (MIMEMessagePart) m_currentMessage;
               // h = mimeMessagePart.getAllHeaders();
               // mimeMessagePart.addHeader( m_previousHeaderName, new String( s, 0, len ) );

                //m_currentMimeMessage.addHeader( m_previousHeaderName, new String( s, 0, len ) );
                m_currentMimeMessage.addHeader( m_previousHeaderName, new String ("\r\n" + new String( s, 0, len ) ));
		    }
		    else
		    {
		        if (m_previousHeaderName == null) 
		        {
		              appToLastHdrOnQueue (MIMEHelper.byteSubstring( s, 0, len)); 
		        }
		        else
			{
                		m_currentMimeMessage.addHeader( m_previousHeaderName, new String ("\r\n" + new String( s, 0, len ) ));
    	     			if (m_dataSink != null)
	     			{
    		                   m_dataSink.addHeader (m_currentMimeMessage.getUserObject(),
							 m_previousHeaderName.getBytes(),
							 MIMEHelper.byteSubstring(s, 0, len));
	     			}
			}

    		}
/*
		    if ( h != null && h[ h.length - 1 ] != null )
		    {
    		    h[ h.length - 1 ].setValue(  h[ h.length - 1 ].getValue() + "\n" + new String( s,0,len ) );
    		}
*/
    		return type;
	   }
        }
	else if (( type == MIME_INFO || type == MIME_HEADER || type == MIME_XHEADER ) && m_dataSink != null)
	{
		// a new header with same name
		m_previousHeaderName = null;
	}


        if ( len == 0 )
	{
            m_emptyLineNo++;

	    /* end of header callback (only on top level MimeMessage) */
	    if (m_emptyLineNo == 1 && m_fEndMessageHeader == false)
	    {
	             m_fEndMessageHeader = true;
     	         if (m_dataSink != null)
                    m_dataSink.endMessageHeader (m_mimeMessage.getUserObject());
	    }
	}

        if ( len == 0 && !m_bStartData )
        {
    	    int nCurrentParentType = getCurrentParentType();   

            if ( m_qp && m_emptyLineNo == 1 )
                m_readCR = true;

            //if ( m_emptyLineNo == 2 )
	    if (m_emptyLineNo >= 2 && !(m_nCurrentMessageType == MIMEMessage.MULTIPART && m_fSeenBoundary==false))
	    {
                     m_bStartData = true;
	    }
	    else if (m_emptyLineNo == 1 && nCurrentParentType == MIMEMessage.MULTIPART && m_fSeenBoundary == true) 
	    {
                if (m_nCurrentMessageType == MIME_UNINITIALIZED)
                     m_bStartData = true; 		
	    }
	    /*else if (m_emptyLineNo == 1 && m_nCurrentMessageType == MIMEMessage.MULTIPART) This broke mime11 */
	    else if (m_emptyLineNo > 1 && m_nCurrentMessageType == MIMEMessage.MULTIPART)  
	    {
		//System.out.println ("m_fSeenBoundary=" + m_fSeenBoundary);

		     if (m_fSeenBoundary == true)            
                     m_bStartData = true;                    
             else
             {                                               
                        mimeMultiPart = (MIMEMultiPart) m_currentMessage;       
                        s[0] = '\r';                                            
                        s[1] = '\n';                                            
                        mimeMultiPart.addPreamble (s, 2);                       
                        return MIME_CRLF;
             }

	    } 						               

            else if ( m_emptyLineNo == 1 )
            {
                // change mimeMessage parent
                if ( m_nextMimeMessage != null )
                {
                    // change parent only after first blank line
                    m_currentMimeMessage = m_nextMimeMessage;
                    m_nextMimeMessage = null;
                }

                // insert headers from queue
                if ( m_nCurrentMessageType != MIME_UNINITIALIZED && m_headerQueue.size() > 0 )
                {
            		if ( m_headerQueue.size() > 0 )
            		{
            		    for ( i = 0; i < m_headerQueue.size(); i++ )
            		    {
            		        mi = (mimeInfo) m_headerQueue.elementAt(i);
            		        addHeader( mi.m_name, mi.m_value, false );
            		    }

            		    m_headerQueue.removeAllElements();
            		}
                }

                // change header parent
                if ( m_nextHeaderParent != null && !( m_nextHeaderParent instanceof MIMEMultiPart) )
                    m_headerParent = m_nextHeaderParent;
            }
        }

        if ( MIMEHelper.bStringEquals( s, len, "content-type" ) )
        {
            if (m_emptyLineNo > 0 && m_nCurrentMessageType != MIMEMessage.MESSAGEPART) 
                m_emptyLineNo = 0;                                                      
        }

        /* boundary */
        if ( ( type == MIME_INFO || type == MIME_HEADER || type == MIME_XHEADER ) &&
          len == 0 && m_nCurrentMessageType != MIME_UNINITIALIZED && m_messagePartSubType != SUBTYPE_EXTERNAL_BODY )
        {
    		if ( m_nCurrentMessageType == MIMEMessage.MESSAGEPART || m_nCurrentMessageType == MIMEMessage.MULTIPART )
    		{
    		    if ( m_emptyLineNo == 1 || m_emptyLineNo == 2 )
        			type = MIME_HEADER;
                else
                {
                    mi = new mimeInfo();

                    // content type
                    mi.m_type = MIME_INFO;
                    mi.m_name = MIMEHelper.stringToByte( "content-type" );
                    mi.m_value = MIMEHelper.stringToByte( "text/plain" );
    		    setData( mi );
                    parseMimeInfo( mi );
                    m_mimeInfo.addElement( mi );

                    // content type
                    //mi.m_type = MIME_INFO;
                    //mi.m_name = MIMEHelper.stringToByte( "content-transfer-encoding" );
                    //mi.m_value = MIMEHelper.stringToByte( "7bit" );
    		    //setData( mi );
                    //parseMimeInfo( mi );
                    //m_mimeInfo.addElement( mi );

       			m_bStartData = true;
			 m_fSeenBoundary = false;
        	    }
    	    }

    		else if ( m_nCurrentMessageType == MIMEMessage.BASICPART )
    		{
    			m_bStartData = true;
			 m_fSeenBoundary = false;
	    	}

            if ( m_emptyLineNo == 1 )
                return type;
        }

        if ( !m_bStartData && len == 0 )
	{
	    // Add to preamble if multi-part
            if (m_nCurrentMessageType == MIMEMessage.MULTIPART)                 
            {
                        m_emptyLineNo = 0;                                      
                        mimeMultiPart = (MIMEMultiPart) m_currentMessage;       

                        s[0] = '\r';                                            
                        s[1] = '\n';                                            
                        mimeMultiPart.addPreamble (s, 2);                       
           }

            return MIME_CRLF;
	}

        /* -------------------------- identify line ----------------------- */
        type2 = checkForLineType( s, len, m_bStartData );

	if (type2 == START_BOUNDARY)
	{
		m_fSeenBoundary = true;
		m_emptyLineNo = 0; 
                if (m_nCurrentMessageType == MIMEMessage.MESSAGEPART && 
                        m_messagePartSubType == SUBTYPE_EXTERNAL_BODY)  
                        m_bStartData = false;  

		// give the boundary 
                if (m_nCurrentMessageType == MIMEMessage.MULTIPART  && m_dataSink != null)
		{
			MIMEMultiPart mp = (MIMEMultiPart) m_currentMessage;
	    		    m_dataSink.boundary (mp.getUserObject(), (mp.getBoundary()).getBytes());
		}
		else if (m_dataSink != null)
		{
			int nCurrentParentType = getCurrentParentType();

		        if (nCurrentParentType == MIMEMessage.MULTIPART)
		        {
			    MIMEMultiPart mp = (MIMEMultiPart) m_currentParent.lastElement();;
	    		    m_dataSink.boundary (mp.getUserObject(), (mp.getBoundary()).getBytes());
			}
		}
	}

        // start of simple basicpart message
        if ( type == MIME_CRLF && m_nCurrentMessageType == MIME_UNINITIALIZED && m_lastBoundry == START_BOUNDARY &&
            type2 == MIME_MESSAGE_DATA && ( m_emptyLineNo == 1 || m_emptyLineNo == 2 ) &&
            m_messagePartSubType != SUBTYPE_EXTERNAL_BODY )
        {
            mi = new mimeInfo();

            // content type
            mi.m_type = MIME_INFO;
            mi.m_name = MIMEHelper.stringToByte( "content-type" );
            mi.m_value = MIMEHelper.stringToByte( "text/plain" );
	        setData( mi );
            parseMimeInfo( mi );
            m_mimeInfo.addElement( mi );
	        m_bStartData = true;
	 	m_fSeenBoundary = false;

            if ( m_emptyLineNo == 1 || m_emptyLineNo == 2 )
            {
                // insert headers from queue
                if ( m_headerQueue.size() > 0 )
//                if ( m_nCurrentMessageType != MIME_UNINITIALIZED && m_headerQueue.size() > 0 )
                {
                    if ( m_nextHeaderParent != null )
                        m_headerParent = m_nextHeaderParent;

        		    for ( i = 0; i < m_headerQueue.size(); i++ )
        		    {
        		        mi = (mimeInfo) m_headerQueue.elementAt(i);
        		        addHeader( mi.m_name, mi.m_value, false );
        		    }

        		    m_headerQueue.removeAllElements();
                }

                // change header parent
                if ( m_nextHeaderParent != null )
                    m_headerParent = m_nextHeaderParent;

                m_emptyLineNo = 0;      // reset for headers
            }
        }

        /* start boundary  */
        if ( !m_bStartData && type2 == START_BOUNDARY )
        {
            unwindCurrentParent( s, false );

            m_out_byte = 0;
            m_out_bits = 0;
            m_leftoverBytes = 0;
            m_QPLeftoverBytes = 0;
            m_emptyLineNo = 0;
            m_nCurrentMessageType = MIME_UNINITIALIZED;
            m_lastBoundry = START_BOUNDARY;
            m_fSeenBoundary = true;
            m_qp = false;
            m_readCR = false;
            m_messagePartSubType = SUBTYPE_RFC822;

            return MIME_INFO;
        }

        /* end boundary */
        else if ( type2 == END_BOUNDARY || type2 == START_BOUNDARY )
        {

            if ( m_dataSink != null )
            {
    		    switch( m_nCurrentMessageType )
    		    {
    		        case MIMEMessage.BASICPART:
				//m_dataSink.endBasicPart( m.getUserObject() ); break;
				((MIMEBasicPart)m).m_endData = true; break;
    		        case MIMEMessage.MULTIPART:
        			if (type2 == END_BOUNDARY)			
				   m_dataSink.endMultiPart (m.getUserObject());
				break;
    		        case MIMEMessage.MESSAGEPART:
				m_dataSink.endMessagePart (m.getUserObject());
				m_msgParts.removeElement (m); 
				break;
    		    }

		    
		    if (type2 == END_BOUNDARY)
		    {
			int nCurrentParentType = getCurrentParentType();

		        if (nCurrentParentType == MIMEMessage.MULTIPART)
		        {
			    MIMEMultiPart mp = (MIMEMultiPart) m_currentParent.lastElement();;
			    if (m_nCurrentMessageType != MIMEMessage.BASICPART)
	    		    	m_dataSink.endMultiPart (mp.getUserObject());
			    else
			    {
				mp.m_endPart = true;
				((MIMEBasicPart)m).m_parentContainer = mp;
			    }
			}
			//else if (m_nCurrentMessageType == MIMEMessage.MULTIPART)
			//{
			//   MIMEMultiPart mp = (MIMEMultiPart) m_currentMessage;
	    		//   m_dataSink.endMultiPart (mp.getUserObject());
			//   System.out.println ("at END_BOUNDARY() nCurrentParentType= " + nCurrentParentType);
			//   System.out.println ("at END_BOUNDARY() m_nCurrentMessageType= " + m_nCurrentMessageType);
			//}
		    }
            }

            if ( type2 == END_BOUNDARY )
            {
                unwindCurrentParent( s, true );
                m_lastBoundry = END_BOUNDARY;
            	m_fSeenBoundary = false;
            }
            else
                 m_lastBoundry = START_BOUNDARY;

            m_emptyLineNo = 0;
            m_bStartData = false;
            decodeDataBuffer();
            m_nCurrentMessageType = MIME_UNINITIALIZED;
            m_qp = false;
            m_readCR = false;
            m_messagePartSubType = SUBTYPE_RFC822;

            return type2;
        }

        /* message data	   */
        else if ( m_bStartData )
        {
            // clone
            byte sss[] = new byte[len];
            System.arraycopy( s, 0, sss, 0, len );
            m_messageData.addElement( sss );
            nIndex = m_messageData.size() - 1;

		if (m_nCurrentMessageType == MIME_UNINITIALIZED)        
            	{                                                       
    		     int nCurrentParentType = getCurrentParentType();   

		     //System.out.println ("m_nCurrentMessageType== UNINIT");
		     //System.out.println ("nCurrentParentType= " + nCurrentParentType);

		     if (nCurrentParentType == MIMEMessage.MULTIPART)   
		     {
                     	mi = new mimeInfo();                           	

                    	// content type
                    	mi.m_type = MIME_INFO;                           	
                    	mi.m_name = MIMEHelper.stringToByte("content-type");    
                    	mi.m_value = MIMEHelper.stringToByte("text/plain");     
                    	setData( mi );                                  	
                    	parseMimeInfo( mi );                            	
                    	m_mimeInfo.addElement( mi );                    	
		     }
            	}                                                       

    		if ( m_nCurrentMessageType == MIMEMessage.BASICPART && m_currentMessage != null )
    		{
    			mimeBasicPart = (MIMEBasicPart) m_currentMessage;

    			if ( mimeBasicPart.getStartMessageDataIndex() == MIMEBasicPart.UNINITIALIZED )
    			    mimeBasicPart.setStartMessageDataIndex( nIndex );

                mimeBasicPart.setEndMessageDataIndex( nIndex );

                if ( lastLine )
                {
                    decodeDataBuffer();

                    if ( m_dataSink != null )
                    {
           				switch( m_nCurrentMessageType )
            		    {
            		        case MIMEMessage.BASICPART:
					m_dataSink.endBasicPart( m.getUserObject() ); break;
            		        case MIMEMessage.MULTIPART: m_dataSink.endMultiPart( m.getUserObject() ); break;
            		        case MIMEMessage.MESSAGEPART:
					m_dataSink.endMessagePart( m.getUserObject() );
					m_msgParts.removeElement (m); 
					break;
            		    }
                    }
                }
    		}

            return MIME_MESSAGE_DATA;
        }

        // add preamble 
        if (m_emptyLineNo> 0 && m_nCurrentMessageType == MIMEMessage.MULTIPART && ! m_fSeenBoundary) 
        {
   		    mimeMultiPart = (MIMEMultiPart) m_currentMessage; 
   		    s[len] = '\n';                                    
            mimeMultiPart.addPreamble(s, len + 1);            
            return type2;                                     
        }


        mi = new mimeInfo();
        mi.m_type = type2;

        boolean startQuote = false;

        for ( i = 0; i < len && !finished; i++ )
        {
            if ( s[i] == '"' )
                startQuote = startQuote ? false : true;

            if ( s[i] == ':' && !startQuote )
            {
                mi.m_name = MIMEHelper.byteSubstring( s, 0, i );

				/* remove junk spaces */
				for ( ++i; i < len && s[i] != 0 && Character.isWhitespace( (char) s[i] ); i++ )
					;

                mi.m_value = MIMEHelper.byteSubstring( s, i, len - i );

                if ( mi.m_type == MIME_INFO )
				{
					setData( mi );

					if ( m_messagePartSubType != SUBTYPE_EXTERNAL_BODY )
					{
                        parseMimeInfo( mi );
                        m_mimeInfo.addElement( mi );
                    }
				}

                else if ( ( mi.m_type == MIME_XHEADER || mi.m_type == MIME_HEADER ) && m_headerParent != null )
                {
                    addHeader( mi.m_name, mi.m_value, true );
                    m_emptyLineNo = 0;
                }

                finished = true;
            }
        }

        // add preamble
        if ( !finished && m_nCurrentMessageType == MIMEMessage.MULTIPART )
        {
		m_emptyLineNo = 0; 
   		mimeMultiPart = (MIMEMultiPart) m_currentMessage;
   		s[len] = '\n';
            	mimeMultiPart.addPreamble( s, len + 1 );
        }

    	mi = null;

        if ( lastLine )
        {
            decodeDataBuffer();

            if ( m_dataSink != null )
            {
    		    switch( m_nCurrentMessageType )
    		    {
    		        case MIMEMessage.BASICPART:
				m_dataSink.endBasicPart( m.getUserObject() ); break;
    		        case MIMEMessage.MULTIPART: m_dataSink.endMultiPart( m.getUserObject() ); break;
    		        case MIMEMessage.MESSAGEPART:
					m_dataSink.endMessagePart( m.getUserObject() );
					m_msgParts.removeElement (m); 
					break;
    		    }
            }
        }

        return type2;
    }


    private void appToLastHdrOnQueue (byte value[]) 
    {
         mimeInfo mi;

         int i = m_headerQueue.size();

         if (i > 0)
         {
              mi = (mimeInfo) m_headerQueue.elementAt(i-1);
              m_headerQueue.removeElementAt (i-1);
              String val = new String (mi.m_value);
              val = val.concat ("\r\n");
              String s2 = new String (value);
              val = val.concat (s2);
              //val.concat (new String (value));
              mi.m_value = val.getBytes();
              m_headerQueue.addElement( mi );
         }
    }

//add cont * headers to the bodypart. used by MessagePart only at this point.
private void addContHeader (MIMEBodyPart bp, mimeInfo mi)
{
      try
      {
        if (MIMEHelper.bStringEquals (mi.m_name, "content-description"))
        {
                bp.setContentDescription (new String (mi.m_value));
        }
        else if (MIMEHelper.bStringEquals (mi.m_name, "content-disposition"))
        {
                int disp = translateDispType (mi.m_value);
                bp.setContentDisposition (disp);
        }
        else if (MIMEHelper.bStringEquals (mi.m_name, "content-id"))
        {
                bp.setContentID (new String (mi.m_value));
        }
        else if (MIMEHelper.bStringEquals (mi.m_name, "content-md5"))
        {
                if (bp instanceof MIMEBasicPart)
                ((MIMEBasicPart)bp).setContentMD5 (new String (mi.m_value));
        }
        else if (MIMEHelper.bStringEquals (mi.m_name, "content-transfer-encoding"))
        {
                //int encoding = mime_translateMimeEncodingType (mi.m_value);
                //bp.setContentEncoding (encoding);
        }
      }
      catch (Exception e)
      {
            // ignore
      }
}


    /**
    * add header
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  mi.name header name
    * @param  mi.value header value
    * @return none
    * @exception MIMEException
    */
    private void addHeader( byte name[], byte value[], boolean addToQueue ) throws MIMEException
    {
    	MIMEMessagePart mimeMessagePart;
    	MIMEBasicPart mimeBasicPart;
    	MIMEMessage mimeMessage;
    	mimeInfo mi;

    	if ( name == null || value == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

        if (m_nCurrentMessageType == MIME_UNINITIALIZED && m_headerParent == m_mimeMessage)
	{
    	     m_mimeMessage.addRHeader( new String( name), new String( value ) );

    	     if (m_dataSink != null)
	     {
		    if (m_previousHeaderName != null && m_previousHeaderName.equals (new String (name)))
    		         m_dataSink.addHeader (m_mimeMessage.getUserObject(),  name,  value );
		    else
		    {
    		    	 m_dataSink.header (m_mimeMessage.getUserObject(),  name,  value );
		    }
	     }

    	     m_previousHeaderName = new String( name );

	     return;
	}

        // insert headers from queue, restore header order
        if ( m_nCurrentMessageType != MIME_UNINITIALIZED && addToQueue && m_headerQueue.size() > 0 )
   		{
		    for ( int i = 0; i < m_headerQueue.size(); i++ )
		    {
		        mi = (mimeInfo) m_headerQueue.elementAt(i);
		        addHeader( mi.m_name, mi.m_value, false );
		    }

		    m_headerQueue.removeAllElements();
        }

        // add to header queue
    	if ( m_nCurrentMessageType == MIME_UNINITIALIZED && addToQueue )
        {
            mi = new mimeInfo();

            if ( mi != null )
            {
                mi.m_name = name;
                mi.m_value = value;
                m_headerQueue.addElement( mi );
            }
        }

        else if ( m_headerParent instanceof MIMEMessage )
        {
    		mimeMessage = (MIMEMessage) m_headerParent;
    		mimeMessage.addRHeader( new String( name ), new String( value ) );

    		if (m_dataSink != null)				                 
		{						                 

		    if (m_previousHeaderName != null && m_previousHeaderName.equals (new String(name))) 
    		       	  m_dataSink.addHeader (mimeMessage.getUserObject(), name, value); 		
	    	    else										
			  m_dataSink.header (mimeMessage.getUserObject(), name, value);  	    	


		}								  

  		m_previousHeaderName = new String( name );
        }

        else if ( m_emptyLineNo > 0 || (m_headerParent instanceof MIMEBasicPart && m_fSeenBoundary))
        {
            if ( m_headerParent instanceof MIMEBasicPart )
            {
    			mimeBasicPart = (MIMEBasicPart) m_headerParent;
    			mimeBasicPart.setHeader( new String( name ), new String( value ) );
    			m_previousHeaderName = new String( name );

    			if ( m_dataSink != null )
			{
    			    m_dataSink.header( mimeBasicPart.getUserObject(),  name,  value );
			}
    	    }
            else if ( m_headerParent instanceof MIMEMessagePart || m_headerParent instanceof MIMEMultiPart )
            {
    			mimeMessage = (MIMEMessage) m_currentMimeMessage;
    			mimeMessage.addRHeader( new String( name), new String( value ) );

    			if ( m_dataSink != null )
			{
		    	    if (m_previousHeaderName != null && m_previousHeaderName.equals (new String (name)))	
    		         	 m_dataSink.addHeader (mimeMessage.getUserObject(), name, value); 
			    else							
			    {
    			         m_dataSink.header(mimeMessage.getUserObject(), name, value);
			    }
			}

    			m_previousHeaderName = new String( name );
           }
        }
    }



    /**
    * fill in data fields from pMimeinfo
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  mi mimeInfo structure
    * @return none
    * @exception MIMEException
    */
    private void setData( mimeInfo mi ) throws MIMEException
    {
    	MIMEMessagePart mimeMessagePart;
    	MIMEBasicPart mimeBasicPart;
    	byte[] param[];
    	int contentDisposition;
        MIMEBodyPart m = (MIMEBodyPart) m_currentMessage;

    	if ( mi == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

        param = new byte[1][];

	  if ( m_messagePartSubType == SUBTYPE_EXTERNAL_BODY && m_nCurrentMessageType == MIMEMessage.MESSAGEPART )
	  {
	        // add as external body headers
	       mimeMessagePart = (MIMEMessagePart) m_currentMessage;    	

	     if ( m_emptyLineNo >= 1 )    					
	     {   								

	         mimeMessagePart.setExtBodyHeader (new String( mi.m_name),    	
	  					 new String( mi.m_value ) );    
	         m_previousHeaderName = new String( mi.m_name );    		
	     }    								
	     else    								
	     {
	         addContHeader (mimeMessagePart, mi);   			
	         //addHeader( mi.m_name, mi.m_value, true );
	     }    								
	  }

        //if ( m_messagePartSubType == SUBTYPE_EXTERNAL_BODY && m_nCurrentMessageType == MIMEMessage.MESSAGEPART )
        //{
         //   // add as external body headers
	//		mimeMessagePart = (MIMEMessagePart) m_currentMessage;
	//		mimeMessagePart.setExtBodyHeader( new String( mi.m_name), new String( mi.m_value ) );
	//		m_previousHeaderName = new String( mi.m_name );
        //}

    	/* create message structure */
        else if ( MIMEHelper.bStringEquals( mi.m_name, "content-type" ) )
        {
    		newMessageStructure( mi.m_value );

    		// add from queue
    		if ( m_mimeInfoQueue.size() > 0 && m_messagePartSubType != SUBTYPE_EXTERNAL_BODY )
    		{
    		    for ( int i = 0; i < m_mimeInfoQueue.size(); i++ )
    		    {
    		        setData( (mimeInfo) m_mimeInfoQueue.elementAt(i) );
    		    }

    		    m_mimeInfoQueue.removeAllElements();
    		}
        }

        // no structure created yet
        else if ( m_currentMessage == null )
            return;

    	else if ( MIMEHelper.bStringEquals( mi.m_name, "content-description" ) )
        {
            if ( m_nCurrentMessageType == MIME_UNINITIALIZED )
                m_mimeInfoQueue.addElement( mi );
            else
            {
    			m.setContentDescription( new String( mi.m_value ) );

    			if ( m_dataSink != null )
    			    m_dataSink.contentDescription( m.getUserObject(), mi.m_value );
			}
        }

    	else if ( MIMEHelper.bStringEquals( mi.m_name, "content-disposition" ) )
        {
            if ( m_nCurrentMessageType == MIME_UNINITIALIZED )
                m_mimeInfoQueue.addElement( mi );
            else
            {
                contentDisposition = translateDispType( mi.m_value, param );

                if ( contentDisposition != NSMAIL_ERR_INVALIDPARAM )
                {
    				m.setContentDisposition( contentDisposition );

    				if ( m_dataSink != null )
    				    m_dataSink.contentDisposition( m.getUserObject(), contentDisposition );

    				if ( param[0] != null )
    				{
        				m.setContentDispParams( new String( param[0] ) );

        				if ( m_dataSink != null )
    				        m_dataSink.contentDispParams( m.getUserObject(), param[0] );
    				}
    			}
    		}
        }

    	else if ( MIMEHelper.bStringEquals( mi.m_name, "content-id" ) )
        {
            if ( m_nCurrentMessageType == MIME_UNINITIALIZED )
                m_mimeInfoQueue.addElement( mi );
            else
            {
    			m.setContentID( new String( mi.m_value ) );

    			if ( m_dataSink != null )
    			    m_dataSink.contentID( m.getUserObject(), mi.m_value );
    	    }
        }

    	else if ( MIMEHelper.bStringEquals( mi.m_name, "content-transfer-encoding" ) )
		{
            if ( m_nCurrentMessageType == MIME_UNINITIALIZED )
                m_mimeInfoQueue.addElement( mi );
            else
            {
                if ( m_currentMessage instanceof MIMEBasicPart )
                {
        			mimeBasicPart = (MIMEBasicPart) m_currentMessage;
        			int encoding;

        		    encoding = mime_translateMimeEncodingType( mi.m_value );
        			mimeBasicPart.setContentEncoding( encoding );

        			if ( encoding == MIMEBodyPart.QP )
        			    m_qp = true;

        			if ( m_dataSink != null )
        			    m_dataSink.contentEncoding( mimeBasicPart.getUserObject(), encoding );
                }

                else if ( m_currentMessage instanceof MIMEMessagePart )
                {
                    mimeMessagePart = (MIMEMessagePart) m_currentMessage;
        			int encoding;

        		    encoding = mime_translateMimeEncodingType( mi.m_value );
        			mimeMessagePart.setContentEncoding( encoding );
        			if ( m_dataSink != null )
        			    m_dataSink.contentEncoding( mimeMessagePart.getUserObject(), encoding );
                }
		else if ( m_currentMessage instanceof MIMEMultiPart )
                {
                        MIMEMultiPart mimeMultiPart = (MIMEMultiPart) m_currentMessage;
                        int encoding = mime_translateMimeEncodingType( mi.m_value );
                        mimeMultiPart.setContentEncoding( encoding );
                }
    	    }
        }

    	else if ( MIMEHelper.bStringEquals( mi.m_name, "content-md5" ) )
		{
            if ( m_nCurrentMessageType == MIME_UNINITIALIZED )
                m_mimeInfoQueue.addElement( mi );
            else
            {
    			mimeBasicPart = (MIMEBasicPart) m_currentMessage;
    			mimeBasicPart.setContentMD5( new String( mi.m_value ) );

    			if ( m_dataSink != null )
    			    m_dataSink.contentMD5( mimeBasicPart.getUserObject(), mi.m_value );
    		}
        }
    	else
    	{
        	if  (m_nCurrentMessageType == MIME_UNINITIALIZED)
              	m_mimeInfoQueue.addElement (mi);
    		else
    		{
    		    // add unknown tags as header
  		        addHeader( mi.m_name, mi.m_value, true );
/*
        			mimeBasicPart = (MIMEBasicPart) m_currentMessage;
        			mimeBasicPart.setHeader (new String(mi.m_name), new String (mi.m_value));

        			if ( m_dataSink != null )
        			    m_dataSink.header (mimeBasicPart.getUserObject(), mi.m_name, mi.m_value);
*/
    		}
    	}
    }



    /**
    * Parse a line into name, value, parameters and store it in a mimeInfo structure
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  mi mimeInfo structure to be parsed
    * @return none
    * @exception MIMEException
    */
    private void parseMimeInfo( mimeInfo mi ) throws MIMEException
    {
        byte[] paramName = null;
        byte[] value = null;
        int offset = 0;
        int len = 0;
        boolean beginQuote = false;
        byte ch;
    	int i;
    	mimeInfo mi2;
    	int nValueLen;

    	if ( mi == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

        nValueLen = mi.m_value.length;

    	/* go through entire line */
        for ( i = 0; i < nValueLen; i++ )
        {
            ch = mi.m_value[i];

            /* handle quotes */
            if ( ch == '"' )
                beginQuote = beginQuote ? false : true;

            if ( beginQuote )
            {
                len++;
                continue;
            }

    		/* parse and fill in fields */
            switch ( ch )
            {
                case ';':
                    value = MIMEHelper.byteSubstring( mi.m_value, offset, len );
                    // trim();
                    offset = i + 1;

    				mi2 = new mimeInfo( MIME_INFO, paramName, value );

    				if ( mi2 == null )
    				    throw new MIMEException( MIMEHelper.szERROR_OUT_OF_MEMORY );

                    mi.m_param.addElement( mi2 );

                    value = null;
                    paramName = null;
                    len = 0;
                    break;

                case '=':
                    paramName = MIMEHelper.byteSubstring( mi.m_value, offset, len );
                    // trim();
                    offset = i + 1;
                    len = 0;
                    break;

                default:
                    len++;
            }
        }

    	/* add it to internal buffer */
        if ( len > 0 )
        {
            value = MIMEHelper.byteSubstring( mi.m_value, offset, len );  //trim();
    		mi2 = new mimeInfo( MIME_INFO, paramName, value );

    		if ( mi2== null )
    		    throw new MIMEException( MIMEHelper.szERROR_OUT_OF_MEMORY );

            mi.m_param.addElement( mi2 );
        }
    }

private byte[] getBytecontType (int nMessageType, int basicPartcontType)
{
	switch ( nMessageType )
	{
		case MIMEMessage.BASICPART:
			switch (basicPartcontType)
			{
				case MIMEBasicPart.TEXT:
				     return mb_TextPart;
				case MIMEBasicPart.AUDIO:
				     return mb_AudioPart;
				case MIMEBasicPart.VIDEO:
				     return mb_VideoPart;
				case MIMEBasicPart.IMAGE:
				     return mb_ImagePart;
				case MIMEBasicPart.APPLICATION:
				     return mb_ApplPart;
			}
		case MIMEMessage.MULTIPART:
			return mb_MultiPart;
		case MIMEMessage.MESSAGEPART:
			return mb_MessagePart;
	}

	return null;
}

    /**
    * create new message structure based on content type
    * the input string will be parsed to determine the content type
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param s input line
    * @return none
    * @exception MIMEException
    */
    private void newMessageStructure( byte[] s ) throws MIMEException
    {
    	MIMEMessagePart mimeMessagePart;
    	MIMEBasicPart mimeBasicPart;
    	MIMEMultiPart mimeMultiPart;
    	int nMessageType;
    	byte[] param[] = new byte[2][];
    	int contentType[] = new int[1];

    	nMessageType = nGetMessageType( s, param, contentType );

		switch ( nMessageType )
		{
			case MIMEMessage.BASICPART:
			    mimeBasicPart = new MIMEBasicPart();

				if ( m_dataSink != null )
				{
                    			//if ( m_bLocalStorage )
					//    mimeBasicPart.setUserObject( m_dataSink.startBasicPart( mimeBasicPart ) );
					//else
					    mimeBasicPart.setUserObject( m_dataSink.startBasicPart() );
				}

				addMessage( mimeBasicPart, MIMEMessage.BASICPART );
				break;

			case MIMEMessage.MULTIPART:
			    mimeMultiPart = new MIMEMultiPart();
			    mimeMultiPart.m_parsedPart=1; 

				if ( m_dataSink != null )
				{
                    			//if ( m_bLocalStorage )
					//    mimeMultiPart.setUserObject( m_dataSink.startMultiPart( mimeMultiPart ) );
					//else
					    mimeMultiPart.setUserObject( m_dataSink.startMultiPart() );
				}

				addMessage( mimeMultiPart, MIMEMessage.MULTIPART );

    			/* save boundary */
    			mimeMultiPart = (MIMEMultiPart) m_currentMessage;
    			String boundary = parseForBoundary( new String(s) );
	    		mimeMultiPart.setBoundary( boundary );

	    		// debug
//	    		System.out.println( "boundary = [" + boundary + "]" );

//	    		if ( m_dataSink != null )
//	    		    m_dataSink.boundary( mimeMultiPart.getUserObject(), boundary.getBytes() );

				break;

			case MIMEMessage.MESSAGEPART:
			    mimeMessagePart = new MIMEMessagePart();

                if ( m_dataSink != null )
                {
                    //if ( m_bLocalStorage )
                    //    mimeMessagePart.setUserObject( m_dataSink.startMessagePart( mimeMessagePart ) );
                    //else
                        mimeMessagePart.setUserObject( m_dataSink.startMessagePart() );

			m_msgParts.addElement (mimeMessagePart); 

                }

                if ( MIMEHelper.bStringEquals( param[0], "external-body" ) )
                {
                    m_messagePartSubType = SUBTYPE_EXTERNAL_BODY;
                }

                // unsupported partial subtype
                else if ( MIMEHelper.bStringEquals( param[0], "partial" ) )
                {
                    throw new MIMEException( MIMEHelper.szERROR_UNSUPPORTED_PARTIAL_SUBTYPE );
                }

		addMessage( mimeMessagePart, MIMEMessage.MESSAGEPART );

                // determine subtype
                if ( MIMEHelper.bStringEquals( param[0], "rfc822" ) )
                {
                    m_messagePartSubType = SUBTYPE_RFC822;
                    MIMEMessage m = new MIMEMessage();
		            m.m_parsedPart = 1; 
                    mimeMessagePart.setMessage( m, false );

                    if ( m_dataSink != null )
                    {
        		        //if (m_bLocalStorage)
            		    //     m.setUserObject (m_dataSink.startMessage (m));
			            //else
                              m.setUserObject (m_dataSink.startMessage());
                    }

                    m_nextHeaderParent = m;

                    // defered assignment till received first blank line
                    m_nextMimeMessage = m;
                }

				break;
		}

        // initialize structure
        if ( m_currentMessage != null )
        {
            MIMEBodyPart m = (MIMEBodyPart) m_currentMessage;

            if ( param[0] != null )
    			m.setContentSubType( new String( param[0] ) );

    		if ( param[1] != null  && !MIMEHelper.bStringEquals( param[1], "boundary"))
		{
			String cps = new String (param[1]);

                        if (! (cps.indexOf ("boundary") > 0))
    			    m.setContentTypeParams( new String( param[1] ) );
		}

		if ( m_dataSink != null )
		{
    		    	//m_dataSink.contentType( m.getUserObject(), contentType[0] );
    		    	m_dataSink.contentType (m.getUserObject(), getBytecontType (nMessageType, contentType[0]));

    	    		if ( param[0] != null )
		            m_dataSink.contentSubType( m.getUserObject(), param[0] );

        	        if (param[1] != null && param[1].length > 0 &&
        	            !MIMEHelper.bStringEqualsLtrim(param[1], "boundary"))
			{
	    	            m_dataSink.contentTypeParams( m.getUserObject(), param[1] );
			}
		}

		if ( m_nCurrentMessageType == MIMEMessage.BASICPART )
		{
       		      mimeBasicPart = (MIMEBasicPart) m_currentMessage;
       		      mimeBasicPart.setContentType( contentType[0] );
    		}
        }
    }




    /**
    * add a new message ( part ) to a multipart message, mimeMessage of messagePart
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  message input message
    * @param  nMessageType message type
    * @return none
    * @exception MIMEException
    */
    private void addMessage( Object message, int nMessageType ) throws MIMEException
    {
    	MIMEMultiPart multiPart = null;
    	MIMEBasicPart basicPart = null;
    	MIMEMessagePart messagePart = null;
    	int nCurrentParentType = getCurrentParentType();

    	if ( message == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

        // determine current message
        if ( nCurrentParentType == MIMEMessage.BASICPART )
        {
            // can't add messages to a basicpart
    		return;
        }

        else if ( nCurrentParentType == MIME_UNINITIALIZED )
        {
            if ( nMessageType == MIMEMessage.BASICPART )
            {
//debug
//System.out.println( "adding basicpart to message" );

                m_mimeMessage.setBody( (MIMEBasicPart) message, false );
                m_nMessageType = MIMEMessage.BASICPART;
            }

            else if ( nMessageType == MIMEMessage.MULTIPART )
            {
//debug
//System.out.println( "adding multipart to message" );

                m_mimeMessage.setBody( (MIMEMultiPart) message, false );
                m_nMessageType = MIMEMessage.MULTIPART;
                m_currentParent.addElement( (MIMEMultiPart) message );
		m_fSeenBoundary = false;
            }

            else if ( nMessageType == MIMEMessage.MESSAGEPART )
            {
//debug
//System.out.println( "adding messagepart to message" );

                m_mimeMessage.setBody( (MIMEMessagePart) message, false );
                m_nMessageType = MIMEMessage.MESSAGEPART;
                m_currentParent.addElement( (MIMEMessagePart) message );
            }

            setCurrentMessage( message, nMessageType );
        }

        // add to vector
        else if ( nCurrentParentType == MIMEMessage.MULTIPART && m_currentMessage != null )
        {
            multiPart = (MIMEMultiPart) m_currentParent.lastElement();

            if ( nMessageType == MIMEMessage.BASICPART )
            {
//debug
//System.out.println( "adding basicpart to multipart" );

                multiPart.addBodyPart( (MIMEBasicPart) message, false );
            }

            else if ( nMessageType == MIMEMessage.MULTIPART )
            {
//debug
//System.out.println( "adding multipart to multipart" );

                multiPart.addBodyPart( (MIMEMultiPart) message, false );
                m_currentParent.addElement( (MIMEMultiPart) message );
            }

            else if ( nMessageType == MIMEMessage.MESSAGEPART )
            {
//debug
//System.out.println( "adding messagepart to multipart" );

                multiPart.addBodyPart( (MIMEMessagePart) message, false );
                m_currentParent.addElement( (MIMEMessagePart) message );
            }

            setCurrentMessage( message, nMessageType );
        }

        else if ( nCurrentParentType == MIMEMessage.MESSAGEPART && m_currentMessage != null )
        {
            messagePart = (MIMEMessagePart) m_currentParent.lastElement();

            if ( nMessageType == MIMEMessage.BASICPART )
            {
//debug
//System.out.println( "adding basicpart to messagepart" );

                messagePart.getMessage( false ).setBody( (MIMEBasicPart) message, false );
            }

            else if ( nMessageType == MIMEMessage.MULTIPART )
            {
//debug
//System.out.println( "adding multipart to messagepart" );

                messagePart.getMessage( false ).setBody( (MIMEMultiPart) message, false );
                m_currentParent.addElement( (MIMEMultiPart) message );
            }

            else if ( nMessageType == MIMEMessage.MESSAGEPART )
            {
//debug
//System.out.println( "adding messagepart to messagepart" );

                messagePart.getMessage( false ).setBody( (MIMEMessagePart) message, false );
                m_currentParent.addElement( (MIMEMessagePart) message );
            }

            setCurrentMessage( message, nMessageType );
        }
    }




    // helper functions ----------------------------------------




    /**
    * Set current mesage
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  message input message
    * @param  nMessageType message type
    * @return MIME_OK if successful, an error code otherwise
    * @exception none
    */
    private int setCurrentMessage( Object message, int nMessageType )
    {
    	if ( message == null )
    		return NSMAIL_ERR_INVALIDPARAM;

    	m_currentMessage = message;
    	m_nCurrentMessageType = nMessageType;
    	m_nextHeaderParent = message;

    	if ( nMessageType == MIMEMessage.BASICPART )
    	{
        	m_currentBasicPart = (MIMEBasicPart) message;
        	m_currentMultiPart = null;
        	m_currentMessagePart = null;

		int nCurrentParentType = getCurrentParentType();

		if (nCurrentParentType == MIMEMessage.MULTIPART && m_fSeenBoundary)
		{
			//start following adding headers to this part itself
			m_headerParent = message;
			m_nextHeaderParent = message;
		}
        }

    	else if ( nMessageType == MIMEMessage.MULTIPART )
    	{
        	m_currentBasicPart = null;
        	m_currentMultiPart = (MIMEMultiPart) message;
        	m_currentMessagePart = null;
        }

    	else if ( nMessageType == MIMEMessage.MESSAGEPART )
    	{
        	m_currentBasicPart = null;
        	m_currentMultiPart = null;
        	m_currentMessagePart = (MIMEMessagePart) message;
        }

    	return NSMAIL_OK;
    }


    /**
    * Determine if line is a boundary
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  s input string
    * @param len size of input string
    * @return : START_BOUNDARY if it's a start boundary, END_BOUNDARY if it's a end boundary, NOT_A_BOUNDARY if it's not a boundary
    * @exception none
    */
    private int boundaryCheck( byte s[], int len )
    {
    	String currentBoundary = getCurrentBoundary();
    	int boundaryLen = 0;

    	if ( len > 0 && Character.isWhitespace( (char) s[len-1] ) )
    	    len--;

    	if ( len > 0 && Character.isWhitespace( (char) s[len-1] ) )
    	    len--;

    	if ( len >= 2 && s != null && currentBoundary != null )
    	{
    	    boundaryLen = currentBoundary.length();

    		if ( (len == boundaryLen + 4) &&  s[0] == '-' && s[1] == '-' &&
			s[ boundaryLen + 2 ] == '-' && s[ boundaryLen + 3 ] == '-' &&
    		    MIMEHelper.bStringEquals( 2, s, currentBoundary ) )
    		{
    			return END_BOUNDARY;
            	}
    		else if ((len == boundaryLen + 2) && s[0] == '-' && s[1] == '-' &&
			 MIMEHelper.bStringEquals( 2, s, currentBoundary ) )
    		{
       			return START_BOUNDARY;
       		}
    	}

    	return NOT_A_BOUNDARY;
    }





    /**
    * check what kind of line this is
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  s input string
    * @param len size of input string
    * @return line type can be MIME_INFO, MIME_XHEADER, MIME_CRLF, MIME_HEADER
    * @exception none
    */
    private int checkForLineType( byte s[], int len, boolean startData )
    {
    	if ( s == null )
    		return NSMAIL_ERR_INVALIDPARAM;

    	if ( startData )
        {
            int type = boundaryCheck(s, len);

            if ( type != NOT_A_BOUNDARY )
                return type;

            return MIME_HEADER;
        }

        else if ( MIMEHelper.bStringEquals( s, "content-" ) )
            return MIME_INFO;

        else if ( MIMEHelper.bStringEquals( s, "x-" ) )
            return MIME_XHEADER;

        else if ( len == 0 )
            return MIME_CRLF;

        else
        {
            int type = boundaryCheck(s, len);

            if ( type != NOT_A_BOUNDARY )
                return type;
            else
            {
		boolean seenNonSpace = false;

                for ( int i = 0; i < len; i++ )
                {
		    if (!Character.isWhitespace ((char) s[i]))
                        seenNonSpace = true;
                    else if (seenNonSpace && Character.isWhitespace ((char) s[i])) // its not a header! Its MESSAGE_DATA!
                    {
                        break;
                    }

                    if ( s[i] == ':' )
                        return MIME_HEADER;
                }
            }
        }

        return MIME_MESSAGE_DATA;
    }




    /**
    * Get message type, subtype, content type parameters and content type
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  s input string
    * @param param[] (output) returns subtype in param[0], parameters in param[1]
    * @param contentType[] (output) returns content type in contentType[0]
    * @return message type, returns subtype in param[0], parameters in param[1]
    * @exception none
    */
    private int nGetMessageType( byte[] s, byte[] param[], int contentType[] ) throws MIMEException
    {
    	int i, nStart, nSubtypeStart;
        byte[] MessageType = null;
    	int nMessageType = MIME_UNINITIALIZED;
    	byte[] subtype = null;

    	if ( s == null || param == null )
    		return NSMAIL_ERR_INVALIDPARAM;

        int len = s.length;

    	/* skip spaces and colons */
    	for ( i=0; i < len && s[i] != 0 && ( Character.isWhitespace( (char)s[i] ) || s[i] == ':' ); i++ )
    		;

    	/* skip to subtype */
    	for ( nStart = i; i < len && s[i] != 0 && s[i] != '/' && s[i] != ';'; i++ )
    		;

    	/* content type */
    	MessageType = MIMEHelper.byteSubstring( s, nStart, i - nStart );

    	/* content subtype */
    	if ( i < len && s[i] == '/' )
    	{
    		for ( nSubtypeStart = ++i; i < len && s[i] != 0 && s[i] != ';' && s[i] != ','; i++ )
    		;

    		/* content subtype */
    		subtype = MIMEHelper.byteSubstring( s, nSubtypeStart, i - nSubtypeStart );
    	}

        param[0] = subtype;

    	/* skip spaces and colons */
        if ( i < len )
        {
        	for ( ; i < len && s[i] != 0 && ( s[i] == ' ' || s[i] == ';'  || s[i] == ',' ); i++ )
        		;
        }

    	param[1] = i > (int) s.length ? null : MIMEHelper.byteSubstring( s, i, s.length-i ); // trim

    	/* determine content type */
    	if ( MIMEHelper.bStringEquals( MessageType, "text" ) )
    	{
    		nMessageType = MIMEMessage.BASICPART;
    		contentType[0] = MIMEBasicPart.TEXT;
        }

    	else if ( MIMEHelper.bStringEquals( MessageType, "audio" ) )
    	{
    		nMessageType = MIMEMessage.BASICPART;
    		contentType[0] = MIMEBasicPart.AUDIO;
        }

    	else if ( MIMEHelper.bStringEquals( MessageType, "image" ) )
    	{
    		nMessageType = MIMEMessage.BASICPART;
    		contentType[0] = MIMEBasicPart.IMAGE;
        }

    	else if ( MIMEHelper.bStringEquals( MessageType, "video" ) )
    	{
    		nMessageType = MIMEMessage.BASICPART;
    		contentType[0] = MIMEBasicPart.VIDEO;
        }

    	else if ( MIMEHelper.bStringEquals( MessageType, "application" ) )
    	{
    		nMessageType = MIMEMessage.BASICPART;
    		contentType[0] = MIMEBasicPart.APPLICATION;
        }

    	else if ( MIMEHelper.bStringEquals( MessageType, "multipart" ) )
    	{
    		nMessageType = MIMEMessage.MULTIPART;
    		contentType[0] = MIMEBasicPart.TEXT;
        }

    	else if ( MIMEHelper.bStringEquals( MessageType, "message" ) )
    	{
    		nMessageType = MIMEMessage.MESSAGEPART;
    		contentType[0] = MIMEBasicPart.TEXT;
        }

    	else
    	{
    		//nMessageType = MIMEBasicPart.TEXT;
    		//contentType[0] = MIMEBasicPart.TEXT;
    	    	throw new MIMEException ("Error: Unknown/Unsupported Content-Type: " + new String (MessageType));
        }

    	return nMessageType;
    }




    /**
    * Parse an input string for boundary
    * default to "-----" if none is found
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  s input string
    * @return boundary string
    * @exception none
    */
    private String parseForBoundary( String s )
    {
    	String boundary;
    	int nLen;
    	int len;
    	int start;
    	int pos;
    	boolean bQuotes = false;
    	char ch;

    	if ( s == null )
    		return null;

    	pos = s.indexOf( "boundary" );
    	len = s.length();

    	if ( pos == -1 )
    		boundary = "-----";

    	else
    	{
    		/* skip spaces and colons */
    		for ( pos = pos + 9, ch = s.charAt(pos); pos < len && ( ch == ' ' || ch == '=' ); ch = s.charAt( ++pos >= len ? 0 : pos ) )
    			;

    		if ( ch == '"' )
    		{
    			bQuotes = true;
    			pos++;
    		}

            start = pos;

    		for ( nLen = 0, ch = s.charAt(pos); pos < len && ch != ';'; nLen++, ch = s.charAt( ++pos >= len ? 0 : pos ) )
    		{
    			if ( bQuotes && ch == '"' )
    				break;
    		}

            boundary = s.substring( start, start + nLen );
    	}

    	return boundary;
    }




    /**
    * translate encoding type to an integer
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param s input string
    * @return encoding type
    * @exception none
    */
    private int mime_translateMimeEncodingType( byte[] s )
    {
    	if ( s == null )
    		return NSMAIL_ERR_INVALIDPARAM;

    	if (  MIMEHelper.bStringEquals( s, "base64" ) )
    		return MIMEBodyPart.BASE64;

    	else if (  MIMEHelper.bStringEquals( s, "qp" ) || MIMEHelper.bStringEquals( s, "quoted-printable" ) )
    		return MIMEBodyPart.QP;

    	else if (  MIMEHelper.bStringEquals( s, "7bit" ) )
    		return MIMEBodyPart.E7BIT;

    	else if (  MIMEHelper.bStringEquals( s, "8bit" ) )
    		return MIMEBodyPart.E8BIT;

    	else if (  MIMEHelper.bStringEquals( s, "binary" ) )
    		return MIMEBodyPart.BINARY;

    	return MIMEBodyPart.E7BIT;
    }




    /**
    * translate disposition type to an integer
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  s input string
    * @param param[] return content disposition parameters in param[0]
    * @return <PRE>
    *   NSMAIL_ERR_INVALIDPARAM     if parameters are invalid
    *   MIMEBodyPart.INLINE         disposition type is inline
    *   MIMEBodyPart.ATTACHMENT     disposition type is an attachment</PRE>
    *
    * @exception MIMEException
    */
    private int translateDispType( byte[] s, byte[] param[] )
    {
    	if ( s == null || param == null )
    		return NSMAIL_ERR_INVALIDPARAM;

        param[0] = null;

    	if ( MIMEHelper.bStringEquals( s, "inline" ) )
    	{
    	    if ( s.length >= 8 )
    	        param[0] = MIMEHelper.byteSubstring( s, 0, 7 );

    		return MIMEBodyPart.INLINE;
    	}

    	else if ( MIMEHelper.bStringEquals( s, "attachment" ) )
    	{
    	    if ( s.length >= 12 )
    	        param[0] = MIMEHelper.byteSubstring( s, 0, 12 );

    		return MIMEBodyPart.ATTACHMENT;
    	}

    	else if ( s.length >= 8 )
            param[0] = MIMEHelper.byteSubstring( s, 0, 7 );

    	return MIMEBodyPart.INLINE;
    }

    // ----------new one 
    private int translateDispType (byte[] s)
    {
    	if ( s == null)
    		return NSMAIL_ERR_INVALIDPARAM;

    	if (MIMEHelper.bStringEquals (s, "inline"))
    		return MIMEBodyPart.INLINE;

    	if (MIMEHelper.bStringEquals (s, "attachment"))
    		return MIMEBodyPart.ATTACHMENT;

    	return MIMEBodyPart.INLINE;
    }

    private int getCurrentParentType()
    {
        if ( m_currentParent.size() > 0 )
        {
            Object o = m_currentParent.lastElement();

            if ( o != null )
            {
                if ( o instanceof MIMEBasicPart )
                    return MIMEMessage.BASICPART;

                else if ( o instanceof MIMEMultiPart )
                    return MIMEMessage.MULTIPART;

                else if ( o instanceof MIMEMessagePart )
                    return MIMEMessage.MESSAGEPART;
            }
        }

        return MIME_UNINITIALIZED;
    }



    /**
    * get the boundary corresponding to the current multiPart message
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  none
    * @return current boundary string, null if there isn't a current boundary
    * @exception none
    */
    private String getCurrentBoundary()
    {
        if ( m_currentParent.size() > 0 )
        {
            for ( int i = m_currentParent.size(); i > 0; i-- )
            {
                Object o = m_currentParent.elementAt( i - 1 );

                if ( o != null && o instanceof MIMEMultiPart )
                    return ( (MIMEMultiPart) o ).getBoundary();
            }
        }

        return null;
    }




    /**
    * get the boundary corresponding to the current multiPart message
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param  none
    * @return current boundary string, null if there isn't a current boundary
    * @exception none
    */
    private void unwindCurrentParent( byte[] s, boolean delete )
    {
        if ( m_currentParent.size() > 0 )
        {
        	String boundary = new String( s, 2, s.length - 2 );

            for ( int i = m_currentParent.size(); i > 0; i-- )
            {
                Object o = m_currentParent.elementAt( i - 1 );

                if ( o != null && o instanceof MIMEMultiPart && boundary.startsWith( ((MIMEMultiPart) o ).getBoundary() ) )
                {
                    if ( delete )
                    {
                        m_currentParent.removeElementAt( i - 1 );

                        // reposition to next multipart
                        for ( ; m_currentParent.size() > 0; )
                        {
                            o = m_currentParent.lastElement();

                            if ( o instanceof MIMEMultiPart )
                                return;

                            m_currentParent.removeElementAt( m_currentParent.size() - 1 );
                        }
                    }

                    return;
                }

                m_currentParent.removeElementAt( i - 1 );
            }

        }
    }




    /**
    * check for empty messages in all basicparts
    *
    * @author Carson Lee
    * @version %I%, %G%
    *
    * @param
    *
    *   o : any mime message structure, usually MIMEMessage
    *
    * @return none
    * @exception MIMEException
    */
    void checkForEmptyMessages( Object o ) throws MIMEException
    {
        if ( o == null )
    	    throw new MIMEException( MIMEHelper.szERROR_BAD_PARAMETER );

		if ( o instanceof MIMEMessage )
	    {
	        checkForEmptyMessages( ((MIMEMessage) o).getBody() );
	    }

		else if ( o instanceof MIMEBasicPart )
		{
   		    if ( ((MIMEBasicPart) o).getMessageDataLen() == 0 )
            	throw new MIMEException( MIMEHelper.szERROR_EMPTY_MESSAGE );
		}

		else if ( o instanceof MIMEMultiPart )
		{
		    MIMEMultiPart m = (MIMEMultiPart) o;
		    int count = m.getBodyPartCount();

		    for ( int i = 0; i < count; i++ )
		    {
		        Object part = m.getBodyPart( i, false );
		        checkForEmptyMessages( part );
		    }
		}

		else if ( o instanceof MIMEMessagePart )
		{
		    String cst = (((MIMEMessagePart)o).getContentSubType()).trim();

		    if (cst == null || cst.equalsIgnoreCase("rfc822"))
		    {
	               MIMEMessage messagePart = ((MIMEMessagePart) o).getMessage();
	               checkForEmptyMessages( messagePart );
		    }
		    else if (cst.equalsIgnoreCase("external-body"))
		    {
			Header[] hdrs = ((MIMEMessagePart)o).getAllHeaders();
			//if (hdrs == null)  
    	    		//  throw new MIMEException (MIMEHelper.szERROR_BAD_EXTERNAL_MESSAGE_PART); 
		    }
		    else if (cst.equalsIgnoreCase("partial"))
		    {
    	    		 throw new MIMEException (MIMEHelper.szERROR_UNSUPPORTED_PARTIAL_SUBTYPE);
		    }
		    else
    	    		throw new MIMEException( MIMEHelper.szERROR_BAD_MIME_MESSAGE);
		}
    }
}

