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
  
import netscape.messaging.imap4.*;
import java.io.*;
import java.net.*;
import java.util.*;
import java.lang.reflect.*;

/**
* An e-mail client that utilizes the IMAP4rev1 SDK
*
* NOTE: This is only used on the client side and is NOT a part of
*		the SDK.. used for testing purposes only
*
* @author alterego@netscape.com
* @version 0.1
*/

public class Email_Client
{
	public Email_Client()
	{

	}


    /**
    * Test capability, noop, status, select, fetch, close, create, rename,
    * list, delete, copy, store, search ,expunge, status
    */
    public void testRun1(IMAP4Client in_client) throws IOException
    {
        System.out.println("---------------------- testRun1 -----------------------------");

        ///////////////////////////////////////////////////////////////////
        // append messages to inbox
        ///////////////////////////////////////////////////////////////////
        File l_file = null;
        FileInputStream l_inputStream = null;

        l_file = new File("msg2.out");
        l_inputStream = new FileInputStream(l_file);
        in_client.append("inbox", "", "", l_inputStream);
        in_client.processResponses();
        l_inputStream = new FileInputStream(l_file);
        in_client.append("inbox", "", "", l_inputStream);
        in_client.processResponses();

        ///////////////////////////////////////////////////////////////////
        // capability, noop, status.. select fetch close
        ///////////////////////////////////////////////////////////////////
        in_client.capability();
        in_client.processResponses();
        in_client.noop();
        in_client.processResponses();
        in_client.status("inbox", "(MESSAGES RECENT UIDVALIDITY UNSEEN UIDNEXT)");
        in_client.processResponses();
        //in_client.status("inbox", "( )");
        //in_client.processResponses();
        in_client.select("inbox");
        in_client.processResponses();
        in_client.fetch("1:2", "(BODY[])");
        in_client.processResponses();
        in_client.fetch("1", "(RFC822.SIZE BODYSTRUCTURE INTERNALDATE)");
        in_client.processResponses();
        in_client.fetch("1", "(BODY[] INTERNALDATE)");
        in_client.processResponses();
        in_client.fetch("1", "(UID BODY[] INTERNALDATE)");
        in_client.processResponses();
        in_client.fetch("1", "(RFC822.TEXT ENVELOPE)");
        in_client.processResponses();
        in_client.close();
        in_client.processResponses();

        ///////////////////////////////////////////////////////////////////
        // Create, rename, list, delete mailbox
        ///////////////////////////////////////////////////////////////////

        in_client.create("NEWMAILBOX");
        in_client.rename("NEWMAILBOX", "RENAMEDBOX");
        in_client.list("\"\"", "*");
        in_client.delete("RENAMEDBOX");
        in_client.processResponses();

        ///////////////////////////////////////////////////////////////////
        // Select inbox, copy msg, delete msg, search, expunge, close inbox
        ///////////////////////////////////////////////////////////////////

        in_client.select("inbox");
        in_client.copy("1:2", "inbox");
        // The following line of code will set the DELETED flag on all the
        // messages in the currently selected mailbox.  This has been removed
        // so that users who run this test program do not accidentally delete
        // their inbox.
        // derekt 06/03/98
        //in_client.store("1:*", "+FLAGS", "(\\DELETED)");
        in_client.search("DELETED");
        in_client.expunge();
        in_client.close();
        in_client.processResponses();
    }


    /**
    * Test create, list, lsub, delete, status, select, search, subscribe,
    * unsubscribe
    */
    public void testRun2(IMAP4Client in_client) throws IOException
    {
        System.out.println("---------------------- testRun2 -----------------------------");

        ///////////////////////////////////////////////////////////////////
        // Test listing functionality
        ///////////////////////////////////////////////////////////////////

        in_client.create("mbox");
        in_client.list("\"\"", "*");
        in_client.processResponses();
        in_client.subscribe("mbox");
        in_client.lsub("\"\"", "*");
        in_client.processResponses();
        in_client.unsubscribe("mbox");
        in_client.lsub("\"\"", "*");
        in_client.delete("mbox");
        in_client.processResponses();

        ///////////////////////////////////////////////////////////////////
        // Test status, search
        ///////////////////////////////////////////////////////////////////
        in_client.status("inbox", "(MESSAGES UIDNEXT UIDVALIDITY)");
        in_client.processResponses();
        in_client.select("inbox");
        in_client.search("SUBJECT \"afternoon\"");
        in_client.processResponses();
    }

    /**
    * Test append, search, fetch, delete
    */
    public void testRun3(IMAP4Client in_client) throws IOException
    {
        File l_file = null;
        FileInputStream l_inputStream = null;

        System.out.println("---------------------- testRun3 -----------------------------");

        l_file = new File("msg2.out");
        l_inputStream = new FileInputStream(l_file);
        in_client.append("inbox", "", "", l_inputStream);
        in_client.processResponses();
    }


