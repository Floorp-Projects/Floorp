/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomeListView.HomeContextMenuInfo;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;

import java.util.LinkedList;

/**
 * A page in about:home that displays a ListView of bookmarks.
 */
public class BookmarksPage extends HomeFragment {
    public static final String LOGTAG = "GeckoBookmarksPage";

    private int mFolderId = Bookmarks.FIXED_ROOT_ID;
    private String mFolderTitle = "";

    private BookmarksListAdapter mCursorAdapter = null;
    private BookmarksQueryTask mQueryTask = null;

    // The view shown by the fragment.
    private HomeListView mList;

    // Folder title for the currently shown list of bookmarks.
    private BookmarkFolderView mFolderView;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    public BookmarksPage() {
        mUrlOpenListener = null;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnUrlOpenListener");
        }

        // Intialize the adapter.
        mCursorAdapter = new BookmarksListAdapter(getActivity());
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUrlOpenListener = null;

        // Can't use getters for adapter. It will create one if null.
        if (mCursorAdapter != null) {
            final Cursor cursor = mCursorAdapter.getCursor();
            mCursorAdapter = null;

            // Gingerbread locks the DB when closing a cursor, so do it in the background.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    if (cursor != null && !cursor.isClosed())
                        cursor.close();
                }
            });
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        super.onCreateView(inflater, container, savedInstanceState);

        // All list views are styled to look the same with a global activity theme.
        // If the style of the list changes, inflate it from an XML.
        mList = new HomeListView(container.getContext());
        return mList;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Folder title view, is always in open state.
        mFolderView = (BookmarkFolderView) LayoutInflater.from(getActivity()).inflate(R.layout.bookmark_folder_row, null);
        mFolderView.open();

        // We need to add the header before we set the adapter, hence make it null
        refreshListWithCursor(null);

        EventHandlers eventHandlers = new EventHandlers();
        mList.setOnTouchListener(eventHandlers);
        mList.setOnItemClickListener(eventHandlers);
        mList.setOnCreateContextMenuListener(eventHandlers);
        mList.setOnKeyListener(GamepadUtils.getListItemClickDispatcher());

        mQueryTask = new BookmarksQueryTask();
        mQueryTask.execute();
    }

    @Override
    public void onDestroyView() {
        mList = null;
        mFolderView = null;
        super.onDestroyView();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Reattach the fragment, forcing a reinflation of its view.
        // We use commitAllowingStateLoss() instead of commit() here to avoid
        // an IllegalStateException. If the phone is rotated while Fennec
        // is in the background, onConfigurationChanged() is fired.
        // onConfigurationChanged() is called before onResume(), so
        // using commit() would throw an IllegalStateException since it can't
        // be used between the Activity's onSaveInstanceState() and
        // onResume().
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    private void refreshListWithCursor(Cursor cursor) {
        // We need to add the header before we set the adapter, hence making it null.
        mList.setAdapter(null);

        // Add a header view based on the root folder.
        if (mFolderId == Bookmarks.FIXED_ROOT_ID) {
            if (mList.getHeaderViewsCount() == 1) {
                mList.removeHeaderView(mFolderView);
            }
        } else {
            if (mList.getHeaderViewsCount() == 0) {
                mList.addHeaderView(mFolderView, null, true);
            }

            mFolderView.setText(mFolderTitle);
        }

        // This will update the cursorAdapter to use the new one if it already exists.
        mCursorAdapter.changeCursor(cursor);
        mList.setAdapter(mCursorAdapter);

        // Reset the task.
        mQueryTask = null;
    }

    /**
     * Internal class to handle different event listeners on the ListView.
     */
    private class EventHandlers implements AdapterView.OnItemClickListener,
                                           View.OnCreateContextMenuListener,
                                           View.OnTouchListener {
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            final ListView list = (ListView) parent;
            final int headerCount = list.getHeaderViewsCount();

            // If we tap on the header view, move back to parent folder.
            if (headerCount == 1 && position == 0) {
                mCursorAdapter.moveToParentFolder();
                return;
            }

            Cursor cursor = mCursorAdapter.getCursor();
            if (cursor == null) {
                return;
            }

            // The header view takes up a spot in the list
            if (headerCount == 1) {
                position--;
            }

            cursor.moveToPosition(position);

            int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
            if (type == Bookmarks.TYPE_FOLDER) {
                // If we're clicking on a folder, update adapter to move to that folder
                int folderId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
                String folderTitle = mCursorAdapter.getFolderTitle(position);
                mCursorAdapter.moveToChildFolder(folderId, folderTitle);
             } else {
                // Otherwise, just open the URL
                String url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
                if (mUrlOpenListener != null) {
                    mUrlOpenListener.onUrlOpen(url);
                }
             }
         }

        @Override
        public boolean onTouch(View view, MotionEvent event) {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                // take focus away from awesome bar to hide the keyboard
                view.requestFocus();
            }
            return false;
        }

        @Override
        public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
            if (!(menuInfo instanceof HomeContextMenuInfo)) {
                return;
            }

            HomeContextMenuInfo info = (HomeContextMenuInfo) menuInfo;

            // Don't show the context menu for folders.
            if (info.isFolder) {
                return;
            }

            MenuInflater inflater = new MenuInflater(view.getContext());
            inflater.inflate(R.menu.home_contextmenu, menu);

            // Show Open Private Tab if we're in private mode, Open New Tab otherwise
            boolean isPrivate = false;
            Tab tab = Tabs.getInstance().getSelectedTab();
            if (tab != null) {
                isPrivate = tab.isPrivate();
            }
            menu.findItem(R.id.open_new_tab).setVisible(!isPrivate);
            menu.findItem(R.id.open_private_tab).setVisible(isPrivate);

            // Hide "Remove" item if there isn't a valid history ID
            if (info.rowId < 0) {
                menu.findItem(R.id.remove_history).setVisible(false);
            }
            menu.setHeaderTitle(info.title);
 
            menu.findItem(R.id.remove_history).setVisible(false);
            menu.findItem(R.id.open_in_reader).setVisible(false);
            return;
        }
    }

    /**
     * Adapter to back the ListView with a list of bookmarks.
     */
    private class BookmarksListAdapter extends SimpleCursorAdapter {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_FOLDER = 1;

        private static final int VIEW_TYPE_COUNT = 2;

        // mParentStack holds folder id/title pairs that allow us to navigate
        // back up the folder heirarchy.
        private LinkedList<Pair<Integer, String>> mParentStack;

        public BookmarksListAdapter(Context context) {
            // Initializing with a null cursor.
            super(context, -1, null, new String[] {}, new int[] {});

            mParentStack = new LinkedList<Pair<Integer, String>>();

            // Add the root folder to the stack
            Pair<Integer, String> rootFolder = new Pair<Integer, String>(mFolderId, mFolderTitle);
            mParentStack.addFirst(rootFolder);
        }

        // Refresh the current folder by executing a new task.
        private void refreshCurrentFolder() {
            // Cancel any pre-existing async refresh tasks
            if (mQueryTask != null) {
                mQueryTask.cancel(false);
            }

            Pair<Integer, String> folderPair = mParentStack.getFirst();
            mFolderId = folderPair.first;
            mFolderTitle = folderPair.second;

            mQueryTask = new BookmarksQueryTask();
            mQueryTask.execute(new Integer(mFolderId));
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
            Cursor c = getCursor();

            if (c.moveToPosition(position) &&
                c.getInt(c.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER) {
                return VIEW_TYPE_FOLDER;
            }

            // Default to retuning normal item type
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
         * Get the title of the folder for a given position.
         *
         * @param position The position of the view.
         * @return The title of the folder at the position.
         */
        public String getFolderTitle(int position) {
            Cursor c = getCursor();
            if (!c.moveToPosition(position)) {
                return "";
            }

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
         * {@inheritDoc}
         */
        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final int viewType = getItemViewType(position);

            if (convertView == null) {
                if (viewType == VIEW_TYPE_ITEM) {
                    convertView = LayoutInflater.from(parent.getContext()).inflate(R.layout.home_item_row, null);
                } else {
                    convertView = LayoutInflater.from(parent.getContext()).inflate(R.layout.bookmark_folder_row, null);
                }
            }

            Cursor cursor = getCursor();
            if (!cursor.moveToPosition(position)) {
                throw new IllegalStateException("Couldn't move cursor to position " + position);
            }

            if (viewType == VIEW_TYPE_ITEM) {
                TwoLinePageRow row = (TwoLinePageRow) convertView;
                row.updateFromCursor(cursor);
            } else {
                BookmarkFolderView row = (BookmarkFolderView) convertView;
                row.setText(getFolderTitle(position));
            }

            return convertView;
        }
    }

    /**
     * AsyncTask to query the DB for bookmarks.
     */
    private class BookmarksQueryTask extends AsyncTask<Integer, Void, Cursor> {
        @Override
        protected Cursor doInBackground(Integer... folderIds) {
            int folderId = Bookmarks.FIXED_ROOT_ID;

            if (folderIds.length != 0) {
                folderId = folderIds[0].intValue();
            }

            return BrowserDB.getBookmarksInFolder(getActivity().getContentResolver(), folderId);
        }

        @Override
        protected void onPostExecute(final Cursor cursor) {
            refreshListWithCursor(cursor);
        }
    }
}
