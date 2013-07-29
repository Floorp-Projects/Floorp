/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.ListView;

/**
 * HomeListView is a custom extension of ListView, that packs a HomeContextMenuInfo
 * when any of its rows is long pressed.
 */
public class HomeListView extends ListView
                          implements OnItemLongClickListener {

    // ContextMenuInfo associated with the currently long pressed list item.
    private HomeContextMenuInfo mContextMenuInfo;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    public HomeListView(Context context) {
        this(context, null);
    }

    public HomeListView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.homeListViewStyle);
    }

    public HomeListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        setOnItemLongClickListener(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        mUrlOpenListener = null;
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        Object item = parent.getItemAtPosition(position);

        // HomeListView could hold headers too. Add a context menu info only for its children.
        if (item instanceof Cursor) {
            Cursor cursor = (Cursor) item;
            mContextMenuInfo = new HomeContextMenuInfo(view, position, id, cursor);
            return showContextMenuForChild(HomeListView.this);
        } else {
            mContextMenuInfo = null;
            return false;
        }
    }

    @Override
    public ContextMenuInfo getContextMenuInfo() {
        return mContextMenuInfo;
    }

    public OnUrlOpenListener getOnUrlOpenListener() {
        return mUrlOpenListener;
    }

    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
    }

    /**
     * A ContextMenuInfo for HomeListView that adds details from the cursor.
     */
    public static class HomeContextMenuInfo extends AdapterContextMenuInfo {
        public int bookmarkId;
        public int historyId;
        public String url;
        public byte[] favicon;
        public String title;
        public int display;
        public boolean isFolder;
        public boolean inReadingList;

        /**
         * This constructor assumes that the cursor was generated from a query
         * to either the combined view or the bookmarks table.
         */
        public HomeContextMenuInfo(View targetView, int position, long id, Cursor cursor) {
            super(targetView, position, id);

            if (cursor == null) {
                return;
            }

            final int typeCol = cursor.getColumnIndex(Bookmarks.TYPE);
            if (typeCol != -1) {
                isFolder = (cursor.getInt(typeCol) == Bookmarks.TYPE_FOLDER);
            } else {
                isFolder = false;
            }

            // We don't show a context menu for folders, so return early.
            if (isFolder) {
                return;
            }

            url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
            title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));

            final int bookmarkIdCol = cursor.getColumnIndex(Combined.BOOKMARK_ID);
            if (bookmarkIdCol == -1) {
                // If there isn't a bookmark ID column, this must be a bookmarks cursor,
                // so the regular ID column will correspond to a bookmark ID.
                bookmarkId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            } else if (cursor.isNull(bookmarkIdCol)) {
                // If this is a combined cursor, we may get a history item without a
                // bookmark, in which case the bookmarks ID column value will be null.
                bookmarkId = -1;
            } else {
                bookmarkId = cursor.getInt(bookmarkIdCol);
            }

            final int historyIdCol = cursor.getColumnIndex(Combined.HISTORY_ID);
            if (historyIdCol != -1) {
                historyId = cursor.getInt(historyIdCol);
            } else {
                historyId = -1;
            }

            final int faviconCol = cursor.getColumnIndex(Combined.FAVICON);
            if (faviconCol != -1) {
                favicon = cursor.getBlob(faviconCol);
            } else {
                favicon = null;
            }

            // We only have the parent column in cursors from getBookmarksInFolder.
            final int parentCol = cursor.getColumnIndex(Bookmarks.PARENT);
            if (parentCol != -1) {
                inReadingList = (cursor.getInt(parentCol) == Bookmarks.FIXED_READING_LIST_ID);
            } else {
                inReadingList = false;
            }

            final int displayCol = cursor.getColumnIndex(Combined.DISPLAY);
            if (displayCol != -1) {
                display = cursor.getInt(displayCol);
            } else {
                display = Combined.DISPLAY_NORMAL;
            }
        }
    }
}
