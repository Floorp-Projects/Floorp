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
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CoderResult;

import ch.boye.httpclientandroidlib.MessageConstraintException;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.config.MessageConstraints;
import ch.boye.httpclientandroidlib.io.BufferInfo;
import ch.boye.httpclientandroidlib.io.HttpTransportMetrics;
import ch.boye.httpclientandroidlib.io.SessionInputBuffer;
import ch.boye.httpclientandroidlib.protocol.HTTP;
import ch.boye.httpclientandroidlib.util.Args;
import ch.boye.httpclientandroidlib.util.Asserts;
import ch.boye.httpclientandroidlib.util.ByteArrayBuffer;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;

/**
 * Abstract base class for session input buffers that stream data from
 * an arbitrary {@link InputStream}. This class buffers input data in
 * an internal byte array for optimal input performance.
 * <p/>
 * {@link #readLine(CharArrayBuffer)} and {@link #readLine()} methods of this
 * class treat a lone LF as valid line delimiters in addition to CR-LF required
 * by the HTTP specification.
 *
 * @since 4.3
 */
@NotThreadSafe
public class SessionInputBufferImpl implements SessionInputBuffer, BufferInfo {

    private final HttpTransportMetricsImpl metrics;
    private final byte[] buffer;
    private final ByteArrayBuffer linebuffer;
    private final int minChunkLimit;
    private final MessageConstraints constraints;
    private final CharsetDecoder decoder;

    private InputStream instream;
    private int bufferpos;
    private int bufferlen;
    private CharBuffer cbuf;

    /**
     * Creates new instance of SessionInputBufferImpl.
     *
     * @param metrics HTTP transport metrics.
     * @param buffersize buffer size. Must be a positive number.
     * @param minChunkLimit size limit below which data chunks should be buffered in memory
     *   in order to minimize native method invocations on the underlying network socket.
     *   The optimal value of this parameter can be platform specific and defines a trade-off
     *   between performance of memory copy operations and that of native method invocation.
     *   If negative default chunk limited will be used.
     * @param constraints Message constraints. If <code>null</code>
     *   {@link MessageConstraints#DEFAULT} will be used.
     * @param chardecoder chardecoder to be used for decoding HTTP protocol elements.
     *   If <code>null</code> simple type cast will be used for byte to char conversion.
     */
    public SessionInputBufferImpl(
            final HttpTransportMetricsImpl metrics,
            final int buffersize,
            final int minChunkLimit,
            final MessageConstraints constraints,
            final CharsetDecoder chardecoder) {
        Args.notNull(metrics, "HTTP transport metrcis");
        Args.positive(buffersize, "Buffer size");
        this.metrics = metrics;
        this.buffer = new byte[buffersize];
        this.bufferpos = 0;
        this.bufferlen = 0;
        this.minChunkLimit = minChunkLimit >= 0 ? minChunkLimit : 512;
        this.constraints = constraints != null ? constraints : MessageConstraints.DEFAULT;
        this.linebuffer = new ByteArrayBuffer(buffersize);
        this.decoder = chardecoder;
    }

    public SessionInputBufferImpl(
            final HttpTransportMetricsImpl metrics,
            final int buffersize) {
        this(metrics, buffersize, buffersize, null, null);
    }

    public void bind(final InputStream instream) {
        this.instream = instream;
    }

    public boolean isBound() {
        return this.instream != null;
    }

    public int capacity() {
        return this.buffer.length;
    }

    public int length() {
        return this.bufferlen - this.bufferpos;
    }

    public int available() {
        return capacity() - length();
    }

    private int streamRead(final byte[] b, final int off, final int len) throws IOException {
        Asserts.notNull(this.instream, "Input stream");
        return this.instream.read(b, off, len);
    }

