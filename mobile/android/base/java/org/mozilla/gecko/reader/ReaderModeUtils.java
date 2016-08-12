/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reader;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.util.StringUtils;

import android.net.Uri;

public class ReaderModeUtils {
    private static final String LOGTAG = "ReaderModeUtils";

    /**
     * Extract the URL from a valid about:reader URL. You may want to use stripAboutReaderUrl
     * instead to always obtain a valid String.
     *
     * @see #stripAboutReaderUrl(String) for a safer version that returns the original URL for malformed/invalid
     *     URLs.
     * @return <code>null</code> if the URL is malformed or doesn't contain a URL parameter.
     */
    public static String getUrlFromAboutReader(String aboutReaderUrl) {
        return StringUtils.getQueryParameter(aboutReaderUrl, "url");
    }

    public static boolean isEnteringReaderMode(String currentUrl, String newUrl) {
        if (currentUrl == null || newUrl == null) {
            return false;
        }

        if (!AboutPages.isAboutReader(newUrl)) {
            return false;
        }

        String urlFromAboutReader = getUrlFromAboutReader(newUrl);
        if (urlFromAboutReader == null) {
            return false;
        }

        return urlFromAboutReader.equals(currentUrl);
    }

    public static String getAboutReaderForUrl(String url) {
        return getAboutReaderForUrl(url, -1);
    }

    public static String stripAboutReaderUrl(String url) {
        if (!AboutPages.isAboutReader(url)) {
            return url;
        }

        final String strippedUrl = getUrlFromAboutReader(url);
        return strippedUrl != null ? strippedUrl : url;
    }

    public static String getAboutReaderForUrl(String url, int tabId) {
        String aboutReaderUrl = AboutPages.READER + "?url=" + Uri.encode(url);

        if (tabId >= 0) {
            aboutReaderUrl += "&tabId=" + tabId;
        }

        return aboutReaderUrl;
    }
}
