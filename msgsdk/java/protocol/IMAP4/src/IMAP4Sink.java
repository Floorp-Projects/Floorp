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
*IMAP4Sink is the default implementation of the response sink for
*all IMAP4 commands.
*<p>To implement the IMAP4 protocol, you must extend the IIMAP4Sink interface.
*The IMAP4Sink class is provided as a convenience. You can save time
*by using this class, or you can derive your own class from the IIMAP4Sink
*interface. The constructor for the IMAP4Client class takes an
*IIMAP4Sink interface as a parameter.
*<p>IMAP4Sink implements all the interfaces in IIMAP4Sink. By default,
*the implementation does nothing, except for the error callback, which
*throws an exception.
* @author alterego@netscape.com
* @version 0.1
*/

public class IMAP4Sink implements IIMAP4Sink
{
    //Issues:
    //  error handling
    //  data dependencies

    /** Creates an IMAP4Sink object.
    *Default constructor for the IMAP4Sink class.
    */
    public IMAP4Sink()
    {}

    //General Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Indicates the end of response(s) for successful commands.
    * @param in_tag The tag associated with the command.
    * @param in_status "OK"
    * @param in_reason Reason for in_status.
    * @see IMAP4Client#sendCommand
    * @see IMAP4Client#capability
    * @see IMAP4Client#noop
    * @see IMAP4Client#login
    * @see IMAP4Client#logout
    * @see IMAP4Client#append
    * @see IMAP4Client#create
    * @see IMAP4Client#delete
    * @see IMAP4Client#examine
    * @see IMAP4Client#list
    * @see IMAP4Client#lsub
    * @see IMAP4Client#rename
    * @see IMAP4Client#select
    * @see IMAP4Client#status
    * @see IMAP4Client#subscribe
    * @see IMAP4Client#unsubscribe
    * @see IMAP4Client#check
    * @see IMAP4Client#close
    * @see IMAP4Client#copy
    * @see IMAP4Client#uidCopy
    * @see IMAP4Client#expunge
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    * @see IMAP4Client#search
    * @see IMAP4Client#uidSearch
    * @see IMAP4Client#store
    * @see IMAP4Client#uidStore
    * @see IMAP4Client#nameSpace
    * @see IMAP4Client#setACL
    * @see IMAP4Client#deleteACL
    * @see IMAP4Client#getACL
    * @see IMAP4Client#myRights
    * @see IMAP4Client#listRights
    */
    public void taggedLine( StringBuffer in_tag, StringBuffer in_status,
                            StringBuffer in_reason)
    {
        return;
    }

    /**
    *Indicates the response for unsuccessful commands (error responses).
    *The two types of in_status are NO and BAD, which may be tagged or
    *untagged.
    *<ul><li>NO response: Indicates an operational error message
    *from the server. If tagged, indicates unsuccessful completion
    *of the associated command. If untagged, indicates a warning,
    *but the command can still complete successfully.
    *<li>BAD response: Indicates an error message for the server.
    *If tagged, reports a protocol-level error in the client's
    *command; the tag indicates the command that caused the error.
    *If untagged, indicates a protocol-level error for which
    *the associated command cannot be determined; can also
    *indicate an internal server failure.
    *</ul>
    * @param in_tag The tag associated with the command.
    * @param in_status Status response showing the status of the command. Values:
    *<ul><li>"NO" If tagged: unsuccessful completion; if untagged: client warning message.
    *<li>"BAD" Protocol-level error; if tagged: client command that contained an error; if untagged: serious or wide-spread problem.
    *</ul>
    * @param in_reason Reason for in_status.
    * @exception IMAP4ServerException If a server response error occurs.
    * @see IMAP4Client#connect
    * @see IMAP4Client#sendCommand
    * @see IMAP4Client#capability
    * @see IMAP4Client#noop
    * @see IMAP4Client#login
    * @see IMAP4Client#logout
    * @see IMAP4Client#append
    * @see IMAP4Client#create
    * @see IMAP4Client#delete
    * @see IMAP4Client#examine
    * @see IMAP4Client#list
    * @see IMAP4Client#lsub
    * @see IMAP4Client#rename
    * @see IMAP4Client#select
    * @see IMAP4Client#status
    * @see IMAP4Client#subscribe
    * @see IMAP4Client#unsubscribe
    * @see IMAP4Client#check
    * @see IMAP4Client#close
    * @see IMAP4Client#copy
    * @see IMAP4Client#uidCopy
    * @see IMAP4Client#expunge
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    * @see IMAP4Client#search
    * @see IMAP4Client#uidSearch
    * @see IMAP4Client#store
    * @see IMAP4Client#uidStore
    * @see IMAP4Client#nameSpace
    * @see IMAP4Client#setACL
    * @see IMAP4Client#deleteACL
    * @see IMAP4Client#getACL
    * @see IMAP4Client#myRights
    * @see IMAP4Client#listRights
    */
    public void error(  StringBuffer in_tag, StringBuffer in_status,
                        StringBuffer in_reason) throws IMAP4ServerException
    {
        return;
    }

