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

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpRequestInterceptor;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.auth.AuthScheme;
import ch.boye.httpclientandroidlib.auth.AuthScope;
import ch.boye.httpclientandroidlib.auth.AuthState;
import ch.boye.httpclientandroidlib.auth.Credentials;
import ch.boye.httpclientandroidlib.client.AuthCache;
import ch.boye.httpclientandroidlib.client.CredentialsProvider;
import ch.boye.httpclientandroidlib.protocol.ExecutionContext;
import ch.boye.httpclientandroidlib.protocol.HttpContext;

/**
 * Request interceptor that can preemptively authenticate against known hosts,
 * if there is a cached {@link AuthScheme} instance in the local
 * {@link AuthCache} associated with the given target or proxy host.
 *
 * @since 4.1
 */
@Immutable
public class RequestAuthCache implements HttpRequestInterceptor {

    public HttpClientAndroidLog log = new HttpClientAndroidLog(getClass());

    public RequestAuthCache() {
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

        AuthCache authCache = (AuthCache) context.getAttribute(ClientContext.AUTH_CACHE);
        if (authCache == null) {
            this.log.debug("Auth cache not set in the context");
            return;
        }

        CredentialsProvider credsProvider = (CredentialsProvider) context.getAttribute(
                ClientContext.CREDS_PROVIDER);
        if (credsProvider == null) {
            this.log.debug("Credentials provider not set in the context");
            return;
        }

        HttpHost target = (HttpHost) context.getAttribute(ExecutionContext.HTTP_TARGET_HOST);
        AuthState targetState = (AuthState) context.getAttribute(ClientContext.TARGET_AUTH_STATE);
        if (target != null && targetState != null && targetState.getAuthScheme() == null) {
            AuthScheme authScheme = authCache.get(target);
            if (authScheme != null) {
                doPreemptiveAuth(target, authScheme, targetState, credsProvider);
            }
        }

        HttpHost proxy = (HttpHost) context.getAttribute(ExecutionContext.HTTP_PROXY_HOST);
        AuthState proxyState = (AuthState) context.getAttribute(ClientContext.PROXY_AUTH_STATE);
        if (proxy != null && proxyState != null && proxyState.getAuthScheme() == null) {
            AuthScheme authScheme = authCache.get(proxy);
            if (authScheme != null) {
                doPreemptiveAuth(proxy, authScheme, proxyState, credsProvider);
            }
        }
    }

    private void doPreemptiveAuth(
            final HttpHost host,
            final AuthScheme authScheme,
            final AuthState authState,
            final CredentialsProvider credsProvider) {
        String schemeName = authScheme.getSchemeName();
        if (this.log.isDebugEnabled()) {
            this.log.debug("Re-using cached '" + schemeName + "' auth scheme for " + host);
        }

        AuthScope authScope = new AuthScope(host.getHostName(), host.getPort(),
                AuthScope.ANY_REALM, schemeName);
        Credentials creds = credsProvider.getCredentials(authScope);

        if (creds != null) {
            authState.setAuthScheme(authScheme);
            authState.setCredentials(creds);
        } else {
            this.log.debug("No credentials for preemptive authentication");
        }
    }

}
