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

package netscape.messaging.mime;
import java.io.*;
import java.lang.*;
import java.net.*;
import java.util.*;


/**
 * The MIMEHelper class defines a set of utility functions.
 * @author Prasad Yendluri,  Carson Lee
 */
public class MIMEHelper
{
    // error messages
    final static public String szERROR_BAD_PARAMETER = "Error: Bad parameter";
    final static public String szERROR_OUT_OF_MEMORY = "Error: Out of memory";
    final public static String szERROR_EMPTY_MESSAGE = "Error: Empty message";
    final public static String szERROR_BAD_MIME_MESSAGE = "Error: Bad mime message";
    final public static String szERROR_BAD_EXTERNAL_MESSAGE_PART = "Error:  No External headers in Message/external-body";
    final public static String szERROR_UNSUPPORTED_PARTIAL_SUBTYPE = "Error: Unsupported Partial SubType";
    final public static String szINVALID_CODE = "is an invalid code";
     protected static final int BUFSZ  = 4096;

	private static final byte base64map [] =
        {//      0   1   2   3   4   5   6   7
                'A','B','C','D','E','F','G','H', // 0
                'I','J','K','L','M','N','O','P', // 1
                'Q','R','S','T','U','V','W','X', // 2
                'Y','Z','a','b','c','d','e','f', // 3
                'g','h','i','j','k','l','m','n', // 4
                'o','p','q','r','s','t','u','v', // 5
                'w','x','y','z','0','1','2','3', // 6
                '4','5','6','7','8','9','+','/'  // 7
        };

	private static final byte hexmap [] =
	{
		 '0', '1', '2', '3', '4', '5', '6', '7',
    		 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};

	private static final byte CR      = '\r';
	private static final byte LF      = '\n';
	private static final byte EQ      = '=';
	private static final byte HT      = '\t';
	private static final byte[] CRLF  = "\r\n".getBytes();
	private static final byte[] EQCRLF = "=\r\n".getBytes();
	private static final byte[] EQQ    = "=?".getBytes();
	private static final byte[] QEQ    = "?=".getBytes();
	private static final byte[] QB64Q  = "?B?".getBytes();
	private static final byte[] QQPQ   = "?Q?".getBytes();


    // Generate base64 decode mapping
    private static byte[] Base64DecMap;
    static
    {
    	Base64DecMap = new byte[128];

    	for ( int idx = 0; idx < base64map.length; idx++ )
    	    Base64DecMap[ base64map[idx] ] = (byte) idx;
    }

//============= BEGIN NEW ONE

	/**
         * Base64 Encodes data from InputStream and writes to OutputStream.
         * @param input InputStream that supplies the data to be encoded.
         * @param output OutputStream that accepts the encoded data.
         * @return Number of bytes written.
         * @exception MIMEException If an encoding error occurs.
	 * @exception IOException If an I/O error occurs.
         */
	public static long encodeBase64 (InputStream input,
					  OutputStream output) throws MIMEException, IOException
	{
		byte buf[] = new byte [3];
		byte a, b, c;
		int read =0, offset=0, nullCount = 0, linelen = 0, written = 0, readlen = 0, leftover=0;
		byte l_bufenc[] = new byte [80];
		byte l_readbuf[] = new byte [BUFSZ];
		int triggerCount = 0;


		while (true)
		{
		    readlen = input.read (l_readbuf, 0, BUFSZ);

		    // trim off the last potential nulls
		    if (readlen < BUFSZ && readlen > 0)
		    {
			    while (l_readbuf [readlen-1] == 0x00)
			    {
				readlen--;
				if (readlen == 0)
				    break;
			        //System.out.println ("decrementing readlen =" + readlen);
			    }
		    }

		    if (readlen <= 0)
		    {
			if (leftover == 0)
			{
		            break;
			}
			else if (leftover == 2)
			{
				a = buf[0];
            			b = buf[1];
            			c = 0;
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);
            			l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			l_bufenc[linelen++] = (base64map[((b << 2) & 0x3c) + ((c >>> 6) & 0x3)]);
            			l_bufenc[linelen++] = EQ;
				written += 4;
		                break;
			}
			else if (leftover == 1)
			{
				a = buf[0];
            			b = 0;
            			c = 0;
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);
            			l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			l_bufenc[linelen++] = EQ;
            			l_bufenc[linelen++] = EQ;
				written += 4;
		                break;
			}
		    }

		    offset = 0;

		    while (readlen > 0)
		    {
			if (leftover > 0)
			{
				if (leftover == 2)
				{
					buf [2] = l_readbuf [offset++];
					readlen -= 1;
					read = 3;
					leftover = 0;
				}
				else if (leftover == 1)
				{
					buf [1] = l_readbuf [offset++];
					readlen -= 1;
					if (readlen > 0)
					{
					    buf [2] = l_readbuf [offset++];
					    readlen -= 1;
					    read = 3;
					    leftover = 0;
					}
					else
					{
				             leftover = 2;
					     continue;
					}
				}

			}
			else if (readlen >= 3)
			{
				buf [0] = l_readbuf [offset++];
				buf [1] = l_readbuf [offset++];
				buf [2] = l_readbuf [offset++];
				readlen -= 3;
				read = 3;
				leftover = 0;
			}
			else if  (readlen == 2)
			{
				buf [0] = l_readbuf [offset++];
				buf [1] = l_readbuf [offset++];
				readlen = 0;
				leftover = 2;
				continue;
			}
			else if  (readlen == 1)
			{
				buf [0] = l_readbuf [offset++];
				readlen = 0;
				leftover = 1;
				continue;
			}

			if ((read == 3 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00) ||
			    (read == 2 && buf[0] == 0x00 && buf[1] == 0x00) ||
		    	    (read == 1 && buf[0] == 0x00))
			{
				nullCount += read;
			}
			else
				nullCount = 0;

			//if (read == 3 && !(nullCount > 100))
			if (read == 3)
			{
				a = buf[0];
            			b = buf[1];
            			c = buf[2];
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);
            			l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			l_bufenc[linelen++] = (base64map[((b << 2) & 0x3c) + ((c >>> 6) & 0x3)]);
            			l_bufenc[linelen++] = (base64map[c & 0x3F]);
				//linelen += 4;
				written += 4;
			}
			//else if (read == 2 && !(nullCount > 100))
			else if (read == 2)
			{
				a = buf[0];
            			b = buf[1];
            			c = 0;
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);

            			l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			l_bufenc[linelen++] = (base64map[((b << 2) & 0x3c) + ((c >>> 6) & 0x3)]);
            			l_bufenc[linelen++] = EQ;
				//linelen += 4;
				written += 4;
			}
			//else if (read == 1 && !(nullCount > 100))
			else if (read == 1)
			{
				a = buf[0];
            			b = 0;
            			c = 0;
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);
            			l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			l_bufenc[linelen++] = EQ;
            			l_bufenc[linelen++] = EQ;
				//linelen += 4;
				written += 4;
			}

			if (linelen > 71)
			{
				//output.write (LF);
				//written += 1;
				l_bufenc[linelen++] =  CR;
				l_bufenc[linelen++] =  LF;
				//linelen += 2;
				written += 2;
				output.write (l_bufenc, 0, linelen);
				linelen = 0;
			}

		    } // end while
		} // end outer while

		// write out the leftover stuff
		if (linelen > 0)
		{
			l_bufenc[linelen++] =  CR;
			l_bufenc[linelen++] =  LF;
			written += 2;
			//l_bufenc[linelen++] =  CR;
			//l_bufenc[linelen++] =  LF;
			//written += 4;
			//linelen += 4;
			output.write (l_bufenc, 0, linelen);

			triggerCount += linelen;
			if (triggerCount > 1000000)
			{
				System.gc();
				//output.flush();
				triggerCount = 0;
			}

			linelen = 0;
		}

		return (written);
	}
