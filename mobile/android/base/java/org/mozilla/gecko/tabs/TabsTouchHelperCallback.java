/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.graphics.Canvas;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;

class TabsTouchHelperCallback extends ItemTouchHelper.Callback {
    private final @Nullable DismissListener dismissListener;
    private final @NonNull DragListener dragListener;
    private final int movementFlags;

    interface DismissListener {
        void onItemDismiss(View view);
    }

    interface DragListener {
        boolean onItemMove(int fromPosition, int toPosition);
    }

    TabsTouchHelperCallback(@NonNull DragListener dragListener, int dragDirections) {
        this(dragListener, dragDirections, null);
    }

    TabsTouchHelperCallback(@NonNull DragListener dragListener, int dragDirections, @Nullable DismissListener dismissListener) {
        this.dragListener = dragListener;
        this.dismissListener = dismissListener;
        // Tabs are only ever swiped left or right to dismiss, not up or down.
        final int swipeDirections = (dismissListener == null) ? 0 : ItemTouchHelper.START | ItemTouchHelper.END;
        movementFlags = makeMovementFlags(dragDirections, swipeDirections);
    }

    @Override
    public boolean isItemViewSwipeEnabled() {
        return dismissListener != null;
    }

    @Override
    public boolean isLongPressDragEnabled() {
        return true;
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        return movementFlags;
    }

    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int i) {
        dismissListener.onItemDismiss(viewHolder.itemView);
    }

    @Override
    public boolean onMove(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder,
                          RecyclerView.ViewHolder target) {
        final int fromPosition = viewHolder.getAdapterPosition();
        final int toPosition = target.getAdapterPosition();
        if (fromPosition == RecyclerView.NO_POSITION || toPosition == RecyclerView.NO_POSITION) {
            return false;
        }
        return dragListener.onItemMove(fromPosition, toPosition);
    }

    /**
     * Returns the alpha an itemView should be set to when swiped by an amount {@code dX}, given
     * that alpha should decrease to its min at distance {@code distanceToAlphaMin}.
     */
    /* package */ protected float alphaForItemSwipeDx(float dX, int distanceToAlphaMin) {
        return 1;
    }

    @Override
    public void onChildDraw(Canvas c,
                            RecyclerView recyclerView,
                            RecyclerView.ViewHolder viewHolder,
                            float dX,
                            float dY,
                            int actionState,
                            boolean isCurrentlyActive) {
        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);

        if (actionState == ItemTouchHelper.ACTION_STATE_SWIPE) {
            // Alpha on an itemView being swiped should decrease to a min over a distance equal to
            // the width of the item being swiped.
            viewHolder.itemView.setAlpha(alphaForItemSwipeDx(dX, viewHolder.itemView.getWidth()));
        }
    }

    @Override
    public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        super.clearView(recyclerView, viewHolder);
        viewHolder.itemView.setAlpha(1);
    }
}
