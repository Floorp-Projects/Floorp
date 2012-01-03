/*
 * ====================================================================
 *
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.impl.conn;

import java.io.IOException;

import ch.boye.httpclientandroidlib.annotation.Immutable;

import ch.boye.httpclientandroidlib.io.HttpTransportMetrics;
import ch.boye.httpclientandroidlib.io.SessionOutputBuffer;
import ch.boye.httpclientandroidlib.protocol.HTTP;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;

/**
 * Logs all data written to the wire LOG.
 *
 *
 * @since 4.0
 */
@Immutable
public class LoggingSessionOutputBuffer implements SessionOutputBuffer {

    /** Original data transmitter. */
    private final SessionOutputBuffer out;

    /** The wire log to use. */
    private final Wire wire;

    private final String charset;

    /**
     * Create an instance that wraps the specified session output buffer.
     * @param out The session output buffer.
     * @param wire The Wire log to use.
     * @param charset protocol charset, <code>ASCII</code> if <code>null</code>
     */
    public LoggingSessionOutputBuffer(
            final SessionOutputBuffer out, final Wire wire, final String charset) {
        super();
        this.out = out;
        this.wire = wire;
        this.charset = charset != null ? charset : HTTP.ASCII;
    }

    public LoggingSessionOutputBuffer(final SessionOutputBuffer out, final Wire wire) {
        this(out, wire, null);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        this.out.write(b,  off,  len);
        if (this.wire.enabled()) {
            this.wire.output(b, off, len);
        }
    }

    public void write(int b) throws IOException {
        this.out.write(b);
        if (this.wire.enabled()) {
            this.wire.output(b);
        }
    }

    public void write(byte[] b) throws IOException {
        this.out.write(b);
        if (this.wire.enabled()) {
            this.wire.output(b);
        }
    }

    public void flush() throws IOException {
        this.out.flush();
    }

    public void writeLine(final CharArrayBuffer buffer) throws IOException {
        this.out.writeLine(buffer);
        if (this.wire.enabled()) {
            String s = new String(buffer.buffer(), 0, buffer.length());
            String tmp = s + "\r\n";
            this.wire.output(tmp.getBytes(this.charset));
        }
    }

    public void writeLine(final String s) throws IOException {
        this.out.writeLine(s);
        if (this.wire.enabled()) {
            String tmp = s + "\r\n";
            this.wire.output(tmp.getBytes(this.charset));
        }
    }

    public HttpTransportMetrics getMetrics() {
        return this.out.getMetrics();
    }

}
