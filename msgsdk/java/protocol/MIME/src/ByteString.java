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

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * ByteString is similar to java.lang.String, except that it contains
 * bytes instead of characters.
 */
public class ByteString extends Object implements Cloneable {
    protected   byte[]  buffer;
    protected   int     lo;
    protected   int     hi;

    protected   static byte CR      = '\r';
    protected   static byte LF      = '\n';
    protected   static byte[] CRLF  = "\r\n".getBytes();

    protected ByteString () {
	buffer = null;
	lo = 0;
	hi = 0;
    }

    public ByteString (String string) {
        buffer  = string.getBytes();
        lo      = 0;
        hi      = buffer.length;
    }

    public ByteString (byte[] buffer) {
        this.buffer = buffer;
        this.lo     = 0;
        this.hi     = buffer.length;
    }

    public ByteString (byte[] buffer, int lo, int hi) {
	if (lo > hi) throw new IndexOutOfBoundsException();
        this.buffer = buffer;
        this.lo     = lo;
        this.hi     = hi;
    }

    /**
     * hashCode() -- Overrides java.lang.Object.hashCode()
     */
    public final int hashCode () {
        int result = 0;
	// TODO: improve hash function
        for (int i = lo; i < hi; i++) {
            result += buffer[i];
        }
        return result;
    }

    /**
     * equals() -- Overrides java.lang.Object.equals()
     */
    public final boolean equals (Object that) {
        if (this == that)
            return true;
        else
            return equals(((ByteString) that).buffer, ((ByteString) that).lo, ((ByteString) that).size());
    }

    public final boolean equals (byte[] buf) {
        return equals(buf, 0, buf.length);
    }

    public final boolean equals (byte[] buf, int low, int length) {

        if (this.size() != length)
            return false;
        for (int i = 0; i < length; i++) {
            if (this.byteAt(i) != buf[low++])
                return false;
        }
        return true;
    }


    /**
     * Compare this to that in lex order
     */
    public final boolean greaterOrEqual (ByteString that) {
        int m = this.size();
        int n = that.size();
        for (int i = 0; i < n; i++) {
            if (i >= m)
                return false;
            else {
                byte x = this.byteAt(i);
                byte y = that.byteAt(i);
                if (x < y)
                    return false;
                else if (x > y)
                    return true;
            }
        }
        return true;
    }

    /**
     * Return number of bytes
     */
    public final int size () {
        return (hi-lo);
    }

    /**
     * Return byte at given offset.
     */
    public final byte byteAt (int i) {
	if (lo + i >= hi) throw new IndexOutOfBoundsException();
        return buffer[lo+i];
    }

    /**
     * Check whether the ByteString starts with the given byte[].
     * Ignores case.
     */
    private final boolean startsWith (byte[] that) {
	if (that.length > hi - lo) return false;
        for (int i = 0; i < that.length; i++) {
            char x = (char)(buffer[lo+i]);
            char y = (char)(that[i]);
            if (x != y && (x < 'a' || x > 'z' || (x + 'A' - 'a') != y))
                return false;
        }
        return true;
    }

    /**
     * Returns the offset for the first occurrence of a given byte
     * in the ByteString if present, and -1 otherwise.
     */
    public final int firstIndexOf (byte x) {
        for (int i = lo; i < hi; i++) {
            if (buffer[i] == x)
                return (i-lo);
        }
        return (-1);
    }

    /**
     * Returns the offset for the first occurrence of a given byte
     * in the ByteString to the right of the given offset if present, and
     * -1 otherwise.
     */
    public final int firstIndexOf (byte x, int offset) {
        for (int i = lo+offset+1; i < hi; i++) {
            if (buffer[i] == x)
                return (i-lo);
        }
        return (-1);
    }

    /**
     * Returns the offset for the last occurrence of a given byte
     * in the ByteString if present, and -1 otherwise.
     */
    public final int lastIndexOf (byte x) {
        for (int i = hi; --i >= lo; ) {
            if (buffer[i] == x)
                return (i-lo);
        }
        return -1;
    }

    /**
     * Returns the offset for the last occurrence of a given byte
     * in the ByteString to the left of the given offset if present, and
     * -1 otherwise.
     */
    public final int lastIndexOf (byte x, int offset) {
        for (int i = lo+offset; --i >= lo; ) {
            if (buffer[i] == x)
                return (i-lo);
        }
        return -1;
    }

    /**
     * Return the offset for the first occurrence of a given byte[]
     * in the ByteString to the right of the given offset if present,
     * and -1 otherwise.  Case insensitive.
     */
    public final int firstIndexOf (byte[] that) {
	return firstIndexOf(that, 0);
    }

    /**
     * Return the offset for the first occurrence of a given byte[]
     * in the ByteString to the right of the given offset if present,
     * and -1 otherwise.  Case insensitive.
     */
    public final int firstIndexOf (byte[] that, int offset) {

        for (int i = lo+offset; i <= hi-that.length; i++) {
            int j;
            for (j = 0; j < that.length; j++) {
                char x = (char)(buffer[i+j]);
                char y = (char)(that[j]);
                if (x != y && (x < 'a' || x > 'z' || (x + 'A' - 'a') != y))
                    break;
            }
            if (j >= that.length) {
                return i-lo;
            }
        }
        return -1;
    }

    /**
     * Convert to String
     */
    public final String toString () {
        return new String(buffer, lo, hi-lo);
    }

    /**
     * Convert to byte[]
     * clone version
     */
    public final byte[] getBytes () { return getBytes( true ); }

    /**
     * Convert to byte[]
     * clonable version
     */
    public final byte[] getBytes ( boolean clone ) 
    {
        if ( clone )
        {
            byte[] result = new byte[hi-lo];
            System.arraycopy(buffer, lo, result, 0, hi-lo);
            return result;
        }
        
        return buffer;
    }

