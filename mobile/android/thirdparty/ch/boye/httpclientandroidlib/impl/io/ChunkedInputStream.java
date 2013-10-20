/*
 * ====================================================================
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.impl.io;

import java.io.IOException;
import java.io.InputStream;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.MalformedChunkCodingException;
import ch.boye.httpclientandroidlib.TruncatedChunkException;
import ch.boye.httpclientandroidlib.io.BufferInfo;
import ch.boye.httpclientandroidlib.io.SessionInputBuffer;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;
import ch.boye.httpclientandroidlib.util.ExceptionUtils;

/**
 * Implements chunked transfer coding. The content is received in small chunks.
 * Entities transferred using this input stream can be of unlimited length.
 * After the stream is read to the end, it provides access to the trailers,
 * if any.
 * <p>
 * Note that this class NEVER closes the underlying stream, even when close
 * gets called.  Instead, it will read until the "end" of its chunking on
 * close, which allows for the seamless execution of subsequent HTTP 1.1
 * requests, while not requiring the client to remember to read the entire
 * contents of the response.
 *
 *
 * @since 4.0
 *
 */
public class ChunkedInputStream extends InputStream {

    private static final int CHUNK_LEN               = 1;
    private static final int CHUNK_DATA              = 2;
    private static final int CHUNK_CRLF              = 3;

    private static final int BUFFER_SIZE = 2048;

    /** The session input buffer */
    private final SessionInputBuffer in;

    private final CharArrayBuffer buffer;

    private int state;

    /** The chunk size */
    private int chunkSize;

    /** The current position within the current chunk */
    private int pos;

    /** True if we've reached the end of stream */
    private boolean eof = false;

    /** True if this stream is closed */
    private boolean closed = false;

    private Header[] footers = new Header[] {};

    /**
     * Wraps session input stream and reads chunk coded input.
     *
     * @param in The session input buffer
     */
    public ChunkedInputStream(final SessionInputBuffer in) {
        super();
        if (in == null) {
            throw new IllegalArgumentException("Session input buffer may not be null");
        }
        this.in = in;
        this.pos = 0;
        this.buffer = new CharArrayBuffer(16);
        this.state = CHUNK_LEN;
    }

    public int available() throws IOException {
        if (this.in instanceof BufferInfo) {
            int len = ((BufferInfo) this.in).length();
            return Math.min(len, this.chunkSize - this.pos);
        } else {
            return 0;
        }
    }

    /**
     * <p> Returns all the data in a chunked stream in coalesced form. A chunk
     * is followed by a CRLF. The method returns -1 as soon as a chunksize of 0
     * is detected.</p>
     *
     * <p> Trailer headers are read automatically at the end of the stream and
     * can be obtained with the getResponseFooters() method.</p>
     *
     * @return -1 of the end of the stream has been reached or the next data
     * byte
     * @throws IOException in case of an I/O error
     */
    public int read() throws IOException {
        if (this.closed) {
            throw new IOException("Attempted read from closed stream.");
        }
        if (this.eof) {
            return -1;
        }
        if (state != CHUNK_DATA) {
            nextChunk();
            if (this.eof) {
                return -1;
            }
        }
        int b = in.read();
        if (b != -1) {
            pos++;
            if (pos >= chunkSize) {
                state = CHUNK_CRLF;
            }
        }
        return b;
    }

