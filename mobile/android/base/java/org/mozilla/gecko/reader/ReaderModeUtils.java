/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reader;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.util.StringUtils;

import android.net.Uri;

public class ReaderModeUtils {
    private static final String LOGTAG = "ReaderModeUtils";

    public static String getUrlFromAboutReader(String aboutReaderUrl) {
        return StringUtils.getQueryParameter(aboutReaderUrl, "url");
    }

    public static boolean isEnteringReaderMode(String oldURL, String newURL) {
        if (oldURL == null || newURL == null) {
            return false;
        }

        if (!AboutPages.isAboutReader(newURL)) {
            return false;
        }

        String urlFromAboutReader = getUrlFromAboutReader(newURL);
        if (urlFromAboutReader == null) {
            return false;
        }

        return urlFromAboutReader.equals(oldURL);
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
