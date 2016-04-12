/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.FeedService;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;

/**
 * SubscribeToFeedAction: Try to fetch a feed and create a subscription if successful.
 */
public class SubscribeToFeedAction extends FeedAction {
    private static final String LOGTAG = "FeedSubscribeAction";

    public static final String EXTRA_FEED_URL = "feed_url";

    private Context context;

    public SubscribeToFeedAction(Context context) {
        this.context = context;
    }

    @Override
    public void perform(BrowserDB browserDB, Intent intent) {
        final UrlAnnotations urlAnnotations = browserDB.getUrlAnnotations();

        final Bundle extras = intent.getExtras();
        final String feedUrl = extras.getString(EXTRA_FEED_URL);

        if (urlAnnotations.hasFeedSubscription(context.getContentResolver(), feedUrl)) {
            log("Already subscribed to " + feedUrl + ". Skipping.");
            return;
        }

        log("Subscribing to feed: " + feedUrl);

        subscribe(urlAnnotations, feedUrl);
    }

    @Override
    public boolean requiresNetwork() {
        return true;
    }

    @Override
    public boolean requiresPreferenceEnabled() {
        return true;
    }

    private void subscribe(UrlAnnotations urlAnnotations, String feedUrl) {
        FeedFetcher.FeedResponse response = FeedFetcher.fetchAndParseFeed(feedUrl);
        if (response == null) {
            log(String.format("Could not fetch feed (%s). Not subscribing for now.", feedUrl));
            return;
        }

        log("Subscribing to feed: " + response.feed.getTitle());
        log("          Last item: " + response.feed.getLastItem().getTitle());

        final FeedSubscription subscription = FeedSubscription.create(feedUrl, response);

        urlAnnotations.insertFeedSubscription(context.getContentResolver(), subscription);

        Telemetry.startUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
        Telemetry.sendUIEvent(TelemetryContract.Event.SAVE, TelemetryContract.Method.SERVICE, "content_update");
        Telemetry.stopUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
    }
}