    /**
     * Read some bytes from the stream.
     * @param b The byte array that will hold the contents from the stream.
     * @param off The offset into the byte array at which bytes will start to be
     * placed.
     * @param len the maximum number of bytes that can be returned.
     * @return The number of bytes returned or -1 if the end of stream has been
     * reached.
     * @throws IOException in case of an I/O error
     */
    public int read (byte[] b, int off, int len) throws IOException {

        if (closed) {
            throw new IOException("Attempted read from closed stream.");
        }

        if (eof) {
            return -1;
        }
        if (state != CHUNK_DATA) {
            nextChunk();
            if (eof) {
                return -1;
            }
        }
        len = Math.min(len, chunkSize - pos);
        int bytesRead = in.read(b, off, len);
        if (bytesRead != -1) {
            pos += bytesRead;
            if (pos >= chunkSize) {
                state = CHUNK_CRLF;
            }
            return bytesRead;
        } else {
            eof = true;
            throw new TruncatedChunkException("Truncated chunk "
                    + "( expected size: " + chunkSize
                    + "; actual size: " + pos + ")");
        }
    }

    /**
     * Read some bytes from the stream.
     * @param b The byte array that will hold the contents from the stream.
     * @return The number of bytes returned or -1 if the end of stream has been
     * reached.
     * @throws IOException in case of an I/O error
     */
    public int read (byte[] b) throws IOException {
        return read(b, 0, b.length);
    }

    /**
     * Read the next chunk.
     * @throws IOException in case of an I/O error
     */
    private void nextChunk() throws IOException {
        chunkSize = getChunkSize();
        if (chunkSize < 0) {
            throw new MalformedChunkCodingException("Negative chunk size");
        }
        state = CHUNK_DATA;
        pos = 0;
        if (chunkSize == 0) {
            eof = true;
            parseTrailerHeaders();
        }
    }

    /**
     * Expects the stream to start with a chunksize in hex with optional
     * comments after a semicolon. The line must end with a CRLF: "a3; some
     * comment\r\n" Positions the stream at the start of the next line.
     *
     * @param in The new input stream.
     * @param required <tt>true<tt/> if a valid chunk must be present,
     *                 <tt>false<tt/> otherwise.
     *
     * @return the chunk size as integer
     *
     * @throws IOException when the chunk size could not be parsed
     */
    private int getChunkSize() throws IOException {
        int st = this.state;
        switch (st) {
        case CHUNK_CRLF:
            this.buffer.clear();
            int i = this.in.readLine(this.buffer);
            if (i == -1) {
                return 0;
            }
            if (!this.buffer.isEmpty()) {
                throw new MalformedChunkCodingException(
                    "Unexpected content at the end of chunk");
            }
            state = CHUNK_LEN;
            //$FALL-THROUGH$
        case CHUNK_LEN:
            this.buffer.clear();
            i = this.in.readLine(this.buffer);
            if (i == -1) {
                return 0;
            }
            int separator = this.buffer.indexOf(';');
            if (separator < 0) {
                separator = this.buffer.length();
            }
            try {
                return Integer.parseInt(this.buffer.substringTrimmed(0, separator), 16);
            } catch (NumberFormatException e) {
                throw new MalformedChunkCodingException("Bad chunk header");
            }
        default:
            throw new IllegalStateException("Inconsistent codec state");
        }
    }

    /**
     * Reads and stores the Trailer headers.
     * @throws IOException in case of an I/O error
     */
    private void parseTrailerHeaders() throws IOException {
        try {
            this.footers = AbstractMessageParser.parseHeaders
                (in, -1, -1, null);
        } catch (HttpException e) {
            IOException ioe = new MalformedChunkCodingException("Invalid footer: "
                    + e.getMessage());
            ExceptionUtils.initCause(ioe, e);
            throw ioe;
        }
    }

    /**
     * Upon close, this reads the remainder of the chunked message,
     * leaving the underlying socket at a position to start reading the
     * next response without scanning.
     * @throws IOException in case of an I/O error
     */
    public void close() throws IOException {
        if (!closed) {
            try {
                if (!eof) {
                    // read and discard the remainder of the message
                    byte buffer[] = new byte[BUFFER_SIZE];
                    while (read(buffer) >= 0) {
                    }
                }
            } finally {
                eof = true;
                closed = true;
            }
        }
    }

    public Header[] getFooters() {
        return (Header[])this.footers.clone();
    }

}
