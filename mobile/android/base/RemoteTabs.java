/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.json.JSONObject;

import android.content.Context;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ExpandableListView;
import android.widget.SimpleExpandableListAdapter;
import android.text.TextUtils;

public class RemoteTabs extends LinearLayout
                        implements TabsPanel.PanelView,
                                   ExpandableListView.OnGroupClickListener,
                                   ExpandableListView.OnChildClickListener, 
                                   TabsAccessor.OnQueryTabsCompleteListener {
    private static final String LOGTAG = "GeckoRemoteTabs";

    private Context mContext;

    private static ExpandableListView mList;
    
    private static ArrayList <ArrayList <HashMap <String, String>>> mTabsList;

    private static final String[] CLIENT_KEY = new String[] { "name" };
    private static final String[] TAB_KEY = new String[] { "title" };
    private static final int[] CLIENT_RESOURCE = new int[] { R.id.client };
    private static final int[] TAB_RESOURCE = new int[] { R.id.tab };

    public RemoteTabs(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        LayoutInflater.from(context).inflate(R.layout.remote_tabs, this);

        mList = (ExpandableListView) findViewById(R.id.list);
        mList.setOnGroupClickListener(this);
        mList.setOnChildClickListener(this);
    }

    @Override
    public ViewGroup getLayout() {
        return this;
    }

    @Override
    public void show() {
        TabsAccessor.getTabs(mContext, this);
    }

    @Override
    public void hide() {
    }

    void autoHideTabs() {
        GeckoApp.mAppContext.autoHideTabs();
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
            autoHideTabs();
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

        Log.d(LOGTAG, "Sending message to Gecko: " + SystemClock.uptimeMillis() + " - Tab:Add");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Add", args.toString()));
        autoHideTabs();
        return true;
    }

    @Override
    public void onQueryTabsComplete(List<TabsAccessor.RemoteTab> remoteTabsList) {
        ArrayList<TabsAccessor.RemoteTab> remoteTabs = new ArrayList<TabsAccessor.RemoteTab> (remoteTabsList);
        if (remoteTabs == null || remoteTabs.size() == 0) {
            autoHideTabs();
            return;
        }
        
        ArrayList <HashMap <String, String>> clients = new ArrayList <HashMap <String, String>>();

        mTabsList = new ArrayList <ArrayList <HashMap <String, String>>>();

        String oldGuid = null;
        ArrayList <HashMap <String, String>> tabsForClient = null;
        HashMap <String, String> client;
        HashMap <String, String> tab;
        
        for (TabsAccessor.RemoteTab remoteTab : remoteTabs) {
            if (oldGuid == null || !TextUtils.equals(oldGuid, remoteTab.guid)) {
                client = new HashMap <String, String>();
                client.put("name", remoteTab.name);
                clients.add(client);
        
                tabsForClient = new ArrayList <HashMap <String, String>>();
                mTabsList.add(tabsForClient);
        
                oldGuid = new String(remoteTab.guid);
            }
        
            tab = new HashMap<String, String>();
            tab.put("title", TextUtils.isEmpty(remoteTab.title) ? remoteTab.url : remoteTab.title);
            tab.put("url", remoteTab.url);
            tabsForClient.add(tab);
        }
        
        mList.setAdapter(new SimpleExpandableListAdapter(mContext,
                                                         clients,
                                                         R.layout.remote_tabs_group,
                                                         CLIENT_KEY,
                                                         CLIENT_RESOURCE,
                                                         mTabsList,
                                                         R.layout.remote_tabs_child,
                                                         TAB_KEY,
                                                         TAB_RESOURCE));
        
        for (int i = 0; i < clients.size(); i++) {
            mList.expandGroup(i);
        }
    }
}