    public final byte[] getBytes ( int len[] )
    {
        len[0] = hi;
        return buffer;
    }

    /**
     * Extract X out of ByteString X+Y+Z, where Y is the given delimiter.
     * Replace original ByteString (X+Y+Z) by Z.  Not MT-safe.
     * Returns null if Y is absent.
     */
    public final ByteString extractTill (byte[] y) {
        int offset = firstIndexOf(y, 0);
        if (offset < 0) {
                return null;
        }
        else {
            ByteString result = substring(0, offset);
            lo += (offset + y.length);
            return result;
        }
    }


    /**
     * Remove Y from ByteString Y+Z, where Y is the given token,
     * If successful, replace original ByteString (Y+Z) by Z, and
     * & return true.  Else return false. Not MT-safe.
     * Throw ArrayIndexOutOfBoundsException if ByteString does
     * not start with Y.
     */
    public final boolean extractToken (byte[] y) {
        if (startsWith(y)) {
            lo += y.length;
            return true;
        }
        else {
            return false;
        }
    }

    /**
     * Discard COUNT bytes from ByteString
     */
    public final void discard(int count) {
	if (lo + count > hi) throw new IndexOutOfBoundsException();
	lo += count;
    }

    /**
     * Discard X+Y from ByteString X+Y+Z, where Y is the given token.
     * Throw ArrayIndexOutOfBoundsException if Y is absent.
     */
    public final void discardTill (byte[] y) {
        lo += (firstIndexOf(y, 0) + y.length);
    }

    /**
     * Discard X+Y from ByteString X+Y+Z, where Y is LF or CRLF.
     * Throw ArrayIndexOutOfBoundsException if Y is absent.
     */
    public final void discardLine () {
        lo += (firstIndexOf(LF, 0) + 1);
    }

    /**
     * Checks for absence of complete lines
     */
    public final boolean noLines () {
        return firstIndexOf(LF, 0) < 0;
    }

    /**
     * Checks for absence of complete lines to the right of the given offset
     */
    public final boolean noLines (int offset) {
        return firstIndexOf(LF, offset) < 0;
    }

    /*
     * Extract the first line and return it as a substring.
     * Throw ArrayIndexOutOfBoundsException if there are no lines.
     */
    public final ByteString extractLine() {
        ByteString result;
        int offset = firstIndexOf(LF, 0);
        if (offset > 1 && byteAt(offset-1) == CR) {
            result = substring(0, offset-1);
            lo += (offset + 1);
        }
        else {
            result = substring(0, offset);
            lo += (offset + 1);
        }

        return result;
    }


    /**
     * Extract the first line and write it to given output stream.
     * Throw ArrayIndexOutOfBoundsException if there are no lines.
     * return number of byte wrote
     */

    public final int extractLine(OutputStream stream)
        throws IOException {

        int origin = lo;

        int offset = firstIndexOf(LF, 0);
        if (offset > 1 && byteAt(offset-1) == CR) {
            stream.write(buffer, lo, offset-1);
            stream.write(CRLF);
            lo += (offset + 1);
        }
        else {
            stream.write(buffer, lo, offset);
            stream.write(CRLF);
            lo += (offset + 1);
        }
        return (lo - origin);
    }

    public void discardLeadingSpace()
    {
        // skip leading space
        for (; lo < hi; lo++) {
                if (buffer[lo] != ' ' )
                        break;
        }
    }

    /**
     * bulk write to a stream.
     */
    public final void write (OutputStream stream)
        throws IOException {

        stream.write(buffer, lo, hi-lo);
    }

    /**
     * Extract a long
     **/
    public final long extractLong() {
        long n = 0;
        for (int i = lo; i < lo+8; i++) {
            n <<= 8;
            n += buffer[i];
        }
        lo += 8;
        return n;
    }

    /**
     * Extract an int
     **/
    public final int extractInt() {
        int n = 0;
        for (int i = lo; i < lo+4; i++) {
            n <<= 8;
            n += buffer[i];
        }
        lo += 4;
        return n;
    }

    /**
     * Return substring from offset start (inclusive) till offset
     * finish (exclusive).
     */
    public ByteString substring (int start, int finish) {
	if (start > finish || lo+finish > hi)
	    throw new IndexOutOfBoundsException();
        return new ByteString(buffer, lo+start, lo+finish);
    }

    /**
     * Return substring from offset start (inclusive) till the
     * end of the ByteString.
     */
    public ByteString substring (int start) {
	if (lo+start > hi) throw new IndexOutOfBoundsException();
        return new ByteString(buffer, lo+start, hi);
    }

    /**
     * Display the contents of the ByteString for
     * the purpose of debugging
     */
    public void displayBuffer (String caption, BufferedOutputStream stream) {
        int saved = lo;

        try {
            for (int i = 0; !noLines(); i++) {
                stream.write(caption.getBytes());
                extractLine(stream);
            }
            stream.write(caption.getBytes());
            stream.write(getBytes());
            lo = saved;
            stream.flush();
        } catch (IOException e) {
            System.err.println(e + " while displaying contents of ByteString");
        }
    }


    /**
     * Clones an instance of the ByteString object.
     * @exception CloneNotSupportedException could be thrown by constituent components.
     */
    public Object clone () throws CloneNotSupportedException
    {
         ByteString l_theClone = (ByteString) super.clone();

	 if (buffer != null)
	 {
	     l_theClone.buffer = new byte [buffer.length];
	     System.arraycopy (buffer, 0, l_theClone.buffer, 0, buffer.length);
	 }

	 return (l_theClone);
    }
    

    public void setSize( int len )
    {
        hi = len;
        lo = 0;
    }
}
