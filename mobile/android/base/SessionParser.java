/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko;

import java.util.LinkedList;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.util.Log;

public abstract class SessionParser {
    private static final String LOGTAG = "GeckoSessionParser";

    public class SessionTab {
        final private String mTitle;
        final private String mUrl;
        final private JSONObject mTabObject;
        private boolean mIsSelected;

        private SessionTab(String title, String url, boolean isSelected, JSONObject tabObject) {
            mTitle = title;
            mUrl = url;
            mIsSelected = isSelected;
            mTabObject = tabObject;
        }

        public String getTitle() {
            return mTitle;
        }

        public String getUrl() {
            return mUrl;
        }

        public boolean isSelected() {
            return mIsSelected;
        }

        public JSONObject getTabObject() {
            return mTabObject;
        }
    };

    abstract public void onTabRead(SessionTab tab);

    public void parse(String... sessionStrings) {
        final LinkedList<SessionTab> sessionTabs = new LinkedList<SessionTab>();
        int totalCount = 0;
        int selectedIndex = -1;
        try {
            for (String sessionString : sessionStrings) {
                final JSONObject window = new JSONObject(sessionString).getJSONArray("windows").getJSONObject(0);
                final JSONArray tabs = window.getJSONArray("tabs");
                final int optSelected = window.optInt("selected", -1);

                for (int i = 0; i < tabs.length(); i++) {
                    final JSONObject tab = tabs.getJSONObject(i);
                    final int index = tab.getInt("index");
                    final JSONObject entry = tab.getJSONArray("entries").getJSONObject(index - 1);
                    final String url = entry.getString("url");

                    String title = entry.optString("title");
                    if (title.length() == 0) {
                        title = url;
                    }

                    totalCount++;
                    boolean selected = false;
                    if (optSelected == i+1) {
                        selected = true;
                        selectedIndex = totalCount;
                    }
                    sessionTabs.add(new SessionTab(title, url, selected, tab));
                }
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON error", e);
            return;
        }

        // If no selected index was found, select the first tab.
        if (selectedIndex == -1 && sessionTabs.size() > 0) {
            sessionTabs.getFirst().mIsSelected = true;
        }

        for (SessionTab tab : sessionTabs) {
            onTabRead(tab);
        }
    }
}
