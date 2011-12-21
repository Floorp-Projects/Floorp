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

package ch.boye.httpclientandroidlib.protocol;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Maintains a map of objects keyed by a request URI pattern.
 * <br>
 * Patterns may have three formats:
 * <ul>
 *   <li><code>*</code></li>
 *   <li><code>*&lt;uri&gt;</code></li>
 *   <li><code>&lt;uri&gt;*</code></li>
 * </ul>
 * <br>
 * This class can be used to resolve an object matching a particular request
 * URI.
 *
 * @since 4.0
 */
public class UriPatternMatcher {

    /**
     * TODO: Replace with ConcurrentHashMap
     */
    private final Map map;

    public UriPatternMatcher() {
        super();
        this.map = new HashMap();
    }

    /**
     * Registers the given object for URIs matching the given pattern.
     *
     * @param pattern the pattern to register the handler for.
     * @param obj the object.
     */
    public synchronized void register(final String pattern, final Object obj) {
        if (pattern == null) {
            throw new IllegalArgumentException("URI request pattern may not be null");
        }
        this.map.put(pattern, obj);
    }

    /**
     * Removes registered object, if exists, for the given pattern.
     *
     * @param pattern the pattern to unregister.
     */
    public synchronized void unregister(final String pattern) {
        if (pattern == null) {
            return;
        }
        this.map.remove(pattern);
    }

    /**
     * @deprecated use {@link #setObjects(Map)}
     */
    public synchronized void setHandlers(final Map map) {
        if (map == null) {
            throw new IllegalArgumentException("Map of handlers may not be null");
        }
        this.map.clear();
        this.map.putAll(map);
    }

    /**
     * Sets objects from the given map.
     * @param map the map containing objects keyed by their URI patterns.
     */
    public synchronized void setObjects(final Map map) {
        if (map == null) {
            throw new IllegalArgumentException("Map of handlers may not be null");
        }
        this.map.clear();
        this.map.putAll(map);
    }

    /**
     * Looks up an object matching the given request URI.
     *
     * @param requestURI the request URI
     * @return object or <code>null</code> if no match is found.
     */
    public synchronized Object lookup(String requestURI) {
        if (requestURI == null) {
            throw new IllegalArgumentException("Request URI may not be null");
        }
        //Strip away the query part part if found
        int index = requestURI.indexOf("?");
        if (index != -1) {
            requestURI = requestURI.substring(0, index);
        }

        // direct match?
        Object obj = this.map.get(requestURI);
        if (obj == null) {
            // pattern match?
            String bestMatch = null;
            for (Iterator it = this.map.keySet().iterator(); it.hasNext();) {
                String pattern = (String) it.next();
                if (matchUriRequestPattern(pattern, requestURI)) {
                    // we have a match. is it any better?
                    if (bestMatch == null
                            || (bestMatch.length() < pattern.length())
                            || (bestMatch.length() == pattern.length() && pattern.endsWith("*"))) {
                        obj = this.map.get(pattern);
                        bestMatch = pattern;
                    }
                }
            }
        }
        return obj;
    }

    /**
     * Tests if the given request URI matches the given pattern.
     *
     * @param pattern the pattern
     * @param requestUri the request URI
     * @return <code>true</code> if the request URI matches the pattern,
     *   <code>false</code> otherwise.
     */
    protected boolean matchUriRequestPattern(final String pattern, final String requestUri) {
        if (pattern.equals("*")) {
            return true;
        } else {
            return
            (pattern.endsWith("*") && requestUri.startsWith(pattern.substring(0, pattern.length() - 1))) ||
            (pattern.startsWith("*") && requestUri.endsWith(pattern.substring(1, pattern.length())));
        }
    }

}
