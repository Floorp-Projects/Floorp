/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.plugin.composer.io;

import java.io.*;

/** A filter that filters out the unselected text from an HTML stream. Use this
 * stream to speeds up applications that only operate on the selected text.
 */

public class SelectedHTMLReader extends Reader {
    /** Create a stream that is used for read-only operations.
     * (This method just calls the two-argument constructor with a null
     * output stream.)
     * @param in the stream to read from.
     */

    public SelectedHTMLReader(Reader in ) throws IOException {
        this(in, null);
    }

    /** Create a stream that is used for read-write operations.
     * If the out parameter is supplied, all the text outside of the
     * HTML selection comments will be automaticly copied to the output stream.
     * <p>
     * Before the constructor returns, all the HTML text before the selection
     * start comment will be copied to the output stream.
     * @param in the stream to read from.
     * @param out the stream to write to. (Can be null).
     * @throws java.io.IOException if there is a problem reading or writing the
     * streams, or if there are no HTML selection comments.
     */

    public SelectedHTMLReader(Reader in, Writer out)
        throws IOException
    {
        this.in = in;
        this.out = out;
        // Crude but effective. We copy the input stream into a buffer.
        StringBuffer buffer = new StringBuffer(1024);
        int len;
        char[] temp = new char[1024];
        while((len = in.read(temp)) >= 0){
            buffer.append(temp,0,len);
        }
        // Now search for the beginning comment
        string = new String(buffer);
        String startComment = Comment.createSelectionStart().toString();
        String endComment = Comment.createSelectionEnd().toString();
        int start = string.indexOf(startComment);
        if ( start < 0 ) {
            // See if it's a sticky-after comment
            start = string.indexOf(Comment.createSelectionStart(true).toString());
            if ( start < 0 ) {
                System.err.println(string);
                throw new IOException("No start comment.");
            }
        }
        int end = string.indexOf(endComment, start);
        if ( end < 0 ) {
            // See if it's a sticky-after comment
            String stickyEnd = Comment.createSelectionEnd(true).toString();
            end = string.indexOf(stickyEnd, start);
            if ( end < 0 ) {
                System.err.println(string);
                throw new IOException("No end comment");
            }
            else {
                end += stickyEnd.length();
            }
        }
        else {
            end += endComment.length();
        }

        this.index = start;
        this.length = end;

        if ( out != null ) {
            // Copy the text before the start to the output
            out.write(string, 0, index);
        }
    }

    /** Closes the input stream.
     * If an output stream was supplied to the constructor,
     * any HTML text remaining in the input stream will be flushed to the
     * output stream. The output stream will not be closed.
     */

    public void close() throws IOException{
        in.close();
        if ( out != null ) {
            // Have to copy any remaining text
            int toWrite = string.length() - index;
            out.write(string, index, toWrite);
            index = string.length();
        }
    }
    public int read() throws IOException {
        if ( index >= length ) {
            return -1;
        }
        return string.charAt(index++);
    }

    public int read(char[] buf)  throws  IOException {
        return read(buf, 0, buf.length);
    }

    public int read(char[] buf, int start, int length)  throws  IOException {
        if ( length <= 0 ) {
            return 0;
        }
        int trueLength = Math.min(length, this.length - index);
        if ( trueLength <= 0 ){
            return -1;
        }
        string.getChars(index, index + trueLength, buf, start);
        index += trueLength;
        return trueLength;
    }

    public long skip(long count) throws  IOException {
        long trueSkip = Math.max(0, Math.min(count, (long) length - index));
        index += (int) trueSkip;
        return trueSkip;
    }

    private Reader in;
    private Writer out;
    private int index; // Current position of stream
    private int length; // Index of first character of buffer to not return.
    private String string; // The whole contents of the input stream.
}
