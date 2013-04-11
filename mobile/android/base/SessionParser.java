/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.util.Log;

public abstract class SessionParser {
    private static final String LOGTAG = "GeckoSessionParser";

    public class SessionTab {
        String mSelectedTitle;
        String mSelectedUrl;
        boolean mIsSelected;
        JSONObject mTabObject;

        private SessionTab(String selectedTitle, String selectedUrl, boolean isSelected, JSONObject tabObject) {
            mSelectedTitle = selectedTitle;
            mSelectedUrl = selectedUrl;
            mIsSelected = isSelected;
            mTabObject = tabObject;
        }

        public String getSelectedTitle() {
            return mSelectedTitle;
        }

        public String getSelectedUrl() {
            return mSelectedUrl;
        }

        public boolean isSelected() {
            return mIsSelected;
        }

        public JSONObject getTabObject() {
            return mTabObject;
        }
    };

    abstract public void onTabRead(SessionTab tab);

    public void parse(String sessionString) {
        final JSONArray tabs;
        final JSONObject window;
        int selected = -1;
        try {
            window = new JSONObject(sessionString).getJSONArray("windows").getJSONObject(0);
            tabs = window.getJSONArray("tabs");
            selected = window.optInt("selected", -1);
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON error", e);
            return;
        }

        int numTabs = tabs.length();
        if (selected < 1 || selected > numTabs) {
            selected = 1;
        }

        for (int i = 0; i < numTabs; i++) {
            try {
                JSONObject tab = tabs.getJSONObject(i);
                int index = tab.getInt("index");
                JSONObject entry = tab.getJSONArray("entries").getJSONObject(index - 1);
                String url = entry.getString("url");

                String title = entry.optString("title");
                if (title.length() == 0) {
                    title = url;
                }

                onTabRead(new SessionTab(title, url, (selected == i+1), tab));
            } catch (JSONException e) {
                Log.e(LOGTAG, "error reading json file", e);
                return;
            }
        }
    }
}
