/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.focus.utils.SafeIntent;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/**
 * A global object keeping the state of the current browsing session.
 *
 * For now it only tracks whether a browsing session is active or not.
 */
public class BrowsingSession {
    private static BrowsingSession instance;

    public interface TrackingCountListener {
        void onTrackingCountChanged(int trackingCount);
    }

    public static synchronized BrowsingSession getInstance() {
        if (instance == null) {
            instance = new BrowsingSession();
        }
        return instance;
    }

    private boolean isActive;
    private int blockedTrackers;
    private WeakReference<TrackingCountListener> listenerWeakReference;
    private @Nullable CustomTabConfig customTabConfig;

    private Map<String, Bundle> webViewStates;

    private BrowsingSession() {
        listenerWeakReference = new WeakReference<>(null);
        webViewStates = new HashMap<>();
    }

    public void start() {
        isActive = true;
    }

    public void stop() {
        isActive = false;
        customTabConfig = null;
        webViewStates.clear();
    }

    public boolean isActive() {
        return isActive;
    }

    public void countBlockedTracker() {
        blockedTrackers++;

        final TrackingCountListener listener = listenerWeakReference.get();
        if (listener != null) {
            listener.onTrackingCountChanged(blockedTrackers);
        }
    }

    public void setTrackingCountListener(TrackingCountListener listener) {
        listenerWeakReference = new WeakReference<>(listener);
        listener.onTrackingCountChanged(blockedTrackers);
    }

    public void resetTrackerCount() {
        blockedTrackers = 0;
    }

    public void loadCustomTabConfig(final @NonNull Context context, final @NonNull SafeIntent intent) {
        if (!CustomTabConfig.isCustomTabIntent(intent)) {
            customTabConfig = null;
            return;
        }

        customTabConfig = CustomTabConfig.parseCustomTabIntent(context, intent);
    }

    /**
     * Keep a reference to this WebView state (saved in Bundle) linked to the given UUID so that
     * we can restore it later.
     */
    public void putWebViewState(@NonNull String uuid, @NonNull Bundle bundle) {
        webViewStates.put(uuid, bundle);
    }

    /**
     * Get the WebView state saved linked to the given UUID or null if no such state exists.
     */
    @Nullable
    public Bundle getWebViewState(@Nullable String uuid) {
        return uuid != null ? webViewStates.get(uuid) : null;
    }

    /**
     * Do we have a WebView state linked to the given UUID?
     */
    public boolean hasWebViewState(@Nullable String uuid) {
        return uuid != null && webViewStates.containsKey(uuid);

    }

    public boolean isCustomTab() {
        return customTabConfig != null;
    }

    public @NonNull CustomTabConfig getCustomTabConfig() {
        if (!isCustomTab()) {
            throw new IllegalStateException("Can't retrieve custom tab config for normal browsing session");
        }

        return customTabConfig;
    }
}
