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

/**
* ServerSink is the data sink for all IMAP4 commands
* @author alterego@netscape.com
* @version 0.1
*/

public class ServerSink extends IMAP4Sink
{
    /** Creates an ServerSink object */
    public ServerSink()
    {
        m_uidNumbers = new Vector();
    }


    /**
    * Indicates the end of response(s) for successful commands
    * @param in_tag The tag associated with the command
    * @param in_status "OK"
    * @param in_reason The reason for in_status
    */
    public void taggedLine( StringBuffer in_tag, StringBuffer in_status, 
                            StringBuffer in_reason) 
    {
        System.out.println("TAG: " + in_tag);
        System.out.println("STATUS: " + in_status);
        System.out.println("REASON: " + in_reason);
    }

    /**
    * Indicates the response for unsuccessful commands (error responses)
    * @param in_tag The tag associated with the command
    * @param in_status "NO" or "BAD"
    * @param in_reason The reason for in_status
    */
    public void error(  StringBuffer in_tag, StringBuffer in_status,
                        StringBuffer in_reason) throws IMAP4ServerException
    {
        System.out.println("----------ERROR---------");
        System.out.println("TAG: " + in_tag);
        System.out.println("STATUS: " + in_status);
        System.out.println("REASON: " + in_reason);
        System.out.println("----------ERROR---------");
        
        throw new IMAP4ServerException(in_tag + " " + in_reason);
    }

    /**
    * An unsolicited ok response
    * @param in_responseCode The response code (optional)
    * @param in_information An information message in the form of human-readable text
    */
    public void ok(StringBuffer in_responseCode, StringBuffer in_information)
    {
        System.out.println("CODE: " + in_responseCode);
        System.out.println("Info: " + in_information);
    }

    /**
    * Raw (unparsed) data is pushed into here.  Note: Matches up with sendCommand
    * and will also be used as a default method to push information that doesn't
    * belong anywhere else
    * @param in_tag The tag associated with the command
    * @param in_data The raw data
    */
    public void rawResponse(StringBuffer in_data)
    {
        System.out.println("rawResponse: " + in_data);
    }
    //Fetch Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Indicates the beginning of a fetch response
    * @param in_msg The message number or uid
    * @return Object A reference object that will be used to associate all data
    * items for a given response
    */
    public Object fetchStart(int in_msg)
    {
        System.out.println("Fetch Start.................. " + in_msg);
        StringBuffer l_buffer = new StringBuffer();
        l_buffer.append("Msg Num: " + in_msg);
        return l_buffer;
    }

    /**
    * The data for the particular message has been completely fetched
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    */
    public void fetchEnd(Object in_reference)
    {
        System.out.println("Fetch End.................. ");
    }

    /**
    * The size of the message
    * Data item(s): RFC822.SIZE
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_size The size of the data
    *
    */
    public void fetchSize(Object in_reference, int in_size)
    {
        String l_data = "Size: " + in_size;
        System.out.println(l_data);
        ((StringBuffer)in_reference).append(l_data);
    }

    /**
    * The data associated with the data items listed below
    * Data item(s): BODY[, BODY.PEEK[, RFC822.HEADER, RFC822, RFC822.TEXT
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_data A chunk of message data
    * @param in_bytesRead The number of bytes read so far
    * @param in_totalBytes The total size of the body segment 
    */
    public void fetchData(Object in_reference, byte[] in_data, int in_bytesRead, int in_totalBytes)
    {
        String l_data = new String(in_data);
//        System.out.println("Data: " + l_data);
        ((StringBuffer)in_reference).append(l_data);
    }

    /**
    * The value of the flags
    * Data item(s): FLAGS
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_flags The flags

    */
    public void fetchFlags(Object in_reference, StringBuffer in_flags)
    {
        System.out.println("Flags: " + in_flags);
        ((StringBuffer)in_reference).append("Flags: " + in_flags);
    }

    /**
    * The value of the body structure
    * Data item(s): BODY, BODYSTRUCTURE
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_bodyStructure The body structure
    */
    public void fetchBodyStructure(Object in_reference, StringBuffer in_bodyStructure)
    {
        System.out.println("BodyStructure: " + in_bodyStructure);
        ((StringBuffer)in_reference).append("BodyStructure: " + in_bodyStructure);
    }

    /**
    * The value of the envelope Note: Use ENVELOPE_* values for quick access
    * Data item(s): ENVELOPE
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_fieldValue The value of the field
    */
    public void fetchEnvelope(Object in_reference, StringBuffer[] in_fieldValue)
    {
        for(int i=0; i<in_fieldValue.length; i++)
        {
            System.out.println("Env: " + in_fieldValue[i]);
            ((StringBuffer)in_reference).append("Env: " + in_fieldValue[i]);
        }
    }

    /**
    * The value of the internal date
    * Data item(s): INTERNALDATE
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_internalDate The internal date of the message
    */
    public void fetchInternalDate(Object in_reference, StringBuffer in_internalDate)
    {
        System.out.println("Internal Date: " + in_internalDate);
        ((StringBuffer)in_reference).append("Internal Date: " + in_internalDate);
    }

