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

import java.util.Locale;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.StatusLine;
import ch.boye.httpclientandroidlib.ReasonPhraseCatalog;

/**
 * Basic implementation of {@link HttpResponse}.
 *
 * @since 4.0
 */
public class BasicHttpResponse extends AbstractHttpMessage
    implements HttpResponse {

    private StatusLine          statusline;
    private HttpEntity          entity;
    private ReasonPhraseCatalog reasonCatalog;
    private Locale              locale;


    /**
     * Creates a new response.
     * This is the constructor to which all others map.
     *
     * @param statusline        the status line
     * @param catalog           the reason phrase catalog, or
     *                          <code>null</code> to disable automatic
     *                          reason phrase lookup
     * @param locale            the locale for looking up reason phrases, or
     *                          <code>null</code> for the system locale
     */
    public BasicHttpResponse(final StatusLine statusline,
                             final ReasonPhraseCatalog catalog,
                             final Locale locale) {
        super();
        if (statusline == null) {
            throw new IllegalArgumentException("Status line may not be null.");
        }
        this.statusline    = statusline;
        this.reasonCatalog = catalog;
        this.locale        = (locale != null) ? locale : Locale.getDefault();
    }

    /**
     * Creates a response from a status line.
     * The response will not have a reason phrase catalog and
     * use the system default locale.
     *
     * @param statusline        the status line
     */
    public BasicHttpResponse(final StatusLine statusline) {
        this(statusline, null, null);
    }

    /**
     * Creates a response from elements of a status line.
     * The response will not have a reason phrase catalog and
     * use the system default locale.
     *
     * @param ver       the protocol version of the response
     * @param code      the status code of the response
     * @param reason    the reason phrase to the status code, or
     *                  <code>null</code>
     */
    public BasicHttpResponse(final ProtocolVersion ver,
                             final int code,
                             final String reason) {
        this(new BasicStatusLine(ver, code, reason), null, null);
    }


    // non-javadoc, see interface HttpMessage
    public ProtocolVersion getProtocolVersion() {
        return this.statusline.getProtocolVersion();
    }

    // non-javadoc, see interface HttpResponse
    public StatusLine getStatusLine() {
        return this.statusline;
    }

    // non-javadoc, see interface HttpResponse
    public HttpEntity getEntity() {
        return this.entity;
    }

    // non-javadoc, see interface HttpResponse
    public Locale getLocale() {
        return this.locale;
    }

    // non-javadoc, see interface HttpResponse
    public void setStatusLine(final StatusLine statusline) {
        if (statusline == null) {
            throw new IllegalArgumentException("Status line may not be null");
        }
        this.statusline = statusline;
    }

    // non-javadoc, see interface HttpResponse
    public void setStatusLine(final ProtocolVersion ver, final int code) {
        // arguments checked in BasicStatusLine constructor
        this.statusline = new BasicStatusLine(ver, code, getReason(code));
    }

    // non-javadoc, see interface HttpResponse
    public void setStatusLine(final ProtocolVersion ver, final int code,
                              final String reason) {
        // arguments checked in BasicStatusLine constructor
        this.statusline = new BasicStatusLine(ver, code, reason);
    }

    // non-javadoc, see interface HttpResponse
    public void setStatusCode(int code) {
        // argument checked in BasicStatusLine constructor
        ProtocolVersion ver = this.statusline.getProtocolVersion();
        this.statusline = new BasicStatusLine(ver, code, getReason(code));
    }

    // non-javadoc, see interface HttpResponse
    public void setReasonPhrase(String reason) {

        if ((reason != null) && ((reason.indexOf('\n') >= 0) ||
                                 (reason.indexOf('\r') >= 0))
            ) {
            throw new IllegalArgumentException("Line break in reason phrase.");
        }
        this.statusline = new BasicStatusLine(this.statusline.getProtocolVersion(),
                                              this.statusline.getStatusCode(),
                                              reason);
    }

    // non-javadoc, see interface HttpResponse
    public void setEntity(final HttpEntity entity) {
        this.entity = entity;
    }

    // non-javadoc, see interface HttpResponse
    public void setLocale(Locale loc) {
        if (loc == null) {
            throw new IllegalArgumentException("Locale may not be null.");
        }
        this.locale = loc;
        final int code = this.statusline.getStatusCode();
        this.statusline = new BasicStatusLine
            (this.statusline.getProtocolVersion(), code, getReason(code));
    }

    /**
     * Looks up a reason phrase.
     * This method evaluates the currently set catalog and locale.
     * It also handles a missing catalog.
     *
     * @param code      the status code for which to look up the reason
     *
     * @return  the reason phrase, or <code>null</code> if there is none
     */
    protected String getReason(int code) {
        return (this.reasonCatalog == null) ?
            null : this.reasonCatalog.getReason(code, this.locale);
    }

    public String toString() {
        return this.statusline + " " + this.headergroup;
    }

}
