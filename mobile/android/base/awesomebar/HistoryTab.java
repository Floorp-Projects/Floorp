/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.res.Resources;
import android.os.AsyncTask;
import android.text.TextUtils;
import android.database.ContentObserver;
import android.content.ContentResolver;
import android.content.Context;
import android.widget.ImageView;
import android.widget.TextView;
import android.view.View;
import android.widget.ListView;
import android.database.Cursor;
import android.view.MenuInflater;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.widget.TabHost.TabContentFactory;
import android.util.Pair;
import android.widget.ExpandableListView;
import android.widget.SimpleExpandableListAdapter;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;

import java.util.LinkedList;
import java.util.HashMap;
import java.util.Map;
import java.util.List;
import java.util.Date;

import org.mozilla.gecko.AwesomeBar.ContextMenuSubject;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

public class HistoryTab extends AwesomeBarTab {
    public static final String LOGTAG = "HISTORY_TAB";
    public static final String TAG = "history";
    private static enum HistorySection { TODAY, YESTERDAY, WEEK, OLDER };
    private ContentObserver mContentObserver;
    private ContentResolver mContentResolver;
    private HistoryQueryTask mQueryTask = null;
    private ExpandableListView mView = null;
    private HistoryListAdapter mCursorAdapter = null;

    public HistoryTab(Context context) {
        super(context);
        mContentObserver = null;
    }

    public int getTitleStringId() {
        return R.string.awesomebar_history_title;
    }

    public String getTag() {
        return TAG;
    }

    public TabContentFactory getFactory() {
        return new TabContentFactory() {
            public View createTabContent(String tag) {
                final ExpandableListView list = (ExpandableListView)getListView();
                list.setOnChildClickListener(new ExpandableListView.OnChildClickListener() {
                    public boolean onChildClick(ExpandableListView parent, View view,
                                                 int groupPosition, int childPosition, long id) {
                        return handleItemClick(groupPosition, childPosition);
                    }
                });

                // This is to disallow collapsing the expandable groups in the
                // history expandable list view to mimic simpler sections. We should
                // Remove this if we decide to allow expanding/collapsing groups.
                list.setOnGroupClickListener(new ExpandableListView.OnGroupClickListener() {
                     public boolean onGroupClick(ExpandableListView parent, View v, int groupPosition, long id) {
                        return true;
                    }
                });
                return list;
            }
       };
    }

    public ListView getListView() {
        if (mView == null) {
            mView = (ExpandableListView) (LayoutInflater.from(mContext).inflate(R.layout.awesomebar_expandable_list, null));
            ((Activity)mContext).registerForContextMenu(mView);
            mView.setTag(TAG);
            mView.setOnTouchListener(mListListener);

            // We need to add the header before we set the adapter, hence make it null
            mView.setAdapter(getCursorAdapter());
            HistoryQueryTask task = new HistoryQueryTask();
            task.execute();
        }
        return mView;
    }

    public void destroy() {
        if (mContentObserver != null)
            BrowserDB.unregisterContentObserver(getContentResolver(), mContentObserver);
    }

    public boolean onBackPressed() {
        // If the soft keyboard is visible in the bookmarks or history tab, the user
        // must have explictly brought it up, so we should try hiding it instead of
        // exiting the activity or going up a bookmarks folder level.
        ListView view = getListView();
        if (hideSoftInput(view))
            return true;

        return false;
    }

    protected HistoryListAdapter getCursorAdapter() {
        return mCursorAdapter;
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
                convertView = getInflater().inflate(R.layout.awesomebar_row, null);

                viewHolder = new AwesomeEntryViewHolder();
                viewHolder.titleView = (TextView) convertView.findViewById(R.id.title);
                viewHolder.urlView = (TextView) convertView.findViewById(R.id.url);
                viewHolder.faviconView = (ImageView) convertView.findViewById(R.id.favicon);
                viewHolder.bookmarkIconView = (ImageView) convertView.findViewById(R.id.bookmark_icon);

                convertView.setTag(viewHolder);
            } else {
                viewHolder = (AwesomeEntryViewHolder) convertView.getTag();
            }