    /**
    * Testing check, examine, pipelineStart, pipelineEnd, sendCommand
    */
    public void testRun4(IMAP4Client in_client) throws IOException
    {
        System.out.println("---------------------- testRun4 -----------------------------");

        in_client.select("inbox");
        in_client.check();
        in_client.close();
        in_client.examine("inbox");
        in_client.close();
        in_client.processResponses();

        in_client.sendCommand("SELECT INBOX");
        in_client.processResponses();
        in_client.sendCommand("CLOSE");
        in_client.processResponses();
    }

    /**
    * Testing uidCopy, uidFetch, uidSearch, uidStore
    */
    public void testRun5(IMAP4Client in_client) throws IOException
    {
        int l_size = 0;
        Integer l_uid = null;
        String l_uidString = null;

        System.out.println("---------------------- testRun5 -----------------------------");

        File l_file = null;
        FileInputStream l_inputStream = null;

        l_file = new File("msg1.out");
        l_inputStream = new FileInputStream(l_file);
        in_client.append("inbox", "", "", l_inputStream);
        in_client.processResponses();
        l_inputStream = new FileInputStream(l_file);
        in_client.append("inbox", "", "", l_inputStream);
        in_client.processResponses();

        in_client.select("inbox");
        in_client.fetch("1:2", "UID");
        in_client.processResponses();
        l_size = m_sink.m_uidNumbers.size();
        for(int i = 0; i < l_size; i++)
        {
            l_uid = (Integer)m_sink.m_uidNumbers.elementAt(i);
            l_uidString = l_uid.toString();
            in_client.uidFetch(l_uidString, "ALL");
            in_client.uidCopy(l_uidString, "inbox");
        }
        in_client.uidSearch("SUBJECT \"afternoon\"");
        in_client.processResponses();
        in_client.uidStore(l_uidString, "+FLAGS", "(\\DELETED)");

        in_client.processResponses();
        in_client.close();
        in_client.processResponses();
        m_sink.m_uidNumbers.removeAllElements();
    }

    /**
    * Testing namespace, setacl, deleteacl, getacl, listrights, myrights
    */
    public void testRun6(IMAP4Client in_client) throws IOException
    {
        System.out.println("---------------------- testRun6 -----------------------------");

        ///////////////////////////////////////////////////////////////////
        // Test namespace extension
        ///////////////////////////////////////////////////////////////////

        in_client.nameSpace();
        in_client.processResponses();

        ///////////////////////////////////////////////////////////////////
        // Test acl extension
        ///////////////////////////////////////////////////////////////////

        in_client.myRights("inbox");
        in_client.processResponses();
        in_client.getACL("inbox");
        in_client.processResponses();
        in_client.deleteACL("inbox", "sama44");
        in_client.processResponses();
        in_client.setACL("inbox", "sama44", "lrswipcda");
        in_client.processResponses();
        in_client.listRights("inbox", "sama44");
        in_client.processResponses();
    }

    //Data members
    ////////////////////////////////////////////////////////////////////
    protected ServerSink m_sink;

	public static void main(String args[])
	{
	    try
	    {
	        Email_Client m_email = new Email_Client();
    	    ServerSink l_sink = new ServerSink();
    	    m_email.m_sink = l_sink;
            IMAP4Client l_client = new IMAP4Client(l_sink);
            SystemPreferences l_preferences = l_client.getSystemPreferences();
            l_preferences.setDebugFlag(true);
            l_preferences.setBlockSize(100000);

            //Connect
            l_client.connect("alterego.mcom.com", 143); //Messaging Server 3.01
//            l_client.connect("sama.mcom.com", 143);   //(Netscape Messaging Server 4.0a0 (built Dec 14 1997)
            l_client.processResponses();

            l_client.noop();
            l_client.processResponses();

            //Login
            l_client.login("imaptest","test");  //Messaging Server 3.01
//            l_client.login("sama44","sama44");    //(Netscape Messaging Server 4.0a0 (built Dec 14 1997)
            l_client.processResponses();

            //Test Runs
            m_email.testRun1(l_client);
            m_email.testRun2(l_client);

            m_email.testRun3(l_client);
            m_email.testRun4(l_client);
            m_email.testRun5(l_client);

            //IMAP extensions
//            m_email.testRun6(l_client);

            //Logout
            l_client.logout();
            l_client.processResponses();
        }
        catch(IMAP4ServerException e)
        {
            System.out.println("IMAP4ServerException: " + e.getMessage());
        }
        catch(IMAP4Exception e)
        {
            System.out.println("IMAP4Exception: " + e.getMessage());
        }
        catch(IOException e)
        {
            System.out.println("IOException: " + e.getMessage());
        }
    }

}