    /**
    * An unsolicited OK response.
    * Gets the OK status response to IMAP4 commands.
    * @param in_responseCode Response code (optional).
    * @param in_information Information text message for the user.
    * @see IMAP4Client#connect
    * @see IMAP4Client#examine
    * @see IMAP4Client#select
    */
    public void ok(StringBuffer in_responseCode, StringBuffer in_information)
    {
        return;
    }

    /**
    * Raw (unparsed) data is pushed into here.
    * <P>Note: Matches up with sendCommand; is used as a default method
    * to push information that does not belong anywhere else.
    * @param in_data Raw data.
    * @see #sendCommand
    */
    public void rawResponse(StringBuffer in_data)
    {
        return;
    }


    //Fetch Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Indicates the beginning of a fetch response.
    * @param in_msg Message number or uid.
    * @return Reference object to use to associate all data
    * items for a given response, or an error code.
    * @see IMAP4Client#noop
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public Object fetchStart(int in_msg)
    {
        return null;
    }

    /**
    * Signals that the data for this message has been completely fetched
    * @param in_reference Reference object to use to associate all
    * data items for a given response.
    * @see IMAP4Client#noop
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchEnd(Object in_reference)
    {
        return;
    }

    /**
    * The size of the message. The value of the following data
    * items are routed here: RFC822.SIZE.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_size Size of the data.
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchSize(Object in_reference, int in_size)
    {
        return;
    }

    /**
    * Fetches the data associated with specific data items.
    * The value of the following data items are routed here:
    * BODY[<section>]<<partial>>, BODY.PEEK[<section>]<<partial>>,
    * RFC822.HEADER, RFC822, RFC822.TEXT
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_data Chunk of message data.
    * @param in_bytesRead Number of bytes read so far.
    * @param in_totalBytes Total size of the body segment.
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchData(Object in_reference, byte[] in_data, int in_bytesRead, int in_totalBytes)
    {
        return;
    }

    /**
    * Fetches the value of the flags.
    * The value of the following data items are routed here: FLAGS.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_flags Flags for the object.
    * @see IMAP4Client#noop
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchFlags(Object in_reference, StringBuffer in_flags)
    {
        return;
    }

    /**
    * Fetches the value of the body structure.
    * The value of the BODYSTRUCTURE data item is routed here.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_bodyStructure Body structure.
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchBodyStructure(Object in_reference, StringBuffer in_bodyStructure)
    {
        return;
    }

    /**
    * Fetches the value of the envelope.
    * Note: Use ENVELOPE_* values for quick access to ENVELOPE Data items.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_fieldValue The value of the field.
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchEnvelope(Object in_reference, StringBuffer[] in_fieldValue)
    {
        return;
    }

    /**
    * Fetches the value of the internal date.
    * The value of the INTERNALDATE data item is routed here.
    * @param in_reference A reference object to use to associate all data
    * items for a given response.
    * @param in_internalDate The internal date of the message.
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchInternalDate(Object in_reference, StringBuffer in_internalDate)
    {
        return;
    }

    /**
    * Fetches the value of the header.
    * The value of the following data items are routed here:
    * BODY[HEADER], BODY.PEEK[HEADER], RFC822.HEADER,
    * RFC822, BODY[0], BODY.PEEK[0], BODY[], BODY.PEEK[].
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_field Text representing the header field name.
    * @param in_value Value of the header field.
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchHeader(Object in_reference, StringBuffer in_field, StringBuffer in_value)
    {
        return;
    }

    /**
    * Fetches the value of the unique ID of the message.
    * The value of the UID data item is routed here.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_uid UID of the message.
    * @see IMAP4Client#fetch
    * @see IMAP4Client#uidFetch
    */
    public void fetchUid(Object in_reference, int in_uid)
    {
        return;
    }



    //Lsub Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Lists subscribed or active mailboxes that match the search criteria.
    * @param in_attribute Attributes of the mailbox.
    * @param in_delimeter Hierarchy delimiter.
    * @param in_name Name of the mailbox.
    * @see #list
    * @see IMAP4Client#lsub
    */
    public void lsub(StringBuffer in_attribute, StringBuffer in_delimeter, StringBuffer in_name)
    {
        return;
    }

    //List Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Lists mailboxes that match the search criteria.
    * @param in_attribute Attributes of the mailbox.
    * @param in_delimeter Hierarchy delimiter.
    * @param in_name Name of the mailbox.
    * @see #lsub
    * @see IMAP4Client#list
    */
    public void list(StringBuffer in_attribute, StringBuffer in_delimeter, StringBuffer in_name)
    {
        return;
    }

