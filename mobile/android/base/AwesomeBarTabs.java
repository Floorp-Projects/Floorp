/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.LightingColorFilter;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.ExpandableListView;
import android.widget.FilterQueryProvider;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.widget.SimpleExpandableListAdapter;
import android.widget.TabHost;
import android.widget.TextView;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

public class AwesomeBarTabs extends TabHost {
    private static final String LOGTAG = "GeckoAwesomeBarTabs";

    private static final String ALL_PAGES_TAB = "all";
    private static final String BOOKMARKS_TAB = "bookmarks";
    private static final String HISTORY_TAB = "history";

    private static enum HistorySection { TODAY, YESTERDAY, WEEK, OLDER };

    private Context mContext;
    private boolean mInflated;
    private LayoutInflater mInflater;
    private OnUrlOpenListener mUrlOpenListener;
    private View.OnTouchListener mListTouchListener;
    private ContentResolver mContentResolver;
    private ContentObserver mContentObserver;
    private SearchEngine mSuggestEngine;
    private ArrayList<SearchEngine> mSearchEngines;

    private BookmarksQueryTask mBookmarksQueryTask;
    private HistoryQueryTask mHistoryQueryTask;
    
    private AwesomeBarCursorAdapter mAllPagesCursorAdapter;
    private BookmarksListAdapter mBookmarksAdapter;
    private SimpleExpandableListAdapter mHistoryAdapter;

    private boolean mInReadingList;

    // FIXME: This value should probably come from a
    // prefs key (just like XUL-based fennec)
    private static final int MAX_RESULTS = 100;

    public interface OnUrlOpenListener {
        public void onUrlOpen(String url);
        public void onSearch(String engine, String text);
        public void onEditSuggestion(String suggestion);
    }

    private class AwesomeEntryViewHolder {
        public TextView titleView;
        public TextView urlView;
        public ImageView faviconView;
        public ImageView bookmarkIconView;
    }

    private class SearchEntryViewHolder {
        public FlowLayout suggestionView;
        public ImageView iconView;
        public LinearLayout userEnteredView;
        public TextView userEnteredTextView;
    }

    private class HistoryListAdapter extends SimpleExpandableListAdapter {
        public HistoryListAdapter(Context context, List<? extends Map<String, ?>> groupData,
                int groupLayout, String[] groupFrom, int[] groupTo,
                List<? extends List<? extends Map<String, ?>>> childData) {

            super(context, groupData, groupLayout, groupFrom, groupTo,
                  childData, -1, new String[] {}, new int[] {});
        }

        @Override
        public View getChildView(int groupPosition, int childPosition, boolean isLastChild,
                View convertView, ViewGroup parent) {
            AwesomeEntryViewHolder viewHolder = null;

            if (convertView == null) {
                convertView = mInflater.inflate(R.layout.awesomebar_row, null);

                viewHolder = new AwesomeEntryViewHolder();
                viewHolder.titleView = (TextView) convertView.findViewById(R.id.title);
                viewHolder.urlView = (TextView) convertView.findViewById(R.id.url);
                viewHolder.faviconView = (ImageView) convertView.findViewById(R.id.favicon);
                viewHolder.bookmarkIconView = (ImageView) convertView.findViewById(R.id.bookmark_icon);

                convertView.setTag(viewHolder);
            } else {
                viewHolder = (AwesomeEntryViewHolder) convertView.getTag();
            }

            @SuppressWarnings("unchecked")
            Map<String,Object> historyItem =
                    (Map<String,Object>) mHistoryAdapter.getChild(groupPosition, childPosition);

            String title = (String) historyItem.get(URLColumns.TITLE);
            String url = (String) historyItem.get(URLColumns.URL);

            if (TextUtils.isEmpty(title))
                title = url;

            viewHolder.titleView.setText(title);
            viewHolder.urlView.setText(url);

            byte[] b = (byte[]) historyItem.get(URLColumns.FAVICON);

            if (b == null) {
                viewHolder.faviconView.setImageDrawable(null);
            } else {
                Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
                viewHolder.faviconView.setImageBitmap(bitmap);
            }

            Integer bookmarkId = (Integer) historyItem.get(Combined.BOOKMARK_ID);
            Integer display = (Integer) historyItem.get(Combined.DISPLAY);

            // The bookmark id will be 0 (null in database) when the url
            // is not a bookmark. Reading list items are irrelevant in history
            // tab. We should never show any sign or them.
            int visibility = (bookmarkId != 0 && display != Combined.DISPLAY_READER ?
                              View.VISIBLE : View.GONE);

            viewHolder.bookmarkIconView.setVisibility(visibility);
            viewHolder.bookmarkIconView.setImageResource(R.drawable.ic_awesomebar_star);

            return convertView;
        }
    }

