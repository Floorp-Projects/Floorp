/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;
import org.mozilla.gecko.feeds.subscriptions.SubscriptionStorage;

import java.util.List;

/**
 * WithdrawAction: Look for feeds to unsubscribe from.
 */
public class WithdrawAction implements BaseAction {
    private static final String LOGTAG = "FeedWithdrawAction";

    private Context context;
    private SubscriptionStorage storage;

    public WithdrawAction(Context context, SubscriptionStorage storage) {
        this.context = context;
        this.storage = storage;
    }

    @Override
    public void perform(Intent intent) {
        BrowserDB db = GeckoProfile.get(context).getDB();

        List<FeedSubscription> subscriptions = storage.getSubscriptions();

        Log.d(LOGTAG, "Checking " + subscriptions.size() + " subscriptions");

        for (FeedSubscription subscription : subscriptions) {
            if (!db.hasBookmarkWithGuid(context.getContentResolver(), subscription.getBookmarkGUID())) {
                unsubscribe(subscription);
            }
        }
    }

    @Override
    public boolean requiresNetwork() {
        return false;
    }

    @Override
    public boolean requiresPreferenceEnabled() {
        return true;
    }

    private void unsubscribe(FeedSubscription subscription) {
        Log.d(LOGTAG, "Unsubscribing from: (" + subscription.getBookmarkGUID() + ") " + subscription.getFeedUrl());

        storage.removeSubscription(subscription);
    }
}
