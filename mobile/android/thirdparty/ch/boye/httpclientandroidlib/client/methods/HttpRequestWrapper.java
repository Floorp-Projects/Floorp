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

package ch.boye.httpclientandroidlib.client.methods;

import java.net.URI;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.RequestLine;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.message.AbstractHttpMessage;
import ch.boye.httpclientandroidlib.message.BasicRequestLine;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.HTTP;

/**
 * A wrapper class for {@link HttpRequest} that can be used to change properties of the current
 * request without modifying the original object.
 *
 * @since 4.3
 */
@SuppressWarnings("deprecation")
@NotThreadSafe
public class HttpRequestWrapper extends AbstractHttpMessage implements HttpUriRequest {

    private final HttpRequest original;
    private final String method;
    private ProtocolVersion version;
    private URI uri;

    private HttpRequestWrapper(final HttpRequest request) {
        super();
        this.original = request;
        this.version = this.original.getRequestLine().getProtocolVersion();
        this.method = this.original.getRequestLine().getMethod();
        if (request instanceof HttpUriRequest) {
            this.uri = ((HttpUriRequest) request).getURI();
        } else {
            this.uri = null;
        }
        setHeaders(request.getAllHeaders());
    }

    public ProtocolVersion getProtocolVersion() {
        return this.version != null ? this.version : this.original.getProtocolVersion();
    }

    public void setProtocolVersion(final ProtocolVersion version) {
        this.version = version;
    }

    public URI getURI() {
        return this.uri;
    }

    public void setURI(final URI uri) {
        this.uri = uri;
    }

    public String getMethod() {
        return method;
    }

    public void abort() throws UnsupportedOperationException {
        throw new UnsupportedOperationException();
    }

    public boolean isAborted() {
        return false;
    }

    public RequestLine getRequestLine() {
        String requestUri = null;
        if (this.uri != null) {
            requestUri = this.uri.toASCIIString();
        } else {
            requestUri = this.original.getRequestLine().getUri();
        }
        if (requestUri == null || requestUri.length() == 0) {
            requestUri = "/";
        }
        return new BasicRequestLine(this.method, requestUri, getProtocolVersion());
    }

    public HttpRequest getOriginal() {
        return this.original;
    }

    @Override
    public String toString() {
        return getRequestLine() + " " + this.headergroup;
    }

    static class HttpEntityEnclosingRequestWrapper extends HttpRequestWrapper
        implements HttpEntityEnclosingRequest {

        private HttpEntity entity;

        public HttpEntityEnclosingRequestWrapper(final HttpEntityEnclosingRequest request) {
            super(request);
            this.entity = request.getEntity();
        }

        public HttpEntity getEntity() {
            return this.entity;
        }

        public void setEntity(final HttpEntity entity) {
            this.entity = entity;
        }

        public boolean expectContinue() {
            final Header expect = getFirstHeader(HTTP.EXPECT_DIRECTIVE);
            return expect != null && HTTP.EXPECT_CONTINUE.equalsIgnoreCase(expect.getValue());
        }

    }

    public static HttpRequestWrapper wrap(final HttpRequest request) {
        if (request == null) {
            return null;
        }
        if (request instanceof HttpEntityEnclosingRequest) {
            return new HttpEntityEnclosingRequestWrapper((HttpEntityEnclosingRequest) request);
        } else {
            return new HttpRequestWrapper(request);
        }
    }

    /**
     * @deprecated (4.3) use
     *   {@link ch.boye.httpclientandroidlib.client.config.RequestConfig}.
     */
    @Override
    @Deprecated
    public HttpParams getParams() {
        if (this.params == null) {
            this.params = original.getParams().copy();
        }
        return this.params;
    }

}
