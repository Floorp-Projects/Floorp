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

package netscape.messaging.imap4;

import java.io.*;
import java.net.*;
import java.util.*;
import java.lang.reflect.*;
import netscape.messaging.IO;

/**
*The IMAP4Client exposes the IMAP4 client level API.
*The client uses this class for communicating with a
*server using the IMAP4 protocol.
*<p>The IMAP4Client class contains API calls that map
*directly to the client commands defined in RFC 2060,
*as well as other API calls that are necessary to establish
*and maintain an IMAP4 session.
*<p>For a list of the IMAP4 RFCs referenced in the
*Messaging Access SDK and their URLs,
*see "Where to Find More Information" in "About This Book."
*/

public class IMAP4Client
{
	/**
	* Creates an IMAP4_Client object.
	* Default constructor for an IMAP4_Client object;
	* takes an IMAP4 response sink as a parameter.
	*@param in_sink Response sink that implements the IIMAP4Sink interface.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public IMAP4Client(IIMAP4Sink in_sink)
	{
	    //Internal objects
		m_IOHandler = new IO();
		m_systemPreferences = new SystemPreferences();
		m_systemPreferences.setIO(m_IOHandler);
        m_pendingCommands = new Vector();
        m_commandVector = new Vector();
        m_firstResponse = true;
        m_tagID = 0;
        m_processBlock = new Integer(1);

		//Parsing variables
	    m_sink = in_sink;
        m_dispatcher = null;
        m_serverResponse = new StringBuffer();
        m_tokens = new StringBuffer[3];
        m_parserArgs = new StringBuffer[1];
		for(int i = 0; i < m_tokens.length; i++)
		{
		    m_tokens[i] = new StringBuffer();
		}
        //Note: The hashtables are static so there is only one set of hashtables
        //for n connections and are defined under data members
    }

	///////////////////////////////////////////////
	//General Commands

	/**
	* Connects to the IMAP server in_IMAPHost using the default port (143).
	*<p>To specify the connection port, use the
    *form of connect that takes a port number.
	* @param in_IMAPHost Name of the host server to connect to.
	* @return Boolean true if successfully connected, false otherwise.
    * @exception IOException If an I/O error occurs.
    * @see #connect
    * @see #disconnect
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
    * @see IIMAP4Sink#ok
	*/
	public synchronized boolean connect(String in_IMAPHost)
	    throws IOException
	{
    	return connect(in_IMAPHost, m_systemPreferences.getPort());
	}

	/**
	* Connects to the IMAP server in_IMAPHost through the specified port.
    *<p>To use the default connection port, use the
    *form of connect that takes only the server name.
	* @param in_IMAPHost Name of the host server to connect to.
	* @param in_portNumber Number of the port to connect to.
	* @return Boolean value: true if successfully connected, false otherwise.
    * @exception IOException If an I/O error occurs.
    * @see #disconnect
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
    * @see IIMAP4Sink#ok
	*/
	public synchronized boolean connect(String in_IMAPHost, int in_portNumber)
	    throws IOException
    {
        //Check parameters
        if(in_IMAPHost == null)
        {
            throw new IllegalArgumentException();
        }

		//Establish the IMAP connection
		m_IOHandler.connect(in_IMAPHost, in_portNumber);
		m_IOHandler.setTimeout(m_systemPreferences.getTimeout());
        m_dispatcher = new Dispatcher(m_IOHandler, m_sink, m_pendingCommands, m_systemPreferences);
        m_pendingCommands.addElement(IGlobals.connect);
        m_firstResponse = true;
        m_tagID = 0;

    	return true;
    }


