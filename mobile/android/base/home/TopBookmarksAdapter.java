/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB.TopSitesCursorWrapper;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.support.v4.widget.CursorAdapter;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import java.util.Map;

/**
 * A cursor adapter holding the pinned and top bookmarks.
 */
public class TopBookmarksAdapter extends CursorAdapter {
    // Cache to store the thumbnails.
    private Map<String, Thumbnail> mThumbnails;

    /**
     *  Class to hold the bitmap of cached thumbnails/favicons.
     */
    public static class Thumbnail {
        // Thumbnail or favicon.
        private final boolean isThumbnail;

        // Bitmap of thumbnail/favicon.
        private final Bitmap bitmap;

        public Thumbnail(Bitmap bitmap, boolean isThumbnail) {
            this.bitmap = bitmap;
            this.isThumbnail = isThumbnail;
        }
    }

    public TopBookmarksAdapter(Context context, Cursor cursor) {
        super(context, cursor);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void onContentChanged() {
        // Don't do anything. We don't want to regenerate every time
        // our database is updated.
        return;
    }

    /**
     * Update the thumbnails returned by the db.
     *
     * @param thumbnails A map of urls and their thumbnail bitmaps.
     */
    public void updateThumbnails(Map<String, Thumbnail> thumbnails) {
        mThumbnails = thumbnails;
        notifyDataSetChanged();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void bindView(View bindView, Context context, Cursor cursor) {
        String url = "";
        String title = "";
        boolean pinned = false;

        // Cursor is already moved to required position.
        if (!cursor.isAfterLast()) {
            url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
            title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
            pinned = ((TopSitesCursorWrapper) cursor).isPinned();
        }

        TopBookmarkItemView view = (TopBookmarkItemView) bindView;
        view.setTitle(title);
        view.setUrl(url);
        view.setPinned(pinned);

        // If there is no url, then show "add bookmark".
        if (TextUtils.isEmpty(url)) {
            view.displayThumbnail(R.drawable.top_bookmark_add);
        } else {
            // Show the thumbnail.
            Thumbnail thumbnail = (mThumbnails != null ? mThumbnails.get(url) : null);
            if (thumbnail == null) {
                view.displayThumbnail(null);
            } else if (thumbnail.isThumbnail) {
                view.displayThumbnail(thumbnail.bitmap);
            } else {
                view.displayFavicon(thumbnail.bitmap);
            }
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public View newView(Context context, Cursor cursor, ViewGroup parent) {
        return new TopBookmarkItemView(context);
    }
}
