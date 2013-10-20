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

package ch.boye.httpclientandroidlib.conn.routing;

import java.net.InetAddress;

import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.util.LangUtils;

import ch.boye.httpclientandroidlib.HttpHost;

/**
 * The route for a request.
 * Instances of this class are unmodifiable and therefore suitable
 * for use as lookup keys.
 *
 * @since 4.0
 */
@Immutable
public final class HttpRoute implements RouteInfo, Cloneable {

    private static final HttpHost[] EMPTY_HTTP_HOST_ARRAY = new HttpHost[]{};

    /** The target host to connect to. */
    private final HttpHost targetHost;

    /**
     * The local address to connect from.
     * <code>null</code> indicates that the default should be used.
     */
    private final InetAddress localAddress;

    /** The proxy servers, if any. Never null. */
    private final HttpHost[] proxyChain;

    /** Whether the the route is tunnelled through the proxy. */
    private final TunnelType tunnelled;

    /** Whether the route is layered. */
    private final LayerType layered;

    /** Whether the route is (supposed to be) secure. */
    private final boolean secure;


    /**
     * Internal, fully-specified constructor.
     * This constructor does <i>not</i> clone the proxy chain array,
     * nor test it for <code>null</code> elements. This conversion and
     * check is the responsibility of the public constructors.
     * The order of arguments here is different from the similar public
     * constructor, as required by Java.
     *
     * @param local     the local address to route from, or
     *                  <code>null</code> for the default
     * @param target    the host to which to route
     * @param proxies   the proxy chain to use, or
     *                  <code>null</code> for a direct route
     * @param secure    <code>true</code> if the route is (to be) secure,
     *                  <code>false</code> otherwise
     * @param tunnelled the tunnel type of this route, or
     *                  <code>null</code> for PLAIN
     * @param layered   the layering type of this route, or
     *                  <code>null</code> for PLAIN
     */
    private HttpRoute(InetAddress local,
                      HttpHost target, HttpHost[] proxies,
                      boolean secure,
                      TunnelType tunnelled, LayerType layered) {
        if (target == null) {
            throw new IllegalArgumentException
                ("Target host may not be null.");
        }
        if (proxies == null) {
            throw new IllegalArgumentException
                ("Proxies may not be null.");
        }
        if ((tunnelled == TunnelType.TUNNELLED) && (proxies.length == 0)) {
            throw new IllegalArgumentException
                ("Proxy required if tunnelled.");
        }

        // tunnelled is already checked above, that is in line with the default
        if (tunnelled == null)
            tunnelled = TunnelType.PLAIN;
        if (layered == null)
            layered = LayerType.PLAIN;

        this.targetHost   = target;
        this.localAddress = local;
        this.proxyChain   = proxies;
        this.secure       = secure;
        this.tunnelled    = tunnelled;
        this.layered      = layered;
    }


    /**
     * Creates a new route with all attributes specified explicitly.
     *
     * @param target    the host to which to route
     * @param local     the local address to route from, or
     *                  <code>null</code> for the default
     * @param proxies   the proxy chain to use, or
     *                  <code>null</code> for a direct route
     * @param secure    <code>true</code> if the route is (to be) secure,
     *                  <code>false</code> otherwise
     * @param tunnelled the tunnel type of this route
     * @param layered   the layering type of this route
     */
    public HttpRoute(HttpHost target, InetAddress local, HttpHost[] proxies,
                     boolean secure, TunnelType tunnelled, LayerType layered) {
        this(local, target, toChain(proxies), secure, tunnelled, layered);
    }


    /**
     * Creates a new route with at most one proxy.
     *
     * @param target    the host to which to route
     * @param local     the local address to route from, or
     *                  <code>null</code> for the default
     * @param proxy     the proxy to use, or
     *                  <code>null</code> for a direct route
     * @param secure    <code>true</code> if the route is (to be) secure,
     *                  <code>false</code> otherwise
     * @param tunnelled <code>true</code> if the route is (to be) tunnelled
     *                  via the proxy,
     *                  <code>false</code> otherwise
     * @param layered   <code>true</code> if the route includes a
     *                  layered protocol,
     *                  <code>false</code> otherwise
     */
    public HttpRoute(HttpHost target, InetAddress local, HttpHost proxy,
                     boolean secure, TunnelType tunnelled, LayerType layered) {
        this(local, target, toChain(proxy), secure, tunnelled, layered);
    }


    /**
     * Creates a new direct route.
     * That is a route without a proxy.
     *
     * @param target    the host to which to route
     * @param local     the local address to route from, or
     *                  <code>null</code> for the default
     * @param secure    <code>true</code> if the route is (to be) secure,
     *                  <code>false</code> otherwise
     */
    public HttpRoute(HttpHost target, InetAddress local, boolean secure) {
        this(local, target, EMPTY_HTTP_HOST_ARRAY, secure, TunnelType.PLAIN, LayerType.PLAIN);
    }


    /**
     * Creates a new direct insecure route.
     *
     * @param target    the host to which to route
     */
    public HttpRoute(HttpHost target) {
        this(null, target, EMPTY_HTTP_HOST_ARRAY, false, TunnelType.PLAIN, LayerType.PLAIN);
    }


