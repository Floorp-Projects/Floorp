/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.content.ContextCompat;
import android.text.format.DateFormat;

import org.json.JSONException;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.parser.Feed;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.StringUtils;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * CheckForUpdatesAction: Check if feeds we subscribed to have new content available.
 */
public class CheckForUpdatesAction extends FeedAction {
    /**
     * This extra will be added to Intents fired by the notification.
     */
    public static final String EXTRA_CONTENT_NOTIFICATION = "content-notification";

    private static final String LOGTAG = "FeedCheckAction";

    private Context context;

    public CheckForUpdatesAction(Context context) {
        this.context = context;
    }

    @Override
    public void perform(BrowserDB browserDB, Intent intent) {
        final UrlAnnotations urlAnnotations = browserDB.getUrlAnnotations();
        final ContentResolver resolver = context.getContentResolver();
        final List<Feed> updatedFeeds = new ArrayList<>();

        log("Checking feeds for updates..");

        Cursor cursor = urlAnnotations.getFeedSubscriptions(resolver);
        if (cursor == null) {
            return;
        }

        try {
            while (cursor.moveToNext()) {
                FeedSubscription subscription = FeedSubscription.fromCursor(cursor);

                FeedFetcher.FeedResponse response = checkFeedForUpdates(subscription);
                if (response != null) {
                    updatedFeeds.add(response.feed);

                    urlAnnotations.updateFeedSubscription(resolver, subscription);
                }
            }
        } catch (JSONException e) {
            log("Could not deserialize subscription", e);
        } finally {
            cursor.close();
        }

        showNotification(updatedFeeds);
    }

    private FeedFetcher.FeedResponse checkFeedForUpdates(FeedSubscription subscription) {
        log("Checking feed: " + subscription.getFeedTitle());

        FeedFetcher.FeedResponse response = fetchFeed(subscription);
        if (response == null) {
            return null;
        }

        if (subscription.hasBeenUpdated(response)) {
            log("* Feed has changed. New item: " + response.feed.getLastItem().getTitle());

            subscription.update(response);

            return response;

        }

        return null;
    }

    private void showNotification(List<Feed> updatedFeeds) {
        final int feedCount = updatedFeeds.size();
        if (feedCount == 0) {
            return;
        }

        if (feedCount == 1) {
            showNotificationForSingleUpdate(updatedFeeds.get(0));
        } else {
            showNotificationForMultipleUpdates(updatedFeeds);
        }

        Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.NOTIFICATION, "content_update");
    }

    private void showNotificationForSingleUpdate(Feed feed) {
        final String date = DateFormat.getMediumDateFormat(context).format(new Date(feed.getLastItem().getTimestamp()));

        NotificationCompat.BigTextStyle style = new NotificationCompat.BigTextStyle()
                .bigText(feed.getLastItem().getTitle())
                .setBigContentTitle(feed.getTitle())
                .setSummaryText(context.getString(R.string.content_notification_updated_on, date));

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setComponent(new ComponentName(context, BrowserApp.class));
        intent.setData(Uri.parse(feed.getLastItem().getURL()));
        intent.putExtra(EXTRA_CONTENT_NOTIFICATION, true);

        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, 0);

        Notification notification = new NotificationCompat.Builder(context)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(feed.getTitle())
                .setContentText(feed.getLastItem().getTitle())
                .setStyle(style)
                .setColor(ContextCompat.getColor(context, R.color.fennec_ui_orange))
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .addAction(createNotificationSettingsAction())
                .build();

        NotificationManagerCompat.from(context).notify(R.id.websiteContentNotification, notification);
    }

    private void showNotificationForMultipleUpdates(List<Feed> feeds) {
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
        intent.putExtra(EXTRA_CONTENT_NOTIFICATION, true);

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
                .addAction(createNotificationSettingsAction())
                .build();

        NotificationManagerCompat.from(context).notify(R.id.websiteContentNotification, notification);
    }

    private NotificationCompat.Action createNotificationSettingsAction() {
        final Intent intent = new Intent(GeckoApp.ACTION_LAUNCH_SETTINGS);
        intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        intent.putExtra(EXTRA_CONTENT_NOTIFICATION, true);

        GeckoPreferences.setResourceToOpen(intent, "preferences_notifications");

        PendingIntent settingsIntent = PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        return new NotificationCompat.Action(
                R.drawable.firefox_settings_alert,
                context.getString(R.string.content_notification_action_settings),
                settingsIntent);
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

    @Override
    public boolean requiresPreferenceEnabled() {
        return true;
    }
}
