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
import java.io.*;

/**
*The POP3ServerException class represents a server response
*error in the POP3 implementation of the Messaging Access SDK.
*<p>The POP3ServerException class handles server response errors.
*A POP3ServerException is thrown only from the error
*callback on the response sink. The interface definition for
*IPOP3Sink states that a POP3 server exception can be thrown,
*but it is up to the developer to determine whether or not
*the implementation of the error callback will throw this
*exception. As a default, the POP3Sink class throws an exception whenever
*the error callback is called.
*<p>This exception is caused when the server sends an
*error saying that some part of the operation failed or is not
*supported. This can happen even when all relevant code executes
*properly.
*@author Derek Tumulak
*@version 1.0
*/
public class POP3ServerException extends POP3Exception
{

    /**
    *Creates a POP3ServerException object.
    *Default constructor for a POP3ServerException object.
    */
    public POP3ServerException()
    {
        super();
    }

    /**
    *Creates a POP3ServerException object that includes a descriptive string.
	*@param s String that describes the exception.
    */
    public POP3ServerException(String s)
    {
        super(s);
    }
}
