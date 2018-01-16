/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.graphics.Paint;
import android.graphics.Rect;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import java.nio.charset.Charset;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class StringUtils {
    private static final String LOGTAG = "GeckoStringUtils";

    private static final String FILTER_URL_PREFIX = "filter://";
    private static final String USER_ENTERED_URL_PREFIX = "user-entered:";


    /**
     * The UTF-8 charset.
     */
    public static final Charset UTF_8 = Charset.forName("UTF-8");

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
        // We remove leading and trailing white spaces when decoding URLs
        text = text.trim();
        if (text.length() == 0) {
            return wasSearchQuery;
        }
        int colon = text.indexOf(':');
        int dot = text.indexOf('.');
        int space = text.indexOf(' ');

        // If a space is found in a trimmed string, we assume this is a search query(Bug 1278245)
        if (space > -1) {
            return true;
        }
        // Otherwise, if a dot or a colon is found, we assume this is a URL
        if (dot > -1 || colon > -1) {
            return false;
        }
        // Otherwise, text is ambiguous, and we keep its status unchanged
        return wasSearchQuery;
    }

    /**
     * Strip the ref from a URL, if present
     *
     * @return The base URL, without the ref. The original String is returned if it has no ref,
     *         of if the input is malformed.
     */
    public static String stripRef(final String inputURL) {
        if (inputURL == null) {
            return null;
        }

        final int refIndex = inputURL.indexOf('#');

        if (refIndex >= 0) {
            return inputURL.substring(0, refIndex);
        }

        return inputURL;
    }

    public static class UrlFlags {
        public static final int NONE = 0;
        public static final int STRIP_HTTPS = 1;
    }

    public static String stripScheme(String url) {
        return stripScheme(url, UrlFlags.NONE);
    }

    public static String stripScheme(String url, int flags) {
        if (url == null) {
            return url;
        }

        String newURL = url;

        if (newURL.startsWith("http://")) {
            newURL = newURL.replace("http://", "");
        } else if (newURL.startsWith("https://") && flags == UrlFlags.STRIP_HTTPS) {
            newURL = newURL.replace("https://", "");
        }

        if (newURL.endsWith("/")) {
            newURL = newURL.substring(0, newURL.length() - 1);
        }

        return newURL;
    }

    public static boolean isHttpOrHttps(String url) {
        if (TextUtils.isEmpty(url)) {
            return false;
        }

        return url.startsWith("http://") || url.startsWith("https://");
    }

    public static String stripCommonSubdomains(String host) {
        if (host == null) {
            return host;
        }

        // In contrast to desktop, we also strip mobile subdomains,
        // since its unlikely users are intentionally typing them
        int start = 0;

        if (host.startsWith("www.")) {
            start = 4;
        } else if (host.startsWith("mobile.")) {
            start = 7;
        } else if (host.startsWith("m.")) {
            start = 2;
        }

        return host.substring(start);
    }

    /**
     * Searches the url query string for the first value with the given key.
     */
    public static String getQueryParameter(String url, String desiredKey) {
        if (TextUtils.isEmpty(url) || TextUtils.isEmpty(desiredKey)) {
            return null;
        }

        final String[] urlParts = url.split("\\?");
        if (urlParts.length < 2) {
            return null;
        }

        final String query = urlParts[1];
        for (final String param : query.split("&")) {
            final String pair[] = param.split("=");
            final String key = Uri.decode(pair[0]);

            // Key is empty or does not match the key we're looking for, discard
            if (TextUtils.isEmpty(key) || !key.equals(desiredKey)) {
                continue;
            }
            // No value associated with key, discard
            if (pair.length < 2) {
                continue;
            }
            final String value = Uri.decode(pair[1]);
            if (TextUtils.isEmpty(value)) {
                return null;
            }
            return value;
        }

        return null;
    }

    public static boolean isFilterUrl(String url) {
        if (TextUtils.isEmpty(url)) {
            return false;
        }

        return url.startsWith(FILTER_URL_PREFIX);
    }

    public static String getFilterFromUrl(String url) {
        if (TextUtils.isEmpty(url)) {
            return null;
        }

        return url.substring(FILTER_URL_PREFIX.length());
    }

    public static boolean isShareableUrl(final String url) {
        final String scheme = Uri.parse(url).getScheme();
        return !("about".equals(scheme) || "chrome".equals(scheme) ||
                "file".equals(scheme) || "resource".equals(scheme));
    }

    public static boolean isUserEnteredUrl(String url) {
        return (url != null && url.startsWith(USER_ENTERED_URL_PREFIX));
    }

    /**
     * Given a url with a user-entered scheme, extract the
     * scheme-specific component. For e.g, given "user-entered://www.google.com",
     * this method returns "//www.google.com". If the passed url
     * does not have a user-entered scheme, the same url will be returned.
     *
     * @param  url to be decoded
     * @return url component entered by user
     */
    public static String decodeUserEnteredUrl(String url) {
        Uri uri = Uri.parse(url);
        if ("user-entered".equals(uri.getScheme())) {
            return uri.getSchemeSpecificPart();
        }
        return url;
    }

    public static String encodeUserEnteredUrl(String url) {
        return Uri.fromParts("user-entered", url, null).toString();
    }

    /**
     * Compatibility layer for API &lt; 11.
     *
     * Returns a set of the unique names of all query parameters. Iterating
     * over the set will return the names in order of their first occurrence.
     *
     * @param uri
     * @throws UnsupportedOperationException if this isn't a hierarchical URI
     *
     * @return a set of decoded names
     */
    public static Set<String> getQueryParameterNames(Uri uri) {
        return uri.getQueryParameterNames();
    }

    /**
     * @return  the index of the path segment of URLs
     */
    public static int pathStartIndex(String text) {
        if (text.contains("://")) {
            return text.indexOf('/', text.indexOf("://") + 3);
        } else {
            return text.indexOf('/');
        }
    }

    public static String safeSubstring(@NonNull final String str, final int start, final int end) {
        return str.substring(
                Math.max(0, start),
                Math.min(end, str.length()));
    }

    /**
     * Check if this might be a RTL (right-to-left) text by looking at the first character.
     */
    public static boolean isRTL(String text) {
        if (TextUtils.isEmpty(text)) {
            return false;
        }

        final char character = text.charAt(0);
        final byte directionality = Character.getDirectionality(character);

        return directionality == Character.DIRECTIONALITY_RIGHT_TO_LEFT
                || directionality == Character.DIRECTIONALITY_RIGHT_TO_LEFT_ARABIC
                || directionality == Character.DIRECTIONALITY_RIGHT_TO_LEFT_EMBEDDING
                || directionality == Character.DIRECTIONALITY_RIGHT_TO_LEFT_OVERRIDE;
    }

    /**
     * Force LTR (left-to-right) by prepending the text with the "left-to-right mark" (U+200E) if needed.
     */
    public static String forceLTR(String text) {
        if (!isRTL(text)) {
            return text;
        }

        return "\u200E" + text;
    }

    /**
     * Case-insensitive version of {@link String#startsWith(String)}.
     */
    public static boolean caseInsensitiveStartsWith(String text, String prefix) {
        return caseInsensitiveStartsWith(text, prefix, 0);
    }

    /**
     * Case-insensitive version of {@link String#startsWith(String, int)}.
     */
    public static boolean caseInsensitiveStartsWith(String text, String prefix, int start) {
        return text.regionMatches(true, start, prefix, 0, prefix.length());
    }

    /**
     * Measures the width of the given substring when rendered using the specified Paint.
     *
     * @param text      String to measure and return its width
     * @param start     Index of the first char in the string to measure
     * @param end       1 past the last char in the string measure
     * @param textPaint the paint used to render the text
     * @return          the width of the specified substring in screen pixels
     */
    public static int getTextWidth(final String text, final int start, final int end, final Paint textPaint) {
        final Rect bounds = new Rect();
        textPaint.getTextBounds(text, start, end, bounds);
        return bounds.width();
    }
}