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

import java.io.IOException;
import java.net.URI;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.RequestLine;
import ch.boye.httpclientandroidlib.client.utils.CloneUtils;
import ch.boye.httpclientandroidlib.conn.ClientConnectionRequest;
import ch.boye.httpclientandroidlib.conn.ConnectionReleaseTrigger;
import ch.boye.httpclientandroidlib.message.AbstractHttpMessage;
import ch.boye.httpclientandroidlib.message.BasicRequestLine;
import ch.boye.httpclientandroidlib.message.HeaderGroup;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.params.HttpProtocolParams;

/**
 * Basic implementation of an HTTP request that can be modified.
 *
 *
 * @since 4.0
 */
@NotThreadSafe
public abstract class HttpRequestBase extends AbstractHttpMessage
    implements HttpUriRequest, AbortableHttpRequest, Cloneable {

    private Lock abortLock;

    private boolean aborted;

    private URI uri;
    private ClientConnectionRequest connRequest;
    private ConnectionReleaseTrigger releaseTrigger;

    public HttpRequestBase() {
        super();
        this.abortLock = new ReentrantLock();
    }

    public abstract String getMethod();

    public ProtocolVersion getProtocolVersion() {
        return HttpProtocolParams.getVersion(getParams());
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
        String method = getMethod();
        ProtocolVersion ver = getProtocolVersion();
        URI uri = getURI();
        String uritext = null;
        if (uri != null) {
            uritext = uri.toASCIIString();
        }
        if (uritext == null || uritext.length() == 0) {
            uritext = "/";
        }
        return new BasicRequestLine(method, uritext, ver);
    }

    public void setURI(final URI uri) {
        this.uri = uri;
    }

    public void setConnectionRequest(final ClientConnectionRequest connRequest)
            throws IOException {
        this.abortLock.lock();
        try {
            if (this.aborted) {
                throw new IOException("Request already aborted");
            }

            this.releaseTrigger = null;
            this.connRequest = connRequest;
        } finally {
            this.abortLock.unlock();
        }
    }

    public void setReleaseTrigger(final ConnectionReleaseTrigger releaseTrigger)
            throws IOException {
        this.abortLock.lock();
        try {
            if (this.aborted) {
                throw new IOException("Request already aborted");
            }

            this.connRequest = null;
            this.releaseTrigger = releaseTrigger;
        } finally {
            this.abortLock.unlock();
        }
    }

    public void abort() {
        ClientConnectionRequest localRequest;
        ConnectionReleaseTrigger localTrigger;

        this.abortLock.lock();
        try {
            if (this.aborted) {
                return;
            }
            this.aborted = true;

            localRequest = connRequest;
            localTrigger = releaseTrigger;
        } finally {
            this.abortLock.unlock();
        }

        // Trigger the callbacks outside of the lock, to prevent
        // deadlocks in the scenario where the callbacks have
        // their own locks that may be used while calling
        // setReleaseTrigger or setConnectionRequest.
        if (localRequest != null) {
            localRequest.abortRequest();
        }
        if (localTrigger != null) {
            try {
                localTrigger.abortConnection();
            } catch (IOException ex) {
                // ignore
            }
        }
    }

    public boolean isAborted() {
        return this.aborted;
    }

    @Override
    public Object clone() throws CloneNotSupportedException {
        HttpRequestBase clone = (HttpRequestBase) super.clone();
        clone.abortLock = new ReentrantLock();
        clone.aborted = false;
        clone.releaseTrigger = null;
        clone.connRequest = null;
        clone.headergroup = (HeaderGroup) CloneUtils.clone(this.headergroup);
        clone.params = (HttpParams) CloneUtils.clone(this.params);
        return clone;
    }

}
