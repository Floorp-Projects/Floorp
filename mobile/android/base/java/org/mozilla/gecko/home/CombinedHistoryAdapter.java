/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.content.res.Resources;
import android.support.v7.widget.RecyclerView;

import android.database.Cursor;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;

public class CombinedHistoryAdapter extends RecyclerView.Adapter<CombinedHistoryItem> implements CombinedHistoryRecyclerView.AdapterContextMenuBuilder {
    private static final int RECENT_TABS_SMARTFOLDER_INDEX = 0;
    private static final int SYNCED_DEVICES_SMARTFOLDER_INDEX = 1;

    // Array for the time ranges in milliseconds covered by each section.
    static final HistorySectionsHelper.SectionDateRange[] sectionDateRangeArray = new HistorySectionsHelper.SectionDateRange[SectionHeader.values().length];

    // Semantic names for the time covered by each section
    public enum SectionHeader {
        TODAY,
        YESTERDAY,
        WEEK,
        THIS_MONTH,
        MONTH_AGO,
        TWO_MONTHS_AGO,
        THREE_MONTHS_AGO,
        FOUR_MONTHS_AGO,
        FIVE_MONTHS_AGO,
        OLDER_THAN_SIX_MONTHS
    }

    private Cursor historyCursor;
    private DevicesUpdateHandler devicesUpdateHandler;
    private int deviceCount = 0;
    private int recentTabsCount = 0;

    // We use a sparse array to store each section header's position in the panel [more cheaply than a HashMap].
    private final SparseArray<SectionHeader> sectionHeaders;

    public CombinedHistoryAdapter(Resources resources) {
        super();
        sectionHeaders = new SparseArray<>();
        HistorySectionsHelper.updateRecentSectionOffset(resources, sectionDateRangeArray);
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
                    notifyItemChanged(SYNCED_DEVICES_SMARTFOLDER_INDEX);
                }
            };
        }
        return devicesUpdateHandler;
    }

    @Override
    public CombinedHistoryItem onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(viewGroup.getContext());
        final View view;

        final CombinedHistoryItem.ItemType itemType = CombinedHistoryItem.ItemType.viewTypeToItemType(viewType);

        switch (itemType) {
            case RECENT_TABS:
            case SYNCED_DEVICES:
                view = inflater.inflate(R.layout.home_smartfolder, viewGroup, false);
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
            case RECENT_TABS:
                ((CombinedHistoryItem.SmartFolder) viewHolder).bind(R.drawable.icon_recent, R.string.home_closed_tabs_title2, R.string.home_closed_tabs_one, R.string.home_closed_tabs_number, recentTabsCount);
                break;

            case SYNCED_DEVICES:
                ((CombinedHistoryItem.SmartFolder) viewHolder).bind(R.drawable.cloud, R.string.home_synced_devices_smartfolder, R.string.home_synced_devices_one, R.string.home_synced_devices_number, deviceCount);
                break;

            case SECTION_HEADER:
                ((TextView) viewHolder.itemView).setText(getSectionHeaderTitle(sectionHeaders.get(localPosition)));
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
        } else if (type == CombinedHistoryItem.ItemType.HISTORY) {
            return position - getHeadersBefore(position) - CombinedHistoryPanel.NUM_SMART_FOLDERS;
        } else {
            return position;
        }
    }

    private CombinedHistoryItem.ItemType getItemTypeForPosition(int position) {
        if (position == RECENT_TABS_SMARTFOLDER_INDEX) {
            return CombinedHistoryItem.ItemType.RECENT_TABS;
        }
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
            final SectionHeader itemSection = getSectionFromTime(visitTime);

            if (section != itemSection) {
                section = itemSection;
                sparseArray.append(historyPosition + sparseArray.size() + CombinedHistoryPanel.NUM_SMART_FOLDERS, section);
            }

            if (section == SectionHeader.OLDER_THAN_SIX_MONTHS) {
                break;
            }
        } while (c.moveToNext());
    }

    private static String getSectionHeaderTitle(SectionHeader section) {
        return sectionDateRangeArray[section.ordinal()].displayName;
    }

    private static SectionHeader getSectionFromTime(long time) {
        for (int i = 0; i < SectionHeader.OLDER_THAN_SIX_MONTHS.ordinal(); i++) {
            if (time > sectionDateRangeArray[i].start) {
                return SectionHeader.values()[i];
            }
        }

        return SectionHeader.OLDER_THAN_SIX_MONTHS;
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

    @Override
    public HomeContextMenuInfo makeContextMenuInfoFromPosition(View view, int position) {
        final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);
        if (itemType == CombinedHistoryItem.ItemType.HISTORY) {
            final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, -1);

            historyCursor.moveToPosition(transformAdapterPositionForDataStructure(CombinedHistoryItem.ItemType.HISTORY, position));
            return populateHistoryInfoFromCursor(info, historyCursor);
        }
        return null;
    }

    protected static HomeContextMenuInfo populateHistoryInfoFromCursor(HomeContextMenuInfo info, Cursor cursor) {
        info.url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));
        info.title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.TITLE));
        info.historyId = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Combined.HISTORY_ID));
        info.itemType = HomeContextMenuInfo.RemoveItemType.HISTORY;
        final int bookmarkIdCol = cursor.getColumnIndexOrThrow(BrowserContract.Combined.BOOKMARK_ID);
        if (cursor.isNull(bookmarkIdCol)) {
            // If this is a combined cursor, we may get a history item without a
            // bookmark, in which case the bookmarks ID column value will be null.
            info.bookmarkId =  -1;
        } else {
            info.bookmarkId = cursor.getInt(bookmarkIdCol);
        }
        return info;
    }

}