    public int fillBuffer() throws IOException {
        // compact the buffer if necessary
        if (this.bufferpos > 0) {
            final int len = this.bufferlen - this.bufferpos;
            if (len > 0) {
                System.arraycopy(this.buffer, this.bufferpos, this.buffer, 0, len);
            }
            this.bufferpos = 0;
            this.bufferlen = len;
        }
        final int l;
        final int off = this.bufferlen;
        final int len = this.buffer.length - off;
        l = streamRead(this.buffer, off, len);
        if (l == -1) {
            return -1;
        } else {
            this.bufferlen = off + l;
            this.metrics.incrementBytesTransferred(l);
            return l;
        }
    }

    public boolean hasBufferedData() {
        return this.bufferpos < this.bufferlen;
    }

    public void clear() {
        this.bufferpos = 0;
        this.bufferlen = 0;
    }

    public int read() throws IOException {
        int noRead;
        while (!hasBufferedData()) {
            noRead = fillBuffer();
            if (noRead == -1) {
                return -1;
            }
        }
        return this.buffer[this.bufferpos++] & 0xff;
    }

    public int read(final byte[] b, final int off, final int len) throws IOException {
        if (b == null) {
            return 0;
        }
        if (hasBufferedData()) {
            final int chunk = Math.min(len, this.bufferlen - this.bufferpos);
            System.arraycopy(this.buffer, this.bufferpos, b, off, chunk);
            this.bufferpos += chunk;
            return chunk;
        }
        // If the remaining capacity is big enough, read directly from the
        // underlying input stream bypassing the buffer.
        if (len > this.minChunkLimit) {
            final int read = streamRead(b, off, len);
            if (read > 0) {
                this.metrics.incrementBytesTransferred(read);
            }
            return read;
        } else {
            // otherwise read to the buffer first
            while (!hasBufferedData()) {
                final int noRead = fillBuffer();
                if (noRead == -1) {
                    return -1;
                }
            }
            final int chunk = Math.min(len, this.bufferlen - this.bufferpos);
            System.arraycopy(this.buffer, this.bufferpos, b, off, chunk);
            this.bufferpos += chunk;
            return chunk;
        }
    }

    public int read(final byte[] b) throws IOException {
        if (b == null) {
            return 0;
        }
        return read(b, 0, b.length);
    }

