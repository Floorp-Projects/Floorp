/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.support.annotation.UiThread;
import android.support.v4.util.Pair;
import android.support.v7.widget.RecyclerView;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import static org.mozilla.gecko.home.CombinedHistoryItem.ItemType.*;

public class ClientsAdapter extends RecyclerView.Adapter<CombinedHistoryItem> implements CombinedHistoryRecyclerView.AdapterContextMenuBuilder {
    public static final String LOGTAG = "GeckoClientsAdapter";

    /**
     * If a device claims to have synced before this date, we will assume it has never synced.
     */
    public static final Date EARLIEST_VALID_SYNCED_DATE;
    static {
        final Calendar c = GregorianCalendar.getInstance();
        c.set(2000, Calendar.JANUARY, 1, 0, 0, 0);
        EARLIEST_VALID_SYNCED_DATE = c.getTime();
    }

    List<Pair<String, Integer>> adapterList = new LinkedList<>();

    // List of hidden remote clients.
    // Only accessed from the UI thread.
    protected final List<RemoteClient> hiddenClients = new ArrayList<>();
    private Map<String, RemoteClient> visibleClients = new HashMap<>();

    // Maintain group collapsed and hidden state. Only accessed from the UI thread.
    protected static RemoteTabsExpandableListState sState;

    private final Context context;

