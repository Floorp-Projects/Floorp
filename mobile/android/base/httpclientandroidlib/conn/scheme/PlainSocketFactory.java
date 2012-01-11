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

package ch.boye.httpclientandroidlib.conn.scheme;

import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;

import ch.boye.httpclientandroidlib.annotation.Immutable;

import ch.boye.httpclientandroidlib.conn.ConnectTimeoutException;
import ch.boye.httpclientandroidlib.params.HttpConnectionParams;
import ch.boye.httpclientandroidlib.params.HttpParams;

/**
 * The default class for creating plain (unencrypted) sockets.
 * <p>
 * The following parameters can be used to customize the behavior of this
 * class:
 * <ul>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#CONNECTION_TIMEOUT}</li>
 *  <li>{@link ch.boye.httpclientandroidlib.params.CoreConnectionPNames#SO_REUSEADDR}</li>
 * </ul>
 *
 * @since 4.0
 */
@SuppressWarnings("deprecation")
@Immutable
public class PlainSocketFactory implements SocketFactory, SchemeSocketFactory {

    private final HostNameResolver nameResolver;

    /**
     * Gets the default factory.
     *
     * @return the default factory
     */
    public static PlainSocketFactory getSocketFactory() {
        return new PlainSocketFactory();
    }

    @Deprecated
    public PlainSocketFactory(final HostNameResolver nameResolver) {
        super();
        this.nameResolver = nameResolver;
    }

    public PlainSocketFactory() {
        super();
        this.nameResolver = null;
    }

    /**
     * @param params Optional parameters. Parameters passed to this method will have no effect.
     *               This method will create a unconnected instance of {@link Socket} class
     *               using default constructor.
     *
     * @since 4.1
     */
    public Socket createSocket(final HttpParams params) {
        return new Socket();
    }

    public Socket createSocket() {
        return new Socket();
    }

    /**
     * @since 4.1
     */
    public Socket connectSocket(
            final Socket socket,
            final InetSocketAddress remoteAddress,
            final InetSocketAddress localAddress,
            final HttpParams params) throws IOException, ConnectTimeoutException {
        if (remoteAddress == null) {
            throw new IllegalArgumentException("Remote address may not be null");
        }
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        Socket sock = socket;
        if (sock == null) {
            sock = createSocket();
        }
        if (localAddress != null) {
            sock.setReuseAddress(HttpConnectionParams.getSoReuseaddr(params));
            sock.bind(localAddress);
        }
        int connTimeout = HttpConnectionParams.getConnectionTimeout(params);
        int soTimeout = HttpConnectionParams.getSoTimeout(params);

        try {
            sock.setSoTimeout(soTimeout);
            sock.connect(remoteAddress, connTimeout);
        } catch (SocketTimeoutException ex) {
            throw new ConnectTimeoutException("Connect to " + remoteAddress + " timed out");
        }
        return sock;
    }

    /**
     * Checks whether a socket connection is secure.
     * This factory creates plain socket connections
     * which are not considered secure.
     *
     * @param sock      the connected socket
     *
     * @return  <code>false</code>
     *
     * @throws IllegalArgumentException if the argument is invalid
     */
    public final boolean isSecure(Socket sock)
        throws IllegalArgumentException {

        if (sock == null) {
            throw new IllegalArgumentException("Socket may not be null.");
        }
        // This check is performed last since it calls a method implemented
        // by the argument object. getClass() is final in java.lang.Object.
        if (sock.isClosed()) {
            throw new IllegalArgumentException("Socket is closed.");
        }
        return false;
    }

    /**
     * @deprecated Use {@link #connectSocket(Socket, InetSocketAddress, InetSocketAddress, HttpParams)}
     */
    @Deprecated
    public Socket connectSocket(
            final Socket socket,
            final String host, int port,
            final InetAddress localAddress, int localPort,
            final HttpParams params) throws IOException, UnknownHostException, ConnectTimeoutException {
        InetSocketAddress local = null;
        if (localAddress != null || localPort > 0) {
            // we need to bind explicitly
            if (localPort < 0) {
                localPort = 0; // indicates "any"
            }
            local = new InetSocketAddress(localAddress, localPort);
        }
        InetAddress remoteAddress;
        if (this.nameResolver != null) {
            remoteAddress = this.nameResolver.resolve(host);
        } else {
            remoteAddress = InetAddress.getByName(host);
        }
        InetSocketAddress remote = new InetSocketAddress(remoteAddress, port);
        return connectSocket(socket, remote, local, params);
    }

}