//============= END NEW ONE

//============= BEGIN OLD ONE
	/**
         * Base64 Encodes data from InputStream and writes to OutputStream.
         * @param input InputStream that supplies the data to be encoded.
         * @param output OutputStream that accepts the encoded data.
         * @return Number of bytes written.
         * @exception MIMEException If an encoding error occurs.
	 * @exception IOException If an I/O error occurs.
         */
	protected static long encodeBase64O (InputStream input,
					  OutputStream output) throws MIMEException, IOException
	{
		byte buf[] = new byte [3];
		byte a, b, c;
		int nullCount = 0, linelen = 0, written = 0, dummy[] = new int[2];
		byte l_bufenc[] = new byte [80];
		int triggerCount = 0;

		while (true)
		{
			int read = input.read (buf, 0, 3);

			if (read <= 0)
			{
			        break;
			}

			if ((read == 3 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00) ||
			    (read == 2 && buf[0] == 0x00 && buf[1] == 0x00) ||
		    	    (read == 1 && buf[0] == 0x00))
			{
				nullCount += read;
			}
			else
				nullCount = 0;

			//if (read == 3 && !(nullCount > 100))
			if (read == 3)
			{
				a = buf[0];
            			b = buf[1];
            			c = buf[2];
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);
            			l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			l_bufenc[linelen++] = (base64map[((b << 2) & 0x3c) + ((c >>> 6) & 0x3)]);
            			l_bufenc[linelen++] = (base64map[c & 0x3F]);
				//linelen += 4;
				written += 4;
			}
			//else if (read == 2 && !(nullCount > 100))
			else if (read == 2)
			{
				a = buf[0];
            			b = buf[1];
            			c = 0;
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);
				if (b != 0x00)
				{
            			   l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			   l_bufenc[linelen++] = (base64map[((b << 2) & 0x3c) + ((c >>> 6) & 0x3)]);
				}
				else
            			     l_bufenc[linelen++] = EQ;
            			l_bufenc[linelen++] = EQ;
				//linelen += 4;
				written += 4;
			}
			//else if (read == 1 && !(nullCount > 100))
			else if (read == 1)
			{
				a = buf[0];
            			b = 0;
            			c = 0;
            			l_bufenc[linelen++] = (base64map[(a >>> 2) & 0x3F]);
            			l_bufenc[linelen++] = (base64map[((a << 4) & 0x30) + ((b >>> 4) & 0xf)]);
            			l_bufenc[linelen++] = EQ;
            			l_bufenc[linelen++] = EQ;
				//linelen += 4;
				written += 4;
			}

			if (linelen > 71)
			{
				//output.write (LF);
				//written += 1;
				l_bufenc[linelen++] =  CR;
				l_bufenc[linelen++] =  LF;
				//linelen += 2;
				written += 2;
				output.write (l_bufenc, 0, linelen);
				linelen = 0;
			}

		} // end while

		// write out the leftover stuff
		if (linelen > 0)
		{
			l_bufenc[linelen++] =  CR;
			l_bufenc[linelen++] =  LF;
			l_bufenc[linelen++] =  CR;
			l_bufenc[linelen++] =  LF;
			//linelen += 4;
			written += 4;
			output.write (l_bufenc, 0, linelen);

			triggerCount += linelen;
			if (triggerCount > 1000000)
			{
				System.gc();
				//output.flush();
				triggerCount = 0;
			}

			linelen = 0;
		}

		return (written);
	}
