/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import com.keepsafe.switchboard.SwitchBoard;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.feeds.action.CheckAction;
import org.mozilla.gecko.feeds.action.EnrollAction;
import org.mozilla.gecko.feeds.action.SetupAction;
import org.mozilla.gecko.feeds.action.SubscribeAction;
import org.mozilla.gecko.feeds.action.WithdrawAction;
import org.mozilla.gecko.feeds.subscriptions.SubscriptionStorage;
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

    public static void subscribe(Context context, String guid, String feedUrl) {
        Intent intent = new Intent(context, FeedService.class);
        intent.setAction(ACTION_SUBSCRIBE);
        intent.putExtra(SubscribeAction.EXTRA_GUID, guid);
        intent.putExtra(SubscribeAction.EXTRA_FEED_URL, feedUrl);
        context.startService(intent);
    }

    private SubscriptionStorage storage;

    public FeedService() {
        super(LOGTAG);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        storage = new SubscriptionStorage(getApplicationContext());
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (intent == null) {
            return;
        }

        if (!SwitchBoard.isInExperiment(this, Experiments.CONTENT_NOTIFICATIONS)) {
            Log.d(LOGTAG, "Not in content notifications experiment. Skipping.");
            return;
        }

        switch (intent.getAction()) {
            case ACTION_SETUP:
                new SetupAction(this).perform();
                break;

            case ACTION_SUBSCRIBE:
                new SubscribeAction(storage).perform(intent);
                break;

            case ACTION_CHECK:
                new CheckAction(this, storage).perform();
                break;

            case ACTION_ENROLL:
                new EnrollAction(this).perform();
                break;

            case ACTION_WITHDRAW:
                new WithdrawAction(this, storage).perform();
                break;

            default:
                Log.e(LOGTAG, "Unknown action: " + intent.getAction());
        }

        storage.persistChanges();

        FeedAlarmReceiver.completeWakefulIntent(intent);
    }
}
