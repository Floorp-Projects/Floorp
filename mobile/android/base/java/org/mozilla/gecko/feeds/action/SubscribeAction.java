/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;
import org.mozilla.gecko.feeds.subscriptions.SubscriptionStorage;

import java.util.UUID;

/**
 * SubscribeAction: Try to fetch a feed and create a subscription if successful.
 */
public class SubscribeAction {
    private static final String LOGTAG = "FeedSubscribeAction";

    public static final String EXTRA_GUID = "guid";
    public static final String EXTRA_FEED_URL = "feed_url";

    private SubscriptionStorage storage;

    public SubscribeAction(SubscriptionStorage storage) {
        this.storage = storage;
    }

    public void perform(Intent intent) {
        Log.d(LOGTAG, "Subscribing to feed..");

        final Bundle extras = intent.getExtras();

        // TODO: Using a random UUID as fallback just so that I can subscribe for things that are not bookmarks (testing)
        final String guid = extras.getString(EXTRA_GUID, UUID.randomUUID().toString());
        final String feedUrl = intent.getStringExtra(EXTRA_FEED_URL);

        if (storage.hasSubscriptionForBookmark(guid)) {
            Log.d(LOGTAG, "Already subscribed to " + feedUrl + ". Skipping.");
            return;
        }

        subscribe(guid, feedUrl);
    }

    private void subscribe(String guid, String feedUrl) {
        FeedFetcher.FeedResponse response = FeedFetcher.fetchAndParseFeed(feedUrl);
        if (response == null) {
            Log.w(LOGTAG, String.format("Could not fetch feed (%s). Not subscribing for now.", feedUrl));
            return;
        }

        Log.d(LOGTAG, "Subscribing to feed: " + response.feed.getTitle());
        Log.d(LOGTAG, "               GUID: " + guid);
        Log.d(LOGTAG, "          Last item: " + response.feed.getLastItem().getTitle());

        storage.addSubscription(FeedSubscription.create(guid, feedUrl, response));
    }
}
