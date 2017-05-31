/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.ThreadUtils;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Shader;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.ViewUtils;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.animation.DecelerateInterpolator;

import java.util.ArrayList;
import java.util.List;

import static org.mozilla.gecko.Tab.TabType;

public class TabStripView extends RecyclerView
                          implements TabsTouchHelperCallback.DragListener {
    private static final int ANIM_TIME_MS = 200;
    private static final DecelerateInterpolator ANIM_INTERPOLATOR = new DecelerateInterpolator();

    private final TabStripAdapter adapter;
    private final TabType type;
    private boolean isPrivate;

    private final TabAnimatorListener animatorListener;

    private final Paint fadingEdgePaint;
    private final int fadingEdgeSize;

    public TabStripView(Context context, AttributeSet attrs) {
        super(context, attrs);

        type = TabType.BROWSING;

        fadingEdgePaint = new Paint();
        final Resources resources = getResources();
        fadingEdgeSize =
                resources.getDimensionPixelOffset(R.dimen.tablet_tab_strip_fading_edge_size);

        animatorListener = new TabAnimatorListener();

        setChildrenDrawingOrderEnabled(true);

        adapter = new TabStripAdapter(context);
        setAdapter(adapter);

        final LinearLayoutManager layoutManager = new LinearLayoutManager(context);
        layoutManager.setOrientation(LinearLayoutManager.HORIZONTAL);
        setLayoutManager(layoutManager);

        setItemAnimator(new TabStripItemAnimator(ANIM_TIME_MS));

        addItemDecoration(new TabStripDividerItem(context));

        final int dragDirections = ItemTouchHelper.START | ItemTouchHelper.END;
        // A TouchHelper handler for drag and drop.
        final TabsTouchHelperCallback callback = new TabsTouchHelperCallback(this, dragDirections);
        final ItemTouchHelper touchHelper = new ItemTouchHelper(callback);
        touchHelper.attachToRecyclerView(this);
    }

    /* package */ void refreshTabs() {
        // Store a different copy of the tabs, so that we don't have
        // to worry about accidentally updating it on the wrong thread.
        final List<Tab> tabs = new ArrayList<>();

        for (final Tab tab : Tabs.getInstance().getTabsInOrder()) {
            if (tab.isPrivate() == isPrivate && tab.getType() == type) {
                tabs.add(tab);
            }
        }

        adapter.refresh(tabs);
        updateSelectedPosition();
    }

    /* package */ void clearTabs() {
        adapter.clear();
    }

    /* package */ void restoreTabs() {
        refreshTabs();
        animateRestoredTabs();
    }

    /* package */ void addTab(Tab tab, int position) {
        if (tab.isPrivate() != isPrivate || tab.getType() != type) {
            return;
        }

        adapter.addTab(tab, position);
    }

    /* package */ void removeTab(Tab tab) {
        adapter.removeTab(tab);
    }

    /* package */ void selectTab(Tab tab) {
        if (tab.isPrivate() != isPrivate) {
            isPrivate = tab.isPrivate();
            refreshTabs();
            return;
        }
        final int position = adapter.getPositionForTab(tab);
        if (position == -1) {
            return;
        }
        // scrollToPosition sometimes needlessly scrolls even when position is already in view, so
        // don't scrollToPosition unless necessary.  (The position == 0 case is needed to scroll
        // tab 0 into view when it's added back after a close undo (in which case the adapter and
        // layout may be temporarily out of sync, so the rest of the if doesn't work).)
        final LinearLayoutManager layoutManager = (LinearLayoutManager) getLayoutManager();
        if (position < layoutManager.findFirstCompletelyVisibleItemPosition() ||
                position > layoutManager.findLastCompletelyVisibleItemPosition() ||
                position == 0) {
            scrollToPosition(position);
        }
    }

    /* package */ void updateTab(Tab tab) {
        adapter.notifyTabChanged(tab);
    }

    /* package */ boolean isPrivate() {
        return isPrivate;
    }

    @Override
    public boolean onItemMove(int fromPosition, int toPosition) {
        return adapter.moveTab(fromPosition, toPosition);
    }

    public int getPositionForSelectedTab() {
        return adapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
    }

    private void updateSelectedPosition() {
        final int selected = getPositionForSelectedTab();
        if (selected != -1) {
            scrollToPosition(selected);
        }
    }

    public void refresh() {
        // This gets called after a rotation.  Without the delay the scroll can fail to scroll far
        // enough if the selected position is the last or next to last position (and there are
        // enough open tabs so that the last two tabs aren't automatically always in view).
        ThreadUtils.postDelayedToUiThread(new Runnable() {
            @Override
            public void run() {
                updateSelectedPosition();
            }
        }, 50);
    }

    @Override
    public void onChildAttachedToWindow(View child) {
        // Make sure we didn't miss any resets after animations etc.
        child.setTranslationX(0);
        child.setTranslationY(0);
    }

    /**
     * Return the position of the currently selected tab relative to the tabs currently visible in
     * the tabs list, or -1 if the currently selected tab isn't visible in the tabs list.
     */
    private int getRelativeSelectedPosition(int visibleTabsCount) {
        final int selectedPosition = getPositionForSelectedTab();
        final LinearLayoutManager layoutManager = (LinearLayoutManager) getLayoutManager();
        final int firstVisiblePosition = layoutManager.findFirstVisibleItemPosition();
        final int relativeSelectedPosition = selectedPosition - firstVisiblePosition;
        if (relativeSelectedPosition < 0 || relativeSelectedPosition > visibleTabsCount - 1) {
            return -1;
        }

        return relativeSelectedPosition;
    }

    @Override
    protected int getChildDrawingOrder(int childCount, int i) {
        final int relativeSelectedPosition = getRelativeSelectedPosition(childCount);
        if (relativeSelectedPosition == -1) {
            // The selected tab isn't visible, so we don't need to adjust drawing order.
            return i;
        }

        // Explanation of the input parameters: there are childCount tabs visible, and right now
        // we're returning which of those tabs to draw i'th, for some i between 0 and
        // childCount - 1.
        if (i == childCount - 1) {
            // Draw the selected tab last.
            return relativeSelectedPosition;
        } else if (i >= relativeSelectedPosition) {
            // Draw the tabs after the selected tab one iteration earlier than normal.
            return i + 1;
        } else {
            // Draw the tabs before the selected tab in their normal order.
            return i;
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        if (w == oldw) {
            return;
        }

        // Gradient argb color stops.
        final int transparent = 0x0;
        final int inBetween = 0x11292C29;
        final int darkest = 0xDD292C29;
        if (ViewUtils.isLayoutRtl(this)) {
            fadingEdgePaint.setShader(new LinearGradient(0, 0, fadingEdgeSize, 0,
                    new int[] { darkest, inBetween, transparent },
                    new float[] { 0, 0.6f, 1.0f }, Shader.TileMode.CLAMP));
        } else {
            fadingEdgePaint.setShader(new LinearGradient(w - fadingEdgeSize, 0, w, 0,
                    new int[] { transparent, inBetween, darkest },
                    new float[] { 0, 0.4f, 1.0f }, Shader.TileMode.CLAMP));
        }
    }

    private float getFadingEdgeStrength(boolean layoutIsLTR) {
        final int childCount = getChildCount();
        if (childCount == 0) {
            return 0.0f;
        } else {
            final LinearLayoutManager layoutManager = (LinearLayoutManager) getLayoutManager();
            if (layoutManager.findLastVisibleItemPosition() < adapter.getItemCount() - 1) {
                return 1.0f;
            }

            final float strength;
            if (layoutIsLTR) {
                final int right = getChildAt(getChildCount() - 1).getRight();
                final int paddingRight = getPaddingRight();
                final int width = getWidth();

                strength = (right > width - paddingRight ?
                        (float) (right - width + paddingRight) / fadingEdgeSize : 0.0f);
            } else {
                final int left = getChildAt(getChildCount() - 1).getLeft();
                final int paddingLeft = getPaddingLeft();

                strength = left < paddingLeft ? (float) (paddingLeft - left) / fadingEdgeSize : 0.0f;
            }

            return Math.max(0.0f, Math.min(strength, 1.0f));
        }
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);
        final boolean isLTR = !ViewUtils.isLayoutRtl(this);
        final float strength = getFadingEdgeStrength(isLTR);
        if (strength > 0.0f) {
            if (isLTR) {
                final int r = getRight();
                canvas.drawRect(r - fadingEdgeSize, getTop(), r, getBottom(), fadingEdgePaint);
            } else {
                canvas.drawRect(0, getTop(), fadingEdgeSize, getBottom(), fadingEdgePaint);
            }
            fadingEdgePaint.setAlpha((int) (strength * 255));
        }
    }

    private void animateRestoredTabs() {
        getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                getViewTreeObserver().removeOnPreDrawListener(this);

                final List<Animator> childAnimators = new ArrayList<>();

                final int tabHeight = getHeight() - getPaddingTop() - getPaddingBottom();
                final int childCount = getChildCount();
                for (int i = 0; i < childCount; i++) {
                    final View child = getChildAt(i);

                    childAnimators.add(
                        ObjectAnimator.ofFloat(child, "translationY", tabHeight, 0));
                }

                final AnimatorSet animatorSet = new AnimatorSet();
                animatorSet.playTogether(childAnimators);
                animatorSet.setDuration(ANIM_TIME_MS);
                animatorSet.setInterpolator(ANIM_INTERPOLATOR);
                animatorSet.addListener(animatorListener);

                animatorSet.start();

                return true;
            }
        });
    }

    private class TabAnimatorListener implements Animator.AnimatorListener {
        private void setLayerType(int layerType) {
            final int childCount = getChildCount();
            for (int i = 0; i < childCount; i++) {
                getChildAt(i).setLayerType(layerType, null);
            }
        }

        @Override
        public void onAnimationStart(Animator animation) {
            setLayerType(View.LAYER_TYPE_HARDWARE);
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            // This method is called even if the animator is canceled.
            setLayerType(View.LAYER_TYPE_NONE);
        }

        @Override
        public void onAnimationRepeat(Animator animation) {
        }

        @Override
        public void onAnimationCancel(Animator animation) {
        }
    }
}
