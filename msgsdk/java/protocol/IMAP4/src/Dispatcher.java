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
import netscape.messaging.IO;

/**
* Dispatcher parses incoming data and dispatches it to the appropriate
* method on the sink
*/

class Dispatcher
{
	/**Create a Dispatcher object*/
	public Dispatcher(  IO in_io, IIMAP4Sink in_sink, Vector in_pendingCommands,
	                    SystemPreferences in_systemPrefs)
	{
	    int l_index = 0, i = 0;

	    //Internal variables
	    m_io = in_io;
	    m_sink = in_sink;
	    m_pendingCommands = in_pendingCommands;
	    m_systemPrefs = in_systemPrefs;

	    //Re-usable buffers
	    m_data = new byte[m_systemPrefs.getBlockSize()];
	    m_buffers = new StringBuffer[5];
	    m_fieldValues = new StringBuffer[10];
	    for(l_index = 0; l_index < m_buffers.length; l_index++)
	    {
	        m_buffers[l_index] = new StringBuffer();
	    }

	    //Note: The starting point for the index is dependent on how many
	    //of the StringBuffers in m_buffers are used while processing
	    //ENVELOPE.. only used in fetchEnvelope
	    for(i = 0, l_index = 2; l_index < m_buffers.length; i++, l_index++)
	    {
            m_fieldValues[i] = m_buffers[l_index];
	    }

	    for(; i < m_fieldValues.length; i++)
	    {
	        m_fieldValues[i] = new StringBuffer();
	    }
	    m_readHeader = false;
	    m_appendStream = null;
	}


    ///////////////////////////////////////////////////////////////////////////////
    //Parsing methods for each server response type

    /**
    * Indicates the end of response(s) for successful commands
    * @param in_data The data to be parsed
    */
    public void taggedLine(StringBuffer in_data) throws IMAP4Exception
    {
        parseTaggedLine(in_data);
        m_pendingCommands.removeElement((m_buffers[0]).toString());
        m_sink.taggedLine(m_buffers[0], m_buffers[1], m_buffers[2]);
    }

    /**
    * Indicates the response for unsuccessful commands (error responses)
    * @param in_data The data to be parsed
    */
    public void error(StringBuffer in_data) throws IMAP4Exception
    {
        parseTaggedLine(in_data);
        m_pendingCommands.removeElement((m_buffers[0]).toString());
        m_sink.error(m_buffers[0], m_buffers[1], m_buffers[2]);
    }

