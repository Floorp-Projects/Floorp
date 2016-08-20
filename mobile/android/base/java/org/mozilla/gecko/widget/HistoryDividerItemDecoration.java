/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import org.mozilla.gecko.R;
import org.mozilla.gecko.home.CombinedHistoryItem;

public class HistoryDividerItemDecoration extends RecyclerView.ItemDecoration {
    private final int mDividerHeight;
    private final Paint mDividerPaint;

    public HistoryDividerItemDecoration(Context context) {
        mDividerHeight = (int) context.getResources().getDimension(R.dimen.page_row_divider_height);

        mDividerPaint = new Paint();
        mDividerPaint.setColor(ContextCompat.getColor(context, R.color.toolbar_divider_grey));
        mDividerPaint.setStyle(Paint.Style.FILL_AND_STROKE);
    }

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        final int position = parent.getChildAdapterPosition(view);
        if (position == RecyclerView.NO_POSITION) {
            // This view is no longer corresponds to an adapter position (pending changes).
            return;
        }

        if (parent.getAdapter().getItemViewType(position) !=
                CombinedHistoryItem.ItemType.itemTypeToViewType(CombinedHistoryItem.ItemType.SECTION_HEADER)) {
            outRect.set(0, 0, 0, mDividerHeight);
        }
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (parent.getChildCount() == 0) {
            return;
        }

        for (int i = 0; i < parent.getChildCount(); i++) {
            final View child = parent.getChildAt(i);
            final int position = parent.getChildAdapterPosition(child);

            if (position == RecyclerView.NO_POSITION) {
                // This view is no longer corresponds to an adapter position (pending changes).
                continue;
            }

            if (parent.getAdapter().getItemViewType(position) !=
                    CombinedHistoryItem.ItemType.itemTypeToViewType(CombinedHistoryItem.ItemType.SECTION_HEADER)) {
                final float bottom = child.getBottom() + child.getTranslationY();
                c.drawRect(0, bottom, parent.getWidth(), bottom + mDividerHeight, mDividerPaint);
            }
        }
    }
}