    //Search Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Indicates the start of the retrieval of messages numbers that
    * match the search criteria.
    * @returns Reference object that associates all data
    * items for a given response or an error code.
    * @see IMAP4Client#search
    * @see IMAP4Client#uidSearch
    */
    public Object searchStart()
    {
        return null;
    }

    /**
    * Gets the messages that match the search criteria.
    * Sends the SEARCH IMAP4 protocol command.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_message Message number.
    * @see IMAP4Client#search
    * @see IMAP4Client#uidSearch
    */
    public void search(Object in_reference, int in_message)
    {
        return;
    }

    /**
    * Indicates the end of the retrieval of message numbers
    * that match the search criteria.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @see #search
    * @see #searchStart
    * @see #fetchBodyStructure
    * @see IMAP4Client#search
    * @see IMAP4Client#uidSearch
    */
    public void searchEnd(Object in_reference)
    {
        return;
    }

    //Status Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Total number of messages.
    * @param in_messages Total number of messages.
    * @see #statusRecent
    * @see #statusUidnext
    * @see IMAP4Client#status
    */
    public void statusMessages(int in_messages)
    {
        return;
    }

    /**
    * Number of messages with the recent flag set.
    * @param in_recent Number of recent messages.
    * @see #statusMessages
    * @see #statusUidnext
    * @see IMAP4Client#status
    */
    public void statusRecent(int in_recent)
    {
        return;
    }

    /**
    * The next uid value to be assigned to a new message in the mailbox.
    * @param in_uidNext The next uid value.
    * @see #statusMessages
    * @see #statusUidvalidity
    * @see IMAP4Client#status
    */
    public void statusUidnext(int in_uidNext)
    {
        return;
    }

    /**
    * The uid validity value of the mailbox.
    * @param in_uidValidity The unique ID of the mailbox.
    * @see #statusMessages,
    * @see #statusUidnext
    * @see IMAP4Client#status
    */
    public void statusUidvalidity(int in_uidValidity)
    {
        return;
    }

    /**
    * The number of messages without the \Seen flag set.
    * @param in_unSeen Number of unseen messages.
    * @see #statusMessages
    * @see #statusUidnext
    * @see IMAP4Client#status
    */
    public void statusUnseen(int in_unSeen)
    {
        return;
    }

    //Capability Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Listing of capabilities supported by the server.
    * Sends the CAPABILITY IMAP4 protocol command.
    * @param in_listing Listing of the capabilities that the server supports.
    * @see IMAP4Client#connect
    * @see IMAP4Client#capability
    */
    public void capability(StringBuffer in_listing)
    {
        return;
    }

    //Exists Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The total number of messages in the selected mailbox.
    * @param in_messages Number of messages in the mailbox.
    * @see IMAP4Client#noop
    * @see IMAP4Client#examine
    * @see IMAP4Client#select
    */
    public void exists(int in_messages)
    {
        return;
    }

    //Expunge Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Expunges the specified message.
    * @param in_message Number of message to expunge.
    * @see IMAP4Client#noop
    * @see IMAP4Client#expunge
    */
    public void expunge(int in_message)
    {
        return;
    }

    //Recent Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The total number of messages with the \Recent flag set.
    * @param in_messages Number of \Recent messages.
    * @see IMAP4Client#noop
    * @see IMAP4Client#examine
    * @see IMAP4Client#select
    */
    public void recent(int in_messages)
    {
        return;
    }

    //Flags Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The flags that are applicable for the selected mailbox.
    * @param in_flags Applicable flags.
    * @see #fetchFlags
    * @see IMAP4Client#examine
    * @see IMAP4Client#select
    */
    public void flags(StringBuffer in_flags)
    {
        return;
    }

    //Bye Response
    //////////////////////////////////////////////////////////////////////

    /**
    * The reason the connection was closed.
    * @param in_reason Reason for closing the connection.
    * @see IMAP4Client#disconnect
    * @see IMAP4Client#logout
    */
    public void bye(StringBuffer in_reason)
    {
        return;
    }

    //Namespace Response
    //////////////////////////////////////////////////////////////////////

    /**
    * Indicates the beginning of a namespace response.
	* Lets the client get the prefixes of namespaces used by a server
	* for personal mailboxes, other user's mailboxes, and shared objects.
    * @return Reference object to use to associate all data
    * items for a given response or an error code.
    * @see #nameSpaceEnd
    * @see #nameSpaceOtherUsers
    * @see #nameSpacePersonal
    * @see #nameSpaceShared
    * @see IMAP4Client#nameSpace
    */
    public Object nameSpaceStart()
    {
        return null;
    }

