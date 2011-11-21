/*
 * Copyright (c) 2008-2010 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

package nu.validator.htmlparser.impl;

import nu.validator.htmlparser.annotation.NoLength;

/**
 * An UTF-16 buffer that knows the start and end indeces of its unconsumed
 * content.
 * 
 * @version $Id$
 * @author hsivonen
 */
public final class UTF16Buffer {

    /**
     * The backing store of the buffer. May be larger than the logical content
     * of this <code>UTF16Buffer</code>.
     */
    private final @NoLength char[] buffer;

    /**
     * The index of the first unconsumed character in the backing buffer.
     */
    private int start;

    /**
     * The index of the slot immediately after the last character in the backing
     * buffer that is part of the logical content of this
     * <code>UTF16Buffer</code>.
     */
    private int end;

    //[NOCPP[
    
    /**
     * Constructor for wrapping an existing UTF-16 code unit array.
     * 
     * @param buffer
     *            the backing buffer
     * @param start
     *            the index of the first character to consume
     * @param end
     *            the index immediately after the last character to consume
     */
    public UTF16Buffer(@NoLength char[] buffer, int start, int end) {
        this.buffer = buffer;
        this.start = start;
        this.end = end;
    }

    // ]NOCPP]
    
    /**
     * Returns the start index.
     * 
     * @return the start index
     */
    public int getStart() {
        return start;
    }

    /**
     * Sets the start index.
     * 
     * @param start
     *            the start index
     */
    public void setStart(int start) {
        this.start = start;
    }

    /**
     * Returns the backing buffer.
     * 
     * @return the backing buffer
     */
    public @NoLength char[] getBuffer() {
        return buffer;
    }

    /**
     * Returns the end index.
     * 
     * @return the end index
     */
    public int getEnd() {
        return end;
    }

    /**
     * Checks if the buffer has data left.
     * 
     * @return <code>true</code> if there's data left
     */
    public boolean hasMore() {
        return start < end;
    }

    /**
     * Adjusts the start index to skip over the first character if it is a line
     * feed and the previous character was a carriage return.
     * 
     * @param lastWasCR
     *            whether the previous character was a carriage return
     */
    public void adjust(boolean lastWasCR) {
        if (lastWasCR && buffer[start] == '\n') {
            start++;
        }
    }

    /**
     * Sets the end index.
     * 
     * @param end
     *            the end index
     */
    public void setEnd(int end) {
        this.end = end;
    }
}
