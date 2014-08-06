/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.TabsAccessor;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;

import android.content.Context;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ExpandableListView;
import android.widget.SimpleExpandableListAdapter;

/**
 * The actual list of synced tabs. This serves as the only child view of {@link RemoteTabsContainerPanel}
 * so it can be refreshed using a swipe-to-refresh gesture.
 */
class RemoteTabsList extends ExpandableListView
                     implements ExpandableListView.OnGroupClickListener,
                                ExpandableListView.OnChildClickListener,
                                TabsAccessor.OnQueryTabsCompleteListener {
    private static final String[] CLIENT_KEY = new String[] { "name", "last_synced" };
    private static final String[] TAB_KEY = new String[] { "title", "url" };
    private static final int[] CLIENT_RESOURCE = new int[] { R.id.client, R.id.last_synced };
    private static final int[] TAB_RESOURCE = new int[] { R.id.tab, R.id.url };

    private final Context context;
    private TabsPanel tabsPanel;

    private ArrayList <HashMap <String, String>> clients;
    private ArrayList <ArrayList <HashMap <String, String>>> tabsList;

    // A list of the clients that are currently expanded.
    private List<String> expandedClientList;

    // The client that previously had an item selected is used to restore the scroll position.
    private String clientScrollPosition;

    public RemoteTabsList(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.context = context;

        setOnGroupClickListener(this);
        setOnChildClickListener(this);
    }

    public void setTabsPanel(TabsPanel panel) {
        tabsPanel = panel;
    }

    private void autoHidePanel() {
        tabsPanel.autoHidePanel();
    }

    @Override
    public boolean onGroupClick(ExpandableListView parent, View view, int groupPosition, long id) {
        final String clientGuid = clients.get(groupPosition).get("guid");

        if (isGroupExpanded(groupPosition)) {
            collapseGroup(groupPosition);
            expandedClientList.remove(clientGuid);
        } else {
            expandGroup(groupPosition);
            expandedClientList.add(clientGuid);
        }

        clientScrollPosition = clientGuid;
        return true;
    }

    @Override
    public boolean onChildClick(ExpandableListView parent, View view, int groupPosition, int childPosition, long id) {
        HashMap <String, String> tab = tabsList.get(groupPosition).get(childPosition);
        if (tab == null) {
            autoHidePanel();
            return true;
        }

        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, "remote");

        Tabs.getInstance().loadUrl(tab.get("url"), Tabs.LOADURL_NEW_TAB);
        autoHidePanel();

        clientScrollPosition = clients.get(groupPosition).get("guid");
        return true;
    }

    @Override
    public void onQueryTabsComplete(List<TabsAccessor.RemoteTab> remoteTabsList) {
        ArrayList<TabsAccessor.RemoteTab> remoteTabs = new ArrayList<TabsAccessor.RemoteTab> (remoteTabsList);
        if (remoteTabs == null || remoteTabs.size() == 0)
            return;

        clients = new ArrayList <HashMap <String, String>>();
        tabsList = new ArrayList <ArrayList <HashMap <String, String>>>();

        String oldGuid = null;
        ArrayList <HashMap <String, String>> tabsForClient = null;
        HashMap <String, String> client;
        HashMap <String, String> tab;

        final long now = System.currentTimeMillis();

        for (TabsAccessor.RemoteTab remoteTab : remoteTabs) {
            final String clientGuid = remoteTab.guid;
            if (oldGuid == null || !TextUtils.equals(oldGuid, clientGuid)) {
                client = new HashMap <String, String>();
                client.put("name", remoteTab.name);
                client.put("last_synced", getLastSyncedString(now, remoteTab.lastModified));
                client.put("guid", clientGuid);
                clients.add(client);

                tabsForClient = new ArrayList <HashMap <String, String>>();
                tabsList.add(tabsForClient);

                oldGuid = new String(clientGuid);
            }

            tab = new HashMap<String, String>();
            tab.put("title", TextUtils.isEmpty(remoteTab.title) ? remoteTab.url : remoteTab.title);
            tab.put("url", remoteTab.url);
            tabsForClient.add(tab);
        }

        setAdapter(new SimpleExpandableListAdapter(context,
                                                   clients,
                                                   R.layout.remote_tabs_group,
                                                   CLIENT_KEY,
                                                   CLIENT_RESOURCE,
                                                   tabsList,
                                                   R.layout.remote_tabs_child,
                                                   TAB_KEY,
                                                   TAB_RESOURCE));

        // Either set the initial UI state, or restore it.
        List<String> newExpandedClientList = new ArrayList<String>();
        for (int i = 0; i < clients.size(); i++) {
            final String clientGuid = clients.get(i).get("guid");

            if (expandedClientList == null) {
                // On initial entry we expand all clients by default.
                newExpandedClientList.add(clientGuid);
                expandGroup(i);
            } else {
                // On subsequent entries, we expand clients based on their previous UI state.
                if (expandedClientList.contains(clientGuid)) {
                    newExpandedClientList.add(clientGuid);
                    expandGroup(i);
                }

                // Additionally we reset the UI scroll position.
                if (clientGuid.equals(clientScrollPosition)) {
                    setSelectedGroup(i);
                }
            }
        }
        expandedClientList = newExpandedClientList;
    }

    /**
     * Return a relative "Last synced" time span for the given tab record.
     *
     * @param now local time.
     * @param time to format string for.
     * @return string describing time span
     */
    protected String getLastSyncedString(long now, long time) {
        CharSequence relativeTimeSpanString = DateUtils.getRelativeTimeSpanString(time, now, DateUtils.MINUTE_IN_MILLIS);
        return getResources().getString(R.string.remote_tabs_last_synced, relativeTimeSpanString);
    }
}
