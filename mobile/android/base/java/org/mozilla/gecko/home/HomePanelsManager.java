/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import static org.mozilla.gecko.home.HomeConfig.createBuiltinPanelConfig;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Queue;
import java.util.Set;
import java.util.concurrent.ConcurrentLinkedQueue;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.HomeProvider;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.PanelInfoManager.PanelInfo;
import org.mozilla.gecko.home.PanelInfoManager.RequestCallback;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ContentResolver;
import android.content.Context;
import android.os.Handler;
import android.util.Log;

public class HomePanelsManager implements GeckoEventListener {
    public static final String LOGTAG = "HomePanelsManager";

    private static final HomePanelsManager sInstance = new HomePanelsManager();

    private static final int INVALIDATION_DELAY_MSEC = 500;
    private static final int PANEL_INFO_TIMEOUT_MSEC = 1000;

    private static final String EVENT_HOMEPANELS_INSTALL = "HomePanels:Install";
    private static final String EVENT_HOMEPANELS_UNINSTALL = "HomePanels:Uninstall";
    private static final String EVENT_HOMEPANELS_UPDATE = "HomePanels:Update";
    private static final String EVENT_HOMEPANELS_REFRESH = "HomePanels:RefreshDataset";

    private static final String JSON_KEY_PANEL = "panel";
    private static final String JSON_KEY_PANEL_ID = "id";

    private enum ChangeType {
        UNINSTALL,
        INSTALL,
        UPDATE,
        REFRESH
    }

    private enum InvalidationMode {
        DELAYED,
        IMMEDIATE
    }

    private static class ConfigChange {
        private final ChangeType type;
        private final Object target;

        public ConfigChange(ChangeType type) {
            this(type, null);
        }

        public ConfigChange(ChangeType type, Object target) {
            this.type = type;
            this.target = target;
        }
    }

    private Context mContext;
    private HomeConfig mHomeConfig;

    private final Queue<ConfigChange> mPendingChanges = new ConcurrentLinkedQueue<ConfigChange>();
    private final Runnable mInvalidationRunnable = new InvalidationRunnable();

    public static HomePanelsManager getInstance() {
        return sInstance;
    }

