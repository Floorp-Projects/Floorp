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

import java.util.List;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

import ch.boye.httpclientandroidlib.auth.AuthSchemeRegistry;
import ch.boye.httpclientandroidlib.client.CookieStore;
import ch.boye.httpclientandroidlib.client.CredentialsProvider;
import ch.boye.httpclientandroidlib.cookie.CookieSpecRegistry;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.HttpContext;

/**
 * Configuration facade for {@link HttpContext} instances.
 *
 * @since 4.0
 */
@NotThreadSafe
public class ClientContextConfigurer implements ClientContext {

    private final HttpContext context;

    public ClientContextConfigurer (final HttpContext context) {
        if (context == null)
            throw new IllegalArgumentException("HTTP context may not be null");
        this.context = context;
    }

    public void setCookieSpecRegistry(final CookieSpecRegistry registry) {
        this.context.setAttribute(COOKIESPEC_REGISTRY, registry);
    }

    public void setAuthSchemeRegistry(final AuthSchemeRegistry registry) {
        this.context.setAttribute(AUTHSCHEME_REGISTRY, registry);
    }

    public void setCookieStore(final CookieStore store) {
        this.context.setAttribute(COOKIE_STORE, store);
    }

    public void setCredentialsProvider(final CredentialsProvider provider) {
        this.context.setAttribute(CREDS_PROVIDER, provider);
    }

    /**
     * @deprecated (4.1-alpha1) Use {@link HttpParams#setParameter(String, Object)} to set the parameters
     * {@link ch.boye.httpclientandroidlib.auth.params.AuthPNames#TARGET_AUTH_PREF AuthPNames#TARGET_AUTH_PREF} and 
     * {@link ch.boye.httpclientandroidlib.auth.params.AuthPNames#PROXY_AUTH_PREF AuthPNames#PROXY_AUTH_PREF} instead
     */
    @Deprecated
    public void setAuthSchemePref(final List<String> list) {
        this.context.setAttribute(AUTH_SCHEME_PREF, list);
    }

}
