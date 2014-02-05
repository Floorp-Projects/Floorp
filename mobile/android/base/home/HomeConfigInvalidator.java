/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.home.PanelManager.PanelInfo;
import org.mozilla.gecko.home.PanelManager.RequestCallback;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import static org.mozilla.gecko.home.HomeConfig.createBuiltinPanelConfig;

import android.content.Context;
import android.os.Handler;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentLinkedQueue;

public class HomeConfigInvalidator implements GeckoEventListener {
    public static final String LOGTAG = "HomeConfigInvalidator";

    private static final HomeConfigInvalidator sInstance = new HomeConfigInvalidator();

    private static final int INVALIDATION_DELAY_MSEC = 500;
    private static final int PANEL_INFO_TIMEOUT_MSEC = 1000;

    private static final String EVENT_HOMEPANELS_INSTALL = "HomePanels:Install";
    private static final String EVENT_HOMEPANELS_REMOVE = "HomePanels:Remove";

    private static final String JSON_KEY_PANEL = "panel";

    private Context mContext;
    private HomeConfig mHomeConfig;

    private final ConcurrentLinkedQueue<PanelConfig> mPendingChanges = new ConcurrentLinkedQueue<PanelConfig>();
    private final Runnable mInvalidationRunnable = new InvalidationRunnable();

    public static HomeConfigInvalidator getInstance() {
        return sInstance;
    }

    public void init(Context context) {
        mContext = context;
        mHomeConfig = HomeConfig.getDefault(context);

        GeckoAppShell.getEventDispatcher().registerEventListener(EVENT_HOMEPANELS_INSTALL, this);
        GeckoAppShell.getEventDispatcher().registerEventListener(EVENT_HOMEPANELS_REMOVE, this);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            final JSONObject json = message.getJSONObject(JSON_KEY_PANEL);
            final PanelConfig panelConfig = new PanelConfig(json);

            if (event.equals(EVENT_HOMEPANELS_INSTALL)) {
                Log.d(LOGTAG, EVENT_HOMEPANELS_INSTALL);
                handlePanelInstall(panelConfig);
            } else if (event.equals(EVENT_HOMEPANELS_REMOVE)) {
                Log.d(LOGTAG, EVENT_HOMEPANELS_REMOVE);
                handlePanelRemove(panelConfig);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to handle event " + event, e);
        }
    }

    /**
     * Runs in the gecko thread.
     */
    private void handlePanelInstall(PanelConfig panelConfig) {
        mPendingChanges.offer(panelConfig);
        Log.d(LOGTAG, "handlePanelInstall: " + mPendingChanges.size());

        scheduleInvalidation();
    }

    /**
     * Runs in the gecko thread.
     */
    private void handlePanelRemove(PanelConfig panelConfig) {
        panelConfig.setIsDeleted(true);
        mPendingChanges.offer(panelConfig);
        Log.d(LOGTAG, "handlePanelRemove: " + mPendingChanges.size());

        scheduleInvalidation();
    }

    /**
     * Runs in the gecko or main thread.
     */
    private void scheduleInvalidation() {
        final Handler handler = ThreadUtils.getBackgroundHandler();

        handler.removeCallbacks(mInvalidationRunnable);
        handler.postDelayed(mInvalidationRunnable, INVALIDATION_DELAY_MSEC);

        Log.d(LOGTAG, "scheduleInvalidation: scheduled new invalidation");
    }

    /**
     * Runs in the background thread.
     */
    private List<PanelConfig> executePendingChanges(List<PanelConfig> panelConfigs) {
        while (!mPendingChanges.isEmpty()) {
            final PanelConfig panelConfig = mPendingChanges.poll();
            final String id = panelConfig.getId();

            if (panelConfig.isDeleted()) {
                if (panelConfigs.remove(panelConfig)) {
                    Log.d(LOGTAG, "executePendingChanges: removed panel " + id);
                }
            } else {
                final int index = panelConfigs.indexOf(panelConfig);
                if (index >= 0) {
                    panelConfigs.set(index, panelConfig);
                    Log.d(LOGTAG, "executePendingChanges: replaced position " + index + " with " + id);
                } else {
                    panelConfigs.add(panelConfig);
                    Log.d(LOGTAG, "executePendingChanges: added panel " + id);
                }
            }
        }

        return executeRefresh(panelConfigs);
    }

