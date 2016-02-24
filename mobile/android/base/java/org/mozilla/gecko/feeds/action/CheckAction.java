/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.parser.Feed;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;
import org.mozilla.gecko.feeds.subscriptions.SubscriptionStorage;

import java.util.List;

/**
 * CheckAction: Check if feeds we subscribed to have new content available.
 */
public class CheckAction {
    private static final String LOGTAG = "FeedCheckAction";

    private Context context;
    private SubscriptionStorage storage;

    public CheckAction(Context context, SubscriptionStorage storage) {
        this.context = context;
        this.storage = storage;
    }

    public void perform() {
        final List<FeedSubscription> subscriptions = storage.getSubscriptions();

        Log.d(LOGTAG, "Checking feeds for updates (" + subscriptions.size() + " feeds) ..");

        for (FeedSubscription subscription : subscriptions) {
            Log.i(LOGTAG, "Checking feed: " + subscription.getFeedTitle());

            FeedFetcher.FeedResponse response = fetchFeed(subscription);
            if (response == null) {
                continue;
            }

            if (subscription.isNewer(response)) {
                Log.d(LOGTAG, "* Feed has changed. New item: " + response.feed.getLastItem().getTitle());

                storage.updateSubscription(subscription, response);

                notify(response.feed);
            }
        }
    }

    private void notify(Feed feed) {
        NotificationCompat.BigTextStyle style = new NotificationCompat.BigTextStyle()
                .bigText(feed.getLastItem().getTitle())
                .setBigContentTitle(feed.getTitle())
                .setSummaryText(feed.getLastItem().getURL());

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setComponent(new ComponentName(context, BrowserApp.class));
        intent.setData(Uri.parse(feed.getLastItem().getURL()));

        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, 0);

        Notification notification = new NotificationCompat.Builder(context)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(feed.getTitle())
                .setContentText(feed.getLastItem().getTitle())
                .setStyle(style)
                .setColor(ContextCompat.getColor(context, R.color.link_blue))
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .build();

        NotificationManagerCompat.from(context).notify(R.id.websiteContentNotification, notification);
    }

    private FeedFetcher.FeedResponse fetchFeed(FeedSubscription subscription) {
        return FeedFetcher.fetchAndParseFeedIfModified(
                subscription.getFeedUrl(),
                subscription.getETag(),
                subscription.getLastModified()
        );
    }
}