    /**
    * The value of the header
    * Data item(s): BODY[HEADER, BODY.PEEK[HEADER, RFC822.HEADER,
    * RFC822, BODY[0], BODY.PEEK[0], BODY[], BODY.PEEK[]
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_field The text representing the field name
    * @param in_value The value of the field
    */
    public void fetchHeader(Object in_reference, StringBuffer in_field, StringBuffer in_value)
    {
        System.out.println("Header: " + in_field + ": " + in_value);
        ((StringBuffer)in_reference).append("Header: " + in_field + ": " + in_value);
    }

    /**
    * The value of the uid of the message
    * Data item(s): UID
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_uid The uid of the message
    */
    public void fetchUid(Object in_reference, int in_uid)
    {
        System.out.println("UID: " + in_uid);
        ((StringBuffer)in_reference).append("UID: " + in_uid);
        m_uidNumbers.addElement(new Integer(in_uid));
    }



    //Lsub Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Subscribed or active mailboxes that matched the search criteria
    * @param in_attribute The attributes of the mailbox
    * @param in_delimeter The hierarchy delimeter
    * @param in_name The name of the mailbox
    */
    public void lsub(StringBuffer in_attribute, StringBuffer in_delimeter, StringBuffer in_name)
    {
        System.out.println("LSub attribute: " + in_attribute + " delimeter: " + in_delimeter + " name: " + in_name);
    }

    //List Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Mailboxes that matched the search criteria
    * @param in_attribute The attributes of the mailbox
    * @param in_delimeter The hierarchy delimeter
    * @param in_name The name of the mailbox
    */
    public void list(StringBuffer in_attribute, StringBuffer in_delimeter, StringBuffer in_name)
    {
        System.out.println("List attribute: " + in_attribute + " delimeter: " + in_delimeter + " name: " + in_name);
    }

    //Search Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The start of the retrieval of messages numbers that matched the search criteria
    * @returns Object A reference object
    */
    public Object searchStart()
    {
        System.out.println("Search Start");
        return new StringBuffer();
    }
    
    /**
    * The messages that matched the search criteria
    * @param in_message The messages
    */
    public void search(Object in_reference, int in_message)
    {
        ((StringBuffer)in_reference).append(in_message + " ");
        System.out.println("SEARCH: " + in_message);
    }

    /**
    * The messages that matched the search criteria
    * @param in_messages The messages
    */
    public void searchEnd(Object in_reference)
    {
        System.out.println("Search Done.");
    }

    //Status Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The total number of messages
    * @param in_messages The total number of messages
    */
    public void statusMessages(int in_messages)
    {
        System.out.println("Status Messages: " + in_messages);
    }

    /**
    * The number of messages with the recent flag set
    * @param in_recent The number of recent messages
    */
    public void statusRecent(int in_recent)
    {
        System.out.println("Status recent: " + in_recent);
    }

    /**
    * The next uid value that will be assigned to a new message in the mailbox
    * @param in_uidNext The next uid value
    */
    public void statusUidnext(int in_uidNext)
    {
        System.out.println("Status UIDNext: " + in_uidNext);
    }

    /**
    * The uid validity value of the mailbox
    * @param in_uidValidity The uid
    */
    public void statusUidvalidity(int in_uidValidity)
    {
        System.out.println("Status UIDValidity: " + in_uidValidity);
    }

    /**
    * The number of messages without the \Seen flag set
    * @param in_unSeen The number of unseen messages
    */
    public void statusUnseen(int in_unSeen)
    {
        System.out.println("Status Unseen: " + in_unSeen);
    }

    //Capability Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The capability of the IMAP server
    * @param in_listing A listing of the capabilities that the server supports
    */
    public void capability(StringBuffer in_listing)
    {
        System.out.println("CAPABILITY: " + in_listing);        
    }

    //Exists Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The total number of messages in the selected mailbox
    * @param in_messages The number of messages
    */
    public void exists(int in_messages)
    {
        System.out.println("EXISTS: " + in_messages);
    }

    //Expunge Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The message number of the expunged message
    * @param in_message The message number
    */
    public void expunge(int in_message)
    {
        System.out.println("EXPUNGE: " + in_message);
    }

    //Recent Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The total number of messages with the \Recent flag set
    * @param in_messages The number of messages
    */
    public void recent(int in_messages)
    {
        System.out.println("RECENT: " + in_messages);
    }

    //Flags Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The flags that are applicable for the selected mailbox
    * @param in_flags The applicable flags
    */
    public void flags(StringBuffer in_flags)
    {
        System.out.println("FLAGS: " + in_flags);
    }

    //Bye Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The reason why the connection was closed
    * @param in_reason The reason
    */
    public void bye(StringBuffer in_reason)
    {
        System.out.println("BYE: " + in_reason);
    }