    /**
     * Runs in the background thread.
     */
    private List<PanelConfig> refreshFromPanelInfos(List<PanelConfig> panelConfigs, List<PanelInfo> panelInfos) {
        Log.d(LOGTAG, "refreshFromPanelInfos");

        final int count = panelConfigs.size();
        for (int i = 0; i < count; i++) {
            final PanelConfig panelConfig = panelConfigs.get(i);

            PanelConfig refreshedPanelConfig = null;
            if (panelConfig.isDynamic()) {
                for (PanelInfo panelInfo : panelInfos) {
                    if (panelInfo.getId().equals(panelConfig.getId())) {
                        refreshedPanelConfig = panelInfo.toPanelConfig();
                        Log.d(LOGTAG, "refreshFromPanelInfos: refreshing from panel info: " + panelInfo.getId());
                        break;
                    }
                }
            } else {
                refreshedPanelConfig = createBuiltinPanelConfig(mContext, panelConfig.getType());
                Log.d(LOGTAG, "refreshFromPanelInfos: refreshing built-in panel: " + panelConfig.getId());
            }

            if (refreshedPanelConfig == null) {
                Log.d(LOGTAG, "refreshFromPanelInfos: no refreshed panel, falling back: " + panelConfig.getId());
                refreshedPanelConfig = panelConfig;
            }

            refreshedPanelConfig.setIsDefault(panelConfig.isDefault());
            refreshedPanelConfig.setIsDisabled(panelConfig.isDisabled());

            Log.d(LOGTAG, "refreshFromPanelInfos: set " + i + " with " + refreshedPanelConfig.getId());
            panelConfigs.set(i, refreshedPanelConfig);
        }

        return panelConfigs;
    }

    /**
     * Runs in the background thread.
     */
    private List<PanelConfig> executeRefresh(List<PanelConfig> panelConfigs) {
        if (panelConfigs.isEmpty()) {
            return panelConfigs;
        }

        Log.d(LOGTAG, "executeRefresh");

        final Set<String> ids = new HashSet<String>();
        for (PanelConfig panelConfig : panelConfigs) {
            ids.add(panelConfig.getId());
        }

        final Object panelRequestLock = new Object();
        final List<PanelInfo> latestPanelInfos = new ArrayList<PanelInfo>();

        final PanelManager pm = new PanelManager();
        pm.requestPanelsById(ids, new RequestCallback() {
            @Override
            public void onComplete(List<PanelInfo> panelInfos) {
                synchronized(panelRequestLock) {
                    latestPanelInfos.addAll(panelInfos);
                    Log.d(LOGTAG, "executeRefresh: fetched panel infos: " + panelInfos.size());

                    panelRequestLock.notifyAll();
                }
            }
        });

        try {
            synchronized(panelRequestLock) {
                panelRequestLock.wait(PANEL_INFO_TIMEOUT_MSEC);

                Log.d(LOGTAG, "executeRefresh: done fetching panel infos");
                return refreshFromPanelInfos(panelConfigs, latestPanelInfos);
            }
        } catch (InterruptedException e) {
            Log.e(LOGTAG, "Failed to fetch panels from gecko", e);
            return panelConfigs;
        }
    }

    /**
     * Runs in the background thread.
     */
    private class InvalidationRunnable implements Runnable {
        @Override
        public void run() {
            mHomeConfig.save(executePendingChanges(mHomeConfig.load()));
        }
    };
}