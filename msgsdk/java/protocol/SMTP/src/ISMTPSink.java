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

/**
*ISMTPSink is the interface for the response sink for all SMTP commands.
*<p>The ISMTPSink interface contains callback methods for each client call.
*The client’s processResponses call invokes the appropriate
*interface callback method.
*<p>To utilize the SMTP client object, you must extend this
*interface. As a convenience, the Messaging Access SDK provides the
*SMTPSink class, which implements the ISMTPSink interface.
*You can save a step by extending the SMTPSink class, or
*you can implement your own class based on the ISMTPSink
*interface. The constructor for the SMTPClient class takes an
*ISMTPSink interface as a parameter.
*<p>These methods return standard SMTP Response Codes, as defined in RFC 821.
*See "SMTP Response Codes" in Chapter 2, "Sending Mail with SMTP"
*for more information. For detailed information about SMTP,
*see RFC 821. (For the URL, see "Where to Find More Information" in "About This Book.")
*@author derekt@netscape.com
*@version 1.0
*/

public interface ISMTPSink
{
    /**
    *Notification for the response to the BDAT command.
    *Sends binary data chunks of the specified size to the server.
    *When using the sendCommand method, send data with the data method
    *and not with bdat.
    *Note: bdat is not supported by Messaging Server 4.0. Use data instead.
    *<p>See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#bdat
    * @see #data
    * @see #send
    */
    public void bdat( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the connection to the server.
    *<P>Connects to the server using the default port (25).
    *See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#connect
    * @see #quit
    */
	public void connect( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the DATA command.
    *Prepares to send data to the server. The sendCommand method requires
    *sending data with the data method and not with bdat.
    *For more information, see RFC 821 (URL: go to  SMTP RFCs).
    *See "SMTP Response Codes" in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#data
    * @see #bdat
    * @see #send
    */
	public void data( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the EHLO command.
    *Along with ehloComplete, returns extended server information.
    *Can get extended server information (which can be multiline).
    *See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_serverInfo Extension supported by the server.
    * @see SMTPClient#ehlo
    * @see #ehloComplete
    */
	public void ehlo( int in_responseCode, StringBuffer in_serverInfo );

    /**
    *Notification for the completion of the EHLO command.
    *Along with ehlo, returns extended server information.
    * @see SMTPClient#ehlo
    * @see #ehlo
    */
    public void ehloComplete();

    /**
    *Error notification.
    *Called when an error occurs.
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_errorMessage Text that describes the error.
    * @exception SMTPServerException If a server response error occurs.
    * @see SMTPClient#processResponses
    */
	public void error( int in_responseCode, StringBuffer in_errorMessage ) throws SMTPServerException;

    /**
    *Notification for the response to the EXPN command.
    *Along with expandComplete, gets the email address of the specified user.
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_user User for whom to get an email address.
    * @see SMTPClient#expand
    * @see #expandComplete
    */
	public void expand( int in_responseCode, StringBuffer in_user );

    /**
    *Notification for the completion of the EXPN command.
    *Along with expand, gets the email address of the specified user.
    * @see SMTPClient#expand
    * @see #expand
    */
    public void expandComplete();

    /**
    *Notification for the response to the HELP command.
    *Along with helpComplete, gets help on the specified topic,
    *which can be multiline.
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_help Line of help text.
    * @see SMTPClient#help
    * @see #helpComplete
    */
	public void help( int in_responseCode, StringBuffer in_help );

    /**
    *Notification for the completion of the HELP command.
    *Along with help, gets help on a specified topic.
    * @see SMTPClient#help
    * @see #help
    */
    public void helpComplete();

    /**
    *Notification for the response to the MAIL FROM command.
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#mailFrom
    * @see #rcptTo
    */
	public void mailFrom( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the NOOP command.
    *<P>Server responds to commands with a "still here" response.
    *Sending the noop method does nothing except force this response.
    *Can be used to maintain server connection, perhaps being issued
    *at timed intervals to make sure that the server is still active.
    *<p>Resets the autologout timer inside the server.
    *Not needed by applications that do not need to maintain the connection.
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#noop
    */
	public void noop( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the QUIT command.
    *Ends the session. If the session is in the Authentication state,
    *the server closes the server connection. If the session is in
    *the Transaction state, the server goes into the Update state
    *and deletes any marked messages, and then quits.
    * See "SMTP Response Codes" in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#quit
    */
	public void quit( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the RCPT TO command.
    *Gets the address of the recipient of the message. Called once for each
    *recipient; should follow the SMTP_mailFrom function.
    * See "SMTP Response Codes" in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#rcptTo
    * @see #mailFrom
    */
	public void rcptTo( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the RSET command.
    *Cancels the current mail transfer and all current processes,
    *discards data, and clears all states. Returns to the state that
    *followed the last method that sent the EHLO command.
    *Returns a response code and optional response text.
    * See "SMTP Response Codes" in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#reset
    * @see #ehlo
    */
	public void reset( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to data sent to the server.
    *Returns a response code and optional response text.
    *This method requires using the SMTP_data command to send data,
    *rather than SMTP_bdat.
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Text that describes the 3-digit Response Code.
    * @see SMTPClient#send
    * @see #data
    */
	public void send( int in_responseCode, StringBuffer in_responseMessage );

    /**
    *Notification for the response to the extended method.
    *Along with sendCommandComplete, extends the protocol to
    *meet client application needs. Sends commands that are not supported
    *by the Messaging SDK implementation of SMTP.
    *Can get extended server information, possibly multiline.
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseLine Buffer for the response message.
    * @see SMTPClient#sendCommand
    * @see #sendCommandComplete
    */
	public void sendCommand( int in_responseCode, StringBuffer in_responseLine );

    /**
    *Notification for the completion of the extended command.
    *Along with sendCommand, extends the protocol to meet client application
    *needs.
    * @see SMTPClient#sendCommand
    * @see #sendCommand
    */
    public void sendCommandComplete();

    /**
    *Notification for the response to the VRFY command.
    *Returns a response code and optional response text.
    *For more information, see RFC 821 (URL are listed in SMTP RFCs).
    * See SMTP Response Codes in Chapter 2, "Sending Mail with SMTP."
    * @param in_responseCode 3-digit Response Code, as defined in RFC 821, for the response.
    * @param in_responseMessage Buffer for the response message.
    * @see SMTPClient#verify
    */
	public void verify( int in_responseCode, StringBuffer in_responseMessage  );
}
