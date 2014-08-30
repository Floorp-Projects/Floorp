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
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HeaderIterator;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.NameValuePair;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.client.config.RequestConfig;
import ch.boye.httpclientandroidlib.client.entity.UrlEncodedFormEntity;
import ch.boye.httpclientandroidlib.client.utils.URIBuilder;
import ch.boye.httpclientandroidlib.message.BasicHeader;
import ch.boye.httpclientandroidlib.message.BasicNameValuePair;
import ch.boye.httpclientandroidlib.message.HeaderGroup;
import ch.boye.httpclientandroidlib.protocol.HTTP;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * Builder for {@link HttpUriRequest} instances.
 * <p/>
 * Please note that this class treats parameters differently depending on composition
 * of the request: if the request has a content entity explicitly set with
 * {@link #setEntity(ch.boye.httpclientandroidlib.HttpEntity)} or it is not an entity enclosing method
 * (such as POST or PUT), parameters will be added to the query component of the request URI.
 * Otherwise, parameters will be added as a URL encoded {@link UrlEncodedFormEntity entity}.
 *
 * @since 4.3
 */
@NotThreadSafe
public class RequestBuilder {

    private String method;
    private ProtocolVersion version;
    private URI uri;
    private HeaderGroup headergroup;
    private HttpEntity entity;
    private LinkedList<NameValuePair> parameters;
    private RequestConfig config;

    RequestBuilder(final String method) {
        super();
        this.method = method;
    }

    RequestBuilder() {
        this(null);
    }

    public static RequestBuilder create(final String method) {
        Args.notBlank(method, "HTTP method");
        return new RequestBuilder(method);
    }

    public static RequestBuilder get() {
        return new RequestBuilder(HttpGet.METHOD_NAME);
    }

    public static RequestBuilder head() {
        return new RequestBuilder(HttpHead.METHOD_NAME);
    }

    public static RequestBuilder post() {
        return new RequestBuilder(HttpPost.METHOD_NAME);
    }

    public static RequestBuilder put() {
        return new RequestBuilder(HttpPut.METHOD_NAME);
    }

    public static RequestBuilder delete() {
        return new RequestBuilder(HttpDelete.METHOD_NAME);
    }

    public static RequestBuilder trace() {
        return new RequestBuilder(HttpTrace.METHOD_NAME);
    }

    public static RequestBuilder options() {
        return new RequestBuilder(HttpOptions.METHOD_NAME);
    }

    public static RequestBuilder copy(final HttpRequest request) {
        Args.notNull(request, "HTTP request");
        return new RequestBuilder().doCopy(request);
    }

    private RequestBuilder doCopy(final HttpRequest request) {
        if (request == null) {
            return this;
        }
        method = request.getRequestLine().getMethod();
        version = request.getRequestLine().getProtocolVersion();
        if (request instanceof HttpUriRequest) {
            uri = ((HttpUriRequest) request).getURI();
        } else {
            uri = URI.create(request.getRequestLine().getUri());
        }
        if (headergroup == null) {
            headergroup = new HeaderGroup();
        }
        headergroup.clear();
        headergroup.setHeaders(request.getAllHeaders());
        if (request instanceof HttpEntityEnclosingRequest) {
            entity = ((HttpEntityEnclosingRequest) request).getEntity();
        } else {
            entity = null;
        }
        if (request instanceof Configurable) {
            this.config = ((Configurable) request).getConfig();
        } else {
            this.config = null;
        }
        this.parameters = null;
        return this;
    }

    public String getMethod() {
        return method;
    }

    public ProtocolVersion getVersion() {
        return version;
    }

    public RequestBuilder setVersion(final ProtocolVersion version) {
        this.version = version;
        return this;
    }

    public URI getUri() {
        return uri;
    }

    public RequestBuilder setUri(final URI uri) {
        this.uri = uri;
        return this;
    }

    public RequestBuilder setUri(final String uri) {
        this.uri = uri != null ? URI.create(uri) : null;
        return this;
    }

    public Header getFirstHeader(final String name) {
        return headergroup != null ? headergroup.getFirstHeader(name) : null;
    }

    public Header getLastHeader(final String name) {
        return headergroup != null ? headergroup.getLastHeader(name) : null;
    }

    public Header[] getHeaders(final String name) {
        return headergroup != null ? headergroup.getHeaders(name) : null;
    }

    public RequestBuilder addHeader(final Header header) {
        if (headergroup == null) {
            headergroup = new HeaderGroup();
        }
        headergroup.addHeader(header);
        return this;
    }

    public RequestBuilder addHeader(final String name, final String value) {
        if (headergroup == null) {
            headergroup = new HeaderGroup();
        }
        this.headergroup.addHeader(new BasicHeader(name, value));
        return this;
    }

    public RequestBuilder removeHeader(final Header header) {
        if (headergroup == null) {
            headergroup = new HeaderGroup();
        }
        headergroup.removeHeader(header);
        return this;
    }

    public RequestBuilder removeHeaders(final String name) {
        if (name == null || headergroup == null) {
            return this;
        }
        for (final HeaderIterator i = headergroup.iterator(); i.hasNext(); ) {
            final Header header = i.nextHeader();
            if (name.equalsIgnoreCase(header.getName())) {
                i.remove();
            }
        }
        return this;
    }

    public RequestBuilder setHeader(final Header header) {
        if (headergroup == null) {
            headergroup = new HeaderGroup();
        }
        this.headergroup.updateHeader(header);
        return this;
    }

    public RequestBuilder setHeader(final String name, final String value) {
        if (headergroup == null) {
            headergroup = new HeaderGroup();
        }
        this.headergroup.updateHeader(new BasicHeader(name, value));
        return this;
    }

    public HttpEntity getEntity() {
        return entity;
    }

    public RequestBuilder setEntity(final HttpEntity entity) {
        this.entity = entity;
        return this;
    }

    public List<NameValuePair> getParameters() {
        return parameters != null ? new ArrayList<NameValuePair>(parameters) :
            new ArrayList<NameValuePair>();
    }

    public RequestBuilder addParameter(final NameValuePair nvp) {
        Args.notNull(nvp, "Name value pair");
        if (parameters == null) {
            parameters = new LinkedList<NameValuePair>();
        }
        parameters.add(nvp);
        return this;
    }

    public RequestBuilder addParameter(final String name, final String value) {
        return addParameter(new BasicNameValuePair(name, value));
    }

    public RequestBuilder addParameters(final NameValuePair... nvps) {
        for (final NameValuePair nvp: nvps) {
            addParameter(nvp);
        }
        return this;
    }

    public RequestConfig getConfig() {
        return config;
    }

    public RequestBuilder setConfig(final RequestConfig config) {
        this.config = config;
        return this;
    }

    public HttpUriRequest build() {
        final HttpRequestBase result;
        URI uri = this.uri != null ? this.uri : URI.create("/");
        HttpEntity entity = this.entity;
        if (parameters != null && !parameters.isEmpty()) {
            if (entity == null && (HttpPost.METHOD_NAME.equalsIgnoreCase(method)
                    || HttpPut.METHOD_NAME.equalsIgnoreCase(method))) {
                entity = new UrlEncodedFormEntity(parameters, HTTP.DEF_CONTENT_CHARSET);
            } else {
                try {
                    uri = new URIBuilder(uri).addParameters(parameters).build();
                } catch (final URISyntaxException ex) {
                    // should never happen
                }
            }
        }
        if (entity == null) {
            result = new InternalRequest(method);
        } else {
            final InternalEntityEclosingRequest request = new InternalEntityEclosingRequest(method);
            request.setEntity(entity);
            result = request;
        }
        result.setProtocolVersion(this.version);
        result.setURI(uri);
        if (this.headergroup != null) {
            result.setHeaders(this.headergroup.getAllHeaders());
        }
        result.setConfig(this.config);
        return result;
    }

    static class InternalRequest extends HttpRequestBase {

        private final String method;

        InternalRequest(final String method) {
            super();
            this.method = method;
        }

        @Override
        public String getMethod() {
            return this.method;
        }

    }

    static class InternalEntityEclosingRequest extends HttpEntityEnclosingRequestBase {

        private final String method;

        InternalEntityEclosingRequest(final String method) {
            super();
            this.method = method;
        }

        @Override
        public String getMethod() {
            return this.method;
        }

    }

}