//============= END OLD ONE


	/**
         * QuotedPrintable Encodes data from InputStream and writes to OutputStream.
         * @param input InputStream that supplies the data to be encoded.
         * @param output OutputStream that accepts the encoded data.
         * @return Number of bytes written.
         * @exception MIMEException If an encoding error occurs.
	 * @exception IOException If an I/O error occurs.
         */
	public static long encodeQP (InputStream input,
				     OutputStream output) throws MIMEException, IOException
        {
		byte current = (byte)0, previous = (byte)0;
		int read, linelen = 0, written = 0, lastspace = 0, nullCount = 0;
		byte l_bufenc[] = new byte [80];

		while (true)
		{
			read = input.read();

			if (read == -1)
			{
				if (linelen > 0)
				{
				    output.write (l_bufenc, 0, linelen);
				    output.write (CRLF);
				    written += 2;
				}

				return (written);
			}

			current = (byte) read;

			if (current == 0x00)
			{
				nullCount++;
			        previous = current;
				lastspace = 0;
				continue;
			}
			else if (nullCount > 0)
			{
				// write out all the nulls first and fall through to process current char.
				for (int idx = 1; idx <= nullCount ; idx++)
				{
					byte tmp = 0x00;
					l_bufenc [linelen++] = EQ;
					l_bufenc [linelen++] = (byte) hexmap [(tmp >>> 4) & 0xF];
					l_bufenc [linelen++] = (byte) hexmap [(tmp & 0xF)];
				//	l_bufenc [linelen++] = (byte)0x00;
				//	l_bufenc [linelen++] = (byte)0x00;
					written += 3;

					if (linelen > 74)
					{
						output.write (l_bufenc, 0, linelen);
						output.write (EQCRLF);
						written += 3;
						linelen = 0;
					}
				}

				previous = (byte) 0;
				nullCount = 0;
			}

			if ((current > ' ') && (current < 0x7F) && (current != '='))
			{
				// Printable chars
				//output.write ((byte) current);
				l_bufenc [linelen++] = (byte) current;
				//linelen += 1;
				written += 1;
				lastspace = 0;
			        previous = current;
			}
			else if ((current == ' ') || (current == HT))
			{
				//output.write ((byte) current);
				l_bufenc [linelen++] = (byte) current;
				//linelen += 1;
				written += 1;
				lastspace = 1;
			        previous = current;
			}
			else if ((current == LF) && (previous == CR))
			{
				// handled this already. Ignore.
			        previous = (byte) 0;
			}
			else if ((current == CR ) || (current == LF))
			{
				// Need to emit a soft line break if last char was SPACE/HT or
				// if we have a period on a line by itself.
				if ((lastspace == 1) || ((previous == '.') && (linelen == 1)))
				{
					l_bufenc [linelen++] = EQ;
					l_bufenc [linelen++] = CR;
					l_bufenc [linelen++] = LF;
					written += 3;
				}

				l_bufenc [linelen++] = CR;
				l_bufenc [linelen++] = LF;
				lastspace = 0;
				written += 2;
				output.write (l_bufenc, 0, linelen);
				previous = (byte) 0;
				linelen = 0;
				//output.write (CRLF);
			        //previous = current;
			}
			else if ( (current < ' ') || (current == '=') || (current >= 0x7F) )
			{
				// Special Chars
				//output.write ((byte) '=');
				//output.write ((byte) hexmap [(current >>> 4)]);
				//output.write ((byte) hexmap [(current & 0xF)]);
				l_bufenc [linelen++] = EQ;

				l_bufenc [linelen++] = (byte) hexmap [(current >>> 4) & 0xF];
				l_bufenc [linelen++] = (byte) hexmap [(current & 0xF)];
				lastspace = 0;
				//linelen += 3;
				written += 3;
				previous = current;
			}
			else
			{
				//output.write ((byte) current);
				l_bufenc [linelen++] = (byte) current;
				lastspace = 0;
				//linelen += 1;
				written += 1;
				previous = current;
			}

			if (linelen > 74)
			{

				output.write (l_bufenc, 0, linelen);
				output.write (EQCRLF);
				written += 3;
				linelen = 0;
				previous = (byte) 0;
			}

		} // while

        } //encodeQP


	/**
         * Q Encodes data from InputStream and writes to OutputStream.
         * @param input InputStream that supplies the data to be encoded.
         * @param output OutputStream that accepts the encoded data.
         * @return Number of bytes written.
         * @exception MIMEException If an encoding error occurs.
	 * @exception IOException If an I/O error occurs.
         */
	private static long encodeQ (InputStream input,
				     OutputStream output) throws MIMEException, IOException
        {
		byte current = (byte)0;
		boolean encode = false;
		int read, written = 0;

		while (true)
		{
			read = input.read();

			if (read == -1)
			{
				return (written);
			}

			current = (byte) read;

			if ((current >= 0x30 && current <= 0x39) ||      // 0-9
			    (current >= 0x41 && current <= 0x5A) ||      // A-Z
			    (current >= 0x61 && current <= 0x7A) ||      // a-z
			    (current == 0x21 || current == 0x2A) ||      // !*
			    (current == 0x2B || current == 0x2D) ||      // +-
			    (current == 0x2F || current == 0x5F))        // /_
			{
				encode = false;
			}
			else
				encode = true;

			if (encode)
			{
				// Special Chars
				output.write ((byte) '=');
				output.write ((byte) hexmap [(current >>> 4)]);
				output.write ((byte) hexmap [(current & 0xF)]);
				written += 3;
			}
			else
			{
				output.write ((byte) current);
				written += 1;
			}

		} // while
        }


    /**
     * String version. Base64 Decodes data from the input string.
     * @param  str The base64-encoded string.
     * @return The decoded string
     */
    protected final static String decodeBase64( String str )
    {
    	if (str == null)
    	    return  null;

//    	byte data[] = new byte[str.length()];
//    	str.getBytes(0, str.length(), data, 0);

	    return new String( decodeBase64( str.getBytes() ) );
    }


    /**
     * byte[] version. Base64 Decodes input bytes.
     * @param  data base64-encoded bytes.
     * @return the decoded bytes
     */
    protected final static byte[] decodeBase64( byte[] data )
    {
        return decodeBase64( data, data.length );
    }


    // byte[] version, ByteBuffer version
    // carsonl
    /**
     * byte[] version. Base64 Decodes input bytes of given length.
     * @param  data base64-encoded bytes.
     * @param  len length of base64-encoded bytes.
     * @return the decoded bytes
     */
    protected final static byte[] decodeBase64( byte[] data, int len )
    {
    	if (data == null)
    	    return null;

    	while (data[len-1] == '=')
    	    len--;

    	byte dest[] = new byte[len - data.length/4];


	    // ascii printable to 0-63 conversion
    	for (int idx = 0; idx <data.length; idx++)
	        data[idx] = Base64DecMap[data[idx]];

    	// 4-byte to 3-byte conversion
	    int sidx, didx;

    	for (sidx = 0, didx=0; didx < dest.length-2; sidx += 4, didx += 3)
	    {
    	    dest[didx]   = (byte) ( ((data[sidx] << 2) & 255) |
    			    ((data[sidx+1] >>> 4) & 003) );

    	    dest[didx+1] = (byte) ( ((data[sidx+1] << 4) & 255) |
    			    ((data[sidx+2] >>> 2) & 017) );

    	    dest[didx+2] = (byte) ( ((data[sidx+2] << 6) & 255) |
    			    (data[sidx+3] & 077) );
    	}

    	if (didx < dest.length)
    	    dest[didx]   = (byte) ( ((data[sidx] << 2) & 255) | ((data[sidx+1] >>> 4) & 003) );

    	if (++didx < dest.length)
    	    dest[didx]   = (byte) ( ((data[sidx+1] << 4) & 255) | ((data[sidx+2] >>> 2) & 017) );

    	return dest;
    }



	/**
         * QuotedPrintable Decodes data from InputStream and writes to OutputStream
         * @param input InputStream that supplies the data to be decoded.
         * @param output OutputStream that accepts the decoded data.
         * @exception MIMEException if a decoding error occurs.
         * @exception IOException if io error occurs.
         */
	public static void decodeQP (InputStream input, OutputStream output) throws MIMEException
        {
            byte inputBuffer[];

            try
            {
                inputBuffer = new byte[ input.available() + 1 ];
                input.read( inputBuffer );
                output.write( decodeQP( inputBuffer ) );
            }
            catch ( IOException e )
            {
	              throw new MIMEException ( e.getMessage() );
            }
            catch ( Exception e )
            {
	              throw new MIMEException ( e.getMessage() );
            }

	       return;
        }


    /**
     * String version. QP decodes data from the input string.
     * @param  str the QP-encoded string.
     * @return the decoded string
     */
    protected final static String decodeQP( String str ) throws MIMEException
    {
        String decodedString = null;

    	if (str == null)
    	    return  null;

//    	byte data[] = new byte[str.length()];
//    	str.getBytes(0, str.length(), data, 0);

        try
        {
            decodedString = new String( decodeQP( str.getBytes() ) );
        }
        catch( Exception e )
        {
            throw new MIMEException( e.getMessage() );
        }

	    return decodedString;
    }



    /**
     * byte[] version. QP decodes input bytes.
     * @param  bytesIn QP-encoded bytes.
     * @return the decoded bytes
     */
    protected final static byte[] decodeQP( byte[] bytesIn ) throws MIMEException
    {
        return decodeQP( bytesIn, bytesIn.length );
    }


    /**
     * byte[] version. QP decodes input bytes of given length.
     * @param  bytesIn QP-encoded bytes.
     * @param  len length of QP-encoded bytes.
     * @return the decoded bytes
     * @exception ParseException If a '=' is not followed by a valid
     *                           2-digit hex number or '\r\n'.
     */
    protected final static byte[] decodeQP( byte[] bytesIn, int len ) throws MIMEException
    {
    	if ( bytesIn == null )
    	    throw new MIMEException( szERROR_BAD_PARAMETER );

    	byte res[] = new byte[ (int) ( len * 1.1) ];
    	byte src[] = bytesIn;
        byte nl[]  = System.getProperty("line.separator", "\n").getBytes();

    	int last   = 0,
    	    j   = 0;

    	for (int i = 0; i < len; )
    	{
    	    byte ch = src[i++];

    	    if (ch == '=')
    	    {
        		if (src[i] == '\n'  ||  src[i] == '\r')
        		{					// Rule #5
        		    i++;

        		    if (src[i-1] == '\r'  && src[i] == '\n')
            			i++;
        		}

        		else					// Rule #1
        		{
        		    byte repl;
        		    int hi = Character.digit( (char) src[i], 16),
        			lo = Character.digit( (char) src[i+1], 16);

        		    if ((hi | lo) < 0)
        		    	throw new MIMEException( new String(src, i-1, 3) + szINVALID_CODE );
        		    else
        		    {
            			repl = (byte) (hi << 4 | lo);
            			i += 2;
        		    }

        		    res[j++] = repl;
        		}

    	    	last = j;
    	    }
    	    else if (ch == '\n'  ||  ch == '\r')	// Rule #4
    	    {
        		if (src[i-1] == '\r'  &&  src[i] == '\n')
        		    i++;

        		for (int idx = 0; idx < nl.length; idx++)
        		    res[last++] = nl[idx];

        		j = last;
    	    }

    	    else					// Rule #1, #2
    	    {
    		    res[j++] = ch;

    		    if (ch != ' '  &&  ch != '\t')		// Rule #3
    		        last = j;
    	    }

    	    if ( j > res.length - 5 )
    	    {
            	byte tmp[] = new byte[ res.length + 500 ];
            	System.arraycopy( res, 0, tmp, 0, res.length );
            	res = tmp;
    	    }
    	}

    	return res;
    }

	/**
         * Encodes a string that typically goes in a header but uses characters other than ASCII
         * per RFC2047. Can be used on unstructured rfc822 headers or comments of structured ones.
	 * The returned string can be passed to setHeader() method in MIMEMessage class.
         * @param inputString The string to encode.
         * @param charsetName Name of the charset the input string is in. If null uses iso-8859-1.
         * @param encoding_type Must "B" or "Q".
         * @return The encoded header string.
         * @exception MIMEException if an encoding error occurs.
         */
	public static String encodeHeader (String inputString,
	                                   String charsetName,
                                       String encoding_type) throws MIMEException, UnsupportedEncodingException
        {
		int l_encoding = -1; // 0 = b64, 1= Q
		int l_inlength = 0;
		byte [] l_inbuf;

		if (encoding_type.equalsIgnoreCase ("B"))
		     l_encoding = 0;
		else if (encoding_type.equalsIgnoreCase ("Q"))
		     l_encoding = 1;
		else
		     throw new MIMEException ("Invalid encoding_type: " + encoding_type);

		if (charsetName == null)
			l_inbuf = inputString.getBytes("iso-8859-1");
		else
			l_inbuf = inputString.getBytes(charsetName);

		l_inlength = l_inbuf.length;


		try
		{
		         ByteArrayInputStream ins  = new ByteArrayInputStream (l_inbuf);
		         ByteArrayOutputStream os  = new ByteArrayOutputStream (l_inlength * 2);

		         long enclen;
			 if (l_encoding == 0)  // base64
 			      enclen = encodeBase64 (ins, os);
			 else
 			      enclen = encodeQ (ins, os);

		         if (enclen <= 0)
		              throw new MIMEException (" Header Encoding error: " +
							 encoding_type);

		         //ByteString l_bs64 = new ByteString (os.toByteArray(), 0, (int)enclen-1);
			 //  hi is length of array not last index!  --------------------^^^^^^^^^^
		         ByteString l_bsenc = new ByteString (os.toByteArray(), 0, (int)enclen);
		         ByteBuffer l_bufenc = new ByteBuffer (l_bsenc);

		         ByteBuffer l_retbuf = new ByteBuffer ((int)enclen+20);
		         l_retbuf.append (EQQ);
		         l_retbuf.append (charsetName.getBytes());
			 if (l_encoding == 0)  // base64
		         	l_retbuf.append (QB64Q);
			 else
		         	l_retbuf.append (QQPQ);
		         l_retbuf.append (l_bsenc);
		         l_retbuf.append (QEQ);

		         return (new String (l_retbuf.buffer));
		}
		catch (Exception e)
		{
			 throw new MIMEException (e.getMessage());
		}
        }

	/**
         * Decodes a string encoded in RFC2047 format. If the string is not encoded
	 * returns the original string. Can be used on the header value returned by
	 * getHeader() and getAllHeaders() methods in MIMEMessage class.
         * @param inputString The string to decode.
         * @return The decoded header string.
         * @exception MIMEException If an decoding error occurs.
         */
	public static String decodeHeader (String inputString) throws MIMEException
    {
    	int len;
    	int i, j;
    	int nStart = 0;
    	int nEnd = 0;
    	String szEncodedText;
    	StringBuffer OutputBuffer = new StringBuffer();
    	String plainPart=null;
    	byte decodedText[] = null;
    	char achCharset[] = new char[16];
    	boolean bEncodeBase64 = false;
    	boolean bEncodeQP = false;
    	boolean bEncodeString = false;

    	if ( inputString == null )
    	    throw new MIMEException( szERROR_BAD_PARAMETER );

        len = inputString.length();

    	/* find start of encoding text */
    	for ( i = 0; i < len && inputString.charAt(i) != 0 ; i++ )
    	{
    		if ( inputString.charAt(i) == '=' && inputString.charAt(i+1) == '?' )
		{
			bEncodeString = true;
    			break;
		}
    	}

	if (!bEncodeString)
		return inputString;

	if (i > 0)
	{
		plainPart = inputString.substring (0, i);
	}

    	/* get charset */
    	for ( i += 2, j = 0; i < len && inputString.charAt(i) != '?'; i++, j++ )
    	{
    		achCharset[j] = inputString.charAt(i);
    	}

    	achCharset[j] = 0;
    	i++;	/* skip ? */

    	/* get encoding type */
    	if ( inputString.charAt(i) == 'Q' || inputString.charAt(i) == 'q' )
    		bEncodeQP = true;

    	else if ( inputString.charAt(i) == 'B' || inputString.charAt(i) == 'b' )
    		bEncodeBase64 = true;

	if (inputString.charAt(i+1) == '?')
    		i++;	/* skip ? */
    	nStart = ++i;

    	/* look for end of encoded text */
    	for ( j = 0;  i < len && inputString.charAt(i) != 0 ; i++, j++ )
    	{
    		if ( inputString.charAt(i) == '?' && i+1 < len && inputString.charAt(i+1) == '=' )
    		{
    			nEnd = i;
    			break;
    		}
    	}


    	if ( nEnd > 0 )
    	{
    		/* extract encoded text */
    		szEncodedText = inputString.substring( nStart, nStart + j );

			if ( bEncodeQP )
				decodedText = decodeQP( szEncodedText.getBytes() );

			else if ( bEncodeBase64 )
				decodedText = decodeBase64( szEncodedText.getBytes() );
    	}
	else 
	{
    	    throw new MIMEException ("Improper encoded String");
	}


	if (plainPart != null)
	{
		OutputBuffer.append (plainPart);	
	}

    	if ( decodedText != null )
	{
	    OutputBuffer.append (new String(decodedText));
	}

	if (nEnd+2 < len-1)
	{
	    String remainder = inputString.substring (nEnd+2, len);
	    String retString = decodeHeader (remainder);
	    if (retString != null)
	    	OutputBuffer.append (retString);	
	}

	return (OutputBuffer.toString());

    }



	/**
	 * Generates and returns a boundary string that can be used in multi-parts.
	 * @return The boundary string.
	 */
        public static String generateBoundary ()
        {
                Random l_rand = new Random (System.currentTimeMillis());
                long l_numboundary = l_rand.nextLong();
                String l_boundary = new String ("-----" + Long.toHexString(l_numboundary));
                return (l_boundary);
        }



	/**
	* Converts the input unicode ASCII printable chars to ASCII bytes.
        * @param inputString The string to convert.
	* @exception MIMEException If the input contains non-printable ASCII or non-ASCII chars.
	*/
	public static byte[] unicodeToASCII (String inputString) throws
							UnsupportedEncodingException, MIMEException
	{
		//byte [] l_bytebuf = inputString.getBytes("iso-8859-1");
		byte [] l_bytebuf = inputString.getBytes();

		for (int i=0, len = l_bytebuf.length; i < len; i++)
		{
			if ( (l_bytebuf[i] > 126  || l_bytebuf[i] < 32) &&
			     l_bytebuf[i] != CR  && l_bytebuf[i] != LF )
			   throw new MIMEException ("Invalid non-printable ASCII or non-ASCII character." + l_bytebuf[i]);
		}

		return (l_bytebuf);

	} // unicodeToASCII

        /**
	 * Based on file extension, attempts to determine the file MIME content-type
	 * sub-type, params and extension. By default returns application/octet-stream.
         * @param filename Filename in the form name.ext. If no extension returns application/octet-stream.
         * @return Object of class fileMIMEType that has the MIME content-type* info set.
	 */
	public static fileMIMEType getFileMIMEType (String filename)
	{
		String inStr, extn;
		int strLen, sepIndex;
		fileMIMEType fmt = new fileMIMEType();

		inStr = filename.trim();
		strLen = inStr.length();
		sepIndex = inStr.lastIndexOf ('.');
		extn =  inStr.substring (sepIndex+1, strLen);

		fmt.file_extn = extn;

		File ff = new File (filename);
		fmt.file_shortname = ff.getName();

		if (extn.equalsIgnoreCase ("txt") || extn.equalsIgnoreCase ("java") ||
		    extn.equalsIgnoreCase ("c")   || extn.equalsIgnoreCase ("C")    ||
                    extn.equalsIgnoreCase ("cc")  || extn.equalsIgnoreCase ("CC")   ||
		    extn.equalsIgnoreCase ("h")   || extn.equalsIgnoreCase ("hxx")  ||
                    extn.equalsIgnoreCase ("bat") || extn.equalsIgnoreCase ("rc")   ||
		    extn.equalsIgnoreCase ("ini") ||  extn.equalsIgnoreCase ("cmd") ||
		    extn.equalsIgnoreCase ("awk") || extn.equalsIgnoreCase ("html") ||
		    extn.equalsIgnoreCase ("sh")  || extn.equalsIgnoreCase ("ksh")  ||
		    extn.equalsIgnoreCase ("pl")  ||  extn.equalsIgnoreCase ("DIC") ||
		    extn.equalsIgnoreCase ("EXC") ||  extn.equalsIgnoreCase ("LOG") ||
		    extn.equalsIgnoreCase ("SCP") ||  extn.equalsIgnoreCase ("WT") ||
		    extn.equalsIgnoreCase ("mk")  || extn.equalsIgnoreCase ("htm"))  // what else?
                {
			fmt.content_type = MIMEBasicPart.TEXT;
			if (extn.equalsIgnoreCase ("html") || extn.equalsIgnoreCase ("htm"))
				fmt.content_subtype = "html";
			else
				fmt.content_subtype = "plain";
			fmt.content_params = "us-ascii";
			fmt.mime_encoding = MIMEBodyPart.E7BIT;
		}
		else if (extn.equals ("pdf")) // PDF is different
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "pdf";
		}
		else if (extn.equalsIgnoreCase ("AIF") || extn.equalsIgnoreCase ("AIFC") ||
		         extn.equalsIgnoreCase ("AIFF"))
		{
			fmt.content_type = MIMEBasicPart.AUDIO;
			fmt.content_subtype = "aiff";
		}
		else if (extn.equalsIgnoreCase ("AU") || extn.equalsIgnoreCase ("SND"))
		{
			fmt.content_type = MIMEBasicPart.AUDIO;
			fmt.content_subtype = "basic";
		}
		else if (extn.equalsIgnoreCase ("WAV"))
		{
			fmt.content_type = MIMEBasicPart.AUDIO;
			fmt.content_subtype = "wav";
		}
		else if (extn.equalsIgnoreCase ("gif") )
		{
			fmt.content_type = MIMEBasicPart.IMAGE;
			fmt.content_subtype = "gif";
		}
		else if (extn.equalsIgnoreCase ("jpg") )
		{
			fmt.content_type = MIMEBasicPart.IMAGE;
			fmt.content_subtype = "jpeg";
		}
		else if (extn.equalsIgnoreCase ("jpeg") )
		{
			fmt.content_type = MIMEBasicPart.IMAGE;
			fmt.content_subtype = "jpeg";
		}
		else if (extn.equalsIgnoreCase ("tif") )
		{
			fmt.content_type = MIMEBasicPart.IMAGE;
			fmt.content_subtype = "tiff";
		}
		else if (extn.equalsIgnoreCase ("XBM") )
		{
			fmt.content_type = MIMEBasicPart.IMAGE;
			fmt.content_subtype = "x-xbitmap";
		}
		else if (extn.equalsIgnoreCase ("avi") )
		{
			fmt.content_type = MIMEBasicPart.VIDEO;
			fmt.content_subtype = "avi";
		}
		else if (extn.equalsIgnoreCase ("mpeg") )
		{
			fmt.content_type = MIMEBasicPart.VIDEO;
			fmt.content_subtype = "mpeg";
		}
		else if (extn.equalsIgnoreCase ("ps") || extn.equalsIgnoreCase ("EPS"))
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "postscript";
		}
		else if (extn.equalsIgnoreCase ("tar") )
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "x-tar";
		}
		else if (extn.equalsIgnoreCase ("zip") )
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "zip";
		}
		else if (extn.equalsIgnoreCase ("js") )
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "x-javascript";
		}
		else if (extn.equalsIgnoreCase ("doc") )
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "msword";
		}
		else if (extn.equalsIgnoreCase ("nsc") )
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "x-conference";
		}
		else if (extn.equalsIgnoreCase ("ARC") || extn.equalsIgnoreCase ("ARJ") ||
			 extn.equalsIgnoreCase ("B64") || extn.equalsIgnoreCase ("BHX") ||
			 extn.equalsIgnoreCase ("GZ") || extn.equalsIgnoreCase ("HQX"))
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "x-gzip";
		}
		else
		{
			fmt.content_type = MIMEBasicPart.APPLICATION;
			fmt.content_subtype = "octet-stream";
		}

		return (fmt);
	}


