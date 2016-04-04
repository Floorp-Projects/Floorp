/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteTabsExpandableListAdapter;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;

public abstract class CombinedHistoryItem extends RecyclerView.ViewHolder {
    public CombinedHistoryItem(View view) {
        super(view);
    }

    public static class BasicItem extends CombinedHistoryItem {
        public BasicItem(View view) {
            super(view);
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
    }

    public static class ClientItem extends CombinedHistoryItem {
        final TextView nameView;
        final ImageView deviceTypeView;
        final TextView lastModifiedView;

        public ClientItem(View view) {
            super(view);
            nameView = (TextView) view.findViewById(R.id.client);
            deviceTypeView = (ImageView) view.findViewById(R.id.device_type);
            lastModifiedView = (TextView) view.findViewById(R.id.last_synced);
        }

        public void bind(RemoteClient client, Context context) {
                this.nameView.setText(client.name);
                this.nameView.setTextColor(ContextCompat.getColor(context, R.color.placeholder_active_grey));
                this.deviceTypeView.setImageResource("desktop".equals(client.deviceType) ? R.drawable.sync_desktop : R.drawable.sync_mobile);

                final long now = System.currentTimeMillis();
                this.lastModifiedView.setText(RemoteTabsExpandableListAdapter.getLastSyncedString(context, now, client.lastModified));
        }
    }
}
