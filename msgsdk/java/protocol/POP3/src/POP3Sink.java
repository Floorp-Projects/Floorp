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

/**
*POP3Sink is the default implementation of the response sink for all
*POP3 commands.
*<p>The POP3Sink object contains callback methods for each client call.
*The client’s processResponses call invokes the appropriate
*object method.
*<p>The Messaging Access SDK provides the POP3Sink class as a convenience
*implementation the POP3Sink interface.
*You can save a step by extending the POP3Sink class, or
*you can implement your own class based on the POP3Sink
*interface. The constructor for the POP3Sink class takes an
*POP3Sink interface as a parameter.
*<p>By default,
*this implementation does nothing, except for the error callback, which
*throws an exception.
*@author derekt@netscape.com
*@version 1.0
*/

public class POP3Sink implements IPOP3Sink
{
    /**
    *Notification for the response to the connection to the server.
    *<P>Connects to the server using the default port.
    *See "POP3 Response Codes" in Chapter 5, "Receiving Mail with POP3."
    * @param in_responseMessage Server response for connect.
    * @see POP3Client#connect
    * @see #quit
    */
    public void connect( StringBuffer in_responseMessage ) {}

    /**
    *Notification for the response to the DELE command.
    *Marks a message for deletion on the server;
    *actually deleted when quit() is called.
    *Sends the DELE [arg] POP3 protocol command.
    * @param in_responseMessage Server response for delete.
    * @see POP3Client#delete
    * @see #quit
    */
    public void delete( StringBuffer in_responseMessage ) {}

    /**
    *Error notification.
    *Called when an error occurs.
    *See "POP3 Response Codes" in Chapter 5, "Receiving Mail with POP3."
    * @param in_responseMessage Server response for an error.
    * @exception POP3ServerException If a server response error occurs.
    * @see POP3Client#processResponses
    */
    public void error( StringBuffer in_responseMessage ) throws POP3ServerException
    {
        throw new POP3ServerException();
    }

    /**
    *Notification for the start of the LIST command.
    *Along with list and listComplete, can be used to list
    *all messages.
    * @see POP3Client#list
    * @see #listComplete
    * @see #list
    */
    public void listStart() {}

    /**
    *Notification for the response to the LIST command.
    *Along with listStart and listStartComplete, can be used to
    *list all messages.
    * @param in_messageNumber Number of message to list.
    * @param in_octetCount Octet count of the message.
    * @see POP3Client#list
    * @see #listStart
    * @see #listComplete
    */
    public void list( int in_messageNumber, int in_octetCount ) {}

    /**
    *Notification for the completion of the LIST command.
    *Along with list and listStartComplete, can be used to list
    *all messages.
    * @see POP3Client#list
    * @see #listStart
    * @see #list
    */
    public void listComplete() {}

    /**
    *Notification for the response to the NOOP command.
    *<P>Server responds to commands with a "still here" response.
    *Sending the noop method does nothing except force this response.
    *Can be used to maintain server connection, perhaps being issued
    *at timed intervals to make sure that the server is still active.
    *<p>Resets the autologout timer inside the server.
    *Not needed by applications that do not need to maintain the connection.
    * @see POP3Client#noop
    */
    public void noop() {}

    /**
    *Notification for the response to the PASS command.
    *Identifies the user password; on success, the POP3
    *session enters the Transaction state.
    * @param in_responseMessage Server response for pass.
    * @see POP3Client#pass
    */
    public void pass( StringBuffer in_responseMessage ) {}

    /**
    *Notification for the response to the QUIT command.
    *Closes the connection with the POP3 server, purges
    *deleted messages, and logs the user out from the server.
    *<p>If issued in Authentication state, server closes
    *connection. If issued in Transaction state, server goes
    *into the Update state and deletes marked messages, then quits.
    * @param in_responseMessage Server response for quit.
    * @see POP3Client#quit
    */
    public void quit( StringBuffer in_responseMessage ) {}

    /**
    *Notification for the response to the RSET command.
    *POP3 can affect server only by deleting/undeleting messages
    *with delete and reset.
    * @param in_responseMessage Server response for reset.
    * @see POP3Client#reset
    * @see #retrieve
    * @see #delete
    */
    // ??? These are not clearly defined in the protocol.
    public void reset( StringBuffer in_responseMessage ) {}

    /**
    *Notification for the start of a message from the RETR command.
    *Along with retrieve and retrieveComplete, can be used to retrieve
    *the message data.
    * @param in_messageNumber Number of message to list.
    * @param in_octetCount Octet count of the message.
    * @see POP3Client#retrieve
    * @see #retrieve
    * @see #retrieveComplete
    */
    public void retrieveStart( int in_messageNumber, int in_octetCount ) {}

    /**
    *Notification for raw message from the RETR command.
    *Along with retrieveStart and retrieveComplete, can be used to
    *retrieve the message data.
    * @param in_messageData Message data to retrieve.
    * @see POP3Client#retrieve
    * @see #retrieveStart
    * @see #retrieveComplete
    */
    public void retrieve( byte[] in_messageData ) {}

    /**
    *Notification for the completion of the RETR command.
    *Along with retrieve and retrieveStart, can be used to
    *retrieve the data in the message.
    * @see POP3Client#retrieve
    * @see #retrieveStart
    * @see #retrieve
    */
    public void retrieveComplete() {}

