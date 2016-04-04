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
    public boolean hasBeenUpdated(FeedFetcher.FeedResponse response) {
        final Item responseItem = response.feed.getLastItem();

        if (responseItem.getTimestamp() > lastItemTimestamp) {
            // The timestamp is from a newer date so we expect that this item is a new item. But this
            // could also mean that the timestamp of an already existing item has been updated. We
            // accept that and assume that the content will have changed too in this case.
            return true;
        }

        if (responseItem.getTimestamp() == lastItemTimestamp && responseItem.getTimestamp() != 0) {
            // We have a timestamp that is not zero and this item has still the timestamp: It's very
            // likely that we are looking at the same item. We assume this is not new content.
            return false;
        }

        if (!responseItem.getURL().equals(lastItemUrl)) {
            // The URL changed: It is very likely that this is a new item. At least it has been updated
            // in a way that we just treat it as new content here.
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
