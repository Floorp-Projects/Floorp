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
import android.text.format.DateFormat;
import android.util.Log;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.parser.Feed;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;
import org.mozilla.gecko.feeds.subscriptions.SubscriptionStorage;
import org.mozilla.gecko.util.StringUtils;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * CheckAction: Check if feeds we subscribed to have new content available.
 */
public class CheckAction implements BaseAction {
    private static final String LOGTAG = "FeedCheckAction";

    private Context context;
    private SubscriptionStorage storage;

    public CheckAction(Context context, SubscriptionStorage storage) {
        this.context = context;
        this.storage = storage;
    }

    @Override
    public void perform(Intent intent) {
        final List<FeedSubscription> subscriptions = storage.getSubscriptions();

        Log.d(LOGTAG, "Checking feeds for updates (" + subscriptions.size() + " feeds) ..");

        List<Feed> updatedFeeds = new ArrayList<>();

        for (FeedSubscription subscription : subscriptions) {
            Log.i(LOGTAG, "Checking feed: " + subscription.getFeedTitle());

            FeedFetcher.FeedResponse response = fetchFeed(subscription);
            if (response == null) {
                continue;
            }

            if (subscription.isNewer(response)) {
                Log.d(LOGTAG, "* Feed has changed. New item: " + response.feed.getLastItem().getTitle());

                storage.updateSubscription(subscription, response);

                updatedFeeds.add(response.feed);
            }
        }

        notify(updatedFeeds);
    }

    private void notify(List<Feed> updatedFeeds) {
        final int feedCount = updatedFeeds.size();

        if (feedCount == 1) {
            notifySingle(updatedFeeds.get(0));
        } else if (feedCount > 1) {
            notifyMultiple(updatedFeeds);
        }
    }

    private void notifySingle(Feed feed) {
        final String date = DateFormat.getMediumDateFormat(context).format(new Date(feed.getLastItem().getTimestamp()));

        NotificationCompat.BigTextStyle style = new NotificationCompat.BigTextStyle()
                .bigText(feed.getLastItem().getTitle())
                .setBigContentTitle(feed.getTitle())
                .setSummaryText(context.getString(R.string.content_notification_updated_on, date));

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setComponent(new ComponentName(context, BrowserApp.class));
        intent.setData(Uri.parse(feed.getLastItem().getURL()));

        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, 0);

        Notification notification = new NotificationCompat.Builder(context)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(feed.getTitle())
                .setContentText(feed.getLastItem().getTitle())
                .setStyle(style)
                .setColor(ContextCompat.getColor(context, R.color.fennec_ui_orange))
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .build();

        NotificationManagerCompat.from(context).notify(R.id.websiteContentNotification, notification);
    }

    private void notifyMultiple(List<Feed> feeds) {
        final ArrayList<String> urls = new ArrayList<>();

        final NotificationCompat.InboxStyle inboxStyle = new NotificationCompat.InboxStyle();
        for (Feed feed : feeds) {
            final String url = feed.getLastItem().getURL();

            inboxStyle.addLine(StringUtils.stripScheme(url, StringUtils.UrlFlags.STRIP_HTTPS));
            urls.add(url);
        }
        inboxStyle.setSummaryText(context.getString(R.string.content_notification_summary));

        Intent intent = new Intent(context, BrowserApp.class);
        intent.setAction(BrowserApp.ACTION_VIEW_MULTIPLE);
        intent.putStringArrayListExtra("urls", urls);
        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, 0);

        Notification notification = new NotificationCompat.Builder(context)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(context.getString(R.string.content_notification_title_plural, feeds.size()))
                .setContentText(context.getString(R.string.content_notification_summary))
                .setStyle(inboxStyle)
                .setColor(ContextCompat.getColor(context, R.color.fennec_ui_orange))
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .setNumber(feeds.size())
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

    @Override
    public boolean requiresNetwork() {
        return true;
    }
}