	/**
	* Closes the socket connection with the server.
	* Can be used to perform a high level "Cancel" while retrieving a message.
	* <P>NOTE: Do not call processResponses() after this method.
    * @exception IOException If an I/O error occurs.
    * @see #connect
    * @see IIMAP4Sink#bye
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized void disconnect() throws IOException
	{
	    m_IOHandler.disconnect();
	    m_pendingCommands.removeAllElements();
        m_tagID = 0;
	}

	/**
    * Processes the server responses for all API commands executed prior to
    * the invocation of this command.
    * Calls the appropriate functions on the response
    * sink to relay the results back to the user.
    * Checks for responses that are buffered at the socket.
    * It then reads, parses, and pushes the data through the IIMAP4Sink.
    * NOTE: Synchronized internally.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public void processResponses() throws IOException
	{
	    synchronized (m_processBlock)
	    {
    		String l_responseType = null;
    		Method l_responseParser = null;
    	    char l_tokenChar;

    	    //Parse server response and route data through the appropriate
    	    //methods in the response sink
    		while(!m_pendingCommands.isEmpty())
    		{
                 m_IOHandler.readLine(m_serverResponse);

                //Special case for connect
                if(m_firstResponse)
                {
                    m_pendingCommands.removeElement(IGlobals.connect);
                }

                //Initialize the tokens array
                for(int i = 0; i < m_tokens.length; i++)
                {
                    m_tokens[i].setLength(0);
                }

    			//Tokenize the server response
    			for(int i = 0, j = 0; i < m_serverResponse.length(); i++)
    			{
                    l_tokenChar = m_serverResponse.charAt(i);
                    if(l_tokenChar == ' ')
                    {
                        j++;
                        if(j >= m_tokens.length)
                        {
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    }
                    m_tokens[j].append(l_tokenChar);
                }

    		    //Parse the server response and dispatch to appropriate method on sink
                try
                {
        		    l_responseType = resolveType(m_tokens);
        		    if(l_responseType.equals(IGlobals.BYE))
        		    {
                	    m_pendingCommands.removeAllElements();
                        m_tagID = 0;
        		    }
        		    m_parserArgs[0] = m_serverResponse;
                    l_responseParser = (Method)m_responseMapper.get(l_responseType);
                    l_responseParser.invoke(m_dispatcher, m_parserArgs);
                }
                catch(InvocationTargetException e)
                {
                    Throwable l_exception = e.getTargetException();
                    if(l_exception instanceof IMAP4ServerException)
                    {
                        throw (IMAP4ServerException)l_exception;
                    }
                    else if(l_exception instanceof IMAP4Exception)
                    {
                        throw (IMAP4Exception)l_exception;
                    }
                    l_exception.printStackTrace();
                    throw (new IOException(IGlobals.DispatcherError + " " + l_exception.getMessage()));
                }
                catch(IllegalAccessException e)
                {
                    throw (new IOException(IGlobals.DispatcherError + " " + e.getMessage()));
                }
    		}
        }
	}

	/**
    * Sends the specified command to the IMAP4 server.
    * Allows the developer to extend the default commands supplied
    * through the SDK. Equivalent to writing the value of
    * the in_command parameter directly to the socket.
    * Do not prepend a tag to the command; this is added
    * internally. The server response is pushed back through
    * IIMAP4Sink.rawResponse().
    *<p>Success does not indicate successful completion of the
    * command, just that it has been invoked on the IMAP4 Server.
    * Finding out if the command executed successfully requires
    * using IMAP4Sink.rawResponse() to examine the returned data.
    * @param in_command The command to send to the IMAP server.
	* @return String The tag associated with the command;
	* FALSE otherwise.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#rawResponse
    * @see IIMAP4Sink#rawResponse
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String sendCommand(String in_command) throws IOException
    {
        //Check parameters
        if(in_command == null)
        {
            throw new IllegalArgumentException();
        }

        m_commandVector.addElement(in_command);

        return sendCommand(m_commandVector);
    }

	/**
    * Sends in_command to the IMAP server.
    * Allows the developer to extend the default commands supplied
    * through the SDK. Equivalent to writing the value of
    * the in_command parameter directly to the socket.
    * Do not prepend a tag to the command; this is added
    * internally. The server response is pushed back through
    * IIMAP4Sink.rawResponse().
    *<p>Success does not indicate successful completion of the
    * command, just that it has been invoked on the IMAP4 Server.
    * Finding out if the command executed successfully requires
    * using IMAP4Sink.rawResponse() to examine the returned data.
    * @param in_command The command to send to the IMAP server
	* @return String The tag associated with the command;
	* FALSE otherwise.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#rawResponse
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	protected synchronized String sendCommand(Vector in_command) throws IOException
	{
	    String l_tag = null;

	    //Prepend a tag id and send the command
        m_tagID++;
        l_tag = String.valueOf(m_tagID);

	    in_command.insertElementAt(IGlobals.Space, 0);
	    in_command.insertElementAt(l_tag, 0);
	    in_command.addElement(IGlobals.CarriageNewLine);

	    try
	    {
            m_IOHandler.write(in_command);
            m_IOHandler.flush();

            m_pendingCommands.addElement(l_tag);
            in_command.removeAllElements();
        }
        catch(IOException e)
        {
            in_command.removeAllElements();
            throw e;
        }

        return l_tag;
	}

	/**
	* Retrieves the system preferences.
	* @return SystemPreferences Object representing the system preferences.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
    public synchronized SystemPreferences getSystemPreferences()
    {
        return m_systemPreferences;
    }

	/**
	* Sets a new response sink to route the server responses to.
	* @param in_sink New IMAP4 response sink to use.
	* @exception IOException If an I/O error occurs.
	* @see #getResponseSink
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized void setResponseSink(IIMAP4Sink in_sink) throws IOException
	{
	    synchronized (m_processBlock)
	    {
            //Check parameters
            if(in_sink == null)
            {
                throw new IllegalArgumentException();

            }

    		m_sink = in_sink;
    		m_dispatcher.setResponseSink(m_sink);
    	}
	}

	/**
	* Retrieves the response sink.
	* @return IIMAP4Sink Response sink to which to route server responses.
	* @see #setResponseSink
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized IIMAP4Sink getResponseSink()
	{
		return m_sink;
	}

	////////////////////////////////////////////////////
	//The non-authenticated state

	/**
    * Logs in to server with user name and password.
    * Identifies the client to the server and carries the plain text password
    * authenticating this user. Logs in the user after establishing a connection
	* to the IMAP4 server, which passes the user
	* password over the network in the clear.
	* @param in_user The user name.
	* @param in_password The password of this user.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
	* @see #logout
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
    * @see IIMAP4Sink#bye
	*/
	public synchronized String login(String in_user, String in_password) throws IOException
	{
        //Check parameters
        if((in_user == null) || (in_password == null))
        {
            throw new IllegalArgumentException();

        }

	    m_commandVector.addElement(IGlobals.LOGIN);
	    m_commandVector.addElement(in_user);
	    m_commandVector.addElement(IGlobals.Space);
	    m_commandVector.addElement(in_password);
		return sendCommand(m_commandVector);
	}

