/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.net.Uri;

public class ReaderModeUtils {
    private static final String LOGTAG = "ReaderModeUtils";

    public static String getAboutReaderForUrl(String url, boolean inReadingList) {
        return getAboutReaderForUrl(url, -1, inReadingList);
    }

    public static String getAboutReaderForUrl(String url, int tabId, boolean inReadingList) {
        String aboutReaderUrl = "about:reader?url=" + Uri.encode(url) +
                                "&readingList=" + (inReadingList ? 1 : 0);

        if (tabId >= 0)
            aboutReaderUrl += "&tabId=" + tabId;

        return aboutReaderUrl;
    }
}
