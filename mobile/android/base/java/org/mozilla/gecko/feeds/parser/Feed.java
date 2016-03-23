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