    private class BookmarksListAdapter extends SimpleCursorAdapter {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_FOLDER = 1;
        private static final int VIEW_TYPE_COUNT = 2;

        private Resources mResources;
        private LinkedList<Pair<Integer, String>> mParentStack;
        private LinearLayout mBookmarksTitleView;

        public BookmarksListAdapter(Context context, Cursor c) {
            super(context, -1, c, new String[] {}, new int[] {});

            mResources = mContext.getResources();

            // mParentStack holds folder id/title pairs that allow us to navigate
            // back up the folder heirarchy
            mParentStack = new LinkedList<Pair<Integer, String>>();

            // Add the root folder to the stack
            Pair<Integer, String> rootFolder = new Pair<Integer, String>(Bookmarks.FIXED_ROOT_ID, "");
            mParentStack.addFirst(rootFolder);
        }

        public void refreshCurrentFolder() {
            // Cancel any pre-existing async refresh tasks
            if (mBookmarksQueryTask != null)
                mBookmarksQueryTask.cancel(false);

            Pair<Integer, String> folderPair = mParentStack.getFirst();
            mInReadingList = (folderPair.first == Bookmarks.FIXED_READING_LIST_ID);

            mBookmarksQueryTask = new BookmarksQueryTask(folderPair.first, folderPair.second);
            mBookmarksQueryTask.execute();
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
                return mResources.getString(R.string.bookmarks_folder_desktop);
            else if (guid.equals(Bookmarks.MENU_FOLDER_GUID))
                return mResources.getString(R.string.bookmarks_folder_menu);
            else if (guid.equals(Bookmarks.TOOLBAR_FOLDER_GUID))
                return mResources.getString(R.string.bookmarks_folder_toolbar);
            else if (guid.equals(Bookmarks.UNFILED_FOLDER_GUID))
                return mResources.getString(R.string.bookmarks_folder_unfiled);
            else if (guid.equals(Bookmarks.READING_LIST_FOLDER_GUID))
                return mResources.getString(R.string.bookmarks_folder_reading_list);

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
                    convertView = mInflater.inflate(R.layout.awesomebar_row, null);
                else
                    convertView = mInflater.inflate(R.layout.awesomebar_folder_row, null);

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
                int guidIndex = cursor.getColumnIndexOrThrow(Bookmarks.GUID);
                String guid = cursor.getString(guidIndex);

                if (guid.equals(Bookmarks.READING_LIST_FOLDER_GUID)) {
                    viewHolder.faviconView.setImageResource(R.drawable.reading_list);
                } else {
                    viewHolder.faviconView.setImageResource(R.drawable.folder);
                }

                viewHolder.titleView.setText(getFolderTitle(position));
            }

            return convertView;
        }

        public LinearLayout getHeaderView() {
            return mBookmarksTitleView;
        }

