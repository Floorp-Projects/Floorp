/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.animation.DecelerateInterpolator;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;

import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.widget.TwoWayView;

public class TabStripView extends TwoWayView {
    private static final String LOGTAG = "GeckoTabStrip";

    private static final int ANIM_TIME_MS = 200;
    private static final DecelerateInterpolator ANIM_INTERPOLATOR =
            new DecelerateInterpolator();

    private final TabStripAdapter adapter;
    private final Drawable divider;

    // Filled by calls to ShapeDrawable.getPadding();
    // saved to prevent allocation in draw().
    private final Rect dividerPadding = new Rect();

    private boolean isPrivate;

    public TabStripView(Context context, AttributeSet attrs) {
        super(context, attrs);

        setOrientation(Orientation.HORIZONTAL);
        setChoiceMode(ChoiceMode.SINGLE);
        setItemsCanFocus(true);
        setChildrenDrawingOrderEnabled(true);
        setWillNotDraw(false);

        final Resources resources = getResources();

        divider = resources.getDrawable(R.drawable.new_tablet_tab_strip_divider);
        divider.getPadding(dividerPadding);

        final int itemMargin =
                resources.getDimensionPixelSize(R.dimen.new_tablet_tab_strip_item_margin);
        setItemMargin(itemMargin);

        adapter = new TabStripAdapter(context);
        setAdapter(adapter);
    }

    private View getViewForTab(Tab tab) {
        final int position = adapter.getPositionForTab(tab);
        return getChildAt(position - getFirstVisiblePosition());
    }

    private int getPositionForSelectedTab() {
        return adapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
    }

    private void updateSelectedStyle(int selected) {
        setItemChecked(selected, true);
    }

    private void updateSelectedPosition(boolean ensureVisible) {
        final int selected = getPositionForSelectedTab();
        if (selected != -1) {
            updateSelectedStyle(selected);

            if (ensureVisible) {
                ensurePositionIsVisible(selected);
            }
        }
    }

