/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.R;

import static org.mozilla.gecko.home.CombinedHistoryItem.ItemType;

public class RecentTabsAdapter extends RecyclerView.Adapter<CombinedHistoryItem> implements CombinedHistoryRecyclerView.AdapterContextMenuBuilder {
    private static final String LOGTAG = "GeckoRecentTabsAdapter";

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
    }

    @Override
    public int getItemCount() {
        return 1;
    }

    private CombinedHistoryItem.ItemType getItemTypeForPosition(int position) {
        if (position == 0) {
            return ItemType.NAVIGATION_BACK;
        }

        return ItemType.CLOSED_TAB;
    }

    @Override
    public int getItemViewType(int position) {
        return CombinedHistoryItem.ItemType.itemTypeToViewType(getItemTypeForPosition(position));
    }

    @Override
    public HomeContextMenuInfo makeContextMenuInfoFromPosition(View view, int position) {
        final CombinedHistoryItem.ItemType itemType = getItemTypeForPosition(position);
        final HomeContextMenuInfo info;

        return null;
    }
}
