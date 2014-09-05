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
package ch.boye.httpclientandroidlib.auth;

import java.util.Locale;

import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.util.Args;
import ch.boye.httpclientandroidlib.util.LangUtils;

/**
 * The class represents an authentication scope consisting of a host name,
 * a port number, a realm name and an authentication scheme name which
 * {@link Credentials Credentials} apply to.
 *
 *
 * @since 4.0
 */
@Immutable
public class AuthScope {

    /**
     * The <tt>null</tt> value represents any host. In the future versions of
     * HttpClient the use of this parameter will be discontinued.
     */
    public static final String ANY_HOST = null;

    /**
     * The <tt>-1</tt> value represents any port.
     */
    public static final int ANY_PORT = -1;

    /**
     * The <tt>null</tt> value represents any realm.
     */
    public static final String ANY_REALM = null;

    /**
     * The <tt>null</tt> value represents any authentication scheme.
     */
    public static final String ANY_SCHEME = null;

    /**
     * Default scope matching any host, port, realm and authentication scheme.
     * In the future versions of HttpClient the use of this parameter will be
     * discontinued.
     */
    public static final AuthScope ANY = new AuthScope(ANY_HOST, ANY_PORT, ANY_REALM, ANY_SCHEME);

    /** The authentication scheme the credentials apply to. */
    private final String scheme;

    /** The realm the credentials apply to. */
    private final String realm;

    /** The host the credentials apply to. */
    private final String host;

    /** The port the credentials apply to. */
    private final int port;

    /** Creates a new credentials scope for the given
     * <tt>host</tt>, <tt>port</tt>, <tt>realm</tt>, and
     * <tt>authentication scheme</tt>.
     *
     * @param host the host the credentials apply to. May be set
     *   to <tt>null</tt> if credentials are applicable to
     *   any host.
     * @param port the port the credentials apply to. May be set
     *   to negative value if credentials are applicable to
     *   any port.
     * @param realm the realm the credentials apply to. May be set
     *   to <tt>null</tt> if credentials are applicable to
     *   any realm.
     * @param scheme the authentication scheme the credentials apply to.
     *   May be set to <tt>null</tt> if credentials are applicable to
     *   any authentication scheme.
     */
    public AuthScope(final String host, final int port,
        final String realm, final String scheme)
    {
        this.host =   (host == null)   ? ANY_HOST: host.toLowerCase(Locale.ENGLISH);
        this.port =   (port < 0)       ? ANY_PORT: port;
        this.realm =  (realm == null)  ? ANY_REALM: realm;
        this.scheme = (scheme == null) ? ANY_SCHEME: scheme.toUpperCase(Locale.ENGLISH);
    }

    /**
     * @since 4.2
     */
    public AuthScope(final HttpHost host, final String realm, final String schemeName) {
        this(host.getHostName(), host.getPort(), realm, schemeName);
    }

    /**
     * @since 4.2
     */
    public AuthScope(final HttpHost host) {
        this(host, ANY_REALM, ANY_SCHEME);
    }

    /** Creates a new credentials scope for the given
     * <tt>host</tt>, <tt>port</tt>, <tt>realm</tt>, and any
     * authentication scheme.
     *
     * @param host the host the credentials apply to. May be set
     *   to <tt>null</tt> if credentials are applicable to
     *   any host.
     * @param port the port the credentials apply to. May be set
     *   to negative value if credentials are applicable to
     *   any port.
     * @param realm the realm the credentials apply to. May be set
     *   to <tt>null</tt> if credentials are applicable to
     *   any realm.
     */
    public AuthScope(final String host, final int port, final String realm) {
        this(host, port, realm, ANY_SCHEME);
    }

    /** Creates a new credentials scope for the given
     * <tt>host</tt>, <tt>port</tt>, any realm name, and any
     * authentication scheme.
     *
     * @param host the host the credentials apply to. May be set
     *   to <tt>null</tt> if credentials are applicable to
     *   any host.
     * @param port the port the credentials apply to. May be set
     *   to negative value if credentials are applicable to
     *   any port.
     */
    public AuthScope(final String host, final int port) {
        this(host, port, ANY_REALM, ANY_SCHEME);
    }

    /**
     * Creates a copy of the given credentials scope.
     */
    public AuthScope(final AuthScope authscope) {
        super();
        Args.notNull(authscope, "Scope");
        this.host = authscope.getHost();
        this.port = authscope.getPort();
        this.realm = authscope.getRealm();
        this.scheme = authscope.getScheme();
    }

    /**
     * @return the host
     */
    public String getHost() {
        return this.host;
    }

    /**
     * @return the port
     */
    public int getPort() {
        return this.port;
    }

    /**
     * @return the realm name
     */
    public String getRealm() {
        return this.realm;
    }

    /**
     * @return the scheme type
     */
    public String getScheme() {
        return this.scheme;
    }

    /**
     * Tests if the authentication scopes match.
     *
     * @return the match factor. Negative value signifies no match.
     *    Non-negative signifies a match. The greater the returned value
     *    the closer the match.
     */
    public int match(final AuthScope that) {
        int factor = 0;
        if (LangUtils.equals(this.scheme, that.scheme)) {
            factor += 1;
        } else {
            if (this.scheme != ANY_SCHEME && that.scheme != ANY_SCHEME) {
                return -1;
            }
        }
        if (LangUtils.equals(this.realm, that.realm)) {
            factor += 2;
        } else {
            if (this.realm != ANY_REALM && that.realm != ANY_REALM) {
                return -1;
            }
        }
        if (this.port == that.port) {
            factor += 4;
        } else {
            if (this.port != ANY_PORT && that.port != ANY_PORT) {
                return -1;
            }
        }
        if (LangUtils.equals(this.host, that.host)) {
            factor += 8;
        } else {
            if (this.host != ANY_HOST && that.host != ANY_HOST) {
                return -1;
            }
        }
        return factor;
    }

    /**
     * @see java.lang.Object#equals(Object)
     */
    @Override
    public boolean equals(final Object o) {
        if (o == null) {
            return false;
        }
        if (o == this) {
            return true;
        }
        if (!(o instanceof AuthScope)) {
            return super.equals(o);
        }
        final AuthScope that = (AuthScope) o;
        return
        LangUtils.equals(this.host, that.host)
          && this.port == that.port
          && LangUtils.equals(this.realm, that.realm)
          && LangUtils.equals(this.scheme, that.scheme);
    }

    /**
     * @see java.lang.Object#toString()
     */
    @Override
    public String toString() {
        final StringBuilder buffer = new StringBuilder();
        if (this.scheme != null) {
            buffer.append(this.scheme.toUpperCase(Locale.ENGLISH));
            buffer.append(' ');
        }
        if (this.realm != null) {
            buffer.append('\'');
            buffer.append(this.realm);
            buffer.append('\'');
        } else {
            buffer.append("<any realm>");
        }
        if (this.host != null) {
            buffer.append('@');
            buffer.append(this.host);
            if (this.port >= 0) {
                buffer.append(':');
                buffer.append(this.port);
            }
        }
        return buffer.toString();
    }

    /**
     * @see java.lang.Object#hashCode()
     */
    @Override
    public int hashCode() {
        int hash = LangUtils.HASH_SEED;
        hash = LangUtils.hashCode(hash, this.host);
        hash = LangUtils.hashCode(hash, this.port);
        hash = LangUtils.hashCode(hash, this.realm);
        hash = LangUtils.hashCode(hash, this.scheme);
        return hash;
    }
}
