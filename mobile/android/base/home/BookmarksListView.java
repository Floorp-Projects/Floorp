/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.util.GamepadUtils;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.CursorAdapter;
import android.widget.ListView;

import java.util.LinkedList;

/**
 * A ListView of bookmarks.
 */
public class BookmarksListView extends HomeListView
                               implements AdapterView.OnItemClickListener{
    
    public static final String LOGTAG = "GeckoBookmarksListView";

    // A listener that knows how to refresh the list for a given folder id.
    // This is usually implemented by the enclosing fragment/activity.
    public static interface OnRefreshFolderListener {

        // The folder id to refresh the list with.
        public void onRefreshFolder(int folderId);

    }

    // A cursor based adapter.
    private BookmarksListAdapter mCursorAdapter = null;

    // The last motion event that was intercepted.
    private MotionEvent mMotionEvent;

    // The default touch slop.
    private int mTouchSlop;

    // Refresh folder listener.
    private OnRefreshFolderListener mListener;

    public BookmarksListView(Context context) {
        this(context, null);
    }

    public BookmarksListView(Context context, AttributeSet attrs) {
        this(context, attrs, android.R.attr.listViewStyle);
    }

    public BookmarksListView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        // Scaled touch slop for this context.
        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        // Intialize the adapter.
        mCursorAdapter = new BookmarksListAdapter(getContext(), null);
        setAdapter(mCursorAdapter);

        setOnItemClickListener(this);
        setOnKeyListener(GamepadUtils.getListItemClickDispatcher());
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mCursorAdapter = null;
        mListener = null;
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

        if (mCursorAdapter.isShowingChildFolder()) {
            if (position == 0) {
                // If we tap on an opened folder, move back to parent folder.
                mCursorAdapter.moveToParentFolder();
                return;
            }

            // Accounting for the folder view.
            position--;
        }

        final Cursor cursor = mCursorAdapter.getCursor();
        if (cursor == null) {
            return;
        }

        cursor.moveToPosition(position);

        int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
        if (type == Bookmarks.TYPE_FOLDER) {
            // If we're clicking on a folder, update adapter to move to that folder
            final int folderId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            final String folderTitle = mCursorAdapter.getFolderTitle(cursor);
            mCursorAdapter.moveToChildFolder(folderId, folderTitle);
        } else {
            // Otherwise, just open the URL
            final String url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
            OnUrlOpenListener listener = getOnUrlOpenListener();
            if (listener != null) {
                listener.onUrlOpen(url);
            }
        }
    }

    public void refreshFromCursor(Cursor cursor) {
        // This will update the cursorAdapter to use the new one if it already exists.
        mCursorAdapter.swapCursor(cursor);
    }

    public void setOnRefreshFolderListener(OnRefreshFolderListener listener) {
        mListener = listener;
    }

    /**
     * Adapter to back the ListView with a list of bookmarks.
     */
    private class BookmarksListAdapter extends CursorAdapter {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_FOLDER = 1;

        private static final int VIEW_TYPE_COUNT = 2;

        // mParentStack holds folder id/title pairs that allow us to navigate
        // back up the folder heirarchy.
        private LinkedList<Pair<Integer, String>> mParentStack;

        public BookmarksListAdapter(Context context, Cursor cursor) {
            // Initializing with a null cursor.
            super(context, cursor);

            mParentStack = new LinkedList<Pair<Integer, String>>();

            // Add the root folder to the stack
            Pair<Integer, String> rootFolder = new Pair<Integer, String>(Bookmarks.FIXED_ROOT_ID, "");
            mParentStack.addFirst(rootFolder);
        }

        // Refresh the current folder by executing a new task.
        private void refreshCurrentFolder() {
            if (mListener != null) {
                mListener.onRefreshFolder(mParentStack.peek().first);
            }
        }

        /**
         * Moves to parent folder, if one exists.
         */
        public void moveToParentFolder() {
            // If we're already at the root, we can't move to a parent folder
            if (mParentStack.size() != 1) {
                mParentStack.removeFirst();
                refreshCurrentFolder();
            }
        }

        /**
         * Moves to child folder, given a folderId.
         *
         * @param folderId The id of the folder to show.
         * @param folderTitle The title of the folder to show.
         */
        public void moveToChildFolder(int folderId, String folderTitle) {
            Pair<Integer, String> folderPair = new Pair<Integer, String>(folderId, folderTitle);
            mParentStack.addFirst(folderPair);
            refreshCurrentFolder();
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public int getItemViewType(int position) {
            // The position also reflects the opened child folder row.
            if (isShowingChildFolder()) {
                if (position == 0) {
                    return VIEW_TYPE_FOLDER;
                }

                // Accounting for the folder view.
                position--;
            }

            Cursor c = getCursor();

            if (!c.moveToPosition(position)) {
                throw new IllegalStateException("Couldn't move cursor to position " + position);
            }

            return getItemViewType(c);
        }

        /**
         * Returns the type of the item at the given position in the cursor.
         *
         * @param cursor A cursor moved to the required position.
         * @return The type of the item.
         */
        public int getItemViewType(Cursor cursor) {
            if (cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER) {
                return VIEW_TYPE_FOLDER;
            }

            // Default to returning normal item type.
            return VIEW_TYPE_ITEM;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        /**
         * Get the title of the folder given a cursor moved to the position.
         *
         * @param cursor A cursor moved to the required position.
         * @return The title of the folder at the position.
         */
        public String getFolderTitle(Cursor c) {
            String guid = c.getString(c.getColumnIndexOrThrow(Bookmarks.GUID));

            // If we don't have a special GUID, just return the folder title from the DB.
            if (guid == null || guid.length() == 12) {
                return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));
            }

            // Use localized strings for special folder names.
            if (guid.equals(Bookmarks.FAKE_DESKTOP_FOLDER_GUID)) {
                return getResources().getString(R.string.bookmarks_folder_desktop);
            } else if (guid.equals(Bookmarks.MENU_FOLDER_GUID)) {
                return getResources().getString(R.string.bookmarks_folder_menu);
            } else if (guid.equals(Bookmarks.TOOLBAR_FOLDER_GUID)) {
                return getResources().getString(R.string.bookmarks_folder_toolbar);
            } else if (guid.equals(Bookmarks.UNFILED_FOLDER_GUID)) {
                return getResources().getString(R.string.bookmarks_folder_unfiled);
            }

            // If for some reason we have a folder with a special GUID, but it's not one of
            // the special folders we expect in the UI, just return the title from the DB.
            return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));
        }

        /**
         * @return true, if currently showing a child folder, false otherwise.
         */
        public boolean isShowingChildFolder() {
            return (mParentStack.peek().first != Bookmarks.FIXED_ROOT_ID);
        }

        @Override
        public int getCount() {
            return super.getCount() + (isShowingChildFolder() ? 1 : 0);
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            // The position also reflects the opened child folder row.
            if (isShowingChildFolder()) {
                if (position == 0) {
                    BookmarkFolderView folder = (BookmarkFolderView) LayoutInflater.from(parent.getContext()).inflate(R.layout.bookmark_folder_row, null);
                    folder.setText(mParentStack.peek().second);
                    folder.open();
                    return folder;
                }

                // Accounting for the folder view.
                position--;
            }

            return super.getView(position, convertView, parent);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            final int viewType = getItemViewType(cursor);

            if (viewType == VIEW_TYPE_ITEM) {
                TwoLinePageRow row = (TwoLinePageRow) view;
                row.updateFromCursor(cursor);
            } else {
                BookmarkFolderView row = (BookmarkFolderView) view;
                row.setText(getFolderTitle(cursor));
                row.close();
            }
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            final int viewType = getItemViewType(cursor);

            final int resId;
            if (viewType == VIEW_TYPE_ITEM) {
                resId = R.layout.home_item_row;
            } else {
                resId = R.layout.bookmark_folder_row;
            }

            return LayoutInflater.from(parent.getContext()).inflate(resId, null);
        }
    }
}