	/**
	* Informs the server that the client is done with the connection.
	* Closes the connection and logs the user out.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #logout
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
    * @see IIMAP4Sink#bye
	*/
	public synchronized String logout() throws IOException
	{
	    //Ending session.. close connection
	    m_commandVector.addElement(IGlobals.LOGOUT);

		return sendCommand(m_commandVector);
	}

	/**
	* Gets positive server response; does not affect IMAP4 session.
    * <P>The server responds to commands with a "still here" response.
    * Sending the noop method does nothing except force this server response.
    * Can be used to maintain server connection, perhaps being issued
    * at timed intervals to make sure that the server is still active.
    * <p>Can be used to poll for mail or get new mail. Most servers look for
    * new mail whenever they receive a command. This method does nothing
    * in itself, so it is ideal for triggering this server action.
    * <p>Resets the autologout timer inside the server; may change the
    * number of messages in a mailbox (due to new mail).
    * May change or update the status of messages in mailboxes.
    * Not needed by applications that do something and disconnect at once.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#noop
    * @see IIMAP4Sink#exists
    * @see IIMAP4Sink#expunge
    * @see IIMAP4Sink#recent
    * @see IIMAP4Sink#fetchStart
    * @see IIMAP4Sink#fetchFlags
    * @see IIMAP4Sink#fetchEnd
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String noop() throws IOException
	{
	    m_commandVector.addElement(IGlobals.NOOP);
		return sendCommand(m_commandVector);
	}

	/**
	* Requests a listing of capabilities that the server supports.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #connect
    * @see IIMAP4Sink#capability
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String capability() throws IOException
	{
	    m_commandVector.addElement(IGlobals.CAPABILITY);
		return sendCommand(m_commandVector);
	}

	////////////////////////////////////////////////////
	//The authenticated state

	/**
	* Appends the literal argument as a new message to the end of the
	* specified destination mailbox.
	* @param in_mailbox Mailbox name.
	* @param in_optFlags Parenthesized list of optional flags.
	* @param in_optDateTime Optional date/time string.
	* @param in_inputLiteralStream The message literal.
	* @return String The command object and the (available) server response.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String append(String in_mailbox, String in_optFlags,
        String in_optDateTime, InputStream in_inputLiteralStream)
        throws IOException
	{
        //Check parameters
        if((in_mailbox == null) || (in_inputLiteralStream == null))
        {
            throw new IllegalArgumentException();
        }

        if(m_dispatcher == null)
        {
            throw new IOException(errNotConnected);
        }

	    //Note: This is a command continuation request.  The subsequent response
	    //to the server's request for the message data is handled in Dispatcher.plus
        String l_flags = in_optFlags;
        String l_dateTime = in_optDateTime;
	    int l_totalSize = in_inputLiteralStream.available();

        //Format the optional parameters
        if(l_flags != null)
        {
	        formatOptionalParam(l_flags);
	    }
	    else
	    {
	        l_flags = IGlobals.EmptyString;
	    }

	    if(l_dateTime != null)
	    {
	        formatOptionalParam(l_dateTime);
	    }
	    else
	    {
	        l_dateTime = IGlobals.EmptyString;
	    }

	    //Pass the input stream to the dispatcher
	    m_dispatcher.setAppendStream(in_inputLiteralStream);

	    //Issue the command
        m_commandVector.addElement(IGlobals.APPEND);
        m_commandVector.addElement(IGlobals.Space);
        m_commandVector.addElement(in_mailbox);
        m_commandVector.addElement(IGlobals.Space);
        m_commandVector.addElement(l_flags);
        m_commandVector.addElement(l_dateTime);
        m_commandVector.addElement(IGlobals.LCurly);
        m_commandVector.addElement(String.valueOf(l_totalSize));
        m_commandVector.addElement(IGlobals.RCurly);

        try
        {
		    return sendCommand(m_commandVector);
		}
		catch(IOException e)
		{
            m_commandVector.removeAllElements();
	        throw e;
		}
	}


	/**
	* Creates a new mailbox with the given name.
	* Be careful to use the correct separator character if you want to create
	* a directory on the server side.
	* You cannot and should not CREATE an INBOX.
	* It is always there.
	* @param in_mailbox Name of the mailbox to create.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String create(String in_mailbox) throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.CREATE);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
    * PERMANENTLY removes the mailbox with the given name.
    * You can not and should not DELETE the INBOX. The system needs it.
	* @param in_mailbox Name of the mailbox to delete.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String delete(String in_mailbox) throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.DELETE);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Selects a server mailbox for read-only access.
	* @param in_mailbox Name of the mailbox to examine.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#flags
    * @see IIMAP4Sink#exists
    * @see IIMAP4Sink#recent
    * @see IIMAP4Sink#ok
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String examine(String in_mailbox) throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.EXAMINE);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Lists a subset of all names available
	* to the client.
	* @param in_refName String with reference name (prefix).
	* @param in_mailbox String with mailbox name (suffix).
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#list
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String list(String in_refName, String in_mailbox)
		throws IOException
	{
        //Check parameters
        if((in_refName == null) || (in_mailbox == null))
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.LIST);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_refName);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Return a subset of all names that the user has
	* declared as being "active" or "subscribed".
	* @param in_refName Reference name.
	* @param in_mailbox Mailbox name with possible wildcards.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#lsub
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String lsub(String in_refName, String in_mailbox)
		throws IOException
	{
        //Check parameters
        if((in_refName == null) || (in_mailbox == null))
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.LSUB);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_refName);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Renames a mailbox.
	* @param in_currentMB String with the mailbox's current name.
	* @param in_newMB String with the mailbox's new name.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String rename(String in_currentMB,String in_newMB)
		throws IOException
	{
        //Check parameters
        if((in_currentMB == null) || (in_newMB == null))
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.RENAME);
		m_commandVector.addElement(in_currentMB);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_newMB);

		return sendCommand(m_commandVector);
	}

	/**
	* Selects a mailbox on the server.
	* @param in_mailbox Name of the mailbox.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #examine
    * @see IIMAP4Sink#flags
    * @see IIMAP4Sink#exists
    * @see IIMAP4Sink#recent
    * @see IIMAP4Sink#ok
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String select(String in_mailbox) throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.SELECT);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Requests the status of the specified mailbox.
	* Requests one or more types of status, as specified by the data items parameter.
	* @param in_mailbox Name of the mailbox.
    * @param in_statusData Status data items.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#statusMessages
    * @see IIMAP4Sink#statusRecent
    * @see IIMAP4Sink#statusUidnext
    * @see IIMAP4Sink#statusUidvalidity
    * @see IIMAP4Sink#statusUnseen
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String status(String in_mailbox, String in_statusData)
		throws IOException
	{
        //Check parameters
        if((in_mailbox == null) || (in_statusData == null))
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.STATUS);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_statusData);

		return sendCommand(m_commandVector);
	}

	/**
	* Adds the specified mailbox name to the server's set of "active" or
	* "subscribed" mailboxes.
	* This list is accessed through the lsub method.
    * @param in_mailbox Name of the mailbox to add.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see #unsubscribe
    * @see #lsub
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String subscribe(String in_mailbox)
		throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.SUBSCRIBE);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Removes the specified mailbox name from the server's set of "active" or
	* "subscribed" mailboxes.
    * @param in_mailbox  Name of the mailbox to remove.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see #unsubscribe
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String unsubscribe(String in_mailbox)
		throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();

        }
		m_commandVector.addElement(IGlobals.UNSUBSCRIBE);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	////////////////////////////////////////////////////
	//The selected state


	/**
	* Requests a checkpoint of the currently selected mailbox.
	* Also flushes existing mailbox states to disk.
	* Actual method behavior depends on server implementation.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String check() throws IOException
	{
		m_commandVector.addElement(IGlobals.CHECK);

		return sendCommand(m_commandVector);
	}

	/**
	* Closes the mailbox.
	* Also removes any messages marked with the \Deleted flag.
	* The session moves to the mailbox that contained the closed mailbox.
	* The expunge() method removes messages without closing.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #expunge
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String close() throws IOException
	{
		m_commandVector.addElement(IGlobals.CLOSE);

		return sendCommand(m_commandVector);
	}

	/**
	* Copies the specified message(s) from the currently selected mailbox
	* to the end of the specified destination mailbox.
	* @param in_msgSet Message numbers to copy (for example, "1:2").
	* @param in_mailbox Name of target mailbox.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #uidCopy
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
    public synchronized String copy(String in_msgSet, String in_mailbox)
		throws IOException
	{
        //Check parameters
        if((in_msgSet == null) || (in_mailbox == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.COPY);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_msgSet);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Removes all messages flagged for deletion.
	* Deletes all the messages in the
	* mailbox with the \Deleted flag set. As described in RFC 1730, if there is
	* an EXPUNGEd message, all messages with higher numbers are automatically
	* decremented by one. This happens BEFORE this call returns,
	* so, immediately following this call, you should think about
	* what the state of the message cache might be.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#expunge
    * @see #delete
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String expunge() throws IOException
	{
		m_commandVector.addElement(IGlobals.EXPUNGE);

		return sendCommand(m_commandVector);
	}

	/**
	* Retrieves data specified by the fetch criteria for the given message
	* set and returns the corresponding message sequence numbers.
	* @param in_msgSet  Message set.
	* @param in_fetchCriteria Message data item names.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #uidFetch
    * @see IIMAP4Sink#fetchStart
    * @see IIMAP4Sink#fetchEnd
    * @see IIMAP4Sink#fetchSize
    * @see IIMAP4Sink#fetchData
    * @see IIMAP4Sink#fetchFlags
    * @see IIMAP4Sink#fetchBodyStructure
    * @see IIMAP4Sink#fetchEnvelope
    * @see IIMAP4Sink#fetchInternalDate
    * @see IIMAP4Sink#fetchHeader
    * @see IIMAP4Sink#fetchUid
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String fetch(String in_msgSet, String in_fetchCriteria)
	    throws IOException
	{
        //Check parameters
        if((in_msgSet == null) || (in_fetchCriteria == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.FETCH);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_msgSet);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_fetchCriteria);

		return sendCommand(m_commandVector);
	}

	/**
	* Searches the mailbox for messages that match the given search
	* criteria.
    * @param in_criteria Search criteria consisting of one or more search keys.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#search
    * @see #uidSearch
    * @see IIMAP4Sink#searchStart
    * @see IIMAP4Sink#search
    * @see IIMAP4Sink#searchEnd
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String search(String in_criteria)
		throws IOException
	{
        //Check parameters
        if(in_criteria == null)
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.SEARCH);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_criteria);

		return sendCommand(m_commandVector);
	}

	/**
	* Alters data associated with a message in the mailbox.
    * @param in_msgSet Message set.
    * @param in_dataItem Message data item name.
    * @param in_value Value for the message data item.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #uidStore
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String store(String in_msgSet, String in_dataItem, String in_value)
		throws IOException
	{
        //Check parameters
        if((in_msgSet == null) || (in_dataItem == null) || (in_value == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.STORE);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_msgSet);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_dataItem);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_value);

		return sendCommand(m_commandVector);
	}

	/**
	* Copies messages specified with a unique identifier.
	* Performs a UID COPY.
	* @param in_msgSet Message numbers to copy (for example, "1:2").
	* @param in_mailbox Name of target mailbox.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #copy
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String uidCopy(String in_msgSet, String in_mailbox)
		throws IOException
	{
        //Check parameters
        if((in_msgSet == null) || (in_mailbox == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.UID);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(IGlobals.COPY);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_msgSet);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Fetches messages specified with a unique identifier
	* and retrieve the corresponding unique identifiers.
	* Performs a UID FETCH.
	* @param in_msgSet  The message set
	* @param in_fetchCriteria The message data item names
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see #fetch
    * @see IIMAP4Sink#fetchStart
    * @see IIMAP4Sink#fetchEnd
    * @see IIMAP4Sink#fetchSize
    * @see IIMAP4Sink#fetchData
    * @see IIMAP4Sink#fetchFlags
    * @see IIMAP4Sink#fetchBodyStructure
    * @see IIMAP4Sink#fetchEnvelope
    * @see IIMAP4Sink#fetchInternalDate
    * @see IIMAP4Sink#fetchHeader
    * @see IIMAP4Sink#fetchUid
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String uidFetch(String in_msgSet, String in_fetchCriteria)
		throws IOException
	{
        //Check parameters
        if((in_msgSet == null) || (in_fetchCriteria == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.UID);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(IGlobals.FETCH);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_msgSet);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_fetchCriteria);

		return sendCommand(m_commandVector);
	}

	/**
	*
	* Stores a message specified by a unique identifier.
	* Performs a UID STORE.
    * @param in_set Message set.
    * @param in_dataItem Message data item name.
    * @param in_value Value for the message data item.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #store
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String uidStore(String in_msgSet, String in_dataItem, String in_value)
		throws IOException
	{
        //Check parameters
        if((in_msgSet == null) || (in_dataItem == null) || (in_value == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.UID);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(IGlobals.STORE);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_msgSet);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_dataItem);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_value);

		return sendCommand(m_commandVector);
	}

	/**
	* Searches the mailbox for messages that match the search criteria
	* and retrieves the corresponding unique identifiers.
	* Performs a UID SEARCH.
    * @param in_criteria Search criteria consisting of one or more search keys.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see #search
    * @see IIMAP4Sink#searchStart
    * @see IIMAP4Sink#search
    * @see IIMAP4Sink#searchEnd
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String uidSearch(String in_criteria)
		throws IOException
	{
        //Check parameters
        if(in_criteria == null)
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.UID);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(IGlobals.SEARCH);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_criteria);

		return sendCommand(m_commandVector);
	}

    ////////////////////////////////////////////////////////////////////
    // Extended Commands
    ////////////////////////////////////////////////////////////////////

	/**
	* Retrieves the prefixes of namespaces used by a server for
	* personal mailboxes, other user's mailboxes, and shared mailboxes.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see IIMAP4Sink#nameSpaceStart
    * @see IIMAP4Sink#nameSpacePersonal
    * @see IIMAP4Sink#nameSpaceOtherUsers
    * @see IIMAP4Sink#nameSpaceShared
    * @see IIMAP4Sink#nameSpaceEnd
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String nameSpace() throws IOException
	{
		m_commandVector.addElement(IGlobals.NAMESPACE);

		return sendCommand(m_commandVector);
	}

	/**
	* Changes the access control list on the specified mailbox.
	* The specified identifier is granted permissions specified in the
	* third argument
	* @param in_mailbox Name of the mailbox.
	* @param in_authId Authentication identifier.
	* @param in_accessRight Access rights modification.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see deleteACL
    * @see #getACL
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String setACL(String in_mailbox, String in_authId, String in_accessRight)
	    throws IOException
	{
        //Check parameters
        if((in_mailbox == null) || (in_authId == null) || (in_accessRight == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.SETACL);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_authId);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_accessRight);

		return sendCommand(m_commandVector);
	}

	/**
	* Removes an <identifier, rights> pair for the specified identifier
	* from the access control list for the specified mailbox.
	* @param in_mailbox Name of the mailbox.
	* @param in_authId Authentication identifier.
	* @return String The tag associated with this command
    * @exception IOException If an I/O error occurs.
    * @see setACL
    * @see #getACL
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String deleteACL(String in_mailbox, String in_authId)
	    throws IOException
	{
        //Check parameters
        if((in_mailbox == null) || (in_authId == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.DELETEACL);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_authId);

		return sendCommand(m_commandVector);
	}

	/**
	* Retrieves the access control list for the mailbox in an untagged ACL reply.
	* @param in_mailbox Name of the mailbox.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see deleteACL
    * @see #setACL
    * @see IIMAP4Sink#aclStart
    * @see IIMAP4Sink#aclIdentifierRight
    * @see IIMAP4Sink#aclEnd
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String getACL(String in_mailbox) throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.GETACL);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

	/**
	* Retrieves the access control list for mailbox in an untagged ACL reply.
	* @param in_mailbox Name of the mailbox.
	* @param in_authId Authentication identifier.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #myRights
    * @see IIMAP4Sink#listRightsStart
    * @see IIMAP4Sink#listRightsRequiredRights
    * @see IIMAP4Sink#listRightsOptionalRights
    * @see IIMAP4Sink#listRightsEnd
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String listRights(String in_mailbox, String in_authId) throws IOException
	{
        //Check parameters
        if((in_mailbox == null) || (in_authId == null))
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.LISTRIGHTS);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_authId);

		return sendCommand(m_commandVector);
	}

	/**
	* Retrieves the user’s rights to the specified mailbox.
	* @param in_mailbox Name of the mailbox.
	* @return String The tag associated with this command.
    * @exception IOException If an I/O error occurs.
    * @see #listRights
    * @see IIMAP4Sink#myRights
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	public synchronized String myRights(String in_mailbox) throws IOException
	{
        //Check parameters
        if(in_mailbox == null)
        {
            throw new IllegalArgumentException();
        }
		m_commandVector.addElement(IGlobals.MYRIGHTS);
		m_commandVector.addElement(IGlobals.Space);
		m_commandVector.addElement(in_mailbox);

		return sendCommand(m_commandVector);
	}

    ////////////////////////////////////////////////////////////////////
    // Internal Methods
    ////////////////////////////////////////////////////////////////////

	/**
	* Returns the token that represents the server response type.
	* @param in_tokens The first 3 tokens in the current server response line
	* @return StringBuffer The server response type or null if unknown type
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
	private String resolveType(StringBuffer in_tokens[])
    {
        int l_token1Length = in_tokens[1].length();
        int l_token2Length = in_tokens[2].length();
        if((in_tokens[0].length() < 1) || (l_token1Length < 1))
        {
            return IGlobals.UNKNOWN;
        }

        if('*' == in_tokens[0].charAt(0))
        {
            switch(in_tokens[1].charAt(0))
            {
                case 'C':
                {
                    if( (l_token1Length > 9) &&
                        (in_tokens[1].charAt(1) == 'A') &&
                        (in_tokens[1].charAt(2) == 'P') &&
                        (in_tokens[1].charAt(3) == 'A') &&
                        (in_tokens[1].charAt(4) == 'B') &&
                        (in_tokens[1].charAt(5) == 'I') &&
                        (in_tokens[1].charAt(6) == 'L') &&
                        (in_tokens[1].charAt(7) == 'I') &&
                        (in_tokens[1].charAt(8) == 'T') &&
                        (in_tokens[1].charAt(9) == 'Y'))
                    {
                        return IGlobals.CAPABILITY;
                    }
                    break;
                }
                case 'L':
                {
                    if( (l_token1Length > 9) &&
                        (in_tokens[1].charAt(1) == 'I') &&
                        (in_tokens[1].charAt(2) == 'S') &&
                        (in_tokens[1].charAt(3) == 'T') &&
                        (in_tokens[1].charAt(4) == 'R') &&
                        (in_tokens[1].charAt(5) == 'I') &&
                        (in_tokens[1].charAt(6) == 'G') &&
                        (in_tokens[1].charAt(7) == 'H') &&
                        (in_tokens[1].charAt(8) == 'T') &&
                        (in_tokens[1].charAt(9) == 'S'))
                    {
                        return IGlobals.LISTRIGHTS;
                    }
                    else if( (l_token1Length > 3) &&
                        (in_tokens[1].charAt(1) == 'I') &&
                        (in_tokens[1].charAt(2) == 'S') &&
                        (in_tokens[1].charAt(3) == 'T'))
                    {
                        return IGlobals.LIST;
                    }
                    else if((l_token1Length > 3) &&
                            (in_tokens[1].charAt(1) == 'S') &&
                            (in_tokens[1].charAt(2) == 'U') &&
                            (in_tokens[1].charAt(3) == 'B'))
                    {
                        return IGlobals.LSUB;
                    }
                    break;
                }
                case 'S':
                {
                    if( (l_token1Length > 5) &&
                        (in_tokens[1].charAt(1) == 'T') &&
                        (in_tokens[1].charAt(2) == 'A') &&
                        (in_tokens[1].charAt(3) == 'T') &&
                        (in_tokens[1].charAt(4) == 'U') &&
                        (in_tokens[1].charAt(5) == 'S'))
                    {
                        return IGlobals.STATUS;
                    }
                    else if((l_token1Length > 5) &&
                            (in_tokens[1].charAt(1) == 'E') &&
                            (in_tokens[1].charAt(2) == 'A') &&
                            (in_tokens[1].charAt(3) == 'R') &&
                            (in_tokens[1].charAt(4) == 'C') &&
                            (in_tokens[1].charAt(5) == 'H'))
                    {
                        return IGlobals.SEARCH;
                    }
                    break;
                }
                case 'F':
                {
                    if( (l_token1Length > 4) &&
                        (in_tokens[1].charAt(1) == 'L') &&
                        (in_tokens[1].charAt(2) == 'A') &&
                        (in_tokens[1].charAt(3) == 'G') &&
                        (in_tokens[1].charAt(4) == 'S'))
                    {
                        return IGlobals.FLAGS;
                    }
                    break;
                }
                case 'O':
                {
                    if( (l_token1Length > 1) &&
                        (in_tokens[1].charAt(1) == 'K'))
                    {
                        return IGlobals.OK;
                    }
                    break;
                }
                case 'N':
                {
                    if( (l_token1Length > 1) &&
                        (in_tokens[1].charAt(1) == 'O'))
                    {
                        return IGlobals.NO;
                    }
                    else if( (l_token1Length > 8) &&
                        (in_tokens[1].charAt(1) == 'A') &&
                        (in_tokens[1].charAt(2) == 'M') &&
                        (in_tokens[1].charAt(3) == 'E') &&
                        (in_tokens[1].charAt(4) == 'S') &&
                        (in_tokens[1].charAt(5) == 'P') &&
                        (in_tokens[1].charAt(6) == 'A') &&
                        (in_tokens[1].charAt(7) == 'C') &&
                        (in_tokens[1].charAt(8) == 'E'))
                    {
                        return IGlobals.NAMESPACE;
                    }
                    break;
                }
                case 'P':
                {
                    if( (l_token1Length > 6) &&
                        (in_tokens[1].charAt(1) == 'R') &&
                        (in_tokens[1].charAt(2) == 'E') &&
                        (in_tokens[1].charAt(3) == 'A') &&
                        (in_tokens[1].charAt(4) == 'U') &&
                        (in_tokens[1].charAt(5) == 'T') &&
                        (in_tokens[1].charAt(6) == 'H'))
                    {
                        return IGlobals.PREAUTH;
                    }
                    break;
                }
                case 'B':
                {
                    if( (l_token1Length > 2) &&
                        (in_tokens[1].charAt(1) == 'Y') &&
                        (in_tokens[1].charAt(2) == 'E'))
                    {
                        return IGlobals.BYE;
                    }
                    else if((l_token1Length > 2) &&
                            (in_tokens[1].charAt(1) == 'A') &&
                            (in_tokens[1].charAt(2) == 'D'))
                    {
                        return IGlobals.BAD;
                    }
                    break;
                }
                case 'A':
                {
                    if( (l_token1Length > 2) &&
                        (in_tokens[1].charAt(1) == 'C') &&
                        (in_tokens[1].charAt(2) == 'L'))
                    {
                        return IGlobals.ACL;
                    }
                    break;
                }
                case 'M':
                {
                    if( (l_token1Length > 7) &&
                        (in_tokens[1].charAt(1) == 'Y') &&
                        (in_tokens[1].charAt(2) == 'R') &&
                        (in_tokens[1].charAt(3) == 'I') &&
                        (in_tokens[1].charAt(4) == 'G') &&
                        (in_tokens[1].charAt(5) == 'H') &&
                        (in_tokens[1].charAt(6) == 'T') &&
                        (in_tokens[1].charAt(7) == 'S'))
                    {
                        return IGlobals.MYRIGHTS;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }

            switch(in_tokens[2].charAt(0))
            {
                case 'R':
                {
                    if( (l_token2Length > 5) &&
                        (in_tokens[2].charAt(1) == 'E') &&
                        (in_tokens[2].charAt(2) == 'C') &&
                        (in_tokens[2].charAt(3) == 'E') &&
                        (in_tokens[2].charAt(4) == 'N') &&
                        (in_tokens[2].charAt(5) == 'T'))
                    {
                        return IGlobals.RECENT;
                    }
                    break;
                }
                case 'E':
                {
                    if( (l_token2Length > 5) &&
                        (in_tokens[2].charAt(1) == 'X') &&
                        (in_tokens[2].charAt(2) == 'I') &&
                        (in_tokens[2].charAt(3) == 'S') &&
                        (in_tokens[2].charAt(4) == 'T') &&
                        (in_tokens[2].charAt(5) == 'S'))
                    {
                        return IGlobals.EXISTS;
                    }
                    else if((l_token2Length > 6) &&
                            (in_tokens[2].charAt(1) == 'X') &&
                            (in_tokens[2].charAt(2) == 'P') &&
                            (in_tokens[2].charAt(3) == 'U') &&
                            (in_tokens[2].charAt(4) == 'N') &&
                            (in_tokens[2].charAt(5) == 'G') &&
                            (in_tokens[2].charAt(6) == 'E'))
                    {
                        return IGlobals.EXPUNGE;
                    }
                    break;
                }
                case 'F':
                {
                    if( (l_token2Length > 4) &&
                        (in_tokens[2].charAt(1) == 'E') &&
                        (in_tokens[2].charAt(2) == 'T') &&
                        (in_tokens[2].charAt(3) == 'C') &&
                        (in_tokens[2].charAt(4) == 'H'))
                    {
                        return IGlobals.FETCH;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        else if('+' == in_tokens[0].charAt(0))
        {
            return IGlobals.PLUS;
        }
        else
        {
            //Check for valid tag
            int l_tag = m_dispatcher.atoi(in_tokens[0], 0, in_tokens[0].length());
            if((l_tag > 0) && (l_tag <= m_tagID))
            {
                switch(in_tokens[1].charAt(0))
                {
                    case 'O':
                    {
                        if( (l_token1Length > 1) &&
                            (in_tokens[1].charAt(1) == 'K'))
                        {
                            return IGlobals.taggedOK;
                        }
                        break;
                    }
                    case 'N':
                    {
                        if( (l_token1Length > 1) &&
                            (in_tokens[1].charAt(1) == 'O'))
                        {
                            return IGlobals.taggedNO;
                        }
                        break;
                    }
                    case 'B':
                    {
                        if( (l_token1Length > 2) &&
                            (in_tokens[1].charAt(1) == 'A') &&
                            (in_tokens[1].charAt(2) == 'D'))
                        {
                            return IGlobals.taggedBAD;
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        }

        return IGlobals.UNKNOWN;

    }


	/**
	* Format optional parameters for command string construction.
	* @param out_optParam The optional parameter.
    * @see IIMAP4Sink#taggedLine
    * @see IIMAP4Sink#error
	*/
    private void formatOptionalParam(String out_optParam)
    {
        //Format the flag string
	    out_optParam.trim();
	    if(out_optParam.length() > 0)
	    {
	        out_optParam += IGlobals.Space;
	    }
    }