    private void animateRemoveTab(Tab removedTab) {
        final int removedPosition = adapter.getPositionForTab(removedTab);

        final View removedView = getViewForTab(removedTab);

        // The removed position might not have a matching child view
        // when it's not within the visible range of positions in the strip.
        if (removedView == null) {
            return;
        }

        // We don't animate the removed child view (it just disappears)
        // but we still need its size of animate all affected children
        // within the visible viewport.
        final int removedSize = removedView.getWidth() + getItemMargin();

        getViewTreeObserver().addOnPreDrawListener(new OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                getViewTreeObserver().removeOnPreDrawListener(this);

                final int firstPosition = getFirstVisiblePosition();
                final List<Animator> childAnimators = new ArrayList<Animator>();

                final int childCount = getChildCount();
                for (int i = removedPosition - firstPosition; i < childCount; i++) {
                    final View child = getChildAt(i);

                    // TODO: optimize with Valueresolver
                    final ObjectAnimator animator =
                            ObjectAnimator.ofFloat(child, "translationX", removedSize, 0);
                    childAnimators.add(animator);
                }

                final AnimatorSet animatorSet = new AnimatorSet();
                animatorSet.playTogether(childAnimators);
                animatorSet.setDuration(ANIM_TIME_MS);
                animatorSet.setInterpolator(ANIM_INTERPOLATOR);
                animatorSet.start();

                return true;
            }
        });
    }

    private void animateNewTab(Tab newTab) {
        final int newPosition = adapter.getPositionForTab(newTab);
        if (newPosition < 0) {
            return;
        }

        getViewTreeObserver().addOnPreDrawListener(new OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                getViewTreeObserver().removeOnPreDrawListener(this);

                final int firstPosition = getFirstVisiblePosition();

                final View newChild = getChildAt(newPosition - firstPosition);
                if (newChild == null) {
                    return true;
                }

                final List<Animator> childAnimators = new ArrayList<Animator>();
                childAnimators.add(
                        ObjectAnimator.ofFloat(newChild, "translationY", newChild.getHeight(), 0));

                // This will momentaneously add a gap on the right side
                // because TwoWayView doesn't provide APIs to control
                // view recycling programatically to handle these transitory
                // states in the container during animations.

                final int tabSize = newChild.getWidth();
                final int newIndex = newPosition - firstPosition;
                final int childCount = getChildCount();
                for (int i = newIndex + 1; i < childCount; i++) {
                    final View child = getChildAt(i);

                    childAnimators.add(
                        ObjectAnimator.ofFloat(child, "translationX", -tabSize, 0));
                }

                final AnimatorSet animatorSet = new AnimatorSet();
                animatorSet.playTogether(childAnimators);
                animatorSet.setDuration(ANIM_TIME_MS);
                animatorSet.setInterpolator(ANIM_INTERPOLATOR);
                animatorSet.start();

                return true;
            }
        });
    }

    private void ensurePositionIsVisible(final int position) {
        getViewTreeObserver().addOnPreDrawListener(new OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                getViewTreeObserver().removeOnPreDrawListener(this);
                smoothScrollToPosition(position);
                return true;
            }
        });
    }

    private int getCheckedIndex(int childCount) {
        final int checkedIndex = getCheckedItemPosition() - getFirstVisiblePosition();
        if (checkedIndex < 0 || checkedIndex > childCount - 1) {
            return INVALID_POSITION;
        }

        return checkedIndex;
    }

    void refreshTabs() {
        // Store a different copy of the tabs, so that we don't have
        // to worry about accidentally updating it on the wrong thread.
        final List<Tab> tabs = new ArrayList<Tab>();

        for (Tab tab : Tabs.getInstance().getTabsInOrder()) {
            if (tab.isPrivate() == isPrivate) {
                tabs.add(tab);
            }
        }

        adapter.refresh(tabs);
        updateSelectedPosition(true);
    }

    void clearTabs() {
        adapter.clear();
    }

    void addTab(Tab tab) {
        // Refresh the list to make sure the new tab is
        // added in the right position.
        refreshTabs();
        animateNewTab(tab);
    }

    void removeTab(Tab tab) {
        animateRemoveTab(tab);
        adapter.removeTab(tab);
        updateSelectedPosition(false);
    }

    void selectTab(Tab tab) {
        if (tab.isPrivate() != isPrivate) {
            isPrivate = tab.isPrivate();
            refreshTabs();
        } else {
            updateSelectedPosition(true);
        }
    }

    void updateTab(Tab tab) {
        final TabStripItemView item = (TabStripItemView) getViewForTab(tab);
        if (item != null) {
            item.updateFromTab(tab);
        }
    }

    @Override
    protected int getChildDrawingOrder(int childCount, int i) {
        final int checkedIndex = getCheckedIndex(childCount);
        if (checkedIndex == INVALID_POSITION) {
            return i;
        }

        // Always draw the currently selected tab on top of all
        // other child views so that its curve is fully visible.
        if (i == childCount - 1) {
            return checkedIndex;
        } else if (checkedIndex <= i) {
            return i + 1;
        } else {
            return i;
        }
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        final int bottom = getHeight() - getPaddingBottom() - dividerPadding.bottom;
        final int top = bottom - divider.getIntrinsicHeight();

        final int dividerWidth = divider.getIntrinsicWidth();
        final int itemMargin = getItemMargin();

        final int childCount = getChildCount();
        final int checkedIndex = getCheckedIndex(childCount);

        for (int i = 1; i < childCount; i++) {
            final View child = getChildAt(i);

            final boolean pressed = (child.isPressed() || getChildAt(i - 1).isPressed());
            final boolean checked = (i == checkedIndex || i == checkedIndex + 1);

            // Don't draw dividers for around checked or pressed items
            // so that they are not drawn on top of the tab curves.
            if (pressed || checked) {
                continue;
            }

            final int left = child.getLeft() - (itemMargin / 2) - dividerWidth;
            final int right = left + dividerWidth;

            divider.setBounds(left, top, right, bottom);
            divider.draw(canvas);
        }
    }
}