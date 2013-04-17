/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

public class StringUtils {
    /*
     * This method tries to guess if the given string could be a search query or URL,
     * and returns a previous result if there is ambiguity
     *
     * Search examples:
     *  foo
     *  foo bar.com
     *  foo http://bar.com
     *
     * URL examples
     *  foo.com
     *  foo.c
     *  :foo
     *  http://foo.com bar
     *
     * wasSearchQuery specifies whether text was a search query before the latest change
     * in text. In ambiguous cases where the new text can be either a search or a URL,
     * wasSearchQuery is returned
    */
    public static boolean isSearchQuery(String text, boolean wasSearchQuery) {
        if (text.length() == 0)
            return wasSearchQuery;

        int colon = text.indexOf(':');
        int dot = text.indexOf('.');
        int space = text.indexOf(' ');

        // If a space is found before any dot and colon, we assume this is a search query
        if (space > -1 && (colon == -1 || space < colon) && (dot == -1 || space < dot)) {
            return true;
        }
        // Otherwise, if a dot or a colon is found, we assume this is a URL
        if (dot > -1 || colon > -1) {
            return false;
        }
        // Otherwise, text is ambiguous, and we keep its status unchanged
        return wasSearchQuery;
    }

    public static String stripScheme(String url) {
        if (url == null)
            return url;

        if (url.startsWith("http://")) {
            return url.substring(7);
        }
        return url;
    }

    public static String stripCommonSubdomains(String host) {
        if (host == null)
            return host;
        // In contrast to desktop, we also strip mobile subdomains,
        // since its unlikely users are intentionally typing them
        if (host.startsWith("www.")) return host.substring(4);
        else if (host.startsWith("mobile.")) return host.substring(7);
        else if (host.startsWith("m.")) return host.substring(2);
        return host;
    }

}
