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

package ch.boye.httpclientandroidlib.client.protocol;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import ch.boye.httpclientandroidlib.annotation.Immutable;

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpRequestInterceptor;
import ch.boye.httpclientandroidlib.ProtocolException;
import ch.boye.httpclientandroidlib.client.CookieStore;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;
import ch.boye.httpclientandroidlib.client.params.HttpClientParams;
import ch.boye.httpclientandroidlib.conn.HttpRoutedConnection;
import ch.boye.httpclientandroidlib.conn.routing.HttpRoute;
import ch.boye.httpclientandroidlib.cookie.Cookie;
import ch.boye.httpclientandroidlib.cookie.CookieOrigin;
import ch.boye.httpclientandroidlib.cookie.CookieSpec;
import ch.boye.httpclientandroidlib.cookie.CookieSpecRegistry;
import ch.boye.httpclientandroidlib.cookie.SetCookie2;
import ch.boye.httpclientandroidlib.protocol.HttpContext;
import ch.boye.httpclientandroidlib.protocol.ExecutionContext;

/**
 * Request interceptor that matches cookies available in the current
 * {@link CookieStore} to the request being executed and generates
 * corresponding <code>Cookie</code> request headers.
 * <p>
 * The following parameters can be used to customize the behavior of this
 * class:
 * <ul>
 *  <li>{@link ch.boye.httpclientandroidlib.cookie.params.CookieSpecPNames#DATE_PATTERNS}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.cookie.params.CookieSpecPNames#SINGLE_COOKIE_HEADER}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#COOKIE_POLICY}</li>
 * </ul>
 *
 * @since 4.0
 */
@Immutable
public class RequestAddCookies implements HttpRequestInterceptor {

    public HttpClientAndroidLog log = new HttpClientAndroidLog(getClass());

    public RequestAddCookies() {
        super();
    }

    public void process(final HttpRequest request, final HttpContext context)
            throws HttpException, IOException {
        if (request == null) {
            throw new IllegalArgumentException("HTTP request may not be null");
        }
        if (context == null) {
            throw new IllegalArgumentException("HTTP context may not be null");
        }

        String method = request.getRequestLine().getMethod();
        if (method.equalsIgnoreCase("CONNECT")) {
            return;
        }

        // Obtain cookie store
        CookieStore cookieStore = (CookieStore) context.getAttribute(
                ClientContext.COOKIE_STORE);
        if (cookieStore == null) {
            this.log.debug("Cookie store not specified in HTTP context");
            return;
        }

        // Obtain the registry of cookie specs
        CookieSpecRegistry registry = (CookieSpecRegistry) context.getAttribute(
                ClientContext.COOKIESPEC_REGISTRY);
        if (registry == null) {
            this.log.debug("CookieSpec registry not specified in HTTP context");
            return;
        }

        // Obtain the target host (required)
        HttpHost targetHost = (HttpHost) context.getAttribute(
                ExecutionContext.HTTP_TARGET_HOST);
        if (targetHost == null) {
            this.log.debug("Target host not set in the context");
            return;
        }

        // Obtain the client connection (required)
        HttpRoutedConnection conn = (HttpRoutedConnection) context.getAttribute(
                ExecutionContext.HTTP_CONNECTION);
        if (conn == null) {
            this.log.debug("HTTP connection not set in the context");
            return;
        }

        String policy = HttpClientParams.getCookiePolicy(request.getParams());
        if (this.log.isDebugEnabled()) {
            this.log.debug("CookieSpec selected: " + policy);
        }

        URI requestURI;
        if (request instanceof HttpUriRequest) {
            requestURI = ((HttpUriRequest) request).getURI();
        } else {
            try {
                requestURI = new URI(request.getRequestLine().getUri());
            } catch (URISyntaxException ex) {
                throw new ProtocolException("Invalid request URI: " +
                        request.getRequestLine().getUri(), ex);
            }
        }

        String hostName = targetHost.getHostName();
        int port = targetHost.getPort();
        if (port < 0) {
            HttpRoute route = conn.getRoute();
            if (route.getHopCount() == 1) {
                port = conn.getRemotePort();
            } else {
                // Target port will be selected by the proxy.
                // Use conventional ports for known schemes
                String scheme = targetHost.getSchemeName();
                if (scheme.equalsIgnoreCase("http")) {
                    port = 80;
                } else if (scheme.equalsIgnoreCase("https")) {
                    port = 443;
                } else {
                    port = 0;
                }
            }
        }

        CookieOrigin cookieOrigin = new CookieOrigin(
                hostName,
                port,
                requestURI.getPath(),
                conn.isSecure());

        // Get an instance of the selected cookie policy
        CookieSpec cookieSpec = registry.getCookieSpec(policy, request.getParams());
        // Get all cookies available in the HTTP state
        List<Cookie> cookies = new ArrayList<Cookie>(cookieStore.getCookies());
        // Find cookies matching the given origin
        List<Cookie> matchedCookies = new ArrayList<Cookie>();
        Date now = new Date();
        for (Cookie cookie : cookies) {
            if (!cookie.isExpired(now)) {
                if (cookieSpec.match(cookie, cookieOrigin)) {
                    if (this.log.isDebugEnabled()) {
                        this.log.debug("Cookie " + cookie + " match " + cookieOrigin);
                    }
                    matchedCookies.add(cookie);
                }
            } else {
                if (this.log.isDebugEnabled()) {
                    this.log.debug("Cookie " + cookie + " expired");
                }
            }
        }
        // Generate Cookie request headers
        if (!matchedCookies.isEmpty()) {
            List<Header> headers = cookieSpec.formatCookies(matchedCookies);
            for (Header header : headers) {
                request.addHeader(header);
            }
        }

        int ver = cookieSpec.getVersion();
        if (ver > 0) {
            boolean needVersionHeader = false;
            for (Cookie cookie : matchedCookies) {
                if (ver != cookie.getVersion() || !(cookie instanceof SetCookie2)) {
                    needVersionHeader = true;
                }
            }

            if (needVersionHeader) {
                Header header = cookieSpec.getVersionHeader();
                if (header != null) {
                    // Advertise cookie version support
                    request.addHeader(header);
                }
            }
        }

        // Stick the CookieSpec and CookieOrigin instances to the HTTP context
        // so they could be obtained by the response interceptor
        context.setAttribute(ClientContext.COOKIE_SPEC, cookieSpec);
        context.setAttribute(ClientContext.COOKIE_ORIGIN, cookieOrigin);
    }

}
