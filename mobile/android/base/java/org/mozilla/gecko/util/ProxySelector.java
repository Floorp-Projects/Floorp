/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This code is based on AOSP /libcore/luni/src/main/java/java/net/ProxySelectorImpl.java

package org.mozilla.gecko.util;

import android.text.TextUtils;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Proxy;

public class ProxySelector {
    public ProxySelector() {
    }

    public Proxy select(String scheme, String host) {
        int port = -1;
        Proxy proxy = null;
        String nonProxyHostsKey = null;
        boolean httpProxyOkay = true;
        if ("http".equalsIgnoreCase(scheme)) {
            port = 80;
            nonProxyHostsKey = "http.nonProxyHosts";
            proxy = lookupProxy("http.proxyHost", "http.proxyPort", Proxy.Type.HTTP, port);
        } else if ("https".equalsIgnoreCase(scheme)) {
            port = 443;
            nonProxyHostsKey = "https.nonProxyHosts"; // RI doesn't support this
            proxy = lookupProxy("https.proxyHost", "https.proxyPort", Proxy.Type.HTTP, port);
        } else if ("ftp".equalsIgnoreCase(scheme)) {
            port = 80; // not 21 as you might guess
            nonProxyHostsKey = "ftp.nonProxyHosts";
            proxy = lookupProxy("ftp.proxyHost", "ftp.proxyPort", Proxy.Type.HTTP, port);
        } else if ("socket".equalsIgnoreCase(scheme)) {
            httpProxyOkay = false;
        } else {
            return Proxy.NO_PROXY;
        }

        if (nonProxyHostsKey != null
                && isNonProxyHost(host, System.getProperty(nonProxyHostsKey))) {
            return Proxy.NO_PROXY;
        }

        if (proxy != null) {
            return proxy;
        }

        if (httpProxyOkay) {
            proxy = lookupProxy("proxyHost", "proxyPort", Proxy.Type.HTTP, port);
            if (proxy != null) {
                return proxy;
            }
        }

        proxy = lookupProxy("socksProxyHost", "socksProxyPort", Proxy.Type.SOCKS, 1080);
        if (proxy != null) {
            return proxy;
        }

        return Proxy.NO_PROXY;
    }

    /**
     * Returns the proxy identified by the {@code hostKey} system property, or
     * null.
     */
    private Proxy lookupProxy(String hostKey, String portKey, Proxy.Type type, int defaultPort) {
        String host = System.getProperty(hostKey);
        if (TextUtils.isEmpty(host)) {
            return null;
        }

        int port = getSystemPropertyInt(portKey, defaultPort);
        return new Proxy(type, InetSocketAddress.createUnresolved(host, port));
    }

    private int getSystemPropertyInt(String key, int defaultValue) {
        String string = System.getProperty(key);
        if (string != null) {
            try {
                return Integer.parseInt(string);
            } catch (NumberFormatException ignored) {
            }
        }
        return defaultValue;
    }

    /**
     * Returns true if the {@code nonProxyHosts} system property pattern exists
     * and matches {@code host}.
     */
    private boolean isNonProxyHost(String host, String nonProxyHosts) {
        if (host == null || nonProxyHosts == null) {
            return false;
        }

        // construct pattern
        StringBuilder patternBuilder = new StringBuilder();
        for (int i = 0; i < nonProxyHosts.length(); i++) {
            char c = nonProxyHosts.charAt(i);
            switch (c) {
            case '.':
                patternBuilder.append("\\.");
                break;
            case '*':
                patternBuilder.append(".*");
                break;
            default:
                patternBuilder.append(c);
            }
        }
        // check whether the host is the nonProxyHosts.
        String pattern = patternBuilder.toString();
        return host.matches(pattern);
    }
}

