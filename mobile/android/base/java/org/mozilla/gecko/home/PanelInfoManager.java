/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

import org.json.JSONException;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.util.Log;
import android.util.SparseArray;

public class PanelInfoManager implements BundleEventListener {
    private static final String LOGTAG = "GeckoPanelInfoManager";

    public class PanelInfo {
        private final String mId;
        private final String mTitle;
        private final GeckoBundle mBundleData;

        public PanelInfo(String id, String title, final GeckoBundle bundleData) {
            mId = id;
            mTitle = title;
            mBundleData = bundleData;
        }

        public String getId() {
            return mId;
        }

        public String getTitle() {
            return mTitle;
        }

        public PanelConfig toPanelConfig() {
            try {
                return new PanelConfig(mBundleData.toJSONObject());
            } catch (final JSONException e) {
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
                EventDispatcher.getInstance().registerUiThreadListener(this,
                    "HomePanels:Data");
            }
            sCallbacks.put(requestId, callback);
        }

        final GeckoBundle message = new GeckoBundle(2);
        message.putInt("requestId", requestId);

        if (ids != null && ids.size() > 0) {
            message.putStringArray("ids", ids.toArray(new String[ids.size()]));
        }

        EventDispatcher.getInstance().dispatch("HomePanels:Get", message);
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
    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback cb) {
        final ArrayList<PanelInfo> panelInfos = new ArrayList<PanelInfo>();
        final GeckoBundle[] panels = message.getBundleArray("panels");

        for (int i = 0; i < panels.length; i++) {
            panelInfos.add(getPanelInfoFromBundle(panels[i]));
        }

        final RequestCallback callback;
        final int requestId = message.getInt("requestId");

        synchronized (sCallbacks) {
            callback = sCallbacks.get(requestId);
            sCallbacks.delete(requestId);

            // Unregister the event listener if there are no more pending callbacks.
            if (sCallbacks.size() == 0) {
                EventDispatcher.getInstance().unregisterUiThreadListener(this,
                    "HomePanels:Data");
            }
        }

        callback.onComplete(panelInfos);
    }

    private PanelInfo getPanelInfoFromBundle(final GeckoBundle bundlePanelInfo) {
        final String id = bundlePanelInfo.getString("id");
        final String title = bundlePanelInfo.getString("title");

        return new PanelInfo(id, title, bundlePanelInfo);
    }
}
