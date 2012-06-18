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
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;
import android.widget.SimpleExpandableListAdapter;
import android.widget.TabHost;
import android.widget.TextView;

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

    private static final String HISTORY_TAB = "history";

    private static enum HistorySection { TODAY, YESTERDAY, WEEK, OLDER };

    private Context mContext;
    private boolean mInflated;
    private LayoutInflater mInflater;
    private OnUrlOpenListener mUrlOpenListener;
    private View.OnTouchListener mListTouchListener;
    private ContentResolver mContentResolver;
    private ContentObserver mContentObserver;

    private HistoryQueryTask mHistoryQueryTask;
    
    private AllPagesTab mAllPagesTab;
    public BookmarksTab mBookmarksTab;
    private SimpleExpandableListAdapter mHistoryAdapter;

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

    // This method checks to see if we're in a bookmark sub-folder. If we are,
    // it will go up a level and return true. Otherwise it will return false.
    public boolean onBackPressed() {
        // If the soft keyboard is visible in the bookmarks or history tab, the user
        // must have explictly brought it up, so we should try hiding it instead of
        // exiting the activity or going up a bookmarks folder level.
        if (getCurrentTabTag().equals(mBookmarksTab.getTag()) || getCurrentTabTag().equals(HISTORY_TAB)) {
            View tabView = getCurrentTabView();
            if (hideSoftInput(tabView))
                return true;
        }

        // If we're not in the bookmarks tab, we have nothing to do. We should
        // also return false if mBookmarksAdapter hasn't been initialized yet.
        if (!getCurrentTabTag().equals(mBookmarksTab.getTag()))
            return false;

        return mBookmarksTab.moveToParentFolder();
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

    public AwesomeBarTabs(Context context, AttributeSet attrs) {
        super(context, attrs);

        Log.d(LOGTAG, "Creating AwesomeBarTabs");

        mContext = context;
        mInflated = false;
        mContentResolver = context.getContentResolver();
        mContentObserver = null;
        mInflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
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
                if (tabId.equals(HISTORY_TAB) && mHistoryAdapter == null
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

    private TabSpec addAwesomeTab(String id, int titleId, TabHost.TabContentFactory factory) {
        TabSpec tab = getTabSpec(id, titleId);
        tab.setContent(factory);
        addTab(tab);

        return tab;
    }

    private TabSpec addAwesomeTab(String id, int titleId, int contentId) {
        TabSpec tab = getTabSpec(id, titleId);
        tab.setContent(contentId);
        addTab(tab);

        return tab;
    }

    private TabSpec getTabSpec(String id, int titleId) {
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
        return tab;
    }

    private void addAllPagesTab() {
        Log.d(LOGTAG, "Creating All Pages tab");

        mAllPagesTab = new AllPagesTab(mContext);
        addAwesomeTab(mAllPagesTab.getTag(),
                      mAllPagesTab.getTitleStringId(),
                      mAllPagesTab.getFactory());
        mAllPagesTab.setListTouchListener(mListTouchListener);
    }

    private void addBookmarksTab() {
        Log.d(LOGTAG, "Creating Bookmarks tab");

        mBookmarksTab = new BookmarksTab(mContext);
        addAwesomeTab(mBookmarksTab.getTag(),
                      mBookmarksTab.getTitleStringId(),
                      mBookmarksTab.getFactory());
        mBookmarksTab.setListTouchListener(mListTouchListener);

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

    private void handleHistoryItemClick(int groupPosition, int childPosition) {
        @SuppressWarnings("unchecked")
        Map<String,Object> historyItem =
                (Map<String,Object>) mHistoryAdapter.getChild(groupPosition, childPosition);

        String url = (String) historyItem.get(URLColumns.URL);

        if (mUrlOpenListener != null)
            mUrlOpenListener.onUrlOpen(url);
    }

    public void setOnUrlOpenListener(OnUrlOpenListener listener) {
        mUrlOpenListener = listener;
        mAllPagesTab.setUrlListener(listener);
        mBookmarksTab.setUrlListener(listener);
    }

    public void destroy() {
        mAllPagesTab.destroy();
        mBookmarksTab.destroy();

        if (mContentObserver != null)
            BrowserDB.unregisterContentObserver(mContentResolver, mContentObserver);
    }

    public void filter(String searchTerm) {
        // Don't let the tab's content steal focus on tab switch
        setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);

        // Ensure the 'All Pages' tab is selected
        setCurrentTabByTag(mAllPagesTab.getTag());

        // Restore normal focus behavior on tab host
        setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);

        // The tabs should only be visible if there's no on-going search
        int tabsVisibility = (searchTerm.length() == 0 ? View.VISIBLE : View.GONE);
        getTabWidget().setVisibility(tabsVisibility);

        // Perform the actual search
        mAllPagesTab.filter(searchTerm);
    }

    /**
     * Sets suggestions associated with the current suggest engine.
     * If there is no suggest engine, this does nothing.
     */
    public void setSuggestions(final ArrayList<String> suggestions) {
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                mAllPagesTab.setSuggestions(suggestions);
            }
        });
    }

    /**
     * Sets search engines to be shown for user-entered queries.
     */
    public void setSearchEngines(final String suggestEngineName, final JSONArray engines) {
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                mAllPagesTab.setSearchEngines(suggestEngineName, engines);
            }
        });
    }

    public boolean isInReadingList() {
        return mBookmarksTab.isInReadingList();
    }
}
