/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.Tab;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import java.util.ArrayList;

public class TabsLayoutAdapter
        extends RecyclerView.Adapter<TabsLayoutAdapter.TabsListViewHolder> {

    private static final String LOGTAG = "Gecko" + TabsLayoutAdapter.class.getSimpleName();

    private final int tabLayoutId;
    private @NonNull ArrayList<Tab> tabs;
    private final LayoutInflater inflater;
    private final boolean isPrivate;
    // Click listener for the close button on itemViews.
    private final Button.OnClickListener closeOnClickListener;

    // The TabsLayoutItemView takes care of caching its own Views, so we don't need to do anything
    // here except not be abstract.
    public static class TabsListViewHolder extends RecyclerView.ViewHolder {
        public TabsListViewHolder(View itemView) {
            super(itemView);
        }
    }

    public TabsLayoutAdapter(Context context, int tabLayoutId, boolean isPrivate,
                             Button.OnClickListener closeOnClickListener) {
        inflater = LayoutInflater.from(context);
        this.tabLayoutId = tabLayoutId;
        this.isPrivate = isPrivate;
        this.closeOnClickListener = closeOnClickListener;
        tabs = new ArrayList<>(0);
    }

    /* package */ final void setTabs(@NonNull ArrayList<Tab> tabs) {
        this.tabs = tabs;
        notifyDataSetChanged();
    }

    /* package */ final void clear() {
        tabs = new ArrayList<>(0);
        notifyDataSetChanged();
    }

    /* package */ final boolean removeTab(Tab tab) {
        final int position = getPositionForTab(tab);
        if (position == -1) {
            return false;
        }
        tabs.remove(position);
        notifyItemRemoved(position);
        return true;
    }

    /* package */ final int getPositionForTab(Tab tab) {
        if (tab == null) {
            return -1;
        }

        return tabs.indexOf(tab);
    }

    /* package */ void notifyTabChanged(Tab tab) {
        final int position = getPositionForTab(tab);
        if (position != -1) {
            notifyItemChanged(position);
        }
    }

    /* package */ void notifyTabInserted(Tab tab, int index) {
        if (index >= 0 && index <= tabs.size()) {
            tabs.add(index, tab);
            notifyItemInserted(index);
        } else {
            // Add to the end.
            tabs.add(tab);
            notifyItemInserted(tabs.size() - 1);
            // index == -1 is a valid way to add to the end, the other cases are errors.
            if (index != -1) {
                Log.e(LOGTAG, "Tab was inserted at an invalid position: " + Integer.toString(index));
            }
        }
    }

    @Override
    public int getItemCount() {
        return tabs.size();
    }

    private Tab getItem(int position) {
        return tabs.get(position);
    }

    @Override
    public void onBindViewHolder(TabsListViewHolder viewHolder, int position) {
        final Tab tab = getItem(position);
        final TabsLayoutItemView itemView = (TabsLayoutItemView) viewHolder.itemView;
        itemView.assignValues(tab);
        // Be careful (re)setting position values here: bind is called on each notifyItemChanged,
        // so you could be stomping on values that have been set in support of other animations
        // that are already underway.
    }

    @Override
    public TabsListViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        final TabsLayoutItemView viewItem = (TabsLayoutItemView) inflater.inflate(tabLayoutId, parent, false);
        viewItem.setPrivateMode(isPrivate);
        viewItem.setCloseOnClickListener(closeOnClickListener);

        return new TabsListViewHolder(viewItem);
    }
}