    /**
    * Indicates an untagged error for NO and BAD
    * @param in_data The data to be parsed
    */
    public void untaggedError(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;

        //Initialize buffers
        for(int i = 0; i < m_buffers.length; i++)
        {
            m_buffers[i].setLength(0);
        }

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //This error is untagged
        m_buffers[0].append(IGlobals.UNTAGGED);

        //Store the status: NO or BAD
        l_end = storeSegment(in_data, m_buffers[1], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Store the human-readable information
        if((l_begin == -1) || (l_end == -1))
        {
            l_begin = 0;
            l_begin = indexOf(in_data, ' ', l_begin);
            if(l_begin == -1)
            {
                throw new IMAP4Exception(IGlobals.UnexpectedServerData);
            }
            l_begin = indexOf(in_data, ' ', l_begin + 1);
            if(l_begin == -1)
            {
                throw new IMAP4Exception(IGlobals.UnexpectedServerData);
            }
            l_begin++;
        }
        else
        {
            l_begin = indexOf(in_data, ' ', l_end);
            if(l_begin == -1)
            {
                throw new IMAP4Exception(IGlobals.UnexpectedServerData);
            }
            l_begin++;
        }

        m_buffers[2].setLength(0);
        for(int i = l_begin; i < in_data.length(); i++)
        {
            m_buffers[2].append(in_data.charAt(i));
        }

        m_sink.error(m_buffers[0], m_buffers[1], m_buffers[2]);
    }

    /**
    * Parses an unsolicited ok response
    * @param in_data The data to be parsed
    * @param IMAP4Exception A parsing error occured
    */
    public void ok(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;

        //Retrieve the response code
        m_buffers[0].setLength(0);
        l_begin = indexOf(in_data, '[', l_begin);
        if(l_begin != -1)
        {
            l_end = indexOf(in_data, ']', l_begin);
            if(l_end != -1)
            {
                for(int i = l_begin; i < (l_end + 1); i++)
                {
                    m_buffers[0].append(in_data.charAt(i));
                }
            }
        }

        //Store the human-readable information
        if((l_begin == -1) || (l_end == -1))
        {
            l_begin = 0;
            l_begin = indexOf(in_data, ' ', l_begin);
            if(l_begin == -1)
            {
                throw new IMAP4Exception(IGlobals.UnexpectedServerData);
            }
            l_begin = indexOf(in_data, ' ', l_begin + 1);
            if(l_begin == -1)
            {
                throw new IMAP4Exception(IGlobals.UnexpectedServerData);
            }
            l_begin++;
        }
        else
        {
            l_begin = indexOf(in_data, ' ', l_end);
            if(l_begin == -1)
            {
                throw new IMAP4Exception(IGlobals.UnexpectedServerData);
            }
            l_begin++;
        }

        m_buffers[1].setLength(0);
        for(int i = l_begin; i < in_data.length(); i++)
        {
            m_buffers[1].append(in_data.charAt(i));
        }

        m_sink.ok(m_buffers[0], m_buffers[1]);
    }

    /**
    * Parses the tagged line
    * @param in_data The data to be parsed
    */
    protected void parseTaggedLine(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;

        //Initialize buffers
        for(int i = 0; i < m_buffers.length; i++)
        {
            m_buffers[i].setLength(0);
        }

        //Store the tag
        l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Store the status
        l_end = storeSegment(in_data, m_buffers[1], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Store the reason
        l_end = in_data.length();
        for(int i = l_begin; i < l_end; i++)
        {
            m_buffers[2].append(in_data.charAt(i));
        }
    }


    /**
    * The capability of the IMAP server
    * @param in_data The data to be parsed
    */
    public void capability(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over CAPABILITY
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //The rest is the capability listing
        m_buffers[0].setLength(0);
        l_end = in_data.length();
        for(int i = l_begin; i < l_end; i++)
        {
            m_buffers[0].append(in_data.charAt(i));
        }
        m_sink.capability(m_buffers[0]);
    }

    /**
    * Mailboxes that matched the search criteria
    * @param in_data The data to be parsed
    */
    public void list(StringBuffer in_data)
    {
        m_buffers[0].setLength(0);
        m_buffers[1].setLength(0);
        m_buffers[2].setLength(0);

        extractMailboxInfo(in_data, m_buffers[0], m_buffers[1], m_buffers[2]);
        m_sink.list(m_buffers[0], m_buffers[1], m_buffers[2]);
    }

    /**
    * Subscribed or active mailboxes that matched the search criteria
    * @param in_data The data to be parsed
    */
    public void lsub(StringBuffer in_data)
    {
        m_buffers[0].setLength(0);
        m_buffers[1].setLength(0);
        m_buffers[2].setLength(0);

        extractMailboxInfo(in_data, m_buffers[0], m_buffers[1], m_buffers[2]);
        m_sink.lsub(m_buffers[0], m_buffers[1], m_buffers[2]);
    }

    /**
    * The response to a status command
    * @param in_data The data to be parsed
    */
    public void status(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over STATUS
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip the opening parenthesis
        l_end = indexOf(in_data, '(', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        while(l_begin < in_data.length())
        {
            //Extract the data items
            m_buffers[0].setLength(0);
            m_buffers[1].setLength(0);
            l_begin = skipWhite(in_data, l_begin);
            l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
            if(l_end == -1)
            {
                break;
            }
            l_end++;
            l_begin = l_end;

            //Extract the value of the data item
            l_begin = skipWhite(in_data, l_begin);

            l_end = storeSegment(in_data, m_buffers[1], ' ', l_begin);
            if(l_end == -1)
            {
                l_end = storeSegment(in_data, m_buffers[1], ')', l_begin);
                if(l_end == -1)
                {
                    throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                }
            }
            l_end++;
            l_begin = l_end;

            switch(resolveStatusDataItem(m_buffers[0]))
            {
                case IGlobals.StatusMessages:
                {
                    m_sink.statusMessages(atoi(m_buffers[1], 0, m_buffers[1].length()));
                    break;
                }
                case IGlobals.StatusRecent:
                {
                    m_sink.statusRecent(atoi(m_buffers[1], 0, m_buffers[1].length()));
                    break;
                }
                case IGlobals.StatusUidNext:
                {
                    m_sink.statusUidnext(atoi(m_buffers[1], 0, m_buffers[1].length()));
                    break;
                }
                case IGlobals.StatusUidValidity:
                {
                    m_sink.statusUidvalidity(atoi(m_buffers[1], 0, m_buffers[1].length()));
                    break;
                }
                case IGlobals.StatusUnSeen:
                {
                    m_sink.statusUnseen(atoi(m_buffers[1], 0, m_buffers[1].length()));
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    /**
    * The messages that matched the search criteria
    * @param in_data The data to be parsed
    */
    public void search(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;
        Object l_reference = null;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over SEARCH
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            l_end =in_data.length();
            l_begin = l_end;
        }
        else
        {
            l_end++;
            l_begin = l_end;
        }
        l_reference = m_sink.searchStart();

        while(l_begin < in_data.length())
        {
            //Extract the message number
            m_buffers[0].setLength(0);
            l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
            if(l_end == -1)
            {
                for(int i = l_begin; i < in_data.length(); i++)
                {
                    m_buffers[0].append(in_data.charAt(i));
                }
                l_end = in_data.length();
                l_begin = l_end;
            }
            else
            {
                l_end++;
                l_begin = l_end;
            }
            m_sink.search(l_reference, atoi(m_buffers[0], 0, m_buffers[0].length()));
        }
        m_sink.searchEnd(l_reference);
    }

    /**
    * The flags that are applicable for the selected mailbox
    * @param in_data The data to be parsed
    */
    public void flags(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;
        m_buffers[0].setLength(0);

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over FLAGS
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        l_end = matchBrackets(in_data, m_buffers[0], l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        m_sink.flags(m_buffers[0]);
    }

    /**
    * The total number of messages in the selected mailbox
    * @param in_data The data to be parsed
    */
    public void exists(StringBuffer in_data)
    {
        m_sink.exists(getNumber(in_data));
    }

    /**
    * The message number of the expunged message
    * @param in_data The data to be parsed
    */
    public void expunge(StringBuffer in_data)
    {
        m_sink.expunge(getNumber(in_data));
    }

    /**
    * The total number of messages with the \Recent flag set
    * @param in_data The data to be parsed
    */
    public void recent(StringBuffer in_data)
    {
        m_sink.recent(getNumber(in_data));
    }

    /**
    * The reason why the connection was closed
    * @param in_data The data to be parsed
    */
    public void bye(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;
        m_buffers[0].setLength(0);

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over BYE
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //The rest is the capability listing
        l_end = in_data.length();
        for(int i = l_begin; i < l_end; i++)
        {
            m_buffers[0].append(in_data.charAt(i));
        }

        m_sink.bye(m_buffers[0]);
    }

    /**
    * Parse the fetch data and pass into the appropriate data sink
    * @param in_data The data to be parsed. "* msg FETCH (....)"
    */
    public void fetch(StringBuffer in_data) throws IOException
    {
        int l_begin = 0, l_end = -1, l_msg = 0;
        Object l_reference = null;
        boolean l_readName = true;

        for(int i=0; i<m_buffers.length; i++)
        {
            m_buffers[i].setLength(0);
        }

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Store the message number in m_buffers[0]
        l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;
        l_reference = m_sink.fetchStart(atoi(m_buffers[0], 0, m_buffers[0].length()));

        //Skip over FETCH
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip the first bracket
        l_end = indexOf(in_data, '(', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        while(l_begin < in_data.length())
        {

            //Extract the data item
            l_begin = skipWhite(in_data, l_begin);
            m_buffers[0].setLength(0);
            l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
            if(l_end == -1)
            {
                break;
            }
            l_end++;
            l_begin = l_end;

            //Extract the value of the data item
            l_begin = skipWhite(in_data, l_begin);

            switch(resolveFetchDataItem(m_buffers[0]))
            {
                case IGlobals.BodyStructure:
                {
                    l_end = matchBrackets(in_data, m_buffers[1], l_begin);
                    if(l_end == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_end++;
                    l_begin = l_end;
                    m_sink.fetchBodyStructure(l_reference, m_buffers[1]);
                    m_buffers[1].setLength(0);
                    break;
                }
                case IGlobals.Message:
                case IGlobals.Rfc822Text:
                {
                    //Extract the size of the message block
                    int l_totalSize = 0;
                    int l_bytesRead = 0;
                    int l_readChunk = 0;
                    int l_index = -1;
                    byte[] l_dataChunklet = null;

                    l_end = indexOf(in_data, '{', l_begin);
                    if(l_end == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_end++;
                    l_begin = l_end;

                    l_end = storeSegment(in_data, m_buffers[1], '}', l_begin);
                    if(l_end == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_totalSize = atoi(m_buffers[1], 0, m_buffers[1].length());

                    //Read in a block of l_totalSize from the IO
                    while(l_bytesRead < l_totalSize)
                    {
                        //Determine the size of the next chunk to read
                        if((l_bytesRead + m_systemPrefs.getBlockSize()) > l_totalSize)
                        {
                            l_readChunk = l_totalSize - l_bytesRead;
                            l_dataChunklet = new byte[l_readChunk];
                        }
                        else
                        {
                            l_readChunk = m_systemPrefs.getBlockSize();
                            l_dataChunklet = m_data;
                        }

                        //Read the chunk in
                        l_readChunk = m_io.read(l_dataChunklet, l_readChunk);
                        if(l_readChunk == -1)
                        {
                            throw new IOException(IGlobals.ConnectionLost);
                        }

                        l_bytesRead += l_readChunk;

                        //Parse the field and value out
                        if(m_readHeader)
                        {
                            int l_fieldIndex = -1;
                            for(int j = 0; j < l_readChunk; j++)
                            {
                                j = readHeaderLine(l_dataChunklet, j, m_buffers[2]);
                                if(j == -1)
                                {
                                    break;
                                }

                                if( (m_buffers[2].charAt(0) == '\r') &&
                                    (m_buffers[2].charAt(1) == '\n'))
                                {
                                    m_readHeader = false;
                                    break;
                                }
                                l_fieldIndex = getFieldName(m_buffers[2], 0, m_buffers[3]);
                                if(l_fieldIndex == -1)
                                {
                                    throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                                }
                                l_fieldIndex++;
                                getFieldValue(m_buffers[2], l_fieldIndex, m_buffers[4]);
                                m_sink.fetchHeader(l_reference, m_buffers[3], m_buffers[4]);
                                m_buffers[2].setLength(0);
                                m_buffers[3].setLength(0);
                                m_buffers[4].setLength(0);
                            }

                        }

                        m_sink.fetchData(l_reference, l_dataChunklet, l_bytesRead, l_totalSize);
                    }

                    m_io.readLine(in_data);
                    l_begin = 0;
                    break;
                }
                case IGlobals.Rfc822Size:
                {
                    l_end = storeSegment(in_data, m_buffers[1], ' ', l_begin);
                    if(l_end == -1)
                    {
                        l_end = storeSegment(in_data, m_buffers[1], ')', l_begin);
                        if(l_end == -1)
                        {
                            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                        }
                    }
                    l_end++;
                    l_begin = l_end;
                    m_sink.fetchSize(l_reference, atoi(m_buffers[1], 0, m_buffers[1].length()));
                    m_buffers[1].setLength(0);
                    break;
                }
                case IGlobals.Uid:
                {
                    l_end = storeSegment(in_data, m_buffers[1], ' ', l_begin);
                    if(l_end == -1)
                    {
                        l_end = storeSegment(in_data, m_buffers[1], ')', l_begin);
                        if(l_end == -1)
                        {
                            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                        }
                    }
                    l_end++;
                    l_begin = l_end;
                    m_sink.fetchUid(l_reference, atoi(m_buffers[1], 0, m_buffers[1].length()));
                    m_buffers[1].setLength(0);
                    break;
                }
                case IGlobals.Flags:
                {
                    m_buffers[1].setLength(0);

                    l_end = matchBrackets(in_data, m_buffers[1], l_begin);
                    if(l_end == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_end++;
                    l_begin = l_end;
                    m_sink.fetchFlags(l_reference, m_buffers[1]);
                    m_buffers[1].setLength(0);
                    break;
                }
                case IGlobals.Envelope:
                {
                    //Get the closed value of the entire envelope
                    int l_fieldBegin = 0, l_fieldEnd = -1, l_fieldIndex = -1;
                    m_buffers[1].setLength(0);
                    for(int i = 0; i < m_fieldValues.length; i++)
                    {
                        m_fieldValues[i].setLength(0);
                    }

                    l_end = matchBrackets(in_data, m_buffers[1], l_begin);
                    if(l_end == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_end++;
                    l_begin = l_end;

                    //Extract the date
                    l_fieldBegin++; //Skip the opening bracket
                    l_fieldEnd = matchFieldQuotes(m_buffers[1], m_fieldValues[0], l_fieldBegin);
                    if(l_fieldEnd == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_fieldEnd++;
                    l_fieldBegin = l_fieldEnd;

                    //Extract the subject
                    l_fieldEnd = matchFieldQuotes(m_buffers[1], m_fieldValues[1], l_fieldBegin);
                    if(l_fieldEnd == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_fieldEnd++;
                    l_fieldBegin = l_fieldEnd;

                    for(int i = 2; i < 8; i++)
                    {
                        //Extract the from, sender, reply-to, to, cc, bcc
                        l_fieldEnd = matchFieldBrackets(m_buffers[1], m_fieldValues[i], l_fieldBegin);
                        if(l_fieldEnd == -1)
                        {
                            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                        }
                        l_fieldEnd++;
                        l_fieldBegin = l_fieldEnd;
                    }

                    //Extract the in-reply-to
                    l_fieldEnd = matchFieldQuotes(m_buffers[1], m_fieldValues[8], l_fieldBegin);
                    if(l_fieldEnd == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_fieldEnd++;
                    l_fieldBegin = l_fieldEnd;

                    //Extract the message-id
                    l_fieldEnd = matchFieldQuotes(m_buffers[1], m_fieldValues[9], l_fieldBegin);
                    if(l_fieldEnd == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_fieldEnd++;
                    l_fieldBegin = l_fieldEnd;

                    m_sink.fetchEnvelope(l_reference, m_fieldValues);
                    break;
                }
                case IGlobals.InternalDate:
                {
                    l_end = indexOf(in_data, '"', l_begin);
                    if(l_end == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_end++;
                    l_begin = l_end;

                    l_end = storeSegment(in_data, m_buffers[1], '"', l_begin);
                    if(l_end == -1)
                    {
                        throw new IMAP4Exception(IGlobals.UnexpectedServerData);
                    }
                    l_end++;
                    l_begin = l_end;
                    m_sink.fetchInternalDate(l_reference, m_buffers[1]);
                    m_buffers[1].setLength(0);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        m_sink.fetchEnd(l_reference);
    }

    /**
    * Parse the data continuation request
    * @param in_data The data to be parsed
    */
    public void plus(StringBuffer in_data) throws IOException
    {
        //Wait for the continuation response
        //Send out message data
        int l_bytesRead = 0, l_readChunk = 0, l_totalSize;

        if(m_appendStream == null)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }

        l_totalSize = m_appendStream.available();

        while(l_bytesRead < l_totalSize)
        {
            //Determine the size of the next chunk to read
            if((l_bytesRead + m_systemPrefs.getBlockSize()) > l_totalSize)
            {
                l_readChunk = l_totalSize - l_bytesRead;
            }
            else
            {
                l_readChunk = m_systemPrefs.getBlockSize();
            }

            //Read the chunk in
            m_appendStream.read(m_data, 0, l_readChunk);
            l_bytesRead += l_readChunk;

            //Write the data out
            m_io.write(m_data, 0, l_readChunk);
        }
        m_io.write(IGlobals.CarriageNewLine);
        m_io.flush();
    }

    /**
    * Parse the namespace data
    * @param in_data The data to be parsed
    */
    public void namespace(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;
        int l_beginNS = 0, l_endNS = -1;
        Object l_reference = null;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over NAMESPACE
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        l_reference = m_sink.nameSpaceStart();

        //Extract the personal namespace
        m_buffers[0].setLength(0);
        l_end = matchFieldBrackets(in_data, m_buffers[0], l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Extract each field
        l_beginNS = 1;
        while(l_beginNS < m_buffers[0].length())
        {
            m_buffers[1].setLength(0);
            l_endNS = matchFieldBrackets(m_buffers[0], m_buffers[1], l_beginNS);
            if(l_endNS == -1)
            {
                break;
            }
            l_endNS++;
            l_beginNS = l_endNS;
            m_sink.nameSpacePersonal(l_reference, m_buffers[1]);
        }

        //Extract the other user's namespace
        m_buffers[0].setLength(0);
        l_end = matchFieldBrackets(in_data, m_buffers[0], l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Extract each field
        l_beginNS = 1;
        while(l_beginNS < m_buffers[0].length())
        {
            m_buffers[1].setLength(0);
            l_endNS = matchFieldBrackets(m_buffers[0], m_buffers[1], l_beginNS);
            if(l_endNS == -1)
            {
                break;
            }
            l_endNS++;
            l_beginNS = l_endNS;
            m_sink.nameSpaceOtherUsers(l_reference, m_buffers[1]);
        }

        //Extract the shared namespace
        l_end = matchFieldBrackets(in_data, m_buffers[0], l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Extract each field
        l_beginNS = 1;
        while(l_beginNS < m_buffers[0].length())
        {
            m_buffers[1].setLength(0);
            l_endNS = matchFieldBrackets(m_buffers[0], m_buffers[1], l_beginNS);
            if(l_endNS == -1)
            {
                break;
            }
            l_endNS++;
            l_beginNS = l_endNS;
            m_sink.nameSpaceShared(l_reference, m_buffers[1]);
        }

        m_sink.nameSpaceEnd(l_reference);
    }

    /**
    * Parse the acl data
    * @param in_data The data to be parsed
    */
    public void acl(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;
        Object l_reference = null;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over ACL
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Get the mailbox name
        m_buffers[0].setLength(0);
        l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;
        l_reference = m_sink.aclStart(m_buffers[0]);

        while(l_begin < in_data.length())
        {
            //Get identifier
            m_buffers[0].setLength(0);
            l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
            if(l_end == -1)
            {
                break;
            }
            l_end++;
            l_begin = l_end;

            //Get the rights
            m_buffers[1].setLength(0);
            l_end = storeSegment(in_data, m_buffers[1], ' ', l_begin);
            if(l_end == -1)
            {
                l_end = storeSegment(in_data, m_buffers[1], '\n', l_begin);
                if(l_end == -1)
                {
                    break;
                }
            }
            l_end++;
            l_begin = l_end;

            m_sink.aclIdentifierRight(l_reference, m_buffers[0], m_buffers[1]);
        }

        m_sink.aclEnd(l_reference);

    }

    /**
    * Parse the listrights data
    * @param in_data The data to be parsed
    */
    public void listrights(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;
        Object l_reference = null;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over LISTRIGHTS
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Get the mailbox name
        m_buffers[0].setLength(0);
        l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Get the identifier
        m_buffers[1].setLength(0);
        l_end = storeSegment(in_data, m_buffers[1], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        l_reference = m_sink.listRightsStart(m_buffers[0], m_buffers[1]);

        //Get the required rights
        m_buffers[0].setLength(0);
        l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;
        m_sink.listRightsRequiredRights(l_reference, m_buffers[0]);

        while(l_begin < in_data.length())
        {
            //Get the optional rights
            m_buffers[0].setLength(0);
            l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
            if(l_end == -1)
            {
                l_end = storeSegment(in_data, m_buffers[0], '\n', l_begin);
                if(l_end == -1)
                {
                    break;
                }
            }
            l_end++;
            l_begin = l_end;
            m_sink.listRightsOptionalRights(l_reference, m_buffers[0]);
        }

        m_sink.listRightsEnd(l_reference);
    }

    /**
    * Parse the myrights data
    * @param in_data The data to be parsed
    */
    public void myrights(StringBuffer in_data) throws IMAP4Exception
    {
        int l_begin = 0, l_end = -1;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Skip over MYRIGHTS
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Get the mailbox name
        m_buffers[0].setLength(0);
        l_end = storeSegment(in_data, m_buffers[0], ' ', l_begin);
        if(l_end == -1)
        {
            throw new IMAP4Exception(IGlobals.UnexpectedServerData);
        }
        l_end++;
        l_begin = l_end;

        //Get the rights
        m_buffers[1].setLength(0);

        for(int i = l_begin; i < in_data.length(); i++)
        {
            m_buffers[1].append(in_data.charAt(i));
        }

        m_sink.myRights(m_buffers[0], m_buffers[1]);
    }

    /**
    * Raw (unparsed) data is pushed into here.  Note: Matches up with sendCommand
    * and will also be used as a default method to push information that doesn't
    * belong anywhere else
    * @param in_data The data to be parsed
    */
    public void rawResponse(StringBuffer in_data)
    {
        m_sink.rawResponse(in_data);
    }

    ///////////////////////////////////////////////////////////////////////////////
    //General methods

    /**
    * Sets the append stream
    * @param in_iStream The message data
    */
    public void setAppendStream(InputStream in_iStream)
    {
        m_appendStream = in_iStream;
    }

    /**
    * Sets the sink
    * @param in_iStream The message data
    */
    public void setResponseSink(IIMAP4Sink in_sink)
    {
        m_sink = in_sink;
    }

    ///////////////////////////////////////////////////////////////////////////////
    //Parsing Utilities

    /**
    * Finds the first occurrence of in_find after in_index in in_src
    * @param in_src The StringBuffer to search through
    * @param in_find The character to find
    * @param in_index The to begin the search from
    * @return int The index of in_find or -1 if not found
    */
    public int indexOf(StringBuffer in_src, char in_find, int in_index)
    {
        for(int i = in_index; i < in_src.length(); i++)
        {
            if(in_src.charAt(i) == in_find)
            {
                //Found in_find
                return i;
            }
        }
        return -1;
    }

    /**
    * Returns the index of the next position that is not white space
    * @param in_src The StringBuffer to search through
    * @param in_start The index to begin the search from
    * @return int The index of the next non-white space
    */
    public int skipWhite(StringBuffer in_src, int in_start)
    {
        for(int i = in_start; i < in_src.length(); i++)
        {
            if(in_src.charAt(i) != ' ')
            {
                //Found in_find
                return i;
            }
        }
        return -1;
    }

    /**
    * Reads a line of the header into out_line
    * @param in_src The data to search through
    * @param in_index The index to begin searching from
    * @param out_field The storage for the line
    * @returns int The index of the end of the line (\r,\n,\r\n) or -1 if
    * end of line not found
    */
    public int readHeaderLine(byte[] in_src, int in_index, StringBuffer out_line)
    {
	    char l_nextChar;

	    for(int i = in_index; i < in_src.length; i++)
	    {
	        l_nextChar = (char)in_src[i];
            out_line.append(l_nextChar);
	        if(l_nextChar == '\n')
	        {
	            if((i+1) < in_src.length)
	            {
                    l_nextChar = (char)in_src[i + 1];
                }
                if((l_nextChar != '\t')&&(l_nextChar != ' '))
                {
                    return i;
                }
	        }
	    }
	    return -1;
    }

    /**
    * Extracts the next field name after in_index and places it in out_field
    * @param in_src The data to search through
    * @param in_index The index to begin searching from
    * @param out_field The storage for the field name
    * @returns int The index of the last position of the field name (index of :)
    * or -1 if the name is not complete
    */
    public int getFieldName(StringBuffer in_src, int in_index, StringBuffer out_field)
    {
	    char l_nextChar;

	    for(int i = in_index; i < in_src.length(); i++)
	    {
	        l_nextChar = in_src.charAt(i);
	        if(l_nextChar == ':')
	        {
	            return i;
	        }
	        else
	        {
	            out_field.append(l_nextChar);
	        }
	    }
	    return -1;
    }


    /**
    * Copies all data from in_index in in_src to out_value
    * @param in_src The data to search through
    * @param in_index The index to begin searching from
    * @param out_value The storage for the field value
    */
    public void getFieldValue(StringBuffer in_src, int in_index, StringBuffer out_value)
    {
        int l_index = in_index;

        l_index = skipWhite(in_src, l_index);

	    for(int i = l_index; i < in_src.length(); i++)
	    {
	        //Don't store the new line at the end of the field value
            if( ((i + 1) < in_src.length()) &&
                ((i + 2) == in_src.length()) &&
                (in_src.charAt(i) == '\r') &&
                (in_src.charAt(i + 1) == '\n'))
            {
                return;
            }
            out_value.append(in_src.charAt(i));
	    }
    }

    /**
    * Finds the first occurrence of in_find after in_index in in_src
    * @param in_src The StringBuffer to search through
    * @param in_dest The StringBuffer to store the block in_index to in_find
    * @param in_find The character to find
    * @param in_index The to begin the search from
    * @return int The index of in_find or -1 if not found
    */
    public int storeSegment(StringBuffer in_src, StringBuffer in_dest, char in_find, int in_index)
    {
        int l_initialLength = in_dest.length();
        for(int i = in_index; i < in_src.length(); i++)
        {
            if(in_src.charAt(i) == in_find)
            {
                //Found in_find
                return i;
            }
            else
            {
                in_dest.append(in_src.charAt(i));
            }
        }
        in_dest.setLength(l_initialLength);
        return -1;
    }


    /**
    * Same as matchBrackets, but also counts NIL value as a bracketed value
    * @param in_data The data to be parsed
    * @param in_value The data within the brackets
    * @param in_start The index to begin parsing from
    * @return int The index where the quoted value ended or -1 on error
    */
    public int matchFieldQuotes(StringBuffer in_data, StringBuffer in_value, int in_start)
    {
        int l_index = in_start;
        int l_fieldEnd = -1, l_fieldBegin = -1;

        l_index = skipWhite(in_data, l_index);
        if( (in_data.charAt(l_index) == 'N') &&
            (in_data.charAt(l_index + 1) == 'I') &&
            (in_data.charAt(l_index + 2) == 'L'))
        {
            in_value.append('N');
            in_value.append('I');
            in_value.append('L');
            return (l_index + 2);
        }

        //Extract the quoted value
        l_fieldEnd = indexOf(in_data, '"', in_start);
        if(l_fieldEnd == -1)
        {
            return -1;
        }
        l_fieldEnd++;
        l_fieldBegin = l_fieldEnd;

        l_fieldEnd = storeSegment(in_data, in_value, '"', l_fieldBegin);
        if(l_fieldEnd == -1)
        {
            return -1;
        }

        return l_fieldEnd;
    }

    /**
    * Extract the number out the server response
    * @param in_data Server response of the form * 233 EXISTS
    * @return int The number at the second token position or -1 if n/a
    */
    protected int getNumber(StringBuffer in_data)
    {
        int l_begin = 0, l_end = -1;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            return -1;
        }
        l_end++;
        l_begin = l_end;

        //Get the number of messages
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            return -1;
        }

        return atoi(in_data, l_begin, l_end);
    }

    /**
    * Resolves the value of in_dataItem without allocating space for a new String
    * @param in_dataItem The fetch data item
    * @return int A constant representing the data item
    */
    protected int resolveFetchDataItem(StringBuffer in_dataItem)
    {
        int l_dataLength = in_dataItem.length();
        if(l_dataLength < 1)
        {
            return IGlobals.Unknown;
        }

        switch(in_dataItem.charAt(0))
        {
            case 'B':
            {
                if( (l_dataLength > 12) &&
                    (in_dataItem.charAt(1) == 'O') &&
                    (in_dataItem.charAt(2) == 'D') &&
                    (in_dataItem.charAt(3) == 'Y') &&
                    (in_dataItem.charAt(4) == 'S') &&
                    (in_dataItem.charAt(5) == 'T') &&
                    (in_dataItem.charAt(6) == 'R') &&
                    (in_dataItem.charAt(7) == 'U') &&
                    (in_dataItem.charAt(8) == 'C') &&
                    (in_dataItem.charAt(9) == 'T') &&
                    (in_dataItem.charAt(10) == 'U') &&
                    (in_dataItem.charAt(11) == 'R') &&
                    (in_dataItem.charAt(12) == 'E'))
                {
                    return IGlobals.BodyStructure;
                }
                else if((l_dataLength > 3) &&
                        (in_dataItem.charAt(1) == 'O') &&
                        (in_dataItem.charAt(2) == 'D') &&
                        (in_dataItem.charAt(3) == 'Y'))
                {
                    if( (l_dataLength > 4) &&
                        (in_dataItem.charAt(4) == '['))
                    {
                        if( (in_dataItem.charAt(5) == ']') ||
                            (in_dataItem.charAt(5) == '0') ||
                            (in_dataItem.charAt(5) == 'H'))
                        {
                            m_readHeader = true;
                        }
                    }
                    else if((l_dataLength > 10) &&
                            (in_dataItem.charAt(4) == '.') &&
                            (in_dataItem.charAt(5) == 'P') &&
                            (in_dataItem.charAt(6) == 'E') &&
                            (in_dataItem.charAt(7) == 'E') &&
                            (in_dataItem.charAt(8) == 'K') &&
                            (in_dataItem.charAt(9) == '['))
                    {
                        if( (in_dataItem.charAt(10) == ']') ||
                            (in_dataItem.charAt(10) == '0') ||
                            (in_dataItem.charAt(10) == 'H'))
                        {
                            m_readHeader = true;
                        }
                    }
                    return IGlobals.Message;
                }
                break;
            }
            case 'R':
            {
                if( (l_dataLength > 5) &&
                    (in_dataItem.charAt(1) == 'F') &&
                    (in_dataItem.charAt(2) == 'C') &&
                    (in_dataItem.charAt(3) == '8') &&
                    (in_dataItem.charAt(4) == '2') &&
                    (in_dataItem.charAt(5) == '2'))
                {
                    if( (l_dataLength > 10) &&
                        (in_dataItem.charAt(6) == '.') &&
                        (in_dataItem.charAt(7) == 'S') &&
                        (in_dataItem.charAt(8) == 'I') &&
                        (in_dataItem.charAt(9) == 'Z') &&
                        (in_dataItem.charAt(10) == 'E'))
                    {
                        return IGlobals.Rfc822Size;
                    }
                    else if((l_dataLength > 10) &&
                            (in_dataItem.charAt(6) == '.') &&
                            (in_dataItem.charAt(7) == 'T') &&
                            (in_dataItem.charAt(8) == 'E') &&
                            (in_dataItem.charAt(9) == 'X') &&
                            (in_dataItem.charAt(10) == 'T'))
                    {
                        return IGlobals.Rfc822Text;
                    }
                    else if((l_dataLength > 12) &&
                            (in_dataItem.charAt(6) == '.') &&
                            (in_dataItem.charAt(7) == 'H') &&
                            (in_dataItem.charAt(8) == 'E') &&
                            (in_dataItem.charAt(9) == 'A') &&
                            (in_dataItem.charAt(10) == 'D') &&
                            (in_dataItem.charAt(11) == 'E') &&
                            (in_dataItem.charAt(12) == 'R'))
                    {
                        m_readHeader = true;
                        return IGlobals.Message;
                    }
                    m_readHeader = true;
                    return IGlobals.Message;
                }
                break;
            }
            case 'U':
            {
                if( (l_dataLength > 2) &&
                    (in_dataItem.charAt(1) == 'I') &&
                    (in_dataItem.charAt(2) == 'D'))
                {
                    return IGlobals.Uid;
                }
                break;
            }
            case 'F':
            {
                if( (l_dataLength > 4) &&
                    (in_dataItem.charAt(1) == 'L') &&
                    (in_dataItem.charAt(2) == 'A') &&
                    (in_dataItem.charAt(3) == 'G') &&
                    (in_dataItem.charAt(4) == 'S'))
                {
                    return IGlobals.Flags;
                }
                break;
            }
            case 'E':
            {
                if( (l_dataLength > 7) &&
                    (in_dataItem.charAt(1) == 'N') &&
                    (in_dataItem.charAt(2) == 'V') &&
                    (in_dataItem.charAt(3) == 'E') &&
                    (in_dataItem.charAt(4) == 'L') &&
                    (in_dataItem.charAt(5) == 'O') &&
                    (in_dataItem.charAt(6) == 'P') &&
                    (in_dataItem.charAt(7) == 'E'))
                {
                    return IGlobals.Envelope;
                }
                break;
            }
            case 'I':
            {
                if( (l_dataLength > 11) &&
                    (in_dataItem.charAt(1) == 'N') &&
                    (in_dataItem.charAt(2) == 'T') &&
                    (in_dataItem.charAt(3) == 'E') &&
                    (in_dataItem.charAt(4) == 'R') &&
                    (in_dataItem.charAt(5) == 'N') &&
                    (in_dataItem.charAt(6) == 'A') &&
                    (in_dataItem.charAt(7) == 'L') &&
                    (in_dataItem.charAt(8) == 'D') &&
                    (in_dataItem.charAt(9) == 'A') &&
                    (in_dataItem.charAt(10) == 'T') &&
                    (in_dataItem.charAt(11) == 'E'))
                {
                    return IGlobals.InternalDate;
                }
                break;
            }
            default:
            {
                break;
            }
        }
        return IGlobals.Unknown;
    }

    /**
    * Resolves the value of in_dataItem without allocating space for a new String
    * @param in_dataItem The status data item
    * @return int A constant representing the data item
    */
    protected int resolveStatusDataItem(StringBuffer in_dataItem)
    {
        int l_dataLength = in_dataItem.length();

        if(l_dataLength < 1)
        {
            return IGlobals.Unknown;
        }
        switch(in_dataItem.charAt(0))
        {
            case 'M':
            {
                if( (l_dataLength > 7) &&
                    (in_dataItem.charAt(1) == 'E') &&
                    (in_dataItem.charAt(2) == 'S') &&
                    (in_dataItem.charAt(3) == 'S') &&
                    (in_dataItem.charAt(4) == 'A') &&
                    (in_dataItem.charAt(5) == 'G') &&
                    (in_dataItem.charAt(6) == 'E') &&
                    (in_dataItem.charAt(7) == 'S'))
                {
                    return IGlobals.StatusMessages;
                }
                break;
            }
            case 'R':
            {
                if( (l_dataLength > 5) &&
                    (in_dataItem.charAt(1) == 'E') &&
                    (in_dataItem.charAt(2) == 'C') &&
                    (in_dataItem.charAt(3) == 'E') &&
                    (in_dataItem.charAt(4) == 'N') &&
                    (in_dataItem.charAt(5) == 'T'))
                {
                    return IGlobals.StatusRecent;
                }
                break;
            }
            case 'U':
            {
                if( (l_dataLength > 6) &&
                    (in_dataItem.charAt(1) == 'I') &&
                    (in_dataItem.charAt(2) == 'D') &&
                    (in_dataItem.charAt(3) == 'N') &&
                    (in_dataItem.charAt(4) == 'E') &&
                    (in_dataItem.charAt(5) == 'X') &&
                    (in_dataItem.charAt(6) == 'T'))
                {
                    return IGlobals.StatusUidNext;
                }
                else if((l_dataLength > 10) &&
                        (in_dataItem.charAt(1) == 'I') &&
                        (in_dataItem.charAt(2) == 'D') &&
                        (in_dataItem.charAt(3) == 'V') &&
                        (in_dataItem.charAt(4) == 'A') &&
                        (in_dataItem.charAt(5) == 'L') &&
                        (in_dataItem.charAt(6) == 'I') &&
                        (in_dataItem.charAt(7) == 'D') &&
                        (in_dataItem.charAt(8) == 'I') &&
                        (in_dataItem.charAt(9) == 'T') &&
                        (in_dataItem.charAt(10) == 'Y'))
                {
                    return IGlobals.StatusUidValidity;
                }
                else if((l_dataLength > 5) &&
                        (in_dataItem.charAt(1) == 'N') &&
                        (in_dataItem.charAt(2) == 'S') &&
                        (in_dataItem.charAt(3) == 'E') &&
                        (in_dataItem.charAt(4) == 'E') &&
                        (in_dataItem.charAt(5) == 'N'))
                {
                    return IGlobals.StatusUnSeen;
                }
                break;
            }
            default:
            {
                break;
            }
        }
        return IGlobals.Unknown;
    }


    /**
    * Convert the ascii number to an int
    * @param in_data The buffer with the number to convert
    * @param in_begin The beginning of the number
    * @param in_end The end of the number
    * @return int The number
    */
    protected int atoi(StringBuffer in_data, int in_begin, int in_end)
    {
        int i;
        int n;
        int sign;

        for ( i = in_begin; Character.isSpaceChar(in_data.charAt(i)); i++ )
        {
        }

        sign = (in_data.charAt(i) == '-') ? -1 : 1;

        if ( in_data.charAt(i) == '+' || in_data.charAt(i) == '-' )
        {
            i++;
        }

        for ( n = 0; (i < in_end) && Character.isDigit(in_data.charAt(i)); i++ )
        {
            n = 10 * n + ( in_data.charAt(i) - '0');
        }

        return sign * n;
    }


    protected int itoa( int in_number, byte[] io_string )
    {
        int i;
        int sign;

        if ((sign = in_number) < 0)
        {
            in_number = -in_number;
        }
        i = 0;

        do
        {
            io_string[i++] = (byte)(in_number % 10 + '0');
        }
        while ((in_number /= 10) > 0);

        if ( sign < 0 )
        {
            io_string[i] = '-';
        }
        reverse(io_string, i );
        return i;
    }

    protected void reverse( byte[] io_string, int in_length )
    {
        int c, i, j;

        for ( i = 0, j = in_length - 1; i < j; i++, j-- )
        {
            c = io_string[i];
            io_string[i] = io_string[j];
            io_string[j] = (byte)c;
        }
    }

    /**
    * Same as matchBrackets, but also counts NIL value as a bracketed value
    * @param in_data The data to be parsed
    * @param in_value The data within the brackets
    * @param in_start The index to begin parsing from
    * @return int The index where the bracketed value ended or -1 on error
    */
    public int matchFieldBrackets(StringBuffer in_data, StringBuffer in_value, int in_start)
    {
        int l_index = in_start;

        l_index = skipWhite(in_data, l_index);
        if( (in_data.charAt(l_index) == 'N') &&
            (in_data.charAt(l_index + 1) == 'I') &&
            (in_data.charAt(l_index + 2) == 'L'))
        {
            in_value.append('N');
            in_value.append('I');
            in_value.append('L');
            return (l_index + 2);
        }

        return matchBrackets(in_data, in_value, in_start);
    }


    /**
    * Stores the value of the next closed set of brackets within in_value
    * @param in_data The data to be parsed
    * @param in_value The data within the brackets
    * @param in_start The index to begin parsing from
    * @return int The index where the bracketed value ended or -1 on error
    */
    public int matchBrackets(StringBuffer in_data, StringBuffer in_value, int in_start)
    {
        //Indexes used to help find the bracketed value
        int l_nIndex = -1;
        int l_nLeftBraces = 0;

        //Flag to indicate when the Left brace at l_nLeft should be recounted
        boolean l_bRecount = false;

        //Verify the first position is a left bracket
        l_nIndex = indexOf(in_data, '(', in_start);
        if(l_nIndex == -1)
        {
            return -1;
        }
        in_value.append(in_data.charAt(l_nIndex));
        l_nIndex++;
        l_nLeftBraces++;

        for(; l_nIndex < in_data.length(); l_nIndex++)
        {
            in_value.append(in_data.charAt(l_nIndex));
            if(in_data.charAt(l_nIndex) == '(')
            {
                l_nLeftBraces++;
            }
            else if(in_data.charAt(l_nIndex) == ')')
            {
                l_nLeftBraces--;
                if(l_nLeftBraces < 1)
                {
                    //Found the closing bracket
                    return l_nIndex;
                }
            }
        }
        in_value.setLength(0);
        return -1;
    }

    /**
    * Extracts the mailbox info
    * @param in_data The data to parse
    * @param out_buffer1 Storage for the attributes
    * @param out_buffer2 Storage for the delimeter
    * @param out_buffer3 Storage for the name
    */
    public void extractMailboxInfo(  StringBuffer in_data, StringBuffer out_buffer1,
                                StringBuffer out_buffer2, StringBuffer out_buffer3)
    {
        int l_begin = 0, l_end = -1;

        //Skip over the *
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            return;
        }
        l_end++;
        l_begin = l_end;

        //Skip over LIST
        l_end = indexOf(in_data, ' ', l_begin);
        if(l_end == -1)
        {
            return;
        }
        l_end++;
        l_begin = l_end;

        //Extract the attributes
        l_end = matchBrackets(in_data, m_buffers[0], l_begin);
        if(l_end == -1)
        {
            return;
        }
        l_end++;
        l_begin = l_end;

        //Extract the delimeter
        l_end = indexOf(in_data, '"', l_begin);
        if(l_end == -1)
        {
            return;
        }
        l_end++;
        l_begin = l_end;

        l_end = storeSegment(in_data, m_buffers[1], '"', l_begin);
        if(l_end == -1)
        {
            return;
        }
        l_end++;
        l_begin = l_end;

        //Extract the name: The rest is the name of the mailbox
        l_begin = skipWhite(in_data, l_begin);
        l_end = in_data.length();
        for(int i = l_begin; i < l_end; i++)
        {
            m_buffers[2].append(in_data.charAt(i));
        }

    }

    //Data members
    ///////////////////////////////////////////////////////////////////////

    /** The io object*/
    private IO m_io;

    /** The data sink*/
    private IIMAP4Sink m_sink;

    /* Re-usable buffers for data sink*/
    private StringBuffer[] m_buffers;

    /** Re-usable buffer for fetchData*/
    private byte[] m_data;

    /** Storage for envelope field values;*/
    private StringBuffer[] m_fieldValues;

    /** The message data*/
    private InputStream m_appendStream;

    /** The commands awaiting responses*/
    private Vector m_pendingCommands;

    /** The system preferences*/
    private SystemPreferences m_systemPrefs;

    /** Parsing flags*/
    private boolean m_readHeader;


}
