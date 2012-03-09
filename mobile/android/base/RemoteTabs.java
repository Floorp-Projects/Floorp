/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.HashMap;

import org.mozilla.gecko.db.BrowserContract.Clients;
import org.mozilla.gecko.db.BrowserContract.Tabs;
import org.mozilla.gecko.db.BrowserContract;

import org.json.JSONObject;

import android.app.Activity;
import android.content.Intent;
import android.content.Context;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ExpandableListView;
import android.widget.SimpleExpandableListAdapter;
import android.text.TextUtils;
import android.util.Log;

public class RemoteTabs extends Activity
       implements ExpandableListView.OnGroupClickListener, ExpandableListView.OnChildClickListener {
    private static final String LOGTAG = "GeckoRemoteTabs";

    private static int sPreferredHeight;
    private static int sChildItemHeight;
    private static int sGroupItemHeight;
    private static ExpandableListView mList;

    private static ArrayList <HashMap <String, String>> mClientsList;
    private static ArrayList <ArrayList <HashMap <String, String>>> mTabsList;

    // 50 for child + 2 for divider
    private static final int CHILD_ITEM_HEIGHT = 52;

    // 30 for group + 2 for divider
    private static final int GROUP_ITEM_HEIGHT = 32;

    private static final String[] PROJECTION_COLUMNS = new String[] {
                                                           BrowserContract.Tabs.TITLE,     // 0
                                                           BrowserContract.Tabs.URL,       // 1
                                                           BrowserContract.Clients.GUID,   // 2
                                                           BrowserContract.Clients.NAME    // 3
                                                       };

    private static final String[] CLIENT_KEY = new String[] { "name" };
    private static final String[] TAB_KEY = new String[] { "title" };
    private static final int[] CLIENT_RESOURCE = new int[] { R.id.client };
    private static final int[] TAB_RESOURCE = new int[] { R.id.tab };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.remote_tabs);

        mList = (ExpandableListView) findViewById(R.id.list);
        mList.setOnGroupClickListener(this);
        mList.setOnChildClickListener(this);

        LinearLayout container = (LinearLayout) findViewById(R.id.container);
        container.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                finishActivity();
            }
        });

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);

        sChildItemHeight = (int) (CHILD_ITEM_HEIGHT * metrics.density);
        sGroupItemHeight = (int) (GROUP_ITEM_HEIGHT * metrics.density);
        sPreferredHeight = (int) (0.67 * metrics.heightPixels);

        // Query the database for remote tabs in AsyncTask
        (new QueryRemoteTabsTask()).execute();
    }

    @Override
    public void onBackPressed() {
        startActivity(new Intent(this, TabsTray.class));
        overridePendingTransition(R.anim.grow_fade_in, 0);
        finishActivity();
    }

    void finishActivity() {
        finish();
        overridePendingTransition(0, R.anim.shrink_fade_out);
    }

    @Override
    public boolean onGroupClick(ExpandableListView parent, View view, int position, long id) {
        // By default, the group collapses/expands. Consume the event.
        return true;
    }

    @Override
    public boolean onChildClick(ExpandableListView parent, View view, int groupPosition, int childPosition, long id) {
        HashMap <String, String> tab = mTabsList.get(groupPosition).get(childPosition);
        if (tab == null) {
            finishActivity();
            return true;
        }

        String url = tab.get("url");
        JSONObject args = new JSONObject();
        try {
            args.put("url", url);
            args.put("engine", null);
            args.put("userEntered", false);
        } catch (Exception e) {
            Log.e(LOGTAG, "error building JSON arguments");
        }

        Log.i(LOGTAG, "Sending message to Gecko: " + SystemClock.uptimeMillis() + " - Tab:Add");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Add", args.toString()));
        finishActivity();
        return true;
    }

    // Tabs List Container holds the ExpandableListView
    public static class TabsListContainer extends LinearLayout {
        public TabsListContainer(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            SimpleExpandableListAdapter adapter = (SimpleExpandableListAdapter) mList.getExpandableListAdapter();
            if (adapter == null) {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                return;
            }

            int groupCount = adapter.getGroupCount();
            int childrenHeight = groupCount * sGroupItemHeight;
            for (int i = 0; i < groupCount; i++)
                 childrenHeight += adapter.getChildrenCount(i) * sChildItemHeight;

            int restrictedHeightSpec = MeasureSpec.makeMeasureSpec(Math.min(childrenHeight, sPreferredHeight), MeasureSpec.EXACTLY);
            super.onMeasure(widthMeasureSpec, restrictedHeightSpec);
        }
    }

    // AsyncTask to query the database
    private class QueryRemoteTabsTask extends GeckoAsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... unused) {
            mClientsList = new ArrayList <HashMap <String, String>>();
            mTabsList = new ArrayList <ArrayList <HashMap <String, String>>>();

            Cursor tabs = getContentResolver().query(BrowserContract.Tabs.CONTENT_URI,
                                                     PROJECTION_COLUMNS,
                                                     BrowserContract.Tabs.CLIENT_GUID + " IS NOT NULL",
                                                     null,
                                                     null);

            if (tabs == null)
                return null;

            String oldGuid = null;
            ArrayList <HashMap <String, String>> tabsForClient = null;
            HashMap <String, String> client;
            HashMap <String, String> tab;

            try {
                while (tabs.moveToNext()) {
                    String title = tabs.getString(0);
                    String url = tabs.getString(1);
                    String guid = tabs.getString(2);
                    String name = tabs.getString(3);

                    if (oldGuid == null || !TextUtils.equals(oldGuid, guid)) {
                        client = new HashMap <String, String>();
                        client.put("name", name);
                        mClientsList.add(client);

                        tabsForClient = new ArrayList <HashMap <String, String>>();
                        mTabsList.add(tabsForClient);

                        oldGuid = new String(guid);
                    }

                    tab = new HashMap<String, String>();
                    tab.put("title", TextUtils.isEmpty(title) ? url : title);
                    tab.put("url", url);
                    tabsForClient.add(tab);
                }
            } finally {
                tabs.close();
            }

            return null;
        }

        @Override
        protected void onPostExecute(Void unused) {
            if (mClientsList.size() == 0) {
                finishActivity();
                return;
            }

            mList.setAdapter(new SimpleExpandableListAdapter(getApplicationContext(),
                                                             mClientsList,
                                                             R.layout.remote_tabs_group,
                                                             CLIENT_KEY,
                                                             CLIENT_RESOURCE,
                                                             mTabsList,
                                                             R.layout.remote_tabs_child,
                                                             TAB_KEY,
                                                             TAB_RESOURCE));

            for (int i = 0; i < mClientsList.size(); i++) {
                mList.expandGroup(i);
            }
        }
    }
}
