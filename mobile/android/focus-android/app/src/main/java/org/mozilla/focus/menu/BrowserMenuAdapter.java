/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.fragment.BrowserFragment;

public class BrowserMenuAdapter extends RecyclerView.Adapter<BrowserMenuViewHolder> {
    static class MenuItem {
        public final int id;
        public final int label;

        public MenuItem(int id, int label) {
            this.id = id;
            this.label = label;
        }
    }

    private static final MenuItem[] MENU_ITEMS = new MenuItem[] {
            new MenuItem(R.id.share, R.string.menu_share),
            new MenuItem(R.id.open, R.string.menu_open_with),
            new MenuItem(R.id.settings, R.string.menu_settings)
    };

    private final BrowserMenu menu;
    private final BrowserFragment fragment;

    public BrowserMenuAdapter(BrowserMenu menu, BrowserFragment fragment) {
        this.menu = menu;
        this.fragment = fragment;
    }

    @Override
    public BrowserMenuViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        if (viewType == NavigationItemViewHolder.LAYOUT_ID) {
            return new NavigationItemViewHolder(inflater.inflate(R.layout.menu_navigation, parent, false), fragment);
        } else if (viewType == MenuItemViewHolder.LAYOUT_ID) {
            return new MenuItemViewHolder(inflater.inflate(R.layout.menu_item, parent, false));
        }

        throw new IllegalArgumentException("Unknown view type: " + viewType);
    }

    @Override
    public void onBindViewHolder(BrowserMenuViewHolder holder, int position) {
        holder.setMenu(menu);
        holder.setOnClickListener(fragment);

        if (position > 0) {
            ((MenuItemViewHolder) holder).bind(MENU_ITEMS[position - 1]);
        }
    }

    @Override
    public int getItemViewType(int position) {
        if (position == 0) {
            return NavigationItemViewHolder.LAYOUT_ID;
        } else {
            return MenuItemViewHolder.LAYOUT_ID;
        }
    }

    @Override
    public int getItemCount() {
        return 1 + MENU_ITEMS.length;
    }
}