/*
	protected static byte[] decodeQPVector( Vector v, int nStart, int nEnd, int nMessageLen, int[] param ) throws MIMEException
	{
		int	i, j, nLen = 0, nTotalLen = 0, last = 0, jj = 0;
        byte[] line;
        byte[] output;
        byte nl[]  = System.getProperty("line.separator", "\n").getBytes();

		if ( v == null || param == null )
    	    throw new MIMEException( szERROR_BAD_PARAMETER );

        // get message len
		for ( j = nStart; j <= nEnd; j++ )
		{
			line = (byte[]) v.elementAt( j );
			nTotalLen = nTotalLen + line.length;
        }

        output = new byte[ (int)( nTotalLen * 1.1 ) ];

		if ( output == null )
    	    throw new MIMEException( "Not enough memory" );

        // read off data from vector line by line
		for ( j = nStart; j <= nEnd; j++ )
		{
			line = (byte[]) v.elementAt( j );
			nLen = line.length;

        	for ( i = 0; i < nLen; )
        	{
        	    byte ch = line[i++];

        	    if (ch == '=')
        	    {
            		if (line[i] == '\n'  ||  line[i] == '\r')
            		{					// Rule #5
            		    i++;

            		    if (line[i-1] == '\r'  && line[i] == '\n')
                			i++;
            		}

            		else					// Rule #1
            		{
            		    byte repl;
            		    int hi = Character.digit( (char) line[i], 16),
            			lo = Character.digit( (char) line[i+1], 16);

            		    if ((hi | lo) < 0)
            		    	throw new MIMEException(new String(line, i-1, 3) + "is an invalid code");
            		    else
            		    {
                			repl = (byte) (hi << 4 | lo);
                			i += 2;
            		    }

            		    output[j++] = repl;
            		}

        	    	last = jj;
        	    }
        	    else if (ch == '\n'  ||  ch == '\r')	// Rule #4
        	    {
            		if (line[i-1] == '\r'  &&  line[i] == '\n')
            		    i++;

            		for (int idx = 0; idx < nl.length; idx++)
            		    output[last++] = nl[idx];

            		jj = last;
        	    }

        	    else					// Rule #1, #2
        	    {
        		    output[jj++] = ch;

        		    if (ch != ' '  &&  ch != '\t')		// Rule #3
        		        last = jj;
        	    }

        	    if ( jj > output.length - 5 )
        	    {
                	byte tmp[] = new byte[ output.length + 500 ];
                	System.arraycopy( output, 0, tmp, 0, output.length );
                	output = tmp;
        	    }

        	}  // for each line

        }   // for vector

        param[2] = jj;  // len

	    return output;
	}
*/

