/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

public class PanelManager implements GeckoEventListener {
    private static final String LOGTAG = "GeckoPanelManager";

    public class PanelInfo {
        public final String id;
        public final String title;
        public final String layout;
        public final JSONArray views;

        public PanelInfo(String id, String title, String layout, JSONArray views) {
            this.id = id;
            this.title = title;
            this.layout = layout;
            this.views = views;
        }
    }

    public interface RequestCallback {
        public void onComplete(List<PanelInfo> panelInfos);
    }

    private static AtomicInteger sRequestId = new AtomicInteger(0);

    // Stores set of pending request callbacks.
    private static final Map<Integer, RequestCallback> sCallbacks = Collections.synchronizedMap(new HashMap<Integer, RequestCallback>());

    /**
     * Asynchronously fetches list of available panels from Gecko.
     *
     * @param callback onComplete will be called on the UI thread.
     */
    public void requestAvailablePanels(RequestCallback callback) {
        final int requestId = sRequestId.getAndIncrement();

        synchronized(sCallbacks) {
            // If there are no pending callbacks, register the event listener.
            if (sCallbacks.isEmpty()) {
                GeckoAppShell.getEventDispatcher().registerEventListener("HomePanels:Data", this);
            }
            sCallbacks.put(requestId, callback);
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("HomePanels:Get", Integer.toString(requestId)));
    }

    /**
     * Handles "HomePanels:Data" events.
     */
    @Override
    public void handleMessage(String event, JSONObject message) {
        final ArrayList<PanelInfo> panelInfos = new ArrayList<PanelInfo>();

        try {
            final JSONArray panels = message.getJSONArray("panels");
            final int count = panels.length();
            for (int i = 0; i < count; i++) {
                final PanelInfo panelInfo = getPanelInfoFromJSON(panels.getJSONObject(i));
                panelInfos.add(panelInfo);
            }

            final RequestCallback callback;
            final int requestId = message.getInt("requestId");

            synchronized(sCallbacks) {
                callback = sCallbacks.remove(requestId);

                // Unregister the event listener if there are no more pending callbacks.
                if (sCallbacks.isEmpty()) {
                    GeckoAppShell.getEventDispatcher().unregisterEventListener("HomePanels:Data", this);
                }
            }

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    callback.onComplete(panelInfos);
                }
            });
        } catch (JSONException e) {
            Log.e(LOGTAG, "Exception handling " + event + " message", e);
        }
    }

    private PanelInfo getPanelInfoFromJSON(JSONObject jsonPanelInfo) throws JSONException {
        final String id = jsonPanelInfo.getString("id");
        final String title = jsonPanelInfo.getString("title");
        final String layout = jsonPanelInfo.getString("layout");
        final JSONArray views = jsonPanelInfo.getJSONArray("views");

        return new PanelInfo(id, title, layout, views);
    }
}
