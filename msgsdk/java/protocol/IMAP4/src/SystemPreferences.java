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

import netscape.messaging.*;
import java.io.*;
import java.net.*;
import java.util.*;

/**
* The SystemPreferences class records the preferences
* for the IMAP4 implementation of the Messaging Access SDK.
* <p>The methods of this class return preferences for block
* size, default port, debug flag, and time-out length.
* @author alterego@netscape.com
* @version 0.1
*/

public class SystemPreferences
{
    /**
    *Creates a SystemPreferences object.
    *Default constructor for the SystemPreferences class.
    */
    public SystemPreferences()
    {
        m_io = null;
        m_imapPort = 143;
        m_timeOut = 0;
        m_fDebug = false;
        m_blockSize = 1024;
    }

	/**
	* Sets the synchronous timeout for reading data from the socket
	* input buffer.
	* @param in_timeOut Time-out in milliseconds. Default: 60000 milliseconds.
	* @exception InterruptedIOException If the timeout period expires.
	* @see #getTimeout
	*/
	public synchronized void setTimeout(int in_timeOut) throws IOException
	{
		m_timeOut = in_timeOut;
		if(m_io != null)
		{
		    m_io.setTimeout(in_timeOut);
		}
	}

	/**
	* Gets the synchronous timeout for reading data from the socket
	* input buffer. If timeout expires, a InterruptedIOException is thrown.
	* @param in_timeOut Time-out in milliseconds.
	* @exception InterruptedIOException If the timeout period expires.
	* @see #setTimeout
	*/
	public synchronized int getTimeout()
	{
		return m_timeOut;
	}

	/**
	* Sets the debug flag.
	* @param in_fDebug Status of the flag. true: debug flag is present;
	* false: no debug flag.
	* @see #getDebugFlag
	*/
	public synchronized void setDebugFlag(boolean in_fDebug)
	{
		m_fDebug = in_fDebug;
	}

	/**
	* Gets the debug flag.
	* @return boolean Status of the debug flag. true: debug flag is present;
	* false: no debug flag.
	* @see #setDebugFlag
	*/
	public synchronized boolean getDebugFlag()
	{
		return m_fDebug;
	}

	/**
	* Sets the block size returned in fetchData.
	* @param int Block size.
	* @see #getBlockSize
	*/
	public synchronized void setBlockSize(int in_blockSize)
	{
		m_blockSize = in_blockSize;
	}

	/**
	* Gets the block size used in fetchData.
	* @return int Block size.
	* @see #setBlockSize
	*/
	public synchronized int getBlockSize()
	{
		return m_blockSize;
	}

	/**
	* Sets the default port to connect to.
	* @param in_port Numeric ID of default port.
	* @see #getPort
	*/
	public synchronized void setPort(int in_port)
	{
		m_imapPort = in_port;
	}

	/**
	* Gets the default port to connect to.
	* @return int The default port.
	* @see #setPort
	*/
	public synchronized int getPort()
	{
		return m_imapPort;
	}

	/**
	* Sets the IO object.
	* @param in_io The IO object
	*/
	protected synchronized void setIO(IO in_io)
	{
		m_io = in_io;
	}


    //Data members

    /**The default IMAP4 port*/
    private int m_imapPort;

    /**The time out in milliseconds for waiting for data on a read off the socket. Default: 1000*/
    private int m_timeOut;

    /**A reference to the dispatcher so that it can update the time out*/
    private IO m_io;

    /**The block size*/
    private int m_blockSize;

    /**The debug flag*/
    private boolean m_fDebug;

}

