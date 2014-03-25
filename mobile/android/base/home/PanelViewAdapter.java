/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomeConfig.ItemType;
import org.mozilla.gecko.home.HomeConfig.ViewConfig;
import org.mozilla.gecko.home.PanelLayout.FilterManager;

import org.mozilla.gecko.R;

import android.content.Context;
import android.database.Cursor;
import android.support.v4.widget.CursorAdapter;
import android.view.View;
import android.view.ViewGroup;

class PanelViewAdapter extends CursorAdapter {
    private static final int VIEW_TYPE_ITEM = 0;
    private static final int VIEW_TYPE_BACK = 1;

    private final ViewConfig viewConfig;
    private FilterManager filterManager;
    private final Context context;
    private int targetWidth = 0;
    private int targetHeight = 0;

    public PanelViewAdapter(Context context, ViewConfig viewConfig) {
        super(context, null, 0);
        this.context = context;
        this.viewConfig = viewConfig;
    }

    public void setFilterManager(FilterManager manager) {
        this.filterManager = manager;
    }

    @Override
    public final int getViewTypeCount() {
        return 2;
    }

    public void setTargetImageSize(int targetWidth, int targetHeight) {
        this.targetWidth = targetWidth;
        this.targetHeight = targetHeight;
    }

    @Override
    public int getCount() {
        return super.getCount() + (isShowingBack() ? 1 : 0);
    }

    @Override
    public int getItemViewType(int position) {
        if (isShowingBack() && position == 0) {
            return VIEW_TYPE_BACK;
        } else {
            return VIEW_TYPE_ITEM;
        }
    }

    @Override
    public final View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            convertView = newView(parent.getContext(), position, parent);
        }

        bindView(convertView, position);
        return convertView;
    }

    private View newView(Context context, int position, ViewGroup parent) {
        final PanelItemView item;
        if (getItemViewType(position) == VIEW_TYPE_BACK) {
            item = new PanelBackItemView(context, viewConfig.getBackImageUrl());
        } else {
            item = PanelItemView.create(context, viewConfig.getItemType());
        }

        if (viewConfig.getItemType() == ItemType.IMAGE && targetWidth > 0 && targetHeight > 0) {
            item.setTargetImageSize(targetWidth, targetHeight);
        }

        return item;
    }

    private void bindView(View view, int position) {
        if (isShowingBack()) {
            if (position == 0) {
                final PanelBackItemView item = (PanelBackItemView) view;
                item.updateFromFilter(filterManager.getPreviousFilter());
                return;
            }

            position--;
        }

        final Cursor cursor = getCursor(position);
        final PanelItemView item = (PanelItemView) view;
        item.updateFromCursor(cursor);
    }

    private boolean isShowingBack() {
        return (filterManager != null ? filterManager.canGoBack() : false);
    }

    private final Cursor getCursor(int position) {
        final Cursor cursor = getCursor();
        if (cursor == null || !cursor.moveToPosition(position)) {
            throw new IllegalStateException("Couldn't move cursor to position " + position);
        }

        return cursor;
    }

    @Override
    public final void bindView(View view, Context context, Cursor cursor) {
        // Do nothing.
    }

    @Override
    public final View newView(Context context, Cursor cursor, ViewGroup parent) {
        return null;
    }
}
