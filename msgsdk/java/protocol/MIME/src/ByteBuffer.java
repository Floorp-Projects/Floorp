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

import java.io.InputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;

/**
 * Mutable subclass of ByteString.
 */
public final class ByteBuffer extends ByteString implements Cloneable {
    boolean wasCloned;

    /**
     * Construct a ByteBuffer of the given capacity
     */
    public ByteBuffer (int capacity) {
        super();
        buffer  = new byte[capacity];
	wasCloned = false;
    }

    /**
     * Construct a ByteBuffer of the given capacity
     */
    public ByteBuffer (ByteString bs) {
        super();
        buffer  = bs.buffer;
        hi      = bs.hi;
        lo      = bs.lo;
	wasCloned = false;
    }

    /**
     * Construct a ByteBuffer of the given capacity
     * no clone version
     */
    public ByteBuffer (byte[] b)
    {
        super();
        buffer  = b;
        hi      = b.length;
        lo      = 0;
    	wasCloned = false;
    }

    /**
     * Construct a ByteBuffer of the given capacity
     * no clone version
     */
    public ByteBuffer (byte[] b, int len)
    {
        super();
        buffer  = b;
        hi      = len;
        lo      = 0;
    	wasCloned = false;
    }

    /**/

    /**
     * Ensure there is capacity for appending num bytes.
     */
    private final void ensureCapacity (int num) {
        byte[] newBuffer = null;
	int newsize;

	if (hi == lo) erase();
	if (buffer.length - hi >= num) return;

	newsize = num + (hi-lo);
	if (newsize*2 < buffer.length) {
	    newsize = buffer.length;
	    if (!wasCloned) newBuffer = buffer;
	}
	else if (newsize < buffer.length*2) newsize = buffer.length*2;

	if (newBuffer == null) 
	    newBuffer = new byte[newsize];

    System.arraycopy(buffer, lo, newBuffer, 0, hi-lo );

    buffer = newBuffer;
	wasCloned = false;
	hi -= lo;
	lo = 0;
    }

    /**
     * Append a ByteString. Not MT-safe.
     */
    public final void append (ByteString that) {
	ensureCapacity(that.size());
        System.arraycopy(that.buffer, that.lo, this.buffer, this.hi, that.size());
        this.hi += that.size();
    }

    /**
     * Append a substring of a ByteString. Not MT-safe.
     */
    public final void append (ByteString that, int start) {
	int substrSize = that.size() - start;

	if (substrSize < 0) throw new IndexOutOfBoundsException();
	ensureCapacity(substrSize);
        System.arraycopy(that.buffer, that.lo+start, this.buffer, this.hi,
			 substrSize);
        this.hi += substrSize;
    }

    /**
     * Append a substring of a ByteString. Not MT-safe.
     */
    public final void append (ByteString that, int start, int finish) {
	if (start > finish || finish > that.size())
	    throw new IndexOutOfBoundsException();
	ensureCapacity(finish - start);
        System.arraycopy(that.buffer, that.lo+start, this.buffer, this.hi, finish-start);
        this.hi += (finish-start);
    }

    /**
     * Append a byte[]. Not MT-safe.
     */
    public final void append (byte[] that) {
	ensureCapacity(that.length);
        System.arraycopy(that, 0, this.buffer, this.hi, that.length);
        this.hi += that.length;
    }

    /**
     * Append a byte[]. Not MT-safe.
     */
    public final void append (byte[] that, int len ) 
    {
	    ensureCapacity(len);
        System.arraycopy(that, 0, this.buffer, this.hi, len);
        this.hi += len;
    }

    /**
     * Add a long
     */
    public final void append (long n) {
	ensureCapacity(8);
        for (int i=hi+8; --i>=hi; ) {
            buffer[i] = (byte)(n & 0xff);
            n >>= 8;
        }
        hi += 8;
    }

    /**
     * Add an int
     */
    public final void append (int n) {
	ensureCapacity(4);
        for (int i=hi+4; --i>=hi; ) {
            buffer[i] = (byte)(n & 0xff);
            n >>= 8;
        }
        hi += 4;
    }

    /**
     * Add a byte
     */
    public final void append (byte n) {
	ensureCapacity(1);
	buffer[hi] = n;
	hi++;
    }

    /**
     * Erase the contents of the ByteBuffering. Not MT safe.
     * Will not shrink capacity.
     */
    public final void erase () {
	if (!wasCloned) hi = 0;
        lo = hi;
    }

    /**
     * Defragment the contents of buffer. Not MT safe.
     */
    public final void cleanup () {
        if (wasCloned || lo+lo <= buffer.length)
            return;
        System.arraycopy(buffer, lo, buffer, 0, hi-lo);
        hi -= lo;
        lo = 0;
    }

    /**
     * Return substring from offset start (inclusive) till offset
     * finish (exclusive).
     */
    public ByteString substring (int start, int finish) {
        ByteString result = super.substring(start, finish);
	wasCloned = true;
        return result;
    }

    /**
     * Return substring to the right of the given offset (inclusive).
     */
    public ByteString substring (int start) {
        ByteString result = super.substring(start);
	wasCloned = true;
        return result;
    }

    /**
     * Add a line. Not MT-safe.
     */
    public final void addLine (ByteString line) {
        append(line);
        append(CRLF);
    }

    /**
     * Add a line. Not MT-safe.
     */
    public final void addLine (byte[] line) {
        append(line);
        append(CRLF);
    }

    /**
     * Read a line read (and maybe more) from a stream. Not MT-safe.
     * Blocks until one line is read.  May read multiple lines.
     */
    public final void readLine (InputStream stream)
        throws IOException {
	    while (noLines()) {
		bulkRead(stream);
	    }
    }

    /**
     * Bulk read from a stream. Not MT-safe.
     * Returns the number of bytes read.
     */
    public final int bulkRead (InputStream stream)
        throws IOException {

        int n = -1;
	ensureCapacity(1024);	// TODO: is this a good number?
        for (int attempt = 0; attempt < 10; attempt++) {
            n = stream.read(buffer, hi, buffer.length-hi);
            if (n > 0) {
                hi += n;
                return n;
            }
            else if (n == 0) {
                System.err.println("BufferedInputStream.read(byte[], int, int) returns 0 on attempt #" + attempt);
                try {
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    System.err.println(e + " while waiting in bulkRead");
                }
            }
            else {
                throw new IOException("Bulk read returns " + n + " bytes");
            }
        }
        throw new IOException("Bulk read returns " + n + " bytes");
    }

    /**
     * Clones an instance of the ByteBuffer object.
     * @exception CloneNotSupportedException could be thrown by constituent components.
     */
    public Object clone () throws CloneNotSupportedException
    {
         ByteBuffer l_theClone = (ByteBuffer) super.clone();
 
         return (l_theClone);
    }
    
    public InputStream getInputStream()
    {
        return new ByteArrayInputStream( buffer, lo, hi );
    }

    public void setSize( int len ) { ensureCapacity( len ); }
}
