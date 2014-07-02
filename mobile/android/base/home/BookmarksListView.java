/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.HeaderViewListAdapter;
import android.widget.ListAdapter;

/**
 * A ListView of bookmarks.
 */
public class BookmarksListView extends HomeListView
                               implements AdapterView.OnItemClickListener{
    public static final String LOGTAG = "GeckoBookmarksListView";

    public BookmarksListView(Context context) {
        this(context, null);
    }

    public BookmarksListView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.bookmarksListViewStyle);
    }

    public BookmarksListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        setOnItemClickListener(this);

        setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                final int action = event.getAction();

                // If the user hit the BACK key, try to move to the parent folder.
                if (action == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
                    return getBookmarksListAdapter().moveToParentFolder();
                }
                return false;
            }
        });
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        final BookmarksListAdapter adapter = getBookmarksListAdapter();
        if (adapter.isShowingChildFolder()) {
            if (position == 0) {
                // If we tap on an opened folder, move back to parent folder.
                adapter.moveToParentFolder();
                return;
            }

            // Accounting for the folder view.
            position--;
        }

        final Cursor cursor = adapter.getCursor();
        if (cursor == null) {
            return;
        }

        cursor.moveToPosition(position);

        int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
        if (type == Bookmarks.TYPE_FOLDER) {
            // If we're clicking on a folder, update adapter to move to that folder
            final int folderId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            final String folderTitle = adapter.getFolderTitle(parent.getContext(), cursor);
            adapter.moveToChildFolder(folderId, folderTitle);
        } else {
            // Otherwise, just open the URL
            final String url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));

            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM);

            // This item is a TwoLinePageRow, so we allow switch-to-tab.
            getOnUrlOpenListener().onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
        }
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        // Adjust the item position to account for the parent folder row that is inserted
        // at the top of the list when viewing the contents of a folder.
        final BookmarksListAdapter adapter = getBookmarksListAdapter();
        if (adapter.isShowingChildFolder()) {
            position--;
        }

        return super.onItemLongClick(parent, view, position, id);
    }

    private BookmarksListAdapter getBookmarksListAdapter() {
        BookmarksListAdapter adapter;
        ListAdapter listAdapter = getAdapter();
        if (listAdapter instanceof HeaderViewListAdapter) {
            adapter = (BookmarksListAdapter) ((HeaderViewListAdapter) listAdapter).getWrappedAdapter();
        } else {
            adapter = (BookmarksListAdapter) listAdapter;
        }
        return adapter;
    }
}
