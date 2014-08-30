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
package ch.boye.httpclientandroidlib.impl.pool;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

import javax.net.SocketFactory;
import javax.net.ssl.SSLSocketFactory;

import ch.boye.httpclientandroidlib.HttpClientConnection;
import ch.boye.httpclientandroidlib.HttpConnectionFactory;
import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.config.ConnectionConfig;
import ch.boye.httpclientandroidlib.config.SocketConfig;
import ch.boye.httpclientandroidlib.impl.DefaultBHttpClientConnection;
import ch.boye.httpclientandroidlib.impl.DefaultBHttpClientConnectionFactory;
import ch.boye.httpclientandroidlib.params.CoreConnectionPNames;
import ch.boye.httpclientandroidlib.params.HttpParamConfig;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.pool.ConnFactory;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * A very basic {@link ConnFactory} implementation that creates
 * {@link HttpClientConnection} instances given a {@link HttpHost} instance.
 *
 * @see HttpHost
 * @since 4.2
 */
@SuppressWarnings("deprecation")
@Immutable
public class BasicConnFactory implements ConnFactory<HttpHost, HttpClientConnection> {

    private final SocketFactory plainfactory;
    private final SSLSocketFactory sslfactory;
    private final int connectTimeout;
    private final SocketConfig sconfig;
    private final HttpConnectionFactory<? extends HttpClientConnection> connFactory;

    /**
     * @deprecated (4.3) use
     *   {@link BasicConnFactory#BasicConnFactory(SocketFactory, SSLSocketFactory, int,
     *     SocketConfig, ConnectionConfig)}.
     */
    @Deprecated
    public BasicConnFactory(final SSLSocketFactory sslfactory, final HttpParams params) {
        super();
        Args.notNull(params, "HTTP params");
        this.plainfactory = null;
        this.sslfactory = sslfactory;
        this.connectTimeout = params.getIntParameter(CoreConnectionPNames.CONNECTION_TIMEOUT, 0);
        this.sconfig = HttpParamConfig.getSocketConfig(params);
        this.connFactory = new DefaultBHttpClientConnectionFactory(
                HttpParamConfig.getConnectionConfig(params));
    }

    /**
     * @deprecated (4.3) use
     *   {@link BasicConnFactory#BasicConnFactory(int, SocketConfig, ConnectionConfig)}.
     */
    @Deprecated
    public BasicConnFactory(final HttpParams params) {
        this(null, params);
    }

    /**
     * @since 4.3
     */
    public BasicConnFactory(
            final SocketFactory plainfactory,
            final SSLSocketFactory sslfactory,
            final int connectTimeout,
            final SocketConfig sconfig,
            final ConnectionConfig cconfig) {
        super();
        this.plainfactory = plainfactory;
        this.sslfactory = sslfactory;
        this.connectTimeout = connectTimeout;
        this.sconfig = sconfig != null ? sconfig : SocketConfig.DEFAULT;
        this.connFactory = new DefaultBHttpClientConnectionFactory(
                cconfig != null ? cconfig : ConnectionConfig.DEFAULT);
    }

    /**
     * @since 4.3
     */
    public BasicConnFactory(
            final int connectTimeout, final SocketConfig sconfig, final ConnectionConfig cconfig) {
        this(null, null, connectTimeout, sconfig, cconfig);
    }

    /**
     * @since 4.3
     */
    public BasicConnFactory(final SocketConfig sconfig, final ConnectionConfig cconfig) {
        this(null, null, 0, sconfig, cconfig);
    }

    /**
     * @since 4.3
     */
    public BasicConnFactory() {
        this(null, null, 0, SocketConfig.DEFAULT, ConnectionConfig.DEFAULT);
    }

    /**
     * @deprecated (4.3) no longer used.
     */
    @Deprecated
    protected HttpClientConnection create(final Socket socket, final HttpParams params) throws IOException {
        final int bufsize = params.getIntParameter(CoreConnectionPNames.SOCKET_BUFFER_SIZE, 8 * 1024);
        final DefaultBHttpClientConnection conn = new DefaultBHttpClientConnection(bufsize);
        conn.bind(socket);
        return conn;
    }

    public HttpClientConnection create(final HttpHost host) throws IOException {
        final String scheme = host.getSchemeName();
        Socket socket = null;
        if ("http".equalsIgnoreCase(scheme)) {
            socket = this.plainfactory != null ? this.plainfactory.createSocket() :
                    new Socket();
        } if ("https".equalsIgnoreCase(scheme)) {
            socket = (this.sslfactory != null ? this.sslfactory :
                    SSLSocketFactory.getDefault()).createSocket();
        }
        if (socket == null) {
            throw new IOException(scheme + " scheme is not supported");
        }
        final String hostname = host.getHostName();
        int port = host.getPort();
        if (port == -1) {
            if (host.getSchemeName().equalsIgnoreCase("http")) {
                port = 80;
            } else if (host.getSchemeName().equalsIgnoreCase("https")) {
                port = 443;
            }
        }
        socket.setSoTimeout(this.sconfig.getSoTimeout());
        socket.connect(new InetSocketAddress(hostname, port), this.connectTimeout);
        socket.setTcpNoDelay(this.sconfig.isTcpNoDelay());
        final int linger = this.sconfig.getSoLinger();
        if (linger >= 0) {
            socket.setSoLinger(linger > 0, linger);
        }
        socket.setKeepAlive(this.sconfig.isSoKeepAlive());
        return this.connFactory.createConnection(socket);
    }

}
