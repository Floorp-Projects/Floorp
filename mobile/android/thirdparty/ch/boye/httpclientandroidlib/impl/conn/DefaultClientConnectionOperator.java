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

package ch.boye.httpclientandroidlib.impl.conn;

import java.io.IOException;
import java.net.ConnectException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.InetAddress;
import java.net.UnknownHostException;

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.annotation.ThreadSafe;

import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.params.HttpConnectionParams;
import ch.boye.httpclientandroidlib.protocol.HttpContext;

import ch.boye.httpclientandroidlib.conn.ConnectTimeoutException;
import ch.boye.httpclientandroidlib.conn.HttpHostConnectException;
import ch.boye.httpclientandroidlib.conn.OperatedClientConnection;
import ch.boye.httpclientandroidlib.conn.ClientConnectionOperator;
import ch.boye.httpclientandroidlib.conn.scheme.LayeredSchemeSocketFactory;
import ch.boye.httpclientandroidlib.conn.scheme.Scheme;
import ch.boye.httpclientandroidlib.conn.scheme.SchemeRegistry;
import ch.boye.httpclientandroidlib.conn.scheme.SchemeSocketFactory;

/**
 * Default implementation of a {@link ClientConnectionOperator}. It uses a {@link SchemeRegistry}
 * to look up {@link SchemeSocketFactory} objects.
 * <p>
 * This connection operator is multihome network aware and will attempt to retry failed connects
 * against all known IP addresses sequentially until the connect is successful or all known
 * addresses fail to respond. Please note the same
 * {@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#CONNECTION_TIMEOUT} value will be used
 * for each connection attempt, so in the worst case the total elapsed time before timeout
 * can be <code>CONNECTION_TIMEOUT * n</code> where <code>n</code> is the number of IP addresses
 * of the given host. One can disable multihome support by overriding
 * the {@link #resolveHostname(String)} method and returning only one IP address for the given
 * host name.
 * <p>
 * The following parameters can be used to customize the behavior of this
 * class:
 * <ul>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreProtocolPNames#HTTP_ELEMENT_CHARSET}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SO_TIMEOUT}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SO_LINGER}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SO_REUSEADDR}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#TCP_NODELAY}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SOCKET_BUFFER_SIZE}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#CONNECTION_TIMEOUT}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#MAX_LINE_LENGTH}</li>
 * </ul>
 *
 * @since 4.0
 */
@ThreadSafe
public class DefaultClientConnectionOperator implements ClientConnectionOperator {

    public HttpClientAndroidLog log = new HttpClientAndroidLog(getClass());

    /** The scheme registry for looking up socket factories. */
    protected final SchemeRegistry schemeRegistry; // @ThreadSafe

    /**
     * Creates a new client connection operator for the given scheme registry.
     *
     * @param schemes   the scheme registry
     */
    public DefaultClientConnectionOperator(final SchemeRegistry schemes) {
        if (schemes == null) {
            throw new IllegalArgumentException("Scheme registry amy not be null");
        }
        this.schemeRegistry = schemes;
    }

    public OperatedClientConnection createConnection() {
        return new DefaultClientConnection();
    }

    public void openConnection(
            final OperatedClientConnection conn,
            final HttpHost target,
            final InetAddress local,
            final HttpContext context,
            final HttpParams params) throws IOException {
        if (conn == null) {
            throw new IllegalArgumentException("Connection may not be null");
        }
        if (target == null) {
            throw new IllegalArgumentException("Target host may not be null");
        }
        if (params == null) {
            throw new IllegalArgumentException("Parameters may not be null");
        }
        if (conn.isOpen()) {
            throw new IllegalStateException("Connection must not be open");
        }

        Scheme schm = schemeRegistry.getScheme(target.getSchemeName());
        SchemeSocketFactory sf = schm.getSchemeSocketFactory();

        InetAddress[] addresses = resolveHostname(target.getHostName());
        int port = schm.resolvePort(target.getPort());
        for (int i = 0; i < addresses.length; i++) {
            InetAddress address = addresses[i];
            boolean last = i == addresses.length - 1;

            Socket sock = sf.createSocket(params);
            conn.opening(sock, target);

            InetSocketAddress remoteAddress = new HttpInetSocketAddress(target, address, port);
            InetSocketAddress localAddress = null;
            if (local != null) {
                localAddress = new InetSocketAddress(local, 0);
            }
            if (this.log.isDebugEnabled()) {
                this.log.debug("Connecting to " + remoteAddress);
            }
            try {
                Socket connsock = sf.connectSocket(sock, remoteAddress, localAddress, params);
                if (sock != connsock) {
                    sock = connsock;
                    conn.opening(sock, target);
                }
                prepareSocket(sock, context, params);
                conn.openCompleted(sf.isSecure(sock), params);
                return;
            } catch (ConnectException ex) {
                if (last) {
                    throw new HttpHostConnectException(target, ex);
                }
            } catch (ConnectTimeoutException ex) {
                if (last) {
                    throw ex;
                }
            }
            if (this.log.isDebugEnabled()) {
                this.log.debug("Connect to " + remoteAddress + " timed out. " +
                        "Connection will be retried using another IP address");
            }
        }
    }

    public void updateSecureConnection(
            final OperatedClientConnection conn,
            final HttpHost target,
            final HttpContext context,
            final HttpParams params) throws IOException {
        if (conn == null) {
            throw new IllegalArgumentException("Connection may not be null");
        }
        if (target == null) {
            throw new IllegalArgumentException("Target host may not be null");
        }
        if (params == null) {
            throw new IllegalArgumentException("Parameters may not be null");
        }
        if (!conn.isOpen()) {
            throw new IllegalStateException("Connection must be open");
        }

        final Scheme schm = schemeRegistry.getScheme(target.getSchemeName());
        if (!(schm.getSchemeSocketFactory() instanceof LayeredSchemeSocketFactory)) {
            throw new IllegalArgumentException
                ("Target scheme (" + schm.getName() +
                 ") must have layered socket factory.");
        }

        LayeredSchemeSocketFactory lsf = (LayeredSchemeSocketFactory) schm.getSchemeSocketFactory();
        Socket sock;
        try {
            sock = lsf.createLayeredSocket(
                    conn.getSocket(), target.getHostName(), target.getPort(), true);
        } catch (ConnectException ex) {
            throw new HttpHostConnectException(target, ex);
        }
        prepareSocket(sock, context, params);
        conn.update(sock, target, lsf.isSecure(sock), params);
    }

    /**
     * Performs standard initializations on a newly created socket.
     *
     * @param sock      the socket to prepare
     * @param context   the context for the connection
     * @param params    the parameters from which to prepare the socket
     *
     * @throws IOException      in case of an IO problem
     */
    protected void prepareSocket(
            final Socket sock,
            final HttpContext context,
            final HttpParams params) throws IOException {
        sock.setTcpNoDelay(HttpConnectionParams.getTcpNoDelay(params));
        sock.setSoTimeout(HttpConnectionParams.getSoTimeout(params));

        int linger = HttpConnectionParams.getLinger(params);
        if (linger >= 0) {
            sock.setSoLinger(linger > 0, linger);
        }
    }

    /**
     * Resolves the given host name to an array of corresponding IP addresses, based on the
     * configured name service on the system.
     *
     * @param host host name to resolve
     * @return array of IP addresses
     * @exception  UnknownHostException  if no IP address for the host could be determined.
     *
     * @since 4.1
     */
    protected InetAddress[] resolveHostname(final String host) throws UnknownHostException {
        return InetAddress.getAllByName(host);
    }

}

