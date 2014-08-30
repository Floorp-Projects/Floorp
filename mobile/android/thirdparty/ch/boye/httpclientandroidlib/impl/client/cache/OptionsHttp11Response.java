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
package ch.boye.httpclientandroidlib.impl.client.cache;

import java.util.Locale;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HeaderIterator;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.StatusLine;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.message.AbstractHttpMessage;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;
import ch.boye.httpclientandroidlib.params.BasicHttpParams;
import ch.boye.httpclientandroidlib.params.HttpParams;

/**
 * @since 4.1
 */
@SuppressWarnings("deprecation")
@Immutable
final class OptionsHttp11Response extends AbstractHttpMessage implements HttpResponse {

    private final StatusLine statusLine = new BasicStatusLine(HttpVersion.HTTP_1_1,
            HttpStatus.SC_NOT_IMPLEMENTED, "");
    private final ProtocolVersion version = HttpVersion.HTTP_1_1;

    public StatusLine getStatusLine() {
        return statusLine;
    }

    public void setStatusLine(final StatusLine statusline) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    public void setStatusLine(final ProtocolVersion ver, final int code) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    public void setStatusLine(final ProtocolVersion ver, final int code, final String reason) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    public void setStatusCode(final int code) throws IllegalStateException {
        // No-op on purpose, this class is not going to be doing any work.
    }

    public void setReasonPhrase(final String reason) throws IllegalStateException {
        // No-op on purpose, this class is not going to be doing any work.
    }

    public HttpEntity getEntity() {
        return null;
    }

    public void setEntity(final HttpEntity entity) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    public Locale getLocale() {
        return null;
    }

    public void setLocale(final Locale loc) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    public ProtocolVersion getProtocolVersion() {
        return version;
    }

    @Override
    public boolean containsHeader(final String name) {
        return this.headergroup.containsHeader(name);
    }

    @Override
    public Header[] getHeaders(final String name) {
        return this.headergroup.getHeaders(name);
    }

    @Override
    public Header getFirstHeader(final String name) {
        return this.headergroup.getFirstHeader(name);
    }

    @Override
    public Header getLastHeader(final String name) {
        return this.headergroup.getLastHeader(name);
    }

    @Override
    public Header[] getAllHeaders() {
        return this.headergroup.getAllHeaders();
    }

    @Override
    public void addHeader(final Header header) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    @Override
    public void addHeader(final String name, final String value) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    @Override
    public void setHeader(final Header header) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    @Override
    public void setHeader(final String name, final String value) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    @Override
    public void setHeaders(final Header[] headers) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    @Override
    public void removeHeader(final Header header) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    @Override
    public void removeHeaders(final String name) {
        // No-op on purpose, this class is not going to be doing any work.
    }

    @Override
    public HeaderIterator headerIterator() {
        return this.headergroup.iterator();
    }

    @Override
    public HeaderIterator headerIterator(final String name) {
        return this.headergroup.iterator(name);
    }

    @Override
    public HttpParams getParams() {
        if (this.params == null) {
            this.params = new BasicHttpParams();
        }
        return this.params;
    }

    @Override
    public void setParams(final HttpParams params) {
        // No-op on purpose, this class is not going to be doing any work.
    }
}
