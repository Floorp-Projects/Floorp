/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AwesomeBar.ContextMenuSubject;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.os.AsyncTask;
import android.util.Log;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.widget.TextView;

import java.util.LinkedList;

public class BookmarksTab extends AwesomeBarTab {
    public static final String LOGTAG = "BOOKMARKS_TAB";
    public static final String TAG = "bookmarks";
    private int mFolderId;
    private String mFolderTitle;
    private BookmarksListAdapter mCursorAdapter = null;
    private BookmarksQueryTask mQueryTask = null;
    private boolean mShowReadingList = false;

    @Override
    public int getTitleStringId() {
        return R.string.awesomebar_bookmarks_title;
    }

    @Override
    public String getTag() {
        return TAG;
    }

    public BookmarksTab(Context context) {
        super(context);
    }

    @Override
    public View getView() {
        if (mView == null) {
            mView = new ListView(mContext, null);
            ((Activity)mContext).registerForContextMenu(mView);
            mView.setTag(TAG);
            mView.setOnTouchListener(mListListener);

            // We need to add the header before we set the adapter, hence make it null
            ListView list = (ListView)mView;
            list.setAdapter(null);
            list.setAdapter(getCursorAdapter());
            list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                @Override
                public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                    handleItemClick(parent, view, position, id);
                }
            });
            list.setOnKeyListener(GamepadUtils.getListItemClickDispatcher());

            if (mShowReadingList) {
                String title = getResources().getString(R.string.bookmarks_folder_reading_list);
                getCursorAdapter().moveToChildFolder(Bookmarks.FIXED_READING_LIST_ID, title);
            } else {
                BookmarksQueryTask task = getQueryTask();
                task.execute();
            }
        }
        return (ListView)mView;
    }

    public void setShowReadingList(boolean showReadingList) {
        mShowReadingList = showReadingList;
    }

    @Override
    public void destroy() {
        // Can't use getters for adapter. It will create one if null.
        if (mCursorAdapter != null && mView != null) {
            ListView list = (ListView)mView;
            list.setAdapter(null);
            final Cursor cursor = mCursorAdapter.getCursor();
            // Gingerbread locks the DB when closing a cursor, so do it in the
            // background.
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
    public boolean onBackPressed() {
        // If the soft keyboard is visible in the bookmarks or history tab, the user
        // must have explictly brought it up, so we should try hiding it instead of
        // exiting the activity or going up a bookmarks folder level.
        if (hideSoftInput(getView()))
            return true;

        return moveToParentFolder();
    }

    protected BookmarksListAdapter getCursorAdapter() {
        return getCursorAdapter(null);
    }

    protected BookmarksListAdapter getCursorAdapter(Cursor c) {
        if (mCursorAdapter == null) {
            mCursorAdapter = new BookmarksListAdapter(mContext, c);
        } else if (c != null) {
            mCursorAdapter.changeCursor(c);
        } else {
            // do a quick return if just asking for the cached adapter
            return mCursorAdapter;
        }

        TextView headerView = mCursorAdapter.getHeaderView();
        if (headerView == null) {
            headerView = (TextView) getInflater().inflate(R.layout.awesomebar_header_row, null);
            mCursorAdapter.setHeaderView(headerView);
        }

        // Add/Remove header based on the root folder
        if (mView != null) {
            ListView list = (ListView)mView;
            if (mFolderId == Bookmarks.FIXED_ROOT_ID) {
                if (list.getHeaderViewsCount() == 1) {
                    list.removeHeaderView(headerView);
                }
            } else {
                if (list.getHeaderViewsCount() == 0) {
                    list.addHeaderView(headerView, null, true);
                }
                headerView.setText(mFolderTitle);
            }
        }

        return mCursorAdapter;
    }

    protected BookmarksQueryTask getQueryTask() {
        if (mQueryTask == null) {
            mQueryTask = new BookmarksQueryTask();
        }
        return mQueryTask;
    }

    public void handleItemClick(AdapterView<?> parent, View view, int position, long id) {
        ListView list = (ListView)getView();
        if (list == null)
            return;

        int headerCount = list.getHeaderViewsCount();
        // If we tap on the header view, there's nothing to do
        if (headerCount == 1 && position == 0)
            return;

        BookmarksListAdapter adapter = getCursorAdapter();
        if (adapter == null)
            return;

        Cursor cursor = adapter.getCursor();
        if (cursor == null)
            return;

        // The header view takes up a spot in the list
        if (headerCount == 1)
            position--;

        cursor.moveToPosition(position);

        int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
        if (type == Bookmarks.TYPE_FOLDER) {
            // If we're clicking on a folder, update adapter to move to that folder
            int folderId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            String folderTitle = adapter.getFolderTitle(position);

            adapter.moveToChildFolder(folderId, folderTitle);
            return;
        }

        // Otherwise, just open the URL
        AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
        if (listener == null) {
            return;
        }

        String url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
        String title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
        long parentId = cursor.getLong(cursor.getColumnIndexOrThrow(Bookmarks.PARENT));
        if (parentId == Bookmarks.FIXED_READING_LIST_ID) {
            url = ReaderModeUtils.getAboutReaderForUrl(url, true);
        }
        listener.onUrlOpen(url, title);
    }

    private class BookmarksListAdapter extends SimpleCursorAdapter {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_FOLDER = 1;
        private static final int VIEW_TYPE_COUNT = 2;

        private LinkedList<Pair<Integer, String>> mParentStack;
        private TextView mBookmarksTitleView;

        public BookmarksListAdapter(Context context, Cursor c) {
            super(context, -1, c, new String[] {}, new int[] {});

            // mParentStack holds folder id/title pairs that allow us to navigate
            // back up the folder heirarchy
            mParentStack = new LinkedList<Pair<Integer, String>>();

            // Add the root folder to the stack
            Pair<Integer, String> rootFolder = new Pair<Integer, String>(Bookmarks.FIXED_ROOT_ID, "");
            mParentStack.addFirst(rootFolder);
        }

        public void refreshCurrentFolder() {
            // Cancel any pre-existing async refresh tasks
            if (mQueryTask != null)
                mQueryTask.cancel(false);

            Pair<Integer, String> folderPair = mParentStack.getFirst();
            mQueryTask = new BookmarksQueryTask(folderPair.first, folderPair.second);
            mQueryTask.execute();
        }

        // Returns false if there is no parent folder to move to
        public boolean moveToParentFolder() {
            // If we're already at the root, we can't move to a parent folder
            if (mParentStack.size() == 1)
                return false;

            mParentStack.removeFirst();
            refreshCurrentFolder();
            return true;
        }

        public void moveToChildFolder(int folderId, String folderTitle) {
            Pair<Integer, String> folderPair = new Pair<Integer, String>(folderId, folderTitle);
            mParentStack.addFirst(folderPair);
            refreshCurrentFolder();
        }

        public boolean isInReadingList() {
            Pair<Integer, String> folderPair = mParentStack.getFirst();
            return (folderPair.first == Bookmarks.FIXED_READING_LIST_ID);
        }

        @Override
        public int getItemViewType(int position) {
            Cursor c = getCursor();
 
            if (c.moveToPosition(position) &&
                c.getInt(c.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER)
                return VIEW_TYPE_FOLDER;

            // Default to retuning normal item type
            return VIEW_TYPE_ITEM;
        }
 
        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        public String getFolderTitle(int position) {
            Cursor c = getCursor();
            if (!c.moveToPosition(position))
                return "";

            String guid = c.getString(c.getColumnIndexOrThrow(Bookmarks.GUID));

            // If we don't have a special GUID, just return the folder title from the DB.
            if (guid == null || guid.length() == 12)
                return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));

            // Use localized strings for special folder names.
            if (guid.equals(Bookmarks.FAKE_DESKTOP_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_desktop);
            else if (guid.equals(Bookmarks.MENU_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_menu);
            else if (guid.equals(Bookmarks.TOOLBAR_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_toolbar);
            else if (guid.equals(Bookmarks.UNFILED_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_unfiled);
            else if (guid.equals(Bookmarks.READING_LIST_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_reading_list);

            // If for some reason we have a folder with a special GUID, but it's not one of
            // the special folders we expect in the UI, just return the title from the DB.
            return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            int viewType = getItemViewType(position);
            AwesomeEntryViewHolder viewHolder = null;

            if (convertView == null) {
                if (viewType == VIEW_TYPE_ITEM)
                    convertView = getInflater().inflate(R.layout.awesomebar_row, null);
                else
                    convertView = getInflater().inflate(R.layout.awesomebar_folder_row, null);

                viewHolder = new AwesomeEntryViewHolder();
                viewHolder.titleView = (TextView) convertView.findViewById(R.id.title);
                viewHolder.faviconView = (ImageView) convertView.findViewById(R.id.favicon);

                if (viewType == VIEW_TYPE_ITEM)
                    viewHolder.urlView = (TextView) convertView.findViewById(R.id.url);

                convertView.setTag(viewHolder);
            } else {
                viewHolder = (AwesomeEntryViewHolder) convertView.getTag();
            }

            Cursor cursor = getCursor();
            if (!cursor.moveToPosition(position))
                throw new IllegalStateException("Couldn't move cursor to position " + position);

            if (viewType == VIEW_TYPE_ITEM) {
                updateTitle(viewHolder.titleView, cursor);
                updateUrl(viewHolder.urlView, cursor);
                updateFavicon(viewHolder.faviconView, cursor);
            } else {
                viewHolder.titleView.setText(getFolderTitle(position));
            }

            return convertView;
        }

        public TextView getHeaderView() {
            return mBookmarksTitleView;
        }

        public void setHeaderView(TextView titleView) {
            mBookmarksTitleView = titleView;
        }
    }

    private class BookmarksQueryTask extends AsyncTask<Void, Void, Cursor> {
        public BookmarksQueryTask() {
            mFolderId = Bookmarks.FIXED_ROOT_ID;
            mFolderTitle = "";
        }

        public BookmarksQueryTask(int folderId, String folderTitle) {
            mFolderId = folderId;
            mFolderTitle = folderTitle;
        }

        @Override
        protected Cursor doInBackground(Void... arg0) {
            return BrowserDB.getBookmarksInFolder(getContentResolver(), mFolderId);
        }

        @Override
        protected void onPostExecute(final Cursor cursor) {
            // Hack: force this to the main thread, even though it should already be on it
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    // this will update the cursorAdapter to use the new one if it already exists
                    // We need to add the header before we set the adapter, hence make it null
                    ListView list = (ListView)mView;
                    list.setAdapter(null);
                    list.setAdapter(getCursorAdapter(cursor));
                }
            });
            mQueryTask = null;
        }
    }

    public boolean moveToParentFolder() {
        // If we're not in the bookmarks tab, we have nothing to do. We should
        // also return false if mBookmarksAdapter hasn't been initialized yet.
        BookmarksListAdapter adapter = getCursorAdapter();
        if (adapter == null)
            return false;

        return adapter.moveToParentFolder();
    }

    /**
     * Whether the user is in the Reading List bookmarks directory in the
     * AwesomeScreen UI.
     */
    public boolean isInReadingList() {
        if (mCursorAdapter == null)
            return false;

        return mCursorAdapter.isInReadingList();
    }

    @Override
    public ContextMenuSubject getSubject(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        ContextMenuSubject subject = null;

        if (!(menuInfo instanceof AdapterView.AdapterContextMenuInfo)) {
            Log.e(LOGTAG, "menuInfo is not AdapterContextMenuInfo");
            return subject;
        }

        ListView list = (ListView)view;
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
        Object selectedItem = list.getItemAtPosition(info.position);

        if (!(selectedItem instanceof Cursor)) {
            Log.e(LOGTAG, "item at " + info.position + " is not a Cursor");
            return subject;
        }

        Cursor cursor = (Cursor) selectedItem;

        // Don't show the context menu for folders
        if (!(cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER)) {
            String keyword = null;
            int keywordCol = cursor.getColumnIndex(URLColumns.KEYWORD);
            if (keywordCol != -1)
                keyword = cursor.getString(keywordCol);

            int id = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));

            subject = new ContextMenuSubject(id,
                                            cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL)),
                                            cursor.getBlob(cursor.getColumnIndexOrThrow(URLColumns.FAVICON)),
                                            cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE)),
                                            keyword,
                                            isInReadingList() ? Combined.DISPLAY_READER : Combined.DISPLAY_NORMAL);
        }

        if (subject == null)
            return subject;

        MenuInflater inflater = new MenuInflater(mContext);
        inflater.inflate(R.menu.awesomebar_contextmenu, menu);
        
        menu.findItem(R.id.remove_history).setVisible(false);
        menu.findItem(R.id.open_in_reader).setVisible(false);
        menu.setHeaderTitle(subject.title);

        return subject;
    }
}
