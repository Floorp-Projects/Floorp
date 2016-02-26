/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.support.v7.widget.RecyclerView;

import android.content.Context;
import android.database.Cursor;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import org.json.JSONArray;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;

import java.util.Collections;
import java.util.ArrayList;
import java.util.List;

public class CombinedHistoryAdapter extends RecyclerView.Adapter<CombinedHistoryItem> {
    private static final String LOGTAG = "GeckoCombinedHistAdapt";

    public enum ItemType {
        CLIENT, HISTORY, NAVIGATION_BACK, CHILD;

        public static ItemType viewTypeToItemType(int viewType) {
            if (viewType >= ItemType.values().length) {
                Log.e(LOGTAG, "No corresponding ItemType!");
            }
            return ItemType.values()[viewType];
        }

        public static int itemTypeToViewType(ItemType itemType) {
            return itemType.ordinal();
        }
    }

    private List<RemoteClient> remoteClients = Collections.emptyList();
    private List<RemoteTab> clientChildren;
    private Cursor historyCursor;
    private final Context context;

    private boolean inChildView = false;

    public CombinedHistoryAdapter(Context context) {
        super();
        this.context = context;
    }

    public void setClients(List<RemoteClient> clients) {
        remoteClients = clients;
        notifyDataSetChanged();
    }

    public void setHistory(Cursor history) {
        historyCursor = history;
        notifyDataSetChanged();
    }

    public JSONArray getCurrentChildTabs() {
        if (clientChildren != null) {
            final JSONArray urls = new JSONArray();
            for (int i = 1; i < clientChildren.size(); i++) {
                urls.put(clientChildren.get(i).url);
            }
            return urls;
        }
        return null;
    }

    public void showChildView(int parentPosition) {
        if (clientChildren == null) {
            clientChildren = new ArrayList<>();
        }
        // Handle "back" view.
        clientChildren.add(null);
        clientChildren.addAll(remoteClients.get(transformPosition(ItemType.CLIENT, parentPosition)).tabs);
        inChildView = true;
        notifyDataSetChanged();
    }

    public void exitChildView() {
        inChildView = false;
        clientChildren.clear();
        notifyDataSetChanged();
    }

    private int transformPosition(ItemType type, int position) {
        if (type == ItemType.CLIENT) {
            return position;
        } else if (type == ItemType.HISTORY){
            return position - remoteClients.size();
        } else {
            return position;
        }
    }

    @Override
    public CombinedHistoryItem onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(viewGroup.getContext());
        final View view;

        final ItemType itemType = ItemType.viewTypeToItemType(viewType);

        switch (itemType) {
            case CLIENT:
                view = inflater.inflate(R.layout.home_remote_tabs_group, viewGroup, false);
                return new CombinedHistoryItem.ClientItem(view);

            case NAVIGATION_BACK:
                view = inflater.inflate(R.layout.home_combined_back_item, viewGroup, false);
                return new CombinedHistoryItem.HistoryItem(view);

            case CHILD:
            case HISTORY:
                view = inflater.inflate(R.layout.home_item_row, viewGroup, false);
                return new CombinedHistoryItem.HistoryItem(view);
            default:
                throw new IllegalArgumentException("Unexpected Home Panel item type");
        }
    }

    @Override
    public int getItemViewType(int position) {
        if (inChildView) {
            if (position == 0) {
                return ItemType.itemTypeToViewType(ItemType.NAVIGATION_BACK);
            } else {
                return ItemType.itemTypeToViewType(ItemType.CHILD);
            }
        } else {
            final int numClients = remoteClients.size();
            return (position < numClients) ? ItemType.itemTypeToViewType(ItemType.CLIENT) : ItemType.itemTypeToViewType(ItemType.HISTORY);
        }
    }

    @Override
    public int getItemCount() {

        if (inChildView) {
            return (clientChildren == null) ? 0 : clientChildren.size();
        } else {
            final int remoteSize = remoteClients.size();
            final int historySize = historyCursor == null ? 0 : historyCursor.getCount();
            return remoteSize + historySize;
        }
    }

    @Override
    public void onBindViewHolder(CombinedHistoryItem viewHolder, int position) {
        final ItemType itemType = ItemType.viewTypeToItemType(getItemViewType(position));
        final int localPosition = transformPosition(itemType, position);

        switch (itemType) {
            case CLIENT:
                final CombinedHistoryItem.ClientItem clientItem = (CombinedHistoryItem.ClientItem) viewHolder;
                final RemoteClient client = remoteClients.get(localPosition);
                clientItem.bind(client, context);
                break;

            case CHILD:
                RemoteTab remoteTab = clientChildren.get(position);
                ((CombinedHistoryItem.HistoryItem) viewHolder).bind(remoteTab);
                break;

            case HISTORY:
                if (historyCursor == null || !historyCursor.moveToPosition(localPosition)) {
                    throw new IllegalStateException("Couldn't move cursor to position " + localPosition);
                }
                ((CombinedHistoryItem.HistoryItem) viewHolder).bind(historyCursor);
                break;
        }
    }
}
