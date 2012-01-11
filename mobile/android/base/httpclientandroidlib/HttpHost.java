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

package ch.boye.httpclientandroidlib;

import java.io.Serializable;
import java.util.Locale;

import ch.boye.httpclientandroidlib.util.CharArrayBuffer;
import ch.boye.httpclientandroidlib.util.LangUtils;

/**
 * Holds all of the variables needed to describe an HTTP connection to a host.
 * This includes remote host name, port and scheme.
 *
 *
 * @since 4.0
 */
//@Immutable
public final class HttpHost implements Cloneable, Serializable {

    private static final long serialVersionUID = -7529410654042457626L;

    /** The default scheme is "http". */
    public static final String DEFAULT_SCHEME_NAME = "http";

    /** The host to use. */
    protected final String hostname;

    /** The lowercase host, for {@link #equals} and {@link #hashCode}. */
    protected final String lcHostname;


    /** The port to use. */
    protected final int port;

    /** The scheme (lowercased) */
    protected final String schemeName;


    /**
     * Creates a new {@link HttpHost HttpHost}, specifying all values.
     * Constructor for HttpHost.
     *
     * @param hostname  the hostname (IP or DNS name)
     * @param port      the port number.
     *                  <code>-1</code> indicates the scheme default port.
     * @param scheme    the name of the scheme.
     *                  <code>null</code> indicates the
     *                  {@link #DEFAULT_SCHEME_NAME default scheme}
     */
    public HttpHost(final String hostname, int port, final String scheme) {
        super();
        if (hostname == null) {
            throw new IllegalArgumentException("Host name may not be null");
        }
        this.hostname   = hostname;
        this.lcHostname = hostname.toLowerCase(Locale.ENGLISH);
        if (scheme != null) {
            this.schemeName = scheme.toLowerCase(Locale.ENGLISH);
        } else {
            this.schemeName = DEFAULT_SCHEME_NAME;
        }
        this.port = port;
    }

    /**
     * Creates a new {@link HttpHost HttpHost}, with default scheme.
     *
     * @param hostname  the hostname (IP or DNS name)
     * @param port      the port number.
     *                  <code>-1</code> indicates the scheme default port.
     */
    public HttpHost(final String hostname, int port) {
        this(hostname, port, null);
    }

    /**
     * Creates a new {@link HttpHost HttpHost}, with default scheme and port.
     *
     * @param hostname  the hostname (IP or DNS name)
     */
    public HttpHost(final String hostname) {
        this(hostname, -1, null);
    }

    /**
     * Copy constructor for {@link HttpHost HttpHost}.
     *
     * @param httphost the HTTP host to copy details from
     */
    public HttpHost (final HttpHost httphost) {
        this(httphost.hostname, httphost.port, httphost.schemeName);
    }

    /**
     * Returns the host name.
     *
     * @return the host name (IP or DNS name)
     */
    public String getHostName() {
        return this.hostname;
    }

    /**
     * Returns the port.
     *
     * @return the host port, or <code>-1</code> if not set
     */
    public int getPort() {
        return this.port;
    }

    /**
     * Returns the scheme name.
     *
     * @return the scheme name
     */
    public String getSchemeName() {
        return this.schemeName;
    }

    /**
     * Return the host URI, as a string.
     *
     * @return the host URI
     */
    public String toURI() {
        CharArrayBuffer buffer = new CharArrayBuffer(32);
        buffer.append(this.schemeName);
        buffer.append("://");
        buffer.append(this.hostname);
        if (this.port != -1) {
            buffer.append(':');
            buffer.append(Integer.toString(this.port));
        }
        return buffer.toString();
    }


    /**
     * Obtains the host string, without scheme prefix.
     *
     * @return  the host string, for example <code>localhost:8080</code>
     */
    public String toHostString() {
        if (this.port != -1) {
            //the highest port number is 65535, which is length 6 with the addition of the colon
            CharArrayBuffer buffer = new CharArrayBuffer(this.hostname.length() + 6);
            buffer.append(this.hostname);
            buffer.append(":");
            buffer.append(Integer.toString(this.port));
            return buffer.toString();
        } else {
            return this.hostname;
        }
    }


    public String toString() {
        return toURI();
    }


    public boolean equals(final Object obj) {
        if (this == obj) return true;
        if (obj instanceof HttpHost) {
            HttpHost that = (HttpHost) obj;
            return this.lcHostname.equals(that.lcHostname)
                && this.port == that.port
                && this.schemeName.equals(that.schemeName);
        } else {
            return false;
        }
    }

    /**
     * @see java.lang.Object#hashCode()
     */
    public int hashCode() {
        int hash = LangUtils.HASH_SEED;
        hash = LangUtils.hashCode(hash, this.lcHostname);
        hash = LangUtils.hashCode(hash, this.port);
        hash = LangUtils.hashCode(hash, this.schemeName);
        return hash;
    }

    public Object clone() throws CloneNotSupportedException {
        return super.clone();
    }

}
