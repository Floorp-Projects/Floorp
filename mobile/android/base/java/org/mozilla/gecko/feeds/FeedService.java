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
import org.mozilla.gecko.feeds.action.BaseAction;
import org.mozilla.gecko.feeds.action.CheckAction;
import org.mozilla.gecko.feeds.action.EnrollAction;
import org.mozilla.gecko.feeds.action.SetupAction;
import org.mozilla.gecko.feeds.action.SubscribeAction;
import org.mozilla.gecko.feeds.action.WithdrawAction;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.Experiments;

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
        intent.putExtra(SubscribeAction.EXTRA_FEED_URL, feedUrl);
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
        if (intent == null) {
            return;
        }

        Log.d(LOGTAG, "Service started with action: " + intent.getAction());

        try {
            if (!SwitchBoard.isInExperiment(this, Experiments.CONTENT_NOTIFICATIONS)) {
                Log.d(LOGTAG, "Not in content notifications experiment. Skipping.");
                return;
            }

            BaseAction action = createActionForIntent(intent);
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
    }

    @Nullable
    private BaseAction createActionForIntent(Intent intent) {
        final Context context = getApplicationContext();

        switch (intent.getAction()) {
            case ACTION_SETUP:
                return new SetupAction(context);

            case ACTION_SUBSCRIBE:
                return new SubscribeAction(context);

            case ACTION_CHECK:
                return new CheckAction(context);

            case ACTION_ENROLL:
                return new EnrollAction(context);

            case ACTION_WITHDRAW:
                return new WithdrawAction(context);

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

    private boolean isPreferenceEnabled() {
        return GeckoSharedPrefs.forApp(this).getBoolean(GeckoPreferences.PREFS_NOTIFICATIONS_CONTENT, true);
    }
}
