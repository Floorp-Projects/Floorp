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
 */

package ch.boye.httpclientandroidlib.impl.client;

import ch.boye.httpclientandroidlib.annotation.ThreadSafe;
import ch.boye.httpclientandroidlib.client.protocol.RequestAcceptEncoding;
import ch.boye.httpclientandroidlib.client.protocol.ResponseContentEncoding;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.BasicHttpProcessor;

/**
 * {@link DefaultHttpClient} sub-class which includes a {@link RequestAcceptEncoding}
 * for the request and response.
 *
 * @since 4.1
 */
@ThreadSafe // since DefaultHttpClient is
public class ContentEncodingHttpClient extends DefaultHttpClient {

    /**
     * Creates a new HTTP client from parameters and a connection manager.
     *
     * @param params    the parameters
     * @param conman    the connection manager
     */
    public ContentEncodingHttpClient(ClientConnectionManager conman, HttpParams params) {
        super(conman, params);
    }

    /**
     * @param params
     */
    public ContentEncodingHttpClient(HttpParams params) {
        this(null, params);
    }

    /**
     *
     */
    public ContentEncodingHttpClient() {
        this(null);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected BasicHttpProcessor createHttpProcessor() {
        BasicHttpProcessor result = super.createHttpProcessor();

        result.addRequestInterceptor(new RequestAcceptEncoding());
        result.addResponseInterceptor(new ResponseContentEncoding());

        return result;
    }

}
