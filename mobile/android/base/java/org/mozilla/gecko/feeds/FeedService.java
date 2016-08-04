/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.support.annotation.Nullable;
import android.support.v4.net.ConnectivityManagerCompat;
import android.util.Log;

import com.keepsafe.switchboard.SwitchBoard;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.feeds.action.FeedAction;
import org.mozilla.gecko.feeds.action.CheckForUpdatesAction;
import org.mozilla.gecko.feeds.action.EnrollSubscriptionsAction;
import org.mozilla.gecko.feeds.action.SetupAlarmsAction;
import org.mozilla.gecko.feeds.action.SubscribeToFeedAction;
import org.mozilla.gecko.feeds.action.WithdrawSubscriptionsAction;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.Experiments;

/**
 * Background service for subscribing to and checking website feeds to notify the user about updates.
 */
public class FeedService extends IntentService {
    private static final String LOGTAG = "GeckoFeedService";

    public static final String ACTION_SETUP = AppConstants.ANDROID_PACKAGE_NAME + ".FEEDS.SETUP";
    public static final String ACTION_SUBSCRIBE = AppConstants.ANDROID_PACKAGE_NAME + ".FEEDS.SUBSCRIBE";
    public static final String ACTION_CHECK = AppConstants.ANDROID_PACKAGE_NAME + ".FEEDS.CHECK";
    public static final String ACTION_ENROLL = AppConstants.ANDROID_PACKAGE_NAME + ".FEEDS.ENROLL";
    public static final String ACTION_WITHDRAW = AppConstants.ANDROID_PACKAGE_NAME + ".FEEDS.WITHDRAW";

    public static void setup(Context context) {
        Intent intent = new Intent(context, FeedService.class);
        intent.setAction(ACTION_SETUP);
        context.startService(intent);
    }

    public static void subscribe(Context context, String feedUrl) {
        Intent intent = new Intent(context, FeedService.class);
        intent.setAction(ACTION_SUBSCRIBE);
        intent.putExtra(SubscribeToFeedAction.EXTRA_FEED_URL, feedUrl);
        context.startService(intent);
    }

    public FeedService() {
        super(LOGTAG);
    }

    private BrowserDB browserDB;

    @Override
    public void onCreate() {
        super.onCreate();

        browserDB = GeckoProfile.get(this).getDB();
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        try {
            if (intent == null) {
                return;
            }

            Log.d(LOGTAG, "Service started with action: " + intent.getAction());

            if (!isInExperiment(this)) {
                Log.d(LOGTAG, "Not in content notifications experiment. Skipping.");
                return;
            }

            FeedAction action = createActionForIntent(intent);
            if (action == null) {
                Log.d(LOGTAG, "No action to process");
                return;
            }

            if (action.requiresPreferenceEnabled() && !isPreferenceEnabled()) {
                Log.d(LOGTAG, "Preference is disabled. Skipping.");
                return;
            }

            if (action.requiresNetwork() && !isConnectedToUnmeteredNetwork()) {
                // For now just skip if we are not connected or the network is metered. We do not want
                // to use precious mobile traffic.
                Log.d(LOGTAG, "Not connected to a network or network is metered. Skipping.");
                return;
            }

            action.perform(browserDB, intent);
        } finally {
            FeedAlarmReceiver.completeWakefulIntent(intent);
        }

        Log.d(LOGTAG, "Done.");
    }

    @Nullable
    private FeedAction createActionForIntent(Intent intent) {
        final Context context = getApplicationContext();

        switch (intent.getAction()) {
            case ACTION_SETUP:
                return new SetupAlarmsAction(context);

            case ACTION_SUBSCRIBE:
                return new SubscribeToFeedAction(context);

            case ACTION_CHECK:
                return new CheckForUpdatesAction(context);

            case ACTION_ENROLL:
                return new EnrollSubscriptionsAction(context);

            case ACTION_WITHDRAW:
                return new WithdrawSubscriptionsAction(context);

            default:
                throw new AssertionError("Unknown action: " + intent.getAction());
        }
    }

    private boolean isConnectedToUnmeteredNetwork() {
        ConnectivityManager manager = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo networkInfo = manager.getActiveNetworkInfo();
        if (networkInfo == null || !networkInfo.isConnected()) {
            return false;
        }

        return !ConnectivityManagerCompat.isActiveNetworkMetered(manager);
    }

    public static boolean isInExperiment(Context context) {
        return SwitchBoard.isInExperiment(context, Experiments.CONTENT_NOTIFICATIONS_12HRS) ||
               SwitchBoard.isInExperiment(context, Experiments.CONTENT_NOTIFICATIONS_5PM) ||
               SwitchBoard.isInExperiment(context, Experiments.CONTENT_NOTIFICATIONS_8AM);
    }

    public static String getEnabledExperiment(Context context) {
        String experiment = null;

        if (SwitchBoard.isInExperiment(context, Experiments.CONTENT_NOTIFICATIONS_12HRS)) {
            experiment = Experiments.CONTENT_NOTIFICATIONS_12HRS;
        } else if (SwitchBoard.isInExperiment(context, Experiments.CONTENT_NOTIFICATIONS_8AM)) {
            experiment = Experiments.CONTENT_NOTIFICATIONS_8AM;
        } else if (SwitchBoard.isInExperiment(context, Experiments.CONTENT_NOTIFICATIONS_5PM)) {
            experiment = Experiments.CONTENT_NOTIFICATIONS_5PM;
        }

        return experiment;
    }

    private boolean isPreferenceEnabled() {
        return GeckoSharedPrefs.forApp(this).getBoolean(GeckoPreferences.PREFS_NOTIFICATIONS_CONTENT, true);
    }
}
