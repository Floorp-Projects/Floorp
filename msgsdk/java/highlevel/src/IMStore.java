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
 * IMStore
 * @author Prasad Yendluri
 * Internet Message Store
 */
public class IMStore 
{
	/**
         * Default constructor for IMStore
         */
	public IMStore ()
        {
            // Do any initialization
        }

	/**
         * Connects to the message store specified, at the default port based on protocol.
         * @param host name of the host to connect to.
         * @param protocol protocol to use to connect. Valid values are "POP3" and "IMAP4"
	 * @exception IOException If the socket connection can not be opened with the host.
	 * @see	IMStore#disconnect
         *
         */
	public void connect (String host, String protocol) throws IOException
        {
             return;
        }

	/**
         * Logs into message store as the specified user.
         * @param user username to login with
         * @param pass password to login with
	 * @exception IMException If not logged in earlier
	 * @see	IMStore#logout
         */
	public void login (String user, String pass) throws IMException
        {
             return;
        }

	/**
	 * Returns list of sub-folders one level below the specified folder.
         * @param rootFolder Name of the folder to list sub-folders.
	 * @return array of names of sub-folders. null if no sub-folders. 
	 * @exception IMException If rootFolder is not a valid name.
	 * @exception IOException If IO error.
         */
	public String[] listSubFolders (String rootFolder) throws IOException, IMException
        {
		String[] flist = new String[2];
                flist[0] = "junk1";
                flist[2] = "junk2";
                return (flist);
        }

	/**
	 * Returns the summary list for the messages in the specified folder.
         * @param folder Name of the folder. A NULL value assumes the folder INBOX.
	 * @return array of IMMessageSummary objects.
	 * @exception IMException If invalid folder.
	 * @exception IOException If IO error.
         */
	public IMMessageSummary[] listMessages (String folder) throws IOException, IMException
        {
		IMMessageSummary[] ims = new IMMessageSummary[2];
                ims[0] = new IMMessageSummary ();
                ims[1] = new IMMessageSummary ();
                return (ims);
        }

	/**
         * Deletes the specified message from the folder.
         * @param folder Name of the folder. A NULL value assumes the folder INBOX.
         * @param msgnum Message sequence number.
	 * @return false if the message can not be deleted, true otherwise.
         */
	public boolean delete (String folder, int msgnum) 
        {
                return (true);
        }

	/**
         * Moves the specified message from the source folder to destination folder.
         * @param source Name of the folder. A NULL value assumes the folder INBOX.
         * @param destination Name of the folder. A NULL value assumes the folder INBOX.
	 * @return false if the message can not be moved, true otherwise.
         */
	public boolean move (String source, String destination, int msgnum) 
        {
                return (true);
        }

	/**
         * Copies the specified message from the source folder to destination folder.
         * @param source Name of the folder. A NULL value assumes the folder INBOX.
         * @param destination Name of the folder. A NULL value assumes the folder INBOX.
	 * @return false if the message can not be copied, true otherwise.
         */
	public boolean copy (String source, String destination, int msgnum) 
        {
                return (true);
        }


	/**
	 * Retrieves the specified message from the message store.
         * @param folder Name of the folder. A NULL value assumes the folder INBOX.
         * @param msgnum Message sequence number.
	 * @exception IOException If an IO error occurs.
	 * @exception IMException If the message does not exist.
	 * @return Returns an inputstream with the Message in MIME format.
         */
	public InputStream retrieve (String folder, int msgnum) throws IOException, IMException
        {
		DataInputStream ins = new DataInputStream(System.in);
                return (ins);
        }

	/**
	 * Fetches the message as specified by the imapURL parameter. This kind of fetch is useful 
	 * for applications that obtain the URL from other sources (like from a webpage).
         * NOTE: For fetch, it is not necessary to connect / login to the message store first.
         * @param imapURL IMAP URL specifying the fully qualified URL for the message to be fetched.
         * The URL must conform to the format specified in RFC 2192 and must represent a 
	 * unique message. 
	 * An example URL is:
         * imap://foo.com/public.messages;UIDVALIDITY=123456/;UID=789
	 * @exception IOException If an IO error occurs.
	 * @exception IMException If the URL is malformed or does not represent a valid unique message.
	 * @return Returns an inputstream with the Message in MIME format.
         */
	public InputStream fetch (String imapURL) throws IOException, IMException
        {
		DataInputStream ins = new DataInputStream(System.in);
                return (ins);
        }

	/**
	 * Searches the folder for messages that match the search criteria.
         * @param folder Name of the folder. A NULL value assumes the folder INBOX.
         * @param criteria Search criteria as defined by the list of terms.
	 * @exception IOException If an IO error occurs.
	 * @exception IMException If no message matches the criteria.
	 * @return Returns a list of message sequence numbers of matched messages.
         */
	public int [] search (String folder, IMSearchTerm[] criteria) throws IOException, IMException
        {
		int [] msn = new int [2];
		msn [1] = 0;
		msn [2] = 1;
                return (msn);
        }

	/**
         * Logs out from the message store.
	 * @exception IMException If not logged in earlier
	 * @see	IMStore#login
         */
	public void logout () throws IMException
        {
             return;
        }

	/**
         * disconnects from the message store.
	 * @exception IMException If not connected earlier
	 * @see	IMStore#connect
         */
	public void disconnect () throws IMException
        {
             return;
        }
}
