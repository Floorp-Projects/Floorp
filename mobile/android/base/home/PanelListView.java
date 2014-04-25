/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;

import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomeConfig.ItemHandler;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.PanelLayout.DatasetBacked;
import org.mozilla.gecko.home.PanelLayout.FilterManager;
import org.mozilla.gecko.home.PanelLayout.OnItemOpenListener;
import org.mozilla.gecko.home.PanelLayout.PanelView;

import android.content.Context;
import android.database.Cursor;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;

public class PanelListView extends HomeListView
                           implements DatasetBacked, PanelView {

    private static final String LOGTAG = "GeckoPanelListView";

    private final ViewConfig viewConfig;
    private final PanelViewAdapter adapter;
    private final PanelViewItemHandler itemHandler;
    private OnItemOpenListener itemOpenListener;

    public PanelListView(Context context, ViewConfig viewConfig) {
        super(context);

        this.viewConfig = viewConfig;
        itemHandler = new PanelViewItemHandler(viewConfig);

        adapter = new PanelViewAdapter(context, viewConfig);
        setAdapter(adapter);

        setOnItemClickListener(new PanelListItemClickListener());
    }

     @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        itemHandler.setOnItemOpenListener(itemOpenListener);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        itemHandler.setOnItemOpenListener(null);
    }

    @Override
    public void setDataset(Cursor cursor) {
        Log.d(LOGTAG, "Setting dataset: " + viewConfig.getDatasetId());
        adapter.swapCursor(cursor);
    }

    @Override
    public void setOnItemOpenListener(OnItemOpenListener listener) {
        itemHandler.setOnItemOpenListener(listener);
        itemOpenListener = listener;
    }

    @Override
    public void setFilterManager(FilterManager filterManager) {
        adapter.setFilterManager(filterManager);
        itemHandler.setFilterManager(filterManager);
    }

    private class PanelListItemClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            itemHandler.openItemAtPosition(adapter.getCursor(), position);
        }
    }
}
