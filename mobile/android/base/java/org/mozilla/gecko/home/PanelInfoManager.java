/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.util.Log;
import android.util.SparseArray;

public class PanelInfoManager implements GeckoEventListener {
    private static final String LOGTAG = "GeckoPanelInfoManager";

    public class PanelInfo {
        private final String mId;
        private final String mTitle;
        private final JSONObject mJSONData;

        public PanelInfo(String id, String title, JSONObject jsonData) {
            mId = id;
            mTitle = title;
            mJSONData = jsonData;
        }

        public String getId() {
            return mId;
        }

        public String getTitle() {
            return mTitle;
        }

        public PanelConfig toPanelConfig() {
            try {
                return new PanelConfig(mJSONData);
            } catch (Exception e) {
                Log.e(LOGTAG, "Failed to convert PanelInfo to PanelConfig", e);
                return null;
            }
        }
    }

    public interface RequestCallback {
        public void onComplete(List<PanelInfo> panelInfos);
    }

    private static final AtomicInteger sRequestId = new AtomicInteger(0);

    // Stores set of pending request callbacks.
    private static final SparseArray<RequestCallback> sCallbacks = new SparseArray<RequestCallback>();

    /**
     * Asynchronously fetches list of available panels from Gecko
     * for the given IDs.
     *
     * @param ids list of panel ids to be fetched. A null value will fetch all
     *        available panels.
     * @param callback onComplete will be called on the UI thread.
     */
    public void requestPanelsById(Set<String> ids, RequestCallback callback) {
        final int requestId = sRequestId.getAndIncrement();

        synchronized (sCallbacks) {
            // If there are no pending callbacks, register the event listener.
            if (sCallbacks.size() == 0) {
                GeckoApp.getEventDispatcher().registerGeckoThreadListener(this,
                    "HomePanels:Data");
            }
            sCallbacks.put(requestId, callback);
        }

        final JSONObject message = new JSONObject();
        try {
            message.put("requestId", requestId);

            if (ids != null && ids.size() > 0) {
                JSONArray idsArray = new JSONArray();
                for (String id : ids) {
                    idsArray.put(id);
                }

                message.put("ids", idsArray);
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Failed to build event to request panels by id", e);
            return;
        }

        GeckoAppShell.notifyObservers("HomePanels:Get", message.toString());
    }

    /**
     * Asynchronously fetches list of available panels from Gecko.
     *
     * @param callback onComplete will be called on the UI thread.
     */
    public void requestAvailablePanels(RequestCallback callback) {
        requestPanelsById(null, callback);
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

            synchronized (sCallbacks) {
                callback = sCallbacks.get(requestId);
                sCallbacks.delete(requestId);

                // Unregister the event listener if there are no more pending callbacks.
                if (sCallbacks.size() == 0) {
                    GeckoApp.getEventDispatcher().unregisterGeckoThreadListener(this,
                        "HomePanels:Data");
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

        return new PanelInfo(id, title, jsonPanelInfo);
    }
}
