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

package netscape.messaging.smtp;

import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;
import netscape.messaging.*;

/**
*The SMTPClient class represents the SMTP client.
*The client uses this class for communicating with a
*server using the SMTP protocol.
*The SMTP client conforms to the specifications of the client
*described in RFC 821.
*<p>For a list of the SMTP RFCs referenced in the
*Messaging Access SDK and their URLs,
*see "Where to Find More Information" in "About This Book."
*@author derekt@netscape.com
*@version 1.0
*/

public class SMTPClient
{
    ///////////////////////////////////////////////////////////////////////////
    // Internal class used for socket I/O.
    ///////////////////////////////////////////////////////////////////////////
    private IO m_io;

    ///////////////////////////////////////////////////////////////////////////
    // Data member used for C-runtime equivalent routines.
    ///////////////////////////////////////////////////////////////////////////
    private Common m_common;

    ///////////////////////////////////////////////////////////////////////////
    // Data member used to store the timeout value.
    ///////////////////////////////////////////////////////////////////////////
    private int m_timeout;

    ///////////////////////////////////////////////////////////////////////////
    // Last character sent to the server used for byte stuffing messages.
    ///////////////////////////////////////////////////////////////////////////
    private byte m_lastSentChar;

    ///////////////////////////////////////////////////////////////////////////
    // State variable indicating if the DATA command was last executed.
    ///////////////////////////////////////////////////////////////////////////
    private boolean m_fSendingData;

    ///////////////////////////////////////////////////////////////////////////
    // Boolean variable to determine if we can send another command.
    ///////////////////////////////////////////////////////////////////////////
    private boolean m_mustProcess;

    ///////////////////////////////////////////////////////////////////////////
    // Callback data members.
    ///////////////////////////////////////////////////////////////////////////

    private Vector m_pendingList;
    private ISMTPSink m_notifySink;

    ///////////////////////////////////////////////////////////////////////////
    // Data members specific for PIPELINING mode.
    ///////////////////////////////////////////////////////////////////////////

    private Vector m_pipelinedCommandList;
    private boolean m_fPipeliningSupported;
    private boolean m_fPipeliningEnabled;

    ///////////////////////////////////////////////////////////////////////////
    // Reused response objects.
    ///////////////////////////////////////////////////////////////////////////

    private StringBuffer m_responseMessage;

    ///////////////////////////////////////////////////////////////////////////
    // Byte arrays that are reused for commands and responses.
    ///////////////////////////////////////////////////////////////////////////

    private byte[] m_response;
    private byte[] m_messageData;
    private byte[] m_itoaBuffer;

    ///////////////////////////////////////////////////////////////////////////
    // Constants.
    ///////////////////////////////////////////////////////////////////////////

    private final static int m_defaultPort = 25;
    private final static int m_minChunkSize = 1024;
    private final static int m_maxReplyLine = 512;

    private final static byte[] m_bdat = { 'B', 'D', 'A', 'T', ' ' };
    private final static byte[] m_data = { 'D', 'A', 'T', 'A' };
    private final static byte[] m_ehlo = { 'E', 'H', 'L', 'O', ' ' };
    private final static byte[] m_expn = { 'E', 'X', 'P', 'N', ' ' };
    private final static byte[] m_help = { 'H', 'E', 'L', 'P', ' ' };
    private final static byte[] m_mail = { 'M', 'A', 'I', 'L', ' ', 'F', 'R', 'O', 'M', ':', '<' };
    private final static byte[] m_noop = { 'N', 'O', 'O', 'P' };
    private final static byte[] m_quit = { 'Q', 'U', 'I', 'T' };
    private final static byte[] m_rcpt = { 'R', 'C', 'P', 'T', ' ', 'T', 'O', ':', '<' };
    private final static byte[] m_rset = { 'R', 'S', 'E', 'T' };
    private final static byte[] m_vrfy = { 'V', 'R', 'F', 'Y', ' ' };
    //private final static byte[] m_rBracket = { '>', ' ' };
    private final static byte[] m_rBracket = { '>' };
    private final static byte[] m_newline = { '\r', '\n' };
    private final static byte[] m_eomessage = { '\r', '\n', '.' };
    private final static byte[] m_bdatLast = { ' ', 'L', 'A', 'S', 'T' };
    private final static byte[] m_empty = {};

