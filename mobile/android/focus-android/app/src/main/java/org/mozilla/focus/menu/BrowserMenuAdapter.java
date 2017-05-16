/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu;

import android.app.PendingIntent;
import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.fragment.BrowserFragment;
import org.mozilla.focus.utils.Browsers;
import org.mozilla.focus.utils.HardwareUtils;
import org.mozilla.focus.web.CustomTabConfig;

import java.util.ArrayList;
import java.util.List;

public class BrowserMenuAdapter extends RecyclerView.Adapter<BrowserMenuViewHolder> {
    static class MenuItem {
        public final int id;
        public final String label;
        public final @Nullable PendingIntent pendingIntent;

        public MenuItem(int id, String label) {
            this(id, label, null);
        }

        public MenuItem(int id, String label, @Nullable PendingIntent pendingIntent) {
            this.id = id;
            this.label = label;
            this.pendingIntent = pendingIntent;
        }
    }

    private final Context context;
    private final BrowserMenu menu;
    private final BrowserFragment fragment;

    private List<MenuItem> items;

    public BrowserMenuAdapter(Context context, BrowserMenu menu, BrowserFragment fragment,
                              final @Nullable CustomTabConfig customTabConfig) {
        this.context = context;
        this.menu = menu;
        this.fragment = fragment;

        initializeMenu(fragment.getUrl(), customTabConfig);
    }

    private void initializeMenu(String url, final @Nullable CustomTabConfig customTabConfig) {
        final Resources resources = context.getResources();
        final Browsers browsers = new Browsers(context, url);

        this.items = new ArrayList<>();

        if (customTabConfig == null || customTabConfig.showShareMenuItem) {
            items.add(new MenuItem(R.id.share, resources.getString(R.string.menu_share)));
        }

        if (browsers.hasFirefoxBrandedBrowserInstalled()) {
            items.add(new MenuItem(R.id.open_firefox, resources.getString(
                    R.string.menu_open_with_default_browser, browsers.getFirefoxBrandedBrowser()
                            .loadLabel(context.getPackageManager()))));
        } else {
            items.add(new MenuItem(R.id.open_firefox, resources.getString(
                    R.string.menu_open_with_default_browser, "Firefox")));
        }

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

        if (customTabConfig != null) {
            addCustomTabMenuItems(items, customTabConfig);
        }
    }

    private void addCustomTabMenuItems(final List<MenuItem> items, final @NonNull CustomTabConfig customTabConfig) {
        for (final CustomTabConfig.CustomTabMenuItem inputItem : customTabConfig.menuItems) {
            final String name = inputItem.name;
            final PendingIntent pendingIntent = inputItem.pendingIntent;

            final MenuItem item = new MenuItem(R.id.custom_tab_menu_item, name, pendingIntent);
            items.add(item);
        }
    }

    @Override
    public BrowserMenuViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        if (viewType == NavigationItemViewHolder.LAYOUT_ID) {
            return new NavigationItemViewHolder(inflater.inflate(R.layout.menu_navigation, parent, false), fragment);
        } else if (viewType == MenuItemViewHolder.LAYOUT_ID) {
            return new MenuItemViewHolder(inflater.inflate(R.layout.menu_item, parent, false));
        } else if (viewType == BlockingItemViewHolder.LAYOUT_ID) {
            return new BlockingItemViewHolder(inflater.inflate(R.layout.menu_blocking_switch, parent, false), fragment);
        } else if (viewType == CustomTabMenuItemViewHolder.LAYOUT_ID) {
            return new CustomTabMenuItemViewHolder(inflater.inflate(R.layout.custom_tab_menu_item, parent, false));
        }

        throw new IllegalArgumentException("Unknown view type: " + viewType);
    }

    @Override
    public void onBindViewHolder(BrowserMenuViewHolder holder, int position) {
        holder.setMenu(menu);
        holder.setOnClickListener(fragment);

        int actualPosition = translateToMenuPosition(position);

        if (actualPosition >= 0 && position != getBlockingSwitchPosition()) {
            ((MenuItemViewHolder) holder).bind(items.get(actualPosition));
        }
    }

    private int translateToMenuPosition(int position) {
        return shouldShowButtonToolbar() ? position - 2 : position - 1;
    }

    @Override
    public int getItemViewType(int position) {
        if (position == 0 && shouldShowButtonToolbar()) {
            return NavigationItemViewHolder.LAYOUT_ID;
        } else if (position == getBlockingSwitchPosition()) {
            return BlockingItemViewHolder.LAYOUT_ID;
        } else {
            final int actualPosition = translateToMenuPosition(position);
            final MenuItem menuItem = items.get(actualPosition);

            if (menuItem.id == R.id.custom_tab_menu_item) {
                return CustomTabMenuItemViewHolder.LAYOUT_ID;
            } else {
                return MenuItemViewHolder.LAYOUT_ID;
            }
        }
    }

    private int getBlockingSwitchPosition() {
        return shouldShowButtonToolbar() ? 1 : 0;
    }

    @Override
    public int getItemCount() {
        int itemCount = items.size();

        if (shouldShowButtonToolbar()) {
            itemCount++;
        }

        // For the blocking switch
        itemCount++;

        return itemCount;
    }

    private boolean shouldShowButtonToolbar() {
        // On phones we show an extra row with toolbar items (forward/refresh)
        return !HardwareUtils.isTablet(context);
    }
}