/*
* carsonl, jan 8,98
* hex to decimal
*
* parameter :
*
*	int nFirstByte : first hex byte
*	int nSecondByte : second hex byte
*
* returns : decimal
*/
static byte nConvertHexToDec( int nFirstByte, int nSecondByte )
{
	byte nValue = 0;

	switch ( nFirstByte )
	{
		case ' ':
		case '0':	nValue += 0; break;
		case '1':	nValue += 16; break;
		case '2':	nValue += 32; break;
		case '3':	nValue += 48; break;
		case '4':	nValue += 64; break;
		case '5':	nValue += 80; break;
		case '6':	nValue += 96; break;
		case '7':	nValue += 112; break;
		case '8':	nValue += 128; break;
		case '9':	nValue += 144; break;
		case 'A':
		case 'a':	nValue += 160; break;
		case 'B':
		case 'b':	nValue += 176; break;
		case 'C':
		case 'c':	nValue += 192; break;
		case 'D':
		case 'd':	nValue += 208; break;
		case 'E':
		case 'e':	nValue += 224; break;
		case 'F':
		case 'f':	nValue += 240; break;
	}

	switch ( nSecondByte )
	{
		case ' ':
		case '0':	nValue += 0; break;
		case '1':	nValue += 1; break;
		case '2':	nValue += 2; break;
		case '3':	nValue += 3; break;
		case '4':	nValue += 4; break;
		case '5':	nValue += 5; break;
		case '6':	nValue += 6; break;
		case '7':	nValue += 7; break;
		case '8':	nValue += 8; break;
		case '9':	nValue += 9; break;
		case 'A':
		case 'a':	nValue += 10; break;
		case 'B':
		case 'b':	nValue += 11; break;
		case 'C':
		case 'c':	nValue += 12; break;
		case 'D':
		case 'd':	nValue += 13; break;
		case 'E':
		case 'e':	nValue += 14; break;
		case 'F':
		case 'f':	nValue += 15; break;
	}

	return nValue;
}


