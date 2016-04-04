/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import org.json.JSONException;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserContract.UrlAnnotations.Key;
import org.mozilla.gecko.feeds.subscriptions.FeedSubscription;

public class LocalUrlAnnotations implements UrlAnnotations {
    private static final String LOGTAG = "LocalUrlAnnotations";

    private Uri urlAnnotationsTableWithProfile;

    public LocalUrlAnnotations(final String profile) {
        urlAnnotationsTableWithProfile = DBUtils.appendProfile(profile, BrowserContract.UrlAnnotations.CONTENT_URI);
    }

    /**
     * Get all feed subscriptions.
     */
    @Override
    public Cursor getFeedSubscriptions(ContentResolver cr) {
        return queryByKey(cr,
                Key.FEED_SUBSCRIPTION,
                new String[] { BrowserContract.UrlAnnotations.URL, BrowserContract.UrlAnnotations.VALUE },
                null);
    }

    /**
     * Insert mapping from website URL to URL of the feed.
     */
    @Override
    public void insertFeedUrl(ContentResolver cr, String originUrl, String feedUrl) {
        insertAnnotation(cr, originUrl, Key.FEED, feedUrl);
    }

    /**
     * Returns true if there's a mapping from the given website URL to a feed URL. False otherwise.
     */
    @Override
    public boolean hasFeedUrlForWebsite(ContentResolver cr, String websiteUrl) {
        return hasResultsForSelection(cr,
                BrowserContract.UrlAnnotations.URL + " = ? AND " + BrowserContract.UrlAnnotations.KEY + " = ?",
                new String[]{websiteUrl, Key.FEED.getDbValue()});
    }

    /**
     * Returns true if there's a website URL with this feed URL. False otherwise.
     */
    @Override
    public boolean hasWebsiteForFeedUrl(ContentResolver cr, String feedUrl) {
        return hasResultsForSelection(cr,
                BrowserContract.UrlAnnotations.VALUE + " = ? AND " + BrowserContract.UrlAnnotations.KEY + " = ?",
                new String[]{feedUrl, Key.FEED.getDbValue()});
    }

    /**
     * Delete the feed URL mapping for this website URL.
     */
    @Override
    public void deleteFeedUrl(ContentResolver cr, String websiteUrl) {
        deleteAnnotation(cr, websiteUrl, Key.FEED);
    }

    /**
     * Get website URLs that are mapped to the given feed URL.
     */
    @Override
    public Cursor getWebsitesWithFeedUrl(ContentResolver cr) {
        return cr.query(urlAnnotationsTableWithProfile,
                new String[] { BrowserContract.UrlAnnotations.URL },
                BrowserContract.UrlAnnotations.KEY + " = ?",
                new String[] { Key.FEED.getDbValue() },
                null);
    }

    /**
     * Returns true if there's a subscription for this feed URL. False otherwise.
     */
    @Override
    public boolean hasFeedSubscription(ContentResolver cr, String feedUrl) {
        return hasResultsForSelection(cr,
                BrowserContract.UrlAnnotations.URL + " = ? AND " + BrowserContract.UrlAnnotations.KEY + " = ?",
                new String[]{feedUrl, Key.FEED_SUBSCRIPTION.getDbValue()});
    }

    /**
     * Insert the given feed subscription (Mapping from feed URL to the subscription object).
     */
    @Override
    public void insertFeedSubscription(ContentResolver cr, FeedSubscription subscription) {
        try {
            insertAnnotation(cr, subscription.getFeedUrl(), Key.FEED_SUBSCRIPTION, subscription.toJSON().toString());
        } catch (JSONException e) {
            Log.w(LOGTAG, "Could not serialize subscription");
        }
    }

    /**
     * Update the feed subscription with new values.
     */
    @Override
    public void updateFeedSubscription(ContentResolver cr, FeedSubscription subscription) {
        try {
            updateAnnotation(cr, subscription.getFeedUrl(), Key.FEED_SUBSCRIPTION, subscription.toJSON().toString());
        } catch (JSONException e) {
            Log.w(LOGTAG, "Could not serialize subscription");
        }
    }

