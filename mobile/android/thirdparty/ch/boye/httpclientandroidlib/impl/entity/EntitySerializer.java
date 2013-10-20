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

package ch.boye.httpclientandroidlib.impl.entity;

import java.io.IOException;
import java.io.OutputStream;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpMessage;
import ch.boye.httpclientandroidlib.entity.ContentLengthStrategy;
import ch.boye.httpclientandroidlib.impl.io.ChunkedOutputStream;
import ch.boye.httpclientandroidlib.impl.io.ContentLengthOutputStream;
import ch.boye.httpclientandroidlib.impl.io.IdentityOutputStream;
import ch.boye.httpclientandroidlib.io.SessionOutputBuffer;

/**
 * HTTP entity serializer.
 * <p>
 * This entity serializer currently supports "chunked" and "identitiy"
 * transfer-coding and content length delimited content.
 * <p>
 * This class relies on a specific implementation of
 * {@link ContentLengthStrategy} to determine the content length or transfer
 * encoding of the entity.
 * <p>
 * This class writes out the content of {@link HttpEntity} to the data stream
 * using a transfer coding based on properties on the HTTP message.
 *
 * @since 4.0
 */
public class EntitySerializer {

    private final ContentLengthStrategy lenStrategy;

    public EntitySerializer(final ContentLengthStrategy lenStrategy) {
        super();
        if (lenStrategy == null) {
            throw new IllegalArgumentException("Content length strategy may not be null");
        }
        this.lenStrategy = lenStrategy;
    }

    /**
     * Creates a transfer codec based on properties of the given HTTP message
     * and returns {@link OutputStream} instance that transparently encodes
     * output data as it is being written out to the output stream.
     * <p>
     * This method is called by the public
     * {@link #serialize(SessionOutputBuffer, HttpMessage, HttpEntity)}.
     *
     * @param outbuffer the session output buffer.
     * @param message the HTTP message.
     * @return output stream.
     * @throws HttpException in case of HTTP protocol violation.
     * @throws IOException in case of an I/O error.
     */
    protected OutputStream doSerialize(
            final SessionOutputBuffer outbuffer,
            final HttpMessage message) throws HttpException, IOException {
        long len = this.lenStrategy.determineLength(message);
        if (len == ContentLengthStrategy.CHUNKED) {
            return new ChunkedOutputStream(outbuffer);
        } else if (len == ContentLengthStrategy.IDENTITY) {
            return new IdentityOutputStream(outbuffer);
        } else {
            return new ContentLengthOutputStream(outbuffer, len);
        }
    }

    /**
     * Writes out the content of the given HTTP entity to the session output
     * buffer based on properties of the given HTTP message.
     *
     * @param outbuffer the output session buffer.
     * @param message the HTTP message.
     * @param entity the HTTP entity to be written out.
     * @throws HttpException in case of HTTP protocol violation.
     * @throws IOException in case of an I/O error.
     */
    public void serialize(
            final SessionOutputBuffer outbuffer,
            final HttpMessage message,
            final HttpEntity entity) throws HttpException, IOException {
        if (outbuffer == null) {
            throw new IllegalArgumentException("Session output buffer may not be null");
        }
        if (message == null) {
            throw new IllegalArgumentException("HTTP message may not be null");
        }
        if (entity == null) {
            throw new IllegalArgumentException("HTTP entity may not be null");
        }
        OutputStream outstream = doSerialize(outbuffer, message);
        entity.writeTo(outstream);
        outstream.close();
    }

}
