/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.graphics.Canvas;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;

abstract class TabsTouchHelperCallback extends ItemTouchHelper.Callback {
    private final DismissListener dismissListener;

    interface DismissListener {
        void onItemDismiss(View view);
    }

    public TabsTouchHelperCallback(DismissListener dismissListener) {
        this.dismissListener = dismissListener;
    }

    @Override
    public boolean isItemViewSwipeEnabled() {
        return true;
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        return makeFlag(ItemTouchHelper.ACTION_STATE_SWIPE, ItemTouchHelper.LEFT | ItemTouchHelper.RIGHT);
    }

    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int i) {
        dismissListener.onItemDismiss(viewHolder.itemView);
    }

    @Override
    public boolean onMove(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder,
                          RecyclerView.ViewHolder target) {
        return false;
    }

    /**
     * Returns the alpha an itemView should be set to when swiped by an amount {@code dX}, given
     * that alpha should decrease to its min at distance {@code distanceToAlphaMin}.
     */
    abstract protected float alphaForItemSwipeDx(float dX, int distanceToAlphaMin);

    /**
     * Alpha on an itemView being swiped should decrease to a min over a distance equal to the
     * width of the item being swiped.
     */
    @Override
    public void onChildDraw(Canvas c,
                            RecyclerView recyclerView,
                            RecyclerView.ViewHolder viewHolder,
                            float dX,
                            float dY,
                            int actionState,
                            boolean isCurrentlyActive) {
        if (actionState != ItemTouchHelper.ACTION_STATE_SWIPE) {
            return;
        }

        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);

        viewHolder.itemView.setAlpha(alphaForItemSwipeDx(dX, viewHolder.itemView.getWidth()));
    }

    public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        super.clearView(recyclerView, viewHolder);
        viewHolder.itemView.setAlpha(1);
    }
}
