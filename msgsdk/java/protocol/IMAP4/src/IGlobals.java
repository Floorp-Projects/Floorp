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

/**
* IGlobals contains global constants
*/

public interface IGlobals
{
	/** The server response types*/
	final static String STAR = "*";
	final static String PLUS = "+";
	final static String connect = "connect";

    //Any State
	final static String CAPABILITY = "CAPABILITY";
	final static String LOGOUT = "LOGOUT";
	final static String NOOP = "NOOP";

	//Non-authenticated state
	final static String LOGIN = "LOGIN ";

    //Authenticated state
	final static String SELECT = "SELECT";
	final static String EXAMINE = "EXAMINE";
	final static String CREATE = "CREATE ";
	final static String DELETE = "DELETE ";
	final static String RENAME = "RENAME ";
	final static String SUBSCRIBE = "SUBSCRIBE";
	final static String UNSUBSCRIBE = "UNSUBSCRIBE";
	final static String LIST = "LIST";
	final static String LSUB = "LSUB";
	final static String STATUS = "STATUS";
	final static String APPEND = "APPEND";

    //Selected state
	final static String CHECK = "CHECK";
	final static String CLOSE = "CLOSE";
	final static String EXPUNGE = "EXPUNGE";
	final static String SEARCH = "SEARCH";
	final static String FETCH = "FETCH";
	final static String STORE = "STORE";
	final static String COPY = "COPY";
	final static String UID = "UID";

	//Extended IMAP
	final static String NAMESPACE = "NAMESPACE";
	final static String SETACL = "SETACL";
	final static String DELETEACL = "DELETEACL";
	final static String GETACL = "GETACL";
	final static String LISTRIGHTS = "LISTRIGHTS";
	final static String MYRIGHTS = "MYRIGHTS";


	//Server response types
	final static String BYE = "BYE";
	final static String RECENT = "RECENT";
	final static String EXISTS = "EXISTS";
	final static String FLAGS = "FLAGS";
	final static String taggedOK = "taggedOK";
	final static String taggedNO = "taggedNO";
	final static String taggedBAD = "taggedBAD";
	final static String OK = "OK";
	final static String NO = "NO";
	final static String BAD = "BAD";
	final static String PREAUTH = "PREAUTH";
	final static String ACL = "ACL";
	final static String UNTAGGED = "UNTAGGED";

    //Others
	final static String UNKNOWN = "UNKNOWN";


    //Fetch data items
    final static int Uid = 0;
    final static int InternalDate = 1;
    final static int BodyStructure = 2;
    final static int Flags = 3;
    final static int Envelope = 4;
    final static int Message = 5;

    final static int Rfc822Text = 6;
    final static int Rfc822Size = 7;

	final static int Unknown = 8;

    //Status data items
    final static int StatusMessages = 0;
    final static int StatusRecent = 1;
    final static int StatusUidNext = 2;
    final static int StatusUidValidity = 3;
    final static int StatusUnSeen = 4;

    //Control Characters
    final static String NewLine = "\n";
    final static String CarriageNewLine = "\r\n";
    final static String Space = " ";
    final static String LCurly = "{";
    final static String RCurly = "}";
    final static String LBracket = "(";
    final static String RBracket = ")";
    final static String Quote = "\"";
    final static String Colon = ":";
    final static String Equal = "=";
    final static String EmptyString = "";

    //Errors
    final static String DispatcherError = "A fatal error has occured in the dispatcher";
    final static String NoConnection = "Must connect to server";
    final static String UnexpectedServerData = "Unexcepted Server Data";
    final static String ConnectedAlready = "A connection currently exists.";
    final static String ConnectionLost = "The connection has been lost unexpectedly.";
}
