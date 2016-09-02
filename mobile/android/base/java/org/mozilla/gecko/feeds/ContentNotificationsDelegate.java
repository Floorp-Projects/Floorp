/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.NotificationManagerCompat;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.delegates.BrowserAppDelegate;
import org.mozilla.gecko.mozglue.SafeIntent;

import java.util.List;

/**
 * BrowserAppDelegate implementation that takes care of handling intents from content notifications.
 */
public class ContentNotificationsDelegate extends BrowserAppDelegate {
    // The application is opened from a content notification
    public static final String ACTION_CONTENT_NOTIFICATION = AppConstants.ANDROID_PACKAGE_NAME + ".action.CONTENT_NOTIFICATION";

    public static final String EXTRA_READ_BUTTON = "read_button";
    public static final String EXTRA_URLS = "urls";

    private static final String TELEMETRY_EXTRA_CONTENT_UPDATE = "content_update";
    private static final String TELEMETRY_EXTRA_READ_NOW_BUTTON = TELEMETRY_EXTRA_CONTENT_UPDATE + "_read_now";

    @Override
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            // This activity is getting restored: We do not want to handle the URLs in the Intent again. The browser
            // will take care of restoring the tabs we already created.
            return;
        }


        final Intent unsafeIntent = browserApp.getIntent();

        // Nothing to do.
        if (unsafeIntent == null) {
            return;
        }

        final SafeIntent intent = new SafeIntent(unsafeIntent);

        if (ACTION_CONTENT_NOTIFICATION.equals(intent.getAction())) {
            openURLsFromIntent(browserApp, intent);
        }
    }

    @Override
    public void onNewIntent(BrowserApp browserApp, @NonNull final SafeIntent intent) {
        if (ACTION_CONTENT_NOTIFICATION.equals(intent.getAction())) {
            openURLsFromIntent(browserApp, intent);
        }
    }

    private void openURLsFromIntent(BrowserApp browserApp, @NonNull final SafeIntent intent) {
        final List<String> urls = intent.getStringArrayListExtra(EXTRA_URLS);
        if (urls != null) {
            browserApp.openUrls(urls);
        }

        Telemetry.startUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(browserApp));

        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, TELEMETRY_EXTRA_CONTENT_UPDATE);

        if (intent.getBooleanExtra(EXTRA_READ_BUTTON, false)) {
            // "READ NOW" button in notification was clicked
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.NOTIFICATION, TELEMETRY_EXTRA_READ_NOW_BUTTON);

            // Android's "auto cancel" won't remove the notification when an action button is pressed. So we do it ourselves here.
            NotificationManagerCompat.from(browserApp).cancel(R.id.websiteContentNotification);
        } else {
            // Notification was clicked
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.NOTIFICATION, TELEMETRY_EXTRA_CONTENT_UPDATE);
        }

        Telemetry.stopUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(browserApp));
    }
}
