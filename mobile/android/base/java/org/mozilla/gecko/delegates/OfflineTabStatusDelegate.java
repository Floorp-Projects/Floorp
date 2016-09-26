/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.delegates;

import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.support.design.widget.Snackbar;
import android.support.v4.content.ContextCompat;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarBuilder;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.WeakHashMap;

/**
 * Displays "Showing offline version" message when tabs are loaded from cache while offline.
 */
public class OfflineTabStatusDelegate extends TabsTrayVisibilityAwareDelegate implements Tabs.OnTabsChangedListener {
    private WeakReference<Activity> activityReference;
    private WeakHashMap<Tab, Void> tabsQueuedForOfflineSnackbar = new WeakHashMap<>();

    @CallSuper
    @Override
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        super.onCreate(browserApp, savedInstanceState);
        activityReference = new WeakReference<Activity>(browserApp);
    }

    @Override
    public void onResume(BrowserApp browserApp) {
        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    public void onPause(BrowserApp browserApp) {
        Tabs.unregisterOnTabsChangedListener(this);
    }

    public void onTabChanged(final Tab tab, Tabs.TabEvents event, String data) {
        if (tab == null) {
            return;
        }

        // Ignore tabs loaded regularly.
        if (!tab.hasLoadedFromCache()) {
            return;
        }

        // Ignore tabs displaying about pages
        if (AboutPages.isAboutPage(tab.getURL())) {
            return;
        }

        // We only want to show these notifications for tabs that were loaded successfully.
        if (tab.getState() != Tab.STATE_SUCCESS) {
            return;
        }

        switch (event) {
            // We listen specifically for the STOP event (as opposed to PAGE_SHOW), because we need
            // to know if page load actually succeeded. When STOP is triggered, tab.getState()
            // will return definitive STATE_SUCCESS or STATE_ERROR. When PAGE_SHOW is triggered,
            // tab.getState() will return STATE_LOADING, which is ambiguous for our purposes.
            // We don't want to show these notifications for 404 pages, for example. See Bug 1304914.
            case STOP:
                // Show offline notification if tab is visible, or queue it for display later.
                if (!isTabsTrayVisible() && Tabs.getInstance().isSelectedTab(tab)) {
                    showLoadedOfflineSnackbar(activityReference.get());
                } else {
                    tabsQueuedForOfflineSnackbar.put(tab, null);
                }
                break;
            // Fallthrough; see Bug 1278980 for details on why this event is here.
            case OPENED_FROM_TABS_TRAY:
            // When tab is selected and offline notification was queued, display it if possible.
            // SELECTED event might also fire when we're on a TabStrip, so check first.
            case SELECTED:
                if (isTabsTrayVisible()) {
                    break;
                }
                if (tabsQueuedForOfflineSnackbar.containsKey(tab)) {
                    showLoadedOfflineSnackbar(activityReference.get());
                    tabsQueuedForOfflineSnackbar.remove(tab);
                }
                break;
        }
    }

    /**
     * Displays the notification snackbar and logs a telemetry event.
     *
     * @param activity which will be used for displaying the snackbar.
     */
    private static void showLoadedOfflineSnackbar(final Activity activity) {
        if (activity == null) {
            return;
        }

        Telemetry.sendUIEvent(TelemetryContract.Event.NETERROR, TelemetryContract.Method.TOAST, "usecache");

        SnackbarBuilder.builder(activity)
                .message(R.string.tab_offline_version)
                .duration(Snackbar.LENGTH_INDEFINITE)
                .backgroundColor(ContextCompat.getColor(activity, R.color.link_blue))
                .buildAndShow();
    }
}