    ///////////////////////////////////////////////////////////////////////////
    // Command types for SMTP.
    ///////////////////////////////////////////////////////////////////////////

    private final static int BDAT = 0;
    private final static int CONN = 1;
    private final static int DATA = 2;
    private final static int EHLO = 3;
    private final static int EXPN = 4;
    private final static int HELP = 5;
    private final static int MAIL = 6;
    private final static int NOOP = 7;
    private final static int QUIT = 8;
    private final static int RCPT = 9;
    private final static int RSET = 10;
    private final static int SEND = 11;
    private final static int SENDCOMMAND = 12;
    private final static int VRFY = 13;

    private final static Integer BDAT_OBJ = new Integer(BDAT);
    private final static Integer CONN_OBJ = new Integer(CONN);
    private final static Integer DATA_OBJ = new Integer(DATA);
    private final static Integer EHLO_OBJ = new Integer(EHLO);
    private final static Integer EXPN_OBJ = new Integer(EXPN);
    private final static Integer HELP_OBJ = new Integer(HELP);
    private final static Integer MAIL_OBJ = new Integer(MAIL);
    private final static Integer NOOP_OBJ = new Integer(NOOP);
    private final static Integer QUIT_OBJ = new Integer(QUIT);
    private final static Integer RCPT_OBJ = new Integer(RCPT);
    private final static Integer RSET_OBJ = new Integer(RSET);
    private final static Integer SEND_OBJ = new Integer(SEND);
    private final static Integer SENDCOMMAND_OBJ = new Integer(SENDCOMMAND);
    private final static Integer VRFY_OBJ = new Integer(VRFY);

    ///////////////////////////////////////////////////////////////////////////
    // Error messages for SMTP.
    ///////////////////////////////////////////////////////////////////////////

    private final static String errProcessResponses = new String("Must call processResponses()");
    private final static String errSendingData = new String("Must send data immediately after the Data() command");
    private final static String errNoPipelining = new String("Pipelining is not supported");
    private final static String errParse = new String("Error while parsing response");

    /**
    *Constructor for SMTPClient that takes an SMTPSink as a parameter.
	*@param in_sink Response sink that implements the ISMTPSink interface.
	*@see ISMTPSink
    */
    public SMTPClient( ISMTPSink in_sink )
    {
        ///////////////////////////////////////////////////////////////////////
        // Variables that get initialized once in the constructor.
        ///////////////////////////////////////////////////////////////////////

        m_io = new IO();
        m_common = new Common();
        m_timeout = 0;
        m_pendingList = new Vector();
        m_notifySink = in_sink;

        m_response = new byte[m_maxReplyLine];
        m_messageData = new byte[m_minChunkSize];
        m_itoaBuffer = new byte[m_maxReplyLine];
        m_responseMessage = new StringBuffer( m_maxReplyLine );

        ///////////////////////////////////////////////////////////////////////
        // Variables that get initialized in the constructor and on every new
        // connection.
        ///////////////////////////////////////////////////////////////////////

        m_fSendingData = false;
        m_fPipeliningSupported = false;
        m_fPipeliningEnabled = false;
        m_lastSentChar = '\n';
    }

    ///////////////////////////////////////////////////////////////////////////
    // Protocol commands
    ///////////////////////////////////////////////////////////////////////////

    /**
    *Sends binary data chunks to the server.
    *<p>Not to be used with the data() command.
    *With SMTPClient.send(), data should be sent with SMTPClient.data
    *and not with SMTPClient.bdat.
    *For more information, see "Sending the Message."
    *<P>Note: bdat is not supported by Messaging Server 4.0. Use SMTPClient.data instead.
	*@param in_data Array for the raw data to send to the server.
	*@param in_offset Offset for data.
	*@param in_length Number of bytes to send.
	*@param in_fLast Indicates whether this is the last chunk of data to send.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#bdat
    *@see ISMTPSink#error
    *@see #data
    */
    public synchronized void bdat( byte[] in_data,
                                    int in_offset,
                                    int in_length,
                                    boolean in_fLast ) throws IOException
    {
        int l_numBytes;

        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_length < 0 || in_offset < 0 )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.write( m_bdat );

        l_numBytes = m_common.itoa( in_length, m_itoaBuffer );
        m_io.write( m_itoaBuffer, 0, l_numBytes );