	////////////////////////////////////////////////////////////////////////
	//Data members
	////////////////////////////////////////////////////////////////////////

	/**A vector of strings representing the command*/
	private Vector m_commandVector;

    /**The IO object*/
	private IO m_IOHandler;

    /**Flags*/
	private boolean m_firstResponse;

	/**The system preferences*/
	private SystemPreferences m_systemPreferences;

    /** The tag id*/
    private int m_tagID;
    protected Vector m_pendingCommands;

	/** Variables to assist the parsing of the server response */
	private StringBuffer[] m_tokens;
	private StringBuffer m_serverResponse;
	private StringBuffer m_parserArgs[];

	/** Used to synchronize processResponses block*/
	private IIMAP4Sink m_sink;
	private Integer m_processBlock;


    public final static int ENVELOPE_DATE = 0;
    public final static int ENVELOPE_SUBJECT = 1;
    public final static int ENVELOPE_FROM = 2;
    public final static int ENVELOPE_SENDER = 3;
    public final static int ENVELOPE_REPLYTO = 4;
    public final static int ENVELOPE_TO = 5;
    public final static int ENVELOPE_CC = 6;
    public final static int ENVELOPE_BCC = 7;
    public final static int ENVELOPE_INREPLYTO = 8;
    public final static int ENVELOPE_MESSAGEID = 9;

