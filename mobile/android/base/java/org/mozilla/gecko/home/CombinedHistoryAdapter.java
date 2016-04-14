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
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;
import org.mozilla.gecko.home.CombinedHistoryPanel.SectionHeader;

import java.util.Collections;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class CombinedHistoryAdapter extends RecyclerView.Adapter<CombinedHistoryItem> {
    private static final String LOGTAG = "GeckoCombinedHistAdapt";

    public enum ItemType {
        CLIENT, HIDDEN_DEVICES, SECTION_HEADER, HISTORY, NAVIGATION_BACK, CHILD;

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
    private int remoteClientIndexOfParent = -1;
    private Cursor historyCursor;

    // Maintain group collapsed and hidden state. Only accessed from the UI thread.
    protected static RemoteTabsExpandableListState sState;

    // List of hidden remote clients.
    // Only accessed from the UI thread.
    protected final List<RemoteClient> hiddenClients = new ArrayList<>();

    // We use a sparse array to store each section header's position in the panel [more cheaply than a HashMap].
    private final SparseArray<CombinedHistoryPanel.SectionHeader> sectionHeaders;

    private final Context context;

    public CombinedHistoryAdapter(Context context, int savedParentIndex) {
        super();
        this.context = context;
        sectionHeaders = new SparseArray<>();

        // This races when multiple Fragments are created. That's okay: one
        // will win, and thereafter, all will be okay. If we create and then
        // drop an instance the shared SharedPreferences backing all the
        // instances will maintain the state for us. Since everything happens on
        // the UI thread, this doesn't even need to be volatile.
        if (sState == null) {
            sState = new RemoteTabsExpandableListState(GeckoSharedPrefs.forProfile(context));
        }
        remoteClientIndexOfParent = savedParentIndex;
    }

    public void setClients(List<RemoteClient> clients) {
        hiddenClients.clear();
        remoteClients.clear();

        final Iterator<RemoteClient> it = clients.iterator();
        while (it.hasNext()) {
            final RemoteClient client = it.next();
            if (sState.isClientHidden(client.guid)) {
                hiddenClients.add(client);
                it.remove();
            }
        }

        remoteClients = clients;

        // Add item for unhiding clients.
        if (!hiddenClients.isEmpty()) {
            remoteClients.add(null);
        }

        notifyItemRangeChanged(0, remoteClients.size());
    }

    public void setHistory(Cursor history) {
        historyCursor = history;
        populateSectionHeaders(historyCursor, sectionHeaders);
        notifyDataSetChanged();
    }

    public void removeItem(int position) {
        final ItemType  itemType = getItemTypeForPosition(position);
        switch (itemType) {
            case CLIENT:
                final boolean hadHiddenClients = !hiddenClients.isEmpty();
                final RemoteClient client = remoteClients.remove(transformAdapterPositionForDataStructure(ItemType.CLIENT, position));
                notifyItemRemoved(position);

                sState.setClientHidden(client.guid, true);
                hiddenClients.add(client);
                if (!hadHiddenClients) {
                    // Add item for unhiding clients;
                    remoteClients.add(null);
                } else {
                    // Update "hidden clients" item because number of hidden clients changed.
                    notifyItemChanged(remoteClients.size() - 1);
                }
                break;
            case HISTORY:
                notifyItemRemoved(position);
                break;
        }
    }

    public void unhideClients(List<RemoteClient> selectedClients) {
        if (selectedClients.size() == 0) {
            return;
        }

        for (RemoteClient client : selectedClients) {
            sState.setClientHidden(client.guid, false);
            hiddenClients.remove(client);
        }

        final int insertIndex = remoteClients.size() - 1;

        remoteClients.addAll(insertIndex, selectedClients);
        notifyItemRangeInserted(insertIndex, selectedClients.size());

        if (hiddenClients.isEmpty()) {
            // No more hidden clients, remove "unhide" item.
            remoteClients.remove(remoteClients.size() - 1);
        } else {
            // Update "hidden clients" item because number of hidden clients changed.
            notifyItemChanged(remoteClients.size() - 1);
        }
    }

    public List<RemoteClient> getHiddenClients() {
        return hiddenClients;
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

    public int getParentIndex() {
        return remoteClientIndexOfParent;
    }

    private boolean isInChildView() {
        return remoteClientIndexOfParent != -1;
    }

    public void showChildView(int parentPosition) {
        if (clientChildren == null) {
            clientChildren = new ArrayList<>();
        }
        // Handle "back" view.
        clientChildren.add(null);
        remoteClientIndexOfParent = transformAdapterPositionForDataStructure(ItemType.CLIENT, parentPosition);
        clientChildren.addAll(remoteClients.get(remoteClientIndexOfParent).tabs);
        notifyDataSetChanged();
    }

    public boolean exitChildView() {
        if (!isInChildView()) {
            return false;
        }
        remoteClientIndexOfParent = -1;
        clientChildren.clear();
        notifyDataSetChanged();
        return true;
    }

    private ItemType getItemTypeForPosition(int position) {
        return ItemType.viewTypeToItemType(getItemViewType(position));
    }

    /**
     * Transform an adapter position to the position for the data structure backing the item type.
     *
     * The type is not strictly necessary and could be fetched from <code>getItemTypeForPosition</code>,
     * but is used for explicitness.
     *
     * @param type ItemType of the item
     * @param position position in the adapter
     * @return position of the item in the data structure
     */
    private int transformAdapterPositionForDataStructure(ItemType type, int position) {
        if (type == ItemType.CLIENT) {
            return position;
        } else if (type == ItemType.SECTION_HEADER) {
            return position - remoteClients.size();
        } else if (type == ItemType.HISTORY) {
            return position - remoteClients.size() - getHeadersBefore(position);
        } else {
            return position;
        }
    }

    public HomeContextMenuInfo makeContextMenuInfoFromPosition(View view, int position) {
        final ItemType itemType = getItemTypeForPosition(position);
        HomeContextMenuInfo info;
        switch (itemType) {
            case CHILD:
                info = new HomeContextMenuInfo(view, position, -1);
                return CombinedHistoryPanel.populateChildInfoFromTab(info, clientChildren.get(position));
            case HISTORY:
                info = new HomeContextMenuInfo(view, position, -1);
                historyCursor.moveToPosition(transformAdapterPositionForDataStructure(ItemType.HISTORY, position));
                return CombinedHistoryPanel.populateHistoryInfoFromCursor(info, historyCursor);
            case CLIENT:
                final int clientPosition = transformAdapterPositionForDataStructure(ItemType.CLIENT, position);
                info = new CombinedHistoryPanel.RemoteTabsClientContextMenuInfo(view, position, -1, remoteClients.get(clientPosition));
                return info;
        }
        return null;
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

            case HIDDEN_DEVICES:
                view = inflater.inflate(R.layout.home_remote_tabs_hidden_devices, viewGroup, false);
                return new CombinedHistoryItem.BasicItem(view);

            case NAVIGATION_BACK:
                view = inflater.inflate(R.layout.home_combined_back_item, viewGroup, false);
                return new CombinedHistoryItem.HistoryItem(view);

            case SECTION_HEADER:
                view = inflater.inflate(R.layout.home_header_row, viewGroup, false);
                return new CombinedHistoryItem.BasicItem(view);

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
        if (isInChildView()) {
            if (position == 0) {
                return ItemType.itemTypeToViewType(ItemType.NAVIGATION_BACK);
            }
            return ItemType.itemTypeToViewType(ItemType.CHILD);
        } else {
            final int numClients = remoteClients.size();
            if (position < numClients) {
                if (!hiddenClients.isEmpty() && position == numClients - 1) {
                    return ItemType.itemTypeToViewType(ItemType.HIDDEN_DEVICES);
                }
                return ItemType.itemTypeToViewType(ItemType.CLIENT);
            }

            final int sectionPosition = transformAdapterPositionForDataStructure(ItemType.SECTION_HEADER, position);
            if (sectionHeaders.get(sectionPosition) != null) {
                return ItemType.itemTypeToViewType(ItemType.SECTION_HEADER);
            }

            return ItemType.itemTypeToViewType(ItemType.HISTORY);
        }
    }

    @Override
    public int getItemCount() {
        if (isInChildView()) {
            if (clientChildren == null) {
                clientChildren = new ArrayList<>();
                clientChildren.add(null);
                clientChildren.addAll(remoteClients.get(remoteClientIndexOfParent).tabs);
            }
            return clientChildren.size();
        } else {
            final int historySize = historyCursor == null ? 0 : historyCursor.getCount();
            return remoteClients.size() + historySize + sectionHeaders.size();
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
        final ItemType itemType = getItemTypeForPosition(position);
        final int localPosition = transformAdapterPositionForDataStructure(itemType, position);

        switch (itemType) {
            case CLIENT:
                final CombinedHistoryItem.ClientItem clientItem = (CombinedHistoryItem.ClientItem) viewHolder;
                final RemoteClient client = remoteClients.get(localPosition);
                clientItem.bind(client, context);
                break;

            case HIDDEN_DEVICES:
                final String hiddenDevicesLabel = context.getResources().getString(R.string.home_remote_tabs_many_hidden_devices, hiddenClients.size());
                ((TextView) viewHolder.itemView).setText(hiddenDevicesLabel);
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
