/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.AdapterView;
import android.widget.HeaderViewListAdapter;
import android.widget.ListAdapter;
import android.widget.ListView;

import java.util.EnumSet;

/**
 * A ListView of bookmarks.
 */
public class BookmarksListView extends HomeListView
                               implements AdapterView.OnItemClickListener{
    public static final String LOGTAG = "GeckoBookmarksListView";

    // The last motion event that was intercepted.
    private MotionEvent mMotionEvent;

    // The default touch slop.
    private int mTouchSlop;

    public BookmarksListView(Context context) {
        this(context, null);
    }

    public BookmarksListView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.bookmarksListViewStyle);
    }

    public BookmarksListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        // Scaled touch slop for this context.
        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        setOnItemClickListener(this);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        switch(event.getAction() & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_DOWN: {
                // Store the event by obtaining a copy.
                mMotionEvent = MotionEvent.obtain(event);
                break;
            }

            case MotionEvent.ACTION_MOVE: {
                if ((mMotionEvent != null) &&
                    (Math.abs(event.getY() - mMotionEvent.getY()) > mTouchSlop)) {
                    // The user is scrolling. Pass the last event to this view,
                    // and make this view scroll.
                    onTouchEvent(mMotionEvent);
                    return true;
                }
                break;
            }

            default: {
                mMotionEvent = null;
                break;
            }
        }

        // Do default interception.
        return super.onInterceptTouchEvent(event);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        final ListView list = (ListView) parent;
        final int headerCount = list.getHeaderViewsCount();

        if (position < headerCount) {
            // The click is on a header, don't do anything.
            return;
        }

        // Absolute position for the adapter.
        position -= headerCount;

        BookmarksListAdapter adapter;
        ListAdapter listAdapter = getAdapter();
        if (listAdapter instanceof HeaderViewListAdapter) {
            adapter = (BookmarksListAdapter) ((HeaderViewListAdapter) listAdapter).getWrappedAdapter();
        } else {
            adapter = (BookmarksListAdapter) listAdapter;
        }

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

            // This item is a TwoLinePageRow, so we allow switch-to-tab.
            getOnUrlOpenListener().onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
        }
    }
}