    public void init(Context context) {
        mContext = context;
        mHomeConfig = HomeConfig.getDefault(context);

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            EVENT_HOMEPANELS_INSTALL,
            EVENT_HOMEPANELS_UNINSTALL,
            EVENT_HOMEPANELS_UPDATE,
            EVENT_HOMEPANELS_REFRESH);
    }

    public void onLocaleReady(final String locale) {
        ThreadUtils.getBackgroundHandler().post(new Runnable() {
            @Override
            public void run() {
                final String configLocale = mHomeConfig.getLocale();
                if (configLocale == null || !configLocale.equals(locale)) {
                    handleLocaleChange();
                }
            }
        });
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals(EVENT_HOMEPANELS_INSTALL)) {
                Log.d(LOGTAG, EVENT_HOMEPANELS_INSTALL);
                handlePanelInstall(createPanelConfigFromMessage(message), InvalidationMode.DELAYED);
            } else if (event.equals(EVENT_HOMEPANELS_UNINSTALL)) {
                Log.d(LOGTAG, EVENT_HOMEPANELS_UNINSTALL);
                final String panelId = message.getString(JSON_KEY_PANEL_ID);
                handlePanelUninstall(panelId);
            } else if (event.equals(EVENT_HOMEPANELS_UPDATE)) {
                Log.d(LOGTAG, EVENT_HOMEPANELS_UPDATE);
                handlePanelUpdate(createPanelConfigFromMessage(message));
            } else if (event.equals(EVENT_HOMEPANELS_REFRESH)) {
                Log.d(LOGTAG, EVENT_HOMEPANELS_REFRESH);
                handleDatasetRefresh(message);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Failed to handle event " + event, e);
        }
    }

    private PanelConfig createPanelConfigFromMessage(JSONObject message) throws JSONException {
        final JSONObject json = message.getJSONObject(JSON_KEY_PANEL);
        return new PanelConfig(json);
    }

     /**
     * Adds a new PanelConfig to the HomeConfig.
     *
     * This posts the invalidation of HomeConfig immediately.
     *
     * @param panelConfig panel to add
     */
    public void installPanel(PanelConfig panelConfig) {
        Log.d(LOGTAG, "installPanel: " + panelConfig.getTitle());
        handlePanelInstall(panelConfig, InvalidationMode.IMMEDIATE);
    }

    /**
     * Runs in the gecko thread.
     */
    private void handlePanelInstall(PanelConfig panelConfig, InvalidationMode mode) {
        mPendingChanges.offer(new ConfigChange(ChangeType.INSTALL, panelConfig));
        Log.d(LOGTAG, "handlePanelInstall: " + mPendingChanges.size());

        scheduleInvalidation(mode);
    }

    /**
     * Runs in the gecko thread.
     */
    private void handlePanelUninstall(String panelId) {
        mPendingChanges.offer(new ConfigChange(ChangeType.UNINSTALL, panelId));
        Log.d(LOGTAG, "handlePanelUninstall: " + mPendingChanges.size());

        scheduleInvalidation(InvalidationMode.DELAYED);
    }

    /**
     * Runs in the gecko thread.
     */
    private void handlePanelUpdate(PanelConfig panelConfig) {
        mPendingChanges.offer(new ConfigChange(ChangeType.UPDATE, panelConfig));
        Log.d(LOGTAG, "handlePanelUpdate: " + mPendingChanges.size());

        scheduleInvalidation(InvalidationMode.DELAYED);
    }

    /**
     * Runs in the background thread.
     */
    private void handleLocaleChange() {
        mPendingChanges.offer(new ConfigChange(ChangeType.REFRESH));
        Log.d(LOGTAG, "handleLocaleChange: " + mPendingChanges.size());

        scheduleInvalidation(InvalidationMode.IMMEDIATE);
    }


    /**
     * Handles a dataset refresh request from Gecko. This is usually
     * triggered by a HomeStorage.save() call in an add-on.
     *
     * Runs in the gecko thread.
     */
    private void handleDatasetRefresh(JSONObject message) {
        final String datasetId;
        try {
            datasetId = message.getString("datasetId");
        } catch (JSONException e) {
            Log.e(LOGTAG, "Failed to handle dataset refresh", e);
            return;
        }

        Log.d(LOGTAG, "Refresh request for dataset: " + datasetId);

        final ContentResolver cr = mContext.getContentResolver();
        cr.notifyChange(HomeProvider.getDatasetNotificationUri(datasetId), null);
    }

    /**
     * Runs in the gecko or main thread.
     */
    private void scheduleInvalidation(InvalidationMode mode) {
        final Handler handler = ThreadUtils.getBackgroundHandler();

        handler.removeCallbacks(mInvalidationRunnable);

        if (mode == InvalidationMode.IMMEDIATE) {
            handler.post(mInvalidationRunnable);
        } else {
            handler.postDelayed(mInvalidationRunnable, INVALIDATION_DELAY_MSEC);
        }

        Log.d(LOGTAG, "scheduleInvalidation: scheduled new invalidation: " + mode);
    }

    /**
     * Runs in the background thread.
     */
    private void executePendingChanges(HomeConfig.Editor editor) {
        boolean shouldRefresh = false;

        while (!mPendingChanges.isEmpty()) {
            final ConfigChange pendingChange = mPendingChanges.poll();

            switch (pendingChange.type) {
                case UNINSTALL: {
                    final String panelId = (String) pendingChange.target;
                    if (editor.uninstall(panelId)) {
                        Log.d(LOGTAG, "executePendingChanges: uninstalled panel " + panelId);
                    }
                    break;
                }

                case INSTALL: {
                    final PanelConfig panelConfig = (PanelConfig) pendingChange.target;
                    if (editor.install(panelConfig)) {
                        Log.d(LOGTAG, "executePendingChanges: added panel " + panelConfig.getId());
                    }
                    break;
                }

                case UPDATE: {
                    final PanelConfig panelConfig = (PanelConfig) pendingChange.target;
                    if (editor.update(panelConfig)) {
                        Log.w(LOGTAG, "executePendingChanges: updated panel " + panelConfig.getId());
                    }
                    break;
                }

                case REFRESH: {
                    shouldRefresh = true;
                }
            }
        }

        // The editor still represents the default HomeConfig
        // configuration and hasn't been changed by any operation
        // above. No need to refresh as the HomeConfig backend will
        // take of forcing all existing HomeConfigLoader instances to
        // refresh their contents.
        if (shouldRefresh && !editor.isDefault()) {
            executeRefresh(editor);
        }
    }

    /**
     * Runs in the background thread.
     */
    private void refreshFromPanelInfos(HomeConfig.Editor editor, List<PanelInfo> panelInfos) {
        Log.d(LOGTAG, "refreshFromPanelInfos");

        for (PanelConfig panelConfig : editor) {
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
                continue;
            }

            Log.d(LOGTAG, "refreshFromPanelInfos: refreshed panel " + refreshedPanelConfig.getId());
            editor.update(refreshedPanelConfig);
        }
    }

    /**
     * Runs in the background thread.
     */
    private void executeRefresh(HomeConfig.Editor editor) {
        if (editor.isEmpty()) {
            return;
        }

        Log.d(LOGTAG, "executeRefresh");

        final Set<String> ids = new HashSet<String>();
        for (PanelConfig panelConfig : editor) {
            ids.add(panelConfig.getId());
        }

        final Object panelRequestLock = new Object();
        final List<PanelInfo> latestPanelInfos = new ArrayList<PanelInfo>();

        final PanelInfoManager pm = new PanelInfoManager();
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
                refreshFromPanelInfos(editor, latestPanelInfos);
            }
        } catch (InterruptedException e) {
            Log.e(LOGTAG, "Failed to fetch panels from gecko", e);
        }
    }

    /**
     * Runs in the background thread.
     */
    private class InvalidationRunnable implements Runnable {
        @Override
        public void run() {
            final HomeConfig.Editor editor = mHomeConfig.load().edit();
            executePendingChanges(editor);
            editor.apply();
        }
    };
}
