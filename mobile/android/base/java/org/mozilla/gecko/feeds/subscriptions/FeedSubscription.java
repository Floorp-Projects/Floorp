/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.subscriptions;

import android.database.Cursor;
import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.parser.Item;

/**
 * An object describing a subscription and containing some meta data about the last time we fetched
 * the feed.
 */
public class FeedSubscription {
    private static final String JSON_KEY_FEED_TITLE = "feed_title";
    private static final String JSON_KEY_LAST_ITEM_TITLE = "last_item_title";
    private static final String JSON_KEY_LAST_ITEM_URL = "last_item_url";
    private static final String JSON_KEY_LAST_ITEM_TIMESTAMP = "last_item_timestamp";
    private static final String JSON_KEY_ETAG = "etag";
    private static final String JSON_KEY_LAST_MODIFIED = "last_modified";

    private String feedUrl;
    private String feedTitle;
    private String lastItemTitle;
    private String lastItemUrl;
    private long lastItemTimestamp;
    private String etag;
    private String lastModified;

    public static FeedSubscription create(String feedUrl, FeedFetcher.FeedResponse response) {
        FeedSubscription subscription = new FeedSubscription();
        subscription.feedUrl = feedUrl;

        subscription.update(response);

        return subscription;
    }

    public static FeedSubscription fromCursor(Cursor cursor) throws JSONException {
        final FeedSubscription subscription = new FeedSubscription();
        subscription.feedUrl = cursor.getString(cursor.getColumnIndex(BrowserContract.UrlAnnotations.URL));

        final String value = cursor.getString(cursor.getColumnIndex(BrowserContract.UrlAnnotations.VALUE));
        subscription.fromJSON(new JSONObject(value));

        return subscription;
    }

    private void fromJSON(JSONObject object) throws JSONException {
        feedTitle = object.getString(JSON_KEY_FEED_TITLE);
        lastItemTitle = object.getString(JSON_KEY_LAST_ITEM_TITLE);
        lastItemUrl = object.getString(JSON_KEY_LAST_ITEM_URL);
        lastItemTimestamp = object.getLong(JSON_KEY_LAST_ITEM_TIMESTAMP);
        etag = object.optString(JSON_KEY_ETAG);
        lastModified = object.optString(JSON_KEY_LAST_MODIFIED);
    }

    public void update(FeedFetcher.FeedResponse response) {
        feedTitle = response.feed.getTitle();
        lastItemTitle = response.feed.getLastItem().getTitle();
        lastItemUrl = response.feed.getLastItem().getURL();
        lastItemTimestamp = response.feed.getLastItem().getTimestamp();
        etag = response.etag;
        lastModified = response.lastModified;
    }

    /**
     * Guesstimate if this response is a newer representation of the feed.
     */
    public boolean isNewer(FeedFetcher.FeedResponse response) {
        final Item otherItem = response.feed.getLastItem();

        if (lastItemTimestamp > otherItem.getTimestamp()) {
            return true; // How to detect if this same item and it only has been updated?
        }

        if (lastItemTimestamp == otherItem.getTimestamp() &&
                lastItemTimestamp != 0) {
            return false;
        }

        if (lastItemUrl == null || !lastItemUrl.equals(otherItem.getURL())) {
            // URL changed: Probably a different item
            return true;
        }

        return false;
    }

    public String getFeedUrl() {
        return feedUrl;
    }

    public String getFeedTitle() {
        return feedTitle;
    }

    public String getETag() {
        return etag;
    }

    public String getLastModified() {
        return lastModified;
    }

    public JSONObject toJSON() throws JSONException {
        JSONObject object = new JSONObject();

        object.put(JSON_KEY_FEED_TITLE, feedTitle);
        object.put(JSON_KEY_LAST_ITEM_TITLE, lastItemTitle);
        object.put(JSON_KEY_LAST_ITEM_URL, lastItemUrl);
        object.put(JSON_KEY_LAST_ITEM_TIMESTAMP, lastItemTimestamp);
        object.put(JSON_KEY_ETAG, etag);
        object.put(JSON_KEY_LAST_MODIFIED, lastModified);

        return object;
    }
}
