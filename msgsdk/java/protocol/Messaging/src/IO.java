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

package netscape.messaging;

import java.net.Socket;
import java.io.OutputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.util.Vector;

/**
*Class for conducting IO with a server.  It communicates directly<br>
*with the input and output streams associated with the socket connected to<br>
*the ESMTP server.  It performs the handling of synchronous versus<br>
*asynchronous methods. It also handles PIPELINING (batching) of commands if<br>
*the user chose this at connection time and the ESMTP server also supports<br>
*PIPELINING. This class also initializes and manages the authentication and<br>
*security objects.<br>
*@author derekt@netscape.com
*@version 1.0
*/

public class IO
{

    ///////////////////////////////////////////////////////////////////////////
    // Data members used for network communication
    ///////////////////////////////////////////////////////////////////////////

    private Socket m_socket;
    private BufferedInputStream m_inStream;
    private BufferedOutputStream m_outStream;

    private int m_timeout;
    private final static byte[] m_newline = { '\r', '\n' };

    ///////////////////////////////////////////////////////////////////////////
    // Error messages.
    ///////////////////////////////////////////////////////////////////////////

    protected final static String errNotConnected =
                                    new String("Must connect to server");
    protected final static String errAlreadyConnected =
                                    new String("Already connected to server");

/**
*Constructor for the IO object. Initializes all data members<br>
*/
    public IO()
    {
        ///////////////////////////////////////////////////////////////////////
        // Initialize data members.
        ///////////////////////////////////////////////////////////////////////

        m_socket = null;
        m_inStream = null;
        m_outStream = null;
        m_timeout = 0;
    }

