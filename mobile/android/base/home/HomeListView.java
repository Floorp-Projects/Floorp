/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
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

    public HomeListView(Context context) {
        this(context, null);
    }

    public HomeListView(Context context, AttributeSet attrs) {
        this(context, attrs, android.R.attr.listViewStyle);
    }

    public HomeListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        setOnItemLongClickListener(this);
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        Cursor cursor = (Cursor) parent.getItemAtPosition(position);
        mContextMenuInfo = new HomeContextMenuInfo(view, position, id, cursor);
        return showContextMenuForChild(HomeListView.this);
    }

    @Override
    public ContextMenuInfo getContextMenuInfo() {
        return mContextMenuInfo;
    }

    /**
     * A ContextMenuInfo for HomeListView that adds details from the cursor.
     */
    public static class HomeContextMenuInfo extends AdapterContextMenuInfo {
        public int rowId;
        public String url;
        public byte[] favicon;
        public String title;
        public String keyword;
        public int display;
        public boolean isFolder;

        public HomeContextMenuInfo(View targetView, int position, long id, Cursor cursor) {
            super(targetView, position, id);

            if (cursor == null) {
                return;
            }

            isFolder = (cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER);

            if (isFolder) {
                return;
            }

            int keywordCol = cursor.getColumnIndex(URLColumns.KEYWORD);
            if (keywordCol != -1) {
                keyword = cursor.getString(keywordCol);
            } else {
                keyword = null;
            }

            rowId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
            title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
            favicon = cursor.getBlob(cursor.getColumnIndexOrThrow(URLColumns.FAVICON));
            display = Combined.DISPLAY_NORMAL;
        }
    }
}
