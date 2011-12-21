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

import ch.boye.httpclientandroidlib.io.EofSensor;
import ch.boye.httpclientandroidlib.io.HttpTransportMetrics;
import ch.boye.httpclientandroidlib.io.SessionInputBuffer;
import ch.boye.httpclientandroidlib.protocol.HTTP;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;

/**
 * Logs all data read to the wire LOG.
 *
 *
 * @since 4.0
 */
@Immutable
public class LoggingSessionInputBuffer implements SessionInputBuffer, EofSensor {

    /** Original session input buffer. */
    private final SessionInputBuffer in;

    private final EofSensor eofSensor;

    /** The wire log to use for writing. */
    private final Wire wire;

    private final String charset;

    /**
     * Create an instance that wraps the specified session input buffer.
     * @param in The session input buffer.
     * @param wire The wire log to use.
     * @param charset protocol charset, <code>ASCII</code> if <code>null</code>
     */
    public LoggingSessionInputBuffer(
            final SessionInputBuffer in, final Wire wire, final String charset) {
        super();
        this.in = in;
        this.eofSensor = in instanceof EofSensor ? (EofSensor) in : null;
        this.wire = wire;
        this.charset = charset != null ? charset : HTTP.ASCII;
    }

    public LoggingSessionInputBuffer(final SessionInputBuffer in, final Wire wire) {
        this(in, wire, null);
    }

    public boolean isDataAvailable(int timeout) throws IOException {
        return this.in.isDataAvailable(timeout);
    }

    public int read(byte[] b, int off, int len) throws IOException {
        int l = this.in.read(b,  off,  len);
        if (this.wire.enabled() && l > 0) {
            this.wire.input(b, off, l);
        }
        return l;
    }

    public int read() throws IOException {
        int l = this.in.read();
        if (this.wire.enabled() && l != -1) {
            this.wire.input(l);
        }
        return l;
    }

    public int read(byte[] b) throws IOException {
        int l = this.in.read(b);
        if (this.wire.enabled() && l > 0) {
            this.wire.input(b, 0, l);
        }
        return l;
    }

    public String readLine() throws IOException {
        String s = this.in.readLine();
        if (this.wire.enabled() && s != null) {
            String tmp = s + "\r\n";
            this.wire.input(tmp.getBytes(this.charset));
        }
        return s;
    }

    public int readLine(final CharArrayBuffer buffer) throws IOException {
        int l = this.in.readLine(buffer);
        if (this.wire.enabled() && l >= 0) {
            int pos = buffer.length() - l;
            String s = new String(buffer.buffer(), pos, l);
            String tmp = s + "\r\n";
            this.wire.input(tmp.getBytes(this.charset));
        }
        return l;
    }

    public HttpTransportMetrics getMetrics() {
        return this.in.getMetrics();
    }

    public boolean isEof() {
        if (this.eofSensor != null) {
            return this.eofSensor.isEof();
        } else {
            return false;
        }
    }

}
