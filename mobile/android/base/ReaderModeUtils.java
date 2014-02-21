/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.util.StringUtils;

import android.net.Uri;
import android.util.Log;

public class ReaderModeUtils {
    private static final String LOGTAG = "ReaderModeUtils";

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

    public static String getAboutReaderForUrl(String url, int tabId) {
        String aboutReaderUrl = AboutPages.READER + "?url=" + Uri.encode(url);

        if (tabId >= 0) {
            aboutReaderUrl += "&tabId=" + tabId;
        }

        return aboutReaderUrl;
    }

    public static void addToReadingList(Tab tab) {
        if (!tab.getReaderEnabled()) {
            return;
        }

        JSONObject json = new JSONObject();
        try {
            json.put("tabID", String.valueOf(tab.getId()));
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON error - failing to add to reading list", e);
            return;
        }

        GeckoEvent e = GeckoEvent.createBroadcastEvent("Reader:Add", json.toString());
        GeckoAppShell.sendEventToGecko(e);
    }

    public static void removeFromReadingList(String url) {
        if (url == null) {
            return;
        }

        GeckoEvent e = GeckoEvent.createBroadcastEvent("Reader:Remove", url);
        GeckoAppShell.sendEventToGecko(e);
    }
}
