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

package ch.boye.httpclientandroidlib.client.utils;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Stack;

import ch.boye.httpclientandroidlib.annotation.Immutable;

import ch.boye.httpclientandroidlib.HttpHost;

/**
 * A collection of utilities for {@link URI URIs}, to workaround
 * bugs within the class or for ease-of-use features.
 *
 * @since 4.0
 */
@Immutable
public class URIUtils {

     /**
         * Constructs a {@link URI} using all the parameters. This should be
         * used instead of
         * {@link URI#URI(String, String, String, int, String, String, String)}
         * or any of the other URI multi-argument URI constructors.
         *
         * @param scheme
         *            Scheme name
         * @param host
         *            Host name
         * @param port
         *            Port number
         * @param path
         *            Path
         * @param query
         *            Query
         * @param fragment
         *            Fragment
         *
         * @throws URISyntaxException
         *             If both a scheme and a path are given but the path is
         *             relative, if the URI string constructed from the given
         *             components violates RFC&nbsp;2396, or if the authority
         *             component of the string is present but cannot be parsed
         *             as a server-based authority
         */
    public static URI createURI(
            final String scheme,
            final String host,
            int port,
            final String path,
            final String query,
            final String fragment) throws URISyntaxException {

        StringBuilder buffer = new StringBuilder();
        if (host != null) {
            if (scheme != null) {
                buffer.append(scheme);
                buffer.append("://");
            }
            buffer.append(host);
            if (port > 0) {
                buffer.append(':');
                buffer.append(port);
            }
        }
        if (path == null || !path.startsWith("/")) {
            buffer.append('/');
        }
        if (path != null) {
            buffer.append(path);
        }
        if (query != null) {
            buffer.append('?');
            buffer.append(query);
        }
        if (fragment != null) {
            buffer.append('#');
            buffer.append(fragment);
        }
        return new URI(buffer.toString());
    }

    /**
     * A convenience method for creating a new {@link URI} whose scheme, host
     * and port are taken from the target host, but whose path, query and
     * fragment are taken from the existing URI. The fragment is only used if
     * dropFragment is false.
     *
     * @param uri
     *            Contains the path, query and fragment to use.
     * @param target
     *            Contains the scheme, host and port to use.
     * @param dropFragment
     *            True if the fragment should not be copied.
     *
     * @throws URISyntaxException
     *             If the resulting URI is invalid.
     */
    public static URI rewriteURI(
            final URI uri,
            final HttpHost target,
            boolean dropFragment) throws URISyntaxException {
        if (uri == null) {
            throw new IllegalArgumentException("URI may nor be null");
        }
        if (target != null) {
            return URIUtils.createURI(
                    target.getSchemeName(),
                    target.getHostName(),
                    target.getPort(),
                    normalizePath(uri.getRawPath()),
                    uri.getRawQuery(),
                    dropFragment ? null : uri.getRawFragment());
        } else {
            return URIUtils.createURI(
                    null,
                    null,
                    -1,
                    normalizePath(uri.getRawPath()),
                    uri.getRawQuery(),
                    dropFragment ? null : uri.getRawFragment());
        }
    }

    private static String normalizePath(String path) {
        if (path == null) {
            return null;
        }
        int n = 0;
        for (; n < path.length(); n++) {
            if (path.charAt(n) != '/') {
                break;
            }
        }
        if (n > 1) {
            path = path.substring(n - 1);
        }
        return path;
    }

    /**
     * A convenience method for
     * {@link URIUtils#rewriteURI(URI, HttpHost, boolean)} that always keeps the
     * fragment.
     */
    public static URI rewriteURI(
            final URI uri,
            final HttpHost target) throws URISyntaxException {
        return rewriteURI(uri, target, false);
    }

    /**
     * Resolves a URI reference against a base URI. Work-around for bug in
     * java.net.URI (<http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4708535>)
     *
     * @param baseURI the base URI
     * @param reference the URI reference
     * @return the resulting URI
     */
    public static URI resolve(final URI baseURI, final String reference) {
        return URIUtils.resolve(baseURI, URI.create(reference));
    }

