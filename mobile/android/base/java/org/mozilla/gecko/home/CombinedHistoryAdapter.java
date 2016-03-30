/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.support.v7.widget.RecyclerView;

import android.content.Context;
import android.database.Cursor;
import android.util.Log;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import org.json.JSONArray;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;
import org.mozilla.gecko.home.CombinedHistoryPanel.SectionHeader;

import java.util.Collections;
import java.util.ArrayList;
import java.util.List;

public class CombinedHistoryAdapter extends RecyclerView.Adapter<CombinedHistoryItem> {
    private static final String LOGTAG = "GeckoCombinedHistAdapt";

    public enum ItemType {
        CLIENT, SECTION_HEADER, HISTORY, NAVIGATION_BACK, CHILD;

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

    // We use a sparse array to store each section header's position in the panel [more cheaply than a HashMap].
    private final SparseArray<CombinedHistoryPanel.SectionHeader> sectionHeaders;

    private final Context context;

    private boolean inChildView = false;

    public CombinedHistoryAdapter(Context context) {
        super();
        this.context = context;
        sectionHeaders = new SparseArray<>();
    }

    public void setClients(List<RemoteClient> clients) {
        remoteClients = clients;
        notifyDataSetChanged();
    }

    public void setHistory(Cursor history) {
        historyCursor = history;
        populateSectionHeaders(historyCursor, sectionHeaders);
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
        } else if (type == ItemType.SECTION_HEADER) {
            return position - remoteClients.size();
        } else if (type == ItemType.HISTORY){
            return position - remoteClients.size() - getHeadersBefore(position);
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

            case SECTION_HEADER:
                view = inflater.inflate(R.layout.home_header_row, viewGroup, false);
                return new CombinedHistoryItem.SectionItem(view);

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
            }
            return ItemType.itemTypeToViewType(ItemType.CHILD);
        } else {
            final int numClients = remoteClients.size();
            if (position < numClients) {
                return ItemType.itemTypeToViewType(ItemType.CLIENT);
            }

            final int sectionPosition = transformPosition(ItemType.SECTION_HEADER, position);
            if (sectionHeaders.get(sectionPosition) != null) {
                return ItemType.itemTypeToViewType(ItemType.SECTION_HEADER);
            }

            return ItemType.itemTypeToViewType(ItemType.HISTORY);
        }
    }

    @Override
    public int getItemCount() {
        if (inChildView) {
            return (clientChildren == null) ? 0 : clientChildren.size();
        } else {
            final int remoteSize = remoteClients.size();
            final int historySize = historyCursor == null ? 0 : historyCursor.getCount();
            return remoteSize + historySize + sectionHeaders.size();
        }
    }

    /**
     * Add only the SectionHeaders that have history items within their range to a SparseArray, where the
     * array index is the position of the header in the history-only (no clients) ordering.
     * @param c data Cursor
     * @param sparseArray SparseArray to populate
     */
    private static void populateSectionHeaders(Cursor c, SparseArray<SectionHeader> sparseArray) {
        sparseArray.clear();

        if (c == null || !c.moveToFirst()) {
            return;
        }

        SectionHeader section = null;

        do {
            final int historyPosition = c.getPosition();
            final long visitTime = c.getLong(c.getColumnIndexOrThrow(BrowserContract.History.DATE_LAST_VISITED));
            final SectionHeader itemSection = CombinedHistoryPanel.getSectionFromTime(visitTime);

            if (section != itemSection) {
                section = itemSection;
                sparseArray.append(historyPosition + sparseArray.size(), section);
            }

            if (section == SectionHeader.OLDER_THAN_SIX_MONTHS) {
                break;
            }
        } while (c.moveToNext());
    }


    public boolean containsHistory() {
        if (historyCursor == null) {
            return false;
        }
        return (historyCursor.getCount() > 0);
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

            case SECTION_HEADER:
                ((TextView) viewHolder.itemView).setText(CombinedHistoryPanel.getSectionHeaderTitle(sectionHeaders.get(localPosition)));
                break;

            case HISTORY:
                if (historyCursor == null || !historyCursor.moveToPosition(localPosition)) {
                    throw new IllegalStateException("Couldn't move cursor to position " + localPosition);
                }
                ((CombinedHistoryItem.HistoryItem) viewHolder).bind(historyCursor);
                break;
        }
    }

    /**
     * Returns the number of section headers before the given history item at the adapter position.
     * @param position position in the adapter
     */
    private int getHeadersBefore(int position) {
        final int adjustedPosition = position - remoteClients.size();
        // Skip the first header case because there will always be a header.
        for (int i = 1; i < sectionHeaders.size(); i++) {
            // If the position of the header is greater than the history position,
            // return the number of headers tested.
            if (sectionHeaders.keyAt(i) > adjustedPosition) {
                return i;
            }
        }
        return sectionHeaders.size();
    }
}
