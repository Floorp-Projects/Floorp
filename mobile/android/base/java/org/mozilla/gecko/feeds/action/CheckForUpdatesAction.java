/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
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
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.feeds.ContentNotificationsDelegate;
import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.FeedService;
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

    private final Context context;

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
                    final Feed feed = response.feed;

                    if (!hasBeenVisited(browserDB, feed.getLastItem().getURL())) {
                        // Only notify about this update if the last item hasn't been visited yet.
                        updatedFeeds.add(feed);
                    } else {
                        Telemetry.startUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
                        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL,
                                TelemetryContract.Method.SERVICE,
                                "content_update");
                        Telemetry.stopUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
                    }

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

    /**
     * Returns true if this URL has been visited before.
     *
     * We do an exact match. So this can fail if the feed uses a different URL and redirects to
     * content. But it's better than no checks at all.
     */
    private boolean hasBeenVisited(final BrowserDB browserDB, final String url) {
        final Cursor cursor = browserDB.getHistoryForURL(context.getContentResolver(), url);
        if (cursor == null) {
            return false;
        }

        try {
            if (cursor.moveToFirst()) {
                return cursor.getInt(cursor.getColumnIndex(BrowserContract.History.VISITS)) > 0;
            }
        } finally {
            cursor.close();
        }

        return false;
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

        Telemetry.startUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
        Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.NOTIFICATION, "content_update");
        Telemetry.stopUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(context));
    }

    private void showNotificationForSingleUpdate(Feed feed) {
        final String date = DateFormat.getMediumDateFormat(context).format(new Date(feed.getLastItem().getTimestamp()));

        final NotificationCompat.BigTextStyle style = new NotificationCompat.BigTextStyle()
                .bigText(feed.getLastItem().getTitle())
                .setBigContentTitle(feed.getTitle())
                .setSummaryText(context.getString(R.string.content_notification_updated_on, date));

        final PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, createOpenIntent(feed), PendingIntent.FLAG_UPDATE_CURRENT);

        final Notification notification = new NotificationCompat.Builder(context)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(feed.getTitle())
                .setContentText(feed.getLastItem().getTitle())
                .setStyle(style)
                .setColor(ContextCompat.getColor(context, R.color.fennec_ui_orange))
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .addAction(createOpenAction(feed))
                .addAction(createNotificationSettingsAction())
                .build();

        NotificationManagerCompat.from(context).notify(R.id.websiteContentNotification, notification);
    }

    private void showNotificationForMultipleUpdates(List<Feed> feeds) {
        final NotificationCompat.InboxStyle inboxStyle = new NotificationCompat.InboxStyle();
        for (Feed feed : feeds) {
            inboxStyle.addLine(StringUtils.stripScheme(feed.getLastItem().getURL(), StringUtils.UrlFlags.STRIP_HTTPS));
        }
        inboxStyle.setSummaryText(context.getString(R.string.content_notification_summary));

        final PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, createOpenIntent(feeds), PendingIntent.FLAG_UPDATE_CURRENT);

        Notification notification = new NotificationCompat.Builder(context)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(context.getString(R.string.content_notification_title_plural, feeds.size()))
                .setContentText(context.getString(R.string.content_notification_summary))
                .setStyle(inboxStyle)
                .setColor(ContextCompat.getColor(context, R.color.fennec_ui_orange))
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .addAction(createOpenAction(feeds))
                .setNumber(feeds.size())
                .addAction(createNotificationSettingsAction())
                .build();

        NotificationManagerCompat.from(context).notify(R.id.websiteContentNotification, notification);
    }

    private Intent createOpenIntent(Feed feed) {
        final List<Feed> feeds = new ArrayList<>();
        feeds.add(feed);

        return createOpenIntent(feeds);
    }

    private Intent createOpenIntent(List<Feed> feeds) {
        final ArrayList<String> urls = new ArrayList<>();
        for (Feed feed : feeds) {
            urls.add(feed.getLastItem().getURL());
        }

        final Intent intent = new Intent(context, BrowserApp.class);
        intent.setAction(ContentNotificationsDelegate.ACTION_CONTENT_NOTIFICATION);
        intent.putStringArrayListExtra(ContentNotificationsDelegate.EXTRA_URLS, urls);

        return intent;
    }

    private NotificationCompat.Action createOpenAction(Feed feed) {
        final List<Feed> feeds = new ArrayList<>();
        feeds.add(feed);

        return createOpenAction(feeds);
    }

    private NotificationCompat.Action createOpenAction(List<Feed> feeds) {
        Intent intent = createOpenIntent(feeds);
        intent.putExtra(ContentNotificationsDelegate.EXTRA_READ_BUTTON, true);

        PendingIntent pendingIntent = PendingIntent.getActivity(context, 1, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        return new NotificationCompat.Action(
                R.drawable.open_in_browser,
                context.getString(R.string.content_notification_action_read_now),
                pendingIntent);
    }

    private NotificationCompat.Action createNotificationSettingsAction() {
        final Intent intent = new Intent(GeckoApp.ACTION_LAUNCH_SETTINGS);
        intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        intent.putExtra(EXTRA_CONTENT_NOTIFICATION, true);

        GeckoPreferences.setResourceToOpen(intent, "preferences_notifications");

        PendingIntent settingsIntent = PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        return new NotificationCompat.Action(
                R.drawable.settings_notifications,
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