    /**
     * Creates a new route through a proxy.
     * When using this constructor, the <code>proxy</code> MUST be given.
     * For convenience, it is assumed that a secure connection will be
     * layered over a tunnel through the proxy.
     *
     * @param target    the host to which to route
     * @param local     the local address to route from, or
     *                  <code>null</code> for the default
     * @param proxy     the proxy to use
     * @param secure    <code>true</code> if the route is (to be) secure,
     *                  <code>false</code> otherwise
     */
    public HttpRoute(HttpHost target, InetAddress local, HttpHost proxy,
                     boolean secure) {
        this(local, target, toChain(proxy), secure,
             secure ? TunnelType.TUNNELLED : TunnelType.PLAIN,
             secure ? LayerType.LAYERED    : LayerType.PLAIN);
        if (proxy == null) {
            throw new IllegalArgumentException
                ("Proxy host may not be null.");
        }
    }


    /**
     * Helper to convert a proxy to a proxy chain.
     *
     * @param proxy     the only proxy in the chain, or <code>null</code>
     *
     * @return  a proxy chain array, may be empty (never null)
     */
    private static HttpHost[] toChain(HttpHost proxy) {
        if (proxy == null)
            return EMPTY_HTTP_HOST_ARRAY;

        return new HttpHost[]{ proxy };
    }


    /**
     * Helper to duplicate and check a proxy chain.
     * <code>null</code> is converted to an empty proxy chain.
     *
     * @param proxies   the proxy chain to duplicate, or <code>null</code>
     *
     * @return  a new proxy chain array, may be empty (never null)
     */
    private static HttpHost[] toChain(HttpHost[] proxies) {
        if ((proxies == null) || (proxies.length < 1))
            return EMPTY_HTTP_HOST_ARRAY;

        for (HttpHost proxy : proxies) {
            if (proxy == null)
                throw new IllegalArgumentException
                        ("Proxy chain may not contain null elements.");
        }

        // copy the proxy chain, the traditional way
        HttpHost[] result = new HttpHost[proxies.length];
        System.arraycopy(proxies, 0, result, 0, proxies.length);

        return result;
    }



    // non-JavaDoc, see interface RouteInfo
    public final HttpHost getTargetHost() {
        return this.targetHost;
    }


    // non-JavaDoc, see interface RouteInfo
    public final InetAddress getLocalAddress() {
        return this.localAddress;
    }


    public final int getHopCount() {
        return proxyChain.length+1;
    }


    public final HttpHost getHopTarget(int hop) {
        if (hop < 0)
            throw new IllegalArgumentException
                ("Hop index must not be negative: " + hop);
        final int hopcount = getHopCount();
        if (hop >= hopcount)
            throw new IllegalArgumentException
                ("Hop index " + hop +
                 " exceeds route length " + hopcount);

        HttpHost result = null;
        if (hop < hopcount-1)
            result = this.proxyChain[hop];
        else
            result = this.targetHost;

        return result;
    }


    public final HttpHost getProxyHost() {
        return (this.proxyChain.length == 0) ? null : this.proxyChain[0];
    }


    public final TunnelType getTunnelType() {
        return this.tunnelled;
    }


    public final boolean isTunnelled() {
        return (this.tunnelled == TunnelType.TUNNELLED);
    }


    public final LayerType getLayerType() {
        return this.layered;
    }


    public final boolean isLayered() {
        return (this.layered == LayerType.LAYERED);
    }


    public final boolean isSecure() {
        return this.secure;
    }


    /**
     * Compares this route to another.
     *
     * @param obj         the object to compare with
     *
     * @return  <code>true</code> if the argument is the same route,
     *          <code>false</code>
     */
    @Override
    public final boolean equals(Object obj) {
        if (this == obj) return true;
        if (obj instanceof HttpRoute) {
            HttpRoute that = (HttpRoute) obj;
            return 
                // Do the cheapest tests first
                (this.secure    == that.secure) &&
                (this.tunnelled == that.tunnelled) &&
                (this.layered   == that.layered) &&
                LangUtils.equals(this.targetHost, that.targetHost) &&
                LangUtils.equals(this.localAddress, that.localAddress) &&
                LangUtils.equals(this.proxyChain, that.proxyChain);
        } else {
            return false;
        }
    }


    /**
     * Generates a hash code for this route.
     *
     * @return  the hash code
     */
    @Override
    public final int hashCode() {
        int hash = LangUtils.HASH_SEED;
        hash = LangUtils.hashCode(hash, this.targetHost);
        hash = LangUtils.hashCode(hash, this.localAddress);
        for (int i = 0; i < this.proxyChain.length; i++) {
            hash = LangUtils.hashCode(hash, this.proxyChain[i]);
        }
        hash = LangUtils.hashCode(hash, this.secure);
        hash = LangUtils.hashCode(hash, this.tunnelled);
        hash = LangUtils.hashCode(hash, this.layered);
        return hash;
    }


    /**
     * Obtains a description of this route.
     *
     * @return  a human-readable representation of this route
     */
    @Override
    public final String toString() {
        StringBuilder cab = new StringBuilder(50 + getHopCount()*30);

        cab.append("HttpRoute[");
        if (this.localAddress != null) {
            cab.append(this.localAddress);
            cab.append("->");
        }
        cab.append('{');
        if (this.tunnelled == TunnelType.TUNNELLED)
            cab.append('t');
        if (this.layered == LayerType.LAYERED)
            cab.append('l');
        if (this.secure)
            cab.append('s');
        cab.append("}->");
        for (HttpHost aProxyChain : this.proxyChain) {
            cab.append(aProxyChain);
            cab.append("->");
        }
        cab.append(this.targetHost);
        cab.append(']');

        return cab.toString();
    }


    // default implementation of clone() is sufficient
    @Override
    public Object clone() throws CloneNotSupportedException {
        return super.clone();
    }

}
