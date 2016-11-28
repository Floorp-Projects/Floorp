/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.graphics.drawable.Drawable;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;
import org.mozilla.gecko.home.RecentTabsAdapter.ClosedTab;

public abstract class CombinedHistoryItem extends RecyclerView.ViewHolder {
    private static final String LOGTAG = "CombinedHistoryItem";

    public CombinedHistoryItem(View view) {
        super(view);
    }

    public enum ItemType {
        CLIENT, HIDDEN_DEVICES, SECTION_HEADER, HISTORY, NAVIGATION_BACK, CHILD, SYNCED_DEVICES,
        RECENT_TABS, CLOSED_TAB;

        public static ItemType viewTypeToItemType(int viewType) {
            if (viewType >= ItemType.values().length) {
                Log.e(LOGTAG, "No corresponding ItemType!");
            }
            return ItemType.values()[viewType];
        }

        public static int itemTypeToViewType(ItemType itemType) {
            return itemType.ordinal();
        }
    }

    public static class BasicItem extends CombinedHistoryItem {
        public BasicItem(View view) {
            super(view);
        }
    }

    public static class SmartFolder extends CombinedHistoryItem {
        final Context context;
        final ImageView icon;
        final TextView title;
        final TextView subtext;

        public SmartFolder(View view) {
            super(view);
            context = view.getContext();

            icon = (ImageView) view.findViewById(R.id.device_type);
            title = (TextView) view.findViewById(R.id.title);
            subtext = (TextView) view.findViewById(R.id.subtext);
        }

        public void bind(int drawableRes, int titleRes, int singleDeviceRes, int multiDeviceRes, int numDevices) {
            icon.setImageResource(drawableRes);
            title.setText(titleRes);
            final String subtitle = numDevices == 1 ? context.getString(singleDeviceRes) : context.getString(multiDeviceRes, numDevices);
            subtext.setText(subtitle);
        }
    }

    public static class HistoryItem extends CombinedHistoryItem {
        public HistoryItem(View view) {
            super(view);
        }

        public void bind(Cursor historyCursor) {
            final TwoLinePageRow pageRow = (TwoLinePageRow) this.itemView;
            pageRow.setShowIcons(true);
            pageRow.updateFromCursor(historyCursor);
        }

        public void bind(RemoteTab remoteTab) {
            final TwoLinePageRow childPageRow = (TwoLinePageRow) this.itemView;
            childPageRow.setShowIcons(true);
            childPageRow.update(remoteTab.title, remoteTab.url);
        }

        public void bind(ClosedTab closedTab) {
            final TwoLinePageRow childPageRow = (TwoLinePageRow) this.itemView;
            childPageRow.setShowIcons(false);
            childPageRow.update(closedTab.title, closedTab.url);
        }
    }

    public static class ClientItem extends CombinedHistoryItem {
        final TextView nameView;
        final ImageView deviceTypeView;
        final TextView lastModifiedView;
        final ImageView deviceExpanded;

        public ClientItem(View view) {
            super(view);
            nameView = (TextView) view.findViewById(R.id.client);
            deviceTypeView = (ImageView) view.findViewById(R.id.device_type);
            lastModifiedView = (TextView) view.findViewById(R.id.last_synced);
            deviceExpanded = (ImageView) view.findViewById(R.id.device_expanded);
        }

        public void bind(Context context, RemoteClient client, boolean isCollapsed) {
            this.nameView.setText(client.name);
            final long now = System.currentTimeMillis();
            this.lastModifiedView.setText(ClientsAdapter.getLastSyncedString(context, now, client.lastModified));

            if (client.isDesktop()) {
                deviceTypeView.setImageResource(isCollapsed ? R.drawable.sync_desktop_inactive : R.drawable.sync_desktop);
            } else {
                deviceTypeView.setImageResource(isCollapsed ? R.drawable.sync_mobile_inactive : R.drawable.sync_mobile);
            }

            nameView.setTextColor(ContextCompat.getColor(context, isCollapsed ? R.color.tabs_tray_icon_grey : R.color.placeholder_active_grey));
            if (client.tabs.size() > 0) {
                deviceExpanded.setImageResource(isCollapsed ? R.drawable.home_group_collapsed : R.drawable.arrow_down);
                Drawable expandedDrawable = deviceExpanded.getDrawable();
                if (expandedDrawable != null) {
                    DrawableCompat.setAutoMirrored(expandedDrawable, true);
                }
            }
        }
    }
}
