/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.subscriptions;

import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.feeds.FeedFetcher;
import org.mozilla.gecko.feeds.parser.Item;

/**
 * An object describing a subscription and containing some meta data about the last time we fetched
 * the feed.
 */
public class FeedSubscription {
    private static final String JSON_KEY_FEED_URL = "feed_url";
    private static final String JSON_KEY_FEED_TITLE = "feed_title";
    private static final String JSON_KEY_WEBSITE_URL = "website_url";
    private static final String JSON_KEY_LAST_ITEM_TITLE = "last_item_title";
    private static final String JSON_KEY_LAST_ITEM_URL = "last_item_url";
    private static final String JSON_KEY_LAST_ITEM_TIMESTAMP = "last_item_timestamp";
    private static final String JSON_KEY_ETAG = "etag";
    private static final String JSON_KEY_LAST_MODIFIED = "last_modified";
    private static final String JSON_KEY_BOOKMARK_GUID = "bookmark_guid";

    private String bookmarkGuid; // Currently a subscription is linked to a bookmark
    private String feedUrl;
    private String feedTitle;
    private String websiteUrl;
    private String lastItemTitle;
    private String lastItemUrl;
    private long lastItemTimestamp;
    private String etag;
    private String lastModified;

    public static FeedSubscription create(String bookmarkGuid, String url, FeedFetcher.FeedResponse response) {
        FeedSubscription subscription = new FeedSubscription();
        subscription.bookmarkGuid = bookmarkGuid;
        subscription.feedUrl = url;

        subscription.update(response);

        return subscription;
    }

    public static FeedSubscription fromJSON(JSONObject object) throws JSONException {
        FeedSubscription subscription = new FeedSubscription();

        subscription.feedUrl = object.getString(JSON_KEY_FEED_URL);
        subscription.feedTitle = object.getString(JSON_KEY_FEED_TITLE);
        subscription.websiteUrl = object.getString(JSON_KEY_WEBSITE_URL);
        subscription.lastItemTitle = object.getString(JSON_KEY_LAST_ITEM_TITLE);
        subscription.lastItemUrl = object.getString(JSON_KEY_LAST_ITEM_URL);
        subscription.lastItemTimestamp = object.getLong(JSON_KEY_LAST_ITEM_TIMESTAMP);
        subscription.etag = object.getString(JSON_KEY_ETAG);
        subscription.lastModified = object.getString(JSON_KEY_LAST_MODIFIED);
        subscription.bookmarkGuid = object.getString(JSON_KEY_BOOKMARK_GUID);

        return subscription;
    }

    /* package-private */ void update(FeedFetcher.FeedResponse response) {
        final String feedUrl = response.feed.getFeedURL();
        if (!TextUtils.isEmpty(feedUrl)) {
            // Prefer to use the URL we get from the feed for further requests
            this.feedUrl = feedUrl;
        }

        feedTitle = response.feed.getTitle();
        websiteUrl = response.feed.getWebsiteURL();
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

    public String getWebsiteUrl() {
        return websiteUrl;
    }

    public String getLastItemTitle() {
        return lastItemTitle;
    }

    public String getLastItemUrl() {
        return lastItemUrl;
    }

    public long getLastItemTimestamp() {
        return lastItemTimestamp;
    }

    public String getETag() {
        return etag;
    }

    public String getLastModified() {
        return lastModified;
    }

    public String getBookmarkGUID() {
        return bookmarkGuid;
    }

    public boolean isForTheSameBookmarkAs(FeedSubscription other) {
        return TextUtils.equals(bookmarkGuid, other.bookmarkGuid);
    }

    public JSONObject toJSON() throws JSONException {
        JSONObject object = new JSONObject();

        object.put(JSON_KEY_FEED_URL, feedUrl);
        object.put(JSON_KEY_FEED_TITLE, feedTitle);
        object.put(JSON_KEY_WEBSITE_URL, websiteUrl);
        object.put(JSON_KEY_LAST_ITEM_TITLE, lastItemTitle);
        object.put(JSON_KEY_LAST_ITEM_URL, lastItemUrl);
        object.put(JSON_KEY_LAST_ITEM_TIMESTAMP, lastItemTimestamp);
        object.put(JSON_KEY_ETAG, etag);
        object.put(JSON_KEY_LAST_MODIFIED, lastModified);
        object.put(JSON_KEY_BOOKMARK_GUID, bookmarkGuid);

        return object;
    }
}