    public ClientsAdapter(Context context) {
        this.context = context;

        // This races when multiple Fragments are created. That's okay: one
        // will win, and thereafter, all will be okay. If we create and then
        // drop an instance the shared SharedPreferences backing all the
        // instances will maintain the state for us. Since everything happens on
        // the UI thread, this doesn't even need to be volatile.
        if (sState == null) {
            sState = new RemoteTabsExpandableListState(GeckoSharedPrefs.forProfile(context));
        }

        this.setHasStableIds(true);
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

            case CLIENT:
                view = inflater.inflate(R.layout.home_remote_tabs_group, parent, false);
                return new CombinedHistoryItem.ClientItem(view);

            case CHILD:
                view = inflater.inflate(R.layout.home_item_row, parent, false);
                return new CombinedHistoryItem.HistoryItem(view);

            case HIDDEN_DEVICES:
                view = inflater.inflate(R.layout.home_remote_tabs_hidden_devices, parent, false);
                return new CombinedHistoryItem.BasicItem(view);
        }
        return null;
    }

    @Override
    public void onBindViewHolder(CombinedHistoryItem holder, final int position) {
       final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);

        switch (itemType) {
            case CLIENT:
                final CombinedHistoryItem.ClientItem clientItem = (CombinedHistoryItem.ClientItem) holder;
                final String clientGuid = adapterList.get(position).first;
                final RemoteClient client = visibleClients.get(clientGuid);
                clientItem.bind(context, client, sState.isClientCollapsed(clientGuid));
                break;

            case CHILD:
                final Pair<String, Integer> pair = adapterList.get(position);
                RemoteTab remoteTab = visibleClients.get(pair.first).tabs.get(pair.second);
                ((CombinedHistoryItem.HistoryItem) holder).bind(remoteTab);
                break;

            case HIDDEN_DEVICES:
                final String hiddenDevicesLabel = context.getResources().getString(R.string.home_remote_tabs_many_hidden_devices, hiddenClients.size());
                ((TextView) holder.itemView).setText(hiddenDevicesLabel);
                break;
        }
    }

    @Override
    public int getItemCount () {
        return adapterList.size();
    }

    private CombinedHistoryItem.ItemType getItemTypeForPosition(int position) {
        if (position == 0) {
            return NAVIGATION_BACK;
        }

        final Pair<String, Integer> pair = adapterList.get(position);
        if (pair == null) {
            return HIDDEN_DEVICES;
        } else if (pair.second == -1) {
            return CLIENT;
        } else {
            return CHILD;
        }
    }

    @Override
    public int getItemViewType(int position) {
        return CombinedHistoryItem.ItemType.itemTypeToViewType(getItemTypeForPosition(position));
    }

    @Override
    public long getItemId(int position) {
        // RecyclerView.NO_ID is -1, so start our hard-coded IDs at -2.
        final int NAVIGATION_BACK_ID = -2;
        final int HIDDEN_DEVICES_ID = -3;

        final String clientGuid;
        // adapterList is a list of tuples (clientGuid, tabId).
        final Pair<String, Integer> pair = adapterList.get(position);

        switch (getItemTypeForPosition(position)) {
            case NAVIGATION_BACK:
                return NAVIGATION_BACK_ID;

            case HIDDEN_DEVICES:
                return HIDDEN_DEVICES_ID;

            // For Clients, return hashCode of their GUIDs.
            case CLIENT:
                clientGuid = pair.first;
                return clientGuid.hashCode();

            // For Tabs, return hashCode of their URLs.
            case CHILD:
                clientGuid = pair.first;
                final Integer tabId = pair.second;

                final RemoteClient remoteClient = visibleClients.get(clientGuid);
                if (remoteClient == null) {
                    return RecyclerView.NO_ID;
                }

                final RemoteTab remoteTab = remoteClient.tabs.get(tabId);
                if (remoteTab == null) {
                    return RecyclerView.NO_ID;
                }

                return remoteTab.url.hashCode();

            default:
                throw new IllegalStateException("Unexpected Home Panel item type");
        }
    }

    public int getClientsCount() {
        return hiddenClients.size() + visibleClients.size();
    }

    @UiThread
    public void setClients(List<RemoteClient> clients) {
        adapterList.clear();
        adapterList.add(null);

        hiddenClients.clear();
        visibleClients.clear();

        for (RemoteClient client : clients) {
            final String guid = client.guid;
            if (sState.isClientHidden(guid)) {
                hiddenClients.add(client);
            } else {
                visibleClients.put(guid, client);
                adapterList.addAll(getVisibleItems(client));
            }
        }

        // Add item for unhiding clients.
        if (!hiddenClients.isEmpty()) {
            adapterList.add(null);
        }

        notifyDataSetChanged();
    }

    private static List<Pair<String, Integer>> getVisibleItems(RemoteClient client) {
        List<Pair<String, Integer>> list = new LinkedList<>();
        final String guid = client.guid;
        list.add(new Pair<>(guid, -1));
        if (!sState.isClientCollapsed(client.guid)) {
            for (int i = 0; i < client.tabs.size(); i++) {
                list.add(new Pair<>(guid, i));
            }
        }
        return list;
    }

    public List<RemoteClient> getHiddenClients() {
        return hiddenClients;
    }

    public void toggleClient(int position) {
        final Pair<String, Integer> pair = adapterList.get(position);
        if (pair.second != -1) {
            return;
        }

        final String clientGuid = pair.first;
        final RemoteClient client = visibleClients.get(clientGuid);

        final boolean isCollapsed = sState.isClientCollapsed(clientGuid);

        sState.setClientCollapsed(clientGuid, !isCollapsed);
        notifyItemChanged(position);

        if (isCollapsed) {
            for (int i = client.tabs.size() - 1; i > -1; i--) {
                // Insert child tabs at the index right after the client item that was clicked.
                adapterList.add(position + 1, new Pair<>(clientGuid, i));
            }
            notifyItemRangeInserted(position + 1, client.tabs.size());
        } else {
            int i = client.tabs.size();
            while (i > 0) {
                adapterList.remove(position + 1);
                i--;
            }
            notifyItemRangeRemoved(position + 1, client.tabs.size());
        }
    }

    public void unhideClients(List<RemoteClient> selectedClients) {
        final int numClients = selectedClients.size();
        if (numClients == 0) {
            return;
        }

        final int insertionIndex = adapterList.size() - 1;
        int itemCount = numClients;

        for (RemoteClient client : selectedClients) {
            final String clientGuid = client.guid;

            sState.setClientHidden(clientGuid, false);
            hiddenClients.remove(client);

            visibleClients.put(clientGuid, client);
            sState.setClientCollapsed(clientGuid, false);
            adapterList.addAll(adapterList.size() - 1, getVisibleItems(client));

            itemCount += client.tabs.size();
        }

        notifyItemRangeInserted(insertionIndex, itemCount);

        final int hiddenDevicesIndex = adapterList.size() - 1;
        if (hiddenClients.isEmpty()) {
            // No more hidden clients, remove "unhide" item.
            adapterList.remove(hiddenDevicesIndex);
            notifyItemRemoved(hiddenDevicesIndex);
        } else {
            // Update "hidden clients" item because number of hidden clients changed.
            notifyItemChanged(hiddenDevicesIndex);
        }
    }

    public void removeItem(int position) {
        final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);
        switch (itemType) {
            case CLIENT:
                final String clientGuid = adapterList.get(position).first;
                final RemoteClient client = visibleClients.remove(clientGuid);
                final boolean hadHiddenClients = !hiddenClients.isEmpty();

                int removeCount = sState.isClientCollapsed(clientGuid) ? 1 : client.tabs.size() + 1;
                int c = removeCount;
                while (c > 0) {
                    adapterList.remove(position);
                    c--;
                }
                notifyItemRangeRemoved(position, removeCount);

                sState.setClientHidden(clientGuid, true);
                hiddenClients.add(client);

                if (!hadHiddenClients) {
                    // Add item for unhiding clients;
                    adapterList.add(null);
                    notifyItemInserted(adapterList.size() - 1);
                } else {
                    // Update "hidden clients" item because number of hidden clients changed.
                    notifyItemChanged(adapterList.size() - 1);
                }
                break;
        }
    }

    @Override
    public HomeContextMenuInfo makeContextMenuInfoFromPosition(View view, int position) {
        final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);
        HomeContextMenuInfo info;
        final Pair<String, Integer> pair = adapterList.get(position);
        switch (itemType) {
            case CHILD:
                info = new HomeContextMenuInfo(view, position, -1);
                return populateChildInfoFromTab(info, visibleClients.get(pair.first).tabs.get(pair.second));

            case CLIENT:
                info = new CombinedHistoryPanel.RemoteTabsClientContextMenuInfo(view, position, -1, visibleClients.get(pair.first));
                return info;
        }
        return null;
    }

    protected static HomeContextMenuInfo populateChildInfoFromTab(HomeContextMenuInfo info, RemoteTab tab) {
        info.url = tab.url;
        info.title = tab.title;
        return info;
    }

    /**
     * Return a relative "Last synced" time span for the given tab record.
     *
     * @param now local time.
     * @param time to format string for.
     * @return string describing time span
     */
    public static String getLastSyncedString(Context context, long now, long time) {
        if (new Date(time).before(EARLIEST_VALID_SYNCED_DATE)) {
            return context.getString(R.string.remote_tabs_never_synced);
        }
        final CharSequence relativeTimeSpanString = DateUtils.getRelativeTimeSpanString(time, now, DateUtils.MINUTE_IN_MILLIS);
        return context.getResources().getString(R.string.remote_tabs_last_synced, relativeTimeSpanString);
    }
}
