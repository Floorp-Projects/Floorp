/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;

import org.json.JSONException;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.feeds.FeedService;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;

/**
 * WithdrawSubscriptionsAction: Look for feeds to unsubscribe from.
 */
public class WithdrawSubscriptionsAction extends FeedAction {
    private static final String LOGTAG = "FeedWithdrawAction";

    private Context context;

    public WithdrawSubscriptionsAction(Context context) {
        this.context = context;
    }

    @Override
    public void perform(BrowserDB browserDB, Intent intent) {
        log("Searching for subscriptions to remove..");

        final UrlAnnotations urlAnnotations = browserDB.getUrlAnnotations();
        final ContentResolver resolver = context.getContentResolver();

        removeFeedsOfUnknownUrls(browserDB, urlAnnotations, resolver);
        removeSubscriptionsOfRemovedFeeds(urlAnnotations, resolver);
    }

    /**
     * Search for website URLs with a feed assigned. Remove entry if website URL is not known anymore:
     * For now this means the website is not bookmarked.
     */
    private void removeFeedsOfUnknownUrls(BrowserDB browserDB, UrlAnnotations urlAnnotations, ContentResolver resolver) {
        Cursor cursor = urlAnnotations.getWebsitesWithFeedUrl(resolver);
        if (cursor == null) {
            return;
        }

        try {
            while (cursor.moveToNext()) {
                final String url = cursor.getString(cursor.getColumnIndex(BrowserContract.UrlAnnotations.URL));

                if (!browserDB.isBookmark(resolver, url)) {
                    log("Removing feed for unknown URL: " + url);

                    urlAnnotations.deleteFeedUrl(resolver, url);
                }
            }
        } finally {
            cursor.close();
        }
    }

    /**
     * Remove subscriptions of feed URLs that are not assigned to a website URL (anymore).
     */
    private void removeSubscriptionsOfRemovedFeeds(UrlAnnotations urlAnnotations, ContentResolver resolver) {
        Cursor cursor = urlAnnotations.getFeedSubscriptions(resolver);
        if (cursor == null) {
            return;
        }

        try {
            while (cursor.moveToNext()) {
                final FeedSubscription subscription = FeedSubscription.fromCursor(cursor);

                if (!urlAnnotations.hasWebsiteForFeedUrl(resolver, subscription.getFeedUrl())) {
                    log("Removing subscription for feed: " + subscription.getFeedUrl());

                    urlAnnotations.deleteFeedSubscription(resolver, subscription);

                    Telemetry.startUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
                    Telemetry.sendUIEvent(TelemetryContract.Event.UNSAVE, TelemetryContract.Method.SERVICE, "content_update");
                    Telemetry.stopUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
                }
            }
        } catch (JSONException e) {
            log("Could not deserialize subscription", e);
        } finally {
            cursor.close();
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
}
