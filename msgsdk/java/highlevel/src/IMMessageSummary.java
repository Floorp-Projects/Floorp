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

package netscape.messaging.convenience;
import java.io.*;
import java.lang.*;
import java.util.*;


/**
 * IMMessageSummary
 * @author Prasad Yendluri
 * Message Summary
 */
public class IMMessageSummary 
{
	/**
         * Name of the folder this message belongs to.
         */
	private String folderName;

	/**
         * Name of the sender of this message.
         */
	private String sender;

	/**
         * Subject of this message.
         */
	private String subject;

	/**
         * Date of this message.
         */
	private String date;

	/**
         * message size.
         */
	private String size;

	/**
         * message sequence number in the folder.
         */
	private int msgnum;

	/**
         * Constructor for the IMMessageSummary
         */
	public IMMessageSummary ()
        {
            // Do any initialization
        }

	/**
         * Returns the Foldername the Message belongs to. 
         */
        public String getFolder () 
        {
                String cts = new String ("INBOX");
                return (cts);
        }

	/**
         * Returns the Sender of the Message. 
         */
        public String getSender () 
        {
                String cts = new String ("Prasad");
                return (cts);
        }

	/**
         * Returns the Subject of the Message. 
         */
        public String getSubject () 
        {
                String cts = new String ("Hello");
                return (cts);
        }

	/**
         * Returns the Date of the Message. 
         */
        public String getDate () 
        {
                String cts = new String ("Date");
                return (cts);
        }

	/**
         * Returns the Size of the Message. 
         */
        public int getSize () 
        {
                return (0);
        }

	/**
         * Returns the sequence number of the Message in the folder. 
         */
        public int getMsgNum () 
        {
                return (1);
        }

}
