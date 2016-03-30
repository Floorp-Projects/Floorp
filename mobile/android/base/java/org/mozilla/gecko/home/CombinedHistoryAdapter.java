/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.database.Cursor;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.RemoteClient;

import java.util.Collections;
import java.util.List;

public class CombinedHistoryAdapter extends RecyclerView.Adapter<CombinedHistoryItem> {
    // These ordinal positions are used in CombinedHistoryAdapter as viewType.
    private enum ItemType {
        CLIENT, HISTORY
    }

    private List<RemoteClient> remoteClients = Collections.emptyList();
    private Cursor historyCursor;

    @Override
    public CombinedHistoryItem onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(viewGroup.getContext());
        final View view;
        if (viewType == ItemType.CLIENT.ordinal()) {
            view = inflater.inflate(R.layout.home_remote_tabs_group, viewGroup, false);
            return new CombinedHistoryItem.ClientItem(view);
        } else {
            view = inflater.inflate(R.layout.home_item_row, viewGroup, false);
            return new CombinedHistoryItem.HistoryItem(view);
        }
    }

    @Override
    public int getItemViewType(int position) {
        final int numClients = remoteClients.size();
        return (position < numClients) ? ItemType.CLIENT.ordinal() : ItemType.HISTORY.ordinal();
    }

    @Override
    public int getItemCount() {
        final int remoteSize = remoteClients.size();
        final int historySize = historyCursor == null ? 0 : historyCursor.getCount();
        return remoteSize + historySize;
    }

    @Override
    public void onBindViewHolder(CombinedHistoryItem viewHolder, int position) {}

}
