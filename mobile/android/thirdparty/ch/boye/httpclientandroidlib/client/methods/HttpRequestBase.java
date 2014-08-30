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

import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.RequestLine;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.client.config.RequestConfig;
import ch.boye.httpclientandroidlib.message.BasicRequestLine;
import ch.boye.httpclientandroidlib.params.HttpProtocolParams;

/**
 * Base implementation of {@link HttpUriRequest}.
 *
 * @since 4.0
 */
@SuppressWarnings("deprecation")
@NotThreadSafe
public abstract class HttpRequestBase extends AbstractExecutionAwareRequest
    implements HttpUriRequest, Configurable {

    private ProtocolVersion version;
    private URI uri;
    private RequestConfig config;

    public abstract String getMethod();

    /**
     * @since 4.3
     */
    public void setProtocolVersion(final ProtocolVersion version) {
        this.version = version;
    }

    public ProtocolVersion getProtocolVersion() {
        return version != null ? version : HttpProtocolParams.getVersion(getParams());
    }

    /**
     * Returns the original request URI.
     * <p>
     * Please note URI remains unchanged in the course of request execution and
     * is not updated if the request is redirected to another location.
     */
    public URI getURI() {
        return this.uri;
    }

    public RequestLine getRequestLine() {
        final String method = getMethod();
        final ProtocolVersion ver = getProtocolVersion();
        final URI uri = getURI();
        String uritext = null;
        if (uri != null) {
            uritext = uri.toASCIIString();
        }
        if (uritext == null || uritext.length() == 0) {
            uritext = "/";
        }
        return new BasicRequestLine(method, uritext, ver);
    }


    public RequestConfig getConfig() {
        return config;
    }

    public void setConfig(final RequestConfig config) {
        this.config = config;
    }

    public void setURI(final URI uri) {
        this.uri = uri;
    }

    /**
     * @since 4.2
     */
    public void started() {
    }

    /**
     * A convenience method to simplify migration from HttpClient 3.1 API. This method is
     * equivalent to {@link #reset()}.
     *
     * @since 4.2
     */
    public void releaseConnection() {
        reset();
    }

    @Override
    public String toString() {
        return getMethod() + " " + getURI() + " " + getProtocolVersion();
    }

}
