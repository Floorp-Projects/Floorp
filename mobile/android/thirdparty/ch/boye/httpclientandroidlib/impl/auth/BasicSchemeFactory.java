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

package ch.boye.httpclientandroidlib.impl.auth;

import java.nio.charset.Charset;

import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.auth.AuthScheme;
import ch.boye.httpclientandroidlib.auth.AuthSchemeFactory;
import ch.boye.httpclientandroidlib.auth.AuthSchemeProvider;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.HttpContext;

/**
 * {@link AuthSchemeProvider} implementation that creates and initializes
 * {@link BasicScheme} instances.
 *
 * @since 4.0
 */
@Immutable
@SuppressWarnings("deprecation")
public class BasicSchemeFactory implements AuthSchemeFactory, AuthSchemeProvider {

    private final Charset charset;

    /**
     * @since 4.3
     */
    public BasicSchemeFactory(final Charset charset) {
        super();
        this.charset = charset;
    }

    public BasicSchemeFactory() {
        this(null);
    }

    public AuthScheme newInstance(final HttpParams params) {
        return new BasicScheme();
    }

    public AuthScheme create(final HttpContext context) {
        return new BasicScheme(this.charset);
    }

}
