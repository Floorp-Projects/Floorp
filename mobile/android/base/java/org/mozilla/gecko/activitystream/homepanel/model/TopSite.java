/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.model;

import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.gecko.db.BrowserContract;

public class TopSite implements WebpageModel {
    private final long id;
    private final String url;
    private final String title;
    private @Nullable Boolean isBookmarked;
    private final @Nullable boolean isPinned;
    private @BrowserContract.TopSites.TopSiteType final int type;
    private final Metadata metadata;

    public static TopSite fromCursor(Cursor cursor) {
        // The Combined View only contains pages that have been visited at least once, i.e. any
        // page in the TopSites query will contain a HISTORY_ID. _ID however will be 0 for all rows.
        final long id = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Combined.HISTORY_ID));
        final String url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));
        final String title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.TITLE));
        final int type = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.TopSites.TYPE));
        final Metadata metadata = Metadata.fromCursor(cursor, BrowserContract.TopSites.PAGE_METADATA_JSON);

        // We can't figure out bookmark state of a pin or suggested site, so we leave it as unknown to be queried later.
        Boolean isBookmarked = null;
        if (type != BrowserContract.TopSites.TYPE_PINNED &&
                type != BrowserContract.TopSites.TYPE_SUGGESTED) {
            isBookmarked = !cursor.isNull(cursor.getColumnIndexOrThrow(BrowserContract.Combined.BOOKMARK_ID));
        }

        return new TopSite(id, url, title, isBookmarked, type, metadata);
    }

    private TopSite(long id, String url, String title, @Nullable Boolean isBookmarked, int type, final Metadata metadata) {
        this.id = id;
        this.url = url;
        this.title = title;
        this.isBookmarked = isBookmarked;
        this.isPinned = type == BrowserContract.TopSites.TYPE_PINNED;
        this.type = type;
        this.metadata = metadata;
    }

    public long getId() {
        return id;
    }

    public String getUrl() {
        return url;
    }

    public String getTitle() {
        return title;
    }

    @Nullable
    public Boolean isBookmarked() {
        return isBookmarked;
    }

    @BrowserContract.TopSites.TopSiteType
    public int getType() {
        return type;
    }

    public Boolean isPinned() {
        return isPinned;
    }

    @NonNull
    @Override
    public String getImageUrl() {
        final String imageUrl = metadata.getImageUrl();
        return imageUrl != null ? imageUrl : "";
    }

    @Override
    public void updateBookmarked(boolean bookmarked) {
        this.isBookmarked = bookmarked;
    }

    @Override
    public void updatePinned(boolean pinned) {
        throw new UnsupportedOperationException(
                "Pinned state of a top site should be known at the time of querying the database already");
    }

    // The TopSites cursor automatically notifies of data changes, so nothing needs to be done here.
    @Override
    public void onStateCommitted() {}
}