    public void connect( String in_server, int in_portNumber ) throws IOException
    {
        connect( in_server, in_portNumber, 1024, 1024 );
    }

/**
*Open a communication channel with the server. An input stream<br>
*is initialized for receiving data from the server and an output stream is<br>
*initialized for send data to the server.<br>
*@param in_szServer Specifies the ESMTP server to connect to.
*@param in_portNumber Specifies the port used to communicate with.
*@exception IOException thrown if IOError.
*/
    public void connect( String in_server,
                            int in_portNumber,
                            int in_inStreamBuf,
                            int in_outStreamBuf ) throws IOException
    {
        if ( m_socket != null )
        {
            throw new IOException( errAlreadyConnected );
        }

        ///////////////////////////////////////////////////////////////////////
        // Initialize counters and flags.
        ///////////////////////////////////////////////////////////////////////

        try
        {
            ///////////////////////////////////////////////////////////////////
            // Initializes the socket, input buffer and output buffer.
            ///////////////////////////////////////////////////////////////////

            m_socket = new Socket( in_server, in_portNumber );
            m_inStream = new BufferedInputStream( m_socket.getInputStream(), in_inStreamBuf );
            m_outStream = new BufferedOutputStream( m_socket.getOutputStream(), in_outStreamBuf );
            setTimeout( m_timeout );
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

    public void disconnect() throws IOException
    {
        ///////////////////////////////////////////////////////////////////////
        // Initialize counters and flags.
        ///////////////////////////////////////////////////////////////////////

        m_inStream = null;
        m_outStream = null;

        if ( m_socket != null )
        {
            m_socket.close();
            m_socket = null;
        }
    }

/**
*Determines if a connection is currently open.
*/
    private boolean isConnected()
    {
        if ( m_inStream == null || m_outStream == null )
        {
            return false;
        }

        return true;
    }

/**
*Fill a byte array buffer.
*/
    public int read( byte[] out_buffer, int in_offset, int in_length ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        if ( in_length > out_buffer.length )
        {
            in_length = out_buffer.length;
        }

        try
        {
            ///////////////////////////////////////////////////////////////////
            // Returns -1 if the specified amount of data is not available.
            ///////////////////////////////////////////////////////////////////

            if ( (m_timeout == -1) && (m_inStream.available() < in_length) )
            {
                return 0;
            }

            ///////////////////////////////////////////////////////////////////
            // Read the data from the input stream.
            ///////////////////////////////////////////////////////////////////

            return m_inStream.read( out_buffer, in_offset, in_length );
        }
        catch( InterruptedIOException e )
        {
            throw e;
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

/**
*Reads a line of data from the input stream.
*/
    public int readLine( byte[] out_buffer ) throws IOException
    {
        return readLine( out_buffer, 0, out_buffer.length );
    }

/**
*Reads a line of data from the input stream.
*/
    public int readLine( byte[] out_buffer, int in_offset, int in_length ) throws IOException
    {
        int l_byteCount = 0;

        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
            m_inStream.mark( in_length );

            ///////////////////////////////////////////////////////////////////
            // Read the data from the input stream.
            ///////////////////////////////////////////////////////////////////

            do
            {
                if ( (m_timeout == -1) && (m_inStream.available() == 0) )
                {
                    m_inStream.reset();
                    throw new InterruptedIOException();
                }

                out_buffer[in_offset] = (byte)m_inStream.read();
                in_offset++;
                l_byteCount++;
            }
            while( out_buffer[in_offset-1] != '\n' );

            return ( l_byteCount - 2 );
        }
        catch( InterruptedIOException e )
        {
            throw e;
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

	/**
	* Read's in in_bytes of data into in_data
	* @param in_data The storage for the data read in
	* @param in_bytes The maximum number of bytes to read
	* @param int The actual number of bytes read
    */
	public int read(byte[] in_data, int in_bytes) throws IOException
	{
	    int l_bytesRead = 0;
	    int l_totalBytes = 0;

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
    	    while(l_totalBytes < in_bytes)
    	    {
        	    l_bytesRead = m_inStream.read(in_data, l_totalBytes, in_bytes - l_totalBytes);
        	    if(l_bytesRead == -1)
        	    {
        	        if(l_totalBytes ==0)
        	        {
        	            return -1;
        	        }
        	        break;
        	    }
        	    l_totalBytes += l_bytesRead;
    	    }
    	    return l_totalBytes;
    	}
    	catch(IOException e)
    	{
    	    disconnect();
    	    throw e;
    	}
	}

	/**
	* Read's a line of data and stores it in m_responseBuffer
	* NOTE: This function strips off /r/n
    */
	public void readLine(StringBuffer in_line) throws IOException
	{
	    char l_nextChar = ' ';
		boolean l_bLineRead = false;
	    in_line.setLength(0);

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
    	    while(!l_bLineRead)
    	    {
    	        //Store a line of the server response
    	        l_nextChar = (char)m_inStream.read();
    	        if((l_nextChar == '\r') || (l_nextChar == '\n'))
    	        {
    	            m_inStream.mark(2);
                    l_nextChar = (char)m_inStream.read();
                    if(l_nextChar != '\n')
                    {
                        m_inStream.reset();
                    }
                    l_bLineRead = true;
    	        }
    	        else if(l_nextChar == -1)
    	        {
    	            throw new IOException("The socket has been closed unexpectedly.");
    	        }
    	        else
    	        {
        	        in_line.append(l_nextChar);
    	        }
    	    }
    	}
    	catch(IOException e)
    	{
    	    disconnect();
    	    throw e;
    	}
	}


/**
*@exception IOException thrown if IOError.
*/
    public void write( byte in_byte ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
            m_outStream.write( in_byte );
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

/**
*@exception IOException thrown if IOError.
*/
    public void write( String in_buffer ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
            int l_length = in_buffer.length();

            for ( int i = 0; i < l_length; i++ )
            {
                m_outStream.write( (byte)in_buffer.charAt(i) );
            }
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

/**
*@exception IOException thrown if IOError.
*/
    public void write( byte[] in_buffer ) throws IOException
    {
        write( in_buffer, 0, in_buffer.length );
    }

/**
*@exception IOException thrown if IOError.
*/
    public void write( byte[] in_buffer, int in_offset, int in_length ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
            ///////////////////////////////////////////////////////////////////
            // Send the command to the server
            ///////////////////////////////////////////////////////////////////

            m_outStream.write( in_buffer, in_offset, in_length );
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }


    /**
    * Sends client commands to IMAP server synchronously.
    * @param in_szCommand The IMAP command to invoke on the server
	* @return boolean TRUE if command executed, FALSE otherwise.  Note: Does not
	* indicate successful completion of the command, just that it has been invoked
	* on the IMAP Server.  You must look in in_command to find the server response
	* once the sendCommand() call returns.
	*/
	public void write(Vector in_data) throws IOException
	{
	    String l_dataString = null;

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
    	    for(int i = 0; i < in_data.size(); i++)
    	    {
    	        l_dataString = (String)in_data.elementAt(i);
    	        if(l_dataString != null)
    	        {
            	    for(int j = 0; j < l_dataString.length(); j++)
            	    {
                        m_outStream.write((byte)l_dataString.charAt(j));
                    }
                }
            }
        }
        catch(IOException e)
        {
            disconnect();
            throw e;
        }
	}


/**
*@exception IOException thrown if IOError.
*/
    public void send( String in_buffer, boolean in_bufferData ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
            int l_length = in_buffer.length();

            for ( int i = 0; i < l_length; i++ )
            {
                m_outStream.write( (byte)in_buffer.charAt(i) );
            }

            m_outStream.write( m_newline );

            if ( in_bufferData == false )
            {
                m_outStream.flush();
            }
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

/**
*@exception IOException thrown if IOError.
*/
    public void send( byte[] in_buffer, boolean in_bufferData ) throws IOException
    {
        send( in_buffer, 0, in_buffer.length, in_bufferData );
    }

/**
*@exception IOException thrown if IOError.
*/
    public void send( byte[] in_buffer,
                                int in_offset,
                                int in_length,
                                boolean in_bufferData ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
            m_outStream.write( in_buffer, in_offset, in_length );
            m_outStream.write( m_newline );

            if ( in_bufferData == false )
            {
                m_outStream.flush();
            }
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

/**
*/
    public void flush() throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }

        try
        {
            ///////////////////////////////////////////////////////////////////
            // Flush the output stream.
            ///////////////////////////////////////////////////////////////////

            m_outStream.flush();
        }
        catch ( IOException e )
        {
            disconnect();
            throw e;
        }
    }

/**
*Sets the socket timeout for reading data from the socket input buffer.
*/
    public void setTimeout( int in_timeout ) throws IOException
    {
        ///////////////////////////////////////////////////////////////////
        // Make sure there is a valid connection.
        ///////////////////////////////////////////////////////////////////

        // We should be able to set timeouts while disconnected.
/*
        if ( isConnected() == false )
        {
            throw new IOException(errNotConnected);
        }
*/
        if ( in_timeout < -1 )
        {
            throw new IllegalArgumentException();
        }

        m_timeout = in_timeout;

        if ( isConnected() == true )
        {
            try
            {
                if ( m_timeout != -1 )
                {
                    m_socket.setSoTimeout( m_timeout );
                }
            }
            catch ( IOException e )
            {
                disconnect();
                throw e;
            }
        }
    }
}