    /**
    * The personal namespace.
    * @param in_reference Reference object to use to associate all data
    * items for a given response or an error code.
    * @param in_personal The personal namespace.
    * @see #nameSpaceEnd
    * @see #nameSpaceOtherUsers
    * @see #nameSpaceStart
    * @see #nameSpaceShared
    * @see IMAP4Client#nameSpace
    */
    public void nameSpacePersonal(Object in_reference, StringBuffer in_personal)
    {
        return;
    }

    /**
    * The other user's namespace.
    * @param in_reference Reference object to use to associate all data
    * items for a given response or an error code.
    * @param in_otherUsers The other user's namespace.
    * @see #nameSpaceEnd
    * @see #nameSpacePersonal
    * @see #nameSpaceStart
    * @see #nameSpaceShared
    * @see IMAP4Client#nameSpace
    */
    public void nameSpaceOtherUsers(Object in_reference, StringBuffer in_otherUsers)
    {
        return;
    }

    /**
    * The shared namespace.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_shared The shared namespace.
    * @see #nameSpaceEnd
    * @see #nameSpaceOtherUsers
    * @see #nameSpacePersonal
    * @see #nameSpaceStart
    * @see IMAP4Client#nameSpace
    */
    public void nameSpaceShared(Object in_reference, StringBuffer in_shared)
    {
        return;
    }

    /**
    * Signals that the data for the particular namespace has been completely fetched.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @see #nameSpaceShared
    * @see #nameSpaceOtherUsers
    * @see #nameSpacePersonal
    * @see #nameSpaceStart
    * @see IMAP4Client#nameSpace
    */
    public void nameSpaceEnd(Object in_reference)
    {
        return;
    }


    //ACL Responses
    //////////////////////////////////////////////////////////////////////

    /**
    * The mailbox name for which the ACL applies.
    * @param in_mailbox Name of the mailbox to which the control list applies.
    * @return Reference object to use to associate all data
    * items for a given response or an error code.
    * @see #aclEnd
    * @see #aclIdentifierRight
    * @see IMAP4Client#getACL
    */
    public Object aclStart(StringBuffer in_mailbox)
    {
        return null;
    }

    /**
    * The identifier rights pairs that apply to mailbox specified in aclStart.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_identifier Identifier for which the entry applies.
    * @param in_rights The client's current set of rights.
    * @see IMAP4Client#getACL
    */
    public void aclIdentifierRight( Object in_reference, StringBuffer in_identifier,
                                    StringBuffer in_rights)
    {
    }

    /**
    * The end of an ACL response.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @see #aclIdentifierRight
    * @see #aclStart
    * @see IMAP4Client#getACL
    */
    public void aclEnd(Object in_reference)
    {
    }


    //LISTRIGHTS Responses
    //////////////////////////////////////////////////////////////////////

    /**
    * The start of a response to the LISTRIGHTS command.
    * @param in_mailbox Mailbox name for which the rights list applies.
    * @param in_identifier Identifier for which the rights list applies.
    * @return Reference object to use to associate all data
    * items for a given response.
    * @see IMAP4Client#listRights
    */
    public Object listRightsStart(StringBuffer in_mailbox, StringBuffer in_identifier)
    {
        return null;
    }

    /**
    * The required rights for the identifier defined in listRightsStart.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_requiredRights The set of rights that the identifier will always
    * be granted in the mailbox.
    * @see #listRightsStart
    * @see IMAP4Client#listRights
    */
    public void listRightsRequiredRights(Object in_reference, StringBuffer in_requiredRights)
    {

    }

    /**
    * The optional rights for the identifier defined in listRightsStart.
    * NOTE: Rights mentioned
    * in the same StringBuffer are tied together. Either all must be granted to the
    * identifier in the mailbox or none may be granted.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @param in_optionalRights Set of rights that the identifier may be granted
    * in the mailbox.
    * @see #listRightsStart
    * @see IMAP4Client#listRights
    */
    public void listRightsOptionalRights(Object in_reference, StringBuffer in_optionalRights)
    {

    }

    /**
    * The end of a LISTRIGHTS response.
    * @param in_reference Reference object to use to associate all data
    * items for a given response.
    * @see #listRightsOptionalRights
    * @see #listRightsRequiredRights
    * @see IMAP4Client#listRights
    */
    public void listRightsEnd(Object in_reference)
    {
    }

    //MYRIGHTS Responses
    //////////////////////////////////////////////////////////////////////

    /**
    * The set of rights that the user has to the mailbox.
    * @param in_mailbox The mailbox name for which these rights apply.
    * @param in_rights The client's current set of rights.
    * @see IMAP4Client#myRights
    */
    public void myRights(StringBuffer in_mailbox, StringBuffer in_rights)
    {
    }

}

