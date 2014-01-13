/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

public class PanelManager implements GeckoEventListener {
    private static final String LOGTAG = "GeckoPanelManager";

    public class PanelInfo {
        public final String id;
        public final String title;

        public PanelInfo(String id, String title) {
            this.id = id;
            this.title = title;
        }
    }

    private final Context mContext;

    public PanelManager(Context context) {
        mContext = context;

        // Add a listener to handle any new panels that are added after the panels have been loaded.
        GeckoAppShell.getEventDispatcher().registerEventListener("HomePanels:Added", this);
    }

    /**
     * Reads list info from SharedPreferences. Don't call this on the main thread!
     *
     * @return List<PanelInfo> A list of PanelInfos for each registered list.
     */
    public List<PanelInfo> getPanelInfos() {
        final ArrayList<PanelInfo> panelInfos = new ArrayList<PanelInfo>();

        // XXX: We need to use PreferenceManager right now because that's what SharedPreferences.jsm uses (see bug 940575)
        final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(mContext);
        final String prefValue = prefs.getString("home_lists", "");

        if (!TextUtils.isEmpty(prefValue)) {
            try {
                final JSONArray lists = new JSONArray(prefValue);
                for (int i = 0; i < lists.length(); i++) {
                    final JSONObject list = lists.getJSONObject(i);
                    final PanelInfo info = new PanelInfo(list.getString("id"), list.getString("title"));
                    panelInfos.add(info);
                }
            } catch (JSONException e) {
                Log.e(LOGTAG, "Exception getting list info", e);
            }
        }
        return panelInfos;
    }

    /**
     * Listens for "HomePanels:Added"
     */
    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            final PanelInfo info = new PanelInfo(message.getString("id"), message.getString("title"));

            // Do something to update the set of list pages.

        } catch (JSONException e) {
            Log.e(LOGTAG, "Exception handling " + event + " message", e);
        }
    }
}
