/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import java.util.ArrayList;

// Adapter to bind tabs into a list
public class TabsLayoutAdapter extends BaseAdapter {
    public static final String LOGTAG = "Gecko" + TabsLayoutAdapter.class.getSimpleName();

    private final Context mContext;
    private final int mTabLayoutId;
    private ArrayList<Tab> mTabs;
    private final LayoutInflater mInflater;

    public TabsLayoutAdapter (Context context, int tabLayoutId) {
        mContext = context;
        mInflater = LayoutInflater.from(mContext);
        mTabLayoutId = tabLayoutId;
    }

    final void setTabs (ArrayList<Tab> tabs) {
        mTabs = tabs;
        notifyDataSetChanged(); // Be sure to call this whenever mTabs changes.
    }

    final boolean removeTab (Tab tab) {
        boolean tabRemoved = mTabs.remove(tab);
        if (tabRemoved) {
            notifyDataSetChanged(); // Be sure to call this whenever mTabs changes.
        }
        return tabRemoved;
    }

    final void clear() {
        mTabs = null;
        notifyDataSetChanged(); // Be sure to call this whenever mTabs changes.
    }

    @Override
    public int getCount() {
        return (mTabs == null ? 0 : mTabs.size());
    }

    @Override
    public Tab getItem(int position) {
        return mTabs.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    final int getPositionForTab(Tab tab) {
        if (mTabs == null || tab == null)
            return -1;

        return mTabs.indexOf(tab);
    }

    @Override
    final public TabsLayoutItemView getView(int position, View convertView, ViewGroup parent) {
        final TabsLayoutItemView view;
        if (convertView == null) {
            view = newView(position, parent);
        } else {
            view = (TabsLayoutItemView) convertView;
        }
        final Tab tab = mTabs.get(position);
        bindView(view, tab);
        return view;
    }

    TabsLayoutItemView newView(int position, ViewGroup parent) {
        return (TabsLayoutItemView) mInflater.inflate(mTabLayoutId, parent, false);
    }

    void bindView(TabsLayoutItemView view, Tab tab) {
        view.assignValues(tab);
    }
}