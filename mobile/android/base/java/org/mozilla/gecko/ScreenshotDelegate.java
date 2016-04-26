/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.os.Bundle;
import android.support.design.widget.Snackbar;
import android.util.Log;

import java.lang.ref.WeakReference;

/**
 * Delegate for observing screenshots being taken.
 */
public class ScreenshotDelegate extends BrowserAppDelegate implements ScreenshotObserver.OnScreenshotListener {
    private static final String LOGTAG = "GeckoScreenshotDelegate";

    private WeakReference<Activity> activityReference;
    private final ScreenshotObserver mScreenshotObserver = new ScreenshotObserver();

    @Override
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        activityReference = new WeakReference<Activity>(browserApp);

        mScreenshotObserver.setListener(browserApp, this);
    }

    @Override
    public void onScreenshotTaken(String screenshotPath, String title) {
        // Treat screenshots as a sharing method.
        Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.BUTTON, "screenshot");

        if (!AppConstants.SCREENSHOTS_IN_BOOKMARKS_ENABLED) {
            return;
        }

        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab == null) {
            Log.w(LOGTAG, "Selected tab is null: could not page info to store screenshot.");
            return;
        }

        final Activity activity = activityReference.get();
        if (activity == null) {
            return;
        }

        GeckoProfile.get(activity).getDB().getUrlAnnotations().insertScreenshot(
                activity.getContentResolver(), selectedTab.getURL(), screenshotPath);

        SnackbarHelper.showSnackbar(activity,
                activity.getResources().getString(R.string.screenshot_added_to_bookmarks), Snackbar.LENGTH_SHORT);
    }

    @Override
    public void onResume(BrowserApp browserApp) {
        mScreenshotObserver.start();
    }

    @Override
    public void onPause(BrowserApp browserApp) {
        mScreenshotObserver.stop();
    }
}
