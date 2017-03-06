/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu;

import android.content.Context;
import android.content.res.Resources;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.fragment.BrowserFragment;
import org.mozilla.focus.utils.Browsers;

import java.util.ArrayList;
import java.util.List;

public class BrowserMenuAdapter extends RecyclerView.Adapter<BrowserMenuViewHolder> {
    static class MenuItem {
        public final int id;
        public final String label;

        public MenuItem(int id, String label) {
            this.id = id;
            this.label = label;
        }
    }

    private final BrowserMenu menu;
    private final BrowserFragment fragment;

    private List<MenuItem> items;

    public BrowserMenuAdapter(Context context, BrowserMenu menu, BrowserFragment fragment) {
        this.menu = menu;
        this.fragment = fragment;

        initializeMenu(context, fragment.getUrl());
    }

    private void initializeMenu(Context context, String url) {
        final Resources resources = context.getResources();
        final Browsers browsers = new Browsers(context, url);

        this.items = new ArrayList<>();

        items.add(new MenuItem(R.id.share, resources.getString(R.string.menu_share)));

        items.add(new MenuItem(R.id.open_firefox, resources.getString(
                R.string.menu_open_with_default_browser, "Firefox")));

        if (browsers.hasThirdPartyDefaultBrowser(context)) {
            items.add(new MenuItem(R.id.open_default, resources.getString(
                    R.string.menu_open_with_default_browser, browsers.getDefaultBrowser().loadLabel(
                            context.getPackageManager()))));
        }

        if (browsers.hasMultipleThirdPartyBrowsers(context)) {
            items.add(new MenuItem(R.id.open_select_browser, resources.getString(
                    R.string.menu_open_with_a_browser)));
        }

        items.add(new MenuItem(R.id.settings, resources.getString(R.string.menu_settings)));
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
            ((MenuItemViewHolder) holder).bind(items.get(position - 1));
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
        // Button toolbar + menu items
        return 1 + items.size();
    }
}
