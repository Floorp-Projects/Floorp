/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.support.v7.widget.RecyclerView;

import android.database.Cursor;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.home.CombinedHistoryPanel.SectionHeader;

public class CombinedHistoryAdapter extends RecyclerView.Adapter<CombinedHistoryItem> implements CombinedHistoryRecyclerView.AdapterContextMenuBuilder {
    private static final int SYNCED_DEVICES_SMARTFOLDER_INDEX = 0;

    private Cursor historyCursor;
    private DevicesUpdateHandler devicesUpdateHandler;
    private int deviceCount = 0;

    // We use a sparse array to store each section header's position in the panel [more cheaply than a HashMap].
    private final SparseArray<CombinedHistoryPanel.SectionHeader> sectionHeaders;

    public CombinedHistoryAdapter() {
        super();
        sectionHeaders = new SparseArray<>();
    }

    public void setHistory(Cursor history) {
        historyCursor = history;
        populateSectionHeaders(historyCursor, sectionHeaders);
        notifyDataSetChanged();
    }

    public interface DevicesUpdateHandler {
        void onDeviceCountUpdated(int count);
    }

    public DevicesUpdateHandler getDeviceUpdateHandler() {
        if (devicesUpdateHandler == null) {
            devicesUpdateHandler = new DevicesUpdateHandler() {
                @Override
                public void onDeviceCountUpdated(int count) {
                    deviceCount = count;
                    notifyItemChanged(0);
                }
            };
        }
        return devicesUpdateHandler;
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
    private int transformAdapterPositionForDataStructure(CombinedHistoryItem.ItemType type, int position) {
        if (type == CombinedHistoryItem.ItemType.SECTION_HEADER) {
            return position;
        } else if (type == CombinedHistoryItem.ItemType.HISTORY){
            return position - getHeadersBefore(position) - CombinedHistoryPanel.NUM_SMART_FOLDERS;
        } else {
            return position;
        }
    }

    @Override
    public HomeContextMenuInfo makeContextMenuInfoFromPosition(View view, int position) {
        HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, -1);
        historyCursor.moveToPosition(transformAdapterPositionForDataStructure(CombinedHistoryItem.ItemType.HISTORY, position));
        return CombinedHistoryPanel.populateHistoryInfoFromCursor(info, historyCursor);
    }

    @Override
    public CombinedHistoryItem onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(viewGroup.getContext());
        final View view;

        final CombinedHistoryItem.ItemType itemType = CombinedHistoryItem.ItemType.viewTypeToItemType(viewType);

        switch (itemType) {
            case SYNCED_DEVICES:
                view = inflater.inflate(R.layout.home_remote_tabs_group, viewGroup, false);
                return new CombinedHistoryItem.SmartFolder(view);

            case SECTION_HEADER:
                view = inflater.inflate(R.layout.home_header_row, viewGroup, false);
                return new CombinedHistoryItem.BasicItem(view);

            case HISTORY:
                view = inflater.inflate(R.layout.home_item_row, viewGroup, false);
                return new CombinedHistoryItem.HistoryItem(view);
            default:
                throw new IllegalArgumentException("Unexpected Home Panel item type");
        }
    }

    @Override
    public void onBindViewHolder(CombinedHistoryItem viewHolder, int position) {
        final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);
        final int localPosition = transformAdapterPositionForDataStructure(itemType, position);

        switch (itemType) {
            case SYNCED_DEVICES:
                ((CombinedHistoryItem.SmartFolder) viewHolder).bind(R.drawable.cloud, R.string.home_synced_devices_smartfolder, R.string.home_synced_devices_number, deviceCount);
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

    private CombinedHistoryItem.ItemType getItemTypeForPosition(int position) {
        if (position == SYNCED_DEVICES_SMARTFOLDER_INDEX) {
            return CombinedHistoryItem.ItemType.SYNCED_DEVICES;
        }
        final int sectionPosition = transformAdapterPositionForDataStructure(CombinedHistoryItem.ItemType.SECTION_HEADER, position);
        if (sectionHeaders.get(sectionPosition) != null) {
            return CombinedHistoryItem.ItemType.SECTION_HEADER;
        }
        return CombinedHistoryItem.ItemType.HISTORY;
    }

    @Override
    public int getItemViewType(int position) {
        return CombinedHistoryItem.ItemType.itemTypeToViewType(getItemTypeForPosition(position));
    }

    @Override
    public int getItemCount() {
        final int historySize = historyCursor == null ? 0 : historyCursor.getCount();
        return historySize + sectionHeaders.size() + CombinedHistoryPanel.NUM_SMART_FOLDERS;
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
                sparseArray.append(historyPosition + sparseArray.size() + CombinedHistoryPanel.NUM_SMART_FOLDERS, section);
            }

            if (section == SectionHeader.OLDER_THAN_SIX_MONTHS) {
                break;
            }
        } while (c.moveToNext());
    }

    /**
     * Returns the number of section headers before the given history item at the adapter position.
     * @param position position in the adapter
     */
    private int getHeadersBefore(int position) {
        // Skip the first header case because there will always be a header.
        for (int i = 1; i < sectionHeaders.size(); i++) {
            // If the position of the header is greater than the history position,
            // return the number of headers tested.
            if (sectionHeaders.keyAt(i) > position) {
                return i;
            }
        }
        return sectionHeaders.size();
    }
}