        if ( in_fLast )
        {
            m_io.write( m_bdatLast );
        }

        m_io.write( m_newline, 0, m_newline.length );
        m_io.write( in_data, in_offset, in_length );
        m_io.write( m_newline, 0, m_newline.length );

        m_pendingList.addElement( BDAT_OBJ );

        if ( m_fPipeliningEnabled == false )
        {
            m_io.flush();
            m_mustProcess = true;
        }

    }

    /**
    *Connects to the server using the default port.
    *<p>To specify the connection port, use the
    *form of connect that takes a port number.
	*@param in_server Name of the host server to connect to.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#connect
    *@see ISMTPSink#error
    *@see #quit
    *@see #connect
    */
    public synchronized void connect( String in_server ) throws IOException
    {
        connect( in_server, m_defaultPort );
    }

    /**
    *Connects to the server using the specified port.
    *<p>To use the default connection port, use the
    *form of connect that takes only the server name.
	*@param in_server Name of the host server to connect to.
	*@param in_port Number of the port to connect to.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#connect
    *@see ISMTPSink##error
    *@see #quit
    *@see #connect
    */
    public synchronized void connect( String in_server, int in_port ) throws IOException
    {
        if ( in_server == null || in_port < 0 )
        {
            throw new IllegalArgumentException();
        }

        m_fSendingData = false;
        m_fPipeliningSupported = false;
        m_fPipeliningEnabled = false;
        m_lastSentChar = '\n';

        m_pendingList.removeAllElements();

        m_io.connect( in_server, in_port, m_minChunkSize, m_messageData.length );

        m_io.setTimeout( m_timeout );

        m_pendingList.addElement( CONN_OBJ );

        m_mustProcess = true;
    }

    /**
    *Prepares to send data to the server.
    *With SMTPClient.send(),
    *data should be sent with SMTPClient.data and not with SMTPClient.bdat.
    *For more information, see "Sending the Message."
    *Note: Not to be used with the bdat() command.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#data
    *@see ISMTPSink#error
    */
    public synchronized void data() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.send( m_data, false );

        m_pendingList.addElement( DATA_OBJ );

        m_mustProcess = true;
    }

    /**
    *Closes the socket connection with the server.
    *Can be used to perform a high level "Cancel" while
    *sending a message.
    *NOTE: Do not call processResponses() after this method.
    *@exception IOException thrown on IO error.
    */
    public synchronized void disconnect() throws IOException
    {
        m_io.disconnect();
    }

    /**
	*Determines the ESMTP server extensions.
	*The callback on the response
	*sink identifies the various SMTP extensions supported by the server.
	*@param in_domain The domain name.
    *@exception IOException thrown on IO error.
    *@see ISMTPSink#ehlo
    *@see ISMTPSink#ehloComplete
    *@see ISMTPSink#error
    */
    public synchronized void ehlo( String in_domain ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_domain == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.write( m_ehlo );
        m_io.send( in_domain , false );

        m_pendingList.addElement( EHLO_OBJ );
        m_mustProcess = true;
    }

    /**
    *Expands a given mailing list.
    *Gets the email addresses of the users on the mailing list.
	*@param in_mailingList The mailing list to expand.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#expand
    *@see ISMTPSink#expandComplete
    *@see ISMTPSink#error
    */
    public synchronized void expand( String in_mailingList ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_mailingList == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.write( m_expn );
        m_io.send( in_mailingList, false );

        m_pendingList.addElement( EXPN_OBJ );
        m_mustProcess = true;
    }

    /**
    *Obtains help on a given topic.
	*@param in_helpTopic One-word help topic to get information on. If null,
	*user may get Help on Help or a default. See implementation on the server.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#help
    *@see ISMTPSink#helpComplete
    *@see ISMTPSink#error
    */
    public synchronized void help( String in_helpTopic ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.write( m_help );

        if ( in_helpTopic != null )
        {
            m_io.send( in_helpTopic, false );
        }
        else
        {
            m_io.send( m_empty, false );
        }

        m_pendingList.addElement( HELP_OBJ );
        m_mustProcess = true;
    }

    /**
    *Sets the sender of the message with optional ESMTP parameters.
	*@param in_reverseAddress Address of the sender of the message.
	*@param in_esmtpParams Any ESMTP (Extended SMTP) parameter.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#mailFrom
    *@see ISMTPSink#error
    *@see #rcptTo
    */
    public synchronized void mailFrom( String in_reverseAddress, String in_esmtpParams ) throws IOException
    {
        int count;

        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_reverseAddress == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.write( m_mail );
        m_io.write( in_reverseAddress );
        m_io.write( m_rBracket );

        if ( in_esmtpParams != null )
        {
            m_io.write( in_esmtpParams );
        }

        m_io.send( m_empty, m_fPipeliningEnabled );
        m_pendingList.addElement( MAIL_OBJ );

        if ( m_fPipeliningEnabled == false )
        {
            m_mustProcess = true;
        }
    }

    /**
    *Gets positive server response; does not affect SMTP session.
    *Issues Noop command.
    *<P>The server responds to commands with a "still here" response.
    *Sending the noop method does nothing except force this server response.
    *Can be used to maintain server connection, perhaps being issued
    *at timed intervals to make sure that the server is still active.
    *Not needed by applications that do something and do not maintain a connection with the server.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#noop
    *@see ISMTPSink#error
    */
    public synchronized void noop() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.send( m_noop, false );

        m_pendingList.addElement( NOOP_OBJ );
        m_mustProcess = true;
    }

    /**
    *Closes the connection with the server.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#quit
    *@see ISMTPSink#error
    */
    public synchronized void quit() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.send( m_quit, false );

        m_pendingList.addElement( QUIT_OBJ );
        m_mustProcess = true;
    }

    /**
    *Sets the recipient of the message with optional ESMTP parameters.
	*@param in_forwardAddress Address of the message recipient.
	*@param in_esmtpParams Any ESMTP parameters to set.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#rcptTo
    *@see ISMTPSink#error
    */
    public synchronized void rcptTo( String in_forwardAddress, String in_esmtpParams ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_forwardAddress == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.write( m_rcpt );
        m_io.write( in_forwardAddress );
        m_io.write( m_rBracket );

        if ( in_esmtpParams != null )
        {
            m_io.write( in_esmtpParams );
        }

        m_io.send( m_empty, m_fPipeliningEnabled );
        m_pendingList.addElement( RCPT_OBJ );

        if ( m_fPipeliningEnabled == false )
        {
            m_mustProcess = true;
        }
    }

    /**
    *Resets the state of the server; flushes any sender and recipient
    *information.
    *<p>Cancels the current mail transfer and all current processes,
    *discards data, and clears all states. Returns to the state
    *that followed the last method that sent the EHLO command.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#reset
    *@see ISMTPSink#error
    *@see #ehlo
    */
    public synchronized void reset() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.send( m_rset, m_fPipeliningEnabled );
        m_pendingList.addElement( RSET_OBJ );
    }

    /**
    *Sends message data to the server.
    *NOTE: To be used with the data() command and not with the bdat() command.
	*@param in_inputStream Input stream containing the data to send.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#send
    *@see ISMTPSink#error
    *@see #data
    */
    public synchronized void send( InputStream in_inputStream ) throws IOException
    {
        int l_offset;
        int l_byteCount;
        byte l_dot = (byte)'.';
        byte l_lineFeed = (byte)'\n';

        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_inputStream == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == false )
        {
            throw new SMTPException(errSendingData);
        }

        ///////////////////////////////////////////////////////////////////
        // Send the raw message data to the server and return the response.
        ///////////////////////////////////////////////////////////////////

        do
        {
            ///////////////////////////////////////////////////////////////
            // Read in the data from the stream.
            ///////////////////////////////////////////////////////////////

            l_byteCount = in_inputStream.read( m_messageData );

            ///////////////////////////////////////////////////////////////
            // Create the string in order to search for the "\n." sequence.
            // and send the data.
            ///////////////////////////////////////////////////////////////

            l_offset = 0;

            for ( int count = 0; count < l_byteCount; count++ )
            {
                if ( (m_lastSentChar == l_lineFeed) &&
                        (l_dot == m_messageData[count]) )
                {
                    m_io.write( m_messageData, l_offset, count-l_offset );
                    m_io.write( l_dot );
                    l_offset = count;
                }
                m_lastSentChar = m_messageData[count];
            }

            if ( l_offset < l_byteCount )
            {
                m_io.write( m_messageData, l_offset, l_byteCount-l_offset );
            }
        }
        while ( l_byteCount != -1 );

        m_io.send( m_eomessage, m_fPipeliningEnabled );

        m_lastSentChar = l_lineFeed;
        m_pendingList.addElement( SEND_OBJ );

        m_fSendingData = false;

        if ( m_fPipeliningEnabled == false )
        {
            m_mustProcess = true;
        }
    }

    /**
    *Sends an unsupported command to the server.
    *Sends commands that are not supported by the Messaging Access SDK
    *implementation of SMTP.
    *<P>NOTE: This method is primarily intended to support extensions
    *to the protocol to meet client application needs.
	*@param in_command Raw command to send to the server.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#sendCommand
    *@see ISMTPSink#sendCommandComplete
    *@see ISMTPSink#error
    */
    public synchronized void sendCommand( String in_command ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_command == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.send( in_command, false );

        m_pendingList.addElement( SENDCOMMAND_OBJ );
        m_mustProcess = true;
    }

    /**
    *Verifies a username.
	*@param in_user User name to verify.
    *@exception IOException If an I/O error occurs.
    *@see ISMTPSink#verify
    *@see ISMTPSink#error
    */
    public synchronized void verify( String in_user ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_user == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new SMTPException(errProcessResponses);
        }

        if ( m_fSendingData == true )
        {
            throw new SMTPException(errSendingData);
        }

        m_io.write( m_vrfy );
        m_io.send( in_user, false );

        m_pendingList.addElement( VRFY_OBJ );
        m_mustProcess = true;
    }

    /**
    *Reads in responses from the server and invokes the appropriate sink methods.
    *<p>Processes the server responses for API commands.
    *It invokes the callback methods provided by the user for all
    *responses that are available at the time of execution.
    *NOTE: If a timeout occurs the user can continue by calling processResponses() again.
    *@exception SMTPServerException If a server error occurs.
    *@exception InterruptedIOException If a time-out occurs.
    *@exception IOException If an I/O error occurs.
    */
    public void processResponses() throws IOException
    {
        synchronized(m_responseMessage)
        {
            try
            {
                Integer l_commandType;

                m_io.flush();

                ///////////////////////////////////////////////////////////////////
                // Prcoess as many of the responses as possible.
                ///////////////////////////////////////////////////////////////////

                while ( !m_pendingList.isEmpty() )
                {
                    l_commandType = (Integer)m_pendingList.firstElement();

                    switch( l_commandType.intValue() )
                    {
                        case BDAT:
                            parseBdat();
                            break;
                        case CONN:
                            parseConnect();
                            break;
                        case DATA:
                            parseData();
                            break;
                        case EHLO:
                            parseEhlo();
                            break;
                        case EXPN:
                            parseExpand();
                            break;
                        case HELP:
                            parseHelp();
                            break;
                        case MAIL:
                            parseMail();
                            break;
                        case NOOP:
                            parseNoop();
                            break;
                        case QUIT:
                            parseQuit();
                            m_io.disconnect();
                            break;
                        case RCPT:
                            parseRcpt();
                            break;
                        case RSET:
                            parseReset();
                            break;
                        case SEND:
                            parseSend();
                            break;
                        case SENDCOMMAND:
                            parseSendCommand();
                            break;
                        case VRFY:
                            parseVerify();
                            break;
                    }

                    m_pendingList.removeElementAt( 0 );
                }

                m_mustProcess = false;
            }
            catch( SMTPServerException e )
            {
                m_mustProcess = false;
                m_pendingList.removeElementAt( 0 );
                throw e;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Preferences
    ///////////////////////////////////////////////////////////////////////////

    /**
    *Sets the size of the data chunks that are read from the input stream and
    *sent to the server.  The minimum chunk size is 1024.
    *NOTE: Do not call processResponses() after this method.
	*@param in_chunkSize Size of chunk used for sending messages using the send() method. Minimum chunk size: 1024. Default: 1 K.
    *@exception IOException thrown on IO error.
    *@see #send
    */
    public synchronized void setChunkSize( int in_chunkSize )
    {
        synchronized(m_responseMessage)
        {
            if ( in_chunkSize < m_minChunkSize )
            {
                throw new IllegalArgumentException();
            }

            m_messageData = new byte[in_chunkSize];
        }
    }

    /**
    *Registers a new response sink, overriding the one passed into the
    *constructor or any one set afterwards.
    *NOTE: Do not call processResponses() after setResponseSink().
	*@param in_responseSink The new ISMTPSink to use.
    *@exception IOException If an I/O error occurs.
    *@see #processResponses
    */
    public synchronized void setResponseSink( ISMTPSink in_responseSink )
    {
        synchronized(m_responseMessage)
        {
            if ( in_responseSink == null )
            {
                throw new IllegalArgumentException();
            }

            m_notifySink = in_responseSink;
        }
    }

    /**
    *Sets the amount of time allowed to wait
    *before returning control to the user.
    *NOTE: Do not call processResponses() after this method.
	*@param in_timeout Time-out period to set. Values, in milliseconds:
	*<ul>
	*<li>0 = infinite time-out (default)
	*<li>-1 = no waiting
	*<li>> 0 = length of time-out period
	*</ul>
    *@exception IOException If an I/O error occurs.
    *@see #processResponses
    */
    public synchronized void setTimeout( int in_timeout ) throws IOException
    {
        synchronized(m_responseMessage)
        {
            if ( in_timeout < -1 )
            {
                throw new IllegalArgumentException();
            }

            m_timeout = in_timeout;

            if ( m_io != null )
            {
                m_io.setTimeout( m_timeout );
            }
        }
    }

    /**
    *Enables PIPELINING (batching) of commands, if supported by the server.
    *If PIPELINING is not supported by the server, an SMTPException
    *is thrown. The user can determine if PIPELINING is supported through the
    *notification of the ehlo() method.
    *<P>NOTE: Do not call processResponses() after this method.
	*@param in_enablePipelining Boolean value to enable/disable PIPELINING.
    *@exception SMTPException If PIPELINING is not supported by the server.
    *@see #ehlo
    */
    public synchronized void setPipelining( boolean in_enablePipelining ) throws SMTPException
    {
        synchronized(m_responseMessage)
        {
            if ( in_enablePipelining )
            {
                if ( m_fPipeliningSupported )
                {
                    m_fPipeliningEnabled = true;
                }
                else
                {
                    throw new SMTPException(errNoPipelining);
                }
            }
            else
            {
                m_fPipeliningEnabled = false;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Internal functions
    ///////////////////////////////////////////////////////////////////////////

    /**
    *Separates the response code from the message part of the response.
    */
    private final int setStatusInfo(
                        byte[] in_response,
                        int in_length,
                        StringBuffer out_responseMessage ) throws IOException
    {
        int l_responseCode;

        if ( in_length < 4 ||
                Character.isDigit((char)in_response[0]) == false ||
                Character.isDigit((char)in_response[1]) == false ||
                Character.isDigit((char)in_response[2]) == false )
        {
            throw new SMTPException(errParse);
        }

        l_responseCode = ( (Character.digit( (char)in_response[0], 10 ) * 100) +
                            (Character.digit( (char)in_response[1], 10 ) * 10) +
                            (Character.digit( (char)in_response[2], 10 ) * 1) );

        out_responseMessage.setLength( in_length - 4 );

        for ( int count = 0; count < (in_length-4); count++ )
        {
            out_responseMessage.setCharAt( count, (char)in_response[count+4] );
        }

        return l_responseCode;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Methods for parsing the responses and invoking sink notifications.
    ///////////////////////////////////////////////////////////////////////////

    private final void parseBdat() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.bdat( l_rCode, m_responseMessage );
        }
    }

    private final void parseConnect() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.connect( l_rCode, m_responseMessage );
        }
    }

    private final void parseData() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.data( l_rCode, m_responseMessage );
            m_fSendingData = true;
        }
    }

    private final void parseEhlo() throws IOException
    {
        int l_rCode;
        int l_byteCount;

        do
        {
            l_byteCount = m_io.readLine( m_response );

            l_rCode = setStatusInfo( m_response,
                                        l_byteCount,
                                        m_responseMessage );

            if ( m_fPipeliningSupported == false &&
                    m_responseMessage.length() >= 10 )
            {
                if ( m_responseMessage.charAt( 0 ) == 'P' &&
                        m_responseMessage.charAt( 1 ) == 'I' &&
                        m_responseMessage.charAt( 2 ) == 'P' &&
                        m_responseMessage.charAt( 3 ) == 'E' &&
                        m_responseMessage.charAt( 4 ) == 'L' &&
                        m_responseMessage.charAt( 5 ) == 'I' &&
                        m_responseMessage.charAt( 6 ) == 'N' &&
                        m_responseMessage.charAt( 7 ) == 'I' &&
                        m_responseMessage.charAt( 8 ) == 'N' &&
                        m_responseMessage.charAt( 9 ) == 'G' )
                {
                    m_fPipeliningSupported = true;
                }
            }

            if ( l_rCode >= 400 )
            {
                try
                {
                    m_notifySink.error( l_rCode, m_responseMessage );
                }
                catch ( SMTPServerException e )
                {
                    while( '-' == (char)m_response[3] )
                    {
                        m_io.readLine( m_response );
                    }
                    throw e;
                }
            }
            else
            {
                m_notifySink.ehlo( l_rCode, m_responseMessage );
            }
        }
        while( '-' == (char)m_response[3] );

        m_notifySink.ehloComplete();
    }

    private final void parseExpand() throws IOException
    {
        int l_rCode;
        int l_byteCount;

        do
        {
            l_byteCount = m_io.readLine( m_response );

            l_rCode = setStatusInfo( m_response,
                                        l_byteCount,
                                        m_responseMessage );

            if ( l_rCode >= 400 )
            {
                try
                {
                    m_notifySink.error( l_rCode, m_responseMessage );
                }
                catch ( SMTPServerException e )
                {
                    while( '-' == (char)m_response[3] )
                    {
                        m_io.readLine( m_response );
                    }
                    throw e;
                }
            }
            else
            {
                m_notifySink.expand( l_rCode, m_responseMessage );
            }
        }
        while( '-' == (char)m_response[3] );

        m_notifySink.expandComplete();
    }

    private final void parseHelp() throws IOException
    {
        int l_rCode;
        int l_byteCount;

        do
        {
            l_byteCount = m_io.readLine( m_response );

            l_rCode = setStatusInfo( m_response,
                                        l_byteCount,
                                        m_responseMessage );

            if ( l_rCode >= 400 )
            {
                try
                {
                    m_notifySink.error( l_rCode, m_responseMessage );
                }
                catch ( SMTPServerException e )
                {
                    while( '-' == (char)m_response[3] )
                    {
                        m_io.readLine( m_response );
                    }
                    throw e;
                }
            }
            else
            {
                m_notifySink.help( l_rCode, m_responseMessage );
            }
        }
        while( '-' == (char)m_response[3] );

        m_notifySink.helpComplete();
    }

    private final void parseMail() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.mailFrom( l_rCode, m_responseMessage );
        }
    }

    private final void parseNoop() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.noop( l_rCode, m_responseMessage );
        }
    }

    private final void parseQuit() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.quit( l_rCode, m_responseMessage );
        }
    }

    private final void parseRcpt() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.rcptTo( l_rCode, m_responseMessage );
        }
    }

    private final void parseReset() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.reset( l_rCode, m_responseMessage );
        }
    }

    private final void parseSend() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.send( l_rCode, m_responseMessage );
        }
    }

    private final void parseSendCommand() throws IOException
    {
        int l_rCode;
        int l_byteCount;

        do
        {
            l_byteCount = m_io.readLine( m_response );

            l_rCode = setStatusInfo( m_response,
                                        l_byteCount,
                                        m_responseMessage );

            if ( l_rCode >= 400 )
            {
                try
                {
                    m_notifySink.error( l_rCode, m_responseMessage );
                }
                catch ( SMTPServerException e )
                {
                    while( '-' == (char)m_response[3] )
                    {
                        m_io.readLine( m_response );
                    }
                    throw e;
                }
            }
            else
            {
                m_notifySink.sendCommand( l_rCode, m_responseMessage );
            }
        }
        while( '-' == (char)m_response[3] );

        m_notifySink.sendCommandComplete();
    }

    private final void parseVerify() throws IOException
    {
        int l_rCode;
        int l_byteCount = m_io.readLine( m_response );

        l_rCode = setStatusInfo( m_response, l_byteCount, m_responseMessage );

        if ( l_rCode >= 400 )
        {
            m_notifySink.error( l_rCode, m_responseMessage );
        }
        else
        {
            m_notifySink.verify( l_rCode, m_responseMessage );
        }
    }
}