    /**
     * Resolves a URI reference against a base URI. Work-around for bugs in
     * java.net.URI (e.g. <http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4708535>)
     *
     * @param baseURI the base URI
     * @param reference the URI reference
     * @return the resulting URI
     */
    public static URI resolve(final URI baseURI, URI reference){
        if (baseURI == null) {
            throw new IllegalArgumentException("Base URI may nor be null");
        }
        if (reference == null) {
            throw new IllegalArgumentException("Reference URI may nor be null");
        }
        String s = reference.toString();
        if (s.startsWith("?")) {
            return resolveReferenceStartingWithQueryString(baseURI, reference);
        }
        boolean emptyReference = s.length() == 0;
        if (emptyReference) {
            reference = URI.create("#");
        }
        URI resolved = baseURI.resolve(reference);
        if (emptyReference) {
            String resolvedString = resolved.toString();
            resolved = URI.create(resolvedString.substring(0,
                resolvedString.indexOf('#')));
        }
        return removeDotSegments(resolved);
    }

    /**
     * Resolves a reference starting with a query string.
     *
     * @param baseURI the base URI
     * @param reference the URI reference starting with a query string
     * @return the resulting URI
     */
    private static URI resolveReferenceStartingWithQueryString(
            final URI baseURI, final URI reference) {
        String baseUri = baseURI.toString();
        baseUri = baseUri.indexOf('?') > -1 ?
            baseUri.substring(0, baseUri.indexOf('?')) : baseUri;
        return URI.create(baseUri + reference.toString());
    }

    /**
     * Removes dot segments according to RFC 3986, section 5.2.4
     *
     * @param uri the original URI
     * @return the URI without dot segments
     */
    private static URI removeDotSegments(URI uri) {
        String path = uri.getPath();
        if ((path == null) || (path.indexOf("/.") == -1)) {
            // No dot segments to remove
            return uri;
        }
        String[] inputSegments = path.split("/");
        Stack<String> outputSegments = new Stack<String>();
        for (int i = 0; i < inputSegments.length; i++) {
            if ((inputSegments[i].length() == 0)
                || (".".equals(inputSegments[i]))) {
                // Do nothing
            } else if ("..".equals(inputSegments[i])) {
                if (!outputSegments.isEmpty()) {
                    outputSegments.pop();
                }
            } else {
                outputSegments.push(inputSegments[i]);
            }
        }
        StringBuilder outputBuffer = new StringBuilder();
        for (String outputSegment : outputSegments) {
            outputBuffer.append('/').append(outputSegment);
        }
        try {
            return new URI(uri.getScheme(), uri.getAuthority(),
                outputBuffer.toString(), uri.getQuery(), uri.getFragment());
        } catch (URISyntaxException e) {
            throw new IllegalArgumentException(e);
        }
    }

    /**
     * Extracts target host from the given {@link URI}.
     * 
     * @param uri 
     * @return the target host if the URI is absolute or <code>null</null> if the URI is 
     * relative or does not contain a valid host name.
     * 
     * @since 4.1
     */
    public static HttpHost extractHost(final URI uri) {
        if (uri == null) {
            return null;
        }
        HttpHost target = null;
        if (uri.isAbsolute()) {
            int port = uri.getPort(); // may be overridden later
            String host = uri.getHost();
            if (host == null) { // normal parse failed; let's do it ourselves
                // authority does not seem to care about the valid character-set for host names
                host = uri.getAuthority();
                if (host != null) {
                    // Strip off any leading user credentials
                    int at = host.indexOf('@');
                    if (at >= 0) {
                        if (host.length() > at+1 ) {
                            host = host.substring(at+1);
                        } else {
                            host = null; // @ on its own
                        }
                    }
                    // Extract the port suffix, if present
                    if (host != null) { 
                        int colon = host.indexOf(':');
                        if (colon >= 0) {
                            if (colon+1 < host.length()) {
                                port = Integer.parseInt(host.substring(colon+1));
                            }
                            host = host.substring(0,colon);
                        }                
                    }                    
                }
            }
            String scheme = uri.getScheme();
            if (host != null) {
                target = new HttpHost(host, port, scheme);
            }
        }
        return target;
    }
    
    /**
     * This class should not be instantiated.
     */
    private URIUtils() {
    }

}