    private int locateLF() {
        for (int i = this.bufferpos; i < this.bufferlen; i++) {
            if (this.buffer[i] == HTTP.LF) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Reads a complete line of characters up to a line delimiter from this
     * session buffer into the given line buffer. The number of chars actually
     * read is returned as an integer. The line delimiter itself is discarded.
     * If no char is available because the end of the stream has been reached,
     * the value <code>-1</code> is returned. This method blocks until input
     * data is available, end of file is detected, or an exception is thrown.
     * <p>
     * This method treats a lone LF as a valid line delimiters in addition
     * to CR-LF required by the HTTP specification.
     *
     * @param      charbuffer   the line buffer.
     * @return     one line of characters
     * @exception  IOException  if an I/O error occurs.
     */
    public int readLine(final CharArrayBuffer charbuffer) throws IOException {
        Args.notNull(charbuffer, "Char array buffer");
        int noRead = 0;
        boolean retry = true;
        while (retry) {
            // attempt to find end of line (LF)
            final int i = locateLF();
            if (i != -1) {
                // end of line found.
                if (this.linebuffer.isEmpty()) {
                    // the entire line is preset in the read buffer
                    return lineFromReadBuffer(charbuffer, i);
                }
                retry = false;
                final int len = i + 1 - this.bufferpos;
                this.linebuffer.append(this.buffer, this.bufferpos, len);
                this.bufferpos = i + 1;
            } else {
                // end of line not found
                if (hasBufferedData()) {
                    final int len = this.bufferlen - this.bufferpos;
                    this.linebuffer.append(this.buffer, this.bufferpos, len);
                    this.bufferpos = this.bufferlen;
                }
                noRead = fillBuffer();
                if (noRead == -1) {
                    retry = false;
                }
            }
            final int maxLineLen = this.constraints.getMaxLineLength();
            if (maxLineLen > 0 && this.linebuffer.length() >= maxLineLen) {
                throw new MessageConstraintException("Maximum line length limit exceeded");
            }
        }
        if (noRead == -1 && this.linebuffer.isEmpty()) {
            // indicate the end of stream
            return -1;
        }
        return lineFromLineBuffer(charbuffer);
    }

    /**
     * Reads a complete line of characters up to a line delimiter from this
     * session buffer. The line delimiter itself is discarded. If no char is
     * available because the end of the stream has been reached,
     * <code>null</code> is returned. This method blocks until input data is
     * available, end of file is detected, or an exception is thrown.
     * <p>
     * This method treats a lone LF as a valid line delimiters in addition
     * to CR-LF required by the HTTP specification.
     *
     * @return HTTP line as a string
     * @exception  IOException  if an I/O error occurs.
     */
    private int lineFromLineBuffer(final CharArrayBuffer charbuffer)
            throws IOException {
        // discard LF if found
        int len = this.linebuffer.length();
        if (len > 0) {
            if (this.linebuffer.byteAt(len - 1) == HTTP.LF) {
                len--;
            }
            // discard CR if found
            if (len > 0) {
                if (this.linebuffer.byteAt(len - 1) == HTTP.CR) {
                    len--;
                }
            }
        }
        if (this.decoder == null) {
            charbuffer.append(this.linebuffer, 0, len);
        } else {
            final ByteBuffer bbuf =  ByteBuffer.wrap(this.linebuffer.buffer(), 0, len);
            len = appendDecoded(charbuffer, bbuf);
        }
        this.linebuffer.clear();
        return len;
    }

    private int lineFromReadBuffer(final CharArrayBuffer charbuffer, final int position)
            throws IOException {
        int pos = position;
        final int off = this.bufferpos;
        int len;
        this.bufferpos = pos + 1;
        if (pos > off && this.buffer[pos - 1] == HTTP.CR) {
            // skip CR if found
            pos--;
        }
        len = pos - off;
        if (this.decoder == null) {
            charbuffer.append(this.buffer, off, len);
        } else {
            final ByteBuffer bbuf =  ByteBuffer.wrap(this.buffer, off, len);
            len = appendDecoded(charbuffer, bbuf);
        }
        return len;
    }

    private int appendDecoded(
            final CharArrayBuffer charbuffer, final ByteBuffer bbuf) throws IOException {
        if (!bbuf.hasRemaining()) {
            return 0;
        }
        if (this.cbuf == null) {
            this.cbuf = CharBuffer.allocate(1024);
        }
        this.decoder.reset();
        int len = 0;
        while (bbuf.hasRemaining()) {
            final CoderResult result = this.decoder.decode(bbuf, this.cbuf, true);
            len += handleDecodingResult(result, charbuffer, bbuf);
        }
        final CoderResult result = this.decoder.flush(this.cbuf);
        len += handleDecodingResult(result, charbuffer, bbuf);
        this.cbuf.clear();
        return len;
    }

    private int handleDecodingResult(
            final CoderResult result,
            final CharArrayBuffer charbuffer,
            final ByteBuffer bbuf) throws IOException {
        if (result.isError()) {
            result.throwException();
        }
        this.cbuf.flip();
        final int len = this.cbuf.remaining();
        while (this.cbuf.hasRemaining()) {
            charbuffer.append(this.cbuf.get());
        }
        this.cbuf.compact();
        return len;
    }

    public String readLine() throws IOException {
        final CharArrayBuffer charbuffer = new CharArrayBuffer(64);
        final int l = readLine(charbuffer);
        if (l != -1) {
            return charbuffer.toString();
        } else {
            return null;
        }
    }

    public boolean isDataAvailable(final int timeout) throws IOException {
        return hasBufferedData();
    }

    public HttpTransportMetrics getMetrics() {
        return this.metrics;
    }

}
