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
package ch.boye.httpclientandroidlib.client.entity;

import java.io.IOException;
import java.io.InputStream;
import java.io.PushbackInputStream;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.entity.HttpEntityWrapper;

/**
 * {@link HttpEntityWrapper} responsible for handling deflate Content Coded responses. In RFC2616
 * terms, <code>deflate</code> means a <code>zlib</code> stream as defined in RFC1950. Some server
 * implementations have misinterpreted RFC2616 to mean that a <code>deflate</code> stream as
 * defined in RFC1951 should be used (or maybe they did that since that's how IE behaves?). It's
 * confusing that <code>deflate</code> in HTTP 1.1 means <code>zlib</code> streams rather than
 * <code>deflate</code> streams. We handle both types in here, since that's what is seen on the
 * internet. Moral - prefer <code>gzip</code>!
 *
 * @see GzipDecompressingEntity
 *
 * @since 4.1
 */
public class DeflateDecompressingEntity extends DecompressingEntity {

    /**
     * Creates a new {@link DeflateDecompressingEntity} which will wrap the specified
     * {@link HttpEntity}.
     *
     * @param entity
     *            a non-null {@link HttpEntity} to be wrapped
     */
    public DeflateDecompressingEntity(final HttpEntity entity) {
        super(entity);
    }

    /**
     * Returns the non-null InputStream that should be returned to by all requests to
     * {@link #getContent()}.
     *
     * @return a non-null InputStream
     * @throws IOException if there was a problem
     */
    @Override
    InputStream getDecompressingInputStream(final InputStream wrapped) throws IOException {
        /*
         * A zlib stream will have a header.
         *
         * CMF | FLG [| DICTID ] | ...compressed data | ADLER32 |
         *
         * * CMF is one byte.
         *
         * * FLG is one byte.
         *
         * * DICTID is four bytes, and only present if FLG.FDICT is set.
         *
         * Sniff the content. Does it look like a zlib stream, with a CMF, etc? c.f. RFC1950,
         * section 2.2. http://tools.ietf.org/html/rfc1950#page-4
         *
         * We need to see if it looks like a proper zlib stream, or whether it is just a deflate
         * stream. RFC2616 calls zlib streams deflate. Confusing, isn't it? That's why some servers
         * implement deflate Content-Encoding using deflate streams, rather than zlib streams.
         *
         * We could start looking at the bytes, but to be honest, someone else has already read
         * the RFCs and implemented that for us. So we'll just use the JDK libraries and exception
         * handling to do this. If that proves slow, then we could potentially change this to check
         * the first byte - does it look like a CMF? What about the second byte - does it look like
         * a FLG, etc.
         */

        /* We read a small buffer to sniff the content. */
        byte[] peeked = new byte[6];

        PushbackInputStream pushback = new PushbackInputStream(wrapped, peeked.length);

        int headerLength = pushback.read(peeked);

        if (headerLength == -1) {
            throw new IOException("Unable to read the response");
        }

        /* We try to read the first uncompressed byte. */
        byte[] dummy = new byte[1];

        Inflater inf = new Inflater();

        try {
            int n;
            while ((n = inf.inflate(dummy)) == 0) {
                if (inf.finished()) {

                    /* Not expecting this, so fail loudly. */
                    throw new IOException("Unable to read the response");
                }

                if (inf.needsDictionary()) {

                    /* Need dictionary - then it must be zlib stream with DICTID part? */
                    break;
                }

                if (inf.needsInput()) {
                    inf.setInput(peeked);
                }
            }

            if (n == -1) {
                throw new IOException("Unable to read the response");
            }

            /*
             * We read something without a problem, so it's a valid zlib stream. Just need to reset
             * and return an unused InputStream now.
             */
            pushback.unread(peeked, 0, headerLength);
            return new InflaterInputStream(pushback);
        } catch (DataFormatException e) {

            /* Presume that it's an RFC1951 deflate stream rather than RFC1950 zlib stream and try
             * again. */
            pushback.unread(peeked, 0, headerLength);
            return new InflaterInputStream(pushback, new Inflater(true));
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Header getContentEncoding() {

        /* This HttpEntityWrapper has dealt with the Content-Encoding. */
        return null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public long getContentLength() {

        /* Length of inflated content is unknown. */
        return -1;
    }

}