    /**
     * Delete the subscription for the feed URL.
     */
    @Override
    public void deleteFeedSubscription(ContentResolver cr, FeedSubscription subscription) {
        deleteAnnotation(cr, subscription.getFeedUrl(), Key.FEED_SUBSCRIPTION);
    }

    private int deleteAnnotation(final ContentResolver cr, final String url, final Key key) {
        return cr.delete(urlAnnotationsTableWithProfile,
                BrowserContract.UrlAnnotations.KEY + " = ? AND " + BrowserContract.UrlAnnotations.URL + " = ?",
                new String[] { key.getDbValue(), url  });
    }

    private int updateAnnotation(final ContentResolver cr, final String url, final Key key, final String value) {
        ContentValues values = new ContentValues();
        values.put(BrowserContract.UrlAnnotations.VALUE, value);
        values.put(BrowserContract.UrlAnnotations.DATE_MODIFIED, System.currentTimeMillis());

        return cr.update(urlAnnotationsTableWithProfile,
                values,
                BrowserContract.UrlAnnotations.KEY + " = ? AND " + BrowserContract.UrlAnnotations.URL + " = ?",
                new String[]{key.getDbValue(), url});
    }

    private void insertAnnotation(final ContentResolver cr, final String url, final Key key, final String value) {
        insertAnnotation(cr, url, key.getDbValue(), value);
    }

    @RobocopTarget
    @Override
    public void insertAnnotation(final ContentResolver cr, final String url, final String key, final String value) {
        final long creationTime = System.currentTimeMillis();
        final ContentValues values = new ContentValues(5);
        values.put(BrowserContract.UrlAnnotations.URL, url);
        values.put(BrowserContract.UrlAnnotations.KEY, key);
        values.put(BrowserContract.UrlAnnotations.VALUE, value);
        values.put(BrowserContract.UrlAnnotations.DATE_CREATED, creationTime);
        values.put(BrowserContract.UrlAnnotations.DATE_MODIFIED, creationTime);
        cr.insert(urlAnnotationsTableWithProfile, values);
    }

    /**
     * @return true if the table contains rows for the given selection.
     */
    private boolean hasResultsForSelection(ContentResolver cr, String selection, String[] selectionArgs) {
        Cursor cursor = cr.query(urlAnnotationsTableWithProfile,
                new String[] { BrowserContract.UrlAnnotations._ID },
                selection,
                selectionArgs,
                null);
        if (cursor == null) {
            return false;
        }

        try {
            return cursor.getCount() > 0;
        } finally {
            cursor.close();
        }
    }

    private Cursor queryByKey(final ContentResolver cr, @NonNull final Key key, @Nullable final String[] projections,
                @Nullable final String sortOrder) {
        return cr.query(urlAnnotationsTableWithProfile,
                projections,
                BrowserContract.UrlAnnotations.KEY + " = ?", new String[] { key.getDbValue() },
                sortOrder);
    }

    @Override
    public Cursor getScreenshots(ContentResolver cr) {
        return queryByKey(cr,
                Key.SCREENSHOT,
                new String[] {
                        BrowserContract.UrlAnnotations._ID,
                        BrowserContract.UrlAnnotations.URL,
                        BrowserContract.UrlAnnotations.KEY,
                        BrowserContract.UrlAnnotations.VALUE,
                        BrowserContract.UrlAnnotations.DATE_CREATED,
                },
                BrowserContract.UrlAnnotations.DATE_CREATED + " DESC");
    }

    public void insertScreenshot(final ContentResolver cr, final String pageUrl, final String screenshotPath) {
        insertAnnotation(cr, pageUrl, Key.SCREENSHOT.getDbValue(), screenshotPath);
    }
}
