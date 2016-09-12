/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.Resources;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.util.AttributeSet;

public class TabsGridLayout extends TabsLayout {
    private final int desiredColumnWidth;

    public TabsGridLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.layout.tabs_layout_item_view);

        final Resources resources = context.getResources();

        setLayoutManager(new GridLayoutManager(context, 1));

        desiredColumnWidth = resources.getDimensionPixelSize(R.dimen.tab_panel_item_width);
        final int viewPaddingHorizontal = resources.getDimensionPixelSize(R.dimen.tab_panel_grid_hpadding);
        final int viewPaddingVertical = resources.getDimensionPixelSize(R.dimen.tab_panel_grid_vpadding);

        setPadding(viewPaddingHorizontal, viewPaddingVertical, viewPaddingHorizontal, viewPaddingVertical);
        setClipToPadding(false);
        setScrollBarStyle(SCROLLBARS_OUTSIDE_OVERLAY);

        setItemAnimator(new TabsGridLayoutAnimator());

        // TODO Add ItemDecoration.

        // A TouchHelper handler for swipe to close.
        final TabsTouchHelperCallback callback = new TabsTouchHelperCallback(this) {
            @Override
            protected float alphaForItemSwipeDx(float dX, int distanceToAlphaMin) {
                return 1f - 2f * Math.abs(dX) / distanceToAlphaMin;
            }
        };
        final ItemTouchHelper touchHelper = new ItemTouchHelper(callback);
        touchHelper.attachToRecyclerView(this);
    }

    @Override
    public void closeAll() {
        autoHidePanel();

        closeAllTabs();
    }

    @Override
    protected boolean addAtIndexRequiresScroll(int index) {
        final GridLayoutManager layoutManager = (GridLayoutManager) getLayoutManager();
        final int spanCount = layoutManager.getSpanCount();
        final int firstVisibleIndex = layoutManager.findFirstVisibleItemPosition();
        // When you add an item at the first visible position to a GridLayoutManager and there's
        // room to scroll, RecyclerView scrolls the new position to anywhere from near the bottom of
        // its row to completely offscreen (for unknown reasons), so we need to scroll to fix that.
        // We also scroll when the item being added is the only item on the final row.
        return index == firstVisibleIndex ||
                (index == getAdapter().getItemCount() - 1 && index % spanCount == 0);
    }

    @Override
    public void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        if (w == oldw) {
            return;
        }

        // TODO This is temporary - we need to take into account item padding and we'll also try to
        // match the previous GridLayout span count.
        final int nonPaddingWidth = w - getPaddingLeft() - getPaddingRight();
        // Adjust span based on space available (what GridView does when you say numColumns="auto_fit").
        final int spanCount = Math.max(1, nonPaddingWidth / desiredColumnWidth);
        final GridLayoutManager layoutManager = (GridLayoutManager) getLayoutManager();
        if (spanCount == layoutManager.getSpanCount()) {
            return;
        }
        layoutManager.setSpanCount(spanCount);
    }
}