        public void setHeaderView(LinearLayout titleView) {
            mBookmarksTitleView = titleView;
        }
    }

    // This method checks to see if we're in a bookmark sub-folder. If we are,
    // it will go up a level and return true. Otherwise it will return false.
    public boolean onBackPressed() {
        // If the soft keyboard is visible in the bookmarks or history tab, the user
        // must have explictly brought it up, so we should try hiding it instead of
        // exiting the activity or going up a bookmarks folder level.
        if (getCurrentTabTag().equals(BOOKMARKS_TAB) || getCurrentTabTag().equals(HISTORY_TAB)) {
            View tabView = getCurrentTabView();
            if (hideSoftInput(tabView))
                return true;
        }

        // If we're not in the bookmarks tab, we have nothing to do. We should
        // also return false if mBookmarksAdapter hasn't been initialized yet.
        if (!getCurrentTabTag().equals(BOOKMARKS_TAB) || mBookmarksAdapter == null)
            return false;

        return mBookmarksAdapter.moveToParentFolder();
    }
     
    private class BookmarksQueryTask extends AsyncTask<Void, Void, Cursor> {
        private int mFolderId;
        private String mFolderTitle;

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
            return BrowserDB.getBookmarksInFolder(mContentResolver, mFolderId);
        }

        @Override
        protected void onPostExecute(final Cursor cursor) {
            final ListView list = (ListView) findViewById(R.id.bookmarks_list);

            // Hack: force this to the main thread, even though it should already be on it
            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                            handleBookmarkItemClick(parent, view, position, id);
                        }
                    });
                    
                    // We need to add the header before we set the adapter, hence make it null
                    list.setAdapter(null);

                    if (mBookmarksAdapter == null) {
                        mBookmarksAdapter = new BookmarksListAdapter(mContext, cursor);
                    } else {
                        mBookmarksAdapter.changeCursor(cursor);
                    }

                    LinearLayout headerView = mBookmarksAdapter.getHeaderView();
                    if (headerView == null) {
                        headerView = (LinearLayout) mInflater.inflate(R.layout.awesomebar_header_row, null);
                        mBookmarksAdapter.setHeaderView(headerView);
                    }

                    // Add/Remove header based on the root folder
                    if (mFolderId == Bookmarks.FIXED_ROOT_ID) {
                        if (list.getHeaderViewsCount() == 1)
                            list.removeHeaderView(headerView);
                    } else {
                        if (list.getHeaderViewsCount() == 0)
                            list.addHeaderView(headerView, null, true);

                        ((TextView) headerView.findViewById(R.id.title)).setText(mFolderTitle);
                    }

                    list.setAdapter(mBookmarksAdapter);
                }
            });

            mBookmarksQueryTask = null;
        }
    }

    private static class GroupList extends LinkedList<Map<String,String>> {
        private static final long serialVersionUID = 0L;
    }

    private static class ChildrenList extends LinkedList<Map<String,Object>> {
        private static final long serialVersionUID = 0L;
    }

    private class HistoryQueryTask extends AsyncTask<Void, Void, Pair<GroupList,List<ChildrenList>>> {
        private static final long MS_PER_DAY = 86400000;
        private static final long MS_PER_WEEK = MS_PER_DAY * 7;

        protected Pair<GroupList,List<ChildrenList>> doInBackground(Void... arg0) {
            Cursor cursor = BrowserDB.getRecentHistory(mContentResolver, MAX_RESULTS);

            Date now = new Date();
            now.setHours(0);
            now.setMinutes(0);
            now.setSeconds(0);

            long today = now.getTime();

            // Split the list of urls into separate date range groups
            // and show it in an expandable list view.
            List<ChildrenList> childrenLists = new LinkedList<ChildrenList>();
            ChildrenList children = null;
            GroupList groups = new GroupList();
            HistorySection section = null;

            // Move cursor before the first row in preparation
            // for the iteration.
            cursor.moveToPosition(-1);

            // Split the history query results into adapters per time
            // section (today, yesterday, week, older). Queries on content
            // Browser content provider don't support limitting the number
            // of returned rows so we limit it here.
            while (cursor.moveToNext()) {
                long time = cursor.getLong(cursor.getColumnIndexOrThrow(URLColumns.DATE_LAST_VISITED));
                HistorySection itemSection = getSectionForTime(time, today);

                if (section != itemSection) {
                    if (section != null) {
                        groups.add(createGroupItem(section));
                        childrenLists.add(children);
                    }

                    section = itemSection;
                    children = new ChildrenList();
                }

                children.add(createHistoryItem(cursor));
            }

            // Add any remaining section to the list if it hasn't
            // been added to the list after the loop.
            if (section != null && children != null) {
                groups.add(createGroupItem(section));
                childrenLists.add(children);
            }

            // Close the query cursor as we won't use it anymore
            cursor.close();

            // groups and childrenLists will be empty lists if there's no history
            return Pair.<GroupList,List<ChildrenList>>create(groups, childrenLists);
        }

        public Map<String,Object> createHistoryItem(Cursor cursor) {
            Map<String,Object> historyItem = new HashMap<String,Object>();

            String url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
            String title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
            byte[] favicon = cursor.getBlob(cursor.getColumnIndexOrThrow(URLColumns.FAVICON));
            Integer bookmarkId = cursor.getInt(cursor.getColumnIndexOrThrow(Combined.BOOKMARK_ID));
            Integer historyId = cursor.getInt(cursor.getColumnIndexOrThrow(Combined.HISTORY_ID));
            Integer display = cursor.getInt(cursor.getColumnIndexOrThrow(Combined.DISPLAY));

            // Use the URL instead of an empty title for consistency with the normal URL
            // bar view - this is the equivalent of getDisplayTitle() in Tab.java
            if (title == null || title.length() == 0)
                title = url;

            historyItem.put(URLColumns.URL, url);
            historyItem.put(URLColumns.TITLE, title);

            if (favicon != null)
                historyItem.put(URLColumns.FAVICON, favicon);

            historyItem.put(Combined.BOOKMARK_ID, bookmarkId);
            historyItem.put(Combined.HISTORY_ID, historyId);
            historyItem.put(Combined.DISPLAY, display);

            return historyItem;
        }

        public Map<String,String> createGroupItem(HistorySection section) {
            Map<String,String> groupItem = new HashMap<String,String>();

            groupItem.put(URLColumns.TITLE, getSectionName(section));

            return groupItem;
        }

        private String getSectionName(HistorySection section) {
            Resources resources = mContext.getResources();

            switch (section) {
            case TODAY:
                return resources.getString(R.string.history_today_section);
            case YESTERDAY:
                return resources.getString(R.string.history_yesterday_section);
            case WEEK:
                return resources.getString(R.string.history_week_section);
            case OLDER:
                return resources.getString(R.string.history_older_section);
            }

            return null;
        }

        private void expandAllGroups(ExpandableListView historyList) {
            int groupCount = mHistoryAdapter.getGroupCount();

            for (int i = 0; i < groupCount; i++) {
                historyList.expandGroup(i);
            }
        }

        private HistorySection getSectionForTime(long time, long today) {
            long delta = today - time;

            if (delta < 0) {
                return HistorySection.TODAY;
            }

            if (delta < MS_PER_DAY) {
                return HistorySection.YESTERDAY;
            }

            if (delta < MS_PER_WEEK) {
                return HistorySection.WEEK;
            }

            return HistorySection.OLDER;
        }

        protected void onPostExecute(Pair<GroupList,List<ChildrenList>> result) {
            mHistoryAdapter = new HistoryListAdapter(
                mContext,
                result.first,
                R.layout.awesomebar_header_row,
                new String[] { URLColumns.TITLE },
                new int[] { R.id.title },
                result.second
            );

            if (mContentObserver == null) {
                // Register an observer to update the history tab contents if they change.
                mContentObserver = new ContentObserver(GeckoAppShell.getHandler()) {
                    public void onChange(boolean selfChange) {
                        mHistoryQueryTask = new HistoryQueryTask();
                        mHistoryQueryTask.execute();
                    }
                };
                BrowserDB.registerHistoryObserver(mContentResolver, mContentObserver);
            }

            final ExpandableListView historyList =
                    (ExpandableListView) findViewById(R.id.history_list);

            // Hack: force this to the main thread, even though it should already be on it
            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    historyList.setOnChildClickListener(new ExpandableListView.OnChildClickListener() {
                        public boolean onChildClick(ExpandableListView parent, View view,
                                int groupPosition, int childPosition, long id) {
                            handleHistoryItemClick(groupPosition, childPosition);
                            return true;
                        }
                    });

                    // This is to disallow collapsing the expandable groups in the
                    // history expandable list view to mimic simpler sections. We should
                    // Remove this if we decide to allow expanding/collapsing groups.
                    historyList.setOnGroupClickListener(new ExpandableListView.OnGroupClickListener() {
                        public boolean onGroupClick(ExpandableListView parent, View v,
                                int groupPosition, long id) {
                            return true;
                        }
                    });

                    historyList.setAdapter(mHistoryAdapter);
                    expandAllGroups(historyList);
                }
            });

            mHistoryQueryTask = null;
        }
    }

    private interface AwesomeBarItem {
        public void onClick();
    }

    private class AwesomeBarCursorAdapter extends SimpleCursorAdapter {
        private String mSearchTerm;

        private static final int ROW_SEARCH = 0;
        private static final int ROW_STANDARD = 1;

        private class AwesomeBarCursorItem implements AwesomeBarItem {
            private Cursor mCursor;

            public AwesomeBarCursorItem(Cursor cursor) {
                mCursor = cursor;
            }

            public void onClick() {
                String url = mCursor.getString(mCursor.getColumnIndexOrThrow(URLColumns.URL));
                if (mUrlOpenListener != null) {
                    int display = mCursor.getInt(mCursor.getColumnIndexOrThrow(Combined.DISPLAY));
                    if (display == Combined.DISPLAY_READER) {
                        url = getReaderForUrl(url);
                    }

                    mUrlOpenListener.onUrlOpen(url);
                }
            }
        }

        private class AwesomeBarSearchEngineItem implements AwesomeBarItem {
            private String mSearchEngine;

            public AwesomeBarSearchEngineItem(String searchEngine) {
                mSearchEngine = searchEngine;
            }

            public void onClick() {
                if (mUrlOpenListener != null)
                    mUrlOpenListener.onSearch(mSearchEngine, mSearchTerm);
            }
        }

        public AwesomeBarCursorAdapter(Context context) {
            super(context, -1, null, new String[] {}, new int[] {});
            mSearchTerm = "";
        }

        public void filter(String searchTerm) {
            mSearchTerm = searchTerm;
            getFilter().filter(searchTerm);
        }

        private int getSuggestEngineCount() {
            return (mSearchTerm.length() == 0 || mSuggestEngine == null) ? 0 : 1;
        }

        // Add the search engines to the number of reported results.
        @Override
        public int getCount() {
            final int resultCount = super.getCount();

            // don't show search engines or suggestions if search field is empty
            if (mSearchTerm.length() == 0)
                return resultCount;

            return resultCount + mSearchEngines.size() + getSuggestEngineCount();
        }

        // If an item is part of the cursor result set, return that entry.
        // Otherwise, return the search engine data.
        @Override
        public Object getItem(int position) {
            int engineIndex = getEngineIndex(position);

            if (engineIndex == -1) {
                // return awesomebar result
                position -= getSuggestEngineCount();
                return new AwesomeBarCursorItem((Cursor) super.getItem(position));
            }

            // return search engine
            return new AwesomeBarSearchEngineItem(getEngine(engineIndex).name);
        }

        private SearchEngine getEngine(int index) {
            final int suggestEngineCount = getSuggestEngineCount();
            if (index < suggestEngineCount)
                return mSuggestEngine;
            return mSearchEngines.get(index - suggestEngineCount);
        }

        private int getEngineIndex(int position) {
            final int resultCount = super.getCount();
            final int suggestEngineCount = getSuggestEngineCount();

            // return suggest engine index
            if (position < suggestEngineCount)
                return 0;

            // not an engine
            if (position - suggestEngineCount < resultCount)
                return -1;

            // return search engine index
            return position - resultCount;
        }

        @Override
        public int getItemViewType(int position) {
            return getEngineIndex(position) == -1 ? ROW_STANDARD : ROW_SEARCH;
        }

        @Override
        public int getViewTypeCount() {
            // view can be either a standard awesomebar row or a search engine row
            return 2;
        }

        @Override
        public boolean isEnabled(int position) {
            // If the suggestion row only contains one item (the user-entered
            // query), allow the entire row to be clickable; clicking the row
            // has the same effect as clicking the single suggestion. If the
            // row contains multiple items, clicking the row will do nothing.
            int index = getEngineIndex(position);
            if (index != -1) {
                return getEngine(index).suggestions.isEmpty();
            }
            return true;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (getItemViewType(position) == ROW_SEARCH) {
                SearchEntryViewHolder viewHolder = null;

                if (convertView == null) {
                    convertView = mInflater.inflate(R.layout.awesomebar_suggestion_row, null);

                    viewHolder = new SearchEntryViewHolder();
                    viewHolder.suggestionView = (FlowLayout) convertView.findViewById(R.id.suggestion_layout);
                    viewHolder.iconView = (ImageView) convertView.findViewById(R.id.suggestion_icon);
                    viewHolder.userEnteredView = (LinearLayout) convertView.findViewById(R.id.suggestion_user_entered);
                    viewHolder.userEnteredTextView = (TextView) convertView.findViewById(R.id.suggestion_text);

                    convertView.setTag(viewHolder);
                } else {
                    viewHolder = (SearchEntryViewHolder) convertView.getTag();
                }

                bindSearchEngineView(getEngine(getEngineIndex(position)), viewHolder);
            } else {
                AwesomeEntryViewHolder viewHolder = null;

                if (convertView == null) {
                    convertView = mInflater.inflate(R.layout.awesomebar_row, null);

                    viewHolder = new AwesomeEntryViewHolder();
                    viewHolder.titleView = (TextView) convertView.findViewById(R.id.title);
                    viewHolder.urlView = (TextView) convertView.findViewById(R.id.url);
                    viewHolder.faviconView = (ImageView) convertView.findViewById(R.id.favicon);
                    viewHolder.bookmarkIconView = (ImageView) convertView.findViewById(R.id.bookmark_icon);

                    convertView.setTag(viewHolder);
                } else {
                    viewHolder = (AwesomeEntryViewHolder) convertView.getTag();
                }

                position -= getSuggestEngineCount();
                Cursor cursor = getCursor();
                if (!cursor.moveToPosition(position))
                    throw new IllegalStateException("Couldn't move cursor to position " + position);

                updateTitle(viewHolder.titleView, cursor);
                updateUrl(viewHolder.urlView, cursor);
                updateFavicon(viewHolder.faviconView, cursor);
                updateBookmarkIcon(viewHolder.bookmarkIconView, cursor);
            }

            return convertView;
        }

        private void bindSearchEngineView(final SearchEngine engine, SearchEntryViewHolder viewHolder) {
            // when a suggestion is clicked, do a search
            OnClickListener clickListener = new OnClickListener() {
                public void onClick(View v) {
                    if (mUrlOpenListener != null) {
                        String suggestion = ((TextView) v.findViewById(R.id.suggestion_text)).getText().toString();
                        mUrlOpenListener.onSearch(engine.name, suggestion);
                    }
                }
            };

            // when a suggestion is long-clicked, copy the suggestion into the URL EditText
            OnLongClickListener longClickListener = new OnLongClickListener() {
                public boolean onLongClick(View v) {
                    if (mUrlOpenListener != null) {
                        String suggestion = ((TextView) v.findViewById(R.id.suggestion_text)).getText().toString();
                        mUrlOpenListener.onEditSuggestion(suggestion);
                        return true;
                    }
                    return false;
                }
            };

            // set the search engine icon (e.g., Google) for the row
            FlowLayout suggestionView = viewHolder.suggestionView;
            viewHolder.iconView.setImageDrawable(engine.icon);

            // user-entered search term is first suggestion
            viewHolder.userEnteredTextView.setText(mSearchTerm);
            viewHolder.userEnteredView.setOnClickListener(clickListener);
            
            // add additional suggestions given by this engine
            int recycledSuggestionCount = suggestionView.getChildCount();
            int suggestionCount = engine.suggestions.size();
            int i = 0;
            for (i = 0; i < suggestionCount; i++) {
                String suggestion = engine.suggestions.get(i);
                View suggestionItem = null;

                // reuse suggestion views from recycled view, if possible
                if (i+1 < recycledSuggestionCount) {
                    suggestionItem = suggestionView.getChildAt(i+1);
                    suggestionItem.setVisibility(View.VISIBLE);
                } else {
                    suggestionItem = mInflater.inflate(R.layout.awesomebar_suggestion_item, null);
                    ((ImageView) suggestionItem.findViewById(R.id.suggestion_magnifier)).setVisibility(View.GONE);
                    suggestionView.addView(suggestionItem);
                }
                ((TextView) suggestionItem.findViewById(R.id.suggestion_text)).setText(suggestion);

                suggestionItem.setOnClickListener(clickListener);
                suggestionItem.setOnLongClickListener(longClickListener);
            }
            
            // hide extra suggestions that have been recycled
            for (++i; i < recycledSuggestionCount; i++) {
                suggestionView.getChildAt(i).setVisibility(View.GONE);
            }
        }
    };

    public AwesomeBarTabs(Context context, AttributeSet attrs) {
        super(context, attrs);

        Log.d(LOGTAG, "Creating AwesomeBarTabs");

        mContext = context;
        mInflated = false;
        mSearchEngines = new ArrayList<SearchEngine>();
        mContentResolver = context.getContentResolver();
        mContentObserver = null;
        mInflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        mInReadingList = false;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // HACK: Without this, the onFinishInflate is called twice
        // This issue is due to a bug when Android inflates a layout with a
        // parent. Fixed in Honeycomb
        if (mInflated)
            return;

        mInflated = true;

        // This should be called before adding any tabs
        // to the TabHost.
        setup();

        mListTouchListener = new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN)
                    hideSoftInput(view);
                return false;
            }
        };

        addAllPagesTab();
        addBookmarksTab();
        addHistoryTab();

        setOnTabChangedListener(new TabHost.OnTabChangeListener() {
            public void onTabChanged(String tabId) {
                boolean hideSoftInput = true;

                // Lazy load bookmarks and history lists. Only query the database
                // if those lists requested by user.
                if (tabId.equals(BOOKMARKS_TAB) && mBookmarksAdapter == null
                        && mBookmarksQueryTask == null) {
                    mBookmarksQueryTask = new BookmarksQueryTask();
                    mBookmarksQueryTask.execute();
                } else if (tabId.equals(HISTORY_TAB) && mHistoryAdapter == null
                        && mHistoryQueryTask == null) {
                    mHistoryQueryTask = new HistoryQueryTask();
                    mHistoryQueryTask.execute();
                } else {
                    hideSoftInput = false;
                }

                // Always dismiss SKB when changing to lazy-loaded tabs
                if (hideSoftInput) {
                    View tabView = getCurrentTabView();
                    hideSoftInput(tabView);
                }
            }
        });

        // Initialize "App Pages" list with no filter
        filter("");
    }

    private TabSpec addAwesomeTab(String id, int titleId, int contentId) {
        TabSpec tab = newTabSpec(id);

        View indicatorView = mInflater.inflate(R.layout.awesomebar_tab_indicator, null);
        Drawable background = indicatorView.getBackground();
        try {
            background.setColorFilter(new LightingColorFilter(Color.WHITE, 0xFFFF9500));
        } catch (Exception e) {
            Log.d(LOGTAG, "background.setColorFilter failed " + e);            
        }
        TextView title = (TextView) indicatorView.findViewById(R.id.title);
        title.setText(titleId);

        tab.setIndicator(indicatorView);
        tab.setContent(contentId);

        addTab(tab);

        return tab;
    }

    private void addAllPagesTab() {
        Log.d(LOGTAG, "Creating All Pages tab");

        addAwesomeTab(ALL_PAGES_TAB,
                      R.string.awesomebar_all_pages_title,
                      R.id.all_pages_list);

        // Load the list using a custom adapter so we can create the bitmaps
        mAllPagesCursorAdapter = new AwesomeBarCursorAdapter(mContext);

        mAllPagesCursorAdapter.setFilterQueryProvider(new FilterQueryProvider() {
            public Cursor runQuery(CharSequence constraint) {
                long start = SystemClock.uptimeMillis();

                Cursor c = BrowserDB.filter(mContentResolver, constraint, MAX_RESULTS);
                c.getCount(); // ensure the query runs at least once

                long end = SystemClock.uptimeMillis();
                Log.i(LOGTAG, "Got cursor in " + (end - start) + "ms");

                return c;
            }
        });

        final ListView allPagesList = (ListView) findViewById(R.id.all_pages_list);

        allPagesList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                ((AwesomeBarItem) allPagesList.getItemAtPosition(position)).onClick();
            }
        });

        allPagesList.setAdapter(mAllPagesCursorAdapter);
        allPagesList.setOnTouchListener(mListTouchListener);
    }

    private void addBookmarksTab() {
        Log.d(LOGTAG, "Creating Bookmarks tab");

        addAwesomeTab(BOOKMARKS_TAB,
                      R.string.awesomebar_bookmarks_title,
                      R.id.bookmarks_list);

        ListView bookmarksList = (ListView) findViewById(R.id.bookmarks_list);
        bookmarksList.setOnTouchListener(mListTouchListener);

        // Only load bookmark list when tab is actually used.
        // See OnTabChangeListener above.
    }

    private void addHistoryTab() {
        Log.d(LOGTAG, "Creating History tab");

        addAwesomeTab(HISTORY_TAB,
                      R.string.awesomebar_history_title,
                      R.id.history_list);

        ListView historyList = (ListView) findViewById(R.id.history_list);
        historyList.setOnTouchListener(mListTouchListener);

        // Only load history list when tab is actually used.
        // See OnTabChangeListener above.
    }

    private boolean hideSoftInput(View view) {
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);

        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    private String getReaderForUrl(String url) {
        // FIXME: still need to define the final way to open items from
        // reading list. For now, we're using an about:reader page.
        return "about:reader?url=" + url;
    }

    private void handleBookmarkItemClick(AdapterView<?> parent, View view, int position, long id) {
        int headerCount = ((ListView) parent).getHeaderViewsCount();
        // If we tap on the header view, there's nothing to do
        if (headerCount == 1 && position == 0)
            return;

        Cursor cursor = mBookmarksAdapter.getCursor();
        // The header view takes up a spot in the list
        if (headerCount == 1)
            position--;

        cursor.moveToPosition(position);

        int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
        if (type == Bookmarks.TYPE_FOLDER) {
            // If we're clicking on a folder, update mBookmarksAdapter to move to that folder
            int folderId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            String folderTitle = mBookmarksAdapter.getFolderTitle(position);

            mBookmarksAdapter.moveToChildFolder(folderId, folderTitle);
            return;
        }

        // Otherwise, just open the URL
        String url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
        if (mUrlOpenListener != null) {
            if (mInReadingList) {
                url = getReaderForUrl(url);
            }

            mUrlOpenListener.onUrlOpen(url);
        }
    }

    private void handleHistoryItemClick(int groupPosition, int childPosition) {
        @SuppressWarnings("unchecked")
        Map<String,Object> historyItem =
                (Map<String,Object>) mHistoryAdapter.getChild(groupPosition, childPosition);

        String url = (String) historyItem.get(URLColumns.URL);

        if (mUrlOpenListener != null)
            mUrlOpenListener.onUrlOpen(url);
    }

    private void updateFavicon(ImageView faviconView, Cursor cursor) {
        byte[] b = cursor.getBlob(cursor.getColumnIndexOrThrow(URLColumns.FAVICON));
        if (b == null) {
            faviconView.setImageDrawable(null);
        } else {
            Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);
            faviconView.setImageBitmap(bitmap);
        }
    }

    private void updateTitle(TextView titleView, Cursor cursor) {
        int titleIndex = cursor.getColumnIndexOrThrow(URLColumns.TITLE);
        String title = cursor.getString(titleIndex);

        // Use the URL instead of an empty title for consistency with the normal URL
        // bar view - this is the equivalent of getDisplayTitle() in Tab.java
        if (TextUtils.isEmpty(title)) {
            int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
            title = cursor.getString(urlIndex);
        }

        titleView.setText(title);
    }

    private void updateUrl(TextView urlView, Cursor cursor) {
        int urlIndex = cursor.getColumnIndexOrThrow(URLColumns.URL);
        String url = cursor.getString(urlIndex);

        urlView.setText(url);
    }

    private void updateBookmarkIcon(ImageView bookmarkIconView, Cursor cursor) {
        int bookmarkIdIndex = cursor.getColumnIndexOrThrow(Combined.BOOKMARK_ID);
        long id = cursor.getLong(bookmarkIdIndex);

        int displayIndex = cursor.getColumnIndexOrThrow(Combined.DISPLAY);
        int display = cursor.getInt(displayIndex);

        // The bookmark id will be 0 (null in database) when the url
        // is not a bookmark.
        int visibility = (id == 0 ? View.GONE : View.VISIBLE);
        bookmarkIconView.setVisibility(visibility);

        if (display == Combined.DISPLAY_READER) {
            bookmarkIconView.setImageResource(R.drawable.ic_awesomebar_reader);
        } else {
            bookmarkIconView.setImageResource(R.drawable.ic_awesomebar_star);
        }
    }

    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
    }

    public void destroy() {
        Cursor allPagesCursor = mAllPagesCursorAdapter.getCursor();
        if (allPagesCursor != null)
            allPagesCursor.close();

        if (mBookmarksAdapter != null) {
            Cursor bookmarksCursor = mBookmarksAdapter.getCursor();
            if (bookmarksCursor != null)
                bookmarksCursor.close();
        }

        if (mContentObserver != null)
            BrowserDB.unregisterContentObserver(mContentResolver, mContentObserver);
    }

    public void filter(String searchTerm) {
        // Don't let the tab's content steal focus on tab switch
        setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);

        // Ensure the 'All Pages' tab is selected
        setCurrentTabByTag(ALL_PAGES_TAB);

        // Restore normal focus behavior on tab host
        setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);

        // The tabs should only be visible if there's no on-going search
        int tabsVisibility = (searchTerm.length() == 0 ? View.VISIBLE : View.GONE);
        getTabWidget().setVisibility(tabsVisibility);

        // Perform the actual search
        mAllPagesCursorAdapter.filter(searchTerm);
    }

    private Drawable getDrawableFromDataURI(String dataURI) {
        String base64 = dataURI.substring(dataURI.indexOf(',') + 1);
        Drawable drawable = null;
        try {
            byte[] bytes = GeckoAppShell.decodeBase64(base64, GeckoAppShell.BASE64_DEFAULT);
            ByteArrayInputStream stream = new ByteArrayInputStream(bytes);
            drawable = Drawable.createFromStream(stream, "src");
            stream.close();
        } catch (IllegalArgumentException e) {
            Log.i(LOGTAG, "exception while decoding drawable: " + base64, e);
        } catch (IOException e) { }
        return drawable;
    }

    private class SearchEngine {
        public String name;
        public Drawable icon;
        public ArrayList<String> suggestions;

        public SearchEngine(String name, Drawable icon) {
            this.name = name;
            this.icon = icon;
            this.suggestions = new ArrayList<String>();
        }
    };

    /**
     * Sets suggestions associated with the current suggest engine.
     * If there is no suggest engine, this does nothing.
     */
    public void setSuggestions(final ArrayList<String> suggestions) {
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                if (mSuggestEngine != null) {
                    mSuggestEngine.suggestions = suggestions;
                    mAllPagesCursorAdapter.notifyDataSetChanged();
                }
            }
        });
    }

    /**
     * Sets search engines to be shown for user-entered queries.
     */
    public void setSearchEngines(String suggestEngineName, JSONArray engines) {
        final ArrayList<SearchEngine> searchEngines = new ArrayList<SearchEngine>();
        SearchEngine suggestEngine = null;
        for (int i = 0; i < engines.length(); i++) {
            try {
                JSONObject engineJSON = engines.getJSONObject(i);
                String name = engineJSON.getString("name");
                String iconURI = engineJSON.getString("iconURI");
                Drawable icon = getDrawableFromDataURI(iconURI);
                if (name.equals(suggestEngineName)) {
                    suggestEngine = new SearchEngine(name, icon);
                } else {
                    searchEngines.add(new SearchEngine(name, icon));
                }
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error getting search engine JSON", e);
                return;
            }
        }

        final SearchEngine suggestEngineArg = suggestEngine;
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                mSuggestEngine = suggestEngineArg;
                mSearchEngines = searchEngines;
                mAllPagesCursorAdapter.notifyDataSetChanged();
            }
        });
    }

    public boolean isInReadingList() {
        return mInReadingList;
    }
}