    /**
    *Notification for the start of the extended method.
    *Along with sendCommand and sendCommandComplete,
    *extends the protocol to meet client application needs.
    *See "POP3 Response Codes" in Chapter 5, "Receiving Mail with POP3."
    * @see POP3Client#sendCommand
    * @see #sendCommand
    * @see #sendCommandComplete
    */
    public void sendCommandStart() {}

    /**
    *Notification for the response to the extended method.
    *Along with sendCommandStart and sendCommandComplete,
    *extends the protocol to meet client application needs.
    *Sends commands that are not supported
    *by the Messaging SDK implementation of POP3. Can get
    *extended server information, possibly multiline.
    *See "POP3 Response Codes" in Chapter 5, "Receiving Mail with POP3."
    * @param in_line Number of lines of the body to read.
    * @see POP3Client#sendCommand, #sendCommandStart, #sendCommandComplete
    */
    public void sendCommand( StringBuffer in_line ) {}

    /**
    *Notification for the completion of the extended command.
    *Along with sendCommandStart and sendCommand,
    *extends the protocol to meet client application needs.
    *See "POP3 Response Codes" in Chapter 5, "Receiving Mail with POP3."
    * @see POP3Client#sendCommand
    * @see #sendCommandStart
    * @see #sendCommand
    */
    public void sendCommandComplete() {}

    /**
    *Notification for the response to the STAT command.
    *Gets the status of the mail drop: returns the number of messages
    *and octet size of the mail drop. Always returns message octet count.
    * @param in_messageCount Number of messages.
    * @param in_octetCount Octet count of the message.
    * @see POP3Client#stat
    */
    public void stat( int in_messageCount, int in_octetCount ) {}

    /**
    *Notification for the start of a message from the TOP command.
    *Along with the top and topComplete methods, retrieves the
    *headers plus the specified number of lines from the message.
    * @param in_messageNumber Number of message.
    * @see POP3Client#top
    * @see #top
    * @see #topComplete
    */
    public void topStart( int in_messageNumber ) {}

    /**
    *Notification for a line of the message from the TOP command.
    *Along with the top and topComplete methods, retrieves the
    *headers plus the specified number of lines from the message.
    *Issues a 'TOP [arg1] [arg2]' command.
    * @param in_line Number of lines of the body to read.
    * @see POP3Client#top
    * @see #topStart
    * @see #topComplete
    */
    public void top( StringBuffer in_line ) {}

    /**
    *Notification for the completion of the TOP command.
    *Along with the top and topStart methods, retrieves the
    *headers plus the specified number of lines from the message.
    * @see POP3Client#top
    * @see #top
    * @see #topStart
    */
    public void topComplete() {}

     /**
    *Notification for the start of the UIDL command.
    *Along with uidList and uidListComplete, goes
    *through all the messages in the mailbox and
    *generates a list line by line. Uses the UIDL POP3 protocol command,
    * @see POP3Client#uidList
    * @see #uidList
    * @see #uidListComplete
    * @see #listStart
    */
    public void uidListStart() {}

    /**
    *Notification for the response to the UIDL command.
    *Along with uidListStart and uidListComplete, goes
    *through all the messages in the mailbox and
    *generates a list line by line.
    * @param in_messageNumber Number of message.
    * @param in_uid Unique ID of message.
    * @see POP3Client#uidList
    * @see #uidListStart
    * @see #uidListComplete
    * @see #list
    */
    public void uidList( int in_messageNumber, StringBuffer in_uid ) {}

    /**
    *Notification for the completion of the UIDL command.
    *Along with uidListStart and uidListComplete, goes
    *through all the messages in the mailbox and
    *generates a list line by line.
    * @see POP3Client#uidList
    * @see #uidList
    * @see #uidListStart
    * @see #ListComplete
    */
    public void uidListComplete() {}

    /**
    *Notification for the response to the USER command.
    * @param in_responseMessage Server response for user.
    * @see POP3Client#user
    */
    public void user( StringBuffer in_responseMessage ) {}

    /**
    *Notification for the start of the XAUTHLIST command.
    *Along with xAuthList and xAuthListComplete, sends the
    *XAUTHLIST POP3 protocol command.
    * @see POP3Client#xAuthList
    * @see #xAuthList
    * @see #xAuthListComplete
    */
    public void xAuthListStart() {}

    /**
    *Notification for the response to the XAUTHLIST command.
    *Along with xAuthListStart and xAuthListComplete, sends the
    *XAUTHLIST POP3 protocol command.
    * @param in_messageNumber Number of message.
    * @param in_octetCount Octet count of the message.
    * @param in_emailAddress Email address.
    * @see POP3Client#xAuthList
    * @see #xAuthListStart
    * @see #xAuthListComplete
    */
    public void xAuthList( int in_messageNumber,
                            int in_octetCount,
                            StringBuffer in_emailAddress ) {}

    /**
    *Notification for the completion of the XAUTHLIST command.
    *Along with xAuthList and xAuthListComplete, sends the
    *XAUTHLIST POP3 protocol command.
    * @see POP3Client#xAuthList
    * @see #xAuthList
    * @see #xAuthListStart
    */
    public void xAuthListComplete() {}

    /**
    *Notification for the response to the XSENDER command.
    *Gets the email address of the sender of the specified message.
    *Sends the XSENDER [arg] POP3 protocol command.
    * @param in_emailAddress Email address.
    * @see POP3Client#xSender
    */
    public void xSender( StringBuffer in_emailAddress ) {}
}
