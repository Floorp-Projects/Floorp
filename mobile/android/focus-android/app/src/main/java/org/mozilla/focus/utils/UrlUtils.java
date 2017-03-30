/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.search.SearchEngine;
import org.mozilla.focus.search.SearchEngineManager;

import java.net.URI;
import java.net.URISyntaxException;

public class UrlUtils {
    public static String normalize(String input) {
        Uri uri = Uri.parse(input);

        if (TextUtils.isEmpty(uri.getScheme())) {
            uri = Uri.parse("http://" + input);
        }

        return uri.toString();
    }

    /**
     * Is the given string a URL or should we perform a search?
     *
     * TODO: This is a super simple and probably stupid implementation.
     */
    public static boolean isUrl(String url) {
        if (url.contains(" ")) {
            return false;
        }

        return url.contains(".") || url.contains(":");
    }

    public static boolean isSearchQuery(String text) {
        return text.contains(" ");
    }

    public static String createSearchUrl(Context context, String searchTerm) {
        final SearchEngine searchEngine = SearchEngineManager.getInstance()
                .getDefaultSearchEngine(context);

        return searchEngine.buildSearchUrl(searchTerm);
    }

    public static String stripUserInfo(String url) {
        try {
            URI uri = new URI(url);

            final String userInfo = uri.getUserInfo();
            if (userInfo == null) {
                return url;
            }

            // Strip the userInfo to minimise spoofing ability. This only affects what's shown
            // during browsing, this information isn't used when we start editing the URL:
            uri = new URI(uri.getScheme(), null, uri.getHost(), uri.getPort(), uri.getPath(), uri.getQuery(), uri.getFragment());

            return uri.toString();
        } catch (URISyntaxException e) {
            // In general this shouldn't happen. But there are some special cases that URI can't handle,
            // such as "http:" by itself. We allow those individual special cases, but we still want
            // to catch other special syntaxes in dev builds if possible
            if (url.equals("http:") ||
                    url.equals("https:") ||
                    url.equals("file:")) {
                return url;
            }

            if (BuildConfig.DEBUG) {
                // WebView should always have supplied a valid URL
                throw new IllegalStateException("WebView is expected to always supply a valid URL");
            } else {
                return url;
            }
        }
    }

    public static boolean isPermittedResourceProtocol(final String url) {
        return url.startsWith("http:") ||
                url.startsWith("https:") ||
                url.startsWith("file:") ||
                url.startsWith("data:");
    }

    public static boolean isSupportedProtocol(final String url) {
        return isPermittedResourceProtocol(url) ||
                url.startsWith("error:");
    }

    public static boolean urlsMatchExceptForTrailingSlash(final @NonNull String url1, final @NonNull String url2) {
        int lengthDifference = url1.length() - url2.length();

        if (lengthDifference == 0) {
            // The simplest case:
            return url1.equalsIgnoreCase(url2);
        } else if (lengthDifference == 1) {
            // url1 is longer:
            return url1.charAt(url1.length() - 1) == '/' &&
                    url1.regionMatches(true, 0, url2, 0, url2.length());
        } else if (lengthDifference == -1) {
            return url2.charAt(url2.length() - 1) == '/' &&
                    url2.regionMatches(true, 0, url1, 0, url1.length());
        }

        return false;
    }
}