protected static byte[] decodeQPVectorNew (Vector v, int nStart, int nEnd,
			                int[] param, byte[] leftOverBytes) throws MIMEException
{
	int i = 0, j = 0, ii = 0, nLen = 0, written=0;
	byte[] line, buffer, retbuf;
	byte[] token = new byte [3];
	int nNoOfLeftOverBytes;

	if ( v == null || param == null )
	    throw new MIMEException( szERROR_BAD_PARAMETER );

    	if ( nStart == -1 || nEnd == -1 )
       	    return null;

	nNoOfLeftOverBytes = param[0];

        // get message len
	nLen = nNoOfLeftOverBytes;
	for (j = nStart; j <= nEnd; j++)
	{
		line = (byte[]) v.elementAt (j);
		nLen = nLen + line.length;
        }

        j = 0;
        buffer = new byte[(int) (nLen * 1.1)];

	if (buffer == null)
	    throw new MIMEException( szERROR_OUT_OF_MEMORY );

        nLen = 0;
	/* get each line */
	for ( ii = nStart; ii <= nEnd; ii++ )
	{
		line = (byte[]) v.elementAt(ii);

		if (ii == nStart && nNoOfLeftOverBytes > 0)
		{
		    System.arraycopy (leftOverBytes, 0, buffer, 0, nNoOfLeftOverBytes);
		    nLen += nNoOfLeftOverBytes;
		}

        	if (line == null || line.length == 0)
            		continue;
	        System.arraycopy (line, 0, buffer, nLen, line.length);
		nLen += line.length;
	}

	written = 0; i=0;
	int read_offset = 0;

	/* first time do it outside the loop */
        while (i < 3 && read_offset < nLen)
        {
		token [i++] =  buffer [read_offset++];
        }

	while (read_offset < nLen || i != 0)
	{
		 while (i < 3 && read_offset < nLen)
		 {
			token [i++] =  buffer [read_offset++];
		 }

		 if (i < 3) /* did not get enough for a token */
		 {
		     if (i == 2 && token [0] == CR && token [1] == LF)
		     {
			 buffer[written++] = token[0];
			 buffer[written++] = token[1];
		         nNoOfLeftOverBytes = 0;
		     }
		     else
		     {
		          for (j=0; j < i; j++)
			      leftOverBytes [j] = token [j];
		          nNoOfLeftOverBytes = i;
		     }

    		     param[0] = nNoOfLeftOverBytes;

		     if (written > 0)
		     {
			retbuf = new byte [written];
	        	System.arraycopy (buffer, 0, retbuf, 0, written);
		        return (retbuf);
		     }
		 }

		i = 0;

	  	if (token [0] == '=')
		{
		  byte c = (byte)0;
		  if ((token[1] >= '0' && token[1] <= '9') ||
		      (token[1] >= 'A' && token[1] <= 'F') ||
		      (token[1] >= 'a' && token[1] <= 'f'))
			c = token[1];
		  else if (token[1] == CR || token[1] == LF)
		  {
		        /* =\n means ignore the newline. */
		        if (token[1] == CR && token[2] == LF)
				;		/* swallow all three chars */
			else
			{
			     read_offset--;	/* put the third char back */
			}
			continue;
		  }
		  else
		  {
		       /* = followed by something other than hex or newline -
			 pass it through unaltered, I guess.
			*/
			if (read_offset > written) { buffer[written++] = token[0]; }
			if (read_offset > written) { buffer[written++] = token[1]; }
			if (read_offset > written) { buffer[written++] = token[2]; }
			continue;
		  }

		  /* Second hex digit */
		  if ((token[2] >= '0' && token[2] <= '9') ||
		      (token[2] >= 'A' && token[2] <= 'F') ||
		      (token[2] >= 'a' && token[2] <= 'f'))
		  	  c = (byte) ((c << 4) | token[2]);
		  else
		  {
			/* We got =xy where "x" was hex and "y" was not, so
			   treat that as a literal "=", x, and y. */
			if (read_offset > written) { buffer[written++] = token[0];}
			if (read_offset > written) { buffer[written++] = token[1];}
			if (read_offset > written) { buffer[written++] = token[2];}
			continue;
		  }

		  buffer[written++] = c;
		}
	  	else
		{
		  buffer[written++] = token[0];

		  token[0] = token[1];
		  token[1] = token[2];
		  i = 2;
		}
	}

        param[0] = 0; // nothing leftover

	if (written > 0)
	{
		retbuf = new byte [written];
		System.arraycopy (buffer, 0, retbuf, 0, written);
		return (retbuf);
	}

	return null;
}

