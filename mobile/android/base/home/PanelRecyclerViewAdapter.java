/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;

import android.content.Context;
import android.database.Cursor;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

public class PanelRecyclerViewAdapter extends RecyclerView.Adapter<PanelRecyclerViewAdapter.PanelViewHolder> {
    private static final int VIEW_TYPE_ITEM = 0;
    private static final int VIEW_TYPE_BACK = 1;
    private static final int VIEW_TYPE_HEADER = 2;

    public static class PanelViewHolder extends RecyclerView.ViewHolder {
        public static PanelViewHolder create(View itemView) {

            // Wrap in a FrameLayout that will handle the highlight on touch
            FrameLayout frameLayout = (FrameLayout) LayoutInflater.from(itemView.getContext())
                    .inflate(R.layout.panel_item_container, null);

            frameLayout.addView(itemView, 0, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

            return new PanelViewHolder(frameLayout);
        }

        private PanelViewHolder(View itemView) {
            super(itemView);
        }
    }

    private final Context context;
    private final HomeConfig.ViewConfig viewConfig;
    private PanelLayout.FilterManager filterManager;
    private Cursor cursor;

    public PanelRecyclerViewAdapter(Context context, HomeConfig.ViewConfig viewConfig) {
        this.context = context;
        this.viewConfig = viewConfig;
    }

    public void setFilterManager(PanelLayout.FilterManager filterManager) {
        this.filterManager = filterManager;
    }

    private boolean isShowingBack() {
        return filterManager != null && filterManager.canGoBack();
    }

    public void swapCursor(Cursor cursor) {
        this.cursor = cursor;

        notifyDataSetChanged();
    }

    public Cursor getCursor() {
        return cursor;
    }

    @Override
    public int getItemViewType(int position) {
        if (viewConfig.hasHeaderConfig() && position == 0) {
            return VIEW_TYPE_HEADER;
        } else if (isShowingBack() && position == getBackPosition()) {
            return VIEW_TYPE_BACK;
        } else {
            return VIEW_TYPE_ITEM;
        }
    }

    @Override
    public PanelViewHolder onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        switch (viewType) {
            case VIEW_TYPE_HEADER:
                return PanelViewHolder.create(new PanelHeaderView(context, viewConfig.getHeaderConfig()));
            case VIEW_TYPE_BACK:
                return PanelViewHolder.create(new PanelBackItemView(context, viewConfig.getBackImageUrl()));
            case VIEW_TYPE_ITEM:
                return PanelViewHolder.create(PanelItemView.create(context, viewConfig.getItemType()));
            default:
                throw new IllegalArgumentException("Unknown view type: " + viewType);
        }
    }

    @Override
    public void onBindViewHolder(PanelViewHolder panelViewHolder, int position) {
        final View view = ((FrameLayout) panelViewHolder.itemView).getChildAt(0);

        if (viewConfig.hasHeaderConfig()) {
            if (position == 0) {
                // Nothing to do here, the header is static
                return;
            }
        }

        if (isShowingBack()) {
            if (position == getBackPosition()) {
                final PanelBackItemView item = (PanelBackItemView) view;
                item.updateFromFilter(filterManager.getPreviousFilter());
                return;
            }
        }

        int actualPosition = position
                - (isShowingBack() ? 1 : 0)
                - (viewConfig.hasHeaderConfig() ? 1 : 0);

        cursor.moveToPosition(actualPosition);

        final PanelItemView panelItemView = (PanelItemView) view;
        panelItemView.updateFromCursor(cursor);
    }

    private int getBackPosition() {
        return viewConfig.hasHeaderConfig() ? 1 : 0;
    }

    @Override
    public int getItemCount() {
        if (cursor == null) {
            return 0;
        }

        return cursor.getCount()
                + (isShowingBack() ? 1 : 0)
                + (viewConfig.hasHeaderConfig() ? 1 : 0);
    }
}
