/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.net.Uri;
import android.text.TextUtils;

public class UrlUtils {
    public static String normalize(String input) {
        Uri uri = Uri.parse(input);

        if (TextUtils.isEmpty(uri.getScheme())) {
            uri = Uri.parse("http://" + input);
        }

        return uri.toString();
    }

    public static boolean isHttps(String url) {
        // TODO: This should actually check the certificate!

        return url.startsWith("https:");
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

        return url.contains(".");
    }

    public static boolean isSearchQuery(String text) {
        return text.contains(" ");
    }

    public static String createSearchUrl(String rawUrl) {
        return Uri.parse("https://duckduckgo.com/").buildUpon()
                .appendQueryParameter("q", rawUrl)
                .build()
                .toString();
    }
}
