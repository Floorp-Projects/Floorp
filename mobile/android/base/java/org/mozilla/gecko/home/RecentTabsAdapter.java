/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SessionParser;
import org.mozilla.gecko.home.CombinedHistoryAdapter.RecentTabsUpdateHandler;
import org.mozilla.gecko.home.CombinedHistoryPanel.PanelStateUpdateHandler;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.ArrayList;
import java.util.List;

import static org.mozilla.gecko.home.CombinedHistoryItem.ItemType;

public class RecentTabsAdapter extends RecyclerView.Adapter<CombinedHistoryItem>
                               implements CombinedHistoryRecyclerView.AdapterContextMenuBuilder, NativeEventListener {
    private static final String LOGTAG = "GeckoRecentTabsAdapter";

    private static final int NAVIGATION_BACK_BUTTON_INDEX = 0;

    // Recently closed tabs from Gecko.
    private ClosedTab[] recentlyClosedTabs;

    // "Tabs from last time".
    private ClosedTab[] lastSessionTabs;

    public static final class ClosedTab {
        public final String url;
        public final String title;
        public final String data;

        public ClosedTab(String url, String title, String data) {
            this.url = url;
            this.title = title;
            this.data = data;
        }
    }

    private final Context context;
    private final RecentTabsUpdateHandler recentTabsUpdateHandler;
    private final PanelStateUpdateHandler panelStateUpdateHandler;

    public RecentTabsAdapter(Context context,
                             RecentTabsUpdateHandler recentTabsUpdateHandler,
                             PanelStateUpdateHandler panelStateUpdateHandler) {
        this.context = context;
        this.recentTabsUpdateHandler = recentTabsUpdateHandler;
        this.panelStateUpdateHandler = panelStateUpdateHandler;
        recentlyClosedTabs = new ClosedTab[0];
        lastSessionTabs = new ClosedTab[0];

        readPreviousSessionData();
    }

    public void startListeningForClosedTabs() {
        EventDispatcher.getInstance().registerGeckoThreadListener(this, "ClosedTabs:Data");
        GeckoAppShell.notifyObservers("ClosedTabs:StartNotifications", null);
    }

    public void stopListeningForClosedTabs() {
        GeckoAppShell.notifyObservers("ClosedTabs:StopNotifications", null);
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this, "ClosedTabs:Data");
    }

    @Override
    public void handleMessage(String event, NativeJSObject message, EventCallback callback) {
        final NativeJSObject[] tabs = message.getObjectArray("tabs");
        final int length = tabs.length;

        final ClosedTab[] closedTabs = new ClosedTab[length];
        for (int i = 0; i < length; i++) {
            final NativeJSObject tab = tabs[i];
            closedTabs[i] = new ClosedTab(tab.getString("url"), tab.getString("title"), tab.getObject("data").toString());
        }

        // Only modify recentlyClosedTabs on the UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                // Save some data about the old panel state, so we can be
                // smarter about notifying the recycler view which bits changed.
                int prevClosedTabsCount = recentlyClosedTabs.length;
                boolean prevSectionHeaderVisibility = isSectionHeaderVisible();
                int prevSectionHeaderIndex = getSectionHeaderIndex();

                recentlyClosedTabs = closedTabs;
                recentTabsUpdateHandler.onRecentTabsCountUpdated(getClosedTabsCount());
                panelStateUpdateHandler.onPanelStateUpdated();

                // Handle the section header hiding/unhiding.
                updateHeaderVisibility(prevSectionHeaderVisibility, prevSectionHeaderIndex);

                // Update the "Recently closed" part of the tab list.
                updateTabsList(prevClosedTabsCount, recentlyClosedTabs.length, getFirstRecentTabIndex(), getLastRecentTabIndex());
            }
        });
    }

    private void readPreviousSessionData() {
        // Make sure that the start up code has had a chance to update sessionstore.bak as necessary.
        GeckoProfile.get(context).waitForOldSessionDataProcessing();

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final String jsonString = GeckoProfile.get(context).readSessionFile(true);
                if (jsonString == null) {
                    // No previous session data.
                    return;
                }

                final List<ClosedTab> parsedTabs = new ArrayList<>();

                new SessionParser() {
                    @Override
                    public void onTabRead(SessionTab tab) {
                        final String url = tab.getUrl();

                        // Don't show last tabs for about:home
                        if (AboutPages.isAboutHome(url)) {
                            return;
                        }

                        parsedTabs.add(new ClosedTab(url, tab.getTitle(), tab.getTabObject().toString()));
                    }
                }.parse(jsonString);

                final ClosedTab[] closedTabs = parsedTabs.toArray(new ClosedTab[parsedTabs.size()]);

                // Only modify lastSessionTabs on the UI thread.
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // Save some data about the old panel state, so we can be
                        // smarter about notifying the recycler view which bits changed.
                        int prevClosedTabsCount = lastSessionTabs.length;
                        boolean prevSectionHeaderVisibility = isSectionHeaderVisible();
                        int prevSectionHeaderIndex = getSectionHeaderIndex();

                        lastSessionTabs = closedTabs;
                        recentTabsUpdateHandler.onRecentTabsCountUpdated(getClosedTabsCount());
                        panelStateUpdateHandler.onPanelStateUpdated();

                        // Handle the section header hiding/unhiding.
                        updateHeaderVisibility(prevSectionHeaderVisibility, prevSectionHeaderIndex);

                        // Update the "Tabs from last time" part of the tab list.
                        updateTabsList(prevClosedTabsCount, lastSessionTabs.length, getFirstLastSessionTabIndex(), getLastLastSessionTabIndex());
                    }
                });
            }
        });
    }

    public void clearLastSessionData() {
        final ClosedTab[] emptyLastSessionTabs = new ClosedTab[0];

        // Only modify mLastSessionTabs on the UI thread.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                // Save some data about the old panel state, so we can be
                // smarter about notifying the recycler view which bits changed.
                int prevClosedTabsCount = lastSessionTabs.length;
                boolean prevSectionHeaderVisibility = isSectionHeaderVisible();
                int prevSectionHeaderIndex = getSectionHeaderIndex();

                lastSessionTabs = emptyLastSessionTabs;
                recentTabsUpdateHandler.onRecentTabsCountUpdated(getClosedTabsCount());
                panelStateUpdateHandler.onPanelStateUpdated();

                // Handle the section header hiding.
                updateHeaderVisibility(prevSectionHeaderVisibility, prevSectionHeaderIndex);

                // Handle the "tabs from last time" being cleared.
                if (prevClosedTabsCount > 0) {
                    notifyItemRangeRemoved(getFirstLastSessionTabIndex(), prevClosedTabsCount);
                }
            }
        });
    }

    private void updateHeaderVisibility(boolean prevSectionHeaderVisibility, int prevSectionHeaderIndex) {
        if (prevSectionHeaderVisibility && !isSectionHeaderVisible()) {
            notifyItemRemoved(prevSectionHeaderIndex);
        } else if (!prevSectionHeaderVisibility && isSectionHeaderVisible()) {
            notifyItemInserted(getSectionHeaderIndex());
        }
    }

    /**
     * Updates the tab list as necessary to account for any changes in tab count in a particular data source.
     *
     * Since the session store only sends out full updates, we don't know for sure what has changed compared
     * to the last data set, so we can only animate if the tab count actually changes.
     *
     * @param prevClosedTabsCount The previous number of closed tabs from that data source.
     * @param closedTabsCount The current number of closed tabs contained in that data source.
     * @param firstTabListIndex The current position of that data source's first item in the RecyclerView.
     * @param lastTabListIndex The current position of that data source's last item in the RecyclerView.
     */
    private void updateTabsList(int prevClosedTabsCount, int closedTabsCount, int firstTabListIndex, int lastTabListIndex) {
        final int closedTabsCountChange = closedTabsCount - prevClosedTabsCount;

        if (closedTabsCountChange <= 0) {
            notifyItemRangeRemoved(lastTabListIndex + 1, -closedTabsCountChange); // Remove tabs from the bottom of the list.
            notifyItemRangeChanged(firstTabListIndex, closedTabsCount); // Update the contents of the remaining items.
        } else { // closedTabsCountChange > 0
            notifyItemRangeInserted(firstTabListIndex, closedTabsCountChange); // Add additional tabs at the top of the list.
            notifyItemRangeChanged(firstTabListIndex + closedTabsCountChange, prevClosedTabsCount); // Update any previous list items.
        }
    }

    public void restoreTabFromPosition(int position) {
        final List<String> dataList = new ArrayList<>(1);
        dataList.add(getClosedTabForPosition(position).data);
        restoreSessionWithHistory(dataList);
    }

    public void restoreAllTabs() {
        if (recentlyClosedTabs.length == 0 && lastSessionTabs.length == 0) {
            return;
        }

        final List<String> dataList = new ArrayList<>(getClosedTabsCount());
        addTabDataToList(dataList, recentlyClosedTabs);
        addTabDataToList(dataList, lastSessionTabs);

        restoreSessionWithHistory(dataList);
    }

    private void addTabDataToList(List<String> dataList, ClosedTab[] closedTabs) {
        for (ClosedTab closedTab : closedTabs) {
            dataList.add(closedTab.data);
        }
    }

    private static void restoreSessionWithHistory(List<String> dataList) {
        final JSONObject json = new JSONObject();
        try {
            json.put("tabs", new JSONArray(dataList));
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON error", e);
        }

        GeckoAppShell.notifyObservers("Session:RestoreRecentTabs", json.toString());
    }

    @Override
    public CombinedHistoryItem onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());
        final View view;

        final CombinedHistoryItem.ItemType itemType = CombinedHistoryItem.ItemType.viewTypeToItemType(viewType);

        switch (itemType) {
            case NAVIGATION_BACK:
                view = inflater.inflate(R.layout.home_combined_back_item, parent, false);
                return new CombinedHistoryItem.HistoryItem(view);

            case SECTION_HEADER:
                view = inflater.inflate(R.layout.home_header_row, parent, false);
                return new CombinedHistoryItem.BasicItem(view);

            case CLOSED_TAB:
                view = inflater.inflate(R.layout.home_item_row, parent, false);
                return new CombinedHistoryItem.HistoryItem(view);
        }
        return null;
    }

    @Override
    public void onBindViewHolder(CombinedHistoryItem holder, final int position) {
        final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);

        switch (itemType) {
            case SECTION_HEADER:
                ((TextView) holder.itemView).setText(context.getString(R.string.home_closed_tabs_title2));
                break;

            case CLOSED_TAB:
                final ClosedTab closedTab = getClosedTabForPosition(position);
                ((CombinedHistoryItem.HistoryItem) holder).bind(closedTab);
                break;
        }
    }

    @Override
    public int getItemCount() {
        int itemCount = 1; // NAVIGATION_BACK button is always visible.

        if (isSectionHeaderVisible()) {
            itemCount += 1;
        }

        itemCount += getClosedTabsCount();

        return itemCount;
    }

    private CombinedHistoryItem.ItemType getItemTypeForPosition(int position) {
        if (position == NAVIGATION_BACK_BUTTON_INDEX) {
            return ItemType.NAVIGATION_BACK;
        }

        if (position == getSectionHeaderIndex() && isSectionHeaderVisible()) {
            return ItemType.SECTION_HEADER;
        }

        return ItemType.CLOSED_TAB;
    }

    @Override
    public int getItemViewType(int position) {
        return CombinedHistoryItem.ItemType.itemTypeToViewType(getItemTypeForPosition(position));
    }

    public int getClosedTabsCount() {
        return recentlyClosedTabs.length + lastSessionTabs.length;
    }

    private boolean isSectionHeaderVisible() {
        return recentlyClosedTabs.length > 0 || lastSessionTabs.length > 0;
    }

    private int getSectionHeaderIndex() {
        return isSectionHeaderVisible() ?
                NAVIGATION_BACK_BUTTON_INDEX + 1 :
                NAVIGATION_BACK_BUTTON_INDEX;
    }

    private int getFirstRecentTabIndex() {
        return getSectionHeaderIndex() + 1;
    }

    private int getLastRecentTabIndex() {
        return getSectionHeaderIndex() + recentlyClosedTabs.length;
    }

    private int getFirstLastSessionTabIndex() {
        return getLastRecentTabIndex() + 1;
    }

    private int getLastLastSessionTabIndex() {
        return getLastRecentTabIndex() + lastSessionTabs.length;
    }

    /**
     * Get the closed tab corresponding to a RecyclerView list item.
     *
     * The Recent Tab folder combines two data sources, so if we want to get the ClosedTab object
     * behind a certain list item, we need to route this request to the corresponding data source
     * and also transform the global list position into a local array index.
     */
    private ClosedTab getClosedTabForPosition(int position) {
        final ClosedTab closedTab;
        if (position <= getLastRecentTabIndex()) { // Upper part of the list is "Recently closed tabs".
            closedTab = recentlyClosedTabs[position - getFirstRecentTabIndex()];
        } else { // Lower part is "Tabs from last time".
            closedTab = lastSessionTabs[position - getFirstLastSessionTabIndex()];
        }

        return closedTab;
    }

    @Override
    public HomeContextMenuInfo makeContextMenuInfoFromPosition(View view, int position) {
        final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);
        final HomeContextMenuInfo info;

        switch (itemType) {
            case CLOSED_TAB:
                info = new HomeContextMenuInfo(view, position, -1);
                ClosedTab closedTab = getClosedTabForPosition(position);
                return populateChildInfoFromTab(info, closedTab);
        }

        return null;
    }

    protected static HomeContextMenuInfo populateChildInfoFromTab(HomeContextMenuInfo info, ClosedTab tab) {
        info.url = tab.url;
        info.title = tab.title;
        return info;
    }
}
