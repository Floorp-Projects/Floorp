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

import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.annotation.ThreadSafe;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.protocol.RequestAddCookies;
import ch.boye.httpclientandroidlib.client.protocol.RequestAuthCache;
import ch.boye.httpclientandroidlib.client.protocol.RequestClientConnControl;
import ch.boye.httpclientandroidlib.client.protocol.RequestDefaultHeaders;
import ch.boye.httpclientandroidlib.client.protocol.RequestProxyAuthentication;
import ch.boye.httpclientandroidlib.client.protocol.RequestTargetAuthentication;
import ch.boye.httpclientandroidlib.client.protocol.ResponseAuthCache;
import ch.boye.httpclientandroidlib.client.protocol.ResponseProcessCookies;
import ch.boye.httpclientandroidlib.conn.ClientConnectionManager;
import ch.boye.httpclientandroidlib.params.CoreConnectionPNames;
import ch.boye.httpclientandroidlib.params.CoreProtocolPNames;
import ch.boye.httpclientandroidlib.params.HttpConnectionParams;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.params.HttpProtocolParams;
import ch.boye.httpclientandroidlib.params.SyncBasicHttpParams;
import ch.boye.httpclientandroidlib.protocol.BasicHttpProcessor;
import ch.boye.httpclientandroidlib.protocol.HTTP;
import ch.boye.httpclientandroidlib.protocol.RequestContent;
import ch.boye.httpclientandroidlib.protocol.RequestExpectContinue;
import ch.boye.httpclientandroidlib.protocol.RequestTargetHost;
import ch.boye.httpclientandroidlib.protocol.RequestUserAgent;
import ch.boye.httpclientandroidlib.util.VersionInfo;

/**
 * Default implementation of {@link HttpClient} pre-configured for most common use scenarios.
 * <p>
 * This class creates the following chain of protocol interceptors per default:
 * <ul>
 * <li>{@link RequestDefaultHeaders}</li>
 * <li>{@link RequestContent}</li>
 * <li>{@link RequestTargetHost}</li>
 * <li>{@link RequestClientConnControl}</li>
 * <li>{@link RequestUserAgent}</li>
 * <li>{@link RequestExpectContinue}</li>
 * <li>{@link RequestAddCookies}</li>
 * <li>{@link ResponseProcessCookies}</li>
 * <li>{@link RequestTargetAuthentication}</li>
 * <li>{@link RequestProxyAuthentication}</li>
 * </ul>
 * <p>
 * This class sets up the following parameters if not explicitly set:
 * <ul>
 * <li>Version: HttpVersion.HTTP_1_1</li>
 * <li>ContentCharset: HTTP.DEFAULT_CONTENT_CHARSET</li>
 * <li>NoTcpDelay: true</li>
 * <li>SocketBufferSize: 8192</li>
 * <li>UserAgent: Apache-HttpClient/release (java 1.5)</li>
 * </ul>
 * <p>
 * The following parameters can be used to customize the behavior of this
 * class:
 * <ul>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#PROTOCOL_VERSION}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#STRICT_TRANSFER_ENCODING}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#HTTP_ELEMENT_CHARSET}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#USE_EXPECT_CONTINUE}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#WAIT_FOR_CONTINUE}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#USER_AGENT}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#TCP_NODELAY}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SO_TIMEOUT}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SO_LINGER}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SO_REUSEADDR}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SOCKET_BUFFER_SIZE}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#CONNECTION_TIMEOUT}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#MAX_LINE_LENGTH}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#MAX_HEADER_COUNT}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#STALE_CONNECTION_CHECK}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.conn.params.ConnRoutePNames#FORCED_ROUTE}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.conn.params.ConnRoutePNames#LOCAL_ADDRESS}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.conn.params.ConnRoutePNames#DEFAULT_PROXY}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.cookie.params.CookieSpecPNames#DATE_PATTERNS}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.cookie.params.CookieSpecPNames#SINGLE_COOKIE_HEADER}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.auth.params.AuthPNames#CREDENTIAL_CHARSET}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#COOKIE_POLICY}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#HANDLE_AUTHENTICATION}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#HANDLE_REDIRECTS}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#MAX_REDIRECTS}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#ALLOW_CIRCULAR_REDIRECTS}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#VIRTUAL_HOST}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#DEFAULT_HOST}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#DEFAULT_HEADERS}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.client.params.ClientPNames#CONNECTION_MANAGER_FACTORY_CLASS_NAME}</li>
 * </ul>
 *
 * @since 4.0
 */