    //Namespace Response
    //////////////////////////////////////////////////////////////////////
    
    /**
    * Indicates the beginning of a namespace response
    * @return Object A reference object that will be used to associate all data
    * items for a given response
    */
    public Object nameSpaceStart()
    {
        System.out.println("NAMESPACE START....");
        return new StringBuffer();        
    }

    /**
    * The personal namespace
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_personal The personal namespace
    */
    public void nameSpacePersonal(Object in_reference, StringBuffer in_personal)
    {
        System.out.println("Personal Namespace: " + in_personal);
        ((StringBuffer)in_reference).append("Personal Namespace: " + in_personal);
    }

    /**
    * The other user's namespace
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_otherUsers The other user's namespace
    */
    public void nameSpaceOtherUsers(Object in_reference, StringBuffer in_otherUsers)
    {
        System.out.println("Other Users Namespace: " + in_otherUsers);
        ((StringBuffer)in_reference).append("Other Users Namespace: " + in_otherUsers);
    }

    /**
    * The shared namespace
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_shared The shared namespace
    */
    public void nameSpaceShared(Object in_reference, StringBuffer in_shared)
    {
        System.out.println("Shared Namespace: " + in_shared);
        ((StringBuffer)in_reference).append("Shared Users Namespace: " + in_shared);
    }

    /**
    * The data for the particular namespace has been completely fetched
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    */
    public void nameSpaceEnd(Object in_reference)
    {
        System.out.println("NAMESPACE END....");
    }
    
    
    //ACL Responses
    //////////////////////////////////////////////////////////////////////
    
    /**
    * The mailbox name for which the ACL applies
    * @return Object A reference object that will be used to associate all data
    * items for a given response
    */
    public Object aclStart(StringBuffer in_mailbox)
    {
        System.out.println("ACL START... " + in_mailbox);
        return new StringBuffer(in_mailbox.toString());
    }
    
    /**
    * The identifier rights pairs that apply to mailbox specified in aclStart
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_identifier The identifier for which the entry applies
    * @param in_rights The set of rights that the identifier has
    */
    public void aclIdentifierRight( Object in_reference, StringBuffer in_identifier,
                                    StringBuffer in_rights)
    {
        System.out.println("ACL: " + in_identifier + " " + in_rights);
        ((StringBuffer)in_reference).append("ACL: " + in_identifier + " " + in_rights);
    }

    /**
    * The end of an ACL response
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    */
    public void aclEnd(Object in_reference)
    {
        System.out.println("ACL END...");
    }

    
    //LISTRIGHTS Responses
    //////////////////////////////////////////////////////////////////////

    /**
    * The start of a response to the LISTRIGHTS command
    * @param in_mailbox The mailbox name for which the rights list applies
    * @param in_identifier The identifier for which the rights list applies
    * @return Object A reference object that will be used to associate all data
    * items for a given response
    */
    public Object listRightsStart(StringBuffer in_mailbox, StringBuffer in_identifier)
    {
        System.out.println("LISTRIGHTS START.... " + in_mailbox + " " + in_identifier);
        return new StringBuffer(in_mailbox.toString() + in_identifier.toString());
    }

    /**
    * The required rights for in_identifier defined in listRightsStart
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_requiredRights The set of rights that the identifier will always
    * be granted in the mailbox
    */
    public void listRightsRequiredRights(Object in_reference, StringBuffer in_requiredRights)
    {
        System.out.println("...REQUIRED RIGHTS : " + in_requiredRights);
        ((StringBuffer)in_reference).append("...REQUIRED RIGHTS : " + in_requiredRights);
    }

    /**
    * The optional rights for in_identifier defined in listRightsStart. NOTE: Rights mentioned
    * in the same StringBuffer are tied together--either all must be granted to the
    * identifier in the mailbox or none may be granted
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    * @param in_optionalRights A set of rights that the identifier may be granted
    * in the mailbox
    */
    public void listRightsOptionalRights(Object in_reference, StringBuffer in_optionalRights)
    {
        System.out.println("...OPTIONAL RIGHTS : " + in_optionalRights);
        ((StringBuffer)in_reference).append("...OPTIONAL RIGHTS : " + in_optionalRights);
    }

    /**
    * The end of an LISTRIGHTS response
    * @param in_reference A reference object that will be used to associate all data
    * items for a given response
    */
    public void listRightsEnd(Object in_reference)
    {
        System.out.println("LISTRIGHTS END....");
    }

    //MYRIGHTS Responses
    //////////////////////////////////////////////////////////////////////

    /**
    * MYRIGHTS response 
    * @param in_mailbox The mailbox name for which these rights apply
    * @param in_rights The set of rights that the client has
    */
    public void myRights(StringBuffer in_mailbox, StringBuffer in_rights)
    {
        System.out.println("MYRIGHTS.... " + in_mailbox + " " + in_rights);
    }
    
    
    //Data members
    //////////////////////////////////////////////////////////////////////
    public Vector m_uidNumbers;

}
