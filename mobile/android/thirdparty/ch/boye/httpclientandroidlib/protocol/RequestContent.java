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

package ch.boye.httpclientandroidlib.protocol;

import java.io.IOException;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpRequestInterceptor;
import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.ProtocolException;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * RequestContent is the most important interceptor for outgoing requests.
 * It is responsible for delimiting content length by adding
 * <code>Content-Length</code> or <code>Transfer-Content</code> headers based
 * on the properties of the enclosed entity and the protocol version.
 * This interceptor is required for correct functioning of client side protocol
 * processors.
 *
 * @since 4.0
 */
@Immutable
public class RequestContent implements HttpRequestInterceptor {

    private final boolean overwrite;

    /**
     * Default constructor. The <code>Content-Length</code> or <code>Transfer-Encoding</code>
     * will cause the interceptor to throw {@link ProtocolException} if already present in the
     * response message.
     */
    public RequestContent() {
        this(false);
    }

    /**
     * Constructor that can be used to fine-tune behavior of this interceptor.
     *
     * @param overwrite If set to <code>true</code> the <code>Content-Length</code> and
     * <code>Transfer-Encoding</code> headers will be created or updated if already present.
     * If set to <code>false</code> the <code>Content-Length</code> and
     * <code>Transfer-Encoding</code> headers will cause the interceptor to throw
     * {@link ProtocolException} if already present in the response message.
     *
     * @since 4.2
     */
     public RequestContent(final boolean overwrite) {
         super();
         this.overwrite = overwrite;
    }

    public void process(final HttpRequest request, final HttpContext context)
            throws HttpException, IOException {
        Args.notNull(request, "HTTP request");
        if (request instanceof HttpEntityEnclosingRequest) {
            if (this.overwrite) {
                request.removeHeaders(HTTP.TRANSFER_ENCODING);
                request.removeHeaders(HTTP.CONTENT_LEN);
            } else {
                if (request.containsHeader(HTTP.TRANSFER_ENCODING)) {
                    throw new ProtocolException("Transfer-encoding header already present");
                }
                if (request.containsHeader(HTTP.CONTENT_LEN)) {
                    throw new ProtocolException("Content-Length header already present");
                }
            }
            final ProtocolVersion ver = request.getRequestLine().getProtocolVersion();
            final HttpEntity entity = ((HttpEntityEnclosingRequest)request).getEntity();
            if (entity == null) {
                request.addHeader(HTTP.CONTENT_LEN, "0");
                return;
            }
            // Must specify a transfer encoding or a content length
            if (entity.isChunked() || entity.getContentLength() < 0) {
                if (ver.lessEquals(HttpVersion.HTTP_1_0)) {
                    throw new ProtocolException(
                            "Chunked transfer encoding not allowed for " + ver);
                }
                request.addHeader(HTTP.TRANSFER_ENCODING, HTTP.CHUNK_CODING);
            } else {
                request.addHeader(HTTP.CONTENT_LEN, Long.toString(entity.getContentLength()));
            }
            // Specify a content type if known
            if (entity.getContentType() != null && !request.containsHeader(
                    HTTP.CONTENT_TYPE )) {
                request.addHeader(entity.getContentType());
            }
            // Specify a content encoding if known
            if (entity.getContentEncoding() != null && !request.containsHeader(
                    HTTP.CONTENT_ENCODING)) {
                request.addHeader(entity.getContentEncoding());
            }
        }
    }

}
