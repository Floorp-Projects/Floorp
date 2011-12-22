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

package ch.boye.httpclientandroidlib.impl.client;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.ConnectException;
import java.net.UnknownHostException;

import javax.net.ssl.SSLException;

import ch.boye.httpclientandroidlib.annotation.Immutable;

import ch.boye.httpclientandroidlib.HttpEntityEnclosingRequest;
import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.client.HttpRequestRetryHandler;
import ch.boye.httpclientandroidlib.protocol.HttpContext;
import ch.boye.httpclientandroidlib.protocol.ExecutionContext;

/**
 * The default {@link HttpRequestRetryHandler} used by request executors.
 *
 *
 * @since 4.0
 */
@Immutable
public class DefaultHttpRequestRetryHandler implements HttpRequestRetryHandler {

    /** the number of times a method will be retried */
    private final int retryCount;

    /** Whether or not methods that have successfully sent their request will be retried */
    private final boolean requestSentRetryEnabled;

    /**
     * Default constructor
     */
    public DefaultHttpRequestRetryHandler(int retryCount, boolean requestSentRetryEnabled) {
        super();
        this.retryCount = retryCount;
        this.requestSentRetryEnabled = requestSentRetryEnabled;
    }

    /**
     * Default constructor
     */
    public DefaultHttpRequestRetryHandler() {
        this(3, false);
    }
    /**
     * Used <code>retryCount</code> and <code>requestSentRetryEnabled</code> to determine
     * if the given method should be retried.
     */
    public boolean retryRequest(
            final IOException exception,
            int executionCount,
            final HttpContext context) {
        if (exception == null) {
            throw new IllegalArgumentException("Exception parameter may not be null");
        }
        if (context == null) {
            throw new IllegalArgumentException("HTTP context may not be null");
        }
        if (executionCount > this.retryCount) {
            // Do not retry if over max retry count
            return false;
        }
        if (exception instanceof InterruptedIOException) {
            // Timeout
            return false;
        }
        if (exception instanceof UnknownHostException) {
            // Unknown host
            return false;
        }
        if (exception instanceof ConnectException) {
            // Connection refused
            return false;
        }
        if (exception instanceof SSLException) {
            // SSL handshake exception
            return false;
        }

        HttpRequest request = (HttpRequest)
            context.getAttribute(ExecutionContext.HTTP_REQUEST);
        if (handleAsIdempotent(request)) {
            // Retry if the request is considered idempotent
            return true;
        }

        Boolean b = (Boolean)
            context.getAttribute(ExecutionContext.HTTP_REQ_SENT);
        boolean sent = (b != null && b.booleanValue());

        if (!sent || this.requestSentRetryEnabled) {
            // Retry if the request has not been sent fully or
            // if it's OK to retry methods that have been sent
            return true;
        }
        // otherwise do not retry
        return false;
    }

    /**
     * @return <code>true</code> if this handler will retry methods that have
     * successfully sent their request, <code>false</code> otherwise
     */
    public boolean isRequestSentRetryEnabled() {
        return requestSentRetryEnabled;
    }

    /**
     * @return the maximum number of times a method will be retried
     */
    public int getRetryCount() {
        return retryCount;
    }

    private boolean handleAsIdempotent(final HttpRequest request) {
        return !(request instanceof HttpEntityEnclosingRequest);
    }

}
