/*
 * ====================================================================
 *
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.impl.cookie;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HeaderElement;
import ch.boye.httpclientandroidlib.NameValuePair;
import ch.boye.httpclientandroidlib.cookie.ClientCookie;
import ch.boye.httpclientandroidlib.cookie.Cookie;
import ch.boye.httpclientandroidlib.cookie.CookieAttributeHandler;
import ch.boye.httpclientandroidlib.cookie.CookieOrigin;
import ch.boye.httpclientandroidlib.cookie.CookieSpec;
import ch.boye.httpclientandroidlib.cookie.MalformedCookieException;
import ch.boye.httpclientandroidlib.cookie.SM;
import ch.boye.httpclientandroidlib.message.BufferedHeader;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;

/**
 * RFC 2965 compliant {@link CookieSpec} implementation.
 *
 * @since 4.0
 */
@NotThreadSafe // superclass is @NotThreadSafe
public class RFC2965Spec extends RFC2109Spec {

    /**
     * Default constructor
     *
     */
    public RFC2965Spec() {
        this(null, false);
    }

    public RFC2965Spec(final String[] datepatterns, boolean oneHeader) {
        super(datepatterns, oneHeader);
        registerAttribHandler(ClientCookie.DOMAIN_ATTR, new RFC2965DomainAttributeHandler());
        registerAttribHandler(ClientCookie.PORT_ATTR, new RFC2965PortAttributeHandler());
        registerAttribHandler(ClientCookie.COMMENTURL_ATTR, new RFC2965CommentUrlAttributeHandler());
        registerAttribHandler(ClientCookie.DISCARD_ATTR, new RFC2965DiscardAttributeHandler());
        registerAttribHandler(ClientCookie.VERSION_ATTR, new RFC2965VersionAttributeHandler());
    }

    @Override
    public List<Cookie> parse(
            final Header header,
            CookieOrigin origin) throws MalformedCookieException {
        if (header == null) {
            throw new IllegalArgumentException("Header may not be null");
        }
        if (origin == null) {
            throw new IllegalArgumentException("Cookie origin may not be null");
        }
        if (!header.getName().equalsIgnoreCase(SM.SET_COOKIE2)) {
            throw new MalformedCookieException("Unrecognized cookie header '"
                    + header.toString() + "'");
        }
        origin = adjustEffectiveHost(origin);
        HeaderElement[] elems = header.getElements();
        return createCookies(elems, origin);
    }

    @Override
    protected List<Cookie> parse(
            final HeaderElement[] elems,
            CookieOrigin origin) throws MalformedCookieException {
        origin = adjustEffectiveHost(origin);
        return createCookies(elems, origin);
    }

    private List<Cookie> createCookies(
            final HeaderElement[] elems,
            final CookieOrigin origin) throws MalformedCookieException {
        List<Cookie> cookies = new ArrayList<Cookie>(elems.length);
        for (HeaderElement headerelement : elems) {
            String name = headerelement.getName();
            String value = headerelement.getValue();
            if (name == null || name.length() == 0) {
                throw new MalformedCookieException("Cookie name may not be empty");
            }

            BasicClientCookie2 cookie = new BasicClientCookie2(name, value);
            cookie.setPath(getDefaultPath(origin));
            cookie.setDomain(getDefaultDomain(origin));
            cookie.setPorts(new int [] { origin.getPort() });
            // cycle through the parameters
            NameValuePair[] attribs = headerelement.getParameters();

            // Eliminate duplicate attributes. The first occurrence takes precedence
            // See RFC2965: 3.2  Origin Server Role
            Map<String, NameValuePair> attribmap =
                    new HashMap<String, NameValuePair>(attribs.length);
            for (int j = attribs.length - 1; j >= 0; j--) {
                NameValuePair param = attribs[j];
                attribmap.put(param.getName().toLowerCase(Locale.ENGLISH), param);
            }
            for (Map.Entry<String, NameValuePair> entry : attribmap.entrySet()) {
                NameValuePair attrib = entry.getValue();
                String s = attrib.getName().toLowerCase(Locale.ENGLISH);

                cookie.setAttribute(s, attrib.getValue());

                CookieAttributeHandler handler = findAttribHandler(s);
                if (handler != null) {
                    handler.parse(cookie, attrib.getValue());
                }
            }
            cookies.add(cookie);
        }
        return cookies;
    }

    @Override
    public void validate(final Cookie cookie, CookieOrigin origin)
            throws MalformedCookieException {
        if (cookie == null) {
            throw new IllegalArgumentException("Cookie may not be null");
        }
        if (origin == null) {
            throw new IllegalArgumentException("Cookie origin may not be null");
        }
        origin = adjustEffectiveHost(origin);
        super.validate(cookie, origin);
    }

    @Override
    public boolean match(final Cookie cookie, CookieOrigin origin) {
        if (cookie == null) {
            throw new IllegalArgumentException("Cookie may not be null");
        }
        if (origin == null) {
            throw new IllegalArgumentException("Cookie origin may not be null");
        }
        origin = adjustEffectiveHost(origin);
        return super.match(cookie, origin);
    }

    /**
     * Adds valid Port attribute value, e.g. "8000,8001,8002"
     */
    @Override
    protected void formatCookieAsVer(final CharArrayBuffer buffer,
            final Cookie cookie, int version) {
        super.formatCookieAsVer(buffer, cookie, version);
        // format port attribute
        if (cookie instanceof ClientCookie) {
            // Test if the port attribute as set by the origin server is not blank
            String s = ((ClientCookie) cookie).getAttribute(ClientCookie.PORT_ATTR);
            if (s != null) {
                buffer.append("; $Port");
                buffer.append("=\"");
                if (s.trim().length() > 0) {
                    int[] ports = cookie.getPorts();
                    if (ports != null) {
                        for (int i = 0, len = ports.length; i < len; i++) {
                            if (i > 0) {
                                buffer.append(",");
                            }
                            buffer.append(Integer.toString(ports[i]));
                        }
                    }
                }
                buffer.append("\"");
            }
        }
    }

    /**
     * Set 'effective host name' as defined in RFC 2965.
     * <p>
     * If a host name contains no dots, the effective host name is
     * that name with the string .local appended to it.  Otherwise
     * the effective host name is the same as the host name.  Note
     * that all effective host names contain at least one dot.
     *
     * @param origin origin where cookie is received from or being sent to.
     * @return
     */
    private static CookieOrigin adjustEffectiveHost(final CookieOrigin origin) {
        String host = origin.getHost();

        // Test if the host name appears to be a fully qualified DNS name,
        // IPv4 address or IPv6 address
        boolean isLocalHost = true;
        for (int i = 0; i < host.length(); i++) {
            char ch = host.charAt(i);
            if (ch == '.' || ch == ':') {
                isLocalHost = false;
                break;
            }
        }
        if (isLocalHost) {
            host += ".local";
            return new CookieOrigin(
                    host,
                    origin.getPort(),
                    origin.getPath(),
                    origin.isSecure());
        } else {
            return origin;
        }
    }

    @Override
    public int getVersion() {
        return 1;
    }

    @Override
    public Header getVersionHeader() {
        CharArrayBuffer buffer = new CharArrayBuffer(40);
        buffer.append(SM.COOKIE2);
        buffer.append(": ");
        buffer.append("$Version=");
        buffer.append(Integer.toString(getVersion()));
        return new BufferedHeader(buffer);
    }

    @Override
    public String toString() {
        return "rfc2965";
    }

}

