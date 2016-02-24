/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.parser;

import ch.boye.httpclientandroidlib.util.TextUtils;

public class Feed {
    private String title;
    private String websiteURL;
    private String feedURL;
    private Item lastItem;

    public static Feed create(String title, String websiteURL, String feedURL, Item lastItem) {
        Feed feed = new Feed();

        feed.setTitle(title);
        feed.setWebsiteURL(websiteURL);
        feed.setFeedURL(feedURL);
        feed.setLastItem(lastItem);

        return feed;
    }

    /* package-private */ Feed() {}

    /* package-private */ void setTitle(String title) {
        this.title = title;
    }

    /* package-private */ void setWebsiteURL(String websiteURL) {
        this.websiteURL = websiteURL;
    }

    /* package-private */ void setFeedURL(String feedURL) {
        this.feedURL = feedURL;
    }

    /* package-private */ void setLastItem(Item lastItem) {
        this.lastItem = lastItem;
    }

    /**
     * Is this feed object sufficiently complete so that we can use it?
     */
    /* package-private */ boolean isSufficientlyComplete() {
        return !TextUtils.isEmpty(title) &&
                lastItem != null &&
                !TextUtils.isEmpty(lastItem.getURL()) &&
                !TextUtils.isEmpty(lastItem.getTitle());
    }

    /**
     * Guesstimate if the given feed is a newer representation of this feed.
     */
    public boolean hasBeenUpdated(Feed newFeed) {
        final Item otherItem = newFeed.getLastItem();

        if (lastItem.getTimestamp() > otherItem.getTimestamp()) {
            // The timestamp is from a newer date so we expect that this item is a new item. But this
            // could also mean that the timestamp of an already existing item has been updated. We
            // accept that and assume that the content will have changed too in this case.
            return true;
        }

        if (lastItem.getTimestamp() == otherItem.getTimestamp() && lastItem.getTimestamp() != 0) {
            // We have a timestamp that is not zero and this item has still the timestamp: It's very
            // likely that we are looking at the same item. We assume this is not new content.
            return false;
        }

        if (!lastItem.getURL().equals(otherItem.getURL())) {
            // The URL changed: It is very likely that this is a new item. At least it has been updated
            // in a way that we just treat it as new content here.
            return true;
        }

        return false;
    }

    public String getTitle() {
        return title;
    }

    public String getWebsiteURL() {
        return websiteURL;
    }

    public String getFeedURL() {
        return feedURL;
    }

    public Item getLastItem() {
        return lastItem;
    }
}
