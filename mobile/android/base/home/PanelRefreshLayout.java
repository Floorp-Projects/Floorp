/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.home.PanelLayout.DatasetBacked;
import org.mozilla.gecko.home.PanelLayout.FilterManager;
import org.mozilla.gecko.widget.GeckoSwipeRefreshLayout;
import org.mozilla.gecko.widget.GeckoSwipeRefreshLayout.OnRefreshListener;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.database.Cursor;
import android.util.Log;
import android.view.View;

/**
 * Used to wrap a {@code DatasetBacked} ListView or GridView to give the child view swipe-to-refresh
 * capabilities.
 *
 * This view acts as a decorator to forward the {@code DatasetBacked} methods to the child view
 * while providing the refresh gesture support on top of it.
 */
class PanelRefreshLayout extends GeckoSwipeRefreshLayout implements DatasetBacked {
    private static final String LOGTAG = "GeckoPanelRefreshLayout";

    private static final String JSON_KEY_PANEL_ID = "panelId";
    private static final String JSON_KEY_VIEW_INDEX = "viewIndex";

    private final String panelId;
    private final int viewIndex;
    private final DatasetBacked datasetBacked;

    /**
     * @param context Android context.
     * @param childView ListView or GridView. Must implement {@code DatasetBacked}.
     * @param panelId The ID from the {@code PanelConfig}.
     * @param viewIndex The index from the {@code ViewConfig}.
     */
    public PanelRefreshLayout(Context context, View childView, String panelId, int viewIndex) {
        super(context);

        if (!(childView instanceof DatasetBacked)) {
            throw new IllegalArgumentException("View must implement DatasetBacked to be refreshed");
        }

        this.panelId = panelId;
        this.viewIndex = viewIndex;
        this.datasetBacked = (DatasetBacked) childView;

        setOnRefreshListener(new RefreshListener());
        addView(childView);

        // Must be called after the child view has been added.
        setColorScheme(R.color.swipe_refresh_orange, R.color.swipe_refresh_white,
                       R.color.swipe_refresh_orange, R.color.swipe_refresh_white);
    }

    @Override
    public void setDataset(Cursor cursor) {
        datasetBacked.setDataset(cursor);
        setRefreshing(false);
    }

    @Override
    public void setFilterManager(FilterManager manager) {
        datasetBacked.setFilterManager(manager);
    }

    private class RefreshListener implements OnRefreshListener {
        @Override
        public void onRefresh() {
            final JSONObject response = new JSONObject();
            try {
                response.put(JSON_KEY_PANEL_ID, panelId);
                response.put(JSON_KEY_VIEW_INDEX, viewIndex);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Could not create refresh message", e);
                return;
            }

            final GeckoEvent event =
                GeckoEvent.createBroadcastEvent("HomePanels:RefreshView", response.toString());
            GeckoAppShell.sendEventToGecko(event);
        }
    }
}
