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

package ch.boye.httpclientandroidlib.message;

import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.RequestLine;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.params.HttpProtocolParams;

/**
 * Basic implementation of {@link HttpRequest}.
 * <p>
 * The following parameters can be used to customize the behavior of this class:
 * <ul>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#PROTOCOL_VERSION}</li>
 * </ul>
 *
 * @since 4.0
 */
public class BasicHttpRequest extends AbstractHttpMessage implements HttpRequest {

    private final String method;
    private final String uri;

    private RequestLine requestline;

    /**
     * Creates an instance of this class using the given request method
     * and URI. The HTTP protocol version will be obtained from the
     * {@link HttpParams} instance associated with the object.
     * The initialization will be deferred
     * until {@link #getRequestLine()} is accessed for the first time.
     *
     * @param method request method.
     * @param uri request URI.
     */
    public BasicHttpRequest(final String method, final String uri) {
        super();
        if (method == null) {
            throw new IllegalArgumentException("Method name may not be null");
        }
        if (uri == null) {
            throw new IllegalArgumentException("Request URI may not be null");
        }
        this.method = method;
        this.uri = uri;
        this.requestline = null;
    }

    /**
     * Creates an instance of this class using the given request method, URI
     * and the HTTP protocol version.
     *
     * @param method request method.
     * @param uri request URI.
     * @param ver HTTP protocol version.
     */
    public BasicHttpRequest(final String method, final String uri, final ProtocolVersion ver) {
        this(new BasicRequestLine(method, uri, ver));
    }

    /**
     * Creates an instance of this class using the given request line.
     *
     * @param requestline request line.
     */
    public BasicHttpRequest(final RequestLine requestline) {
        super();
        if (requestline == null) {
            throw new IllegalArgumentException("Request line may not be null");
        }
        this.requestline = requestline;
        this.method = requestline.getMethod();
        this.uri = requestline.getUri();
    }

    /**
     * Returns the HTTP protocol version to be used for this request. If an
     * HTTP protocol version was not explicitly set at the construction time,
     * this method will obtain it from the {@link HttpParams} instance
     * associated with the object.
     *
     * @see #BasicHttpRequest(String, String)
     */
    public ProtocolVersion getProtocolVersion() {
        return getRequestLine().getProtocolVersion();
    }

    /**
     * Returns the request line of this request. If an HTTP protocol version
     * was not explicitly set at the construction time, this method will obtain
     * it from the {@link HttpParams} instance associated with the object.
     *
     * @see #BasicHttpRequest(String, String)
     */
    public RequestLine getRequestLine() {
        if (this.requestline == null) {
            ProtocolVersion ver = HttpProtocolParams.getVersion(getParams());
            this.requestline = new BasicRequestLine(this.method, this.uri, ver);
        }
        return this.requestline;
    }

    public String toString() {
        return this.method + " " + this.uri + " " + this.headergroup;
    }

}
