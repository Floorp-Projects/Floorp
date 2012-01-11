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

package ch.boye.httpclientandroidlib.impl.client;

import java.net.URI;
import java.net.URISyntaxException;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.ProtocolException;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.RequestLine;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.message.AbstractHttpMessage;
import ch.boye.httpclientandroidlib.message.BasicRequestLine;
import ch.boye.httpclientandroidlib.params.HttpProtocolParams;

/**
 * A wrapper class for {@link HttpRequest}s that can be used to change
 * properties of the current request without modifying the original
 * object.
 * </p>
 * This class is also capable of resetting the request headers to
 * the state of the original request.
 *
 *
 * @since 4.0
 */
@NotThreadSafe
public class RequestWrapper extends AbstractHttpMessage implements HttpUriRequest {

    private final HttpRequest original;

    private URI uri;
    private String method;
    private ProtocolVersion version;
    private int execCount;

    public RequestWrapper(final HttpRequest request) throws ProtocolException {
        super();
        if (request == null) {
            throw new IllegalArgumentException("HTTP request may not be null");
        }
        this.original = request;
        setParams(request.getParams());
        setHeaders(request.getAllHeaders());
        // Make a copy of the original URI
        if (request instanceof HttpUriRequest) {
            this.uri = ((HttpUriRequest) request).getURI();
            this.method = ((HttpUriRequest) request).getMethod();
            this.version = null;
        } else {
            RequestLine requestLine = request.getRequestLine();
            try {
                this.uri = new URI(requestLine.getUri());
            } catch (URISyntaxException ex) {
                throw new ProtocolException("Invalid request URI: "
                        + requestLine.getUri(), ex);
            }
            this.method = requestLine.getMethod();
            this.version = request.getProtocolVersion();
        }
        this.execCount = 0;
    }

    public void resetHeaders() {
        // Make a copy of original headers
        this.headergroup.clear();
        setHeaders(this.original.getAllHeaders());
    }

    public String getMethod() {
        return this.method;
    }

    public void setMethod(final String method) {
        if (method == null) {
            throw new IllegalArgumentException("Method name may not be null");
        }
        this.method = method;
    }

    public ProtocolVersion getProtocolVersion() {
        if (this.version == null) {
            this.version = HttpProtocolParams.getVersion(getParams());
        }
        return this.version;
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

    public RequestLine getRequestLine() {
        String method = getMethod();
        ProtocolVersion ver = getProtocolVersion();
        String uritext = null;
        if (uri != null) {
            uritext = uri.toASCIIString();
        }
        if (uritext == null || uritext.length() == 0) {
            uritext = "/";
        }
        return new BasicRequestLine(method, uritext, ver);
    }

    public void abort() throws UnsupportedOperationException {
        throw new UnsupportedOperationException();
    }

    public boolean isAborted() {
        return false;
    }

    public HttpRequest getOriginal() {
        return this.original;
    }

    public boolean isRepeatable() {
        return true;
    }

    public int getExecCount() {
        return this.execCount;
    }

    public void incrementExecCount() {
        this.execCount++;
    }

}
