/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.parser;

public class Item {
    private String title;
    private String url;
    private long timestamp;

    public static Item create(String title, String url, long timestamp) {
        Item item = new Item();

        item.setTitle(title);
        item.setURL(url);
        item.setTimestamp(timestamp);

        return item;
    }

    /* package-private */ void setTitle(String title) {
        this.title = title;
    }

    /* package-private */ void setURL(String url) {
        this.url = url;
    }

    /* package-private */ void setTimestamp(long timestamp) {
        this.timestamp = timestamp;
    }

    public String getTitle() {
        return title;
    }

    public String getURL() {
        return url;
    }

    /**
     * @return the number of milliseconds since Jan. 1, 1970, midnight GMT.
     */
    public long getTimestamp() {
        return timestamp;
    }
}
