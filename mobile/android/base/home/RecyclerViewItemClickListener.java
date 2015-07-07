/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.GestureDetector;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

/**
 * RecyclerView.OnItemTouchListener implementation that will notify an OnClickListener about clicks and long clicks
 * on items displayed by the RecyclerView.
 */
public class RecyclerViewItemClickListener implements RecyclerView.OnItemTouchListener,
                                                      GestureDetector.OnGestureListener {
    public interface OnClickListener {
        void onClick(View view, int position);
        void onLongClick(View view, int position);
    }

    private final OnClickListener clickListener;
    private final GestureDetector gestureDetector;
    private final RecyclerView recyclerView;

    public RecyclerViewItemClickListener(Context context, RecyclerView recyclerView, OnClickListener clickListener) {
        this.clickListener = clickListener;
        this.gestureDetector = new GestureDetector(context, this);
        this.recyclerView = recyclerView;
    }

    @Override
    public boolean onInterceptTouchEvent(RecyclerView recyclerView, MotionEvent event) {
        return gestureDetector.onTouchEvent(event);
    }

    @Override
    public boolean onSingleTapUp(MotionEvent event) {
        View childView = recyclerView.findChildViewUnder(event.getX(), event.getY());

        if (childView != null) {
            final int position = recyclerView.getChildAdapterPosition(childView);
            clickListener.onClick(childView, position);
        }

        return true;
    }

    @Override
    public void onLongPress(MotionEvent event) {
        View childView = recyclerView.findChildViewUnder(event.getX(), event.getY());

        if (childView != null) {
            final int position = recyclerView.getChildAdapterPosition(childView);
            childView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
            clickListener.onLongClick(childView, position);
        }
    }

    @Override
    public boolean onDown(MotionEvent e) {
        return false;
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        return false;
    }

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        return false;
    }

    @Override
    public void onShowPress(MotionEvent e) {}

    @Override
    public void onTouchEvent(RecyclerView recyclerView, MotionEvent motionEvent) {}

    @Override
    public void onRequestDisallowInterceptTouchEvent(boolean b) {}
}
