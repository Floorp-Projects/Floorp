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

package netscape.messaging.pop3;

import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;
import netscape.messaging.*;

/**
*The POP3Client class represents the POP3 client.
*The client uses this class for communicating with a
*server using the POP3 protocol.
*The POP3 client conforms to the client specifications
*described in RFC 1939. For the URL,
*see "Where to Find More Information" in "About This Book."
*@author derekt@netscape.com
*@version 1.0
*/
public class POP3Client
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
    // Boolean variable to determine if we can send another command.
    ///////////////////////////////////////////////////////////////////////////
    private boolean m_mustProcess;

    ///////////////////////////////////////////////////////////////////////////
    // Callback data members.
    ///////////////////////////////////////////////////////////////////////////
    private Vector m_pendingList;
    private IPOP3Sink m_notifySink;

    ///////////////////////////////////////////////////////////////////////////
    // Reused response objects.
    ///////////////////////////////////////////////////////////////////////////
	private StringBuffer m_responseLine;
    private StringBuffer m_responseMessage;
    private int m_currentMessageNumber;

    ///////////////////////////////////////////////////////////////////////////
    // Byte arrays that are reused for commands and responses.
    ///////////////////////////////////////////////////////////////////////////
    private byte[] m_response;
    private byte[] m_messageData;
    private int m_messageDataSize;
    private int m_overFlowSize;
    private byte[] m_itoaBuffer;
    private byte[] m_tokenBuffer;
    private boolean m_multiLineState;

    ///////////////////////////////////////////////////////////////////////////
    // Constants.
    ///////////////////////////////////////////////////////////////////////////

    private final static int m_defaultPort = 110;
    private final static int m_minChunkSize = 1024;
    private final static int m_maxReplyLine = 512;

    private final static byte[] m_dele = { 'D', 'E', 'L', 'E', ' ' };
    private final static byte[] m_list = { 'L', 'I', 'S', 'T', ' ' };
    private final static byte[] m_noop = { 'N', 'O', 'O', 'P' };
    private final static byte[] m_pass = { 'P', 'A', 'S', 'S', ' ' };
    private final static byte[] m_quit = { 'Q', 'U', 'I', 'T' };
    private final static byte[] m_rset = { 'R', 'S', 'E', 'T' };
    private final static byte[] m_retr = { 'R', 'E', 'T', 'R', ' ' };
    private final static byte[] m_stat = { 'S', 'T', 'A', 'T' };
    private final static byte[] m_top = { 'T', 'O', 'P', ' ' };
    private final static byte[] m_uidl = { 'U', 'I', 'D', 'L', ' ' };
    private final static byte[] m_user = { 'U', 'S', 'E', 'R', ' ' };
    private final static byte[] m_xauthlist = { 'X', 'A', 'U', 'T', 'H', 'L', 'I', 'S', 'T', ' ' };
    private final static byte[] m_xsender = { 'X', 'S', 'E', 'N', 'D', 'E', 'R', ' ' };
    private final static byte[] m_space = { ' ' };

    ///////////////////////////////////////////////////////////////////////////
    // Command types for POP3.
    ///////////////////////////////////////////////////////////////////////////

    private final static int CONN = 0;
    private final static int DELE = 1;
    private final static int LISTA = 2;
    private final static int LIST = 3;
    private final static int NOOP = 4;
    private final static int PASS = 5;
    private final static int QUIT = 6;
    private final static int RSET = 7;
    private final static int RETR = 8;
    private final static int SENDCOMMANDA = 9;
    private final static int SENDCOMMAND = 10;
    private final static int STAT = 11;
    private final static int TOP = 12;
    private final static int UIDLA = 13;
    private final static int UIDL = 14;
    private final static int USER = 15;
    private final static int XAUTHLISTA = 16;
    private final static int XAUTHLIST = 17;
    private final static int XSENDER = 18;

    private final static Integer CONN_OBJ = new Integer(CONN);
    private final static Integer DELE_OBJ = new Integer(DELE);
    private final static Integer LISTA_OBJ = new Integer(LISTA);
    private final static Integer LIST_OBJ = new Integer(LIST);
    private final static Integer NOOP_OBJ = new Integer(NOOP);
    private final static Integer PASS_OBJ = new Integer(PASS);
    private final static Integer QUIT_OBJ = new Integer(QUIT);
    private final static Integer RSET_OBJ = new Integer(RSET);
    private final static Integer RETR_OBJ = new Integer(RETR);
    private final static Integer SENDCOMMANDA_OBJ = new Integer(SENDCOMMANDA);
    private final static Integer SENDCOMMAND_OBJ = new Integer(SENDCOMMAND);
    private final static Integer STAT_OBJ = new Integer(STAT);
    private final static Integer TOP_OBJ = new Integer(TOP);
    private final static Integer UIDLA_OBJ = new Integer(UIDLA);
    private final static Integer UIDL_OBJ = new Integer(UIDL);
    private final static Integer USER_OBJ = new Integer(USER);
    private final static Integer XAUTHLISTA_OBJ = new Integer(XAUTHLISTA);
    private final static Integer XAUTHLIST_OBJ = new Integer(XAUTHLIST);
    private final static Integer XSENDER_OBJ = new Integer(XSENDER);

    ///////////////////////////////////////////////////////////////////////////
    // Error messages for POP3.
    ///////////////////////////////////////////////////////////////////////////

    private final static String errProcessResponses = new String("Must call processResponses()");
    private final static String errParse = new String("Error while parsing response");

    /**
    *Constructor for POP3Client that takes a POP3Sink as a parameter.
	*@param in_sink Response sink that implements the IPOP3Sink interface.
	*@see #IPOP3Sink
    */
    public POP3Client( IPOP3Sink in_sink )
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
        m_itoaBuffer = new byte[m_maxReplyLine];
        m_tokenBuffer = new byte[m_maxReplyLine];
        m_messageData = new byte[m_minChunkSize];
        m_responseLine = new StringBuffer( m_maxReplyLine );
        m_responseMessage = new StringBuffer( m_maxReplyLine );

        ///////////////////////////////////////////////////////////////////////
        // Variables that get initialized in the constructor and on every new
        // connection.
        ///////////////////////////////////////////////////////////////////////

        m_multiLineState = false;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Protocol commands
    ///////////////////////////////////////////////////////////////////////////

    /**
    *Connects to the server using the default port.
    *<p>To specify the connection port, use the
    *form of connect that takes a port number.
	*@param in_server Name of the host server to connect to.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#connect
    *@see IPOP3Sink#error
    *@see #quit
    *@see #connect
    */
    public synchronized void connect( String in_server ) throws IOException
    {
        connect(  in_server, m_defaultPort );
    }

    /**
    *Connects to the server using the specified port.
    *<p>To use the default connection port, use the
    *form of connect that takes only the server name.
	*@param in_server Name of the host server to connect to.
	*@param in_port Number of the port to connect to.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#connect
    *@see IPOP3Sink#error
    *@see #quit
    *@see #connect
    */
    public synchronized void connect( String in_server, int in_port ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_server == null || in_port < 0 )
        {
            throw new IllegalArgumentException();
        }

        m_multiLineState = false;

        m_pendingList.removeAllElements();

        m_io.connect( in_server, in_port, m_messageData.length, 1024 );

        m_io.setTimeout( m_timeout );

        m_pendingList.addElement( CONN_OBJ );

        m_mustProcess = true;
    }

    /**
    *Deletes a message.
    *Marks a message for deletion on the server;
    *actually deleted when quit() is called.
	*@param in_messageNumber Number of the message to delete.
    *@exception IOException If an IO error occurs.
    *@see IPOP3Sink#delete
    *@see IPOP3Sink#error
    *@see #quit
    */
    public synchronized void delete( int in_messageNumber ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_dele );
        m_io.send( m_itoaBuffer, 0, m_common.itoa(in_messageNumber, m_itoaBuffer), false );

        m_pendingList.addElement( DELE_OBJ );

        m_mustProcess = true;
    }

    /**
    *Closes the socket connection with the server.
    *Can be used to perform a high level "Cancel" while
    *retrieving a message.
    *NOTE: Do not call processResponses() after this method.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#delete
    *@see #quit
    */
    public synchronized void disconnect() throws IOException
    {
        m_io.disconnect();
    }

    /**
    *Lists all messages.
    *POP3 includes two forms of LIST. This form goes through all
    *the messages in the mailbox and generates a list of messages.
    *The other form takes the message number of the message to
    *list as an argument.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#listStart
    *@see IPOP3Sink#list
    *@see IPOP3Sink#listComplete
    *@see IPOP3Sink#error
    *@see #list
    */
    public synchronized void list() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( m_list, false );
        m_pendingList.addElement( LIST_OBJ );
        m_mustProcess = true;
    }

    /**
    *List a message.
    *POP3 includes two forms of LIST.
    *This form takes the message number of the message to list
    *as an argument. The other form goes through all the messages
    *in the mailbox and generates a list of messages.
	*@param in_messageNumber The message number to list.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#listStart
    *@see IPOP3Sink#list
    *@see IPOP3Sink#listComplete
    *@see IPOP3Sink#error
    *@see #list
    */
    public synchronized void list( int in_messageNumber ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_list );
        m_io.send( m_itoaBuffer, 0, m_common.itoa(in_messageNumber, m_itoaBuffer), false );
        m_pendingList.addElement( LISTA_OBJ );
        m_mustProcess = true;
    }

    /**
    *Gets positive server response; does not affect POP3 session.
    *Issues Noop command.
    *<P>Server responds to commands with a "still here" response.
    *Sending the noop method does nothing except force this server response.
    *Can be used to maintain server connection, perhaps being issued
    *at timed intervals to make sure that the server is still active.
    *Not needed by applications that do not need to maintain the connection.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#noop
    *@see IPOP3Sink#error
    */
    public synchronized void noop() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( m_noop, false );
        m_pendingList.addElement( NOOP_OBJ );
        m_mustProcess = true;
    }

    /**
    *Enters a password.
    *Identifies the user password; on success, the POP3
    *session enters the Transaction state.
	*@param in_password User password.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#pass
    *@see IPOP3Sink#error
    *@see #user
    */
    public synchronized void pass( String in_password ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( in_password == null )
        {
            throw new IllegalArgumentException();
        }

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_pass );
        m_io.send( in_password, false );
        m_pendingList.addElement( PASS_OBJ );
        m_mustProcess = true;
    }

    /**
    *Closes the connection with the server and expunges any deleted messages.
    *Ends the session, purging deleted messages and logging the user
    *out from the POP3 server. If issued in Authentication state,
    *server closes connections. If issued in Transaction state,
    *server goes into Update state and deletes marked messages,
    *then quits.
    *@exception IOException If an I/O error occurs.
    *@see IPO3Sink#quit
    *@see IPOP3Sink#error
    *@see #delete
    */
    public synchronized void quit() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( m_quit, false );
        m_pendingList.addElement( QUIT_OBJ );
        m_mustProcess = true;
    }

    /**
    *Resets the state of the server by undeleting any deleted messages.
    *Cancels the current mail transfer and all current processes,
    *discards data, and clears all states.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#reset
    *@see IPOP3Sink#error
    */
    public synchronized void reset() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( m_rset, false );
        m_pendingList.addElement( RSET_OBJ );
        m_mustProcess = true;
    }

    /**
    *Retrieves a message.
	*@param in_messageNumber Number of message to retrieve.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#retrieve
    *@see IPOP3Sink#retrieveComplete
    *@see IPOP3Sink#retrieveStart
    *@see IPOP3Sink#error
    */
    public synchronized void retrieve( int in_messageNumber ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_retr );
        m_io.send(m_itoaBuffer, 0, m_common.itoa(in_messageNumber, m_itoaBuffer), false);
        m_pendingList.addElement( RETR_OBJ );
        m_currentMessageNumber = in_messageNumber;
        m_mustProcess = true;
    }

    /**
    *Sends an unsupported command to the server.
    *Sends commands that are not supported by the Messaging Access SDK
    *implementation of POP3.
    *NOTE: This method is primarily intended to support extensions
    *to the protocol to meet client application needs.
	*@param in_command Raw command to send to the server.
	*@param in_multiLine Identifies whether response is multiline or not.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#sendCommand
    *@see IPOP3Sink#sendCommandStart
    *@see IPOP3Sink#sendCommandComplete
    *@see IPOP3Sink#error
    */
    public synchronized void sendCommand( String in_command, boolean in_multiLine ) throws IOException
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
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( in_command, false );

        if ( in_multiLine )
        {
            m_pendingList.addElement( SENDCOMMAND_OBJ );
        }
        else
        {
            m_pendingList.addElement( SENDCOMMANDA_OBJ );
        }

        m_mustProcess = true;
    }

    /**
    *Gets the status of the mail drop.
    *Returns the number of messages and octet size of the mail drop.
    *Always returns message octet count.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#stat
    *@see IPOP3Sink#error
    */
    public synchronized void stat() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( m_stat, false );
        m_pendingList.addElement( STAT_OBJ );
        m_mustProcess = true;
    }

    /**
    *Retrieves headers plus the specified number of lines from the body.
	*@param in_messageNumber Number of message to retrieve.
	*@param in_lines Number of lines of the message body to return.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#topStart
    *@see IPOP3Sink#top
    *@see IPOP3Sink#topComplete
    *@see IPOP3Sink#error
    */
    public synchronized void top( int in_messageNumber, int in_lines ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_top );
        m_io.write( m_itoaBuffer, 0, m_common.itoa(in_messageNumber, m_itoaBuffer) );
        m_io.write( m_space );
        m_io.send( m_itoaBuffer, 0, m_common.itoa(in_lines, m_itoaBuffer), false );
        m_pendingList.addElement( TOP_OBJ );
        m_currentMessageNumber = in_messageNumber;
        m_mustProcess = true;
    }

    /**
    *Lists all messages with their unique-ids.
    *POP3 includes two forms of uidList(). This form goes through all
    *the messages in the mailbox and generates a list of messages
    *with their unique ids.
    *The other form takes the message number of the message to
    *list as an argument.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#uidList
    *@see IPOP3Sink#uidListStart
    *@see IPOP3Sink#uidListComplete
    *@see IPOP3Sink#error
    */
    public synchronized void uidList() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( m_uidl, false );

        m_pendingList.addElement( UIDL_OBJ );

        m_mustProcess = true;
    }

    /**
    *Lists a message with its unique id.
    *POP3 includes two forms of uidList(). This form takes the message number of the message to
    *list with its unique id as an argument.
    *The other form goes through all
    *the messages in the mailbox and generates a list of messages.
	*@param in_messageNumber Number of message for which to return a unique id. .
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#uidList
    *@see IPOP3Sink#uidListStart
    *@see IPOP3Sink#uidListComplete
    *@see IPOP3Sink#error
    */
    public synchronized void uidList( int in_messageNumber ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_uidl );
        m_io.send( m_itoaBuffer, 0, m_common.itoa(in_messageNumber, m_itoaBuffer), false );
        m_pendingList.addElement( UIDLA_OBJ );
        m_mustProcess = true;
    }

    /**
    *Enters a username.
    *Identifies the user or mail drop by name to the server;
    *the server returns a known or unknown response.
	*@param in_user Name of the user's maildrop.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#user
    *@see IPOP3Sink#error
    *@see #pass
    */
    public synchronized void user( String in_user ) throws IOException
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
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_user );
        m_io.send( in_user, false );
        m_pendingList.addElement( USER_OBJ );
        m_mustProcess = true;
    }

    /**
    *Returns a list of authenticated users.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#xAuthList
    *@see IPOP3Sink#xAuthListStart
    *@see IPOP3Sink#xAuthListComplete
    *@see IPOP3Sink#error
    */
    public synchronized void xAuthList() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.send( m_xauthlist, false );
        m_pendingList.addElement( XAUTHLIST_OBJ );
        m_mustProcess = true;
   }

    /**
    *Returns a list of authenticated users for a specified message.
    *@param in_messageNumber The message number.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#xAuthList
    *@see IPOP3Sink#xAuthListStart
    *@see IPOP3Sink#xAuthListComplete
    *@see IPOP3Sink#error
    */
    public synchronized void xAuthList( int in_messageNumber ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_xauthlist );
        m_io.send(m_itoaBuffer, 0, m_common.itoa(in_messageNumber, m_itoaBuffer), false);
        m_pendingList.addElement( XAUTHLISTA_OBJ );
        m_mustProcess = true;
    }

    /**
    *Gets the email address of the sender of the specified message.
    *Gets the email address of the sender of the specified message.
    *Sends the XSENDER [arg] POP3 protocol command.
	*@param in_messageNumber Number of the message.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#xSender
    *@see IPOP3Sink#error
    */
    public synchronized void xSender( int in_messageNumber ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Error checking before proceeding with command.
        ///////////////////////////////////////////////////////////////////////

        if ( m_mustProcess )
        {
            throw new POP3Exception(errProcessResponses);
        }

        m_io.write( m_xsender );
        m_io.send(m_itoaBuffer, 0, m_common.itoa(in_messageNumber, m_itoaBuffer), false);
        m_pendingList.addElement( XSENDER_OBJ );
        m_mustProcess = true;
    }

    /**
    *Reads in responses from the server and invokes the appropriate sink methods.
    *Processes the server responses for API commands. Invokes the
    *callback methods provided by the user for all responses that
    *are available at the time of execution.
    *NOTE: If a timeout occurs, the user can continue by calling processResponses() again.
	*@return int The number of responses processed.
    *@exception POP3ServerException If a server response error occurs.
    *@exception InterruptedIOException If a time-out occurs.
    *@exception IOException If an I/O error occurs.
    *@see IPOP3Sink#error
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
                        case CONN:
                            parseConnect();
                            break;
                        case DELE:
                            parseDelete();
                            break;
                        case LISTA:
                            parseListA();
                            break;
                        case LIST:
                            parseList();
                            break;
                        case NOOP:
                            parseNoop();
                            break;
                        case PASS:
                            parsePass();
                            break;
                        case QUIT:
                            parseQuit();
                            m_io.disconnect();
                            break;
                        case RSET:
                            parseRset();
                            break;
                        case RETR:
                            parseRetr();
                            break;
                        case SENDCOMMANDA:
                            parseSendCommandA();
                            break;
                        case SENDCOMMAND:
                            parseSendCommand();
                            break;
                        case STAT:
                            parseStat();
                            break;
                        case TOP:
                            parseTop();
                            break;
                        case UIDLA:
                            parseUidlA();
                            break;
                        case UIDL:
                            parseUidl();
                            break;
                        case USER:
                            parseUser();
                            break;
                        case XAUTHLISTA:
                            parseXAuthListA();
                            break;
                        case XAUTHLIST:
                            parseXAuthList();
                            break;
                        case XSENDER:
                            parseXSender();
                            break;
                    }

                    m_pendingList.removeElementAt( 0 );
                }

                m_mustProcess = false;
            }
            catch( POP3ServerException e )
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
    *Sets the size of the data chunks that are passed to the response sink
    *for the retrieve() callback.  The chunk size must be >= 1024.
	*@param in_chunkSize Size of chunk used for sending messages using
	*the send method. Minimum chunk size: 1024. Default = 1K.
    *@exception IOException If an I/O error occurs.
    *@see #retrieve
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
    *constructor or any set afterwards.
    *Server reponses are routed to this new response sink.
	*@param in_responseSink New POP3 response sink to use.
    *@exception IOException If an I/O error occurs.
    */
    public synchronized void setResponseSink( IPOP3Sink in_responseSink  )
    {
        synchronized(m_responseMessage)
        {
            if ( in_responseSink == null )
            {
                throw new NullPointerException();
            }

            m_notifySink = in_responseSink;
        }
    }

    /**
    *Sets the amount of time allowed to wait before
    *returning control to the user.
	*@param in_timeout Time-out period to set in milliseconds.
	*Values, in milliseconds:
	*<ul>
	*<li>0 = infinite time-out (default)
	*<li>-1 = no waiting
	*<li>> 0 = length of time-out period
	</ul>
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

    ///////////////////////////////////////////////////////////////////////////
    // Internal functions
    ///////////////////////////////////////////////////////////////////////////

    /**
    *Separates the response code from the message part of the response.
    */
    private final boolean setStatusInfo(
                        byte[] in_response,
                        int in_byteCount,
                        StringBuffer out_responseMessage ) throws IOException
    {
        boolean l_responseStatus;

        if ( in_byteCount < 3 )
        {
            throw new POP3Exception(errParse);
        }

        if ( '+' == (char)in_response[0] &&
                'O' == (char)in_response[1] &&
                'K' == (char)in_response[2] )
        {
            l_responseStatus = true;
        }
        else
        {
            l_responseStatus = false;
        }

        if ( in_byteCount > 4 )
        {
            m_responseMessage.setLength( 0 );

            for ( int count = 4; count < in_byteCount; count++ )
            {
                m_responseMessage.append( (char)m_response[count] );
            }
        }
        else
        {
            m_responseMessage.setLength( 0 );
        }

        return l_responseStatus;
    }

    /**
    *Copies the next token into the specified byte array.
    */
    private final int getNextToken(
                        byte[] in_parseLine,
                        int in_offset,
                        byte[] out_token ) throws IOException
    {
        int i;
        int n = 0;

        for ( i = in_offset; Character.isSpaceChar((char)in_parseLine[i]); i++ )
        {
        }

        while ( i < in_parseLine.length &&
                Character.isDigit((char)in_parseLine[i]) )
        {
            out_token[n++] = in_parseLine[i++];
        }

        out_token[n] = '\0';

        return i;
    }

    /**
    *Copies the bytes in a byte array into a StringBuffer.
    */
    private final void toStringBuffer( StringBuffer out_buffer,
                                    byte[] in_buffer,
                                    int offset,
                                    int in_length )
    {
        out_buffer.setLength( in_length );

        for ( int i = 0; i < in_length; i++ )
        {
            out_buffer.setCharAt( i, (char)in_buffer[offset+i] );
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Methods for parsing the responses and invoking sink notifications.
    ///////////////////////////////////////////////////////////////////////////

    private final void parseConnect() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.connect( m_responseMessage );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseDelete() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.delete( m_responseMessage );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseListA() throws IOException
    {
        int l_offset = 3;
        int l_messageNumber;
        int l_octetCount;
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.listStart();

            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_messageNumber = m_common.atoi( m_tokenBuffer );
            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_octetCount = m_common.atoi( m_tokenBuffer );

            m_notifySink.list( l_messageNumber, l_octetCount );
            m_notifySink.listComplete();
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseList() throws IOException
    {
        int l_offset = 0;
        int l_messageNumber;
        int l_octetCount;
        int l_byteCount;
        boolean l_responseStatus;

        if ( m_multiLineState == false )
        {
            l_byteCount = m_io.readLine( m_response );

            if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
            {
                m_notifySink.listStart();
            }
            else
            {
                m_notifySink.error( m_responseMessage );
                return;
            }

            m_multiLineState = true;
        }

        l_byteCount = m_io.readLine( m_response );

        while( ('.' != (char)m_response[0]) || ( l_byteCount != 1 ) )
        {
            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_messageNumber = m_common.atoi( m_tokenBuffer );
            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_octetCount = m_common.atoi( m_tokenBuffer );
            l_offset = 0;

            m_notifySink.list( l_messageNumber, l_octetCount );

            l_byteCount = m_io.readLine( m_response );
        }

        m_multiLineState = false;
        m_notifySink.listComplete();
    }

    private final void parseNoop() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.noop();
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parsePass() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.pass( m_responseMessage );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseQuit() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.quit( m_responseMessage );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseRset() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.reset( m_responseMessage );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseRetr() throws IOException
    {
        int l_nextByte;
        int l_byteCount;
        int l_curByteCount;
        int l_octetCount;
        boolean l_responseStatus;

        if ( m_multiLineState == false )
        {
            l_byteCount = m_io.readLine( m_response );

            if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
            {
                int ret_offset = getNextToken( m_response, 3, m_tokenBuffer );
		if (ret_offset != 3) // there was an octetCount after +OK
		{
                	l_octetCount = m_common.atoi (m_tokenBuffer);
                	m_notifySink.retrieveStart (m_currentMessageNumber, l_octetCount);
		}
		else
		{
                	m_notifySink.retrieveStart (m_currentMessageNumber, 0);
		}
            }
            else
            {
                m_notifySink.error( m_responseMessage );
                return;
            }

            m_multiLineState = true;
            m_messageDataSize = 0;
        }

        m_io.readLine( m_responseLine );
        m_responseLine.append( '\r' );
        m_responseLine.append( '\n' );
        l_byteCount = m_responseLine.length();

        while( ( m_responseLine.charAt(0) != '.') || ( l_byteCount != 3 ) )
        {
            l_curByteCount = 0;

            if ( m_responseLine.charAt(0) == '.' )
            {
                l_curByteCount++;
            }

            while ( l_curByteCount < l_byteCount )
            {
                if ( m_messageDataSize == m_messageData.length )
                {
                    m_notifySink.retrieve( m_messageData );
                    m_messageDataSize = 0;
                }

                m_messageData[m_messageDataSize++] = (byte)m_responseLine.charAt(l_curByteCount++);
            }

            m_io.readLine( m_responseLine );
            m_responseLine.append( '\r' );
            m_responseLine.append( '\n' );
            l_byteCount = m_responseLine.length();
        }

        if ( m_messageDataSize > 0 )
        {
            byte[] l_buffer = new byte[m_messageDataSize];

            for ( int i = 0; i < m_messageDataSize; i++ )
            {
                l_buffer[i] = m_messageData[i];
            }
            m_notifySink.retrieve( l_buffer );
        }

        m_multiLineState = false;
        m_notifySink.retrieveComplete();
    }

    private final void parseSendCommandA() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo( m_response, l_byteCount, m_responseMessage ) )
        {
            m_notifySink.sendCommandStart();
            m_notifySink.sendCommand( m_responseMessage );
            m_notifySink.sendCommandComplete();
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseSendCommand() throws IOException
    {
        int l_byteCount;
        boolean l_responseStatus;

        if ( m_multiLineState == false )
        {
            l_byteCount = m_io.readLine( m_response );

            if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
            {
                m_notifySink.listStart();
            }
            else
            {
                m_notifySink.error( m_responseMessage );
                return;
            }

            m_multiLineState = true;
        }

        l_byteCount = m_io.readLine( m_response );

        while( ('.' != (char)m_response[0]) && ( l_byteCount != 1 ) )
        {
            toStringBuffer( m_responseMessage, m_response, 0, l_byteCount );

            m_notifySink.sendCommand( m_responseMessage );

            m_io.readLine( m_response );
        }

        m_multiLineState = false;
        m_notifySink.sendCommandComplete();
    }

    private final void parseStat() throws IOException
    {
        int l_offset = 3;
        int l_messageCount;
        int l_octetCount;
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
        {
            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_messageCount = m_common.atoi( m_tokenBuffer );
            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_octetCount = m_common.atoi( m_tokenBuffer );

            m_notifySink.stat( l_messageCount, l_octetCount );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseTop() throws IOException
    {
        int l_byteCount;
        boolean l_responseStatus;

        if ( m_multiLineState == false )
        {
            l_byteCount = m_io.readLine( m_response );

            if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
            {
                m_notifySink.topStart( m_currentMessageNumber );
            }
            else
            {
                m_notifySink.error( m_responseMessage );
                return;
            }

            m_multiLineState = true;
        }

        l_byteCount = m_io.readLine( m_response );

        while( ('.' != (char)m_response[0]) && ( l_byteCount != 1 ) )
        {
            if ( '.' != (char)m_response[0] )
            {
                toStringBuffer( m_responseMessage, m_response, 0, l_byteCount );
            }
            else
            {
                toStringBuffer( m_responseMessage, m_response, 1, l_byteCount - 1 );
            }

            m_notifySink.top( m_responseMessage );

            l_byteCount = m_io.readLine( m_response );
        }

        m_multiLineState = false;
        m_notifySink.topComplete();
    }

    private final void parseUidlA() throws IOException
    {
        int l_offset = 3;
        int l_messageNumber;
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
        {
            m_notifySink.uidListStart();

            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_messageNumber = m_common.atoi( m_tokenBuffer );

            getNextToken( m_response, l_offset, m_tokenBuffer );
            toStringBuffer( m_responseMessage, m_tokenBuffer, 0, l_byteCount - l_offset );

            m_notifySink.uidList( l_messageNumber, m_responseMessage );
            m_notifySink.uidListComplete();
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseUidl() throws IOException
    {
        int l_offset = 0;
        int l_byteCount;
        int l_messageNumber;
        boolean l_responseStatus;

        if ( m_multiLineState == false )
        {
            l_byteCount = m_io.readLine( m_response );

            if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
            {
                m_notifySink.uidListStart();
            }
            else
            {
                m_notifySink.error( m_responseMessage );
                return;
            }

            m_multiLineState = true;
        }

        l_byteCount = m_io.readLine( m_response );

        while( ('.' != (char)m_response[0]) && ( l_byteCount != 1 ) )
        {
            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_messageNumber = m_common.atoi( m_tokenBuffer );

            getNextToken( m_response, l_offset, m_tokenBuffer );
            toStringBuffer( m_responseMessage, m_tokenBuffer, 0, l_byteCount - l_offset );
            l_offset = 0;

            m_notifySink.uidList( l_messageNumber, m_responseMessage );

            l_byteCount = m_io.readLine( m_response );
        }

        m_multiLineState = false;
        m_notifySink.uidListComplete();
    }

    private final void parseUser() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
        {
            m_notifySink.user( m_responseMessage );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseXAuthListA() throws IOException
    {
        int l_offset = 3;
        int l_octetCount;
        int l_messageNumber;
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
        {
            m_notifySink.xAuthListStart();

            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_messageNumber = m_common.atoi( m_tokenBuffer );

            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_octetCount = m_common.atoi( m_tokenBuffer );

            getNextToken( m_response, l_offset, m_tokenBuffer );
            toStringBuffer( m_responseMessage, m_tokenBuffer, 0, l_byteCount - l_offset );

            m_notifySink.xAuthList( l_messageNumber, l_octetCount, m_responseMessage );
            m_notifySink.xAuthListComplete();
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }

    private final void parseXAuthList() throws IOException
    {
        int l_offset = 0;
        int l_byteCount;
        int l_octetCount;
        int l_messageNumber;
        boolean l_responseStatus;

        if ( m_multiLineState == false )
        {
            l_byteCount = m_io.readLine( m_response );

            if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
            {
                m_notifySink.xAuthListStart();
            }
            else
            {
                m_notifySink.error( m_responseMessage );
                return;
            }

            m_multiLineState = true;
        }

        l_byteCount = m_io.readLine( m_response );

        while( ('.' != (char)m_response[0]) && ( l_byteCount != 1 ) )
        {
            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_messageNumber = m_common.atoi( m_tokenBuffer );

            l_offset = getNextToken( m_response, l_offset, m_tokenBuffer );
            l_octetCount = m_common.atoi( m_tokenBuffer );

            getNextToken( m_response, l_offset, m_tokenBuffer );
            toStringBuffer( m_responseMessage, m_tokenBuffer, 0, l_byteCount - l_offset );
            l_offset = 0;

            m_notifySink.xAuthList( l_messageNumber, l_octetCount, m_responseMessage );

            l_byteCount = m_io.readLine( m_response );
        }

        m_multiLineState = false;
        m_notifySink.xAuthListComplete();
    }

    private final void parseXSender() throws IOException
    {
        int l_byteCount = m_io.readLine( m_response );

        if ( setStatusInfo(m_response, l_byteCount, m_responseMessage) )
        {
            m_notifySink.xSender( m_responseMessage );
        }
        else
        {
            m_notifySink.error( m_responseMessage );
        }
    }
}
