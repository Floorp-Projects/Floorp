/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.PanelLayout.DatasetBacked;
import org.mozilla.gecko.home.PanelLayout.PanelView;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;
import org.mozilla.gecko.widget.RecyclerViewClickSupport.OnItemClickListener;
import org.mozilla.gecko.widget.RecyclerViewClickSupport.OnItemLongClickListener;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;

/**
 * RecyclerView implementation for grid home panels.
 */
@SuppressLint("ViewConstructor") // View is only created from code
public class PanelRecyclerView extends RecyclerView
        implements DatasetBacked, PanelView, OnItemClickListener, OnItemLongClickListener {
    private final PanelRecyclerViewAdapter adapter;
    private final GridLayoutManager layoutManager;
    private final PanelViewItemHandler itemHandler;
    private final float columnWidth;
    private final boolean autoFit;
    private final HomeConfig.ViewConfig viewConfig;

    private PanelLayout.OnItemOpenListener itemOpenListener;
    private HomeContextMenuInfo contextMenuInfo;
    private HomeContextMenuInfo.Factory contextMenuInfoFactory;

    public PanelRecyclerView(Context context, HomeConfig.ViewConfig viewConfig) {
        super(context);

        this.viewConfig = viewConfig;

        final Resources resources = context.getResources();

        int spanCount;
        if (viewConfig.getItemType() == HomeConfig.ItemType.ICON) {
            autoFit = false;
            spanCount = getResources().getInteger(R.integer.panel_icon_grid_view_columns);
        } else {
            autoFit = true;
            spanCount = 1;
        }

        columnWidth = resources.getDimension(R.dimen.panel_grid_view_column_width);
        layoutManager = new GridLayoutManager(context, spanCount);
        adapter = new PanelRecyclerViewAdapter(context, viewConfig);
        itemHandler = new PanelViewItemHandler();

        layoutManager.setSpanSizeLookup(new PanelSpanSizeLookup());

        setLayoutManager(layoutManager);
        setAdapter(adapter);

        int horizontalSpacing = (int) resources.getDimension(R.dimen.panel_grid_view_horizontal_spacing);
        int verticalSpacing = (int) resources.getDimension(R.dimen.panel_grid_view_vertical_spacing);
        int outerSpacing = (int) resources.getDimension(R.dimen.panel_grid_view_outer_spacing);

        addItemDecoration(new SpacingDecoration(horizontalSpacing, verticalSpacing));

        setPadding(outerSpacing, outerSpacing, outerSpacing, outerSpacing);
        setClipToPadding(false);

        RecyclerViewClickSupport.addTo(this)
            .setOnItemClickListener(this)
            .setOnItemLongClickListener(this);
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        super.onMeasure(widthSpec, heightSpec);

        if (autoFit) {
            // Adjust span based on space available (What GridView does when you say numColumns="auto_fit")
            final int spanCount = (int) Math.max(1, getMeasuredWidth() / columnWidth);
            layoutManager.setSpanCount(spanCount);
        }
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
        adapter.swapCursor(cursor);
    }

    @Override
    public void setFilterManager(PanelLayout.FilterManager manager) {
        adapter.setFilterManager(manager);
        itemHandler.setFilterManager(manager);
    }

    @Override
    public void setOnItemOpenListener(PanelLayout.OnItemOpenListener listener) {
        itemOpenListener = listener;
        itemHandler.setOnItemOpenListener(listener);
    }

    @Override
    public HomeContextMenuInfo getContextMenuInfo() {
        return contextMenuInfo;
    }

    @Override
    public void setContextMenuInfoFactory(HomeContextMenuInfo.Factory factory) {
        contextMenuInfoFactory = factory;
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        if (viewConfig.hasHeaderConfig()) {
            if (position == 0) {
                itemOpenListener.onItemOpen(viewConfig.getHeaderConfig().getUrl(), null);
                return;
            }

            position--;
        }

        itemHandler.openItemAtPosition(adapter.getCursor(), position);
    }

    @Override
    public boolean onItemLongClicked(RecyclerView recyclerView, int position, View v) {
        if (viewConfig.hasHeaderConfig()) {
            if (position == 0) {
                final HomeConfig.HeaderConfig headerConfig = viewConfig.getHeaderConfig();

                final HomeContextMenuInfo info = new HomeContextMenuInfo(v, position, -1);
                info.url = headerConfig.getUrl();
                info.title = headerConfig.getUrl();

                contextMenuInfo = info;
                return showContextMenuForChild(this);
            }

            position--;
        }

        Cursor cursor = adapter.getCursor();
        cursor.moveToPosition(position);

        contextMenuInfo = contextMenuInfoFactory.makeInfoForCursor(recyclerView, position, -1, cursor);
        return showContextMenuForChild(this);
    }

    private class PanelSpanSizeLookup extends GridLayoutManager.SpanSizeLookup {
        @Override
        public int getSpanSize(int position) {
            if (position == 0 && viewConfig.hasHeaderConfig()) {
                return layoutManager.getSpanCount();
            }

            return 1;
        }
    }
}
