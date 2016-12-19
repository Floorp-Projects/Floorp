/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;

class TabStripDividerItem extends RecyclerView.ItemDecoration {
    private final int margin;
    private final int dividerWidth;
    private final int dividerHeight;
    private final int dividerPaddingBottom;
    private final Paint dividerPaint;

    TabStripDividerItem(Context context) {
        margin = (int) context.getResources().getDimension(R.dimen.tablet_tab_strip_item_margin);
        dividerWidth = (int) context.getResources().getDimension(R.dimen.tablet_tab_strip_divider_width);
        dividerHeight = (int) context.getResources().getDimension(R.dimen.tablet_tab_strip_divider_height);
        dividerPaddingBottom = (int) context.getResources().getDimension(R.dimen.tablet_tab_strip_divider_padding_bottom);

        dividerPaint = new Paint();
        dividerPaint.setColor(ContextCompat.getColor(context, R.color.tablet_tab_strip_divider_color));
        dividerPaint.setStyle(Paint.Style.FILL_AND_STROKE);
    }

    /**
     * Return whether a divider should be drawn on the left side of the tab represented by
     * {@code view}.
     */
    private static boolean drawLeftDividerForView(View view, RecyclerView parent) {
        final int position = parent.getChildAdapterPosition(view);
        // No left divider if this is tab 0 or this tab is currently pressed.
        if (position == 0 || view.isPressed()) {
            return false;
        }

        final int selectedPosition = ((TabStripView) parent).getPositionForSelectedTab();
        // No left divider if this tab or the previous tab is the current selected tab.
        if (selectedPosition != RecyclerView.NO_POSITION &&
                (position == selectedPosition || position == selectedPosition + 1)) {
            return false;
        }

        final RecyclerView.ViewHolder holder = parent.findViewHolderForAdapterPosition(position - 1);
        // No left divider if the previous tab is currently pressed.
        if (holder != null && holder.itemView.isPressed()) {
            return false;
        }

        return true;
    }

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        final int position = parent.getChildAdapterPosition(view);
        final int leftOffset = position == 0 ? 0 : margin;
        final int rightOffset = position == parent.getAdapter().getItemCount() - 1 ? 0 : margin;

        outRect.set(leftOffset, 0, rightOffset, 0);
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        final int childCount = parent.getChildCount();
        if (childCount == 0) {
            return;
        }

        for (int i = 0; i < childCount; i++) {
            final View child = parent.getChildAt(i);
            if (drawLeftDividerForView(child, parent)) {
                final float left = child.getLeft() + child.getTranslationX() + Math.abs(margin);
                final float top = child.getTop() + child.getTranslationY();
                final float dividerTop = top + child.getHeight() - dividerHeight - dividerPaddingBottom;
                final float dividerBottom = dividerTop + dividerHeight;
                c.drawRect(left - dividerWidth, dividerTop, left, dividerBottom, dividerPaint);
            }
        }
    }
}
