/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.tabs;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.PropertyAnimator.Property;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.tabs.TabsPanel.TabsLayout;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.widget.TwoWayView;
import org.mozilla.gecko.widget.themed.ThemedRelativeLayout;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.Button;

import java.util.ArrayList;
import java.util.List;

class TabsListLayout extends TwoWayView
                     implements TabsLayout,
                                Tabs.OnTabsChangedListener {

    private static final String LOGTAG = "Gecko" + TabsListLayout.class.getSimpleName();

    // Time to animate non-flinged tabs of screen, in milliseconds
    private static final int ANIMATION_DURATION = 250;

    // Time between starting successive tab animations in closeAllTabs.
    private static final int ANIMATION_CASCADE_DELAY = 75;

    private final boolean isPrivate;
    private final TabsLayoutAdapter tabsAdapter;
    private final List<View> pendingClosedTabs;
    private TabsPanel tabsPanel;
    private int closeAnimationCount;
    private int closeAllAnimationCount;
    private int originalSize;

    public TabsListLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        pendingClosedTabs = new ArrayList<View>();

        setItemsCanFocus(true);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsLayout);
        isPrivate = (a.getInt(R.styleable.TabsLayout_tabs, 0x0) == 1);
        a.recycle();

        tabsAdapter = new TabsListLayoutAdapter(context);
        setAdapter(tabsAdapter);

        final TabSwipeGestureListener swipeListener = new TabSwipeGestureListener();
        setOnTouchListener(swipeListener);
        setOnScrollListener(swipeListener.makeScrollListener());
        setRecyclerListener(new RecyclerListener() {
            @Override
            public void onMovedToScrapHeap(View view) {
                TabsLayoutItemView item = (TabsLayoutItemView) view;
                item.setThumbnail(null);
                item.setCloseVisible(true);
            }
        });
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        tabsPanel = panel;
    }

    @Override
    public void show() {
        setVisibility(View.VISIBLE);
        Tabs.getInstance().refreshThumbnails();
        Tabs.registerOnTabsChangedListener(this);
        refreshTabsData();
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        Tabs.unregisterOnTabsChangedListener(this);
        GeckoAppShell.notifyObservers("Tab:Screenshot:Cancel", "");
        tabsAdapter.clear();
    }

    @Override
    public boolean shouldExpand() {
        return isVertical();
    }

    private void autoHidePanel() {
        tabsPanel.autoHidePanel();
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        switch (msg) {
            case ADDED:
                // Refresh the list to make sure the new tab is added in the right position.
                refreshTabsData();
                break;

            case CLOSED:
                if (tab.isPrivate() == isPrivate && tabsAdapter.getCount() > 0) {
                    if (tabsAdapter.removeTab(tab)) {
                        int selected = tabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
                        updateSelectedStyle(selected);
                    }
                }
                break;

            case SELECTED:
                // Update the selected position, then fall through...
                updateSelectedPosition();
            case UNSELECTED:
                // We just need to update the style for the unselected tab...
            case THUMBNAIL:
            case TITLE:
            case RECORDING_CHANGE:
            case AUDIO_PLAYING_CHANGE:
                View view = getChildAt(tabsAdapter.getPositionForTab(tab) - getFirstVisiblePosition());
                if (view == null) {
                    return;
                }

                TabsLayoutItemView item = (TabsLayoutItemView) view;
                item.assignValues(tab);
                break;
        }
    }

    // Updates the selected position in the list so that it will be scrolled to the right place.
    private void updateSelectedPosition() {
        int selected = tabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
        updateSelectedStyle(selected);
        if (selected != -1) {
            setSelection(selected);
        }
    }

    /**
     * Updates the selected/unselected style for the tabs.
     *
     * @param selected position of the selected tab
     */
    private void updateSelectedStyle(int selected) {
        for (int i = 0; i < tabsAdapter.getCount(); i++) {
            setItemChecked(i, (i == selected));
        }
    }

    private void refreshTabsData() {
        // Store a different copy of the tabs, so that we don't have to worry about
        // accidentally updating it on the wrong thread.
        ArrayList<Tab> tabData = new ArrayList<Tab>();
        Iterable<Tab> allTabs = Tabs.getInstance().getTabsInOrder();

        for (Tab tab : allTabs) {
            if (tab.isPrivate() == isPrivate) {
                tabData.add(tab);
            }
        }

        tabsAdapter.setTabs(tabData);
        updateSelectedPosition();
    }

    public void resetTransforms(View view) {
        ViewHelper.setAlpha(view, 1);

        if (isVertical()) {
            ViewHelper.setTranslationX(view, 0);
        } else {
            ViewHelper.setTranslationY(view, 0);
        }

        // We only need to reset the height or width after individual tab close animations.
        if (originalSize != 0) {
            if (isVertical()) {
                ViewHelper.setHeight(view, originalSize);
            } else {
                ViewHelper.setWidth(view, originalSize);
            }
        }
    }

    private boolean isVertical() {
        return (getOrientation().compareTo(TwoWayView.Orientation.VERTICAL) == 0);
    }

    @Override
    public void closeAll() {
        final int childCount = getChildCount();

        // Just close the panel if there are no tabs to close.
        if (childCount == 0) {
            autoHidePanel();
            return;
        }

        // Disable the view so that gestures won't interfere wth the tab close animation.
        setEnabled(false);

        // Delay starting each successive animation to create a cascade effect.
        int cascadeDelay = 0;
        for (int i = childCount - 1; i >= 0; i--) {
            final View view = getChildAt(i);

            final PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
            animator.attach(view, Property.ALPHA, 0);

            if (isVertical()) {
                animator.attach(view, Property.TRANSLATION_X, view.getWidth());
            } else {
                animator.attach(view, Property.TRANSLATION_Y, view.getHeight());
            }

            closeAllAnimationCount++;

            animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
                @Override
                public void onPropertyAnimationStart() {
                }

                @Override
                public void onPropertyAnimationEnd() {
                    closeAllAnimationCount--;
                    if (closeAllAnimationCount > 0) {
                        return;
                    }

                    // Hide the panel after the animation is done.
                    autoHidePanel();

                    // Re-enable the view after the animation is done.
                    TabsListLayout.this.setEnabled(true);

                    // Then actually close all the tabs.
                    final Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();

                    for (Tab tab : tabs) {
                        // In the normal panel we want to close all tabs (both private and normal),
                        // but in the private panel we only want to close private tabs.
                        if (!isPrivate || tab.isPrivate()) {
                            Tabs.getInstance().closeTab(tab, false);
                        }
                    }
                }
            });

            ThreadUtils.getUiHandler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    animator.start();
                }
            }, cascadeDelay);

            cascadeDelay += ANIMATION_CASCADE_DELAY;
        }
    }

    private void animateClose(final View view, int pos) {
        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
        animator.attach(view, Property.ALPHA, 0);

        if (isVertical()) {
            animator.attach(view, Property.TRANSLATION_X, pos);
        } else {
            animator.attach(view, Property.TRANSLATION_Y, pos);
        }

        closeAnimationCount++;

        pendingClosedTabs.add(view);

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                closeAnimationCount--;
                if (closeAnimationCount > 0) {
                    return;
                }

                for (View pendingView : pendingClosedTabs) {
                    animateFinishClose(pendingView);
                }
                pendingClosedTabs.clear();
            }
        });

        if (tabsAdapter.getCount() == 1) {
            autoHidePanel();
        }

        animator.start();
    }

    private void animateFinishClose(final View view) {
        final boolean isVertical = isVertical();

        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);

        if (isVertical) {
            animator.attach(view, Property.HEIGHT, 1);
        } else {
            animator.attach(view, Property.WIDTH, 1);
        }

        final int tabId = ((TabsLayoutItemView) view).getTabId();

        // Caching this assumes that all rows are the same height
        if (originalSize == 0) {
            originalSize = (isVertical ? view.getHeight() : view.getWidth());
        }

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getTab(tabId);
                tabs.closeTab(tab, true);
            }
        });

        animator.start();
    }

    private void animateCancel(final View view) {
        PropertyAnimator animator = new PropertyAnimator(ANIMATION_DURATION);
        animator.attach(view, Property.ALPHA, 1);

        if (isVertical()) {
            animator.attach(view, Property.TRANSLATION_X, 0);
        } else {
            animator.attach(view, Property.TRANSLATION_Y, 0);
        }

        animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
            }

            @Override
            public void onPropertyAnimationEnd() {
                TabsLayoutItemView tab = (TabsLayoutItemView) view;
                tab.setCloseVisible(true);
            }
        });

        animator.start();
    }

    private class TabsListLayoutAdapter extends TabsLayoutAdapter {
        private final Button.OnClickListener mCloseOnClickListener;

        public TabsListLayoutAdapter(Context context) {
            super(context, R.layout.tabs_list_item_view);
            mCloseOnClickListener = new Button.OnClickListener() {
                @Override
                public void onClick(View v) {
                    // The view here is the close button, which has a reference
                    // to the parent TabsLayoutItemView in it's tag, hence the getTag() call
                    TabsLayoutItemView item = (TabsLayoutItemView) v.getTag();
                    final int pos = (isVertical() ? item.getWidth() : 0 - item.getHeight());
                    animateClose(item, pos);
                }
            };
        }

        @Override
        public TabsLayoutItemView newView(int position, ViewGroup parent) {
            TabsLayoutItemView item = super.newView(position, parent);
            item.setCloseOnClickListener(mCloseOnClickListener);
            ((ThemedRelativeLayout) item.findViewById(R.id.wrapper)).setPrivateMode(isPrivate);
            return item;
        }

        @Override
        public void bindView(TabsLayoutItemView view, Tab tab) {
            super.bindView(view, tab);
            // If we're recycling this view, there's a chance it was transformed during
            // the close animation. Remove any of those properties.
            resetTransforms(view);
        }
    }

    private class TabSwipeGestureListener implements View.OnTouchListener {
        // same value the stock browser uses for after drag animation velocity in pixels/sec
        // http://androidxref.com/4.0.4/xref/packages/apps/Browser/src/com/android/browser/NavTabScroller.java#61
        private static final float MIN_VELOCITY = 750;

        private final int swipeThreshold;
        private final int minFlingVelocity;
        private final int maxFlingVelocity;
        private VelocityTracker velocityTracker;
        private int listWidth = 1;
        private int listHeight = 1;
        private View swipeView;
        private Runnable pendingCheckForTap;
        private float swipeStartX;
        private float swipeStartY;
        private boolean swiping;
        private boolean enabled;

        public TabSwipeGestureListener() {
            enabled = true;

            ViewConfiguration vc = ViewConfiguration.get(TabsListLayout.this.getContext());

            swipeThreshold = vc.getScaledTouchSlop();
            minFlingVelocity = (int) (getContext().getResources().getDisplayMetrics().density * MIN_VELOCITY);
            maxFlingVelocity = vc.getScaledMaximumFlingVelocity();
        }

        public void setEnabled(boolean enabled) {
            this.enabled = enabled;
        }

        public TwoWayView.OnScrollListener makeScrollListener() {
            return new TwoWayView.OnScrollListener() {

                @Override
                public void onScrollStateChanged(TwoWayView twoWayView, int scrollState) {
                    setEnabled(scrollState != TwoWayView.OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);
                }

                @Override
                public void onScroll(TwoWayView twoWayView, int i, int i1, int i2) {
                }
            };
        }

        @Override
        public boolean onTouch(View view, MotionEvent e) {
            if (!enabled) {
                return false;
            }

            if (listWidth < 2 || listHeight < 2) {
                listWidth = TabsListLayout.this.getWidth();
                listHeight = TabsListLayout.this.getHeight();
            }

            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: {
                    // Check if we should set pressed state on the
                    // touched view after a standard delay.
                    triggerCheckForTap();

                    final float x = e.getRawX();
                    final float y = e.getRawY();

                    // Find out which view is being touched
                    swipeView = findViewAt(x, y);

                    if (swipeView != null) {
                        swipeStartX = e.getRawX();
                        swipeStartY = e.getRawY();
                        velocityTracker = VelocityTracker.obtain();
                        velocityTracker.addMovement(e);
                    }

                    view.onTouchEvent(e);
                    return true;
                }
                case MotionEvent.ACTION_UP: {
                    if (swipeView == null) {
                        break;
                    }

                    cancelCheckForTap();
                    swipeView.setPressed(false);

                    if (!swiping) {
                        TabsLayoutItemView item = (TabsLayoutItemView) swipeView;
                        final int tabId = item.getTabId();
                        Tabs.getInstance().selectTab(tabId);
                        autoHidePanel();
                        Tabs.getInstance().notifyListeners(
                                Tabs.getInstance().getTab(tabId), Tabs.TabEvents.OPENED_FROM_TABS_TRAY
                        );
                        velocityTracker.recycle();
                        velocityTracker = null;
                        break;
                    }

                    velocityTracker.addMovement(e);
                    velocityTracker.computeCurrentVelocity(1000, maxFlingVelocity);

                    float velocityX = Math.abs(velocityTracker.getXVelocity());
                    float velocityY = Math.abs(velocityTracker.getYVelocity());
                    boolean dismiss = false;
                    boolean dismissDirection = false;
                    int dismissTranslation;

                    if (isVertical()) {
                        float deltaX = ViewHelper.getTranslationX(swipeView);

                        if (Math.abs(deltaX) > listWidth / 2) {
                            dismiss = true;
                            dismissDirection = (deltaX > 0);
                        } else if (minFlingVelocity <= velocityX && velocityX <= maxFlingVelocity
                                                                 && velocityY < velocityX) {

                            dismiss = swiping && (deltaX * velocityTracker.getXVelocity() > 0);
                            dismissDirection = (velocityTracker.getXVelocity() > 0);
                        }

                        dismissTranslation = (dismissDirection ? listWidth : -listWidth);
                    } else {
                        float deltaY = ViewHelper.getTranslationY(swipeView);

                        if (Math.abs(deltaY) > listHeight / 2) {
                            dismiss = true;
                            dismissDirection = (deltaY > 0);
                        } else if (minFlingVelocity <= velocityY && velocityY <= maxFlingVelocity
                                                                 && velocityX < velocityY) {

                            dismiss = swiping && (deltaY * velocityTracker.getYVelocity() > 0);
                            dismissDirection = (velocityTracker.getYVelocity() > 0);
                        }

                        dismissTranslation = (dismissDirection ? listHeight : -listHeight);
                    }

                    if (dismiss) {
                        animateClose(swipeView, dismissTranslation);
                    } else {
                        animateCancel(swipeView);
                    }

                    velocityTracker.recycle();
                    velocityTracker = null;
                    swipeView = null;
                    swipeStartX = 0;
                    swipeStartY = 0;
                    swiping = false;
                    break;
                }

                case MotionEvent.ACTION_MOVE: {
                    if (swipeView == null || velocityTracker == null) {
                        break;
                    }

                    velocityTracker.addMovement(e);

                    final boolean isVertical = isVertical();
                    float deltaX = e.getRawX() - swipeStartX;
                    float deltaY = e.getRawY() - swipeStartY;
                    float delta = (isVertical ? deltaX : deltaY);
                    boolean isScrollingX = Math.abs(deltaX) > swipeThreshold;
                    boolean isScrollingY = Math.abs(deltaY) > swipeThreshold;
                    boolean isSwipingToClose = (isVertical ? isScrollingX : isScrollingY);

                    // If we're actually swiping, make sure we don't
                    // set pressed state on the swiped view.
                    if (isScrollingX || isScrollingY) {
                        cancelCheckForTap();
                    }

                    if (isSwipingToClose) {
                        swiping = true;
                        TabsListLayout.this.requestDisallowInterceptTouchEvent(true);
                        ((TabsLayoutItemView) swipeView).setCloseVisible(false);

                        // Stops listview from highlighting the touched item
                        // in the list when swiping.
                        MotionEvent cancelEvent = MotionEvent.obtain(e);
                        cancelEvent.setAction(MotionEvent.ACTION_CANCEL |
                                (e.getActionIndex() << MotionEvent.ACTION_POINTER_INDEX_SHIFT));

                        TabsListLayout.this.onTouchEvent(cancelEvent);
                        cancelEvent.recycle();
                    }
                    if (swiping) {
                        if (isVertical) {
                            ViewHelper.setTranslationX(swipeView, delta);
                        } else {
                            ViewHelper.setTranslationY(swipeView, delta);
                        }

                        ViewHelper.setAlpha(swipeView, Math.max(0.1f, Math.min(1f,
                                1f - 2f * Math.abs(delta) / (isVertical ? listWidth : listHeight))));

                        return true;
                    }
                    break;
                }
            }
            return false;
        }

        private View findViewAt(float rawX, float rawY) {
            Rect rect = new Rect();

            int[] listViewCoords = new int[2];
            TabsListLayout.this.getLocationOnScreen(listViewCoords);

            int x = (int) rawX - listViewCoords[0];
            int y = (int) rawY - listViewCoords[1];

            for (int i = 0; i < TabsListLayout.this.getChildCount(); i++) {
                View child = TabsListLayout.this.getChildAt(i);
                child.getHitRect(rect);

                if (rect.contains(x, y)) {
                    return child;
                }
            }
            return null;
        }

        private void triggerCheckForTap() {
            if (pendingCheckForTap == null) {
                pendingCheckForTap = new CheckForTap();
            }
            TabsListLayout.this.postDelayed(pendingCheckForTap, ViewConfiguration.getTapTimeout());
        }

        private void cancelCheckForTap() {
            if (pendingCheckForTap == null) {
                return;
            }
            TabsListLayout.this.removeCallbacks(pendingCheckForTap);
        }

        private class CheckForTap implements Runnable {
            @Override
            public void run() {
                if (!swiping && swipeView != null && enabled) {
                    swipeView.setPressed(true);
                }
            }
        }
    }
}
