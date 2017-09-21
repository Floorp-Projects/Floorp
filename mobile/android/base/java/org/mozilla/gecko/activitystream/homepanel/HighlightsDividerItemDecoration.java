/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.RecyclerView;
import android.view.View;

/**
 * ItemDecoration implementation that draws horizontal divider line between items
 * in the AS newtab page.
 */
/* package */ class HighlightsDividerItemDecoration extends RecyclerView.ItemDecoration {

    // We do not want to draw a divider above the first item.
    private static final int START_DRAWING_AT_POSITION = 1;

    private static final int[] ATTRS = new int[]{
            android.R.attr.listDivider
    };
    private Drawable divider;

    /* package */ HighlightsDividerItemDecoration(Context context) {
        final TypedArray a = context.obtainStyledAttributes(ATTRS);
        divider = a.getDrawable(0);
        a.recycle();
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        final int left = parent.getPaddingLeft();
        final int right = parent.getWidth() - parent.getPaddingRight();

        final int childCount = parent.getChildCount();
        for (int i = 0; i < childCount; i++) {
            final View child = parent.getChildAt(i);

            if (parent.getChildAdapterPosition(child) < START_DRAWING_AT_POSITION) {
                continue;
            }

            if (child.getVisibility() == View.GONE) {
                continue;
            }

            // Do not draw dividers above section title items.
            final int childViewType = parent.getAdapter().getItemViewType(i);
            if (childViewType == StreamRecyclerAdapter.RowItemType.HIGHLIGHTS_TITLE.getViewType()
                    || childViewType == StreamRecyclerAdapter.RowItemType.TOP_STORIES_TITLE.getViewType()) {
                continue;
            }

            final RecyclerView.LayoutParams params = (RecyclerView.LayoutParams) child
                    .getLayoutParams();
            final int topOfDivider = child.getTop() + params.topMargin;
            final int bottomOfDivider = topOfDivider + divider.getIntrinsicHeight();
            divider.setBounds(left, topOfDivider, right, bottomOfDivider);
            divider.draw(c);
        }
    }

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.set(0, 0, 0, divider.getIntrinsicHeight());
    }
}