	/** The dispatcher*/
	private Dispatcher m_dispatcher;

    /** String constants*/
    protected final static String errNotConnected = new String("Must connect to server");

	/** Hashtables for mapping of response types to appropriate sink methods*/
	private static Hashtable m_responseMapper;
    private static Class[] m_args = new Class[1];

    static
    {
        try
        {
            //The hash tables
            m_responseMapper = new Hashtable();

            //Initialize arguments
            m_args[0] = StringBuffer.class;

            //Command completed methods
            m_responseMapper.put(IGlobals.taggedOK, Dispatcher.class.getMethod("taggedLine", m_args));
            m_responseMapper.put(IGlobals.taggedBAD, Dispatcher.class.getMethod("error", m_args));
            m_responseMapper.put(IGlobals.taggedNO, Dispatcher.class.getMethod("error", m_args));

            m_responseMapper.put(IGlobals.BAD, Dispatcher.class.getMethod("untaggedError", m_args));
            m_responseMapper.put(IGlobals.NO, Dispatcher.class.getMethod("untaggedError", m_args));

            m_responseMapper.put(IGlobals.OK, Dispatcher.class.getMethod("ok", m_args));
            m_responseMapper.put(IGlobals.CAPABILITY, Dispatcher.class.getMethod("capability", m_args));
            m_responseMapper.put(IGlobals.LIST, Dispatcher.class.getMethod("list", m_args));
            m_responseMapper.put(IGlobals.LSUB, Dispatcher.class.getMethod("lsub", m_args));
            m_responseMapper.put(IGlobals.STATUS, Dispatcher.class.getMethod("status", m_args));
            m_responseMapper.put(IGlobals.SEARCH, Dispatcher.class.getMethod("search", m_args));
            m_responseMapper.put(IGlobals.FLAGS, Dispatcher.class.getMethod("flags", m_args));
            m_responseMapper.put(IGlobals.BYE, Dispatcher.class.getMethod("bye", m_args));
            m_responseMapper.put(IGlobals.RECENT, Dispatcher.class.getMethod("recent", m_args));
            m_responseMapper.put(IGlobals.EXISTS, Dispatcher.class.getMethod("exists", m_args));
            m_responseMapper.put(IGlobals.EXPUNGE, Dispatcher.class.getMethod("expunge", m_args));
            m_responseMapper.put(IGlobals.FETCH, Dispatcher.class.getMethod("fetch", m_args));
            m_responseMapper.put(IGlobals.PLUS, Dispatcher.class.getMethod("plus", m_args));
            m_responseMapper.put(IGlobals.NAMESPACE, Dispatcher.class.getMethod("namespace", m_args));
            m_responseMapper.put(IGlobals.ACL, Dispatcher.class.getMethod("acl", m_args));
            m_responseMapper.put(IGlobals.LISTRIGHTS, Dispatcher.class.getMethod("listrights", m_args));
            m_responseMapper.put(IGlobals.MYRIGHTS, Dispatcher.class.getMethod("myrights", m_args));
            m_responseMapper.put(IGlobals.UNKNOWN, Dispatcher.class.getMethod("rawResponse", m_args));
        }
        catch(NoSuchMethodException e)
        {
            System.out.println("Fatal error: can't load functions " + e.getMessage());
        }

    }
}
