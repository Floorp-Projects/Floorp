/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;

import java.util.ArrayList;
import java.util.List;

class TabStripAdapter extends RecyclerView.Adapter<TabStripAdapter.TabStripViewHolder> {
    private static final String LOGTAG = "Gecko" + TabStripAdapter.class.getSimpleName();

    private @NonNull List<Tab> tabs;
    private final LayoutInflater inflater;

    static class TabStripViewHolder extends RecyclerView.ViewHolder {
        TabStripViewHolder(View itemView) {
            super(itemView);
        }
    }

    TabStripAdapter(Context context) {
        inflater = LayoutInflater.from(context);
        tabs = new ArrayList<>(0);
    }

    public void refresh(@NonNull List<Tab> tabs) {
        this.tabs = tabs;
        notifyDataSetChanged();
    }

    @Override
    public int getItemCount() {
        return tabs.size();
    }

    /* package */ int getPositionForTab(Tab tab) {
        if (tab == null) {
            return -1;
        }

        return tabs.indexOf(tab);
    }

    /* package */ void addTab(Tab tab, int position) {
        if (position >= 0 && position <= tabs.size()) {
            tabs.add(position, tab);
            notifyItemInserted(position);
        } else {
            // Add to the end.
            tabs.add(tab);
            notifyItemInserted(tabs.size() - 1);
            // index == -1 is a valid way to add to the end, the other cases are errors.
            if (position != -1) {
                Log.e(LOGTAG, "Tab was inserted at an invalid position: " + position);
            }
        }
    }

    /* package */ void removeTab(Tab tab) {
        final int position = getPositionForTab(tab);
        if (position == -1) {
            return;
        }
        tabs.remove(position);
        notifyItemRemoved(position);
    }

    /* package */ void notifyTabChanged(Tab tab) {
        final int position =  getPositionForTab(tab);
        if (position == -1) {
            return;
        }
        notifyItemChanged(position);
    }

    /* package */ void clear() {
        tabs = new ArrayList<>(0);
        notifyDataSetChanged();
    }

    @Override
    public TabStripViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        final TabStripItemView view = (TabStripItemView) inflater.inflate(R.layout.tab_strip_item, parent, false);

        return new TabStripViewHolder(view);
    }

    @Override
    public void onBindViewHolder(TabStripViewHolder viewHolder, int position) {
        final Tab tab = tabs.get(position);
        final TabStripItemView itemView = (TabStripItemView) viewHolder.itemView;
        itemView.updateFromTab(tab);
    }
}