/*
* carson, Jan 8,98
* QuotedPrintable Decodes data from vector, write decoded data to szOutput
*
* params :
*
*	int nStart : starting element in vector
*	int nEndt  : ending element in vector
*	Vector *v  : vector
*	char *szOutput : output buffer
*	int MaxBufferSize : max buffer size
*
* return : number of decoded bytes
*/
protected static byte[] decodeQPVector( Vector v, int nStart, int nEnd, int[] param, byte[] leftOverBytes ) throws MIMEException
{
	byte ch, ch2;
	int i = 0;
	int j = 0;
	int k = 0;
	int	ii, nLen=0;
	byte[] line;
	byte[] output;
	byte[] buffer;
	boolean bAppendLeftOverBytes = false;
	int nNoOfLeftOverBytes;
    int hi = 0, lo = 0, last = 0;
    char nl[] = System.getProperty("line.separator", "\n").toCharArray();

	if ( v == null || param == null )
	    throw new MIMEException( szERROR_BAD_PARAMETER );

    if ( nStart == -1 || nEnd == -1 )
        return null;

	nNoOfLeftOverBytes = param[0];

    // get message len
	for ( j = nStart; j <= nEnd; j++ )
	{
		line = (byte[]) v.elementAt( j );
		nLen = nLen + line.length;
    }

    output = new byte[ (int) ( nLen * 1.1 ) ];
    buffer = new byte[ 128 ];
    j = 0;

	if ( output == null || buffer == null )
	    throw new MIMEException( szERROR_OUT_OF_MEMORY );

	/* get each line */
	for ( ii = nStart; ii <= nEnd; ii++ )
	{
		line = (byte[]) v.elementAt(ii);

		if ( !bAppendLeftOverBytes )
		{
			if ( nNoOfLeftOverBytes > 0 )
			{
			    System.arraycopy( leftOverBytes, 0, buffer, 0, nNoOfLeftOverBytes );
			    System.arraycopy( line, 0, buffer, nNoOfLeftOverBytes, line.length );
			    line = buffer;
			}

			bAppendLeftOverBytes = false;
		}

        if ( line.length == 0 )
            continue;

		/* enumerate through each char */
//		for ( i = 0, ch = line[i]; ch != 0 && i < line.length; ch = line[++i] )

/* ---------------- old ---------------------------
		for ( i = 0; i < line.length; i++ )
		{
		    ch = line[i];

			if ( ( ch >= 33 && ch <= 60 ) || ( ch >= 62 && ch <= 126 ) || ( ch == 9 || ch == 32 ) )
			{
				output[j++] = ch;
			}

			else if ( ch == '=' )
			{
				if ( i + 2 < line.length )
				{
					output[j++] = nConvertHexToDec( line[++i], line[++i] );
				}

				// get more data
				else if ( ii < nEnd )
				{

				    int len2 = line.length - i - 1;
				    System.arraycopy( line, i, buffer, 0, len2 );
        		    line = (byte[]) v.elementAt(++ii);
        		    System.arraycopy( line, 0, buffer, len2, line.length );
        		    line = buffer;
        		    i = -1;
				}

				// save it for next time
				else
				{
					leftOverBytes[0] = ch; nNoOfLeftOverBytes = 1;

					if ( i+1 < line.length )
						leftOverBytes[1] = ch; nNoOfLeftOverBytes = 2;
				}
			}
		}
----------- old ------------------------------------ */


/* -------------------------- new ---------------------*/

    	for ( i = 0; i < line.length; )
    	{
    	    ch = line[i++];

    	    if (ch == '=')
    	    {
        		if ( i < line.length && ( line[i] == '\n' || line[i] == '\r' ) )
        		{					// Rule #5
        		    i++;

        		    if ( i < line.length && line[i-1] == '\r' && line[i] == '\n')
            			i++;
        		}

        		else if ( i + 1 < line.length )					// Rule #1
        		{
        		    hi = Character.digit( (char) line[i], 16);
        			lo = Character.digit( (char) line[i+1], 16);

if ( hi < 0 )
    hi = 0;
if ( lo < 0 )
    lo = 0;

        		    {
        			    ch2 = (byte) (hi << 4 | lo);
            			i += 2;
        		    }

        		    output[j++] = ch2;
        		}

        		// get more data
        		else if ( ii < nEnd )
        		{
				    int len2 = line.length - i;
				    System.arraycopy( line, i, buffer, 0, len2 );
        		    line = (byte[]) v.elementAt(++ii);
        		    System.arraycopy( line, 0, buffer, len2, line.length );
        		    line = buffer;
        		}

        		last = j;
    	    }

    	    else if (ch == '\n'  ||  ch == '\r')	// Rule #4
    	    {
        		if ( ch == '\n' && i >= line.length - 1 && i >= 3 && line[i-3] != '=' )
        		{
               		output[last++] = 10;
                }

        		else if ( ch == '\r' )
        		{
        		    if ( i < line.length && line[i-1] == '\r'  &&  line[i] == '\n')
        		       i++;

               		output[last++] = 13;
                }

/*
        		for ( k = 0; k < nl.length; k++ )
        		{
        		    output[last++] = (byte) nl[k];

        		}
*/
        		j = last;
    	    }

    	    else					// Rule #1, #2
    	    {
        		output[j++] = ch;

        		if (ch != ' '  &&  ch != '\t')		// Rule #3
        		    last = j;
    	    }

//    	    if (j > output.length-5)
//        		output = Util.outputizeArray(output, output.length+500);
    	}
/*-------------------------------------------------------------- */

	}

    param[0] = nNoOfLeftOverBytes;
    param[1] = j;
    buffer = null;

	return output;
}



	/**
     * Base64 Decodes data from InputStream and writes to OutputStream.
     * @param input InputStream that supplies the data to be decoded.
     * @param output OutputStream that accepts the decoded data.
     * @exception MIMEException If a decoding error occurs.
	 * return Number of decoded bytes
     */
	protected static byte[] decodeBase64Vector( Vector v, int nStart, int nEnd, int nMessageLen, int param[] ) throws MIMEException
	{
		int				add_bits;
		int				mask;
		int				out_byte = 0;
		int				out_bits = 0;
		int				byte_pos = 0;
		int				i, j, nLen = 0, nTotalLen = 0;
		byte[]			szLine;
        byte[]          output;

		if ( v == null || param == null )
    	    throw new MIMEException( szERROR_BAD_PARAMETER );

        // restore previous state
        out_byte = param[0];
        out_bits = param[1];

        // invalid range
        if ( nStart == -1 || nEnd == -1 )
            return null;

        // get message len
		for ( j = nStart; j <= nEnd; j++ )
		{
			szLine = (byte[]) v.elementAt( j );
			nTotalLen = nTotalLen + szLine.length;
        }

        output = new byte[ nTotalLen ];

		if ( output == null )
    	    throw new MIMEException( szERROR_OUT_OF_MEMORY );

        // read off data from vector line by line
		for ( j = nStart; j <= nEnd; j++ )
		{
			szLine = (byte[]) v.elementAt( j );
			nLen = szLine.length;

			// process line
			for ( i = 0; i < nLen && szLine[i] != 0; i++ )
			{
				add_bits = Base64DecMap[(char) szLine[i]];

				if (add_bits >= 64)
					continue;

				out_byte = (out_byte << 6) + add_bits;
				out_bits += 6;

				// If the queue has gotten big enough, put into the buffer
				if (out_bits >= 24)
				{
				    if ( byte_pos < nTotalLen - 3 )
				    {
    					output[byte_pos++] = (byte) ( (out_byte & 0xFF0000) >> 16 );
    					output[byte_pos++] = (byte) ( (out_byte & 0x00FF00) >> 8 );
    					output[byte_pos++] = (byte) ( (out_byte & 0x0000FF) );
    					out_bits = 0;
    					out_byte = 0;
    			    }
    			    else
    			    {
    			        System.out.println( "byte_pos = " + byte_pos );
    			        break;
    			    }
				}
			}
		}

        // save current state
        param[0] = out_byte;
        param[1] = out_bits;
        param[2] = byte_pos;

        return output;
	}



	protected static byte[] decodeBase64LeftOverBytes( int out_bits, int out_byte )
	{
		int		mask;
        byte[]  output = new byte[4];
        int     byte_pos = 0;

		/* Handle any bits still in the queue */
		while (out_bits >= 8)
		{
			if (out_bits == 8)
			{
				output[byte_pos++] = (byte) out_byte;
				out_byte = 0;
			}

			else
			{
				mask = out_bits == 8 ? 0xFF : (0xFF << (out_bits - 8));
				output[byte_pos++] = (byte) ( (out_byte & mask) >> (out_bits - 8) );
				out_byte &= ~mask;
			}

			out_bits -= 8;
		}

        return byteSubstring( output, 0, byte_pos );
    }



    /**
     *
     * Base64 Decodes data from InputStream and writes to OutputStream
     * @param input InputStream that supplies the data to be decoded.
     * @param output OutputStream that accepts the decoded data.
     * @exception MIMEException if a decoding error occurs.
     * return number of decoded bytes
     */
	public static void decodeBase64( InputStream input, OutputStream output ) throws MIMEException
	{
		int				add_bits;
		int				mask;
		int				out_byte = 0;
		int				out_bits = 0;
		int				byte_pos = 0;
		int				i, nLen;

		if ( input == null || output == null )
    	    throw new MIMEException( szERROR_BAD_PARAMETER );

        try
        {
    		/* Queue up relevant bits */
    		for ( i = 0, nLen = input.available(); i < nLen; i++ )
    		{
      			add_bits = Base64DecMap[(char) input.read() ];

    			if (add_bits >= 64)
    				continue;

    			out_byte = (out_byte << 6) + add_bits;
    			out_bits += 6;

    			/* If the queue has gotten big enough, put into the buffer */
    			if (out_bits == 24)
    			{
    				output.write( (byte) ( (out_byte & 0xFF0000) >> 16 ) );
    				output.write( (byte) ( (out_byte & 0x00FF00) >> 8 ) );
    				output.write( (byte) ( (out_byte & 0x0000FF) ) );

    				byte_pos =+ 3;
    				out_bits = 0;
    				out_byte = 0;
    			}
    		}

    		/* Handle any bits still in the queue */
    		while ( out_bits >= 8 )
    		{
    			if (out_bits == 8)
    			{
    				output.write( (byte) out_byte );
    				byte_pos++;
    				out_byte = 0;
    			}

    			else
    			{
    				mask = out_bits == 8 ? 0xFF : (0xFF << (out_bits - 8));
    				output.write( (byte) ( (out_byte & mask) >> (out_bits - 8) ) );
    				byte_pos++;
    				out_byte &= ~mask;
    			}

    			out_bits -= 8;
    		}
    	}
        catch ( IOException e )
        {
            throw new MIMEException ( e.getMessage() );
        }

        return;
	}



    /* ------------------- utility functions ------------------
    * determine if both strings are the same based on the length of the second string
    * case insensitive
    * clee, oct 6,97
    * return TRUE if equale
    */
    protected static boolean bStringEquals( String s1, String s2 )
    {
        char ch;

    	if ( s1 != null && s2 != null )
    	{
    		int len = s2.length();

    		if ( len > s1.length() )
    		    return false;

    		for ( int i = 0; i < len; i++ )
    		{
    		    ch = s2.charAt(i);

    		    // lowercase
    		    if ( ch >= 97 )
    		    {
    		        if ( s1.charAt(i) != ch && s1.charAt(i) != ( ch - 32 ) )
       		           return false;
                }
    		    else if ( s1.charAt(i) != ch && s1.charAt(i) != ( ch + 32 ) )
    		        return false;
    		}

    		return true;
    	}

    	return false;
    }


    /* ------------------- utility functions ------------------
    * determine if both strings are the same based on the length of the second string
    * case insensitive
    * clee, oct 6,97
    * return TRUE if equale
    */
    protected static boolean bStringEquals( byte s1[], String s2 )
    {
        char ch;

    	if ( s1 != null && s2 != null )
    	{
    		int len = s2.length();

    		if ( len > s1.length )
    		    return false;

    		for ( int i = 0; i < len; i++ )
    		{
    		    ch = s2.charAt(i);

    		    // lowercase
    		    if ( ch >= 97 )
    		    {
    		        if ( s1[i] != ch && s1[i] != ( ch - 32 ) )
           		        return false;
                }
    		    else if ( s1[i] != ch && s1[i] != ( ch + 32 ) )
    		        return false;
    		}

    		return true;
    	}

    	return false;
    }

    protected static boolean bStringEqualsLtrim( byte s1[], String s2 )
    {
        char ch;

    	if ( s1 != null && s2 != null )
    	{
		int j=0;

    		int len = s2.length();

    		if ( len > s1.length )
    		    return false;

    		for (j = 0; j < len; j++)
		{
			if (s1[j] == '\n' || s1[j] == '\r' || s1[j] == '\t' || s1[j] == ' ');
			else
			{
			     break;
			}
		}

		if (j > 0)
		{
			return (bStringEquals(j+1, s1, s2));
		}

    		for ( int i = 0; i < len; i++ )
    		{
    		    ch = s2.charAt(i);

    		    // lowercase
    		    if ( ch >= 97 )
    		    {
    		        if ( s1[i] != ch && s1[i] != ( ch - 32 ) )
           		        return false;
                }
    		    else if ( s1[i] != ch && s1[i] != ( ch + 32 ) )
    		        return false;
    		}

    		return true;
    	}

    	return false;
    }


    /* ------------------- utility functions ------------------
    * determine if both strings are the same based on the length of the second string
    * case insensitive
    * clee, oct 6,97
    * return TRUE if equale
    */
    protected static boolean bStringEquals( byte s1[], int s1_len, String s2 )
    {
        char ch;

    	if ( s1 != null && s2 != null )
    	{
    		int len = s2.length();

    		if ( len > s1_len )
    		    return false;

    		for ( int i = 0; i < len; i++ )
    		{
    		    ch = s2.charAt(i);

    		    // lowercase
    		    if ( ch >= 97 )
    		    {
    		        if ( s1[i] != ch && s1[i] != ( ch - 32 ) )
           		        return false;
                }
    		    else if ( s1[i] != ch && s1[i] != ( ch + 32 ) )
    		        return false;
    		}

    		return true;
    	}

    	return false;
    }


    /* ------------------- utility functions ------------------
    * determine if both strings are the same based on the length of the second string
    * case insensitive
    * clee, oct 6,97
    * return TRUE if equale
    */
    protected static boolean bStringEquals( int offset, byte s1[], String s2 )
    {
        char ch;

    	if ( s1 != null && s2 != null )
    	{
    		int len = s2.length();

    		if ( len > s1.length )
    		    return false;

    		for ( int i = 0; i < len; i++ )
    		{
    		    ch = s2.charAt(i);

    		    // lowercase
    		    if ( ch >= 97 )
    		    {
    		        if ( s1[i+offset] != ch && s1[i+offset] != ( ch - 32 ) )
           		        return false;
                }
    		    else if ( s1[i+offset] != ch && s1[i+offset] != ( ch + 32 ) )
    		        return false;
    		}

    		return true;
    	}

    	return false;
    }


    /* ------------------- utility functions ------------------
    * determine if both strings are the same based on the length of the second string
    * case insensitive
    * clee, oct 6,97
    * return TRUE if equale
    */
    protected static boolean bStringEquals( byte s1[], byte s2[] )
    {
        char ch;

    		if ( s2.length > s1.length )
    		    return false;

    	if ( s1 != null && s2 != null )
    	{
    		for ( int i = 0; i < s2.length; i++ )
    		{
    		    // lowercase
    		    if ( s2[i] >= 97 )
    		    {
    		        if ( s1[i] != s2[i] && s1[i] != ( s2[i] - 32 ) )
           		        return false;
                }
    		    else if ( s1[i] != s2[i] && s1[i] != ( s2[i] + 32 ) )
    		        return false;
    		}

    		return true;
    	}

    	return false;
    }

    protected static byte[] stringToByte( String s ) { return s.getBytes(); }


    protected static byte[] stringToByte( char[] s, int len )
    {
        byte b[] = new byte[ len ];

        for ( int i = 0; i < len; i++ )
            b[i] = (byte) s[i];

        return b;
    }

    protected static byte[] byteSubstring( byte[] s, int len )
    {
        byte b[] = new byte[ len ];

        for ( int i = 0; i < len; i++ )
            b[i] = s[i];

        return b;
    }

    protected static byte[] byteSubstring( byte[] s, int offset, int len )
    {
        byte b[] = new byte[ len ];

        for ( int i = 0, j = offset; i < len && j < s.length; i++, j++ )
            b[i] = s[j];

        return b;
    }


//----------------------------------------------------------------------------------------

} // End class