@ThreadSafe
public class DefaultHttpClient extends AbstractHttpClient {

    /**
     * Creates a new HTTP client from parameters and a connection manager.
     *
     * @param params    the parameters
     * @param conman    the connection manager
     */
    public DefaultHttpClient(
            final ClientConnectionManager conman,
            final HttpParams params) {
        super(conman, params);
    }


    /**
     * @since 4.1
     */
    public DefaultHttpClient(
            final ClientConnectionManager conman) {
        super(conman, null);
    }


    public DefaultHttpClient(final HttpParams params) {
        super(null, params);
    }


    public DefaultHttpClient() {
        super(null, null);
    }


    /**
     * Creates the default set of HttpParams by invoking {@link DefaultHttpClient#setDefaultHttpParams(HttpParams)}
     *
     * @return a new instance of {@link SyncBasicHttpParams} with the defaults applied to it.
     */
    @Override
    protected HttpParams createHttpParams() {
        HttpParams params = new SyncBasicHttpParams();
        setDefaultHttpParams(params);
        return params;
    }

    /**
     * Saves the default set of HttpParams in the provided parameter.
     * These are:
     * <ul>
     * <li>{@link CoreProtocolPNames#PROTOCOL_VERSION}: 1.1</li>
     * <li>{@link CoreProtocolPNames#HTTP_CONTENT_CHARSET}: ISO-8859-1</li>
     * <li>{@link CoreConnectionPNames#TCP_NODELAY}: true</li>
     * <li>{@link CoreConnectionPNames#SOCKET_BUFFER_SIZE}: 8192</li>
     * <li>{@link CoreProtocolPNames#USER_AGENT}: Apache-HttpClient/<release> (java 1.5)</li>
     * </ul>
     */
    public static void setDefaultHttpParams(HttpParams params) {
        HttpProtocolParams.setVersion(params, HttpVersion.HTTP_1_1);
        HttpProtocolParams.setContentCharset(params, HTTP.DEFAULT_CONTENT_CHARSET);
        HttpConnectionParams.setTcpNoDelay(params, true);
        HttpConnectionParams.setSocketBufferSize(params, 8192);

        // determine the release version from packaged version info
        final VersionInfo vi = VersionInfo.loadVersionInfo
            ("ch.boye.httpclientandroidlib.client", DefaultHttpClient.class.getClassLoader());
        final String release = (vi != null) ?
            vi.getRelease() : VersionInfo.UNAVAILABLE;
        HttpProtocolParams.setUserAgent(params,
                "Apache-HttpClient/" + release + " (java 1.5)");
    }


    @Override
    protected BasicHttpProcessor createHttpProcessor() {
        BasicHttpProcessor httpproc = new BasicHttpProcessor();
        httpproc.addInterceptor(new RequestDefaultHeaders());
        // Required protocol interceptors
        httpproc.addInterceptor(new RequestContent());
        httpproc.addInterceptor(new RequestTargetHost());
        // Recommended protocol interceptors
        httpproc.addInterceptor(new RequestClientConnControl());
        httpproc.addInterceptor(new RequestUserAgent());
        httpproc.addInterceptor(new RequestExpectContinue());
        // HTTP state management interceptors
        httpproc.addInterceptor(new RequestAddCookies());
        httpproc.addInterceptor(new ResponseProcessCookies());
        // HTTP authentication interceptors
        httpproc.addInterceptor(new RequestAuthCache());
        httpproc.addInterceptor(new ResponseAuthCache());
        httpproc.addInterceptor(new RequestTargetAuthentication());
        httpproc.addInterceptor(new RequestProxyAuthentication());
        return httpproc;
    }

}
