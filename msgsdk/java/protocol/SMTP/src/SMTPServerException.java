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

import java.io.*;

/**
*The SMTPServerException class represents a server response
*error in the SMTP implementation of the Messaging Access SDK.
*<p>The SMTPServerException class handles server response errors.
*An SMTPServerException is thrown only from the error
*callback on the response sink. The interface definition for
*ISMTPSink states that an SMTPServerException can be thrown,
*but it is up to the developer to determine whether or not
*the implementation of the error callback will throw this
*exception. As a default, the SMTPSink class throws an exception whenever
*the error callback is called.
*<p>This exception is caused when the server sends an
*error saying that some part of the operation failed or is not
*supported. This can happen even when all relevant code executes
*properly.
*@author derekt@netscape.com
*@version 1.0
*/

public class SMTPServerException extends SMTPException
{
    /**
    *Creates an SMTPServerException object.
    *Default constructor for a SMTPServerException object.
    */
    public SMTPServerException()
    {
    }

    /**
    *Creates an SMTPServerException object that includes a descriptive string.
	*@param msg String that describes the exception.
    */
    public SMTPServerException(String msg)
    {
        super(msg);
    }
}