            HistoryListAdapter adapter = getCursorAdapter();
            if (adapter == null)
                return null;

            @SuppressWarnings("unchecked")
            Map<String,Object> historyItem =
                    (Map<String,Object>) adapter.getChild(groupPosition, childPosition);

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
            Pair<GroupList, List<ChildrenList>> result = null;
            Cursor cursor = BrowserDB.getRecentHistory(getContentResolver(), MAX_RESULTS);

            Date now = new Date();
            now.setHours(0);
            now.setMinutes(0);
            now.setSeconds(0);

            long today = now.getTime();

            // Split the list of urls into separate date range groups
            // and show it in an expandable list view.
            List<ChildrenList> childrenLists = null;
            ChildrenList children = null;
            GroupList groups = null;
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

                if (groups == null)
                    groups = new GroupList();

                if (childrenLists == null)
                    childrenLists = new LinkedList<ChildrenList>();

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

            if (groups != null && childrenLists != null) {
                result = Pair.<GroupList,List<ChildrenList>>create(groups, childrenLists);
            }

            return result;
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
            int groupCount = mCursorAdapter.getGroupCount();

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
            // FIXME: display some sort of message when there's no history
            if (result == null)
                return;

            mCursorAdapter = new HistoryListAdapter(
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
                        mQueryTask = new HistoryQueryTask();
                        mQueryTask.execute();
                    }
                };
                BrowserDB.registerHistoryObserver(getContentResolver(), mContentObserver);
            }

            final ExpandableListView historyList = (ExpandableListView)getListView();

            // Hack: force this to the main thread, even though it should already be on it
            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    historyList.setAdapter(mCursorAdapter);
                    expandAllGroups(historyList);
                }
            });

            mQueryTask = null;
        }
    }

    public boolean handleItemClick(int groupPosition, int childPosition) {
        HistoryListAdapter adapter = getCursorAdapter();
        if (adapter == null)
            return false;

        @SuppressWarnings("unchecked")
        Map<String,Object> historyItem = (Map<String,Object>) adapter.getChild(groupPosition, childPosition);

        String url = (String) historyItem.get(URLColumns.URL);
        AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
        if (!TextUtils.isEmpty(url) && listener != null)
            listener.onUrlOpen(url);

        return true;
    }

    public ContextMenuSubject getSubject(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
        ContextMenuSubject subject = null;

        if (!(menuInfo instanceof ExpandableListView.ExpandableListContextMenuInfo)) {
            Log.e(LOGTAG, "menuInfo is not ExpandableListContextMenuInfo");
            return subject;
        }

        ExpandableListView.ExpandableListContextMenuInfo info = (ExpandableListView.ExpandableListContextMenuInfo) menuInfo;
        int childPosition = ExpandableListView.getPackedPositionChild(info.packedPosition);
        int groupPosition = ExpandableListView.getPackedPositionGroup(info.packedPosition);

        // Check if long tap is on a header row
        if (groupPosition < 0 || childPosition < 0)
            return subject;

        ExpandableListView exList = (ExpandableListView) view;

        // The history list is backed by a SimpleExpandableListAdapter
        @SuppressWarnings("rawtypes")
        Map map = (Map) exList.getExpandableListAdapter().getChild(groupPosition, childPosition);
        subject = new AwesomeBar.ContextMenuSubject((Integer) map.get(Combined.HISTORY_ID),
                                                     (String) map.get(URLColumns.URL),
                                                     (byte[]) map.get(URLColumns.FAVICON),
                                                     (String) map.get(URLColumns.TITLE),
                                                     null);

        MenuInflater inflater = new MenuInflater(mContext);
        inflater.inflate(R.menu.awesomebar_contextmenu, menu);
        
        menu.findItem(R.id.remove_bookmark).setVisible(false);
        menu.findItem(R.id.edit_bookmark).setVisible(false);

        // Hide "Remove" item if there isn't a valid history ID
        if (subject.id < 0)
            menu.findItem(R.id.remove_history).setVisible(false);

        menu.setHeaderTitle(subject.title);
        return subject;
    }
}
